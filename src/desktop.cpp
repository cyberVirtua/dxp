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
