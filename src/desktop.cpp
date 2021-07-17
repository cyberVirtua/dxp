#include "desktop.hpp"
#include "config.hpp"   // for dxp_height, dxp_width
#include "xcb_util.hpp" // for check, xcb_unique_ptr
#include <cmath>        // for floor
#include <memory>       // for unique_ptr
#include <string>       // for allocator
#include <xcb/xcb.h>    // for xcb_generic_error_t
#include <xcb/xproto.h> // for xcb_get_image, xcb_get_image_data, xcb_get_i...

constexpr bool dxp_horizontal_stacking = (dxp_width == 0);
constexpr bool dxp_vertical_stacking = (dxp_height == 0);

dxp_desktop::dxp_desktop (
    const int16_t x,  ///< x coordinate of the top left corner
    const int16_t y,  ///< y coordinate of the top left corner
    const uint width, ///< width of display
    const uint height ///< height of display
    )
    : drawable (x, y, width, height)
{
  // Initializing non built-in types to zeros
  this->image_ptr = nullptr;

  // Check if both are set or unset simultaneously
  static_assert ((dxp_height == 0) != (dxp_width == 0),
                 "Height and width can't be set or unset simultaneously");

  float screen_ratio = float (this->width) / this->height; ///< width/height

  if (dxp_horizontal_stacking)
    {
      this->pixmap_height = dxp_height;
      this->pixmap_width = this->pixmap_height * screen_ratio;
    }
  else if (dxp_vertical_stacking)
    {
      this->pixmap_width = dxp_width;
      this->pixmap_height = this->pixmap_width / screen_ratio;
    }

  // Create a small pixmap with the size of downscaled screenshot from config
  this->pixmap.resize (this->pixmap_width * this->pixmap_height * 4U);
}

/**
 * Make screenshot, downsize it and set this->image_ptr to point to it
 */
void
dxp_desktop::save_screen ()
{
  // Request the screenshot of the virtual desktop
  auto gi_cookie = xcb_get_image (
      drawable::c,               /* Connection */
      XCB_IMAGE_FORMAT_Z_PIXMAP, /* Z_Pixmap is 100 faster than XY_PIXMAP */
      drawable::screen->root,    /* Screenshot relative to root */
      this->x, this->y,          /* X, Y offset */
      this->width, this->height, /* Dimensions */
      uint32_t (~0)              /* Plane mask (all bits to get all planes) */
  );

  // Not freeing gi_reply causes memory leak, as xcb_get_image always
  // allocates new space for the image
  xcb_generic_error_t *e = nullptr;
  auto gi_reply = xcb_unique_ptr<xcb_get_image_reply_t> (
      xcb_get_image_reply (drawable::c, gi_cookie, &e));
  check (e, "XCB error while getting image reply");

  this->image_ptr = xcb_get_image_data (gi_reply.get ());

  // To remove aliasing in the resulting image,
  // a low pass filter should be used on the source.
  int radius = this->width / this->pixmap_width / 2;

  this->box_blur_horizontal (radius);
  this->box_blur_vertical (radius);

  this->nn_resize ();
}

/**
 * Apply a horizontal box filter (low pass) to the image.
 *
 * Uses algorithm described here
 * http://blog.ivank.net/fastest-gaussian-blur.html
 * and here https://www.gamasutra.com/view/feature/3102
 */
void
dxp_desktop::box_blur_horizontal (uint radius)
{
  auto *input32 = reinterpret_cast<uint32_t *> (image_ptr);
  class pixmap img (input32, width); // Adds operator[][]

  constexpr uint32_t a_mask = 0xFF000000;
  constexpr uint32_t r_mask = 0x00FF0000;
  constexpr uint32_t g_mask = 0x0000FF00;
  constexpr uint32_t b_mask = 0x000000FF;

  int n = radius * 2 + 1; ///< Amount of pixels in a kernel

  for (int y = 0; y < height; y++)
    {
      // Accumulators for color values
      uint32_t r = 0;
      uint32_t g = 0;
      uint32_t b = 0;

      /// Leftmost part of the kernel.
      /// As changes to the image are made in place,
      /// unchanged kernel values must be stored
      std::vector<uint32_t> left_pixels{};
      left_pixels.reserve (radius + 2);

      // Fill accumulators. Image gets mirrored for edge pixels
      for (int x = radius; x > 0; x--)
        {
          r += 2 * (img[x][y] & r_mask);
          g += 2 * (img[x][y] & g_mask);
          b += 2 * (img[x][y] & b_mask);

          left_pixels.insert (left_pixels.cbegin (), img[x][y]);
        }

      // Add the first pixel to the accumulators
      r += img[0][y] & r_mask;
      g += img[0][y] & g_mask;
      b += img[0][y] & b_mask;

      // Main loop.
      // Set pixel to the accumulated value and increment accumulator
      for (int x = 0; x < width - radius - 1; x++)
        {
          left_pixels.push_back (img[x][y]);
          img[x][y] = (r / n) & r_mask | (g / n) & g_mask | (b / n) & b_mask;

          // Subtract leftmost
          r -= left_pixels[0] & r_mask;
          g -= left_pixels[0] & g_mask;
          b -= left_pixels[0] & b_mask;

          // Add rightmost
          r += img[x + radius + 1][y] & r_mask;
          g += img[x + radius + 1][y] & g_mask;
          b += img[x + radius + 1][y] & b_mask;

          left_pixels.erase (left_pixels.cbegin ());
        }

      // Mirror image for edge pixels
      for (int i = 1, x = width - radius - 1; x < width; x++)
        {
          left_pixels.push_back (img[x][y]);
          img[x][y] = (r / n) & r_mask | (g / n) & g_mask | (b / n) & b_mask;

          r -= left_pixels[0] & r_mask;
          g -= left_pixels[0] & g_mask;
          b -= left_pixels[0] & b_mask;

          r += img[width - i][y] & r_mask;
          g += img[width - i][y] & g_mask;
          b += img[width - i][y] & b_mask;
          i++;

          left_pixels.erase (left_pixels.cbegin ());
        }
    }
}

