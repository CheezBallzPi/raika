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
static VkDevice vulkanLogicalDevice = NULL;
static VkQueue vulkanGraphicsQueue = NULL;
static VkDebugUtilsMessengerEXT debugMessenger = NULL;
static VkPhysicalDevice vulkanPhysicalDevice = VK_NULL_HANDLE;
static VkResult res = VK_SUCCESS;
static uint32_t graphicsQueueIndex = 0;

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
static PFN_vkEnumeratePhysicalDevices fnEnumeratePhysicalDevices = NULL;
static PFN_vkGetPhysicalDeviceProperties fnGetPhysicalDeviceProperties = NULL;
static PFN_vkGetPhysicalDeviceFeatures fnGetPhysicalDeviceFeatures = NULL;
static PFN_vkGetPhysicalDeviceQueueFamilyProperties fnGetPhysicalDeviceQueueFamilyProperties = NULL;
static PFN_vkCreateDevice fnCreateDevice = NULL;
static PFN_vkDestroyDevice fnDestroyDevice = NULL;
static PFN_vkGetDeviceQueue fnGetDeviceQueue = NULL;

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

int initVulkan() {
  // Load getInstanceProcAddr so we can load other functions later
  fnGetInstanceProcAddr = (PFN_vkGetInstanceProcAddr) SDL_Vulkan_GetVkGetInstanceProcAddr();
  // Load global functions
  LOAD_VK_FN(NULL, EnumerateInstanceExtensionProperties);
  LOAD_VK_FN(NULL, EnumerateInstanceLayerProperties);
  LOAD_VK_FN(NULL, CreateInstance);

  uint32_t availableExtensionCount = 0;
  if((res = fnEnumerateInstanceExtensionProperties(NULL, &availableExtensionCount, NULL)) != VK_SUCCESS) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Error getting extensions: %d\n", res);
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
      SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "Couldn't find extension %s\n", extensions[i]);
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
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Error getting layer count: %s\n", SDL_GetError());
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
      SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "Couldn't find layer %s\n", layers[i]);
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
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to make vulkan instance\n");
  };

  // Load functions
  LOAD_VK_FN(vulkanInstance, DestroyInstance);
  LOAD_VK_FN(vulkanInstance, EnumeratePhysicalDevices);
  LOAD_VK_FN(vulkanInstance, GetPhysicalDeviceProperties);
  LOAD_VK_FN(vulkanInstance, GetPhysicalDeviceFeatures);
  LOAD_VK_FN(vulkanInstance, GetPhysicalDeviceQueueFamilyProperties);
  LOAD_VK_FN(vulkanInstance, CreateDevice);
  LOAD_VK_FN(vulkanInstance, GetDeviceQueue);

  // Create debug messenger
  if(!layerMissing && !extensionMissing) {
    LOAD_VK_FN(vulkanInstance, CreateDebugUtilsMessengerEXT);
    LOAD_VK_FN(vulkanInstance, DestroyDebugUtilsMessengerEXT);
    if(fnCreateDebugUtilsMessengerEXT(vulkanInstance, &debugCreateInfo, NULL, &debugMessenger) != VK_SUCCESS) {
      SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to make debug messenger\n");
    }
  }

  // Get physical devices
  uint32_t deviceCount = 0;
  if(fnEnumeratePhysicalDevices(vulkanInstance, &deviceCount, NULL) != VK_SUCCESS) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Error getting device count: %s\n", SDL_GetError());
    return -1;
  };
  SDL_Log("Devices count: %d\n", deviceCount);
  VkPhysicalDevice* devices = (VkPhysicalDevice*) malloc(sizeof(VkPhysicalDevice) * deviceCount);
  fnEnumeratePhysicalDevices(vulkanInstance, &deviceCount, devices);
  SDL_Log("Devices written: %d\n", deviceCount);

  // Check suitability
  for(int i = 0; i < deviceCount; i++) {
    VkPhysicalDeviceProperties deviceProperties;
    VkPhysicalDeviceFeatures deviceFeatures;
    fnGetPhysicalDeviceProperties(devices[i], &deviceProperties);
    fnGetPhysicalDeviceFeatures(devices[i], &deviceFeatures);

    SDL_Log("Checking device: %s\n", deviceProperties.deviceName);
    // Get queue families for the device we found
    uint32_t availableQueueCount = 0;
    fnGetPhysicalDeviceQueueFamilyProperties(devices[i], &availableQueueCount, NULL);
    SDL_Log("Queues count: %d\n", availableQueueCount);
    VkQueueFamilyProperties* availableQueues = (VkQueueFamilyProperties*) malloc(sizeof(VkQueueFamilyProperties) * availableQueueCount);
    fnGetPhysicalDeviceQueueFamilyProperties(devices[i], &availableQueueCount, availableQueues);
    SDL_Log("Queues written: %d\n", availableQueueCount);

    for(int j = 0; j < availableQueueCount; j++) {
      if(availableQueues[j].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
        SDL_Log("Found queue: %d (Count: %d)\n", j, availableQueues[j].queueCount);
        graphicsQueueIndex = j; 

        SDL_Log("Found suitable physical device: %s\n", deviceProperties.deviceName);
        vulkanPhysicalDevice = devices[i];

        // Create logical device from physical queue
        VkDeviceQueueCreateInfo queueCreateInfos[1] = {};
        queueCreateInfos[0].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueCreateInfos[0].queueFamilyIndex = graphicsQueueIndex;
        queueCreateInfos[0].queueCount = 1;
        const float priorities[1] = { 1.0f };
        queueCreateInfos[0].pQueuePriorities = priorities;
        
        VkDeviceCreateInfo logicalDeviceCreateInfo = {};
        logicalDeviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        logicalDeviceCreateInfo.queueCreateInfoCount = 1;
        logicalDeviceCreateInfo.pQueueCreateInfos = queueCreateInfos;
        logicalDeviceCreateInfo.enabledExtensionCount = 0;
        logicalDeviceCreateInfo.ppEnabledExtensionNames = NULL; // Enable extensions here if needed later
        logicalDeviceCreateInfo.pEnabledFeatures = NULL; // Enable features here if needed later

        if((res = fnCreateDevice(vulkanPhysicalDevice, &logicalDeviceCreateInfo, NULL, &vulkanLogicalDevice)) != VK_SUCCESS) {
          SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to initialize logical device: %d\n", res);
        }

        fnGetDeviceQueue(vulkanLogicalDevice, graphicsQueueIndex, 0, &vulkanGraphicsQueue);

        SDL_Log("Successfully initialized Vulkan.\n");
        return 0;
      }
    }
    SDL_Log("Couldn't find queue that supports graphics, moving on to next...\n");
  }

  if(vulkanPhysicalDevice == VK_NULL_HANDLE) {
    // No suitable device found
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to find suitable physical device.\n");
  }
  SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to initialize Vulkan.\n");
  return -1;
}

int initSDL() {
  // Init SDL
  if(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) < 0) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to initialize SDL. Error: %s\n", SDL_GetError());
    return -1;
  }

  // Load vulkan driver
  SDL_Vulkan_LoadLibrary(NULL);
  // Start vulkan
  initVulkan();

  // Create window
  window = SDL_CreateWindow(
    title, 
    SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
    400, 400,
    SDL_WINDOW_VULKAN
  );

  if(window == NULL) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to initialize window. Error: %s\n", SDL_GetError());
    return -1;
  }

  return 0;
}

void cleanupVulkan() {
  fnDestroyDevice(vulkanLogicalDevice, NULL);
  fnDestroyDebugUtilsMessengerEXT(vulkanInstance, debugMessenger, NULL);
  fnDestroyInstance(vulkanInstance, NULL);
}

void cleanupSDL() {
  // Cleanup
  cleanupVulkan();
  SDL_DestroyWindow(window);
  SDL_Quit();
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
          SDL_Log("Quitting...\n");
          running = false;
          break;
        }
        default: {
          SDL_Log("Unhandled Event: %d\n", event.type);
          break;
        }
      }
    }
  }

  cleanupSDL();
  return 0;
}