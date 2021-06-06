#include "DesktopPixmap.hpp"
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
  // NOTE Big values don't work. Can't screenshot entire screen
  DesktopPixmap d (1080, 0, 1440, 1440, "DP-1");

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

  d.saveImage ();
  d.putImage ();

  for (int i = 0; i < 1000; i += 4)
    {
      std::cout << static_cast<int> (d.image_ptr[i]) << " "
                << static_cast<int> (d.image_ptr[i + 1]) << " "
                << static_cast<int> (d.image_ptr[i + 2]) << "\n";
    }
  std::cout << std::endl;

  // Mapping pixmap onto window
  while (1)
    {
      usleep (1000);

      d.saveImage ();
      d.putImage ();

      xcb_copy_area (c, d.pixmap_id, window_id, DesktopPixmap::gc_, 0, 0, 0, 0,
                     d.height, d.width);

      /* We flush the request */
      xcb_flush (c);
    }
}
