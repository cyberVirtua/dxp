#include "Window.hpp"
#include <unistd.h>
#include "desktop_pixmap.hpp"
#include <vector>
#include "desktop_pixmap.hpp"
#include "dexpo_socket.hpp"
#include "config.hpp"

//Used for testing purposes
std::vector <dexpo_pixmap> test_get_vector() 
{
  desktop_pixmap d1 (0, 0, 1920, 1080, "DP-1");
  desktop_pixmap d2 (0, 0, 1920, 1080, "DP-2");
  desktop_pixmap d3 (0, 0, 1920, 1080, "DP-3");

  d1.save_screen ();
  d2.save_screen ();
  d3.save_screen ();

  dexpo_pixmap d1_1 {0, 200, 113, d1.pixmap_id, "DP-1"};
  dexpo_pixmap d2_1 {1, 200, 113, d2.pixmap_id, "DP-2"};
  dexpo_pixmap d3_1 {2, 200, 113, d3.pixmap_id, "DP-3"};
  return (std::vector <dexpo_pixmap> {d1_1, d2_1, d3_1});
};

int main () 
{
  // Gets width, height and screenshot of every desktop
  auto v = test_get_vector();

  // Calculates size of GUI's window
  auto conf_width = k_dexpo_width;    // variable for width based on configuration
  auto conf_height = k_dexpo_height;  // variable for height based on configuration
  if (conf_width == 0) {
    conf_width += k_dexpo_padding; // Add the padding before first screenshot
    for (const auto &dexpo_pixmap : v)
            {
              conf_width += dexpo_pixmap.width;
              conf_width += k_dexpo_padding;
            }
  }
  else if (conf_height == 0) {
    conf_height += k_dexpo_padding;
    for (const auto &dexpo_pixmap : v)
            {
              conf_height += dexpo_pixmap.height;
              conf_height += k_dexpo_padding;
            }
  };
  Window parent(0, 0, conf_width, conf_height, "Win_1", XCB_NONE);
  
  // Mapping pixmap onto window
  while (1)
    {
      usleep(100000);
      if (k_dexpo_width == 0) {
        auto act_width = k_dexpo_padding;
        for (const auto &dexpo_pixmap : v) 
        {
          xcb_copy_area (parent.c_, dexpo_pixmap.id, parent.win_id, desktop_pixmap::gc_, 0, 0, act_width, 0,
                     dexpo_pixmap.width, dexpo_pixmap.height);
          act_width += dexpo_pixmap.width;
          act_width += k_dexpo_padding;
        }; 
      }
      else if (k_dexpo_height == 0) {
        auto act_height = k_dexpo_padding;
        for (const auto &dexpo_pixmap : v) 
        {
          xcb_copy_area (parent.c_, dexpo_pixmap.id, parent.win_id, desktop_pixmap::gc_, 0, 0, 0, act_height,
                     dexpo_pixmap.width, dexpo_pixmap.height);
          act_height += dexpo_pixmap.height;
          act_height += k_dexpo_padding;
        };
      }
      
      /* We flush the request */
      xcb_flush (parent.c_);
    }
};

