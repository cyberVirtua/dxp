#include "drawtypes/DesktopPreview.hpp"
#include "drawtypes/Window.hpp"
#include <iostream>
#include <unistd.h>
#include <xcb/xcb.h>

int
main ()
{
  xcb_connection_t *connection = xcb_connect (NULL, NULL);
  std::cout << doubleadd (5, 3) << minus (5, 3) << "\n";
  std::cout << doubleadd (1834, 1233) << minus (5, 5) << "\n";
  return 0;
}
