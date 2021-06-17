#include "Window.hpp"
#include <vector>
#include <config.hpp>

/* NOTE: First part is mostly close to DesktopPixmap */
Window::Window (
    const short x,          ///< x coordinate of the top left corner
    const short y,          ///< y coordinate of the top left corner
    const uint16_t width,   ///< width of display
    const uint16_t height,  ///< height of display
    const std::string &name, ///< _NET_WM_NAME of display
    const uint32_t parent
)
{
  this->x = x;
  this->y = y;
  this->width = width;
  this->height = height;
  this->name = name;
  this->b_width = 5;
  this->parent = parent;

  //Connection to X-server, as well as generating id before drawing window itself
  // Sets default parent to root
  if (this->parent == XCB_NONE) {
    Window::c_ = xcb_connect (NULL, NULL);
    Window::screen_ = xcb_setup_roots_iterator (xcb_get_setup (Window::c_)).data;
    Window::win_id = xcb_generate_id(this->c_);
    this->parent = this->screen_->root;
    createWindow();
  }
};

Window::~Window ()
{
  xcb_disconnect (Window::c_);
}

// TODO: Add support for masks if needed. Also ierarchy
void Window::createWindow ()
{
  //mask as for now aren't fully implemened. This one was partially copied from example_DesktopPixmap to make possible distinguishing of several windows' borders
  uint32_t mask;
  uint32_t values[3];

  mask = XCB_CW_BACK_PIXEL | XCB_CW_EVENT_MASK | XCB_CONFIG_WINDOW_STACK_MODE;
  values[0] = screen_->white_pixel;
  values[1] = XCB_EVENT_MASK_EXPOSURE | XCB_EVENT_MASK_KEY_PRESS;
  values[2] = XCB_STACK_MODE_ABOVE; 
  xcb_create_window (this->c_,                         /* Connection, separate from one of daemon */
                    XCB_COPY_FROM_PARENT,          /* depth (same as root)*/
                    this->win_id,                     /* window Id */
                    this->parent,                  /* parent window */
                    this->x, this->y,                          /* x, y */
                    this->width, this->height,             /* width, height */
                    this->b_width,                            /* border_width */
                    XCB_WINDOW_CLASS_INPUT_OUTPUT, /* class */
                    this->screen_->root_visual,           /* visual */
                    mask, values);                      /* masks, not used yet */

  /* Map the window on the screen */
  xcb_map_window (Window::c_, this->win_id);
  xcb_flush(this->c_);
};

void Window::connect_to_parent_Window (xcb_connection_t *c, xcb_screen_t *screen)
{
  Window::c_ = c;
  Window::screen_ = screen;
  Window::win_id = xcb_generate_id(c);
  createWindow();
  xcb_flush(this->c_);
};

int Window::getScreenPosition (int DesktopNumber, std::vector <dexpo_pixmap> v)
{
    int coord = k_dexpo_padding;
    for (const auto &dexpo_pixmap : v)
            if (dexpo_pixmap.desktop_number < DesktopNumber) 
            {
              coord += k_dexpo_padding;
              if (k_dexpo_height == 0) 
              {
                coord += dexpo_pixmap.height;
              }
              else coord += dexpo_pixmap.width;
            }
            else return coord;
}
