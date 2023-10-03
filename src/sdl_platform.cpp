#include "raika.cpp"

#include <SDL2/SDL.h>
#include <SDL2/SDL_vulkan.h>
#include <vulkan/vulkan.h>

#include <glm/vec4.hpp>
#include <glm/mat4x4.hpp>

#include <stdlib.h>
#include <malloc.h>

// Globals
static bool running = false;
static SDL_Window *window = NULL;
static SDL_Surface *windowSurface = NULL;

const char *title = "Raika";

int initSDL() {
  // Init SDL
  if(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) < 0) {
    SDL_Log("Failed to initialize SDL. Error: %s\n", SDL_GetError());
    return -1;
  }

  // Load vulkan driver
  SDL_Vulkan_LoadLibrary(NULL);

  // Create window
  window = SDL_CreateWindow(
    title, 
    SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
    400, 400,
    SDL_WINDOW_VULKAN
  );

  if(window == NULL) {
    SDL_Log("Failed to initialize window. Error: %s\n", SDL_GetError());
    return -1;
  }

  windowSurface = SDL_GetWindowSurface(window);

  if(windowSurface == NULL) {
    SDL_Log("Failed to initialize surface. Error: %s\n", SDL_GetError());
    return -1;
  }
  return 0;
}

int cleanupSDL() {
  // Cleanup
  SDL_DestroyWindow(window);
  SDL_Quit();
  return 0;
}

SDL_Surface* loadImage(char* path) {
  SDL_RWops* file = SDL_RWFromFile(path, "r");
  if(file == NULL) {
    SDL_Log("Failed to initialize file. Error: %s\n", SDL_GetError());
    return NULL;
  }
  SDL_Surface* surface = SDL_LoadBMP_RW(file, 1);
  if(surface == NULL) {
    SDL_Log("Failed to initialize surface from file. Error: %s\n", SDL_GetError());
    return NULL;
  }
  return surface;
}

int main(int argc, char *argv[]) {
  initSDL();
  
  uint32_t extensionCount = 0;
  SDL_Vulkan_GetInstanceExtensions(window, &extensionCount, NULL);
  SDL_Log("Extension Count: %d\n", extensionCount);
  const char** extensions = (const char**) malloc(sizeof(const char*) * extensionCount);
  SDL_Vulkan_GetInstanceExtensions(window, &extensionCount, extensions);
  SDL_Log("Extensions written: %d\n", extensionCount);

  VkApplicationInfo appInfo = {};
  appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
  appInfo.pApplicationName = "Raika";
  appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
  appInfo.pEngineName = "Raika Engine";
  appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
  appInfo.apiVersion = VK_API_VERSION_1_3;

  // SDL_UpdateWindowSurface(window);

  // SDL_Event event;

  // running = true;
  // while(running) {
  //   // Handle events
  //   while(SDL_PollEvent(&event)) {
  //     switch(event.type) {
  //       case SDL_QUIT: {
  //         printf("Quitting...\n");
  //         running = false;
  //         break;
  //       }
  //       default: {
  //         printf("Unhandled Event: %d\n", event.type);
  //         break;
  //       }
  //     }
  //   }
  // }

  cleanupSDL();
  return 0;
}