#include "window.hpp"
#include "config.hpp"
#include "drawable.hpp"
#include <vector>
#include <xcb/xproto.h>

constexpr bool k_horizontal_stacking = (dexpo_width == 0);
constexpr bool k_vertical_stacking = (dexpo_height == 0);

window::window (const int16_t x,       ///< x coordinate of the top left corner
                const int16_t y,       ///< y coordinate of the top left corner
                const uint16_t width,  ///< width of the window
                const uint16_t height) ///< height of the window
    : drawable (x, y, width, height)
{
  this->xcb_id = xcb_generate_id (drawable::c_);
  this->desktop_sel = 0; // Id of the preselected desktop

  create_gc ();
  create_window ();
}

window::~window () { xcb_destroy_window (drawable::c_, this->xcb_id); }

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
  std::array<uint32_t, 3> mask_values{
    dexpo_bgcolor,
    // These values are used to subscribe to relevant events
    XCB_EVENT_MASK_EXPOSURE | XCB_EVENT_MASK_KEY_PRESS
        | XCB_EVENT_MASK_BUTTON_PRESS | XCB_EVENT_MASK_KEY_RELEASE
        | XCB_EVENT_MASK_FOCUS_CHANGE | XCB_EVENT_MASK_POINTER_MOTION
        | XCB_EVENT_MASK_LEAVE_WINDOW | XCB_EVENT_MASK_ENTER_WINDOW
  };

  xcb_create_window (window::c_, /* Connection, separate from one of daemon */
                     XCB_COPY_FROM_PARENT,          /* depth (same as root)*/
                     this->xcb_id,                  /* window Id */
                     drawable::screen_->root,       /* parent window */
                     this->x, this->y,              /* x, y */
                     this->width, this->height,     /* width, height */
                     dexpo_outer_border,            /* border_width */
                     XCB_WINDOW_CLASS_INPUT_OUTPUT, /* class */
                     window::screen_->root_visual,  /* visual */
                     mask, &mask_values);           /* masks, not used yet */

  /* Fixes window in place */
  const std::array<uint32_t, 2> override_redirect{ 1U };
  xcb_change_window_attributes (c_, xcb_id, XCB_CW_OVERRIDE_REDIRECT,
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
window::draw_gui ()
{
  // Coordinates for the desktop relative to the window
  int16_t x = dexpo_padding;
  int16_t y = dexpo_padding;
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
          x += dexpo_padding;
        }
      else if (k_vertical_stacking)
        {
          y += desktop.height;
          y += dexpo_padding;
        };
    }
}

/**
 * Get coordinate of the displayed desktop relative to the window
 */
int
window::get_desktop_coord (size_t desktop_id)
{
  // As all screenshots have at least one common coordinate of corner, we need
  // to find only the second one
  auto pos = dexpo_padding;
  for (const auto &desktop : this->desktops)
    {
      // Counting space for all desktops up to desktop_id
      if (desktop.id == desktop_id)
        {
          return pos;
        }

      // Append width for horizontal stacking and height for vertical
      pos += k_horizontal_stacking ? desktop.width : desktop.height;
      pos += dexpo_padding;
    }
  return 0;
}

/**
 * Get id of the desktop above which the cursor is hovering.
 * If cursor is not above any desktop return -1.
 */
int
window::get_hover_desktop (int16_t x, int16_t y)
{
  for (const auto &d : this->desktops)
    {
      if (k_vertical_stacking)
        {
          auto desktop_y = get_desktop_coord (d.id);
          // Check if cursor is inside of the desktop
          if (x >= dexpo_padding &&           // Not to the left
              x <= d.width + dexpo_padding && // Not to the right
              y >= desktop_y &&               // Not above
              y <= d.height + desktop_y)      // Not below
            {
              return int (d.id);
            }
        }
      else if (k_horizontal_stacking)
        {
          auto desktop_x = get_desktop_coord (d.id);
          // Check if cursor is inside of the desktop
          if (x >= desktop_x &&              // To the right of left border
              x <= d.width + desktop_x &&    // To the left of right border
              y >= dexpo_padding &&          // Not above
              y <= d.height + dexpo_padding) // Not below
            {
              return int (d.id);
            }
        }
    }
  return -1;
};

/**
 * Draw a border of color around desktop with desktop_id
 *
 * @note color can be set to bgcolor to remove highlight
 */
void
window::draw_border (size_t desktop_id, uint32_t color)
{
  int16_t x = dexpo_padding;
  int16_t y = dexpo_padding;

  uint16_t width = this->desktops[desktop_id].width;
  uint16_t height = this->desktops[desktop_id].height;

  if (k_vertical_stacking)
    {
      y = int16_t (get_desktop_coord (desktop_id));
    }
  else if (k_horizontal_stacking)
    {
      x = int16_t (get_desktop_coord (desktop_id));
    }

  // The best way to create rectangular border with xcb
  // is to draw 4 filled rectangles.
  std::array<xcb_rectangle_t, 4> borders{
    // Left border
    xcb_rectangle_t{
        int16_t (x - dexpo_hlwidth),          /* x */
        int16_t (y - dexpo_hlwidth),          /* y */
        uint16_t (dexpo_hlwidth),             /* width */
        uint16_t (height + 2 * dexpo_hlwidth) /* height */
    },
    // Right border
    xcb_rectangle_t{
        int16_t (x + width),                  /* x */
        int16_t (y - dexpo_hlwidth),          /* y */
        uint16_t (dexpo_hlwidth),             /* width */
        uint16_t (height + 2 * dexpo_hlwidth) /* height */
    },
    // Top border
    xcb_rectangle_t{
        int16_t (x - 1),             /* x */
        int16_t (y - dexpo_hlwidth), /* y */
        uint16_t (width + 1),        /* width */
        uint16_t (dexpo_hlwidth)     /* height */
    },
    // Bottom border
    xcb_rectangle_t{
        int16_t (x - 1),         /* x */
        int16_t (y + height),    /* y */
        uint16_t (width + 1),    /* width */
        uint16_t (dexpo_hlwidth) /* height */
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
 * Draw a preselection border of color=dexpo_hlcolor
 * around desktop=desktop[this->desktop_sel].
 */
void
window::draw_preselection ()
{
  draw_border (this->desktop_sel, dexpo_preselected_desktop_color);
};

/**
 * Remove a preselection border around desktop[this->desktop_sel].
 *
 * @note This implementation just draws border of color dexpo_bgcolor above
 * existing border.
 */
void
window::clear_preselection ()
{
  draw_border (this->desktop_sel, dexpo_desktop_color);
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
