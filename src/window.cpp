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
  this->b_width = dexpo_outer_border;
  this->id = xcb_generate_id (drawable::c_);
  create_window ();
}

window::~window () { xcb_destroy_window (drawable::c_, this->id); }

void
window::create_window ()
{
  // mask as for now aren't fully implemened. This one was partially copied from
  // example_DesktopPixmap to make possible distinguishing of several windows'
  // borders
  uint32_t mask = 0;
  uint32_t values[3];
  const bool override_redirect[] = { true };
  mask = XCB_CW_BACK_PIXEL | XCB_CW_EVENT_MASK | XCB_CONFIG_WINDOW_STACK_MODE;
  values[0] = dexpo_bgcolor; // Background color
  // Will be used in future to handle events
  values[1] = XCB_EVENT_MASK_EXPOSURE | XCB_EVENT_MASK_KEY_PRESS;
  values[2] = XCB_STACK_MODE_ABOVE; // Places created window on top
  xcb_create_window (window::c_, /* Connection, separate from one of daemon */
                     XCB_COPY_FROM_PARENT,          /* depth (same as root)*/
                     this->id,                      /* window Id */
                     drawable::screen_->root,       /* parent window */
                     this->x, this->y,              /* x, y */
                     this->width, this->height,     /* width, height */
                     this->b_width,                 /* border_width */
                     XCB_WINDOW_CLASS_INPUT_OUTPUT, /* class */
                     window::screen_->root_visual,  /* visual */
                     mask, values);                 /* masks, not used yet */

  /* Fixes window's place */
  xcb_set_input_focus (c_, XCB_INPUT_FOCUS_POINTER_ROOT, id,
                       XCB_TIME_CURRENT_TIME);
  xcb_change_window_attributes (c_, id, XCB_CW_OVERRIDE_REDIRECT,
                                &override_redirect);

  /* Map the window on the screen */
  xcb_map_window (window::c_, this->id);

  /* Sends commands to the server */
  xcb_flush (window::c_);
}

int
window::get_screen_position (int desktop_number)
{
  // As all screenshots have at least one common coordinate of corner, we need
  // to find only the second one
  int coord = dexpo_padding;
  for (const auto &dexpo_pixmap : pixmaps)
    {
      // Estimating all space before the screenshot we search
      if (dexpo_pixmap.desktop_number < desktop_number)
        {
          coord += dexpo_padding;
          if (dexpo_height == 0)
            {
              coord += dexpo_pixmap.height;
            }
          else if (dexpo_width == 0)
            {
              coord += dexpo_pixmap.width;
            }
        }
      else
        {
          return coord;
        }
    }
  return 0;
}

void
window::highlight_window (int desktop_number, uint32_t color)
{
  int16_t x = dexpo_padding;
  int16_t y = dexpo_padding;
  uint32_t values[1]; // mask for changing border's color

  uint16_t width = this->pixmaps[size_t (desktop_number)].width;
  uint16_t height = this->pixmaps[size_t (desktop_number)].height;

  if (dexpo_height == 0)
    {
      y = get_screen_position (desktop_number);
    }
  else
    {
      x = get_screen_position (desktop_number);
    }

  // We can't control rectangle's thickness, so instead we spawn several close
  // to each other
  xcb_rectangle_t rectangles[]
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
  // Drawing the highlighting itself
  xcb_poly_fill_rectangle (window::c_, this->id, desktop_pixmap::gc_, 4,
                           rectangles);

  // Changing color
  uint32_t mask = XCB_GC_FOREGROUND;
  values[0] = color;
  xcb_change_gc (c_, desktop_pixmap::gc_, mask, &values);
}