#if !defined(RAIKA_H)
// This includes everything that the platform will provide to the game
// File IO

// This includes everything that the game will provide to the platform.
struct graphics_buffer {
  void *memory;
  int width;
  int height;
  int pitch;
};

static void GameUpdateAndRender(graphics_buffer *buffer);

#define RAIKA_H
#endif