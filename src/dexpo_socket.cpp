#include "dexpo_socket.hpp"
#include <cstring>
#include <iostream>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>

/**
 * Thrower for custom errors.
 * To make error throwing fit into one line.
 */
template <typename er, typename s>
void
is (s status) // This name looks readable: is<socket_error> (this->fd)
{
  // Check if this is one of the custom errors, because this
  // won't work for functions that return something other than -1 on error
  static_assert (std::is_base_of<std::runtime_error, er>::value);

  if (status == -1) // Most of the C libs return -1 on error
    {
      throw er ();
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
  std::strncpy (sock_name.sun_path, SOCKET_PATH,
                sizeof (sock_name.sun_path) - 1);

  // Trick compiler into thinking that we pass pointer to sockaddr struct
  // https://stackoverflow.com/questions/21099041
  auto *sock_addr = reinterpret_cast<struct sockaddr *> (&sock_name);

  // XXX Make socket abstract
  // sock_name.sun_path[0] = '\0';

  int s; // Return status of functions to check for errors
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
      std::cerr << e.what () << std::endl;

      unlink (SOCKET_PATH); // Remove existing socket

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

dexpo_socket::~dexpo_socket ()
{
  close (this->fd);
  unlink (SOCKET_PATH);
};

/**
 * Request and receive dexpo_pixmaps from daemon
 */
std::vector<dexpo_pixmap>
dexpo_socket::get_pixmaps () const
{
  // Send pixmap request to daemon
  char cmd = kRequestPixmaps;
  auto s = write (this->fd, &cmd, 1);
  is<write_error> (s);

  std::vector<dexpo_pixmap> pixmap_array; // Pixmap array that will be returned
  dexpo_pixmap pixmap;                    // Pixmap to copy data in

  size_t num;
  auto rcv_bytes = read (this->fd, &num, sizeof (num));
  is<read_error> (rcv_bytes);

  for (; num > 0; num--) // Read `num` pixmaps from socket
    {
      rcv_bytes = read (this->fd, &pixmap, sizeof (pixmap));
      is<read_error> (rcv_bytes);
      pixmap_array.push_back (pixmap);
    }
  return pixmap_array; // Compiler is smart so vector won't be copied here
};

/**
 * Starts an infinite loop that listens for the kRequestPixmaps write
 * and sends pixmaps one by one in return
 */
void
dexpo_socket::send_pixmaps_on_event (std::vector<dexpo_pixmap> &pixmap_array)
{
  auto data_fd = accept (this->fd, NULL, NULL); // Anonymous socket
  char cmd = -1;

  this->running = true; // Can later turn off the loop
  while (this->running) // FIXME This loop exit is ugly
    {
      if (read (data_fd, &cmd, 1) == kRequestPixmaps) // Listen for the command
        {
          // First write -- number of subsequent packets
          auto num = pixmap_array.size ();
          auto s = write (data_fd, &num, sizeof (num));
          is<write_error> (s);

          // Writes from second to `num+1` -- subsequent packets
          for (auto &pixmap : pixmap_array)
            {
              s = write (data_fd, &pixmap, sizeof (pixmap));
              is<write_error> (s);
            }
        }
    }
};
