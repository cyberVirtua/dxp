#include "window.hpp"
#include "config.hpp"
#include "drawable.hpp"
#include <vector>
#include <xcb/xproto.h>

/* NOTE: First part is mostly close to DesktopPixmap */
window::window (const int16_t x,       ///< x coordinate of the top left corner
                const int16_t y,       ///< y coordinate of the top left corner
                const uint16_t width,  ///< width of display
                const uint16_t height, ///< height of display
                const std::string &name, ///< _NET_WM_NAME of display
                const uint32_t parent)
    : drawable (x, y, width, height)
{
  this->name = name;
  this->b_width = 0;
  this->parent = parent;

  // Connection to X-server, as well as generating id before drawing window
  // itself
  // Sets default parent to root
  if (this->parent == XCB_NONE)
    {
      this->win_id = xcb_generate_id (drawable::c_);
      this->parent = drawable::screen_->root;
      create_window ();
    }
};

window::~window () { xcb_destroy_window (drawable::c_, this->win_id); }

// TODO: Add support for masks if needed. Also ierarchy
void
window::create_window ()
{
  // mask as for now aren't fully implemened. This one was partially copied from
  // example_DesktopPixmap to make possible distinguishing of several windows'
  // borders
  uint32_t mask;
  uint32_t values[3];

  mask = XCB_CW_BACK_PIXEL | XCB_CW_EVENT_MASK | XCB_CONFIG_WINDOW_STACK_MODE;
  values[0] = screen_->white_pixel;
  values[1] = XCB_EVENT_MASK_EXPOSURE | XCB_EVENT_MASK_KEY_PRESS;
  values[2] = XCB_STACK_MODE_ABOVE;
  xcb_create_window (window::c_, /* Connection, separate from one of daemon */
                     XCB_COPY_FROM_PARENT,          /* depth (same as root)*/
                     this->win_id,                  /* window Id */
                     this->parent,                  /* parent window */
                     this->x, this->y,              /* x, y */
                     this->width, this->height,     /* width, height */
                     this->b_width,                 /* border_width */
                     XCB_WINDOW_CLASS_INPUT_OUTPUT, /* class */
                     window::screen_->root_visual,  /* visual */
                     mask, values);                 /* masks, not used yet */

  /* Map the window on the screen */
  xcb_map_window (window::c_, this->win_id);
  xcb_flush (this->c_);
};

void
window::connect_to_parent_window (xcb_connection_t *c, xcb_screen_t *screen)
{
  window::c_ = c;
  window::screen_ = screen;
  window::win_id = xcb_generate_id (c);
  create_window ();
  xcb_flush (window::c_);
};

int
window::get_screen_position (int DesktopNumber, std::vector<dexpo_pixmap> v)
{
  int coord = k_dexpo_padding;
  for (const auto &dexpo_pixmap : v)
    {
      if (dexpo_pixmap.desktop_number < DesktopNumber)
        {
          coord += k_dexpo_padding;
          if (k_dexpo_height == 0)
            {
              coord += dexpo_pixmap.height;
            }
          else
            {
              coord += dexpo_pixmap.width;
            }
        }
      else
        {
          return coord;
        }
    }
}
