#include "desktop_pixmap.hpp"
#include "config.hpp"
#include <iostream>
#include <math.h>
#include <ostream>
#include <xcb/xcb.h>
#include <xcb/xproto.h>
#define MAX_MALLOC 16711568

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

  // Calculate pixmap width and height based on config values
  this->pixmap_width = k_dexpo_width;
  this->pixmap_height = k_dexpo_height;
  if (this->pixmap_width==0)
  {
    this->pixmap_width=(float(this->width)/float(this->height))*k_dexpo_height;
  }

  if (this->pixmap_height==0)
  {
    this->pixmap_height=(float(this->height)/float(this->width))*k_dexpo_width;
  }

  // Create a pixmap
  // TODO This is suboptimal for compressed screenshots
  create_pixmap (this->pixmap_width, this->pixmap_height);
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
 *
 * malloc() can reserve only 16711568 bytes for a pixmap.
 * But ZPixmap of QHD Ultrawide is 3440 * 1440 * 4 = 19814400 bytes long.
 * The while() loop in save_screen fixes this.
 */
void
desktop_pixmap::save_screen (pixmap_format pixmap_format)
{
  this->format = pixmap_format;

  uint16_t screen_width = this->width;
  uint16_t screen_height = this->height;

  uint16_t resized_width = pixmap_width;
  uint16_t resized_height = pixmap_height;

  uint16_t max_height = MAX_MALLOC / screen_width / 4;

  int16_t y_exception; // max_height might be changed in last iteration,
  // y must stay i*previous max_height

  int i = 0;
  while (i * MAX_MALLOC < screen_width * screen_height * 4)
    {

      y_exception = i * max_height; //sets y point to copy from

      if (max_height > screen_height - max_height * i)//last iteration may receive a smaller than MAX_MALLOC image
        {
          max_height = screen_height - max_height * i;
        }; 

      //gets image or image part
      auto gi_cookie = xcb_get_image (
          drawable::c_,            /* Connection */
          this->format,            /* Image format */
          drawable::screen_->root, /* Screenshot relative to root */
          0, y_exception, screen_width, max_height, /* Dimensions */
          uint32_t (~0) /* Plane mask (all bits to get all planes) */
      );

      // TODO Handle errors
      // Saving reply to free it later. Fixes memory leak
      this->gi_reply = xcb_get_image_reply (drawable::c_, gi_cookie, nullptr);

      // Casting for later use
      this->length
          = uint32_t (xcb_get_image_data_length (gi_reply));
      this->image_ptr = xcb_get_image_data (gi_reply);

      y_exception = this->height * i;//sets y point to copy to

      // TODO: add case if resized height is defined instead of width.
      resize (
          screen_width, max_height, resized_width,
          'w'); 

      // we use this->height and this-> width because this params change in
      // resize
      xcb_put_image (drawable::c_, this->format,
                     this->pixmap_id, /* Pixmap to put image on */
                     desktop_pixmap::gc_, this->width, this->height, 0,
                     y_exception, 0, drawable::screen_->root_depth,
                     this->length, this->image_ptr);

      i++;
      // Not freeing gi_reply causes memory leak,
      // as xcb_get_image always allocates new space
      // This deallocates saved image on x server
      free(this->gi_reply);
    }
  this->width = resized_width;
  this->height = resized_height;
}

void 
desktop_pixmap::render_geometries ()
{
  
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
desktop_pixmap::create_pixmap (int pix_width, int pix_height)
{
  this->pixmap_id = xcb_generate_id (drawable::c_);
  xcb_create_pixmap (drawable::c_, drawable::screen_->root_depth,
                     this->pixmap_id, drawable::screen_->root, pix_width,
                     pix_height);
}

void
desktop_pixmap::resize (int old_width, int old_height, int new_dimension,
                        char flag)
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
      det = float (old_height) / float (new_height);
      new_width = std::lround (float (old_width) / det);
      break;
    case 'w':
      new_width = new_dimension;
      det = float (old_width) / float (new_width);
      new_height = std::lround (float (old_height) / det);
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
                            + this->image_ptr[std::lround (i * det) * 4 + xi
                                              + old_width * 4
                                                    * (std::lround (j * det) + k)
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
