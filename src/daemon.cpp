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
 * Get array of desktop_info
 */
std::vector<desktop_info>
get_desktops_info ()
{
  // TODO Real parser
  /**
   * PLAN:
   *
   * 1. Get a number of desktops:
   *     getatom("NET_NUMBER_OF_DESKTOPS")
   * 2. Get their names:
   *     getatom("NET_DESKTOP_NAMES")
   * 3.  For each name in c, get their atom . I'll write that. store in smth
   *     like z2 may need to rewrite getatom. Do atoms count as drawables?
   * 4.  For each atom in z2, get their geometries, (g), viewports (v)
   * 5.  d[i]={i,v[2*i], v[2*i+1], g[2*i], g[2*i+1], z[i]} profit.
   */
  auto monitor_info = get_screen_data ();
  auto *workarea = reinterpret_cast<int *> (atom_parser ("_NET_WORKAREA"));

  auto total_desktops
      = (reinterpret_cast<int *> (atom_parser ("_NET_NUMBER_OF_DESKTOPS")))[0];

  auto *names
      = reinterpret_cast<const char **> (atom_parser ("_NET_DESKTOP_NAMES"));

  std::vector<desktop_info> info;

  for (int d_num = 0; d_num < total_desktops; d_num++)
    {
      int min = 3000;
      int assigned_monitor = 0;

      // Find minimal delta monitors/workareas
      for (int size = 0; size < int (monitor_info.size ()) / 4; size++)
        {
          if (min > monitor_info[4 * size + 3] - workarea[4 * size + 3])
            {
              min = monitor_info[4 * size + 3] - workarea[4 * size + 3];
              assigned_monitor = size;
            }
        }
      info.push_back ({ d_num, monitor_info[4 * assigned_monitor],
                        monitor_info[4 * assigned_monitor + 1],
                        monitor_info[4 * assigned_monitor + 2],
                        monitor_info[4 * assigned_monitor + 3], names[d_num] });
    }
  // auto d0 = desktop_info{ 0, 0, 0, 1080, 1920, "Desktop" };
  // auto d1 = desktop_info{ 1, 1080, 0, 3440, 1440, "first" };
  // auto d2 = desktop_info{ 2, 1080, 0, 3440, 1440, "second" };
  // auto d3 = desktop_info{ 3, 1080, 0, 3440, 1440, "third" };
  // auto d4 = desktop_info{ 4, 1080, 0, 3440, 1440, "fourth" };
  return info;
  // return std::vector<desktop_info>{ d0, d1, d2, d3, d4 };
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
