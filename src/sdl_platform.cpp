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
static VkDebugUtilsMessengerEXT debugMessenger = NULL;
static VkResult res = VK_SUCCESS;

// Function load macro
#define LOAD_VK_FN(INSTANCE, NAME) do { \
    fn ## NAME = (PFN_vk ## NAME) fnGetInstanceProcAddr(INSTANCE, "vk" #NAME); \
    if(!fn ## NAME) SDL_Log("Failed to load %s\n", #NAME); \
  } while(0)

// Functions
static PFN_vkGetInstanceProcAddr fnGetInstanceProcAddr = NULL;
static PFN_vkEnumerateInstanceExtensionProperties fnEnumerateInstanceExtensionProperties = NULL;
static PFN_vkEnumerateInstanceLayerProperties fnEnumerateInstanceLayerProperties = NULL;
static PFN_vkCreateInstance fnCreateInstance = NULL;
static PFN_vkDestroyInstance fnDestroyInstance = NULL;
static PFN_vkCreateDebugUtilsMessengerEXT fnCreateDebugUtilsMessengerEXT = NULL;
static PFN_vkDestroyDebugUtilsMessengerEXT fnDestroyDebugUtilsMessengerEXT = NULL;

static const char *title = "Raika";
static const char* layers[1] = {"VK_LAYER_KHRONOS_validation"};
static const int layerCount = 1;
static const char* extensions[1] = {"VK_EXT_debug_utils"};
static const int extensionCount = 1;

VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
  VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
  VkDebugUtilsMessageTypeFlagsEXT messageTypes,
  const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
  void* pUserData
) {
  SDL_Log("[DBG]: %s\n", pCallbackData->pMessage);
  return VK_FALSE;
}

int initSDL() {
  // Init SDL
  if(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) < 0) {
    SDL_Log("Failed to initialize SDL. Error: %s\n", SDL_GetError());
    return -1;
  }

  // Load vulkan driver
  SDL_Vulkan_LoadLibrary(NULL);

  // Load getInstanceProcAddr so we can load other functions later
  fnGetInstanceProcAddr = (PFN_vkGetInstanceProcAddr) SDL_Vulkan_GetVkGetInstanceProcAddr();
  // Load global functions
  LOAD_VK_FN(NULL, EnumerateInstanceExtensionProperties);
  LOAD_VK_FN(NULL, EnumerateInstanceLayerProperties);
  LOAD_VK_FN(NULL, CreateInstance);

  uint32_t availableExtensionCount = 0;
  if((res = fnEnumerateInstanceExtensionProperties(NULL, &availableExtensionCount, NULL)) != VK_SUCCESS) {
    SDL_Log("Error getting extensions: %d\n", res);
    return res;
  };
  SDL_Log("Extension Count: %d\n", availableExtensionCount);
  VkExtensionProperties* availableExtensions = (VkExtensionProperties*) malloc(sizeof(VkExtensionProperties) * availableExtensionCount);
  fnEnumerateInstanceExtensionProperties(NULL, &availableExtensionCount, availableExtensions);
  SDL_Log("Extensions written: %d\n", availableExtensionCount);
  for(int i = 0; i < availableExtensionCount; i++) {
    SDL_Log("Extension %d: %s\n", i + 1, availableExtensions[i]);
  }

  bool extensionMissing = false;

  for(int i = 0; i < extensionCount; i++) {
    bool extensionFound = false;
    for(int j = 0; j < availableExtensionCount; j++) {
      if(strcmp(extensions[i], availableExtensions[j].extensionName) == 0) {
        extensionFound = true;
        break;
      }
    }
    if(!extensionFound) {
      SDL_Log("Couldn't find extension %s\n", extensions[i]);
      extensionMissing = true;
      break;
    }
  }

  VkApplicationInfo appInfo = {};
  appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
  appInfo.pApplicationName = "Raika";
  appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
  appInfo.pEngineName = "Raika Engine";
  appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
  appInfo.apiVersion = VK_API_VERSION_1_3;

  uint32_t availableLayerCount;
  if(fnEnumerateInstanceLayerProperties(&availableLayerCount, NULL) != VK_SUCCESS) {
    SDL_Log("Error getting layer count: %s\n", SDL_GetError());
    return -1;
  };
  SDL_Log("Layers count: %d\n", availableLayerCount);
  VkLayerProperties* availableLayers = (VkLayerProperties*) malloc(sizeof(VkLayerProperties) * availableLayerCount);
  fnEnumerateInstanceLayerProperties(&availableLayerCount, availableLayers);
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
      break;
    }
  }
    
  VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo = {};
    
  // This is linked to the instance createInfo to enable debug for the instance creation
  debugCreateInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
  debugCreateInfo.messageSeverity = // All messages
    VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT |
    VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
    VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT |
    VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT;
  debugCreateInfo.messageType = 
    VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
    VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
    VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
  debugCreateInfo.pfnUserCallback = debugCallback;

  VkInstanceCreateInfo instanceCreateInfo = {};
  instanceCreateInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
  instanceCreateInfo.pApplicationInfo = &appInfo;
  instanceCreateInfo.enabledExtensionCount = extensionCount;
  instanceCreateInfo.ppEnabledExtensionNames = extensions;
  if(!layerMissing && !extensionMissing) {
    instanceCreateInfo.enabledLayerCount = layerCount;
    instanceCreateInfo.ppEnabledLayerNames = layers;
    instanceCreateInfo.pNext = (VkDebugUtilsMessengerCreateInfoEXT*) &debugCreateInfo;
  } else {
    SDL_Log("Missing validation layer, skipping debug\n");
    instanceCreateInfo.enabledLayerCount = 0;
    instanceCreateInfo.ppEnabledLayerNames = NULL;
  }

  if(fnCreateInstance(&instanceCreateInfo, nullptr, &vulkanInstance) != VK_SUCCESS) {
    SDL_Log("Failed to make vulkan instance\n");
  };

  // Load functions
  LOAD_VK_FN(vulkanInstance, DestroyInstance);

  if(!layerMissing && !extensionMissing) {
    LOAD_VK_FN(vulkanInstance, CreateDebugUtilsMessengerEXT);
    LOAD_VK_FN(vulkanInstance, DestroyDebugUtilsMessengerEXT);
    if(fnCreateDebugUtilsMessengerEXT(vulkanInstance, &debugCreateInfo, NULL, &debugMessenger) != VK_SUCCESS) {
      SDL_Log("Failed to make debug messenger\n");
    }
  }


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

  return 0;
}

int cleanupSDL() {
  // Cleanup
  fnDestroyDebugUtilsMessengerEXT(vulkanInstance, debugMessenger, NULL);
  fnDestroyInstance(vulkanInstance, NULL);
  SDL_DestroyWindow(window);
  SDL_Quit();
  return 0;
}

int main(int argc, char *argv[]) {
  initSDL();

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