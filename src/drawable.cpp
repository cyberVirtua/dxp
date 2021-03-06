#include "drawable.hpp"

drawable::drawable (const int16_t x,  ///< x coordinate of the top left corner
                    const int16_t y,  ///< y coordinate of the top left corner
                    const uint width, ///< width of display
                    const uint height ///< height of display
)
{
  this->x = x;
  this->y = y;
  this->width = width;
  this->height = height;

  // Initialize static attributes only when they don't already exist
  if (!drawable::c || !drawable::screen)
    {
      // Connect to X. NULL will use $DISPLAY
      drawable::c = xcb_connect (nullptr, nullptr);
      drawable::screen
          = xcb_setup_roots_iterator (xcb_get_setup (drawable::c)).data;
      drawable::root = screen->root;
    }
}

drawable::drawable () : drawable (0, 0, 0, 0) {}
