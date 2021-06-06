#include "DesktopPixmap.hpp"
#include <iostream>
#include <ostream>
#include <xcb/xcb.h>
#include <xcb/xproto.h>

DesktopPixmap::DesktopPixmap (
    const short x,          ///< x coordinate of the top left corner
    const short y,          ///< y coordinate of the top left corner
    const uint16_t width,   ///< width of display
    const uint16_t height,  ///< height of display
    const std::string &name ///< _NET_WM_NAME of display
)
{
  this->x = x;
  this->y = y;
  this->width = width;
  this->height = height;
  this->name = name;

  // Initializing non built-in types to zeros
  DesktopPixmap::gc_ = 0;
  this->image_ptr = nullptr;
  this->pixmap_id = 0;
  this->length = 0;

  // Connect to X. NULL will use $DISPLAY
  DesktopPixmap::c_ = xcb_connect (NULL, NULL);
  DesktopPixmap::screen_
      = xcb_setup_roots_iterator (xcb_get_setup (DesktopPixmap::c_)).data;

  // Initialize Graphic Context
  create_gc ();

  // Create a pixmap
  // TODO This is suboptimal for compressed screenshots
  create_pixmap ();
}

DesktopPixmap::~DesktopPixmap ()
{
  delete[] this->image_ptr;
  xcb_free_pixmap (DesktopPixmap::c_, this->pixmap_id);
  xcb_free_gc (DesktopPixmap::c_, DesktopPixmap::gc_);
  xcb_disconnect (DesktopPixmap::c_);
}

void
DesktopPixmap::saveImage (PixmapFormat pixmap_format)
{
  this->format = pixmap_format;

  // FIXME How to free image_ptr?
  // It causes memory leak, as xcb_get_image always allocates new space
  // delete[] this->image_ptr;

  auto gi_cookie = xcb_get_image (
      DesktopPixmap::c_,            /* Connection */
      this->format,                 /* Image format */
      DesktopPixmap::screen_->root, /* Screenshot relative to root */
      this->x, this->y, this->width, this->height, /* Dimensions */
      static_cast<uint32_t> (~0) /* Plane mask (all bits to get all planes) */
  );

  // TODO Handle errors
  auto *gi_reply = xcb_get_image_reply (DesktopPixmap::c_, gi_cookie, nullptr);

  // Casting for later use
  this->length = static_cast<uint32_t> (xcb_get_image_data_length (gi_reply));
  this->image_ptr = xcb_get_image_data (gi_reply);
}

void
DesktopPixmap::putImage () const
{
  xcb_put_image (DesktopPixmap::c_, this->format,
                 this->pixmap_id, /* Pixmap to put image on */
                 DesktopPixmap::gc_, this->width, this->height, 0, 0, 0,
                 DesktopPixmap::screen_->root_depth, this->length,
                 this->image_ptr);
}

void
DesktopPixmap::create_gc ()
{
  uint32_t mask;
  uint32_t values[2];

  // TODO Figure out correct mask and values
  mask = XCB_GC_FOREGROUND | XCB_GC_GRAPHICS_EXPOSURES;
  values[0] = DesktopPixmap::screen_->black_pixel;
  values[1] = 0;

  DesktopPixmap::gc_ = xcb_generate_id (c_);
  xcb_create_gc (DesktopPixmap::c_, DesktopPixmap::gc_,
                 DesktopPixmap::screen_->root, mask, values);
}

void
DesktopPixmap::create_pixmap ()
{
  this->pixmap_id = xcb_generate_id (DesktopPixmap::c_);
  xcb_create_pixmap (DesktopPixmap::c_, DesktopPixmap::screen_->root_depth,
                     this->pixmap_id, DesktopPixmap::screen_->root, this->width,
                     this->height);
}
