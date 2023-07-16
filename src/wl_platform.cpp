#include "raika.cpp"

#include <stdio.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <time.h>
#include <errno.h>
#include <unistd.h>
#include <wayland-client.h>

struct wl_state { // This will hold the stuff we grab from wl_display
  struct wl_compositor *compositor;
  struct wl_shm *shm;
  struct xdg_wm_base *xdgBase;
};

struct wl_doubleBuffer {
  struct wl_buffer *a;
  void *memA;
  struct wl_buffer *b;
  void *memB;
  int current;
};

// globals
static bool running;

// Listeners
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
    state->shm = (wl_shm*) wl_registry_bind(registry, name, &xdg_wm_base, version);
  } 
}

static void registry_handle_global_remove(
  void *data, 
  struct wl_registry *registry, 
  uint32_t name
) {
	// This space deliberately left blank
}

static const struct wl_registry_listener registry_listener = {
	registry_handle_global,
	registry_handle_global_remove,
};

// Helpers
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

int main(int argc, char *argv[]) {
  struct wl_state state = {0};
  struct wl_doubleBuffer doubleBuffer = {0};
  struct wl_display *display = wl_display_connect(NULL);
  if(!display) {
    fprintf(stderr, "Failed to connect to Wayland display.\n");
    return 1;
  }
  struct wl_registry *registry = wl_display_get_registry(display);
  wl_registry_add_listener(registry, &registry_listener, &state);
  wl_display_roundtrip(display); // Run all the events to bind everything
  struct wl_surface *surface = wl_compositor_create_surface(state.compositor);

  // init memory
  int width = 1920;
  int height = 1080;
  int bytesPerPixel = 4;
  int shmMemSize = bytesPerPixel * width * height * 2; // we want 2 of these
  int fd = allocate_shm_file(shmMemSize);
  void *memory = mmap(NULL, shmMemSize, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);

  struct wl_shm_pool *pool = wl_shm_create_pool(state.shm, fd, shmMemSize);
  doubleBuffer.a = wl_shm_pool_create_buffer(pool, 0, width, height, bytesPerPixel, WL_SHM_FORMAT_XRGB8888);
  doubleBuffer.b = wl_shm_pool_create_buffer(pool, height * bytesPerPixel, width, height, bytesPerPixel, WL_SHM_FORMAT_XRGB8888);
  doubleBuffer.memA = memory;
  doubleBuffer.memB = (void*)(((int8_t*) memory) + (bytesPerPixel * width * height));

  struct graphics_buffer *graphicsBuffer;
  struct sound_buffer *soundBuffer;
  struct game_input *gameInput;
  graphicsBuffer->memory = doubleBuffer.memA,
  graphicsBuffer->width = width,
  graphicsBuffer->height = height,
  graphicsBuffer->pitch = width * bytesPerPixel;

  GameUpdateAndRender(graphicsBuffer, soundBuffer, gameInput);

  wl_surface_attach(surface, doubleBuffer.a, 0, 0);
  wl_surface_damage(surface, 0, 0, width, height);
  wl_surface_commit(surface);

  printf("Success!\n");
  // Cleanup
  wl_display_disconnect(display);
  return 0;
}