#include "raika.h"

#include <stdint.h>
#include <math.h>
#include <cstring>

#define Pi32 3.1415926535897f

static void GameOutputSound(sound_buffer *buffer) {
  static int currentAudioPos = 0;

  int waveHz = 256;
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
        uint8_t blue = (x + xoffset);
        uint8_t green = (y + yoffset);
        uint8_t red = (xoffset + yoffset);

        *pixel++ = (red << 16) | (green << 8) | (blue);
      }
      row += buffer->pitch;
  }
}

static void GameUpdateAndRender(
    graphics_buffer *graphicsBuffer,
    sound_buffer *soundBuffer
) {
    int xoffset = 0;
    int yoffset = 0;

    GameOutputSound(soundBuffer);
    RenderGradient(graphicsBuffer, xoffset, yoffset);
}
