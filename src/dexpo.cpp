#include "config.hpp"
#include "desktop_pixmap.hpp"
#include "dexpo_socket.hpp"
#include "window.hpp"
#include <unistd.h>
#include <vector>
desktop_pixmap d1 (0, 0, 1920, 1080, "DP-1");
desktop_pixmap d2 (0, 0, 1920, 1080, "DP-2");
desktop_pixmap d3 (0, 0, 1920, 1080, "DP-3");

// Used for testing purposes
std::vector<dexpo_pixmap>
test_get_vector ()
{
  d1.save_screen ();
  d2.save_screen ();
  d3.save_screen ();

  dexpo_pixmap d1_1{ 0, d1.pixmap_width, d1.pixmap_height, d1.pixmap_id,
                     "DP-1" };
  dexpo_pixmap d2_1{ 1, d2.pixmap_width, d2.pixmap_height, d2.pixmap_id,
                     "DP-2" };
  dexpo_pixmap d3_1{ 2, d3.pixmap_width, d3.pixmap_height, d3.pixmap_id,
                     "DP-3" };
  return std::vector<dexpo_pixmap>{ d1_1, d2_1, d3_1 };
}

int
main ()
{
  // Gets width, height and screenshot of every desktop
  auto v = test_get_vector ();

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
          conf_width += dexpo_pixmap.width;
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
          conf_height += dexpo_pixmap.height;
          conf_height += dexpo_padding;
        }
    };

  window w (dexpo_x, dexpo_y, conf_width, conf_height);
  w.pixmaps = v;
  // Mapping pixmap onto window
  int highlighted = 0;
  xcb_generic_event_t *event;
  while ((event = xcb_wait_for_event (window::c_)))
    {
      switch (event->response_type & ~0x80)
        {
        case XCB_EXPOSE:
          {
            if (dexpo_width == 0)
              {
                // Drawing screenshots starting from the left top corner
                auto act_width = dexpo_padding;
                for (const auto &dexpo_pixmap : v)
                  {
                    xcb_copy_area (window::c_, dexpo_pixmap.id, w.id,
                                   desktop_pixmap::gc_, 0, 0, act_width,
                                   dexpo_padding, dexpo_pixmap.width,
                                   dexpo_pixmap.height);
                    act_width += dexpo_pixmap.width;
                    act_width += dexpo_padding;
                  };
              }
            else if (dexpo_height == 0)
              {
                auto act_height = dexpo_padding;
                for (const auto &dexpo_pixmap : v)
                  {
                    xcb_copy_area (window::c_, dexpo_pixmap.id, w.id,
                                   desktop_pixmap::gc_, 0, 0, dexpo_padding,
                                   act_height, dexpo_pixmap.width,
                                   dexpo_pixmap.height);
                    act_height += dexpo_pixmap.height;
                    act_height += dexpo_padding;
                  };
              }
            w.highlight_window (0, dexpo_hlcolor);
            break;
          }
        case XCB_KEY_PRESS:
          {
            xcb_key_press_event_t *kp
                = reinterpret_cast<xcb_key_press_event_t *> (event);
            ;
            if (kp->detail == 114
                or kp->detail == 116) // right arrow or up arrow
              {
                w.highlight_window (highlighted, dexpo_bgcolor);
                highlighted += 1;
                highlighted = highlighted % int (v.size ());
                w.highlight_window (highlighted, dexpo_hlcolor);
              }
            if (kp->detail == 113
                or kp->detail == 111) // left arrow or down arrow
              {
                w.highlight_window (highlighted, dexpo_bgcolor);
                if (highlighted == 0)
                  {
                    highlighted = v.size ();
                  }
                highlighted -= 1;
              }
            w.highlight_window (highlighted, dexpo_hlcolor);
            if (kp->detail == 9) // escape
              {
                exit (0);
              }
            break;
            if (kp->detail == 36) // enter
              {
                break;
              }
            break;
          }

        case XCB_MOTION_NOTIFY: // pointer motion within window
          {
            xcb_motion_notify_event_t *mn
                = reinterpret_cast<xcb_motion_notify_event_t *> (event);
            int det = (mn->event_y - dexpo_padding)
                      / v[0].height; // haha watch me name it like this again
            w.highlight_window (highlighted, dexpo_bgcolor);
            highlighted = det;
            w.highlight_window (highlighted, dexpo_hlcolor);
            break;
          }
        case XCB_ENTER_NOTIFY: // pointer enters window
          {
            xcb_set_input_focus (w.c_, XCB_INPUT_FOCUS_POINTER_ROOT, w.id,
                                 XCB_TIME_CURRENT_TIME);
            break;
          }
        case XCB_LEAVE_NOTIFY: // pointer leaves window
          {
            w.highlight_window (highlighted, dexpo_bgcolor);
            xcb_set_input_focus (w.c_, XCB_INPUT_FOCUS_POINTER_ROOT,
                                 w.screen_->root, XCB_TIME_CURRENT_TIME);
            break;
          }
        }
      xcb_flush (w.c_);
    }
}