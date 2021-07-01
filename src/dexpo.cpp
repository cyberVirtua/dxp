#include "config.hpp"
#include "socket.hpp"
#include "window.hpp"
#include "xcb_util.hpp"
#include <iostream>
#include <unistd.h>
#include <vector>
#include <xcb/xcb.h>

/**
 * 1. Get desktops from daemon
 * 2. Calculate actual window dimensions
 * 3. Create window and map it onto screen
 * 4. Wait for events and handle them
 */
int
main ()
{
  try
    {
      // Get screenshots from socket
      dxp_socket client;
      auto v = client.get_desktops ();

      window w (dexpo_x, dexpo_y, v);

      // Handling incoming events
      // Freeing it with free() is not specified in the docs, but it works
      while (auto event = xcb_unique_ptr<xcb_generic_event_t> (
                 xcb_wait_for_event (window::c_)))
        {
          if (w.handle_event (event.get ()) == 0)
            {
              return 0;
            };
        }
    }
  catch (const std::runtime_error &e)
    {
      std::cerr << e.what () << std::endl;
      return 0;
    }
  catch (...)
    {
      std::cerr << "Unknown error occured" << std::endl;
      return 0;
    }
}
