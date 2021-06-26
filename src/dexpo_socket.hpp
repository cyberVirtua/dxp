#ifndef DEXPO_SOCKET
#define DEXPO_SOCKET

#include "desktop_pixmap.hpp"
#include <atomic>
#include <cstdint>
#include <mutex>
#include <stdexcept>
#include <sys/socket.h>
#include <vector>

constexpr const char *k_socket_path = "/tmp/dexpo.socket";

/**
 * Screenshot pixmap struct
 */
struct dexpo_pixmap
{
  int desktop_number; // _NET_CURRENT_DESKTOP
  uint16_t width;
  uint16_t height;
  uint32_t pixmap_len;
  std::vector<uint8_t> pixmap; ///< Pixmap in RBGA format
};

/**
 * All possible commands that can be sent from client to daemon
 */
enum daemon_event
{
  kRequestPixmaps = 1 // Request all pixmaps
};

class dexpo_socket
{
public:
  int fd;                           ///< Socket File Descriptor
  std::atomic<bool> running = true; ///< Thread status

  dexpo_socket ();
  ~dexpo_socket ();

  // Explicitly delete unused constructors to comply with rule of five
  dexpo_socket (const dexpo_socket &other) = delete;
  dexpo_socket (dexpo_socket &&other) noexcept = delete;
  dexpo_socket &operator= (const dexpo_socket &other) = delete;
  dexpo_socket &operator= (dexpo_socket &&other) = delete;

  std::vector<dexpo_pixmap> get_pixmaps () const;
  void send_pixmaps_on_event (const std::vector<dexpo_pixmap> &,
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

#endif /* ifndef DEXPO_SOCKET */
