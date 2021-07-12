#ifndef DRAWABLE_HPP
#define DRAWABLE_HPP

#include <cstdint>      // for int16_t
#include <sys/types.h>  // for uint
#include <xcb/xcb.h>    // for xcb_connection_t
#include <xcb/xproto.h> // for xcb_screen_t, xcb_window_t

/**
 * XCB base drawable class. Implements static connection and dimensions
 */
class drawable
{
public:
  inline static xcb_connection_t *c;  ///< Xserver connection
  inline static xcb_screen_t *screen; ///< X screen
  inline static xcb_window_t root;    ///< Root window of the screen
  int16_t x;                          ///< x coordinate of the top left corner
  int16_t y;                          ///< y coordinate of the top left corner
  uint width;
  uint height;

  /// Initialize static variables, set x, y, width, height to zero
  drawable ();
  drawable (int16_t x,  ///< x coordinate of the top left corner
            int16_t y,  ///< y coordinate of the top left corner
            uint width, ///< Width of the display
            uint height ///< Height of the display
  );
};

#endif /* ifndef DRAWABLE_HPP */
