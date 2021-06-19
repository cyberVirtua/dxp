#ifndef WINDOW_HPP
#define WINDOW_HPP

#include "dexpo_socket.hpp"
#include <string>
#include <vector>
#include <xcb/xproto.h>

/**
 * Visualizes workspaces previews for user interface is sized based on config
 * settings
 */
class window : public drawable
{
public:
  inline static xcb_gcontext_t gc_; ///< Graphic context
  uint32_t id;                      // Identificator for Window
  uint16_t b_width; // Border width NOTE: Maybe it could be changed to constant
  std::vector<dexpo_pixmap *> pixmaps;
  int highlighted;
  // TODO: Add masks for events

  // The same as for DesktopPixmap
  window (int16_t x,      ///< x coordinate of the top left corner
          int16_t y,      ///< y coordinate of the top left corner
          uint16_t width, ///< Width of the display
          uint16_t height ///< Height of the display
  );
  ~window ();

  // Explicitly deleting unused constructors to comply with rule of five
  window (const window &other) = delete;
  window (window &&other) noexcept = delete;
  window &operator= (const window &other) = delete;
  window &operator= (window &&other) = delete;

  /**
   * Gets position of top left corner of screenshot with given number
   */
  int get_screen_position (int desktop_number);

  /**
   * Creates empty window to later place gui in it
   */
  void create_window ();

  /**
   * Draws a rectangle around chosen window
   */
  void highlight_window (int desktop_number, uint32_t color);

  /**
   * Draws GUI inside of the pre-created window
   */
  void draw_gui ();

private:
  /**
   * Initializes class-level graphic context
   */
  static void create_gc ();
};

#endif /* ifndef WINDOW_HPP */
