#ifndef DXP_DAEMON_HPP
#define DXP_DAEMON_HPP

#include "socket.hpp"
#include "xcb_util.hpp"
#include <mutex>
#include <vector>
#include <xcb/xproto.h>

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
