#include "desktop_pixmap.hpp"
#include "dexpo_socket.hpp"
#include <string.h>
#include <sys/un.h>
#include <unistd.h>
#include <vector>
#include <xcb/xcb.h>
#include <xcb/xproto.h>

auto *c = xcb_connect (nullptr, nullptr);
auto *screen = xcb_setup_roots_iterator (xcb_get_setup (c)).data;
auto root = screen -> root;

/**
 *
 * @return array
 *
 * usage:
 * auto a=atom_parser(c,screen->root, "_NET_NUMBER_OF_DESKTOPS");
 * std::cout<<"Desktops: "<<a[0]<<'\n';
 */
auto
atom_parser (char *atom_name)
{
  // get atom itself
  xcb_intern_atom_cookie_t atom_cookie;
  xcb_atom_t atom;
  xcb_intern_atom_reply_t *rep;

  atom_cookie = xcb_intern_atom (c, 0, strlen (atom_name), atom_name);
  rep = xcb_intern_atom_reply (c, atom_cookie, NULL);
  if (NULL != rep)
    {
      atom = rep->atom;
      free (rep);
    }
  else
    {
      throw; // TODO Add exception
    }

  // get property from atom

  xcb_get_property_cookie_t prop_cookie;
  xcb_get_property_reply_t *reply_prop;

  prop_cookie
      = xcb_get_property (c, 0, root, atom, XCB_GET_PROPERTY_TYPE_ANY, 0, 128);
  reply_prop = xcb_get_property_reply (c, prop_cookie, nullptr);

  int value_len
      = xcb_get_property_value_length (reply_prop); // kind of handles errors

  if (value_len != 0)
    {
      auto a = (uint32_t *)xcb_get_property_value (reply_prop);
      return a;
    }
  else
    {
      throw; // TODO Add exception
    }
};

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

  dexpo_socket server{};

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
