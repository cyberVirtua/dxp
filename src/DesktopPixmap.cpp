#include "DesktopPixmap.hpp"
#include <iostream>
#include <ostream>
#include <xcb/xcb.h>
#include <xcb/xproto.h>

DesktopPixmap::DesktopPixmap (
    const short x,          ///< x coordinate of the top left corner
    const short y,          ///< y coordinate of the top left corner
    const uint16_t height,  ///< height of display
    const uint16_t width,   ///< width of display
    const std::string &name ///< _NET_WM_NAME of display
)
{
  this->x = x;
  this->y = y;
  this->height = height;
  this->width = width;
  this->name = name; // XXX Is this OK?

  // Initializing non built-in types to zeros
  DesktopPixmap::gc_ = 0;
  this->image_ptr = nullptr;
  this->pixmap_id = 0;
  this->length = 0;
}

DesktopPixmap::~DesktopPixmap ()
{
  delete[] this->image_ptr;
  // TODO Might need to delete pixmap
}

void
DesktopPixmap::saveImage (xcb_connection_t *c, xcb_screen_t &screen,
                          PixmapFormat pixmap_format)
{
  // Initialize graphic context if it does not exist
  if (!DesktopPixmap::gc_)
    {
      create_gc (c, screen);
    }

  // Initialize pixmap if it does not exist
  if (!this->pixmap_id)
    {
      create_pixmap (c, screen);
    }

  this->format = pixmap_format;

  auto gi_cookie = xcb_get_image (
      c,            /* Connection */
      this->format, /* Image format */
      screen.root,  /* Screenshot relative to root */
      this->x, this->y, this->width, this->height, /* Dimensions */
      static_cast<uint32_t> (~0) /* Plane mask (all bits to get all planes) */
  );

  // TODO Handle errors
  auto *gi_reply = xcb_get_image_reply (c, gi_cookie, nullptr);

  // Casting for later use
  this->length = static_cast<uint32_t> (xcb_get_image_data_length (gi_reply));
  this->image_ptr = xcb_get_image_data (gi_reply);
}

void
DesktopPixmap::putImage (xcb_connection_t *c, xcb_screen_t &screen)
{
  xcb_put_image (c, this->format, this->pixmap_id, /* Pixmap to put image on */
                 DesktopPixmap::gc_,               /* Graphic context */
                 this->width, this->height, 0, 0, 0, /* Dimensions */
                 screen.root_depth, this->length, this->image_ptr);
}

void
DesktopPixmap::create_gc (xcb_connection_t *c, xcb_screen_t &screen)
{
  uint32_t mask;
  uint32_t values[2];

  // TODO Figure out correct mask and values
  mask = XCB_GC_FOREGROUND | XCB_GC_GRAPHICS_EXPOSURES;
  values[0] = screen.black_pixel;
  values[1] = 0;

  DesktopPixmap::gc_ = xcb_generate_id (c);
  xcb_create_gc (c, DesktopPixmap::gc_, screen.root, mask, values);
}

void
DesktopPixmap::create_pixmap (xcb_connection_t *c, xcb_screen_t &screen)
{
  this->pixmap_id = xcb_generate_id (c);
  xcb_create_pixmap (c, screen.root_depth, this->pixmap_id,
                     screen.root, // TODO Check if this is OK
                     this->width, this->height);
}
