#include "socket.hpp"
#include <stddef.h>      // for offsetof
#include <stdio.h>       // for perror
#include <sys/socket.h>  // for accept4, bind, connect, listen, socket, AF_UNIX
#include <sys/un.h>      // for sockaddr_un
#include <unistd.h>      // for ssize_t, close, unlink, read, write
#include <cstring>       // for size_t, strlen, strncpy
#include <mutex>         // for mutex, scoped_lock
#include <type_traits>   // for is_base_of

/**
 * Thrower for custom errors.
 * Exists to make error throwing fit into one line.
 *
 * Its name looks readable: is<socket_error> (this->fd)
 */
template <typename E, typename S>
void
is (S status, const std::string &error_msg)
{
  // Check if this is one of the custom errors, because this
  // won't work for functions that return something other than -1 on error
  static_assert (std::is_base_of<std::runtime_error, E>::value);

  if (status == -1) // Most of the C libs return -1 on error
    {
      perror (E ().what ());
      throw E (error_msg);
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
read_unix (int fd, D *dest, size_t length, const std::string &error_msg)
{
  size_t received = 0; // Number of bytes that we have already read
  ssize_t rcv = 0;     // Response of the read(2)

  while (received != length)
    {
      // Read what is left to read with an offset
      rcv = read (fd, dest + received, length - received);
      is<read_error> (rcv, error_msg);

      // Increment the received amount with the number of bytes just received
      received += rcv;
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
write_unix (int fd, D *src, size_t length, const std::string &error_msg)
{
  size_t sent = 0; // Number of bytes that we have already sent
  ssize_t wr = 0;  // Response of the write(2). Number of bytes written

  while (sent != length)
    {
      // Send what is left to send with an offset
      wr = write (fd, src + sent, length - sent);
      is<write_error> (wr, error_msg);

      // Increment the sent amount with the number of bytes just transferred
      sent += wr;
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
dxp_socket::dxp_socket ()
{
  struct sockaddr_un sock_name = {};
  sock_name.sun_family = AF_UNIX;

  // Safely putting socket name into a buffer
  std::strncpy (sock_name.sun_path, k_socket_path,
                std::strlen (k_socket_path) + 1);

  // Trick compiler into thinking that we pass pointer to sockaddr struct
  // https://stackoverflow.com/questions/21099041
  auto *sock_addr = reinterpret_cast<struct sockaddr *> (&sock_name);

  // XXX Make socket abstract
  // sock_name.sun_path[0] = '\0';

  int s = 0; ///< Return status of functions to check for errors

  // Create a socket file descriptor
  this->fd = socket (AF_UNIX, SOCK_STREAM, 0);
  is<socket_error> (this->fd, "Failed to create a socket file descriptor");

  // Connect to named socket
  s = connect (this->fd, sock_addr, sizeof (sock_name));
  if (s == -1)
    {
      // This should be run only for the initial connection,
      // (daemon startup) as socket may not exist then.

      unlink (k_socket_path); // Remove existing socket

      s = bind (this->fd, sock_addr, sizeof (sock_name)); // Bind name to fd
      s = listen (this->fd, 2);                           /* 2 is arbitrary */
      is<bind_error> (s, "Failed to bind a name to the socket");
    }
};

dxp_socket::~dxp_socket () { close (this->fd); };

/**
 * Request and receive socket_pixmaps from daemon
 */
std::vector<dxp_socket_desktop>
dxp_socket::get_desktops () const
{
  // Send pixmap request to daemon
  char cmd = kRequestDesktops;
  write_unix (this->fd, &cmd, 1,
              "Failed to send a desktops request to the daemon. "
              "Please check if the daemon is running");

  /// Pixmap array that will be returned
  std::vector<dxp_socket_desktop> pixmap_array;

  size_t num = 0; ///< Amount of desktops that will be received
  read_unix (this->fd, &num, sizeof (num),
             "Failed to get number of desktops from the daemon");

  for (; num > 0; num--) // Read `num` desktops from socket
    {
      dxp_socket_desktop p; ///< Desktop struct for read data

      // Reading all desktop data except raw pixmap
      read_unix (this->fd, &p, offsetof (dxp_socket_desktop, pixmap),
                 "Failed to get desktop data from the daemon");

      // Allocating space for an incoming raw pixmap
      p.pixmap.resize (p.pixmap_len);

      // Reading raw pixmap
      read_unix (this->fd, p.pixmap.data (), size_t (p.pixmap_len),
                 "Failed to get raw desktop pixmap from the daemon");

      pixmap_array.push_back (p);
    }
  return pixmap_array; // Compiler is smart so vector won't be copied here
};

/**
 * Starts an infinite loop that listens for the kRequestDesktops write
 * and sends desktops one by one in return
 */
void
dxp_socket::send_desktops_on_event (
    const std::vector<dxp_socket_desktop> &desktops,
    std::mutex &desktops_lock) const
{
  int data_fd = 0; // Socket file descriptor

  // SOCK_CLOEXEC is because of https://stackoverflow.com/questions/22304631
  while ((data_fd = accept4 (this->fd, nullptr, nullptr, SOCK_CLOEXEC))
         && this->running)
    {
      // Check the incoming command
      char cmd = -1;
      read_unix (data_fd, &cmd, 1, "Failed to get a command from dxp");

      if (cmd == kRequestDesktops)
        {
          // Lock desktops to prevent race condition when reading and writing
          // desktops to socket
          std::scoped_lock<std::mutex> guard (desktops_lock);

          // First write -- number of desktops to be sent
          size_t num = desktops.size ();
          write_unix (data_fd, &num, sizeof (num),
                      "Failed to send number of desktops to dxp");

          // Writes from second to `num+1` * 2 -- sending desktops
          for (const auto &p : desktops)
            {
              // Sending everything except raw pixmap
              write_unix (data_fd, &p, offsetof (dxp_socket_desktop, pixmap),
                          "Failed to send desktop data to dxp");

              // Sending pixmap
              write_unix (data_fd, p.pixmap.data (), p.pixmap_len,
                          "Failed to send raw desktop pixmap to dxp");
            }
        }
    }
};
