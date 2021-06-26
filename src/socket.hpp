#ifndef DEXPO_SOCKET_HPP
#define DEXPO_SOCKET_HPP

#include "desktop.hpp"
#include <atomic>
#include <cstdint>
#include <mutex>
#include <stdexcept>
#include <sys/socket.h>
#include <vector>

constexpr const char *k_socket_path = "/tmp/dexpo.socket";

/**
 * Dekstop struct that will be transferred over socket
 * Contains only necessary data
 */
struct dxp_socket_desktop
{
  int id; // _NET_CURRENT_DESKTOP
  uint16_t width;
  uint16_t height;
  uint32_t pixmap_len;
  std::vector<uint8_t> pixmap; ///< Pixmap in RBGA format
};

/**
 * All possible commands that can be sent from client to daemon
 */
enum dxp_event
{
  kRequestPixmaps = 1 // Request all pixmaps
};

class dxp_socket
{
public:
  int fd;                           ///< Socket File Descriptor
  std::atomic<bool> running = true; ///< Thread status

  dxp_socket ();
  ~dxp_socket ();

  // Explicitly delete unused constructors to comply with rule of five
  dxp_socket (const dxp_socket &other) = delete;
  dxp_socket (dxp_socket &&other) noexcept = delete;
  dxp_socket &operator= (const dxp_socket &other) = delete;
  dxp_socket &operator= (dxp_socket &&other) = delete;

  std::vector<dxp_socket_desktop> get_pixmaps () const;
  void send_pixmaps_on_event (const std::vector<dxp_socket_desktop> &,
                              std::mutex &pixmaps_lock) const;
  void server () const;
};

/*
 * Custom error classes to catch socket related errors
 */

class connect_error : public std::runtime_error
{
public:
  connect_error () : std::runtime_error ("Could not connect to the socket"){};
};

class socket_error : public std::runtime_error
{
public:
  socket_error ()
      : std::runtime_error ("Could not create socket file descriptor"){};
};

class write_error : public std::runtime_error
{
public:
  write_error ()
      : std::runtime_error ("Could not write into the file descriptor"){};
};

class read_error : public std::runtime_error
{
public:
  read_error ()
      : std::runtime_error ("Could not read from the file descriptor"){};
};

class bind_error : public std::runtime_error
{
public:
  bind_error () : std::runtime_error ("Could not bind name to the socket"){};
};

#endif /* ifndef DEXPO_SOCKET_HPP */
