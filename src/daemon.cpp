#include "desktop_pixmap.hpp"
#include "dexpo_socket.hpp"
#include <iostream>
#include <memory>
#include <string.h>
#include <sys/un.h>
#include <unistd.h>
#include <vector>
#include <xcb/randr.h>
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
std::vector<uint32_t>
atom_parser (const char *atom_name)
{
  auto atom_cookie = xcb_intern_atom (c, 0, strlen (atom_name), atom_name);
  auto atom_reply = std::unique_ptr<xcb_intern_atom_reply_t> (
      xcb_intern_atom_reply (c, atom_cookie, nullptr));

  // TODO add proper exception
  auto atom = atom_reply ? atom_reply->atom : throw;

  /* Getting property from atom */

  auto prop_cookie = xcb_get_property (
      c, 0, root, atom, XCB_GET_PROPERTY_TYPE_ANY, 0, UINT32_MAX);
  auto prop_reply = std::unique_ptr<xcb_get_property_reply_t> (
      xcb_get_property_reply (c, prop_cookie, nullptr));

  int prop_length = xcb_get_property_value_length (prop_reply.get ());

  auto *prop = prop_length ? (reinterpret_cast<uint32_t *> (
                   xcb_get_property_value (prop_reply.get ())))
                           : throw;

  std::vector<uint32_t> atom_data;

  for (int i = 0; i < prop_length; i++)
    {
      atom_data.push_back (prop[i]);
    }

  return atom_data;
};

struct monitor_info
{
  int x;
  int y;
  int width;
  int height;
};

std::vector<monitor_info>
get_screen_data ()
{
  std::vector<monitor_info> monitors;

  auto screen_resources_reply
      = std::unique_ptr<xcb_randr_get_screen_resources_current_reply_t> (
          xcb_randr_get_screen_resources_current_reply (
              c, xcb_randr_get_screen_resources_current (c, root), nullptr));

  auto *randr_outputs = xcb_randr_get_screen_resources_current_outputs (
      screen_resources_reply.get ());

  int len = xcb_randr_get_screen_resources_current_outputs_length (
      screen_resources_reply.get ());

  for (int i = 0; i < len; i++)
    {
      auto output_cookie = xcb_randr_get_output_info (c, randr_outputs[i],
                                                      XCB_TIME_CURRENT_TIME);

      auto output = std::unique_ptr<xcb_randr_get_output_info_reply_t> (
          xcb_randr_get_output_info_reply (c, output_cookie, nullptr));

      if (output == nullptr || output->crtc == XCB_NONE
          || output->connection == XCB_RANDR_CONNECTION_DISCONNECTED)
        {
          continue;
        }

      auto crtc = std::unique_ptr<xcb_randr_get_crtc_info_reply_t> (
          xcb_randr_get_crtc_info_reply (
              c, xcb_randr_get_crtc_info (c, output->crtc, XCB_CURRENT_TIME),
              nullptr));

      monitor_info m{ crtc->x, crtc->y, crtc->width, crtc->height };
      monitors.push_back (m);
    }
  return monitors;
}

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
 * Get an array of desktop_info.
 *
 * FIXME Uses EWMH and won't work if WM doesn't support large desktops
 */
std::vector<desktop_info>
get_desktops_info ()
{
  auto monitors = get_screen_data ();

  auto number_of_desktops = atom_parser ("_NET_NUMBER_OF_DESKTOPS")[0];

  auto viewport = atom_parser ("_NET_DESKTOP_VIEWPORT");

  // TODO Parse names

  std::vector<desktop_info> info;

  for (size_t i = 0; i < number_of_desktops; i++)
    {
      int x = int (viewport[i * 2]);
      int y = int (viewport[i * 2 + 1]);
      int width = 0;
      int height = 0;

      for (const auto &monitor : monitors)
        {
          // Finding monitor on which the desktop is located
          if (monitor.x == x && monitor.y == y)
            {
              width = monitor.width;
              height = monitor.height;
            }
        }

      // This should not happen
      // May happen if monitors have some weird configuration
      if (width == 0 || height == 0)
        {
          throw; // TODO Log errors
        }

      info.push_back (desktop_info{ int (i), x, y, width, height, "" });
    }

  return info;
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
  auto a = get_desktops_info ();
  exit (0);
  auto desktop_viewports = get_desktops_info ();
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
