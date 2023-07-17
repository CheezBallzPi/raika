#include "raika.cpp"

#include <stdio.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <time.h>
#include <errno.h>
#include <unistd.h>
#include <wayland-client.h>
#include "../build/xdg-shell-client-protocol.h"

struct wl_state { // This will hold the stuff we grab from wl_display
  struct wl_compositor *compositor;
  struct wl_shm *shm;
  struct xdg_wm_base *xdgBase;
};

struct wl_buffer_with_mem {
  struct wl_buffer *buffer;
  void *mem;
};

struct wl_doubleBuffer {
  struct wl_buffer_with_mem a;
  struct wl_buffer_with_mem b;
};

// globals
static bool running;
static bool currentA;
static struct wl_state globalState = {0};
static struct wl_doubleBuffer globalDoubleBuffer = {0};
static bool updateSurface;

// Helpers
static wl_buffer_with_mem getBuffer(bool first) {
  if(first == currentA) {
    return globalDoubleBuffer.a;
  } else {
    return globalDoubleBuffer.b;
  }
}

static void randname(char *buf) {
	struct timespec ts;
	clock_gettime(CLOCK_REALTIME, &ts);
	long r = ts.tv_nsec;
	for (int i = 0; i < 6; ++i) {
		buf[i] = 'A'+(r&15)+(r&16)*2;
		r >>= 5;
	}
}

static int create_shm_file() {
  int retries = 100;
  do {
    char name[] = "/wl_shm-XXXXXX";
		randname(name + sizeof(name) - 7);
		--retries;
		int fd = shm_open(name, O_RDWR | O_CREAT | O_EXCL, 0600);
		if (fd >= 0) {
			shm_unlink(name);
			return fd;
		}
	} while (retries > 0 && errno == EEXIST);
  return -1;
}

int allocate_shm_file(size_t size) {
	int fd = create_shm_file();
	if (fd < 0)
		return -1;
	int ret;
	do {
		ret = ftruncate(fd, size);
	} while (ret < 0 && errno == EINTR);
	if (ret < 0) {
		close(fd);
		return -1;
	}
	return fd;
}

// Listeners
// wl_registry
static void registry_handle_global(
  void *data, 
  struct wl_registry *registry, 
  uint32_t name, 
  const char *interface, 
  uint32_t version
) {
	struct wl_state *state = (wl_state*) data;
  if(strcmp(interface, wl_compositor_interface.name) == 0) {
    state->compositor = (wl_compositor*) wl_registry_bind(registry, name, &wl_compositor_interface, version);
  } else if(strcmp(interface, wl_shm_interface.name) == 0) {
    state->shm = (wl_shm*) wl_registry_bind(registry, name, &wl_shm_interface, version);
  } else if(strcmp(interface, xdg_wm_base_interface.name) == 0) {
    state->xdgBase = (xdg_wm_base*) wl_registry_bind(registry, name, &xdg_wm_base_interface, version);
  } 
}

static void registry_handle_global_remove(void *data, struct wl_registry *registry, uint32_t name) {
	// This space deliberately left blank
}

static const struct wl_registry_listener registry_listener = {
	.global = registry_handle_global,
	.global_remove = registry_handle_global_remove,
};

// xdg_wm_base
static void xdgBase_handle_ping(void *data, struct xdg_wm_base *wmBase, uint serial) {
  xdg_wm_base_pong(wmBase, serial);
}

static const struct xdg_wm_base_listener xdgBase_listener = {
  .ping = xdgBase_handle_ping,
};

// xdg_surface
static void xdgSurface_handle_configure(void *data, struct xdg_surface *surface, uint serial) {
  xdg_surface_ack_configure(surface, serial);

  struct wl_surface *wlsurface = (wl_surface*) data;
  struct wl_buffer *currentBuffer = getBuffer(true).buffer;
  wl_surface_attach(wlsurface, currentBuffer, 0, 0);
  wl_surface_damage_buffer(wlsurface, 0, 0, INT32_MAX, INT32_MAX);
  wl_surface_commit(wlsurface);
}

