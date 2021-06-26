#include "config.hpp"
#include "desktop.hpp"
#include "ewmh.hpp"
#include "socket.hpp"
#include "window.hpp"
#include <unistd.h>
#include <vector>

// TODO Holy fuck we need to fix this burning shit
dxp_desktop d0 (0, 0, 1080, 1920);

int
main ()
{
  // Gets width, height and screenshot of every desktop
  dxp_socket client;
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
  w.desktops = v;
  // Mapping pixmap onto window
  xcb_generic_event_t *event = nullptr;
  while ((event = xcb_wait_for_event (window::c_)))
    {
      switch (event->response_type & ~0x80)
        {
        case XCB_EXPOSE:
          {
            w.draw_gui ();
            w.highlight_window (0, dexpo_hlcolor);
            break;
          }
        case XCB_KEY_PRESS:
          {
            xcb_key_press_event_t *kp
                = reinterpret_cast<xcb_key_press_event_t *> (event);
            if (kp->detail == 114
                or kp->detail == 116) // right arrow or up arrow
              {
                w.highlight_window (w.desktop_sel, dexpo_bgcolor);
                w.desktop_sel += 1;
                w.desktop_sel = w.desktop_sel % int (v.size ());
                w.highlight_window (w.desktop_sel, dexpo_hlcolor);
              }
            if (kp->detail == 113
                or kp->detail == 111) // left arrow or down arrow
              {
                w.highlight_window (w.desktop_sel, dexpo_bgcolor);
                w.desktop_sel = (w.desktop_sel == 0) ? int (v.size () - 1)
                                                     : w.desktop_sel - 1;
              }
            w.highlight_window (w.desktop_sel, dexpo_hlcolor);
            if (kp->detail == 9) // escape
              {

                exit (0);
              }
            if (kp->detail == 36) // enter

              {
                w.highlight_window (w.desktop_sel, dexpo_bgcolor);
                ewmh_change_desktop (window::c_, window::screen_,
                                     w.desktop_sel);
              }
          }
          break;
        case XCB_MOTION_NOTIFY: // pointer motion within window
          {
            xcb_motion_notify_event_t *mn
                = reinterpret_cast<xcb_motion_notify_event_t *> (event);
            xcb_set_input_focus (window::c_, XCB_INPUT_FOCUS_POINTER_ROOT,
                                 w.xcb_id, XCB_TIME_CURRENT_TIME);
            int det = -1;
            // TODO: Keep coordinates of pixmap in window class instead
            // of this hardcoding
            for (const auto &dexpo_pixmap : w.desktops)
              {
                if (dexpo_height == 0)
                  {
                    if ((mn->event_x - dexpo_padding > 0)
                        and (mn->event_x - dexpo_padding < dexpo_pixmap.width)
                        and (mn->event_y
                                 - w.get_screen_position (dexpo_pixmap.id)
                             > 0)
                        and (mn->event_y
                                 - w.get_screen_position (dexpo_pixmap.id)
                             < dexpo_pixmap.height))
                      {
                        det = dexpo_pixmap.id;
                        break;
                      }
                  }
                else if (dexpo_width == 0)
                  {
                    if ((mn->event_x - w.get_screen_position (dexpo_pixmap.id)
                         > 0)
                        and (mn->event_x
                                 - w.get_screen_position (dexpo_pixmap.id)
                             < dexpo_pixmap.width)
                        and (mn->event_y - dexpo_padding > 0)
                        and (mn->event_y - dexpo_padding < dexpo_pixmap.height))
                      {
                        det = dexpo_pixmap.id;
                        break;
                      }
                  }
              }
            w.highlight_window (w.desktop_sel, dexpo_bgcolor);
            if (det > -1)
              {
                w.desktop_sel = det;
                w.highlight_window (w.desktop_sel, dexpo_hlcolor);
              }
            break;
          }
        case XCB_ENTER_NOTIFY: // pointer enters window
          {
            xcb_set_input_focus (window::c_, XCB_INPUT_FOCUS_POINTER_ROOT,
                                 w.xcb_id, XCB_TIME_CURRENT_TIME);
            break;
          }
        case XCB_LEAVE_NOTIFY: // pointer leaves window
          {
            w.highlight_window (w.desktop_sel, dexpo_bgcolor);
            xcb_set_input_focus (window::c_, XCB_INPUT_FOCUS_POINTER_ROOT,
                                 window::screen_->root, XCB_TIME_CURRENT_TIME);
            break;
          }
        case XCB_BUTTON_PRESS: // mouse click
          {
            w.highlight_window (w.desktop_sel, dexpo_bgcolor);
            ewmh_change_desktop (window::c_, window::screen_, w.desktop_sel);
            break;
          }
        case XCB_FOCUS_OUT:
          {
            xcb_set_input_focus (window::c_, XCB_INPUT_FOCUS_POINTER_ROOT,
                                 w.xcb_id, XCB_TIME_CURRENT_TIME);
            break;
          }
        }
      xcb_flush (window::c_);
    }
}
