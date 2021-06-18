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
 * Get a vector with EWMH property values
 *
 * @note vector size is inconsistent so vector may contain other data
 */
std::vector<uint32_t>
get_property_value (const char *atom_name)
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

  // This shouldn't be unique_ptr as it belongs to prop_reply and
  // will be freed by prop_reply
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

/**
 * Information about your connected monitor.
 *
 * Used to calculate sizes of virtual desktops
 */
struct monitor_info
{
  int x;
  int y;
  int width;
  int height;
};

/**
 * Get monitor_info for each connected monitor
 */
std::vector<monitor_info>
get_monitors ()
{
  std::vector<monitor_info> monitors;

  auto screen_resources_reply
      = std::unique_ptr<xcb_randr_get_screen_resources_current_reply_t> (
          xcb_randr_get_screen_resources_current_reply (
              c, xcb_randr_get_screen_resources_current (c, root), nullptr));

  // This shouldn't be unique_ptr as it belongs to screen_resources_reply and
  // will be freed by screen_resources_reply
  auto *randr_outputs = xcb_randr_get_screen_resources_current_outputs (
      screen_resources_reply.get ());

  int len = xcb_randr_get_screen_resources_current_outputs_length (
      screen_resources_reply.get ());

  // Iterating over all randr outputs. They correspond to GPU ports and other
  // things so a lot are disconnected. Such monitors will be caught with if
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
get_desktops ()
{
  auto monitors = get_monitors ();

  auto number_of_desktops = get_property_value ("_NET_NUMBER_OF_DESKTOPS")[0];

  auto viewport = get_property_value ("_NET_DESKTOP_VIEWPORT");

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
 * Get value of _NET_CURRENT_DESKTOP
 */
int
get_current_desktop ()
{
  return int (get_property_value ("_NET_CURRENT_DESKTOP")[0]);
};

/**
 * Parse monitor dimensions and initialize appropriate desktop_pixmaps.
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
  auto desktops = get_desktops ();

  /* Initializing desktop_pixmap objects */
  std::vector<desktop_pixmap> pixmaps;
  for (const auto &d : desktops)
    {
      pixmaps.push_back (desktop_pixmap (d.x, d.y, d.width, d.height, d.name));
    }

  /* Initializing pixmaps that will be shared over socket */
  std::vector<dexpo_pixmap> socket_pixmaps;
  for (size_t i = 0; i < pixmaps.size (); i++)
    {
      socket_pixmaps.push_back (dexpo_pixmap{ static_cast<int> (i),
                                              pixmaps[i].width,
                                              pixmaps[i].height,
                                              pixmaps[i].pixmap_id,
                                              { *pixmaps[i].name.c_str () } });
    }

  dexpo_socket server;

  std::mutex socket_pixmaps_lock;
  // Start a server that will share pixmaps over socket in a separate thread
  server.send_pixmaps_on_event (socket_pixmaps, socket_pixmaps_lock);

  bool running = true;
  while (running)
    {
      size_t c = size_t (get_current_desktop ());

      socket_pixmaps_lock.lock ();
      pixmaps[c].save_screen ();
      socket_pixmaps_lock.unlock ();

      sleep (1);
    };

  return 0;
}
