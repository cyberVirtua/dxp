#include "desktop.hpp"
#include "config.hpp"
#include "xcb_util.hpp"
#include <cmath>
#include <xcb/xcb.h>
#include <xcb/xproto.h>

constexpr bool k_horizontal_stacking = (dexpo_width == 0);
constexpr bool k_vertical_stacking = (dexpo_height == 0);

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
  static_assert ((dexpo_height == 0) != (dexpo_width == 0),
                 "Height and width can't be set or unset simultaneously");

  float screen_ratio = float (this->width) / this->height; ///< width/height

  if (k_horizontal_stacking)
    {
      this->pixmap_height = dexpo_height;
      this->pixmap_width = this->pixmap_height * screen_ratio;
    }
  else if (k_vertical_stacking)
    {
      this->pixmap_width = dexpo_width;
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
      drawable::c_,              /* Connection */
      XCB_IMAGE_FORMAT_Z_PIXMAP, /* Z_Pixmap is 100 faster than XY_PIXMAP */
      drawable::screen_->root,   /* Screenshot relative to root */
      this->x, this->y,          /* X, Y offset */
      this->width, this->height, /* Dimensions */
      uint32_t (~0)              /* Plane mask (all bits to get all planes) */
  );

  // Not freeing gi_reply causes memory leak, as xcb_get_image always
  // allocates new space for the image
  xcb_generic_error_t *e = nullptr;
  auto gi_reply = xcb_unique_ptr<xcb_get_image_reply_t> (
      xcb_get_image_reply (drawable::c_, gi_cookie, &e));
  check (e, "XCB error while getting image reply");

  this->image_ptr = xcb_get_image_data (gi_reply.get ());

  nn_resize (this->image_ptr, this->pixmap.data (), this->width, this->height,
             this->pixmap_width, this->pixmap_height);
}

void
box_blur (uint32_t *img, int width, int height, uint radius)
{
  constexpr uint32_t r_mask = 0x00FF0000;
  constexpr uint32_t g_mask = 0x0000FF00;
  constexpr uint32_t b_mask = 0x000000FF;

  // Do hblur
  int num = radius * 2 + 1;

  for (int y = 0; y < height; y++)
    {
      uint32_t r = 0;
      uint32_t g = 0;
      uint32_t b = 0;

      uint32_t *row = &img[y * width];

      std::vector<uint32_t> left_pixels{};
      left_pixels.reserve (radius + 2);

      for (int x = radius; x > 0; x--)
        {
          r += 2 * (row[x] & r_mask);
          g += 2 * (row[x] & g_mask);
          b += 2 * (row[x] & b_mask);

          left_pixels.insert (left_pixels.cbegin (), row[x]);
        }

      r += row[0] & r_mask;
      g += row[0] & g_mask;
      b += row[0] & b_mask;

      for (int x = 0; x < width - radius - 1; x++)
        {
          left_pixels.push_back (row[x]);
          row[x] = (r / num) & r_mask | (g / num) & g_mask | (b / num) & b_mask;

          // Subtract leftmost
          r -= left_pixels[0] & r_mask;
          g -= left_pixels[0] & g_mask;
          b -= left_pixels[0] & b_mask;

          // Add rightmost
          r += row[x + radius + 1] & r_mask;
          g += row[x + radius + 1] & g_mask;
          b += row[x + radius + 1] & b_mask;

          left_pixels.erase (left_pixels.cbegin ());
        }
      for (int i = 1, x = width - radius - 1; x < width; x++)
        {
          left_pixels.push_back (row[x]);
          row[x] = (r / num) & r_mask | (g / num) & g_mask | (b / num) & b_mask;

          r -= left_pixels[0] & r_mask;
          g -= left_pixels[0] & g_mask;
          b -= left_pixels[0] & b_mask;

          r += row[width - i] & r_mask;
          g += row[width - i] & g_mask;
          b += row[width - i] & b_mask;
          i++;

          left_pixels.erase (left_pixels.cbegin ());
        }
    }

  // Do vblur
}

/**
 * Nearest neighbor resize. Very fast
 * https://stackoverflow.com/questions/28566290
 */
void
dxp_desktop::nn_resize (uint8_t *__restrict input, uint8_t *__restrict output,
                        int source_width, /* Source dimensions */
                        int source_height,
                        int target_width, /* Target dimensions */
                        int target_height)
{
  auto *input32 = reinterpret_cast<uint32_t *> (input);
  auto *output32 = reinterpret_cast<uint32_t *> (output);

  box_blur (input32, source_width, source_height, 100);

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

  const int x_ratio = (source_width << k_precision_bytes) / target_width;
  const int y_ratio = (source_height << k_precision_bytes) / target_height;

  for (int y = 0; y < target_height; y++)
    {
      int y_source = ((y * y_ratio) >> k_precision_bytes) * source_width;
      int y_dest = y * target_width;

      int x_source = 0;
      const uint32_t *input32_line = input32 + y_source;
      for (int x = 0; x < target_width; x++)
        {
          x_source += x_ratio;
          output32[y_dest + x] = input32_line[x_source >> k_precision_bytes];
        }
    }
}

/**
 * TODO
 */
void
dxp_desktop::bilinear_resize (uint8_t *__restrict input,
                              uint8_t *__restrict output,
                              int source_width, /* Source dimensions */
                              int source_height,
                              int target_width, /* Target dimensions */
                              int target_height)
{
  // To remove aliasing in the resulting image,
  // a low pass filter should be used on the source.
  auto *input32 = reinterpret_cast<uint32_t *> (input);
  auto *output32 = reinterpret_cast<uint32_t *> (output);

  box_blur (input32, source_width, source_height, 10);

  const float x_ratio = float (source_width) / target_width;
  const float y_ratio = float (source_height) / target_height;

  for (int y_dst = 0; y_dst < target_height; y_dst++)
    {
      float y_src = y_dst * y_ratio;
      int y = std::floor (y_src);
      float dy = y_src - y;

      for (int x_dst = 0; x_dst < target_width; x_dst++)
        {
          float x_src = x_dst * x_ratio;
          int x = std::floor (x_src);
          float dx = x_src - x;

          int y_offset = y_dst * target_width;

          int y0_offset = y * source_width;
          int y1_offset = y0_offset + source_width;

          for (int ch = 0; ch < 4; ch++)
            {
              output[(x_dst + y_offset) * 4 + ch]
                  = (input[(x + y0_offset) * 4 + ch] * (1 - dx) * (1 - dy)
                     + input[(x + y1_offset) * 4 + ch] * (1 - dx) * dy
                     + input[(x + 1 + y0_offset) * 4 + ch] * dx * (1 - dy)
                     + input[(x + 1 + y1_offset) * 4 + ch] * dx * dy);
            }
        }
    }
}
