#ifndef DEXPO_XCB_HPP
#define DEXPO_XCB_HPP

#include <memory>
#include <vector>
#include <xcb/randr.h>
#include <xcb/xcb.h>

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
 * Get an array of desktop_info.
 *
 * FIXME Uses EWMH and won't work if WM doesn't support large desktops
 */
std::vector<desktop_info> get_desktops (xcb_connection_t *c, xcb_window_t root);

/**
 * Get value of _NET_CURRENT_DESKTOP
 */
int get_current_desktop (xcb_connection_t *c, xcb_window_t root);

void ewmh_change_desktop (xcb_connection_t *c, xcb_screen_t *screen,
                          uint destkop_id);

#endif /* ifndef DEXPO_XCB_HPP */
