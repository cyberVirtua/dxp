#include "window.hpp"
#include "config.hpp"
#include "drawable.hpp"
#include "xcb_util.hpp"
#include <cmath>
#include <iostream>
#include <vector>
#include <xcb/xproto.h>

constexpr bool k_horizontal_stacking = (dxp_width == 0);
constexpr bool k_vertical_stacking = (dxp_height == 0);

window::window (const std::vector<dxp_socket_desktop> &desktops)
    : drawable (0, 0, 0, 0) // Width and height will be calculated from config
{
  this->xcb_id = xcb_generate_id (drawable::c_);
  this->desktops = desktops;
  this->pres = 0; ///< id of the preselected desktop

  set_window_dimensions ();
  create_gc ();
  create_window ();
}

window::~window () { xcb_destroy_window (drawable::c_, this->xcb_id); }

/**
 * Calculate dimensions of the window based on
 * stacking mode and desktops from the daemon
 */
void
window::set_window_dimensions ()
{
  uint constant = 0; ///< Constant dimension. The one specified in the config
  uint dynamic = 0;  ///< Dynamic dimension dependent on the desktops

  constant += 2 * dxp_padding + 2 * dxp_border_pres_width;
  dynamic += dxp_padding + dxp_border_pres_width;

  for (const auto &d : this->desktops)
    {
      dynamic += k_horizontal_stacking ? d.width : d.height;
      dynamic += dxp_padding + 2 * dxp_border_pres_width;
    }

  this->width += k_vertical_stacking ? constant + dxp_width : dynamic;
  this->height += k_horizontal_stacking ? constant + dxp_height : dynamic;

  /* Determine what monitor to draw window on */
  monitor_info dxp_monitor{ 0, 0, 0, 0, "" };

  if (dxp_monitor_name.empty ()) // Use primary if name is not specified
    {
      dxp_monitor = get_monitor_primary (window::c_, window::root_);
    }
  else // Get coordinates of the monitor that was specified in the config
    {
      auto monitors = get_monitors (window::c_, window::root_);
      for (const auto &m : monitors)
        {
          if (m.name == dxp_monitor_name)
            {
              dxp_monitor = m;
              break;
            }
        }
      if (dxp_monitor.height == 0) // If target monitor was not found
        {
          std::cerr << "Monitor with name \"" << dxp_monitor_name
                    << "\" was not found. Using primary monitor" << std::endl;

          dxp_monitor = get_monitor_primary (window::c_, window::root_);
        }
    }

  // Set window coordinates based on config values
  x = dxp_x >= 0 && !std::signbit (dxp_x) // signbit is to handle negative zero
          ? dxp_x
          : dxp_monitor.width - this->width - std::abs (dxp_x);
  x += dxp_monitor.x; // Add monitor's offset

  y += dxp_y >= 0 && !std::signbit (dxp_y)
           ? dxp_y
           : dxp_monitor.height - this->height - std::abs (dxp_y);
  y += dxp_monitor.y;
};

/**
 * Initialize window, subscribe to relevant events and map it onto the desktop
 */
void
window::create_window ()
{
  // Mask used
  uint32_t mask = 0;
  mask = XCB_CW_BACK_PIXEL | XCB_CW_EVENT_MASK;

  // Each mask value entry corresponds to mask enum first to last
  std::array<uint32_t, 2> mask_values{
    dxp_background,
    // These values are used to subscribe to relevant events
    XCB_EVENT_MASK_EXPOSURE | XCB_EVENT_MASK_KEY_PRESS
        | XCB_EVENT_MASK_BUTTON_PRESS | XCB_EVENT_MASK_KEY_RELEASE
        | XCB_EVENT_MASK_FOCUS_CHANGE | XCB_EVENT_MASK_POINTER_MOTION
        | XCB_EVENT_MASK_LEAVE_WINDOW | XCB_EVENT_MASK_ENTER_WINDOW
  };

  xcb_create_window (window::c_, /* Connection, separate from one of daemon */
                     XCB_COPY_FROM_PARENT,          /* depth (same as root)*/
                     this->xcb_id,                  /* window Id */
                     window::root_,                 /* parent window */
                     this->x, this->y,              /* x, y */
                     this->width, this->height,     /* width, height */
                     dxp_border_width,              /* border_width */
                     XCB_WINDOW_CLASS_INPUT_OUTPUT, /* class */
                     window::screen_->root_visual,  /* visual */
                     mask, &mask_values);           /* masks, not used yet */

  /* Fixes window in place */
  const std::array<uint32_t, 2> override_redirect{ 1U };
  xcb_change_window_attributes (c_, this->xcb_id, XCB_CW_OVERRIDE_REDIRECT,
                                &override_redirect);

  /* Map the window onto the screen */
  xcb_map_window (window::c_, this->xcb_id);

  /* Focus on the window. Doing it *after* mapping the window is crucial. */
  xcb_set_input_focus (c_, XCB_INPUT_FOCUS_POINTER_ROOT, this->xcb_id,
                       XCB_TIME_CURRENT_TIME);

  /* Sends commands to the server */
  xcb_flush (window::c_);
}

