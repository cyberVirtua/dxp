#include "desktop_pixmap.hpp"
#include "config.hpp"
#include <memory>
#include <xcb/xcb.h>
#include <xcb/xproto.h>

#define MAX_MALLOC 16711568 // Workaround for xcb_get_image_reply allocation

desktop_pixmap::desktop_pixmap (
    const int16_t x,        ///< x coordinate of the top left corner
    const int16_t y,        ///< y coordinate of the top left corner
    const uint16_t width,   ///< width of display
    const uint16_t height,  ///< height of display
    const std::string &name ///< _NET_WM_NAME of display
    )
    : drawable (x, y, width, height)
{
  this->name = name;

  // Initializing non built-in types to zeros
  this->image_ptr = nullptr;
  this->pixmap_id = 0;
  this->length = 0;

  // Initialize graphic context if it doesn't exist
  if (!desktop_pixmap::gc_)
    {
      create_gc ();
    }

  // Check if both are set or unset simultaneously
  static_assert ((k_dexpo_height == 0) != (k_dexpo_width == 0),
                 "Height and width can't be set or unset simultaneously");

  this->pixmap_width = k_dexpo_width;
  this->pixmap_height = k_dexpo_height;

  if (this->pixmap_width == 0)
    {
      this->pixmap_width = (this->width / this->height) * this->pixmap_height;
    }
  else if (this->pixmap_height == 0)
    {
      this->pixmap_height
          = uint16_t (double (this->height) / double (this->width)
                      * double (this->pixmap_width));
    }

  // Create a small pixmap
  create_pixmap ();
}

desktop_pixmap::~desktop_pixmap ()
{
  xcb_free_pixmap (drawable::c_, this->pixmap_id);
  // TODO Add separate class to manage graphic context's destructor
}

/**
 * Saves pointer to the screen image in the specified pixmap_format.
 *
 * @param pixmap_format Bit format for the saved image. Defaults to the faster
 * Z_Pixmap.
 *
 * malloc() can reserve only 16711568 bytes for a pixmap.
 * But ZPixmap of QHD Ultrawide is 3440 * 1440 * 4 = 19814400 bytes long.
 *
 * The while() loop in save_screen fixes this. It takes multiple screenshots of
 * the screen (top to bottom), each taking up less than MAX_MALLOC. It downsizes
 * each screenshot and puts it on the pixmap.
 */
void
desktop_pixmap::save_screen ()
{
  // Set height so that screenshots won't exceed MAX_MALLOC size
  uint16_t image_height = uint16_t (MAX_MALLOC / this->width / 4);
  uint16_t image_width = this->width;

  const uint screen_size = (this->width * this->height * 4);

  uint16_t i = 0;
  while (i * MAX_MALLOC < screen_size)
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
      int target_height = int (float (image_height) / float (image_width)
                               * float (this->pixmap_width));

      auto pixmap = std::make_unique<uint8_t[]> (this->length + 1);

      resize (this->image_ptr, pixmap.get (), image_width, image_height,
              target_width, target_height);
      this->length = uint (target_width * target_height * 4);

      xcb_put_image (drawable::c_, XCB_IMAGE_FORMAT_Z_PIXMAP,
                     this->pixmap_id,             /* Pixmap to put image on */
                     desktop_pixmap::gc_,         /* Graphic context */
                     target_width, target_height, /* Dimensions */
                     0,                           /* Destination X coordinate */
                     image_height_offset,         /* Destination Y coordinate */
                     0, drawable::screen_->root_depth,
                     this->length, /* Image size in bytes */
                     pixmap.get ());

      i++;
    }
}

/**
 * Definetely copied. Looks like I'm too retarded to code this myself.
 * https://stackoverflow.com/questions/28566290
 *
 * TODO Optimize and fix warnings
 */
void
desktop_pixmap::resize (const uint8_t *input, uint8_t *output,
                        int source_width, /* Source dimensions */
                        int source_height, int target_width, int target_height)
{
  const int x_ratio = (int)((source_width << 16) / target_width);
  const int y_ratio = (int)((source_height << 16) / target_height);
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
  this->pixmap_id = xcb_generate_id (drawable::c_);
  xcb_create_pixmap (drawable::c_, drawable::screen_->root_depth,
                     this->pixmap_id, drawable::screen_->root,
                     this->pixmap_width, this->pixmap_height);
}
