#include "config.hpp"
#include "desktop.hpp"
#include "socket.hpp"
#include "window.hpp"
#include "xcb_util.hpp"
#include <unistd.h>
#include <vector>
#include <xcb/xcb.h>
#include <xcb/xcb_keysyms.h>

constexpr bool k_horizontal_stacking = (dexpo_width == 0);
constexpr bool k_vertical_stacking = (dexpo_height == 0);

// Creating duplicates of window dimensions that will represent its real size
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
      window_width += dexpo_padding;
      window_height += 2 * dexpo_padding;

      for (const auto &dexpo_pixmap : v)
        {
          window_width += dexpo_pixmap.width;
          window_width += dexpo_padding;
        }
    }
  else if (k_vertical_stacking)
    {
      window_height += dexpo_padding;
      window_width += 2 * dexpo_padding;

      for (const auto &dexpo_pixmap : v)
        {
          window_height += dexpo_pixmap.height;
          window_height += dexpo_padding;
        }
    };
}

/**
 * 1. Get desktops from daemon
 * 2. Calculate actual window dimensions
 * 3. Create window and map it onto screen
 * 4. Wait for events and handle them
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

  // Handling incoming events
  while ((event = xcb_wait_for_event (window::c_)))
    {
      // TODO(mangalinor): Document code (why ~0x80)
      switch (event->response_type & ~0x80)
        {
        case XCB_EXPOSE:
          {
            w.draw_gui ();
            for (size_t i = 0; i < w.desktops.size (); i++)
              {
                w.draw_border (i, dexpo_desktop_color);
              }
            w.desktop_sel
                = get_current_desktop (window::c_, window::screen_->root);
            w.draw_preselection ();
            break;
          }
        case XCB_KEY_PRESS:
          {
            auto *kp = reinterpret_cast<xcb_key_press_event_t *> (event);

            /// Right arrow or down arrow
            bool next = kp->detail == 114 || kp->detail == 116;
            /// Left arrow or up arrow
            bool prev = kp->detail == 113 || kp->detail == 111;
            bool entr = kp->detail == 36; /// Enter
            bool esc = kp->detail == 9;   /// Escape

            w.clear_preselection ();
            if (next)
              // Pressing right arrow or down arrow
              // Preselects next desktop
              {
                w.desktop_sel++;
                w.desktop_sel %= v.size ();
              }
            if (prev)
              // Pressing left arrow or up arrow
              // Preselects previous by modulus desktop
              {
                w.desktop_sel
                    = w.desktop_sel == 0 ? v.size () - 1 : w.desktop_sel - 1;
              }
            if (entr)
              {
                ewmh_change_desktop (window::c_, window::screen_,
                                     w.desktop_sel);
                // Next line fixes bug that appears while switching to active
                // desktop
                if (w.desktop_sel
                    == get_current_desktop (window::c_, window::screen_->root))
                  {
                    return 0;
                  };
              }
            if (esc)
              {
                _exit (0); // Thread safe exit
              }
            w.draw_preselection ();
            break;
          }
        case XCB_MOTION_NOTIFY: // Cursor motion within window
          {
            auto *cur = reinterpret_cast<xcb_motion_notify_event_t *> (event);
            int d = w.get_hover_desktop (cur->event_x, cur->event_y);

            if (d != -1)
              {
                w.clear_preselection ();
                w.desktop_sel = uint (d);
                w.draw_preselection ();
              }
            break;
          }
        case XCB_BUTTON_PRESS: // mouse click
          {
            w.clear_preselection ();
            ewmh_change_desktop (window::c_, window::screen_, w.desktop_sel);
            // Next line fixes bug that appears while switching to active
            // desktop
            if (w.desktop_sel
                == get_current_desktop (window::c_, window::screen_->root))
              {
                return 0;
              };
            break;
          }
        case XCB_FOCUS_OUT:
          // This happens when desktop is switched or
          // mouse button is pressed in another window
          {
            return 0;
            break;
          }
        }
      xcb_flush (window::c_);
    }
}
