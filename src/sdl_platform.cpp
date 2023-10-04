#include "raika.cpp"

#include <SDL2/SDL.h>
#include <SDL2/SDL_vulkan.h>
#include <vulkan/vulkan.h>

#include <glm/vec4.hpp>
#include <glm/mat4x4.hpp>

#include <stdlib.h>
#include <malloc.h>
#include <string.h>

// Globals
static bool running = false;
static SDL_Window *window = NULL;
static SDL_Surface *windowSurface = NULL;
static VkInstance vulkanInstance = NULL;

static const char *title = "Raika";
static const char* layers[1] = {"VK_LAYER_KHRONOS_validation"};
static const int layerCount = 1;
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
}

int cleanupSDL() {
  // Cleanup
  vkDestroyInstance(vulkanInstance, NULL);
  SDL_DestroyWindow(window);
  SDL_Quit();
  return 0;
}

int main(int argc, char *argv[]) {
  initSDL();
    
  uint32_t extensionCount = 0;
  if(SDL_Vulkan_GetInstanceExtensions(window, &extensionCount, NULL) != SDL_TRUE) {
    SDL_Log("Error getting extensions: %s\n", SDL_GetError());
    return -1;
  };
  SDL_Log("Extension Count: %d\n", extensionCount);
  const char** extensions = (const char**) malloc(sizeof(const char*) * extensionCount);
  SDL_Vulkan_GetInstanceExtensions(window, &extensionCount, extensions);
  SDL_Log("Extensions written: %d\n", extensionCount);
  for(int i = 0; i < extensionCount; i++) {
    SDL_Log("Extension %d: %s\n", i + 1, extensions[i]);
  }

  VkApplicationInfo appInfo = {};
  appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
  appInfo.pApplicationName = "Raika";
  appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
  appInfo.pEngineName = "Raika Engine";
  appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
  appInfo.apiVersion = VK_API_VERSION_1_3;

  uint32_t availableLayerCount;
  vkEnumerateInstanceLayerProperties(&availableLayerCount, NULL);
  SDL_Log("Layers count: %d\n", availableLayerCount);
  VkLayerProperties* availableLayers = (VkLayerProperties*) malloc(sizeof(VkLayerProperties) * availableLayerCount);
  vkEnumerateInstanceLayerProperties(&availableLayerCount, availableLayers);
  SDL_Log("Layers written: %d\n", availableLayerCount);
  for(int i = 0; i < availableLayerCount; i++) {
    SDL_Log("Layer %d: %s\n", i + 1, availableLayers[i].layerName);
  }

  bool layerMissing = false;

  for(int i = 0; i < layerCount; i++) {
    bool layerFound = false;
    for(int j = 0; j < availableLayerCount; j++) {
      if(strcmp(layers[i], availableLayers[j].layerName) == 0) {
        layerFound = true;
        break;
      }
    }
    if(!layerFound) {
      SDL_Log("Couldn't find layer %s\n", layers[i]);
      layerMissing = true;
    }
  }

  if(layerMissing) {
    SDL_Log("Missing layer.\n");
    return -1;
  }

  VkInstanceCreateInfo createInfo = {};
  createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
  createInfo.pApplicationInfo = &appInfo;
  createInfo.enabledExtensionCount = extensionCount;
  createInfo.ppEnabledExtensionNames = extensions;
  createInfo.enabledLayerCount = layerCount;
  createInfo.ppEnabledLayerNames = layers;

  if(vkCreateInstance(&createInfo, NULL, &vulkanInstance) != VK_SUCCESS) {
    SDL_Log("Failed to make vulkan instance\n");
  };

  SDL_Event event;

  running = true;
  while(running) {
    // Handle events
    while(SDL_PollEvent(&event)) {
      switch(event.type) {
        case SDL_QUIT: {
          printf("Quitting...\n");
          running = false;
          break;
        }
        default: {
          printf("Unhandled Event: %d\n", event.type);
          break;
        }
      }
    }
  }

  cleanupSDL();
  return 0;
}