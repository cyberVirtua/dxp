#include "desktop.hpp"
#include "config.hpp"
#include "xcb_util.hpp"
#include <memory>
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

  resize (this->image_ptr, this->pixmap.data (), this->width, this->height,
          this->pixmap_width, this->pixmap_height);
}

/**
 * Definitely copied. Looks like I'm too retarded to code this myself.
 * https://stackoverflow.com/questions/28566290
 *
 * TODO Optimize and fix warnings, comment on names, add anti aliasing
 */
void
dxp_desktop::resize (const uint8_t *input, uint8_t *output,
                     int source_width, /* Source dimensions */
                     int source_height,
                     int target_width, /* Target dimensions */
                     int target_height)
{
  // TODO(mmskv): not sure what this variable is meant to represent
  constexpr size_t k_half_int = 16;

  const int x_ratio = (source_width << k_half_int) / target_width;
  const int y_ratio = (source_height << k_half_int) / target_height;
  const int colors = 4;

  for (int y = 0; y < target_height; y++)
    {
      int y2_xsource = ((y * y_ratio) >> k_half_int) * source_width;
      int i_xdest = y * target_width;

      for (int x = 0; x < target_width; x++)
        {
          int x2 = ((x * x_ratio) >> k_half_int);
          int y2_x2_colors = (y2_xsource + x2) * colors;
          int i_x_colors = (i_xdest + x) * colors;

          output[i_x_colors] = input[y2_x2_colors];
          output[i_x_colors + 1] = input[y2_x2_colors + 1];
          output[i_x_colors + 2] = input[y2_x2_colors + 2];
          output[i_x_colors + 3] = input[y2_x2_colors + 3];
        }
    }
}
