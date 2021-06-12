#include "desktop_pixmap.hpp"
#include <iostream>
#include <math.h>
#include <ostream>
#include <xcb/xcb.h>
#include <xcb/xproto.h>

desktop_pixmap::desktop_pixmap (
    const short x,          ///< x coordinate of the top left corner
    const short y,          ///< y coordinate of the top left corner
    const uint16_t width,   ///< width of display
    const uint16_t height,  ///< height of display
    const std::string &name ///< _NET_WM_NAME of display
    )
    : drawable (x, y, width, height)
{
  this->name = name;
  this->format = kZPixmap; // Default to the fastest pixmap

  // Initializing non built-in types to zeros
  this->image_ptr = nullptr;
  this->pixmap_id = 0;
  this->length = 0;
  this->gi_reply = nullptr;

  // Initialize grahic context if it doesn't exist
  if (!desktop_pixmap::gc_)
    {
      create_gc ();
    }

  // Create a pixmap
  // TODO This is suboptimal for compressed screenshots
  create_pixmap ();
}

desktop_pixmap::~desktop_pixmap ()
{
  delete[] this->gi_reply;
  xcb_free_pixmap (drawable::c_, this->pixmap_id);
  // TODO Should you destroy grahic context?
}

/**
 * Saves pointer to the screen image in the specified pixmap_format.
 *
 * @param pixmap_format Bit format for the saved image. Defaults to the faster
 * Z_Pixmap.
 *
 * FIXME Implement screenshotter that will not capture entire screen at once
 * malloc() can reserve only 16711568 bytes for a pixmap.
 * But ZPixmap of QHD Ultrawide is 3440 * 1440 * 4 = 19814400 bytes long.
 */
void
desktop_pixmap::save_screen (pixmap_format pixmap_format)
{
  this->format = pixmap_format;

  // Not freeing gi_reply causes memory leak,
  // as xcb_get_image always allocates new space
  // This deallocates saved image on x server
  delete[] this->gi_reply;

  auto gi_cookie = xcb_get_image (
      drawable::c_,            /* Connection */
      this->format,            /* Image format */
      drawable::screen_->root, /* Screenshot relative to root */
      this->x, this->y, this->width, this->height, /* Dimensions */
      static_cast<uint32_t> (~0) /* Plane mask (all bits to get all planes) */
  );

  // TODO Handle errors
  // Saving reply to free it later. Fixes memory leak
  this->gi_reply = xcb_get_image_reply (drawable::c_, gi_cookie, nullptr);

  // Casting for later use
  this->length = static_cast<uint32_t> (xcb_get_image_data_length (gi_reply));
  this->image_ptr = xcb_get_image_data (gi_reply);
}

/**
 * Puts image on the instance's pixmap
 */
void
desktop_pixmap::put_screen () const
{
  xcb_put_image (drawable::c_, this->format,
                 this->pixmap_id, /* Pixmap to put image on */
                 desktop_pixmap::gc_, this->width, this->height, 0, 0, 0,
                 drawable::screen_->root_depth, this->length, this->image_ptr);
}

void
desktop_pixmap::create_gc ()
{
  uint32_t mask;
  uint32_t values[2];

  // TODO Figure out correct mask and values
  mask = XCB_GC_FOREGROUND | XCB_GC_GRAPHICS_EXPOSURES;
  values[0] = drawable::screen_->black_pixel;
  values[1] = 0;

  desktop_pixmap::gc_ = xcb_generate_id (c_);
  xcb_create_gc (drawable::c_, desktop_pixmap::gc_, drawable::screen_->root,
                 mask, values);
}

void
desktop_pixmap::create_pixmap ()
{
  this->pixmap_id = xcb_generate_id (drawable::c_);
  xcb_create_pixmap (drawable::c_, drawable::screen_->root_depth,
                     this->pixmap_id, drawable::screen_->root, this->width,
                     this->height);
}

void
desktop_pixmap::resize (int new_dimension, char flag)
{
  // Optional: add bilinear. See belinear_attempts.cpp. This one is ok,
  // Under condition old dimension/new dimension>20/13.
  // Some artifacts can appear however.
  // O(length)
  int new_width;
  int new_height;
  float det;
  float sum = 0;
  switch (flag)
    {
    case 'h':
      new_height = new_dimension;
      det = float (this->height) / float (new_height);
      new_width = round (float (this->width) / det);
      break;
    case 'w':
      new_width = new_dimension;
      det = float (this->width) / float (new_width);
      new_height = round (float (this->height) / det);
      break;
    };

  int upper = int (ceil (det));
  for (int j = 0; j < new_height; j++) // height, rows traversal, pixels!
    {

      for (int i = 0; i < new_width; i++) // width, bite in row traversal
        {
          for (int xi = 0; xi < 3; xi++) // color channel, r g b 0, r g b 0
            {
              for (int k = 0; k < upper;
                   k++) // the number of arguments in sum depends on ceil(det)!
                        // doesn't switch pixels!
                {
                  for (int z = 0; z < upper;
                       z++) // the number of arguments in sum depends on
                            // ceil(det)! doesn't switch pixels!
                    {
                      sum = sum
                            + this->image_ptr[int ((i * det)) * 4 + xi
                                              + this->width * 4
                                                    * (int ((j * det)) + k)
                                              + z * 4];
                      // aprox. column bite + color chanell + aprox. row * (cur
                      // row counter + count of bites needed by y) + count of
                      // bites needed by x * color chanell multiplier
                    };
                };
              this->image_ptr[i * 4 + xi + j * new_width * 4]
                  = sum / upper / upper;
              sum = 0;
            };
        };
    };

  this->length = new_height * new_width * 4;
  this->width = new_width;
  this->height = new_height;
}
