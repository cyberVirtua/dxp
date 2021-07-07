#ifndef DEXPO_SOCKET_HPP
#define DEXPO_SOCKET_HPP

#include "desktop.hpp"
#include <atomic>
#include <cstdint>
#include <mutex>
#include <stdexcept>
#include <sys/socket.h>
#include <vector>

constexpr const char *k_socket_path = "/tmp/dxp.socket";

/**
 * Dekstop struct that will be transferred over socket
 * Contains only necessary data
 */
struct dxp_socket_desktop
{
  uint id; // _NET_CURRENT_DESKTOP
  uint16_t width;
  uint16_t height;
  uint32_t pixmap_len;
  std::vector<uint8_t> pixmap; ///< Pixmap in RBGA format
};

/**
 * All commands that can be sent by client to daemon
 */
enum dxp_event
{
  kRequestDesktops = 1 // Request all pixmaps
};

class dxp_socket
{
public:
  int fd;                            ///< Socket File Descriptor
  std::atomic<bool> running{ true }; ///< Thread status. Used to kill while loop

  dxp_socket ();
  ~dxp_socket ();

  // Explicitly delete unused constructors to comply with the rule of five
  dxp_socket (const dxp_socket &other) = delete;
  dxp_socket (dxp_socket &&other) noexcept = delete;
  dxp_socket &operator= (const dxp_socket &other) = delete;
  dxp_socket &operator= (dxp_socket &&other) = delete;

  [[nodiscard]] std::vector<dxp_socket_desktop> get_desktops () const;
  void send_desktops_on_event (const std::vector<dxp_socket_desktop> &,
                               std::mutex &desktops_lock) const;
  void server () const;
};

/*
 * Custom error classes to catch socket related errors
 */

class connect_error : public std::runtime_error
{
public:
  // Default constructor exists only to print errno
  connect_error ()
      : std::runtime_error ("Got an error while connecting to the socket"){};
  explicit connect_error (const std::string &msg) : std::runtime_error (msg){};
};

class socket_error : public std::runtime_error
{
public:
  socket_error ()
      : std::runtime_error (
          "Got an error while creating a socket file descriptor"){};
  explicit socket_error (const std::string &msg) : std::runtime_error (msg){};
};

class write_error : public std::runtime_error
{
public:
  write_error ()
      : std::runtime_error ("Got an error while writing to the socket"){};
  explicit write_error (const std::string &msg) : std::runtime_error (msg){};
};

class read_error : public std::runtime_error
{
public:
  read_error ()
      : std::runtime_error ("Got an error while reading from the socket"){};
  explicit read_error (const std::string &msg) : std::runtime_error (msg){};
};

class bind_error : public std::runtime_error
{
public:
  bind_error ()
      : std::runtime_error ("Got an error while binding name to the socket"){};
  explicit bind_error (const std::string &msg) : std::runtime_error (msg){};
};

#endif /* ifndef DEXPO_SOCKET_HPP */
