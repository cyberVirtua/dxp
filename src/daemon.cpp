#include "config.hpp"
#include "desktop.hpp"
#include "socket.hpp"
#include <iostream>
#include <memory>
#include <string.h>
#include <sys/un.h>
#include <thread>
#include <unistd.h>
#include <vector>
#include <xcb/randr.h>
#include <xcb/xcb.h>
#include <xcb/xproto.h>

/**
 * Monitor information
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
 * Virtual desktop information
 */
struct desktop_info
{
  int id; // XXX Is it necessary?
  int x;
  int y;
  int width;
  int height;
};

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

/**
 * Get an array of desktop_info.
 *
 * FIXME Uses EWMH and won't work if WM doesn't support large desktops
 */
std::vector<desktop_info>
get_desktops ()
{
  auto monitors = get_monitors ();

  auto number_of_desktops
      = !dexpo_viewport.empty () /* If config is not empty: */
            // Set amount of desktops to the one specified in the config
            ? dexpo_viewport.size () / 2
            // Otherwise parse amount of desktops from EWMH
            : get_property_value ("_NET_NUMBER_OF_DESKTOPS")[0];

  auto viewport = !dexpo_viewport.empty () /* If config is not empty: */
                      // Set viewport to the one specified in the config
                      ? dexpo_viewport
                      // Otherwise parse viewport from EWMH
                      : get_property_value ("_NET_DESKTOP_VIEWPORT");

  // TODO Parse names

  std::vector<desktop_info> info;

  // Figuring out width and height of a desktop based on existing
  // monitors and desktop x, y coordinates
  for (size_t i = 0; i < number_of_desktops; i++)
    {
      int x = int (viewport[i * 2]);
      int y = int (viewport[i * 2 + 1]);
      int width = 0;
      int height = 0;

      for (const auto &m : monitors)
        {
          // Finding monitor the desktop belongs to
          if (m.x == x && m.y == y)
            {
              width = m.width;
              height = m.height;
            }
        }

      // This should not happen
      // May happen if monitors have some weird configuration
      if (width == 0 || height == 0)
        {
          throw; // TODO Log errors
        }

      info.push_back (desktop_info{ int (i), x, y, width, height });
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
  auto desktops_info = get_desktops ();

  /* Initializing desktop objects. They are each binded to a separate
   * virtual desktop */

  std::vector<dxp_desktop> desktops{};
  for (const auto &d : desktops_info)
    {
      desktops.push_back (dxp_desktop (d.x, d.y, d.width, d.height));
    }

  /* Initializing pixmaps that will be shared over socket They are a different
   * datatype from the desktops as they don't store useless data.
   *
   * Copying only useful data from the desktop objects. */

  std::vector<dxp_socket_desktop> socket_desktops{};
  for (size_t i = 0; i < desktops.size (); i++)
    {
      uint32_t pixmap_len
          = desktops[i].pixmap_width * desktops[i].pixmap_height * 4;

      dxp_socket_desktop p;

      p.id = int (i);
      p.pixmap_len = pixmap_len;
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
      size_t c = size_t (get_current_desktop ());
      if (!dexpo_viewport.empty () && c >= dexpo_viewport.size () / 2)
        {
          throw std::runtime_error (
              "The amount of virtual desktops specified in the config does not "
              "match the amount of your virtual deskops in your system.");
        }

      socket_desktops_lock.lock ();
      desktops[c].save_screen ();

      // Copying pixmap from desktops into socket pixmaps
      socket_desktops[c].pixmap = desktops[c].pixmap;

      socket_desktops_lock.unlock ();

      usleep (dexpo_screenshot_timeout * 1000000); // usec to sec
    };

  return 0;
}
