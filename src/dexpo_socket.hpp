#ifndef DEXPO_SOCKET
#define DEXPO_SOCKET

#include "desktop_pixmap.hpp"
#include <cstdint>
#include <stdexcept>
#include <sys/socket.h>
#include <vector>

#define SOCKET_PATH "/tmp/dexpo.socket"
#define DESKTOP_NAME_MAX_LEN 255

/**
 * Screenshot pixmap struct
 */
struct dexpo_pixmap
{
  int desktop_number; // _NET_CURRENT_DESKTOP
  uint16_t width;
  uint16_t height;
  xcb_pixmap_t id; ///< Id of the screenshot's pixmap
  char name[DESKTOP_NAME_MAX_LEN];
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
  int fd;       ///< Socket File Descriptor
  bool running; ///< Thread status

  dexpo_socket ();
  ~dexpo_socket ();

  void send (dexpo_pixmap &) const;
  void send (std::vector<dexpo_pixmap>) const;

  std::vector<dexpo_pixmap> get_pixmaps () const;
  void send_pixmaps_on_event (std::vector<dexpo_pixmap> &);
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
