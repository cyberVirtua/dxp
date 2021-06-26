#include "desktop_pixmap.hpp"
#include "config.hpp"
#include <memory>
#include <xcb/xcb.h>
#include <xcb/xproto.h>

// Workaround for xcb_get_image_reply allocation. see save_screen()
// Represents maximum pixmap size in bytes that xcb_get_image can allocate
constexpr int k_max_malloc = 16711568;

desktop_pixmap::desktop_pixmap (
    const int16_t x,      ///< x coordinate of the top left corner
    const int16_t y,      ///< y coordinate of the top left corner
    const uint16_t width, ///< width of display
    const uint16_t height ///< height of display
    )
    : drawable (x, y, width, height)
{
  // Initializing non built-in types to zeros
  this->image_ptr = nullptr;
  this->length = 0;

  // Initialize graphic context if it doesn't exist
  if (!desktop_pixmap::gc_)
    {
      create_gc ();
    }

  // Check if both are set or unset simultaneously
  static_assert ((dexpo_height == 0) != (dexpo_width == 0),
                 "Height and width can't be set or unset simultaneously");

  this->pixmap_width = dexpo_width;
  this->pixmap_height = dexpo_height;

  if (this->pixmap_width == 0)
    {
      this->pixmap_width = uint16_t (double (this->width) / this->height
                                     * this->pixmap_height);
    }
  else if (this->pixmap_height == 0)
    {
      this->pixmap_height
          = uint16_t (double (this->height) / this->width * this->pixmap_width);
    }

  // Create a small pixmap with the size of downscaled screenshot from config
  create_pixmap ();
}

desktop_pixmap::~desktop_pixmap ()
{
  // xcb_free_pixmap (drawable::c_, this->pixmap_);
  // TODO Add separate class to manage graphic context's destructor
}

desktop_pixmap::desktop_pixmap (const desktop_pixmap &src)
    : drawable (src.x, src.y, src.width, src.height)
{
  this->image_ptr = src.image_ptr;
  this->length = src.length;
  this->pixmap = src.pixmap;
  this->pixmap_width = src.pixmap_width;
  this->pixmap_height = src.pixmap_height;
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
desktop_pixmap::save_screen ()
{
  // Set height so that screenshots won't exceed k_max_malloc size
  uint16_t image_height = uint16_t (k_max_malloc / this->width / 4);
  uint16_t image_width = this->width;

  const uint screen_size = uint (this->width * this->height * 4);

  uint16_t i = 0;
  while (i * k_max_malloc < screen_size)
    {
      // Set screen height that we have already captured
      // New screenshots will be captured with Y offset = image_height_offset
      int16_t image_height_offset = int16_t (i * image_height);

      // Last screenshot will be smaller, so we set its height to what's left
      image_height = std::min (image_height,
                               uint16_t (this->height - image_height_offset));

      // Request the screenshot of the virtual desktop
      auto gi_cookie = xcb_get_image (
          drawable::c_,              /* Connection */
          XCB_IMAGE_FORMAT_Z_PIXMAP, /* Z_Pixmap is 100 faster than XY_PIXMAP */
          drawable::screen_->root,   /* Screenshot relative to root */
          this->x,                   /* X offset */
          int16_t (this->y + image_height_offset), /* Y offset */
          image_width, image_height,               /* Dimensions */
          uint32_t (~0) /* Plane mask (all bits to get all planes) */
      );

      // TODO Handle errors
      // Not freeing gi_reply causes memory leak, as xcb_get_image always
      // allocates new space for the image
      auto gi_reply = std::unique_ptr<xcb_get_image_reply_t> (
          xcb_get_image_reply (drawable::c_, gi_cookie, nullptr));

      // Casting for later use
      this->length = uint32_t (xcb_get_image_data_length (gi_reply.get ()));
      this->image_ptr = xcb_get_image_data (gi_reply.get ());

      int target_width = this->pixmap_width;
      int target_height
          = int (float (image_height) / image_width * this->pixmap_width);

      this->length = uint (target_width * target_height * 4);
      int target_height_offset
          = int (float (image_height_offset) * pixmap_width / image_width);

      uint32_t pixmap_offset
          = uint32_t (target_height_offset * target_width * 4);

      resize (this->image_ptr, this->pixmap.data () + pixmap_offset,
              image_width, image_height, target_width, target_height);
      i++;
    }
}

/**
 * Definitely copied. Looks like I'm too retarded to code this myself.
 * https://stackoverflow.com/questions/28566290
 *
 * TODO Optimize and fix warnings, comment on names, add anti aliasing
 */
void
desktop_pixmap::resize (const uint8_t *input, uint8_t *output,
                        int source_width, /* Source dimensions */
                        int source_height, int target_width, int target_height)
{
  const int x_ratio = (source_width << 16) / target_width;
  const int y_ratio = (source_height << 16) / target_height;
  const int colors = 4;

  for (int y = 0; y < target_height; y++)
    {
      int y2_xsource = ((y * y_ratio) >> 16) * source_width;
      int i_xdest = y * target_width;

      for (int x = 0; x < target_width; x++)
        {
          int x2 = ((x * x_ratio) >> 16);
          int y2_x2_colors = (y2_xsource + x2) * colors;
          int i_x_colors = (i_xdest + x) * colors;

          output[i_x_colors] = input[y2_x2_colors];
          output[i_x_colors + 1] = input[y2_x2_colors + 1];
          output[i_x_colors + 2] = input[y2_x2_colors + 2];
          output[i_x_colors + 3] = input[y2_x2_colors + 3];
        }
    }
}

void
desktop_pixmap::create_gc ()
{
  uint32_t mask = 0;
  uint32_t values[2];

  // TODO Figure out correct mask and values
  mask = XCB_GC_FOREGROUND | XCB_GC_GRAPHICS_EXPOSURES;
  values[0] = drawable::screen_->black_pixel;
  values[1] = 0;

  desktop_pixmap::gc_ = xcb_generate_id (c_);
  xcb_create_gc (drawable::c_, desktop_pixmap::gc_, drawable::screen_->root,
                 mask, &values);
}

void
desktop_pixmap::create_pixmap ()
{
  this->pixmap.resize (this->pixmap_width * this->pixmap_height * 4);
}
