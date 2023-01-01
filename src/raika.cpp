#include "raika.h"

static void GameOutputSound(
    sound_buffer *buffer, 
    int sampleCount
) {
    for(int i = 0; i < sampleCount; ++sampleCount) {
        
    }
}

static void RenderGradient(
    offscreen_buffer *buffer, 
    int xoffset, 
    int yoffset
) {
  uint8_t *row = (uint8_t *) buffer->memory;
  for(int y = 0; y < buffer->height; ++y) {
      uint32_t *pixel = (uint32_t *) row;
      for(int x = 0; x < buffer->width; ++x) {
        uint8_t blue = (x + xoffset);
        uint8_t green = (y + yoffset);
        uint8_t red = (xoffset + yoffset);

        *pixel++ = (red << 16) | (green << 8) | (blue);
      }
      row += buffer->pitch;
  }
}

static void GameUpdateAndRender(
    offscreen_buffer *buffer
) {
    int xoffset = 0;
    int yoffset = 0;
    GameOutputSound(&soundBuffer, sampleCount);
    RenderGradient(buffer, xoffset, yoffset);
}
