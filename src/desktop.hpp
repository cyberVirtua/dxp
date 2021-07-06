#ifndef DESKTOP_HPP
#define DESKTOP_HPP

#include "drawable.hpp"
#include <string>
#include <vector>
#include <xcb/xproto.h>

class pixmap
{
public:
  uint32_t *data;
  int width;

  pixmap (uint32_t *img, int width)
  {
    this->data = img;
    this->width = width;
  }

  class proxy
  {
  public:
    explicit proxy (uint32_t *data, int width)
    {
      this->data = data;
      this->width = width;
    }

    uint32_t &
    operator[] (int y)
    {
      return data[y * width];
    }

  private:
    uint32_t *data;
    int width;
  };

  proxy
  operator[] (int x) const
  {
    return proxy (&data[x], width);
  }
};

/**
 * Captures, downsizes and stores desktop screenshot
 */
class dxp_desktop : public drawable
{
public:
  uint8_t *image_ptr;          ///< Pointer to the image structure.
  std::vector<uint8_t> pixmap; ///< Constant id for the screenshot's pixmap
  uint pixmap_width;           ///< Width of the pixmap that stores screenshot
  uint pixmap_height;          ///< Height of the pixmap that stores screenshot

  dxp_desktop (int16_t x,    ///< x coordinate of the top left corner
               int16_t y,    ///< y coordinate of the top left corner
               uint width,   ///< Width of the display
               uint height); ///< Height of the display

  /**
   * Save screenshot, downsize it and set this->image_ptr to point to it
   */
  void save_screen ();

  /**
   * Resize image to specified dimensions with nearest neighbour algorithm
   */
  static void
  nn_resize (const uint8_t *input, ///< Input RGBA 1D array pointer
             uint8_t *output,      ///< Output RGBA 1D array pointers
             int source_width,     /* Dimensions of unresized screenshot */
             int source_height,
             int target_width, /* Dimensions of screenshot after resize */
             int target_height);

  /**
   * Resize image to specified dimensions with bilinear interpolation algorithm
   */
  static void
  bilinear_resize (const uint8_t *input, ///< Input RGBA 1D array pointer
                   uint8_t *output,      ///< Output RGBA 1D array pointers
                   int source_width, /* Dimensions of unresized screenshot */
                   int source_height,
                   int target_width, /* Dimensions of screenshot after resize */
                   int target_height);
};

/**
 * Apply a horizontal box filter (low pass) to the image.
 */
void box_blur_horizontal (class pixmap img, int width, int height, uint radius);
/**
 * Apply a vertical box filter (low pass) to the image.
 */
void box_blur_vertical (class pixmap img, int width, int height, uint radius);

#endif /* ifndef DESKTOP_PIXMAP_HPP */
