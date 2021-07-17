#ifndef DEXPO_XCB_HPP
#define DEXPO_XCB_HPP

#include "config.hpp"        // for keys_size, dxp_keys, dxp_keys::next
#include "keys.hpp"          // for keymap
#include <algorithm>         // for find, find_if
#include <array>             // for array
#include <cstdint>           // for uint32_t
#include <cstdlib>           // for free, size_t
#include <iterator>          // for pair
#include <memory>            // for allocator, unique_ptr
#include <stdexcept>         // for runtime_error, range_error
#include <string>            // for char_traits, operator+, to_string, string
#include <string_view>       // for basic_string_view, operator==, string_view
#include <sys/types.h>       // for uint
#include <utility>           // for pair
#include <vector>            // for vector
#include <xcb/xcb.h>         // for xcb_connection_t, xcb_generic_error_t
#include <xcb/xcb_keysyms.h> // for xcb_key_symbols_free, xcb_key_symbols_a...
#include <xcb/xproto.h>      // for xcb_keycode_t, xcb_window_t, xcb_keysym_t

/**
 * Virtual desktop information
 */
struct desktop_info
{
  // uint id;
  int x;
  int y;
  uint width;
  uint height;
};

/**
 * Monitor information
 *
 * Used to calculate sizes of virtual desktops
 */
struct monitor_info
{
  int x;
  int y;
  uint width;
  uint height;
  std::string name;
};

/**
 * Information about window on a desktop.
 *
 * Used to draw desktop layouts in place of desktop screenshots
 */
struct window_info
{
  uint desktop; ///< Id of the desktop the window belongs to
  uint x;
  uint y;
  uint width;
  uint height;
  // xcb_pixmap_t *icon ; TODO(sthussky): implement this
};

class xcb_error : public std::runtime_error
{
public:
  /**
   * Create a runtime_error and append an error struct to message
   */
  explicit xcb_error (xcb_generic_error_t *e, const std::string &msg)
      : std::runtime_error (
          msg + " error code: " + std::to_string (e->error_code)
          + ", sequence:" + std::to_string (e->sequence)
          + ", resource id:" + std::to_string (e->resource_id)
          + ", major code: " + std::to_string (e->major_code)
          + ", minor code: " + std::to_string (e->minor_code)){};
};

/**
 * Check if got an XCB error and throw it in case
 */
void check (xcb_generic_error_t *e, const std::string &msg);

/**
 * Create unique_ptr with free as default deleter.
 *
 * Some xcb return types need to be deallocated by calling free.
 * This templated makes that easy to implement.
 */
template <typename T>
std::unique_ptr<T, decltype (&std::free)>
xcb_unique_ptr (T *ptr)
{
  return std::unique_ptr<T, decltype (&std::free)> (ptr, &std::free);
}

/**
 * Get a vector with EWMH property values
 *
 * @note vector size is inconsistent so vector may contain other data
 */
std::vector<uint32_t> get_property_value (xcb_connection_t *c,
                                          xcb_window_t root, std::string &name);

/**
 * Get monitor_info for each connected monitor
 */
std::vector<monitor_info> get_monitors (xcb_connection_t *c, xcb_window_t root);

/**
 * Get monitor_info of the primary monitor
 */
monitor_info get_monitor_primary (xcb_connection_t *c, xcb_window_t root);

/**
 * Get an array of desktop_info.
 *
 * FIXME Uses EWMH and won't work if WM doesn't support large desktops
 */
std::vector<desktop_info> get_desktops (xcb_connection_t *c, xcb_window_t root);

/**
 * Get value of _NET_CURRENT_DESKTOP
 */
uint get_current_desktop (xcb_connection_t *c, xcb_window_t root);

/**
 * Change _NET_CURRENT_DESKTOP to destkop_id
 */
void ewmh_change_desktop (xcb_connection_t *c, xcb_window_t root,
                          uint destkop_id);

/**
 * Returns an array of window informations
 */
std::vector<window_info> get_windows (xcb_connection_t *c, xcb_window_t root);

/*******************************************************************************
 * XKB Related code
 ******************************************************************************/

using keysyms = std::array<xcb_keysym_t, keys_size>;
using codes = std::vector<xcb_keycode_t>;

