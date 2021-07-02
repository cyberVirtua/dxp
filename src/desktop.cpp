#include "desktop.hpp"
#include "config.hpp"
#include "xcb_util.hpp"
#include <memory>
#include <xcb/xcb.h>
#include <xcb/xproto.h>

// Workaround for xcb_get_image_reply allocation. see save_screen()
// Represents maximum pixmap size in bytes that xcb_get_image can allocate
constexpr int k_max_malloc = 16711568;

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

  this->pixmap_width = dexpo_width;
  this->pixmap_height = dexpo_height;

  float screen_ratio = float (this->width) / this->height; ///< width/height

  if (this->pixmap_width == 0)
    {
      this->pixmap_width = this->pixmap_height * screen_ratio;
    }
  else if (this->pixmap_height == 0)
    {
      this->pixmap_height = this->pixmap_width / screen_ratio;
    }

  // Create a small pixmap with the size of downscaled screenshot from config
  this->pixmap.resize (this->pixmap_width * this->pixmap_height * 4U);
}

/**
 * Saves pointer to the screen image in the specified pixmap_format.
 *
 * NOTE: malloc() can reserve only 16711568 bytes for a pixmap.
 * But ZPixmap of QHD Ultrawide is 3440 * 1440 * 4 = 19814400 bytes long.
 *
 * The while() loop in save_screen fixes this. It takes multiple screenshots of
 * the screen (top to bottom), each taking up less than k_max_malloc. It
 * downsizes each screenshot and puts it on the pixmap.
 */
void
dxp_desktop::save_screen ()
{
  // Set height so that screenshots won't exceed k_max_malloc size
  uint16_t screen_height = k_max_malloc / this->width / 4;
  uint16_t screen_width = this->width;

  const uint screen_size = this->width * this->height * 4;

  for (uint i = 0; i * k_max_malloc < screen_size; i++)
    {
      // Set screen height that we have already captured
      // New screenshots will be captured with Y offset = image_height_offset
      int16_t screen_height_offset = i * screen_height;

      // Last screenshot will be smaller, so we set its height to what's left
      screen_height = std::min (screen_height,
                                uint16_t (this->height - screen_height_offset));

      // Request the screenshot of the virtual desktop
      auto gi_cookie = xcb_get_image (
          drawable::c_,              /* Connection */
          XCB_IMAGE_FORMAT_Z_PIXMAP, /* Z_Pixmap is 100 faster than XY_PIXMAP */
          drawable::screen_->root,   /* Screenshot relative to root */
          this->x,                   /* X offset */
          this->y + screen_height_offset, /* Y offset */
          screen_width, screen_height,    /* Dimensions */
          uint32_t (~0) /* Plane mask (all bits to get all planes) */
      );

      // Not freeing gi_reply causes memory leak, as xcb_get_image always
      // allocates new space for the image
      xcb_generic_error_t *e = nullptr;
      auto gi_reply = xcb_unique_ptr<xcb_get_image_reply_t> (
          xcb_get_image_reply (drawable::c_, gi_cookie, &e));
      check (e, "XCB error while getting image reply");

      this->image_ptr = xcb_get_image_data (gi_reply.get ());

      uint target_width = this->pixmap_width;
      uint target_height
          = float (screen_height) / screen_width * this->pixmap_width;

      uint target_height_offset ///< y coordinate of the screenshot
          = float (screen_height_offset) * this->pixmap_width / screen_width;

      /// Offset in bytes at which screenshot should be placed in the 1D array
      auto pixmap_offset = target_height_offset * target_width * 4;

      resize (this->image_ptr, this->pixmap.data () + pixmap_offset,
              screen_width, screen_height, target_width, target_height);
    }
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
