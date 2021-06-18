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
  int16_t x;                           ///< x coordinate of the top left corner
  int16_t y;                           ///< y coordinate of the top left corner
  uint16_t width;
  uint16_t height;

  drawable (int16_t x,      ///< x coordinate of the top left corner
            int16_t y,      ///< y coordinate of the top left corner
            uint16_t width, ///< Width of the display
            uint16_t height ///< Height of the display
  );
};

#endif /* ifndef DRAWABLE_HPP */
