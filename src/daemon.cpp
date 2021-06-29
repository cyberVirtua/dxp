#include "daemon.hpp"
#include "config.hpp"
#include <thread>

dxp_daemon::dxp_daemon ()
{
  this->c = xcb_connect (nullptr, nullptr);
  this->screen = xcb_setup_roots_iterator (xcb_get_setup (this->c)).data;
  this->root = this->screen->root;

  auto desktops_info = get_desktops (c, root);

  /* Initializing desktop objects. They are each binded to a separate
   * virtual desktop */

  desktops.reserve (desktops_info.size ());
  for (const auto &d : desktops_info)
    {
      desktops.emplace_back (d.x, d.y, d.width, d.height);
    }

  /* Initializing pixmaps that will be shared over socket They are a different
   * datatype from the desktops as they don't store useless data.
   *
   * Copying only useful data from the desktop objects. */

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
}

void
dxp_daemon::run ()
{
  // Start a server that will share pixmaps over socket in a separate thread
  std::thread daemon_thread (&dxp_socket::send_desktops_on_event, &server,
                             std::ref (this->socket_desktops),
                             std::ref (this->socket_desktops_lock));

  while (this->running)
    {
      auto current = size_t (get_current_desktop (this->c, this->root));
      if (!dexpo_viewport.empty () && current >= dexpo_viewport.size () / 2)
        {
          throw std::runtime_error (
              "The amount of virtual desktops specified in the config does not "
              "match the amount of your virtual deskops in your system.");
        }

      this->socket_desktops_lock.lock ();
      this->desktops[current].save_screen ();

      // Copying pixmap from desktops into socket pixmaps
      this->socket_desktops[current].pixmap = this->desktops[current].pixmap;

      this->socket_desktops_lock.unlock ();

      std::this_thread::sleep_for (dexpo_screenshot_timeout);
    };
}
