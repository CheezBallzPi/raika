#include "raika.h"

#include <math.h>
#include <cstring>

#define Pi32 3.1415926535897f

static void GameOutputSound(sound_buffer *buffer, int waveHz) {
  static int currentAudioPos = 0;

  int64_t waveAmplitude = (((int64_t) 1) << ((buffer->bytesPerSample) * 8) - 1) / 5; // 20% volume

  char * bufferPointer = (char *) (buffer->memory);
  double sinValue;
  int64_t writeValue;

  int wavePeriod = buffer->samplesPerSecond / waveHz;
  for(int i = 0; i < buffer->samplesRequested; ++i) {
    sinValue = sinf(((float) currentAudioPos / (float) wavePeriod) * 2 * Pi32);
    writeValue = waveAmplitude * sinValue;
    for(int j = 0; j < buffer->channels; j++) {
      memcpy(bufferPointer, &writeValue, buffer->bytesPerSample);
      bufferPointer += buffer->bytesPerSample;
    }

    currentAudioPos++;
    if(currentAudioPos == wavePeriod) {
      currentAudioPos = 0;
    }
  }
}

static void RenderGradient(
    graphics_buffer *buffer, 
    int xoffset, 
    int yoffset
) {
  uint8_t *row = (uint8_t *) buffer->memory;
  for(int y = 0; y < buffer->height; ++y) {
      uint32_t *pixel = (uint32_t *) row;
      for(int x = 0; x < buffer->width; ++x) {
        uint8_t blue = x + xoffset;
        uint8_t green = y + yoffset;
        uint8_t red = 0;

        *pixel++ = (red << 16) | (green << 8) | (blue);
      }
      row += buffer->pitch;
  }
}

static int xoffset = 0;
static int yoffset = 0;

static void GameUpdateAndRender(
    graphics_buffer *graphicsBuffer,
    sound_buffer *soundBuffer,
    game_input *gameInput
) {
    player_controller playerInput = gameInput->keyboard;
    int waveHz = 256 + (playerInput.buttons[0] ? 200 : 0);
    xoffset += playerInput.dpad[0] ? 10 : 0;
    yoffset += playerInput.dpad[1] ? 10 : 0;

    GameOutputSound(soundBuffer, waveHz);
    RenderGradient(graphicsBuffer, xoffset, yoffset);
}
