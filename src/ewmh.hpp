#ifndef DEXPO_EWMH_HPP
#define DEXPO_EWMH_HPP

#include <xcb/xcb.h>

void ewmh_change_desktop (xcb_connection_t *c, xcb_screen_t *screen,
                          int destkop_number);

#endif /* ifndef DEXPO_EWMH_HPP */
