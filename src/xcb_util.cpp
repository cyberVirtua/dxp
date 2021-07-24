#include "xcb_util.hpp"
#include "config.hpp"  // for dxp_viewport
#include <cstdint>     // for uint32_t, uint8_t, UINT32_MAX
#include <cstring>     // for strlen
#include <xcb/randr.h> // for xcb_randr_get_crtc_info_reply_t, xcb_randr_ge...
#include <xcb/xcb.h>   // for xcb_connection_t, xcb_generic_error_t, XCB_CU...

/**
 * Check if got an XCB error and throw it in case
 */
void
check (xcb_generic_error_t *e, const std::string &msg)
{
  if (e != nullptr)
    {
      throw xcb_error (e, msg);
    }
};

/**
 * Get a vector with EWMH property values
 *
 * @note vector size is inconsistent so vector may contain other data
 */
std::vector<uint32_t>
get_property_value (xcb_connection_t *c, xcb_window_t root,
                    const char *atom_name)
{
  xcb_generic_error_t *e = nullptr; // TODO(mmskv): Check for memory leak

  auto atom_cookie = xcb_intern_atom (c, 0, strlen (atom_name), atom_name);
  auto atom_reply = xcb_unique_ptr<xcb_intern_atom_reply_t> (
      xcb_intern_atom_reply (c, atom_cookie, &e));
  check (e, "XCB error while getting atom reply");

  auto atom = atom_reply
                  ? atom_reply->atom
                  : throw std::runtime_error (
                      std::string ("Could not get atom for ") + atom_name);

  /* Getting property from atom */

  auto prop_cookie = xcb_get_property (
      c, 0, root, atom, XCB_GET_PROPERTY_TYPE_ANY, 0, UINT32_MAX);
  auto prop_reply = xcb_unique_ptr<xcb_get_property_reply_t> (
      xcb_get_property_reply (c, prop_cookie, &e));
  check (e, "XCB error while getting property reply");

  auto prop_length = prop_reply->length;

  // This shouldn't be unique_ptr as it belongs to prop_reply and
  // will be freed by prop_reply
  auto *prop = prop_length ? (reinterpret_cast<uint32_t *> (
                   xcb_get_property_value (prop_reply.get ())))
                           : throw;

  std::vector<uint32_t> atom_data;

  atom_data.reserve (prop_length);
  for (uint i = 0; i < prop_length; i++)
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
  xcb_generic_error_t *e = nullptr;
  std::vector<monitor_info> monitors;

  auto screen_resources_reply
      = xcb_unique_ptr<xcb_randr_get_screen_resources_current_reply_t> (
          xcb_randr_get_screen_resources_current_reply (
              c, xcb_randr_get_screen_resources_current (c, root), &e));
  check (e, "XCB error while getting randr screen resources reply");

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
          xcb_randr_get_output_info_reply (c, output_cookie, &e));
      check (e, "XCB error while getting randr info reply");

      if (output == nullptr || output->crtc == XCB_NONE
          || output->connection == XCB_RANDR_CONNECTION_DISCONNECTED)
        {
          continue;
        }
      std::string name (reinterpret_cast<char *> (
                            xcb_randr_get_output_info_name (output.get ())),
                        output->name_len);

      auto crtc = xcb_unique_ptr<xcb_randr_get_crtc_info_reply_t> (
          xcb_randr_get_crtc_info_reply (
              c, xcb_randr_get_crtc_info (c, output->crtc, XCB_CURRENT_TIME),
              &e));
      check (e, "XCB error while getting randr info reply");

      monitor_info m{ crtc->x, crtc->y, crtc->width, crtc->height, name };
      monitors.push_back (m);
    }
  return monitors;
}

/**
 * Get monitor_info of the primary monitor.
 */
