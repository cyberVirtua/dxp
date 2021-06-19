#include "config.hpp"
#include "desktop_pixmap.hpp"
#include "dexpo_socket.hpp"
#include "window.hpp"
#include <unistd.h>
#include <vector>

desktop_pixmap d0 (0, 0, 1080, 1920, "");

int
main ()
{
  // Gets width, height and screenshot of every desktop
  dexpo_socket client;
  auto v = client.get_pixmaps ();

  // Calculates size of GUI's window
  auto conf_width = dexpo_width;   // Width from config
  auto conf_height = dexpo_height; // Height from config
  if (conf_width == 0)
    {
      conf_width += dexpo_padding; // Add the padding before first screenshot
      conf_height
          += 2 * dexpo_padding; // Add the paddng at both sizes of interface
      for (const auto &dexpo_pixmap : v)
        {
          conf_width += dexpo_pixmap->width;
          conf_width += dexpo_padding;
        }
    }
  else if (conf_height == 0)
    {
      conf_height += dexpo_padding; // Add the padding before first screenshot
      conf_width
          += 2 * dexpo_padding; // Add the paddng at both sizes of interface
      for (const auto &dexpo_pixmap : v)
        {
          conf_height += dexpo_pixmap->height;
          conf_height += dexpo_padding;
        }
    };

  window w (dexpo_x, dexpo_y, conf_width, conf_height);
  w.pixmaps = v;
  // Mapping pixmap onto window
  while (1)
    {
      usleep (10000);

      if (dexpo_width == 0)
        {
          // Drawing screenshots starting from the left top corner
          auto act_width = dexpo_padding;
          for (const auto &pixmap : w.pixmaps)
            {

              xcb_put_image (desktop_pixmap::c_, XCB_IMAGE_FORMAT_Z_PIXMAP,
                             w.id,                /* Pixmap to put image on */
                             desktop_pixmap::gc_, /* Graphic context */
                             pixmap->width, pixmap->height, /* Dimensions */
                             0, /* Destination X coordinate */
                             0, /* Destination Y coordinate */
                             0, drawable::screen_->root_depth,
                             pixmap->pixmap_len, /* Image size in bytes */
                             pixmap->pixmap);

              // xcb_copy_area (window::c_, dexpo_pixmap->id, w.id, window::gc_,
              // 0,
              //                0, act_width, dexpo_padding,
              //                dexpo_pixmap->width, dexpo_pixmap.height);
              act_width += pixmap->width;
              act_width += dexpo_padding;
            };
        }
      else if (dexpo_height == 0)
        {
          auto act_height = dexpo_padding;
          for (const auto &pixmap : w.pixmaps)
            {
              xcb_put_image (desktop_pixmap::c_, XCB_IMAGE_FORMAT_Z_PIXMAP,
                             w.id,                /* Pixmap to put image on */
                             desktop_pixmap::gc_, /* Graphic context */
                             pixmap->width, pixmap->height, /* Dimensions */
                             0, /* Destination X coordinate */
                             0, /* Destination Y coordinate */
                             0, drawable::screen_->root_depth,
                             pixmap->pixmap_len, /* Image size in bytes */
                             pixmap->pixmap);
              // xcb_copy_area (window::c_, dexpo_pixmap.id, w.id, window::gc_,
              // 0,
              //                0, dexpo_padding, act_height,
              //                dexpo_pixmap.width, dexpo_pixmap.height);
              act_height += pixmap->height;
              act_height += dexpo_padding;
            };
        }
      w.highlight_window (0, dexpo_hlcolor);
      /* We flush the request */
      xcb_flush (window::c_);
    }
}