/**
 * Draw pixmaps on the window
 */
void
window::draw_desktops ()
{
  // Coordinates for the desktop relative to the window
  int16_t x = dxp_padding + dxp_border_pres_width;
  int16_t y = dxp_padding + dxp_border_pres_width;
  for (const auto &desktop : this->desktops)
    {
      xcb_put_image (window::c_, XCB_IMAGE_FORMAT_Z_PIXMAP,
                     this->xcb_id,                  /* Pixmap to put image on */
                     window::gc_,                   /* Graphic context */
                     desktop.width, desktop.height, /* Dimensions */
                     x, /* Destination X coordinate */
                     y, /* Destination Y coordinate */
                     0, drawable::screen_->root_depth,
                     desktop.pixmap_len, /* Image size in bytes */
                     desktop.pixmap.data ());
      if (k_horizontal_stacking)
        {
          x += desktop.width;
          x += dxp_padding + 2 * dxp_border_pres_width;
        }
      else if (k_vertical_stacking)
        {
          y += desktop.height;
          y += dxp_padding + 2 * dxp_border_pres_width;
        };
    }
}

/**
 * Get coordinate of the displayed desktop relative to the window
 */
int16_t
window::get_desktop_coord (uint desktop_id)
{
  // As all screenshots have at least one common coordinate of corner, we need
  // to find only the second one
  auto pos = dxp_padding + dxp_border_pres_width;
  for (const auto &desktop : this->desktops)
    {
      // Counting space for all desktops up to desktop_id
      if (desktop.id == desktop_id)
        {
          return pos;
        }

      // Append width for horizontal stacking and height for vertical
      pos += k_horizontal_stacking ? desktop.width : desktop.height;
      pos += dxp_padding + 2 * dxp_border_pres_width;
    }
  return 0;
}

/**
 * Get id of the desktop above which the cursor is hovering.
 * If cursor is not above any desktop return -1.
 *
 * TODO Cache desktop coordinates
 */
uint
window::get_hover_desktop (int16_t x, int16_t y)
{
  for (const auto &d : this->desktops)
    {
      if (k_vertical_stacking)
        {
          auto desktop_y = get_desktop_coord (d.id);
          // Check if cursor is inside of the desktop
          if (x >= dxp_padding &&           // Not to the left
              x <= d.width + dxp_padding && // Not to the right
              y >= desktop_y &&             // Not above
              y <= d.height + desktop_y)    // Not below
            {
              return d.id;
            }
        }
      else if (k_horizontal_stacking)
        {
          auto desktop_x = get_desktop_coord (d.id);
          // Check if cursor is inside of the desktop
          if (x >= desktop_x &&            // To the right of left border
              x <= d.width + desktop_x &&  // To the left of right border
              y >= dxp_padding &&          // Not above
              y <= d.height + dxp_padding) // Not below
            {
              return d.id;
            }
        }
    }
  return -1U;
};

/**
 * Draw a border of color around desktop with desktop_id
 *
 * @note color can be set to bgcolor to remove highlight
 */
void
window::draw_desktop_border (uint desktop_id, uint32_t color)
{
  int16_t x = 0;
  int16_t y = 0;

  uint16_t width = this->desktops[desktop_id].width;
  uint16_t height = this->desktops[desktop_id].height;

  if (k_vertical_stacking)
    {
      x = dxp_padding + dxp_border_pres_width;
      y = get_desktop_coord (desktop_id);
    }
  else if (k_horizontal_stacking)
    {
      x = get_desktop_coord (desktop_id);
      y = dxp_padding + dxp_border_pres_width;
    }

  auto border = dxp_border_pres_width;

  // The best way to create rectangular border with xcb
  // is to draw 4 filled rectangles.
  std::array<xcb_rectangle_t, 4> borders{
    // Top border
    xcb_rectangle_t{
        int16_t (x - border),          /* x */
        int16_t (y - border),          /* y */
        uint16_t (width + 2 * border), /* width */
        uint16_t (border)              /* height */
    },
    // Left border
    xcb_rectangle_t{
        int16_t (x - border),          /* x */
        int16_t (y - border),          /* y */
        uint16_t (border),             /* width */
        uint16_t (height + 2 * border) /* height */
    },
    // Right border
    xcb_rectangle_t{
        int16_t (x + width),           /* x */
        int16_t (y - border),          /* y */
        uint16_t (border),             /* width */
        uint16_t (height + 2 * border) /* height */
    },
    // Bottom border
    xcb_rectangle_t{
        int16_t (x - border),          /* x */
        int16_t (y + height),          /* y */
        uint16_t (width + 2 * border), /* width */
        uint16_t (border)              /* height */
    }
  };

  // Setting border color
  uint32_t mask = XCB_GC_FOREGROUND;
  std::array<uint32_t, 1> mask_values{ color }; // Mask value
  xcb_change_gc (c_, window::gc_, mask, &mask_values);

  // Drawing the preselection border
  xcb_poly_fill_rectangle (window::c_, this->xcb_id, window::gc_,
                           borders.size (), &borders[0]);
}

