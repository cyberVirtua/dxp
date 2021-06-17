#include "desktop_pixmap.hpp"
#include <iostream>
#include <ostream>
#include <unistd.h>
#include <xcb/xcb.h>
#include <xcb/xproto.h>

int
main ()
{
  xcb_connection_t *c;
  xcb_window_t window_id;
  xcb_screen_t *screen;

  // NOTE Change dimensions to fit your display
  // FIXME Implement screenshotter that will not capture entire screen at once
  // malloc() can reserve only 16711568 bytes for a pixmap
  // But ZPixmap of QHD Ultrawide is 3440 * 1440 * 4 = 19814400 bytes long
  desktop_pixmap d (0, 0, 1920, 1080, "DP-1");//magic numbers!!!

  // connect to the X server and get screen
  c = xcb_connect (NULL, NULL);
  screen = xcb_setup_roots_iterator (xcb_get_setup (c)).data;

  uint32_t mask;
  uint32_t values[2];

  mask = XCB_CW_BACK_PIXEL | XCB_CW_EVENT_MASK;
  values[0] = screen->white_pixel;
  values[1] = XCB_EVENT_MASK_EXPOSURE | XCB_EVENT_MASK_KEY_PRESS;

  window_id = xcb_generate_id (c);
  xcb_create_window (c,                       /* connection */
                     XCB_COPY_FROM_PARENT,    /* TODO */
                     window_id,               /* window_id for new window */
                     screen->root,            /* parent */
                     0, 0, d.width, d.height, /* dimensions */
                     0,                       /* border width */
                     XCB_WINDOW_CLASS_INPUT_OUTPUT, /* class bit mask */
                     screen->root_visual,           /* id for the visual */
                     mask,                          /* TODO event mask?*/
                     values);

  xcb_map_window (c, window_id);

  d.save_screen ();
  // Mapping pixmap onto window
  while (1)
    {
      usleep (10000);

      xcb_copy_area (c, d.pixmap_id, window_id, desktop_pixmap::gc_, 0, 0, 0, 0,
                     d.width, d.height);

      /* We flush the request */
      xcb_flush (c);
    }
}