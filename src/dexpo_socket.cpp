#include "dexpo_socket.hpp"
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <ostream>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>

template <typename t>
void
handle_error (t var)
{
  if (var == -1)
    {
      throw;
    }
}

/**
 * Connect to socket.
 *
 * If socket does not exist:
 * - Create a socket file descriptor
 * - Bind socket to name
 * - Listen (specify max connection number)
 * - Accept
 *
 * If socket exists:
 * - Create a socket file descriptor
 * - Connect to socket
 */
dexpo_socket::dexpo_socket ()
{
  struct sockaddr_un sock_name = {};
  sock_name.sun_family = AF_UNIX;
  // Safely putting socket name into a buffer
  std::strncpy (sock_name.sun_path, SOCKET_PATH,
                sizeof (sock_name.sun_path) - 1);

  // XXX Make socket abstract
  // sock_name.sun_path[0] = '\0';

  // Create a socket file descriptor
  this->fd = socket (AF_UNIX, SOCK_STREAM, 0);
  handle_error (this->fd);

  // Trick compiler into thinking that we pass pointer to sockaddr struct
  // https://stackoverflow.com/questions/21099041
  auto *sock_addr = reinterpret_cast<struct sockaddr *> (&sock_name);

  // Connect to named socket
  int e = connect (this->fd, sock_addr, sizeof (sock_name));
  if (e == -1) // Socket doesn't exist
    {
      unlink (SOCKET_PATH);
      int ret = bind (this->fd, sock_addr, sizeof (sock_name));
      ret = listen (this->fd, 20);
      if (ret == -1)
        {
          close (this->fd);
          unlink (SOCKET_PATH);
          throw;
        }
    }
};

dexpo_socket::~dexpo_socket ()
{
  close (this->fd);
  unlink (SOCKET_PATH);
};

void
dexpo_socket::send (std::vector<dexpo_pixmap> pixmap_array) const
{
  // First write -- number of subsequent packets
  auto num = pixmap_array.size ();
  write (this->fd, &num, sizeof (num));
  // Second to `num` -- subsequent packets
  for (auto &pixmap : pixmap_array)
    {
      write (this->fd, &pixmap, sizeof (pixmap));
    }
};

/**
 * Request and receive dexpo_pixmaps from daemon
 */
std::vector<dexpo_pixmap>
dexpo_socket::get_pixmaps () const
{
  // Send pixmap request
  char cmd = kRequestPixmaps;
  if (write (this->fd, &cmd, 1) == -1)
    {
      throw;
    };

  dexpo_pixmap pixmap; // Pixmap to copy data in
  std::vector<dexpo_pixmap> pixmap_array;

  size_t num;
  auto rcv_bytes = read (this->fd, &num, sizeof (num));

  if (rcv_bytes == -1)
    {
      throw;
    }
  for (; num > 0; num--)
    {
      rcv_bytes = read (this->fd, &pixmap, sizeof (pixmap));
      pixmap_array.push_back (pixmap);
    }
  return pixmap_array;
};

void
dexpo_socket::send_pixmaps_on_event (daemon_event e,
                                     std::vector<dexpo_pixmap> &pixmap_array)
{
  auto data_fd = accept (this->fd, NULL, NULL);
  char cmd = -1;
  this->running = true;
  while (this->running)
    {
      if (read (data_fd, &cmd, 1) == e)
        {
          // First write -- number of subsequent packets
          auto num = pixmap_array.size ();
          write (data_fd, &num, sizeof (num));

          // Writes from second to `num` -- subsequent packets
          for (auto &pixmap : pixmap_array)
            {
              write (data_fd, &pixmap, sizeof (pixmap));
            }
        }
    }
};
