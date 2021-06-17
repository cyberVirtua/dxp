#include "desktop_pixmap.hpp"
#include "dexpo_socket.hpp"
#include <string.h>
#include <sys/un.h>
#include <vector>
#include <xcb/xcb.h>
#include <xcb/xproto.h>

struct desktop_info
{
  int desktop_number; // XXX Is it necessary?
  int x;
  int y;
  int width;
  int height;
  std::string name;
};

/**
 * Get array of desktop_info
 */
std::vector<desktop_info>
get_desktop_viewport ()
{
  // TODO Real parser
  auto d0 = desktop_info{ 0, 0, 0, 1080, 1920, "Desktop" };
  auto d1 = desktop_info{ 1, 1080, 0, 3440, 1440, "first" };
  auto d2 = desktop_info{ 2, 1080, 0, 3440, 1440, "second" };
  auto d3 = desktop_info{ 3, 1080, 0, 3440, 1440, "third" };
  auto d4 = desktop_info{ 4, 1080, 0, 3440, 1440, "fourth" };

  return std::vector<desktop_info>{ d0, d1, d2, d3, d4 };
}

/**
 * Get value of _NET_CURRENT_DESKTOP atom
 */
int
get_current_desktop ()
{
  return 1;
};

/**
 * Parse monitor dimensions and initialize appropriate desktop_pixmaps.
 * At a timeout check current display and screenshot it (in a separate
 * thread). Start a socket listner.
 */
int
main ()
{
  auto desktop_viewports = get_desktop_viewport ();
  std::vector<desktop_pixmap> desktop_pixmaps;
  for (const auto &d : desktop_viewports)
    {
      desktop_pixmaps.push_back (
          desktop_pixmap (d.x, d.y, d.width, d.height, d.name));
    }

  std::vector<dexpo_pixmap> dexpo_pixmaps;

  for (size_t i = 0; i < desktop_pixmaps.size (); i++)
    {
      dexpo_pixmaps.push_back (
          dexpo_pixmap{ static_cast<int> (i),
                        desktop_pixmaps[i].width,
                        desktop_pixmaps[i].height,
                        desktop_pixmaps[i].pixmap_id,
                        { *desktop_pixmaps[i].name.c_str () } });
    }

  auto server = dexpo_socket ();
  std::mutex dexpo_pixmaps_lock;
  server.send_pixmaps_on_event (dexpo_pixmaps, dexpo_pixmaps_lock);

  bool running = true;
  while (running)
    {
      size_t c = size_t (get_current_desktop ());
      dexpo_pixmaps_lock.lock ();
      desktop_pixmaps[c].save_screen ();
      dexpo_pixmaps_lock.unlock ();
      sleep (2);
    };

  return 0;
}
