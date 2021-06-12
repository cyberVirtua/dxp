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
  pixmap_format format;   /* XY_PIXMAP = 1, Z_PIXMAP = 2 */
  xcb_pixmap_t pixmap_id; ///< Constant id for the screenshot's pixmap
  xcb_get_image_reply_t *gi_reply; ///< get_image reply. Saving to free it later

  desktop_pixmap (
      short x,                ///< x coordinate of the top left corner
      short y,                ///< y coordinate of the top left corner
      uint16_t width,         ///< Width of the display
      uint16_t height,        ///< Height of the display
      const std::string &name ///< Name of the display (_NET_WM_NAME)
  );
  ~desktop_pixmap ();

  /**
   * Screenshots current desktop and saves it inside instance
   */
  void save_screen (pixmap_format format = kZPixmap); // Default to fastest
  /**
   * Saves screenshot to the instance's pixmap
   */
  void put_screen () const;
  /**
   * TODO Resizes screenshot to specified dimensions
   */
  void resize (int new_dim, char flag);

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
