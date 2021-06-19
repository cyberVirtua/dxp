#include "window.hpp"
#include <iostream>
#include <memory>
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <xcb/xcb.h>
#include <xcb/xproto.h> 

void send_message(xcb_connection_t *c, xcb_screen_t *screen, char *msg,
                    unsigned long data0, unsigned long data1,
                    unsigned long data2, unsigned long data3,
                    unsigned long data4)
{
    xcb_client_message_event_t event;
    xcb_intern_atom_cookie_t atom_cookie;
    xcb_intern_atom_reply_t *atom_reply;
    xcb_atom_t atom_id;

    xcb_map_window(c, screen->root);
    atom_cookie = xcb_intern_atom(c, 0, strlen(msg), msg);
    atom_reply = xcb_intern_atom_reply(c, atom_cookie, nullptr);
    atom_id = atom_reply->atom;

    event.response_type = XCB_CLIENT_MESSAGE;
    event.format = 32;
    event.sequence = 0;
    event.window = screen->root;
    event.type = atom_id;
    event.data.data32[0] = data0; // source: 1=application 2=pager
    event.data.data32[1] = data1; // timestamp
    event.data.data32[2] = data2; // currently active window (none)
    event.data.data32[3] = data3;
    event.data.data32[4] = data4;
    xcb_send_event(c, 0, screen->root,  XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY | XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT, (const char *)&event);
   
    free(atom_reply);
}

void ewmh_change_desktop(xcb_connection_t *c, xcb_screen_t *screen, int destkop_number)
{ 
    send_message(c, screen, "_NET_CURRENT_DESKTOP", destkop_number, 0, 0, 0, 0);
}