#ifndef DESKTOP_PIXMAP_HPP
#define DESKTOP_PIXMAP_HPP

#include <string>
#include <xcb/xproto.h>

enum PixmapFormat
{
  kXyPixmap = 1,
  kZPixmap = 2
};

/**
 * Captures and stores desktop screenshot in the specified pixmap format
 *
 * XXX Maybe use static xcb_connection_t?
 */
class DesktopPixmap
{
public:
  inline static xcb_gcontext_t gc_;    ///< Graphic context
  inline static xcb_connection_t *c_;  ///< Xserver connection
  inline static xcb_screen_t *screen_; ///< X screen
  short x;                             ///< x coordinate of the top left corner
  short y;                             ///< y coordinate of the top left corner
  uint16_t width;
  uint16_t height;
  uint8_t *image_ptr;     ///< Pointer to the image structure. TODO Make smart?
  uint32_t length;        ///< Length of the XImage data structure
  std::string name;       ///< _NET_WM_NAME of display
  PixmapFormat format;    /* XY_PIXMAP = 1, Z_PIXMAP = 2 */
  xcb_pixmap_t pixmap_id; ///< Constant id for the screenshot's pixmap

  DesktopPixmap (short x,         ///< x coordinate of the top left corner
                 short y,         ///< y coordinate of the top left corner
                 uint16_t width,  ///< Width of the display
                 uint16_t height, ///< Height of the display
                 const std::string &name ///< Name of the display (_NET_WM_NAME)
  );
  ~DesktopPixmap ();

  /**
   * TODO Rename to something more fitting
   */
  void saveImage (PixmapFormat format = kZPixmap); // Default to more efficient
  /**
   * Saves image to the instance's pixmap
   */
  void putImage () const;

private:
  static void create_gc ();
  void create_pixmap ();
};

#endif /* ifndef DESKTOP_PIXMAP_HPP */