/**
 * Apply a vertical box filter (low pass) to the image.
 */
void
dxp_desktop::box_blur_vertical (uint radius)
{
  auto *input32 = reinterpret_cast<uint32_t *> (image_ptr);
  class pixmap img (input32, width);

  constexpr uint32_t r_mask = 0x00FF0000;
  constexpr uint32_t g_mask = 0x0000FF00;
  constexpr uint32_t b_mask = 0x000000FF;

  int n = radius * 2 + 1;

  for (int x = 0; x < width; x++)
    {
      uint32_t r = 0;
      uint32_t g = 0;
      uint32_t b = 0;

      std::vector<uint32_t> top_pixels{};
      top_pixels.reserve (radius + 2);

      for (int y = radius; y > 0; y--)
        {
          r += 2 * (img[x][y] & r_mask);
          g += 2 * (img[x][y] & g_mask);
          b += 2 * (img[x][y] & b_mask);

          top_pixels.insert (top_pixels.cbegin (), img[x][y]);
        }

      r += img[x][0] & r_mask;
      g += img[x][0] & g_mask;
      b += img[x][0] & b_mask;

      for (int y = 0; y < height - radius - 1; y++)
        {
          top_pixels.push_back (img[x][y]);
          img[x][y] = (r / n) & r_mask | (g / n) & g_mask | (b / n) & b_mask;

          r -= top_pixels[0] & r_mask;
          g -= top_pixels[0] & g_mask;
          b -= top_pixels[0] & b_mask;

          r += img[x][y + radius + 1] & r_mask;
          g += img[x][y + radius + 1] & g_mask;
          b += img[x][y + radius + 1] & b_mask;

          top_pixels.erase (top_pixels.cbegin ());
        }
      for (int i = 1, y = height - radius - 1; y < height; y++)
        {
          top_pixels.push_back (img[x][y]);
          img[x][y] = (r / n) & r_mask | (g / n) & g_mask | (b / n) & b_mask;

          r -= top_pixels[0] & r_mask;
          g -= top_pixels[0] & g_mask;
          b -= top_pixels[0] & b_mask;

          r += img[x][height - i] & r_mask;
          g += img[x][height - i] & g_mask;
          b += img[x][height - i] & b_mask;
          i++;

          top_pixels.erase (top_pixels.cbegin ());
        }
    }
}

/**
 * Nearest neighbor resize. Very fast
 * https://stackoverflow.com/questions/28566290
 */
void
dxp_desktop::nn_resize ()
{
  const auto *input32 = reinterpret_cast<const uint32_t *> (image_ptr);
  auto *output32 = reinterpret_cast<uint32_t *> (pixmap.data ());

  //
  // Bitshifts are used to preserve precision in x_ratio and y_ratio.
  // Straight up dividing source_width by target_width will lose some data.
  // Bitshifting this value left increases precision after the division.
  // And when the real x_ratio needs to be used it can be bitshifted right.
  //
  // k_precision_bytes is the number of bits to reserve for precision.
  // Can be way lower than 16.
  //
  constexpr int k_precision_bytes = 16;

  const int x_ratio = (width << k_precision_bytes) / pixmap_width;
  const int y_ratio = (height << k_precision_bytes) / pixmap_height;

  for (int y = 0; y < pixmap_height; y++)
    {
      int y_source = ((y * y_ratio) >> k_precision_bytes) * width;
      int y_dest = y * pixmap_width;

      int x_source = 0;
      const uint32_t *input32_line = input32 + y_source;
      for (int x = 0; x < pixmap_width; x++)
        {
          x_source += x_ratio;
          output32[y_dest + x] = input32_line[x_source >> k_precision_bytes];
        }
    }
}

/**
 * Bilinear resize.
 * Additional overhead compared to nearest neighbor resize does not result in
 * significantly better images. So this resize algorithm is not used.
 */
void
dxp_desktop::bilinear_resize ()
{
  const float x_ratio = float (width) / pixmap_width;
  const float y_ratio = float (height) / pixmap_height;

  for (int y_dst = 0; y_dst < pixmap_height; y_dst++)
    {
      float y_src = y_dst * y_ratio;
      int y = std::floor (y_src);
      float dy = y_src - y;

      for (int x_dst = 0; x_dst < pixmap_width; x_dst++)
        {
          float x_src = x_dst * x_ratio;
          int x = std::floor (x_src);
          float dx = x_src - x;

          int y_offset = y_dst * pixmap_width;

          int y0_offset = y * width;
          int y1_offset = y0_offset + width;

          for (int ch = 0; ch < 4; ch++)
            {
              pixmap[(x_dst + y_offset) * 4 + ch]
                  = (image_ptr[(x + y0_offset) * 4 + ch] * (1 - dx) * (1 - dy)
                     + image_ptr[(x + y1_offset) * 4 + ch] * (1 - dx) * dy
                     + image_ptr[(x + 1 + y0_offset) * 4 + ch] * dx * (1 - dy)
                     + image_ptr[(x + 1 + y1_offset) * 4 + ch] * dx * dy);
            }
        }
    }
}
