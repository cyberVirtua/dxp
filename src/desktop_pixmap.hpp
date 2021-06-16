#ifndef DESKTOP_PIXMAP_HPP
#define DESKTOP_PIXMAP_HPP

#include "drawable.hpp"
#include <string>
#include <xcb/xproto.h>

enum pixmap_format
{
  // TODO XY Bitmap does not work
  kXyPixmap = 1, // 100 times slower than kZPixmap for big pixmap
  kZPixmap = 2
};

/**
 * Captures and stores desktop screenshot in the specified pixmap format
 */
class desktop_pixmap : public drawable
{
public:
  inline static xcb_gcontext_t gc_; ///< Graphic context
  uint8_t *image_ptr;     ///< Pointer to the image structure. TODO Make smart?
  uint32_t length;        ///< Length of the XImage data structure
  std::string name;       ///< _NET_WM_NAME of display
  xcb_pixmap_t pixmap_id; ///< Constant id for the screenshot's pixmap
  uint16_t pixmap_width;  ///< dimensions of pixmap that stores screenshot
  uint16_t pixmap_height;

  desktop_pixmap (
      int16_t x,              ///< x coordinate of the top left corner
      int16_t y,              ///< y coordinate of the top left corner
      uint16_t width,         ///< Width of the display
      uint16_t height,        ///< Height of the display
      const std::string &name ///< Name of the display (_NET_WM_NAME)
  );
  ~desktop_pixmap (); // deleter

  // Explicitly deleting unused constructors to comply with rule of five
  desktop_pixmap (const desktop_pixmap &other) = delete;
  desktop_pixmap (desktop_pixmap &&other) noexcept = delete;
  desktop_pixmap &operator= (const desktop_pixmap &other) = delete;
  desktop_pixmap &operator= (desktop_pixmap &&other) = delete;

  /**
   * Screenshots current desktop and saves it inside instance
   */
  void save_screen ();
  /**
   * TODO Resizes screenshot to specified dimensions
   */
  void resize (const uint8_t *input, uint8_t *output,
               int image_width, ///< Width of source image
               int image_height, int target_width,
               int target_height); ///< Height of source image

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
