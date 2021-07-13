#ifndef WINDOW_HPP
#define WINDOW_HPP

#include "drawable.hpp" // for drawable
#include "socket.hpp"   // for dxp_socket_desktop
#include <cstdint>      // for int16_t, uint32_t
#include <sys/types.h>  // for uint
#include <vector>       // for vector
#include <xcb/xcb.h>    // for xcb_generic_event_t
#include <xcb/xproto.h> // for xcb_gcontext_t

/**
 * Stores x and y coordinates of desktop.
 * Optimizes get_hover_desktop function by avoiding
 * frequent calls to get_desktop_coord.
 */
struct dxp_window_desktop : public dxp_socket_desktop
{
  int16_t x = 0;
  int16_t y = 0;
};

/**
 * Draws desktop previews
 */
class window : public drawable
{
public:
  inline static xcb_gcontext_t gc = 0; ///< Graphic context
  uint32_t xcb_id;
  std::vector<dxp_socket_desktop> desktops; ///< Desktops received from daemon
  dxp_window_desktop recent_hover_desktop;  ///< Recently cached hover desktop
  uint pres;                                ///< id of the preselected desktop

  explicit window (const std::vector<dxp_socket_desktop> &desktops);
  ~window ();

  // Explicitly deleting unused constructors to comply with rule of five
  window (const window &other) = delete;
  window (window &&other) noexcept = delete;
  window &operator= (const window &other) = delete;
  window &operator= (window &&other) = delete;

  /**
   * Calculate dimensions of the window based on
   * stacking mode and desktops from daemon
   */
  void set_window_dimensions ();

  /**
   * Create an empty window to later place gui in it.
   */
  void create_window ();

  /**
   * Draw desktops on the window
   */
  void draw_desktops ();

  /**
   * Get position of the desktop's top left corner by desktop's id.
   */
  int16_t get_desktop_coord (uint desktop_id);

  /**
   * Get id of the desktop above which the mouse is hovering
   * If mouse is not above any desktop return -1
   */
  uint get_hover_desktop (int16_t x, int16_t y);

  /**
   * Draw a border of specified color around specified desktop.
   */
  void draw_desktop_border (uint desktop_id, uint32_t color);

  /**
   * Draw a preselection border of color=dxp_hlcolor
   * around desktop=desktop[this->desktop_sel].
   */
  void draw_preselection ();

  /**
   * Remove a preselection border around desktop[this->desktop_sel].
   *
   * @note This implementation just draws border of color dxp_bgcolor above
   * existing border.
   */
  void clear_preselection ();

  /**
   * TODO Document
   *
   * Returns zero on exit event
   */
  int handle_event (xcb_generic_event_t *event);

private:
  /**
   * Initialize class-level graphic context.
   */
  static void create_gc ();
};

#endif /* ifndef WINDOW_HPP */
