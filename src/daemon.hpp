#ifndef DXP_DAEMON_HPP
#define DXP_DAEMON_HPP

#include <xcb/xcb.h>     // for xcb_connection_t
#include <xcb/xproto.h>  // for xcb_screen_t, xcb_window_t
#include <atomic>        // for atomic
#include <mutex>         // for mutex
#include <vector>        // for vector
#include "desktop.hpp"   // for dxp_desktop
#include "socket.hpp"    // for dxp_socket_desktop, dxp_socket
#include "xcb_util.hpp"  // for desktop_info

class dxp_daemon
{
public:
  xcb_connection_t *c;  ///< Xserver connection
  xcb_screen_t *screen; ///< X screen
  xcb_window_t root;
  std::vector<desktop_info> desktops_info;
  /// Desktops that correspond to virtual desktops
  std::vector<dxp_desktop> desktops;
  /// Desktop that will be sent over sockets
  std::vector<dxp_socket_desktop> socket_desktops;
  std::atomic<bool> running{ true }; ///< Thread status
  dxp_socket server;                 ///< Socket server
  std::mutex socket_desktops_lock;

  dxp_daemon ();
  void run ();
};

#endif /* ifndef DXP_DAEMON_HPP */
