#include "drawable.hpp" // for drawable::c
#include "socket.hpp"   // for dxp_socket, read_error
#include "window.hpp"   // for window
#include "xcb_util.hpp" // for xcb_unique_ptr
#include <iostream>     // for operator<<, endl, basic_ostream, cerr, ostream
#include <memory>       // for allocator, unique_ptr, operator==
#include <stdexcept>    // for runtime_error
#include <xcb/xcb.h>    // for xcb_wait_for_event, xcb_generic_event_t

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
      // Get screenshots from socket or create them manually
      // if (dxp_do_screenshots)
      //   {
      dxp_socket client;
      auto v = client.get_desktops ();
      //   }
      // else
      //   {
      //     auto v = get_desktops ();
      //  }

      window w (v);
      auto windows = get_windows (window::c, window::root);

      // Handling incoming events
      // Freeing it with free() is not specified in the docs, but it works
      while (auto event = xcb_unique_ptr<xcb_generic_event_t> (
                 xcb_wait_for_event (window::c)))
        {
          if (event == nullptr)
            {
              throw read_error ("Could not read an event from X server");
            }
          if (w.handle_event (event.get ()) == 0)
            {
              return 0;
            }
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
