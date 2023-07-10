#include "raika.cpp"

#include <stdio.h>
#include <xcb/xcb.h>

// globals
static bool running;
static xcb_intern_atom_cookie_t protocols_cookie;
static xcb_intern_atom_reply_t* protocols_reply;
static xcb_intern_atom_cookie_t delwin_cookie;
static xcb_intern_atom_reply_t* delwin_reply;

void handleEvent(xcb_generic_event_t* event) {
  if(event == NULL) return; // pass on no event
  switch(event->response_type & ~0x80) {
    case XCB_CLIENT_MESSAGE: {
      xcb_client_message_event_t * e = (xcb_client_message_event_t *)event;
      printf("XCB_CLIENT_MESSAGE\n");
      if((*e).data.data32[0] == (*delwin_reply).atom) {
        // Delete window
        running = false;
      }
      break;
    }
    case XCB_EXPOSE: {
      xcb_expose_event_t * e = (xcb_expose_event_t *)event;
      printf("XCB_EXPOSE\n");
      break;
    }
    case XCB_KEY_PRESS: {
      xcb_key_press_event_t * e = (xcb_key_press_event_t *)event;
      printf("XCB_KEY_PRESS\n");
      break;
    }
    case XCB_KEY_RELEASE: {
      xcb_key_release_event_t * e = (xcb_key_release_event_t *)event;
      printf("XCB_KEY_RELEASE\n");
      break;
    }
    case XCB_BUTTON_PRESS: {
      xcb_button_press_event_t * e = (xcb_button_press_event_t *)event;
      printf("XCB_BUTTON_PRESS\n");
      break;
    }
    case XCB_BUTTON_RELEASE: {
      xcb_button_release_event_t * e = (xcb_button_release_event_t *)event;
      printf("XCB_BUTTON_RELEASE\n");
      break;
    }
    default: {
      printf("Unknown event: %d\n", event->response_type);
      break;
    }
  }
}

int main() {
  // Connect to the X server
  xcb_connection_t * connection = xcb_connect(NULL, NULL);

  // Get first screen
  const xcb_setup_t * setup = xcb_get_setup(connection);
  xcb_screen_iterator_t iter = xcb_setup_roots_iterator(setup);
  xcb_screen_t * screen = iter.data;

  // Set masks we want
  uint32_t mask = XCB_CW_EVENT_MASK;
  uint32_t valwin[1] = {
    // Events
    XCB_EVENT_MASK_EXPOSURE | XCB_EVENT_MASK_KEY_PRESS | XCB_EVENT_MASK_KEY_RELEASE | 
    XCB_EVENT_MASK_BUTTON_PRESS | XCB_EVENT_MASK_BUTTON_RELEASE
  };

  // Make window
  xcb_window_t window = xcb_generate_id(connection);
  xcb_create_window(
    connection,
    XCB_COPY_FROM_PARENT,
    window,
    screen->root,
    0, 0, // x, y
    100, 100, // w, h
    0,
    XCB_WINDOW_CLASS_INPUT_OUTPUT,
    screen->root_visual,
    mask, valwin
  );

  // Setup some atoms
  protocols_cookie = xcb_intern_atom(connection, 1, 12, "WM_PROTOCOLS");
  protocols_reply = xcb_intern_atom_reply(connection, protocols_cookie, 0);
  delwin_cookie = xcb_intern_atom(connection, 0, 16, "WM_DELETE_WINDOW");
  delwin_reply = xcb_intern_atom_reply(connection, delwin_cookie, 0);

  // Put delete window into protocols
  xcb_change_property(
    connection, 
    XCB_PROP_MODE_REPLACE, 
    window, 
    (*protocols_reply).atom, 
    4, // Supposed to be Type. One day find out what this is or it will drive you mad.
    32, 
    1, 
    &(*delwin_reply).atom
  );


  // Map window
  xcb_map_window(connection, window);
  xcb_flush (connection);

  // Let's start
  running = true;
  xcb_generic_event_t *event;
  while(running) {
    event = xcb_poll_for_event(connection);
    handleEvent(event);
  }

  // Cleanup
  free(event);

  return 0;
}