#include "xcb_util.hpp"
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
 * TODO(kud-aa) Document function
 */
void
send_message (xcb_connection_t *c, xcb_screen_t *screen, const char *msg,
              unsigned long data0, unsigned long data1 = 0,
              unsigned long data2 = 0, unsigned long data3 = 0,
              unsigned long data4 = 0)
{
  xcb_client_message_event_t event;
  xcb_intern_atom_cookie_t atom_cookie;
  xcb_intern_atom_reply_t *atom_reply;
  xcb_atom_t atom_id;

  xcb_map_window (c, screen->root);
  atom_cookie = xcb_intern_atom (c, 0, strlen (msg), msg);
  atom_reply = xcb_intern_atom_reply (c, atom_cookie, nullptr);
  atom_id = atom_reply->atom;

  event.response_type = XCB_CLIENT_MESSAGE;
  event.format = 32;
  event.sequence = 0;
  event.window = screen->root;
  event.type = atom_id;
  event.data.data32[0] = data0; // source: 1=application 2=pager
  event.data.data32[1] = data1; // timestamp
  event.data.data32[2] = data2; // currently active window (none)
  event.data.data32[3] = data3;
  event.data.data32[4] = data4;
  xcb_send_event (c, 0, screen->root,
                  XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY
                      | XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT,
                  reinterpret_cast<const char *> (&event));
  xcb_flush (c);
  free (atom_reply);
}

void
ewmh_change_desktop (xcb_connection_t *c, xcb_screen_t *screen,
                     size_t destkop_id)
{
  send_message (c, screen, "_NET_CURRENT_DESKTOP", destkop_id);
}
