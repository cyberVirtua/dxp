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
 *
 * TODO Put this inside window class
 */
void
set_window_dimensions (std::vector<dxp_socket_desktop> &v)
{
  if (k_horizontal_stacking)
    {
      window_width += dexpo_padding + dexpo_border_pres_width;
      window_height += 2 * dexpo_padding + 2 * dexpo_border_pres_width;

      for (const auto &dexpo_pixmap : v)
        {
          window_width += dexpo_pixmap.width;
          window_width += dexpo_padding + 2 * dexpo_border_pres_width;
        }
    }
  else if (k_vertical_stacking)
    {
      window_width += 2 * dexpo_padding + 2 * dexpo_border_pres_width;
      window_height += dexpo_padding + dexpo_border_pres_width;

      for (const auto &dexpo_pixmap : v)
        {
          window_height += dexpo_pixmap.height;
          window_height += dexpo_padding + 2 * dexpo_border_pres_width;
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
  auto *c = window::c_;              ///< window::c_ alias
  auto root = window::screen_->root; ///< window::screen_->root alias

  // Mapping pixmap onto window
  xcb_generic_event_t *event = nullptr;

  // Handling incoming events
  while ((event = xcb_wait_for_event (c)))
    {
      // TODO(mangalinor): Document code (why ~0x80)
      switch (event->response_type & ~0x80)
        {
        case XCB_EXPOSE:
          {
            w.draw_desktops ();
            w.pres = get_current_desktop (c, root);

            // Drawing borders around all desktops
            for (uint i = 0; i < w.desktops.size (); i++)
              {
                i == w.pres ? w.draw_preselection ()
                            : w.draw_desktop_border (i, dexpo_border_nopres);
              }
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
              {
                w.pres++;
                w.pres %= v.size ();
              }
            if (prev) // Decrementing preselected deskot and taking modulus
              {
                w.pres = w.pres == 0 ? v.size () - 1 : w.pres - 1;
              }
            if (entr)
              {
                // Desktop change is thought as an event after which the
                // user doesn't need dexpo any more
                ewmh_change_desktop (c, window::screen_, w.pres);
                xcb_flush (c);
                return 0;
              }
            if (esc)
              {
                return 0;
              }
            w.draw_preselection ();
            break;
          }
        case XCB_MOTION_NOTIFY: // Cursor motion within window
          {
            auto *cur = reinterpret_cast<xcb_motion_notify_event_t *> (event);
            auto d = w.get_hover_desktop (cur->event_x, cur->event_y);

            if (d != -1U)
              {
                w.clear_preselection ();
                w.pres = d;
                w.draw_preselection ();
              }
            break;
          }
        case XCB_BUTTON_PRESS: // Mouse click
          {
            // Desktop change is thought as an event after which the
            // user doesn't need dexpo any more
            ewmh_change_desktop (c, window::screen_, w.pres);
            xcb_flush (c);
            return 0;
          }
        case XCB_FOCUS_OUT:
          // Click otside of window is thought as an event after which the
          // user doesn't need dexpo any more
          {
            return 0;
            break;
          }
        }
      xcb_flush (c);
    }
}
