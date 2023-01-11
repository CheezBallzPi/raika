#if !defined(RAIKA_H)

#define ArrayCount(array) (sizeof(array) / sizeof((array)[0]))
// This includes everything that the platform will provide to the game
// File IO

// This includes everything that the game will provide to the platform.
struct sound_buffer {
    void *memory;
    int samplesPerSecond;
    int samplesRequested;
    int bytesPerSample;
    int channels;
};

struct graphics_buffer {
  void *memory;
  int width;
  int height;
  int pitch;
};

struct player_controller {
  float lStickX;
  float lStickY;
  float rStickX;
  float rStickY;
  bool dpad[4];
  bool buttons[20];
};

struct game_input {
  struct player_controller controllers[4];
};

static void GameUpdateAndRender(
  graphics_buffer *graphicsBuffer, 
  sound_buffer *soundBuffer, 
  game_input *gameInput
);

#define RAIKA_H
#endif