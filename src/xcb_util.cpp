#include "xcb_util.hpp"
#include "config.hpp"
#include "window.hpp"
#include <cstring>
#include <memory>
#include <xcb/xcb.h>

/**
 * Get a vector with EWMH property values
 *
 * @note vector size is inconsistent so vector may contain other data
 */
std::vector<uint32_t>
get_property_value (xcb_connection_t *c, xcb_window_t root,
                    const char *atom_name)
{
  auto atom_cookie = xcb_intern_atom (c, 0, strlen (atom_name), atom_name);
  auto atom_reply = xcb_unique_ptr<xcb_intern_atom_reply_t> (
      xcb_intern_atom_reply (c, atom_cookie, nullptr));

  // TODO(mmskv): add proper exception
  auto atom = atom_reply ? atom_reply->atom : throw;

  /* Getting property from atom */

  auto prop_cookie = xcb_get_property (
      c, 0, root, atom, XCB_GET_PROPERTY_TYPE_ANY, 0, UINT32_MAX);
  auto prop_reply = xcb_unique_ptr<xcb_get_property_reply_t> (
      xcb_get_property_reply (c, prop_cookie, nullptr));

  // auto prop_length = size_t (xcb_get_property_value_length (prop_reply.get
  // ()));
  auto prop_length = prop_reply->length;

  // This shouldn't be unique_ptr as it belongs to prop_reply and
  // will be freed by prop_reply
  auto *prop = prop_length ? (reinterpret_cast<uint32_t *> (
                   xcb_get_property_value (prop_reply.get ())))
                           : throw;

  std::vector<uint32_t> atom_data;

  atom_data.reserve (prop_length);
  for (size_t i = 0; i < prop_length; i++)
    {
      atom_data.push_back (prop[i]);
    }

  return atom_data;
};

/**
 * Get monitor_info for each connected monitor
 */
std::vector<monitor_info>
get_monitors (xcb_connection_t *c, xcb_window_t root)
{
  std::vector<monitor_info> monitors;

  auto screen_resources_reply
      = xcb_unique_ptr<xcb_randr_get_screen_resources_current_reply_t> (
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

      // Using free instead of delete to deallocate memory as required by xcb
      auto output = xcb_unique_ptr<xcb_randr_get_output_info_reply_t> (
          xcb_randr_get_output_info_reply (c, output_cookie, nullptr));

      if (output == nullptr || output->crtc == XCB_NONE
          || output->connection == XCB_RANDR_CONNECTION_DISCONNECTED)
        {
          continue;
        }

      auto crtc = xcb_unique_ptr<xcb_randr_get_crtc_info_reply_t> (
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
get_desktops (xcb_connection_t *c, xcb_window_t root)
{
  auto monitors = get_monitors (c, root);

  auto number_of_desktops
      = !dexpo_viewport.empty () /* If config is not empty: */
            // Set amount of desktops to the one specified in the config
            ? dexpo_viewport.size () / 2
            // Otherwise parse amount of desktops from EWMH
            : get_property_value (c, root, "_NET_NUMBER_OF_DESKTOPS")[0];

  auto viewport = !dexpo_viewport.empty () /* If config is not empty: */
                      // Set viewport to the one specified in the config
                      ? dexpo_viewport
                      // Otherwise parse viewport from EWMH
                      : get_property_value (c, root, "_NET_DESKTOP_VIEWPORT");

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
      // May happen if monitors have some weird configuration or not all
      // desktops are specified in dexpo_viewport
      if (width == 0 || height == 0)
        {
          throw std::runtime_error (
              "Couldn't parse your desktops. Perhaps your window manager does "
              "not support large desktops and _NET_DESKTOP_VIEWPORT property "
              "is set to (0,0).\nYou might want to manually specify your "
              "desktop coordinates in the src/config.hpp. See "
              "`dexpo_viewport`.");
        }

      info.push_back (desktop_info{ int (i), x, y, width, height });
    }

  return info;
}

/**
 * Get value of _NET_CURRENT_DESKTOP
 */
int
get_current_desktop (xcb_connection_t *c, xcb_window_t root)
{
  return int (get_property_value (c, root, "_NET_CURRENT_DESKTOP")[0]);
};

constexpr uint8_t k_event_data32_length = 5;
/**
 * Generating and sending client message to the x server
 */
void
send_xcb_message (xcb_connection_t *c, xcb_screen_t *screen, const char *msg,
                  const std::array<uint32_t, k_event_data32_length> &data)
{

  auto atom_cookie = xcb_intern_atom (c, 0, strlen (msg), msg);
  auto atom_reply = xcb_unique_ptr<xcb_intern_atom_reply_t> (
      xcb_intern_atom_reply (c, atom_cookie, nullptr));
  auto atom_id = atom_reply->atom; /// Id of request

  // Building an event to send
  xcb_client_message_event_t event; /// Name of the event
  event.response_type = XCB_CLIENT_MESSAGE;
  // EWMH asks to set format to 32
  // https://specifications.freedesktop.org/wm-spec/wm-spec-1.3.html
  constexpr uint8_t k_net_current_desktop_format = 32;
  event.format = k_net_current_desktop_format;
  event.sequence = 0;
  event.window = screen->root;
  event.type = atom_id;
  std::copy (data.begin (), data.end (), std::begin (event.data.data32));
  xcb_send_event (c, 0, screen->root, XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT,
                  reinterpret_cast<const char *> (&event));
  // xcb_flush (c);
}

/**
 * Changing the number of desktop
 */
void
ewmh_change_desktop (xcb_connection_t *c, xcb_screen_t *screen, uint destkop_id)
{
  // Setting data[1]=timestamp to zero
  // EWMH: Note that the timestamp may be 0 for clients using an older version
  // of this spec, in which case the timestamp field should be ignored.
  send_xcb_message (c, screen, "_NET_CURRENT_DESKTOP", { destkop_id });
}
