#include "config.hpp"
#include "desktop.hpp"
#include "socket.hpp"
#include "xcb_util.hpp"
#include <iostream>
#include <memory>
#include <sys/un.h>
#include <thread>
#include <unistd.h>
#include <vector>
#include <xcb/randr.h>
#include <xcb/xcb.h>
#include <xcb/xproto.h>

auto *c = xcb_connect (nullptr, nullptr);
auto *screen = xcb_setup_roots_iterator (xcb_get_setup (c)).data;
auto root = screen -> root;

/**
 * Parse monitor dimensions and initialize appropriate pixmap objects.
 *
 * At a timeout check current display and screenshot it.
 * Start a socket listener.
 *
 * TODO Handle workspace deletions
 * TODO Handle errors
 */
int
main ()
{
  auto desktops_info = get_desktops (c, root);

  /* Initializing desktop objects. They are each binded to a separate
   * virtual desktop */

  std::vector<dxp_desktop> desktops{};

  desktops.reserve (desktops_info.size ());
  for (const auto &d : desktops_info)
    {
      desktops.emplace_back (d.x, d.y, d.width, d.height);
    }

  /* Initializing pixmaps that will be shared over socket They are a different
   * datatype from the desktops as they don't store useless data.
   *
   * Copying only useful data from the desktop objects. */

  std::vector<dxp_socket_desktop> socket_desktops{};
  for (size_t i = 0; i < desktops.size (); i++)
    {
      dxp_socket_desktop p;

      p.id = i;
      p.pixmap_len = desktops[i].pixmap_width * desktops[i].pixmap_height * 4U;
      p.width = desktops[i].pixmap_width;
      p.height = desktops[i].pixmap_height;

      // Copying pixmap from desktops into socket pixmaps
      p.pixmap = desktops[i].pixmap; // Should be as fast as memcpy

      socket_desktops.push_back (p);
    }

  dxp_socket server;

  std::mutex socket_desktops_lock;
  // Start a server that will share pixmaps over socket in a separate thread
  std::thread daemon_thread (&dxp_socket::send_pixmaps_on_event, &server,
                             std::ref (socket_desktops),
                             std::ref (socket_desktops_lock));

  bool running = true;
  while (running)
    {
      auto current = size_t (get_current_desktop (c, root));
      if (!dexpo_viewport.empty () && current >= dexpo_viewport.size () / 2)
        {
          throw std::runtime_error (
              "The amount of virtual desktops specified in the config does not "
              "match the amount of your virtual deskops in your system.");
        }

      socket_desktops_lock.lock ();
      desktops[current].save_screen ();

      // Copying pixmap from desktops into socket pixmaps
      socket_desktops[current].pixmap = desktops[current].pixmap;

      socket_desktops_lock.unlock ();

      std::this_thread::sleep_for (dexpo_screenshot_timeout);
    };

  return 0;
}
