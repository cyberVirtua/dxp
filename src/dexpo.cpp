#include "config.hpp"
#include "desktop.hpp"
#include "ewmh.hpp"
#include "socket.hpp"
#include "window.hpp"
#include <unistd.h>
#include <vector>
#include <xcb/xcb.h>
#include <xcb/xcb_keysyms.h>
#include <xcb/xinput.h>

constexpr bool k_horizontal_stacking = (dexpo_width == 0);
constexpr bool k_vertical_stacking = (dexpo_height == 0);

auto window_width = dexpo_width;   // Will be redefined
auto window_height = dexpo_height; // Will be redefined

/**
 * Calculate dimensions of the window based on
 * stacking mode and desktops from daemon
 */
void
set_window_dimensions (std::vector<dxp_socket_desktop> &v)
{
  if (k_horizontal_stacking)
    {
      // Add the padding before first screenshot
      window_width += dexpo_padding;

      // Add the paddng at both sizes of interface
      window_height += 2 * dexpo_padding;

      for (const auto &dexpo_pixmap : v)
        {
          window_width += dexpo_pixmap.width;
          window_width += dexpo_padding;
        }
    }
  else if (k_vertical_stacking)
    {
      // Add the padding before first screenshot
      window_height += dexpo_padding;

      // Add the paddng at both sizes of interface
      window_width += 2 * dexpo_padding;

      for (const auto &dexpo_pixmap : v)
        {
          window_height += dexpo_pixmap.height;
          window_height += dexpo_padding;
        }
    };
};

/**
 *
 */
int
main ()
{
  // Get screenshots from socket
  dxp_socket client;
  auto v = client.get_pixmaps ();

  set_window_dimensions (v);

  window w (dexpo_x, dexpo_y, window_width, window_height);
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
            auto *kp = reinterpret_cast<xcb_key_press_event_t *> (event);
            // right arrow or up arrow
            if (kp->detail == 114 or kp->detail == 116)
              {
                w.highlight_window (w.desktop_sel, dexpo_bgcolor);
                w.desktop_sel += 1;
                w.desktop_sel = w.desktop_sel % int (v.size ());
                w.highlight_window (w.desktop_sel, dexpo_hlcolor);
              }
            // left arrow or down arrow
            if (kp->detail == 113 or kp->detail == 111)
              {
                w.highlight_window (w.desktop_sel, dexpo_bgcolor);
                w.desktop_sel = (w.desktop_sel == 0) ? int (v.size () - 1)
                                                     : w.desktop_sel - 1;
              }
            w.highlight_window (w.desktop_sel, dexpo_hlcolor);

            // escape
            if (kp->detail == 9)
              {
                exit (0);
              }

            // enter
            if (kp->detail == 36)

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
                                 - w.get_desktop_coordinates (dexpo_pixmap.id)
                             > 0)
                        and (mn->event_y
                                 - w.get_desktop_coordinates (dexpo_pixmap.id)
                             < dexpo_pixmap.height))
                      {
                        det = dexpo_pixmap.id;
                        break;
                      }
                  }
                else if (dexpo_width == 0)
                  {
                    if ((mn->event_x
                             - w.get_desktop_coordinates (dexpo_pixmap.id)
                         > 0)
                        and (mn->event_x
                                 - w.get_desktop_coordinates (dexpo_pixmap.id)
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
