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
  short x;                             ///< x coordinate of the top left corner
  short y;                             ///< y coordinate of the top left corner
  uint16_t width;
  uint16_t height;

  drawable (short x,        ///< x coordinate of the top left corner
            short y,        ///< y coordinate of the top left corner
            uint16_t width, ///< Width of the display
            uint16_t height ///< Height of the display
  );
  ~drawable (); // TODO Should you disconnect from Xserver?
};

#endif /* ifndef DRAWABLE_HPP */
