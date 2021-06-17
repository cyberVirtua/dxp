#ifndef WINDOW_HPP
#define WINDOW_HPP

#include "dexpo_socket.hpp"
#include <string>
#include <vector>
#include <xcb/xproto.h>

/* Visualizes workspaces previews for user */
class window : public drawable
{
public:
  uint32_t win_id;  // Identificator for Window
  int16_t x;        // x coordinate of the top left corner
  int16_t y;        // y coordinate of the top left corner
  short b_width;    // Border width NOTE: Maybe it could be changed to constant
  std::string name; // _NET_WM_NAME of display, MAYBE UNNEEDED THERE
  uint16_t width;
  uint16_t height;
  uint32_t parent;
  // TODO: Add masks

  // The same as for DesktopPixmap
  window (int16_t x,               ///< x coordinate of the top left corner
          int16_t y,               ///< y coordinate of the top left corner
          uint16_t width,          ///< Width of the display
          uint16_t height,         ///< Height of the display
          const std::string &name, ///< Name of the display (_NET_WM_NAME)
          uint32_t parent);
  ~window ();

  // Explicitly deleting unused constructors to comply with rule of five
  window (const window &other) = delete;
  window (window &&other) noexcept = delete;
  window &operator= (const window &other) = delete;
  window &operator= (window &&other) = delete;

  /* Gets coordinates of top left corner of screenshot with given number */
  int get_screen_position (int DesktopNumber, std::vector<dexpo_pixmap> v);

  /* Creates a visible window */
  void create_window ();

  /* Creates new window inside the existing one */
  void connect_to_parent_window (xcb_connection_t *c, xcb_screen_t *screen);
};

#endif /* ifndef WINDOW_HPP */