static const struct xdg_surface_listener xdgSurface_listener = {
  .configure = xdgSurface_handle_configure,
};

// wl_callback
static void callback_done(void *data, struct wl_callback *cb, uint callback_data);
static struct wl_callback_listener callback_listener = {
  .done = callback_done,
};

static void callback_done(void *data, struct wl_callback *cb, uint callback_data) {
  struct wl_surface *wlsurface = (wl_surface*) data;
  wl_callback_destroy(cb);
  struct wl_callback *newcb = wl_surface_frame(wlsurface);
  wl_callback_add_listener(newcb, &callback_listener, data);
  
  int width = 1920;
  int height = 1080;
  int bytesPerPixel = 4;

  struct graphics_buffer graphicsBuffer = {0};
  struct sound_buffer soundBuffer = {0};
  struct game_input gameInput = {0};

  graphicsBuffer.memory = getBuffer(false).mem,
  graphicsBuffer.width = width,
  graphicsBuffer.height = height,
  graphicsBuffer.pitch = width * bytesPerPixel;

  GameUpdateAndRender(&graphicsBuffer, &soundBuffer, &gameInput);
  wl_surface_attach(wlsurface, getBuffer(false).buffer, 0, 0);
  wl_surface_damage_buffer(wlsurface, 0, 0, INT32_MAX, INT32_MAX);
  wl_surface_commit(wlsurface);
  currentA = !currentA;
}

// Main

int main(int argc, char *argv[]) {
  struct wl_display *display = wl_display_connect(NULL);
  if(!display) {
    fprintf(stderr, "Failed to connect to Wayland display.\n");
    return 1;
  }
  struct wl_registry *registry = wl_display_get_registry(display);
  wl_registry_add_listener(registry, &registry_listener, &globalState);
  wl_display_roundtrip(display); // Run all the events to bind everything
  xdg_wm_base_add_listener(globalState.xdgBase, &xdgBase_listener, NULL);
  struct wl_surface *surface = wl_compositor_create_surface(globalState.compositor);

  // init memory
  int width = 1920;
  int height = 1080;
  int bytesPerPixel = 4;
  int shmMemSize = bytesPerPixel * width * height * 2; // we want 2 of these
  int fd = allocate_shm_file(shmMemSize);
  void *memory = mmap(NULL, shmMemSize, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);

  struct wl_shm_pool *pool = wl_shm_create_pool(globalState.shm, fd, shmMemSize);
  globalDoubleBuffer.a.buffer = wl_shm_pool_create_buffer(pool, 0, width, height, bytesPerPixel * width, WL_SHM_FORMAT_XRGB8888);
  globalDoubleBuffer.b.buffer = wl_shm_pool_create_buffer(pool, height * width * bytesPerPixel, width, height, bytesPerPixel * width, WL_SHM_FORMAT_XRGB8888);
  globalDoubleBuffer.a.mem = memory;
  globalDoubleBuffer.b.mem = (void*)(((int8_t*) memory) + (bytesPerPixel * width * height));

  struct xdg_surface *xdgSurface = xdg_wm_base_get_xdg_surface(globalState.xdgBase, surface);
  xdg_surface_add_listener(xdgSurface, &xdgSurface_listener, surface);
  struct xdg_toplevel *xdgToplevel = xdg_surface_get_toplevel(xdgSurface);
  xdg_toplevel_set_title(xdgToplevel, "Raika");
  wl_surface_commit(surface);
  struct wl_callback *cb = wl_surface_frame(surface);
  wl_callback_add_listener(cb, &callback_listener, surface);

  running = true;
  updateSurface = true;
  while(running) {
    wl_display_dispatch(display);
    wl_display_flush(display);
  }
  // Cleanup
  wl_display_disconnect(display);
  return 0;
}