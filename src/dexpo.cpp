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

// Creating duplicates of window dimensions that will represent real size
auto window_width = dexpo_width;
auto window_height = dexpo_height;

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
 * TODO(mangalinor): Document code
 * (explain what happens in the main step by step)
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
      // TODO(mangalinor): Document code (why ~0x80)
      switch (event->response_type & ~0x80)
        {
        case XCB_EXPOSE:
          {
            w.draw_gui ();
            // w.draw_border (0, dexpo_hlcolor);
            break;
          }
        case XCB_KEY_PRESS:
          {
            auto *kp = reinterpret_cast<xcb_key_press_event_t *> (event);

            /// Right arrow or Up arrow
            bool next = kp->detail == 114 || kp->detail == 116;
            /// Left arrow or down arrow
            bool prev = kp->detail == 113 || kp->detail == 111;
            bool entr = kp->detail == 36; /// Enter
            bool esc = kp->detail == 9;   /// Escape

            w.clear_preselection ();
            if (next) // TODO(mangalinor) Document code
              {
                w.desktop_sel++;
                w.desktop_sel %= v.size ();
              }
            if (prev) // TODO(mangalinor) Document code
              {
                w.desktop_sel
                    = w.desktop_sel == 0 ? v.size () - 1 : w.desktop_sel - 1;
              }
            if (entr)
              {
                ewmh_change_desktop (window::c_, window::screen_,
                                     w.desktop_sel);
              }
            if (esc)
              {
                _exit (0); // Thread safe exit
              }
            w.draw_preselection ();
          }
          break;
        case XCB_MOTION_NOTIFY: // Pointer motion within window
          {
            auto *mn = reinterpret_cast<xcb_motion_notify_event_t *> (event);

            xcb_set_input_focus (window::c_, XCB_INPUT_FOCUS_POINTER_ROOT,
                                 w.xcb_id, XCB_TIME_CURRENT_TIME);
            int det = -1;
            // TODO: Keep coordinates of pixmap in window class instead
            // of this hardcoding
            for (const auto &dexpo_pixmap : w.desktops)
              {
                if (k_vertical_stacking)
                  {
                    // TODO(mangalinor) Document code
                    if ((mn->event_x - dexpo_padding > 0)
                        and (mn->event_x - dexpo_padding < dexpo_pixmap.width)
                        and (mn->event_y - w.get_desktop_coord (dexpo_pixmap.id)
                             > 0)
                        and (mn->event_y - w.get_desktop_coord (dexpo_pixmap.id)
                             < dexpo_pixmap.height))
                      {
                        det = dexpo_pixmap.id;
                        break;
                      }
                  }
                else if (k_horizontal_stacking)
                  {
                    // TODO(mangalinor) Document code
                    if ((mn->event_x - w.get_desktop_coord (dexpo_pixmap.id)
                         > 0)
                        and (mn->event_x - w.get_desktop_coord (dexpo_pixmap.id)
                             < dexpo_pixmap.width)
                        and (mn->event_y - dexpo_padding > 0)
                        and (mn->event_y - dexpo_padding < dexpo_pixmap.height))
                      {
                        det = dexpo_pixmap.id;
                        break;
                      }
                  }
              }
            w.clear_preselection ();
            if (det > -1) // TODO(mangalinor) Document code
              {
                w.desktop_sel = size_t (det);
                w.draw_preselection ();
              }
            break;
          }
        case XCB_ENTER_NOTIFY: // pointer enters window
          {
            // TODO(mangalinor) Document code
            // (why doesn't it happen automatically?)
            xcb_set_input_focus (window::c_, XCB_INPUT_FOCUS_POINTER_ROOT,
                                 w.xcb_id, XCB_TIME_CURRENT_TIME);
            break;
          }
        case XCB_LEAVE_NOTIFY: // pointer leaves window
          // TODO(mangalinor) Document code
          // (what is the difference between XCB_LEAVE_NOTIFY and XCB_FOCUS_OUT)
          {
            // TODO(mangalinor) Document code (why set focus on leave)
            w.clear_preselection ();
            xcb_set_input_focus (window::c_, XCB_INPUT_FOCUS_POINTER_ROOT,
                                 window::screen_->root, XCB_TIME_CURRENT_TIME);
            break;
          }
        case XCB_BUTTON_PRESS: // mouse click
          {
            w.clear_preselection ();
            ewmh_change_desktop (window::c_, window::screen_, w.desktop_sel);
            break;
          }
        case XCB_FOCUS_OUT:
          // TODO(mangalinor) Document code
          // (what is the difference between XCB_LEAVE_NOTIFY and XCB_FOCUS_OUT)
          {
            // TODO(mangalinor) Document code (why set focus on focus out)
            xcb_set_input_focus (window::c_, XCB_INPUT_FOCUS_POINTER_ROOT,
                                 w.xcb_id, XCB_TIME_CURRENT_TIME);
            break;
          }
        }
      xcb_flush (window::c_);
    }
}
