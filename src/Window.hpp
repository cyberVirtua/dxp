#ifndef WINDOW_HPP
#define WINDOW_HPP

#include <xcb/xproto.h>
#include <string>
#include <vector>
#include <dexpo_socket.hpp>

/* Visualizes workspaces previews for user */
class Window
{
public: 
  inline static xcb_connection_t *c_;   // Connection to X-server
  inline static xcb_screen_t *screen_;  // X screen
  uint32_t win_id;                      // Identificator for Window
  short x;                              // x coordinate of the top left corner
  short y;                              // y coordinate of the top left corner
  short b_width;                        // Border width NOTE: Maybe it could be changed to constant
  std::string name;                     // _NET_WM_NAME of display, MAYBE UNNEEDED THERE
  uint16_t width;
  uint16_t height;
  uint32_t parent;
  // TODO: Add masks

  //The same as for DesktopPixmap
  Window (short x,         ///< x coordinate of the top left corner
          short y,         ///< y coordinate of the top left corner
          uint16_t width,  ///< Width of the display
          uint16_t height,  ///< Height of the display
          const std::string &name, ///< Name of the display (_NET_WM_NAME)
          uint32_t parent
  );
  ~Window ();

  /* Gets coordinates of top left corner of screenshot with given number */
  int getScreenPosition (int DesktopNumber, std::vector <dexpo_pixmap> v);

  /* Creates a visible window */
  void createWindow ();

  /* Creates new window inside the existing one */
  void connect_to_parent_Window (xcb_connection_t *c, xcb_screen_t *screen);

};

#endif /* ifndef WINDOW_HPP */