/**
 * Draw a preselection border around current preselected desktop
 */
void
window::draw_preselection ()
{
  draw_desktop_border (this->pres, dxp_border_pres);
};

/**
 * Remove a preselection border around current preselected desktop.
 *
 * @note This implementation just draws border of color dxp_border_nopres
 * over existing border.
 */
void
window::clear_preselection ()
{
  draw_desktop_border (this->pres, dxp_border_nopres);
};

/**
 * TODO Document
 *
 * XXX Accepting *event does not work. Why?
 *
 * Returns zero on exit event
 */
int
window::handle_event (xcb_generic_event_t *event)
{
  // Ingore the leftmost bit of event
  // https://stackoverflow.com/questions/60744214
  constexpr uint8_t k_xcb_event_mask = 127; // 01111111
  switch (event->response_type & k_xcb_event_mask)
    {
    case XCB_EXPOSE:
      {
        draw_desktops ();
        pres = get_current_desktop (c_, root_);

        // Drawing borders around all desktops
        for (uint i = 0; i < desktops.size (); i++)
          {
            i == pres ? draw_preselection ()
                      : draw_desktop_border (i, dxp_border_nopres);
          }
        break;
      }
    case XCB_KEY_PRESS:
      {
        auto *kp = reinterpret_cast<xcb_key_press_event_t *> (event);

        /// Right arrow or down arrow
        bool next = kp->detail == 114 || kp->detail == 116;
        /// Left arrow or up arrow
        bool prev = kp->detail == 113 || kp->detail == 111;
        bool entr = kp->detail == 36; /// Enter
        bool esc = kp->detail == 9;   /// Escape

        clear_preselection ();
        if (next)
          {
            pres++;
            pres %= desktops.size ();
          }
        if (prev) // Decrementing preselected desktop and taking modulus
          {
            pres = pres == 0 ? desktops.size () - 1 : pres - 1;
          }
        if (entr)
          {
            // Desktop change is thought as an event after which the
            // user doesn't need dxp any more
            ewmh_change_desktop (c_, root_, pres);
            xcb_flush (c_);
            return 0; // Kill dxp
          }
        if (esc)
          {
            return 0;
          }
        draw_preselection ();
        xcb_flush (c_);
        break;
      }
    case XCB_MOTION_NOTIFY: // Cursor motion within window
      {
        auto *cur = reinterpret_cast<xcb_motion_notify_event_t *> (event);
        auto d = get_hover_desktop (cur->event_x, cur->event_y);

        if (d != -1U)
          {
            clear_preselection ();
            pres = d;
            draw_preselection ();
          }
        break;
      }
    case XCB_BUTTON_PRESS: // Mouse click
      {
        // Desktop change is thought as an event after which the
        // user doesn't need dxp any more
        ewmh_change_desktop (c_, root_, pres);
        xcb_flush (c_);
        return 0; // Kill dxp
      }
    case XCB_FOCUS_OUT:
      // Click outside of the window is thought as an event after which the
      // user doesn't need dxp any more
      {
        return 0; // Kill dxp
      }
    case 0:
      auto *e = reinterpret_cast<xcb_generic_error_t *> (event);
      check (e, "XCB error while receiving event");
    }
  xcb_flush (c_);
  return 1;
};

/**
 * Initialize a graphic context.
 *
 * Required by xcb functions
 */
void
window::create_gc ()
{
  // TODO(mangalinor): Figure out correct mask and values
  uint32_t mask = XCB_GC_FOREGROUND | XCB_GC_GRAPHICS_EXPOSURES;
  std::array<uint32_t, 2> mask_values{ drawable::screen_->black_pixel, 0 };

  window::gc_ = xcb_generate_id (c_);
  xcb_create_gc (drawable::c_, window::gc_, drawable::screen_->root, mask,
                 &mask_values);
}