monitor_info
get_monitor_primary (xcb_connection_t *c, xcb_window_t root)
{

  xcb_generic_error_t *e = nullptr;
  auto primary_cookie = xcb_randr_get_output_primary (c, root);

  auto primary = xcb_unique_ptr<xcb_randr_get_output_primary_reply_t> (
      xcb_randr_get_output_primary_reply (c, primary_cookie, &e));
  check (e, "XCB error while getting primary monitor reply");

  auto primary_info
      = xcb_randr_get_output_info (c, primary->output, XCB_CURRENT_TIME);

  auto primary_output = xcb_unique_ptr<xcb_randr_get_output_info_reply_t> (
      xcb_randr_get_output_info_reply (c, primary_info, &e));
  check (e, "XCB error while getting randr info reply");

  auto crtc = xcb_unique_ptr<xcb_randr_get_crtc_info_reply_t> (
      xcb_randr_get_crtc_info_reply (
          c,
          xcb_randr_get_crtc_info (c, primary_output->crtc, XCB_CURRENT_TIME),
          &e));
  check (e, "XCB error while getting randr info reply");

  return monitor_info{ crtc->x, crtc->y, crtc->width, crtc->height, "" };
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
      = !dxp_viewport.empty () /* If config is not empty: */
            // Set amount of desktops to the one specified in the config
            ? dxp_viewport.size () / 2
            // Otherwise parse amount of desktops from EWMH
            : get_property_value (c, root, "_NET_NUMBER_OF_DESKTOPS")[0];

  auto viewport = !dxp_viewport.empty () /* If config is not empty: */
                      // Set viewport to the one specified in the config
                      ? dxp_viewport
                      // Otherwise parse viewport from EWMH
                      : get_property_value (c, root, "_NET_DESKTOP_VIEWPORT");

  std::vector<desktop_info> info;

  // Figuring out width and height of a desktop based on existing
  // monitors and desktop x, y coordinates
  for (uint i = 0; i < number_of_desktops; i++)
    {
      int x = viewport[i * 2];
      int y = viewport[i * 2 + 1];
      uint width = 0;
      uint height = 0;

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
      // desktops are specified in dxp_viewport
      if (width == 0 || height == 0)
        {
          throw std::runtime_error (
              "Couldn't parse your desktops. Perhaps your window manager does "
              "not support large desktops and _NET_DESKTOP_VIEWPORT property "
              "is set to (0,0).\nYou might want to manually specify your "
              "desktop coordinates in the src/config.hpp. See "
              "`dxp_viewport`.");
        }

      info.push_back (desktop_info{ i, x, y, width, height });
    }

  return info;
}

/**
 * Get value of _NET_CURRENT_DESKTOP
 */
uint32_t
get_current_desktop (xcb_connection_t *c, xcb_window_t root)
{
  return get_property_value (c, root, "_NET_CURRENT_DESKTOP")[0];
};

constexpr uint8_t k_event_data32_length = 5;
/**
 * Generating and sending client message to the x server
 */
void
send_xcb_message (xcb_connection_t *c, xcb_window_t destination_window,
                  const char *msg,
                  const std::array<uint32_t, k_event_data32_length> &data)
{
  // Making a double pointer to error doesn't look right
  xcb_generic_error_t *e = nullptr;

  auto atom_cookie = xcb_intern_atom (c, 0, strlen (msg), msg);
  auto atom_reply = xcb_unique_ptr<xcb_intern_atom_reply_t> (
      xcb_intern_atom_reply (c, atom_cookie, &e));
  check (e, "XCB error while getting internal atom reply");

  auto atom_id = atom_reply->atom; /// Id of request

  // Building an event to send
  xcb_client_message_event_t event; /// Name of the event
  event.response_type = XCB_CLIENT_MESSAGE;
  // EWMH asks to set format to 32
  // https://specifications.freedesktop.org/wm-spec/wm-spec-1.3.html
  constexpr uint8_t emwh_specified_format = 32;
  event.format = emwh_specified_format;
  event.sequence = 0;
  event.window = destination_window;
  event.type = atom_id;
  std::copy (data.begin (), data.end (), std::begin (event.data.data32));
  xcb_send_event (c, 0, destination_window,
                  XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT
                      | XCB_EVENT_MASK_STRUCTURE_NOTIFY
                      | XCB_EVENT_MASK_PROPERTY_CHANGE,
                  reinterpret_cast<const char *> (&event));
  // xcb_flush (c);
}

/**
 * Moves all windows from desktops
 * in vector to a preselected one
 */

void
move_relative_desktops (xcb_connection_t *c, xcb_window_t root,
                        std::vector<uint> from_desktops, uint to_desktop)
{
  auto all_windows = get_property_value (c, root, "_NET_CLIENT_LIST");
  for (uint32_t window : all_windows)
    {
      for (uint desktop : from_desktops)
        {
          if (get_property_value (c, window, "_NET_WM_DESKTOP")[0] == desktop)
            {
              // 2 stands for source indicator
              // Sources with 2 as indicator are
              // perceived as direct user actions
              send_xcb_message (c, window, "_NET_WM_DESKTOP",
                                { to_desktop, 2 });
            }
        }
    }
}

/**
 * Change _NET_CURRENT_DESKTOP to destkop_id
 */
void
ewmh_change_desktop (xcb_connection_t *c, xcb_window_t root, uint destkop_id)
{
  // Setting data[1]=timestamp to zero
  // EWMH: Note that the timestamp may be 0 for clients using an older version
  // of this spec, in which case the timestamp field should be ignored.
  send_xcb_message (c, root, "_NET_CURRENT_DESKTOP", { destkop_id });
}
