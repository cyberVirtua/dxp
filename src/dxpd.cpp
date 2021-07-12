#include "daemon.hpp" // for dxp_daemon
#include <iostream>   // for operator<<, endl, basic_ostream, cerr, ostream
#include <stdexcept>  // for runtime_error

/**
 * Parse monitor dimensions and initialize appropriate pixmap objects.
 *
 * At a timeout check current display and screenshot it.
 * Start a socket listener.
 *
 * TODO Handle workspace deletions
 * TODO Handle errors
 */
int
main ()
{
  try
    {
      auto d = dxp_daemon ();
      d.run ();
    }
  catch (const std::runtime_error &e)
    {
      std::cerr << e.what () << std::endl;
      return 0;
    }

  return 0;
}
