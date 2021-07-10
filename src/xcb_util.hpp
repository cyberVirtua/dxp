#ifndef DEXPO_XCB_HPP
#define DEXPO_XCB_HPP

#include "keys.hpp"
#include <cstdlib>
#include <exception>
#include <memory>
#include <vector>
#include <xcb/randr.h>
#include <xcb/xcb.h>

/**
 * Virtual desktop information
 */
struct desktop_info
{
  uint id; // XXX Is it necessary?
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
                                          xcb_window_t root,
                                          const char *atom_name);

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

/*******************************************************************************
 * XKB Related code
 ******************************************************************************/

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
    = map<std::string_view, int, keymap.size ()>{ { keymap } };

/**
 * Get array of keynames from array of keysyms
 */
template <std::size_t size>
constexpr std::array<int, size>
get_keysyms (const std::array<std::string_view, size> &keynames)
{
  std::array<int, size> keysyms{};
  for (std::size_t i = 0; i < size; i++)
    {
      if (keynames[i] != "")
        {
          keysyms[i] = dxp_keymap.at (keynames[i]);
        }
    }
  return keysyms;
};

/**
 * Get an array of all keycodes that can be assostiated with a keysym
 */
std::vector<xcb_keycode_t> get_keycodes (xcb_connection_t *c, xcb_keysym_t sym);

#endif /* ifndef DEXPO_XCB_HPP */
