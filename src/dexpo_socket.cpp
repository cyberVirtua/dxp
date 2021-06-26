#include "dexpo_socket.hpp"
#include <cstring>
#include <iostream>
#include <mutex>
#include <signal.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>

/**
 * Thrower for custom errors.
 * Exists to make error throwing fit into one line.
 */
template <typename E, typename S>
void
is (S status) // This name looks readable: is<socket_error> (this->fd)
{
  // Check if this is one of the custom errors, because this
  // won't work for functions that return something other than -1 on error
  static_assert (std::is_base_of<std::runtime_error, E>::value);

  if (status == -1) // Most of the C libs return -1 on error
    {
      throw E ();
    }
}

/**
 * read(2) is not guaranteed to read all sent data in one read,
 * so we may need to read from the socket multiple times to get everything.
 *
 * @param dest destination address
 */
template <typename D>
void
read_unix (int fd, D *dest, size_t length)
{
  size_t received = 0; // Number of bytes that we have already read
  ssize_t rcv = 0;     // Response of the read(2)

  while (received != length)
    {
      // Read what is left to read with an offset
      rcv = read (fd, dest + received, length - received);
      is<read_error> (rcv);

      // Increment the received amount with the number of bytes just received
      received += size_t (rcv);
    }
}

/**
 * read(2) is not guaranteed to read all sent data in one read,
 * so we may need to read from the socket multiple times to get everything.
 *
 * @param dest destination address
 */
template <typename D>
void
write_unix (int fd, D *src, size_t length)
{
  size_t sent = 0; // Number of bytes that we have already sent
  ssize_t wr = 0;  // Response of the write(2). Number of bytes written

  while (sent != length)
    {
      // Send what is left to send with an offset
      wr = write (fd, src + sent, length - sent);
      is<read_error> (wr);

      // Increment the sent amount with the number of bytes just transferred
      sent += size_t (wr);
    }
}

/**
 * Connect to socket.
 *
 * If socket does not exist:
 * - Create a socket file descriptor
 * - Bind socket to name
 * - Listen (specify max connection number)
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
  std::strncpy (&sock_name.sun_path[0], k_socket_path,
                std::strlen (k_socket_path) + 1);

  // Trick compiler into thinking that we pass pointer to sockaddr struct
  // https://stackoverflow.com/questions/21099041
  auto *sock_addr = reinterpret_cast<struct sockaddr *> (&sock_name);

  // XXX Make socket abstract
  // sock_name.sun_path[0] = '\0';

  int s = 0; // Return status of functions to check for errors
  try
    {
      // Create a socket file descriptor
      this->fd = socket (AF_UNIX, SOCK_STREAM, 0);
      is<socket_error> (this->fd);

      // Connect to named socket
      s = connect (this->fd, sock_addr, sizeof (sock_name));
      is<connect_error> (s);
    }
  catch (const connect_error &e)
    {
      // This should be run only for the first connection, as socket may not
      // exist then.
      // And should not be run when there are active connections to the socket

      unlink (k_socket_path); // Remove existing socket

      s = bind (this->fd, sock_addr, sizeof (sock_name)); // Bind name to fd
      s = listen (this->fd, 2);                           // 2 is arbitrary
      is<bind_error> (s);
    }
  catch (const socket_error &e)
    {
      std::cerr << e.what () << std::endl;
      throw;
    }
  catch (...)
    {
      throw;
    }
};

dexpo_socket::~dexpo_socket () { close (this->fd); };

/**
 * Request and receive dexpo_pixmaps from daemon
 */
std::vector<dexpo_pixmap>
dexpo_socket::get_pixmaps () const
{
  // Send pixmap request to daemon
  char cmd = kRequestPixmaps;
  write_unix (this->fd, &cmd, 1);

  std::vector<dexpo_pixmap> pixmap_array; // Pixmap array that will be returned

  size_t num = 0; // Amount of pixmaps that will be received
  read_unix (this->fd, &num, sizeof (num));

  for (; num > 0; num--) // Read `num` pixmaps from socket
    {
      // TODO Check for errors here
      dexpo_pixmap p;

      // Reading everything except raw pixmap
      read_unix (this->fd, &p, offsetof (dexpo_pixmap, pixmap));

      // Allocating space for an incoming pixmap
      p.pixmap.resize (p.pixmap_len);

      // Reading raw pixmap
      read_unix (this->fd, p.pixmap.data (), size_t (p.pixmap_len));

      pixmap_array.push_back (p);
    }
  return pixmap_array; // Compiler is smart so vector won't be copied here
};

/**
 * Starts an infinite loop that listens for the kRequestPixmaps write
 * and sends pixmaps one by one in return
 */
void
dexpo_socket::send_pixmaps_on_event (const std::vector<dexpo_pixmap> &pixmaps,
                                     std::mutex &pixmaps_lock) const
{
  int data_fd = 0; // Socket file descriptor

  while ((data_fd = accept (this->fd, nullptr, nullptr)) && this->running)
    {
      // Check the incoming command
      char cmd = -1;
      read_unix (data_fd, &cmd, 1);

      if (cmd == kRequestPixmaps)
        {
          // Lock pixmaps to prevent race condition when reading and writing
          // pixmaps to socket
          std::scoped_lock<std::mutex> guard (pixmaps_lock);

          // First write -- number of subsequent packets
          size_t num = pixmaps.size ();
          write_unix (data_fd, &num, sizeof (num));

          // Writes from second to `num+1` -- subsequent packets
          for (const auto &p : pixmaps)
            {
              // Sending everything except raw pixmap
              write_unix (data_fd, &p, offsetof (dexpo_pixmap, pixmap));

              // Sending pixmap
              write_unix (data_fd, p.pixmap.data (), p.pixmap_len);
            }
        }
    }
};
