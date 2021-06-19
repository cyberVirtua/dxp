#ifndef DESKTOP_PIXMAP_HPP
#define DESKTOP_PIXMAP_HPP

#include "drawable.hpp"
#include <string>
#include <xcb/xproto.h>

enum pixmap_format
{
  kXyPixmap = 1, // 100 times slower than kZPixmap for big image
  kZPixmap = 2
};

/**
 * Captures and stores desktop screenshot in the specified pixmap format
 */
class desktop_pixmap : public drawable
{
public:
  inline static xcb_gcontext_t gc_ = 0; ///< Graphic context
  uint8_t *image_ptr;                   ///< Pointer to the image structure.
  uint32_t length;                      ///< Length of the XImage data structure
  std::string name;                     ///< _NET_WM_NAME of display
  xcb_pixmap_t pixmap_id; ///< Constant id for the screenshot's pixmap
  uint16_t pixmap_width;  ///< Width of the pixmap that stores screenshot
  uint16_t pixmap_height; ///< Height of the pixmap that stores screenshot

  desktop_pixmap (
      int16_t x,              ///< x coordinate of the top left corner
      int16_t y,              ///< y coordinate of the top left corner
      uint16_t width,         ///< Width of the display
      uint16_t height,        ///< Height of the display
      const std::string &name ///< Name of the display (_NET_WM_NAME)
  );
  ~desktop_pixmap ();

  // Declaring copy operator to use it in vector
  desktop_pixmap (const desktop_pixmap &);

  /**
   * Screenshots current desktop, downsizes it and stores inside instance
   */
  void save_screen ();
  /**
   * Resizes screenshot to specified dimensions
   */
  static void
  resize (const uint8_t *input, ///< Input RGBA 1D array pointer
          uint8_t *output,      ///< Output RGBA 1D array pointers
          int source_width,     /* Dimensions of unresized screenshot */
          int source_height,
          int target_width, /* Dimensions of screenshot after resize */
          int target_height);

  void render_geometries ();

private:
  /**
   * Initializes class-level graphic context
   */
  static void create_gc ();
  /**
   * Initializes instance-level pixmap
   */
  void create_pixmap ();
};

#endif /* ifndef DESKTOP_PIXMAP_HPP */
