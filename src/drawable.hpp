#ifndef DRAWABLE_HPP
#define DRAWABLE_HPP

#include <xcb/xproto.h>

/**
 * XCB base drawable class. Implements static connection and dimensions
 */
class drawable
{
public:
  inline static xcb_connection_t *c_;  ///< Xserver connection
  inline static xcb_screen_t *screen_; ///< X screen
  inline static xcb_window_t root_;    ///< Root window of the screen
  int16_t x;                           ///< x coordinate of the top left corner
  int16_t y;                           ///< y coordinate of the top left corner
  uint width;
  uint height;

  drawable (int16_t x,  ///< x coordinate of the top left corner
            int16_t y,  ///< y coordinate of the top left corner
            uint width, ///< Width of the display
            uint height ///< Height of the display
  );
};

#endif /* ifndef DRAWABLE_HPP */
