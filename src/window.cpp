#include "window.hpp"
#include "config.hpp"
#include "drawable.hpp"
#include <vector>
#include <xcb/xproto.h>

/* NOTE: First part is mostly close to DesktopPixmap */
window::window (const int16_t x,       ///< x coordinate of the top left corner
                const int16_t y,       ///< y coordinate of the top left corner
                const uint16_t width,  ///< width of display
                const uint16_t height) ///< height of display
    : drawable (x, y, width, height)
{
  this->xcb_id = xcb_generate_id (drawable::c_);
  this->desktop_sel = 0; // Id of the preselected desktop
  create_window ();
}

window::~window () { xcb_destroy_window (drawable::c_, this->xcb_id); }

void
window::create_window ()
{
  uint32_t mask = 0;
  uint32_t values[3];
  const bool override_redirect[] = { true };
  mask = XCB_CW_BACK_PIXEL | XCB_CW_EVENT_MASK;
  values[0] = dexpo_bgcolor; // Background color
  // Will be used in future to handle events
  values[1] = XCB_EVENT_MASK_EXPOSURE | XCB_EVENT_MASK_KEY_PRESS
              | XCB_EVENT_MASK_BUTTON_PRESS | XCB_EVENT_MASK_KEY_RELEASE
              | XCB_EVENT_MASK_FOCUS_CHANGE | XCB_EVENT_MASK_POINTER_MOTION
              | XCB_EVENT_MASK_LEAVE_WINDOW | XCB_EVENT_MASK_ENTER_WINDOW;

  values[2] = XCB_STACK_MODE_ABOVE; // Places created window on top
  xcb_create_window (window::c_, /* Connection, separate from one of daemon */
                     XCB_COPY_FROM_PARENT,          /* depth (same as root)*/
                     this->xcb_id,                  /* window Id */
                     drawable::screen_->root,       /* parent window */
                     this->x, this->y,              /* x, y */
                     this->width, this->height,     /* width, height */
                     dexpo_outer_border,            /* border_width */
                     XCB_WINDOW_CLASS_INPUT_OUTPUT, /* class */
                     window::screen_->root_visual,  /* visual */
                     mask, values);                 /* masks, not used yet */

  /* Fixes window's place */
  xcb_change_window_attributes (c_, xcb_id, XCB_CW_OVERRIDE_REDIRECT,
                                &override_redirect);

  /* Map the window on the screen */
  xcb_map_window (window::c_, this->xcb_id);

  // Set the focus. Doing it after mapping window is crucial.
  xcb_set_input_focus (c_, XCB_INPUT_FOCUS_POINTER_ROOT, this->xcb_id,
                       XCB_TIME_CURRENT_TIME);

  /* Sends commands to the server */
  xcb_flush (window::c_);
}

void
window::draw_gui ()
{
  if (dexpo_width == 0)
    {
      // Drawing screenshots starting from the left top corner
      auto act_width = dexpo_padding;
      for (const auto &desktop : this->desktops)
        {
          xcb_put_image (dxp_desktop::c_, XCB_IMAGE_FORMAT_Z_PIXMAP,
                         this->xcb_id,     /* Pixmap to put image on */
                         dxp_desktop::gc_, /* Graphic context */
                         desktop.width, desktop.height, /* Dimensions */
                         act_width,     /* Destination X coordinate */
                         dexpo_padding, /* Destination Y coordinate */
                         0, drawable::screen_->root_depth,
                         desktop.pixmap_len, /* Image size in bytes */
                         desktop.pixmap.data ());
          act_width += desktop.width;
          act_width += dexpo_padding;
        };
    }
  else if (dexpo_height == 0)
    {
      auto act_height = dexpo_padding;
      for (const auto &desktop : this->desktops)
        {
          xcb_put_image (dxp_desktop::c_, XCB_IMAGE_FORMAT_Z_PIXMAP,
                         this->xcb_id,     /* Pixmap to put image on */
                         dxp_desktop::gc_, /* Graphic context */
                         desktop.width, desktop.height, /* Dimensions */
                         dexpo_padding, /* Destination X coordinate */
                         act_height,    /* Destination Y coordinate */
                         0, drawable::screen_->root_depth,
                         desktop.pixmap_len, /* Image size in bytes */
                         desktop.pixmap.data ());
          act_height += desktop.height;
          act_height += dexpo_padding;
        };
    }
}

int
window::get_screen_position (int desktop_id)
{
  // As all screenshots have at least one common coordinate of corner, we need
  // to find only the second one
  int pos = dexpo_padding;
  for (const auto &desktop : this->desktops)
    {
      // Estimating all space before the screenshot we search
      if (desktop.id < desktop_id)
        {
          pos += dexpo_padding;
          if (dexpo_height == 0)
            {
              pos += desktop.height;
            }
          else if (dexpo_width == 0)
            {
              pos += desktop.width;
            }
        }
      else
        {
          return pos;
        }
    }
  return 0;
}

void
window::highlight_window (int desktop_id, uint32_t color)
{
  int16_t x = dexpo_padding;
  int16_t y = dexpo_padding;
  uint32_t values[1]; // mask for changing border's color

  uint16_t width = this->desktops[size_t (desktop_id)].width;
  uint16_t height = this->desktops[size_t (desktop_id)].height;

  if (dexpo_height == 0)
    {
      y = get_screen_position (desktop_id);
    }
  else
    {
      x = get_screen_position (desktop_id);
    }

  // The best way to create rectangular border with xcb
  // is to draw 4 filled rectangles
  xcb_rectangle_t borders[]
      = { // left border
          {
              int16_t (x - dexpo_hlwidth),          /* x */
              int16_t (y - dexpo_hlwidth),          /* y */
              uint16_t (dexpo_hlwidth),             /* width */
              uint16_t (height + 2 * dexpo_hlwidth) /* height */
          },
          // right border
          {
              int16_t (x + width),                  /* x */
              int16_t (y - dexpo_hlwidth),          /* y */
              uint16_t (dexpo_hlwidth),             /* width */
              uint16_t (height + 2 * dexpo_hlwidth) /* height */
          },
          // top border
          {
              int16_t (x - 1),             /* x */
              int16_t (y - dexpo_hlwidth), /* y */
              uint16_t (width + 1),        /* width */
              uint16_t (dexpo_hlwidth)     /* height */
          },
          // bottom border
          {
              int16_t (x - 1),         /* x */
              int16_t (y + height),    /* y */
              uint16_t (width + 1),    /* width */
              uint16_t (dexpo_hlwidth) /* height */
          }
        };
  // Changing color
  uint32_t mask = XCB_GC_FOREGROUND;
  values[0] = color;
  xcb_change_gc (c_, dxp_desktop::gc_, mask, &values);
  // Drawing the highlighting itself
  xcb_poly_fill_rectangle (window::c_, this->xcb_id, dxp_desktop::gc_, 4,
                           borders);
}
