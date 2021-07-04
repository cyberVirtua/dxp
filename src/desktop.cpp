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

  bilinear_resize (this->image_ptr, this->pixmap.data (), this->width,
                   this->height, this->pixmap_width, this->pixmap_height);
}

/**
 * Nearest neighbor resize. Very fast
 * https://stackoverflow.com/questions/28566290
 */
void
dxp_desktop::nn_resize (const uint8_t *__restrict input,
                        uint8_t *__restrict output,
                        int source_width, /* Source dimensions */
                        int source_height,
                        int target_width, /* Target dimensions */
                        int target_height)
{
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

  const auto *input32 = reinterpret_cast<const uint32_t *> (input);
  auto *output32 = reinterpret_cast<uint32_t *> (output);

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

void
dxp_desktop::resize_hq_4ch (const uint8_t *input, uint8_t *output,
                            int source_width, int source_height,
                            int target_width, int target_height)
{
  static int *g_px1a = nullptr;
  static int g_px1a_w = 0;
  static int *g_px1ab = nullptr;
  static int g_px1ab_w = 0;

  // Both buffers must be in RGBA format, and a scanline should be w*4 bytes.

  // NOTE: THIS WILL OVERFLOW for really major downsizing (2800x2800 to 1x1 or
  // more) (2800 ~ sqrt(2^23)) - for a lazy fix, just call this in two passes.

  const auto *input32 = reinterpret_cast<const uint32_t *> (input);
  auto *output32 = reinterpret_cast<uint32_t *> (output);

  // If too many input pixels map to one output pixel, our 32-bit
  // accumulation values could overflow - so, if we have huge mappings like
  // that, cut down the weights:
  //    256 max color value
  //   *256 weight_x
  //   *256 weight_y
  //   *256 (16*16) maximum # of input pixels (x,y) - unless we cut the
  //   weights down...
  int weight_shift = 0;
  float source_texels_per_out_pixel
      = ((source_width / float (target_width) + 1)
         * (source_height / float (target_height) + 1));
  float weight_per_pixel
      = source_texels_per_out_pixel * 256 * 256;  // weight_x * weight_y
  float accum_per_pixel = weight_per_pixel * 256; // color value is 0-255
  float weight_div = accum_per_pixel / 4294967000.0f;
  if (weight_div > 1)
    {
      weight_shift = int (ceilf (logf (weight_div) / logf (2.0f)));
    }
  weight_shift = 1; // this could go to 15 and still be ok.

  float y_ratio = 256 * source_height / float (target_height);
  float x_ratio = 256 * source_width / float (target_width);

  // cache x1a, x1b for all the columns:
  // ...and your OS better have garbage collection on process exit :)
  if (g_px1ab_w < target_width)
    {
      if (g_px1ab)
        delete[] g_px1ab;
      g_px1ab = new int[target_width * 2 * 2];
      g_px1ab_w = target_width * 2;
    }
  for (int x_dst = 0; x_dst < target_width; x_dst++)
    {
      // find the x-range of input pixels that will contribute:
      int x1a = int (x_dst * x_ratio);
      int x1b = int ((x_dst + 1) * x_ratio);
      x1b = std::min (x1b, 256 * source_width - 1);
      g_px1ab[x_dst * 2 + 0] = x1a;
      g_px1ab[x_dst * 2 + 1] = x1b;
    }

  // For every output pixel
  for (int y_dst = 0; y_dst < target_height; y_dst++)
    {
      // Find the y-range of input pixels that will contribute:
      int y1a = int ((y_dst)*y_ratio);
      int y1b = int ((y_dst + 1) * y_ratio);
      // Map to same pixel -> we want to interpolate Between two pixels!
      y1b = std::min (y1b, 256 * source_height - 1);
      int y1c = y1a >> 8;
      int y1d = y1b >> 8;

      for (int x_dst = 0; x_dst < target_width; x_dst++)
        {
          // find the x-range of input pixels that will contribute:
          int x1a = g_px1ab[x_dst * 2 + 0]; // (computed earlier)
          int x1b = g_px1ab[x_dst * 2 + 1]; // (computed earlier)
          int x1c = x1a >> 8;
          int x1d = x1b >> 8;

          // ADD UP ALL INPUT PIXELS CONTRIBUTING TO THIS OUTPUT PIXEL:
          uint r = 0, g = 0, b = 0, a = 0, d = 0;
          for (int y = y1c; y <= y1d; y++)
            {
              uint weight_y = 256;
              if (y1c != y1d)
                {
                  if (y == y1c)
                    {
                      weight_y = 256 - (y1a & 0xFF);
                    }

                  else if (y == y1d)
                    {
                      weight_y = (y1b & 0xFF);
                    }
                }

              const auto *dsrc2 = &input32[y * source_width + x1c];
              for (int x = x1c; x <= x1d; x++)
                {
                  uint weight_x = 256;
                  if (x1c != x1d)
                    {
                      if (x == x1c)
                        {
                          weight_x = 256 - (x1a & 0xFF);
                        }
                      else if (x == x1d)
                        {
                          weight_x = (x1b & 0xFF);
                        }
                    }

                  uint c = *dsrc2++; // dsrc[y*w1 + x];
                  uint r_src = (c)&0xFF;
                  uint g_src = (c >> 8) & 0xFF;
                  uint b_src = (c >> 16) & 0xFF;
                  uint d_src = (c >> 24) & 0xFF;
                  uint w = (weight_x * weight_y) >> weight_shift;
                  r += r_src * w;
                  g += g_src * w;
                  b += b_src * w;
                  d += d_src * w;
                  a += w;
                }
            }

          // write results
          uint c
              = ((r / a)) | ((g / a) << 8) | ((b / a) << 16) | ((d / a) << 24);
          *output32++ = c; // ddest[y2*w2 + x2] = c;
        }
    }
}

/**
 * TODO
 */
void
dxp_desktop::bilinear_resize (const uint8_t *__restrict input,
                              uint8_t *__restrict output,
                              int source_width, /* Source dimensions */
                              int source_height,
                              int target_width, /* Target dimensions */
                              int target_height)
{
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

          for (int n = 0; n < 4; n++)
            {
              output[(x_dst + y_offset) * 4 + n]
                  = (input[(x + y0_offset) * 4 + n] * (1 - dx) * (1 - dy)
                     + input[(x + y1_offset) * 4 + n] * (1 - dx) * dy
                     + input[(x + 1 + y0_offset) * 4 + n] * dx * (1 - dy)
                     + input[(x + 1 + y1_offset) * 4 + n] * dx * dy);
            }
        }
    }
}
