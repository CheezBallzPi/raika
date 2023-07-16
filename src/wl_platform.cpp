#include "raika.cpp"

#include <stdio.h>
#include <wayland-client-core.h>

// globals
static bool running;

int main(int argc, char *argv[]) {
  struct wl_display *display = wl_display_connect(NULL);
  if(!display) {
    fprintf(stderr, "Failed to connect to Wayland display.\n");
    return 1;
  }

  printf("Success!\n");
  // Cleanup
  wl_display_disconnect(display);
  return 0;
}