/**
 * Keycodes parsed from keys specified in config
 */
struct dxp_keycodes
{
  codes next;
  codes prev;
  codes slct;
  codes exit;

  /**
   * Check if there is a keycode present in a vector of keycodes.
   * Used to check if pressed key matches any keys of interest.
   */
  static bool
  has (codes &codes, xcb_keycode_t code)
  {
    return std::find (codes.cbegin (), codes.cend (), code) != codes.cend ();
  }
};

/**
 * Constexpr map template from Jason Turner
 */
template <typename Key, typename Value, std::size_t size> struct map
{
  std::array<std::pair<Key, Value>, size> data{};

  [[nodiscard]] constexpr Value
  at (const Key &key) const
  {
    const auto itr
        = std::find_if (begin (data), end (data),
                        [&key] (const auto &v) { return v.first == key; });
    if (itr != end (data))
      {
        return itr->second;
      }
    throw std::range_error ("Not Found");
  }
};

/// Constexpr map of keyname : keysym
static constexpr auto dxp_keymap
    = map<std::string_view, xcb_keysym_t, keymap.size ()>{ { keymap } };

/**
 * Get array of keynames from array of keysyms
 */
template <std::size_t size>
constexpr std::array<xcb_keysym_t, size>
get_keysyms (const std::array<std::string_view, size> &keynames)
{
  std::array<xcb_keysym_t, size> keysyms{};
  for (std::size_t i = 0; i < size; i++)
    {
      if (keynames.at (i) != "")
        {
          keysyms.at (i) = dxp_keymap.at (keynames.at (i));
        }
    }
  return keysyms;
};

/**
 * Get an array of all keycodes that can be assostiated with the given keysyms
 */
template <std::size_t size>
std::vector<xcb_keycode_t>
keysyms2keycodes (xcb_connection_t *c,
                  const std::array<xcb_keysym_t, size> &all_keysyms)
{
  std::vector<xcb_keycode_t> all_keycodes{};
  all_keycodes.reserve (size * 2);

  for (const auto &keysym : all_keysyms)
    {
      if (keysym == 0) // Ignore empty array elements
        {
          continue;
        }

      // Use custom deleter
      auto keysyms = std::unique_ptr<xcb_key_symbols_t,
                                     decltype (xcb_key_symbols_free) *> (
          xcb_key_symbols_alloc (c), xcb_key_symbols_free);

      if (!keysyms)
        {
          throw std::runtime_error ("Could not get keycodes for the keysym \""
                                    + std::to_string (keysym) + "\"");
        }
      auto smart_keycodes = xcb_unique_ptr<xcb_keycode_t> (
          xcb_key_symbols_get_keycode (keysyms.get (), keysym));

      auto keycodes = smart_keycodes.get ();
      while (*keycodes != XCB_NO_SYMBOL)
        {
          all_keycodes.push_back (*keycodes);
          keycodes++;
        }
    }
  return all_keycodes;
}

/**
 * Keysyms parsed from keynames.
 * This is an intermediate step in conversion of keynames -> keycodes.
 */
struct dxp_keysyms
{
  static constexpr keysyms next = get_keysyms<keys_size> (dxp_keys::next);
  static constexpr keysyms prev = get_keysyms<keys_size> (dxp_keys::prev);
  static constexpr keysyms slct = get_keysyms<keys_size> (dxp_keys::slct);
  static constexpr keysyms exit = get_keysyms<keys_size> (dxp_keys::exit);
};

/**
 * Get all keycodes for keys in the config
 */
template <std::size_t size>
dxp_keycodes
get_keycodes (xcb_connection_t *c)
{
  dxp_keycodes keycodes;
  keycodes.next = keysyms2keycodes<size> (c, dxp_keysyms::next);
  keycodes.prev = keysyms2keycodes<size> (c, dxp_keysyms::prev);
  keycodes.slct = keysyms2keycodes<size> (c, dxp_keysyms::slct);
  keycodes.exit = keysyms2keycodes<size> (c, dxp_keysyms::exit);

  return keycodes;
}

#endif /* ifndef DEXPO_XCB_HPP */
