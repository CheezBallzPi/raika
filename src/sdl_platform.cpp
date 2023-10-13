#include "raika.cpp"

#include <SDL2/SDL.h>
#include <SDL2/SDL_vulkan.h>
#include <vulkan/vulkan.h>

#include <glm/vec4.hpp>
#include <glm/mat4x4.hpp>

#include <stdlib.h>
#include <malloc.h>
#include <string.h>
#include <set>

// Constants
static const char *title = "Raika";
static const char* layers[1] = {"VK_LAYER_KHRONOS_validation"};
static const int layerCount = 1;
// static const char* layers[0] = {};
// static const int layerCount = 0;
static const char* instanceExtensions[1] = {
  VK_EXT_DEBUG_UTILS_EXTENSION_NAME
};
static const int instanceExtensionCount = 1;
static const char* deviceExtensions[1] = {
  VK_KHR_SWAPCHAIN_EXTENSION_NAME
};
static const int deviceExtensionCount = 1;

// Globals
static bool running = false;
static SDL_Window *window = NULL;
static VkSurfaceKHR vulkanSurface = NULL;
static VkInstance vulkanInstance = NULL;
static VkDevice vulkanLogicalDevice = NULL;
static VkQueue vulkanGraphicsQueue = NULL;
static VkQueue vulkanPresentQueue = NULL;
static VkDebugUtilsMessengerEXT debugMessenger = NULL;
static VkPhysicalDevice vulkanPhysicalDevice = VK_NULL_HANDLE;
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
static PFN_vkEnumeratePhysicalDevices fnEnumeratePhysicalDevices = NULL;
static PFN_vkGetPhysicalDeviceProperties fnGetPhysicalDeviceProperties = NULL;
static PFN_vkGetPhysicalDeviceFeatures fnGetPhysicalDeviceFeatures = NULL;
static PFN_vkGetPhysicalDeviceQueueFamilyProperties fnGetPhysicalDeviceQueueFamilyProperties = NULL;
static PFN_vkCreateDevice fnCreateDevice = NULL;
static PFN_vkDestroyDevice fnDestroyDevice = NULL;
static PFN_vkGetDeviceQueue fnGetDeviceQueue = NULL;
static PFN_vkDestroySurfaceKHR fnDestroySurfaceKHR = NULL;
static PFN_vkGetPhysicalDeviceSurfaceSupportKHR fnGetPhysicalDeviceSurfaceSupportKHR = NULL;
static PFN_vkEnumerateDeviceExtensionProperties fnEnumerateDeviceExtensionProperties = NULL;
static PFN_vkGetPhysicalDeviceSurfaceCapabilitiesKHR fnGetPhysicalDeviceSurfaceCapabilitiesKHR = NULL;
static PFN_vkGetPhysicalDeviceSurfaceFormatsKHR fnGetPhysicalDeviceSurfaceFormatsKHR = NULL;
static PFN_vkGetPhysicalDeviceSurfacePresentModesKHR fnGetPhysicalDeviceSurfacePresentModesKHR = NULL;

VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
  VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
  VkDebugUtilsMessageTypeFlagsEXT messageTypes,
  const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
  void* pUserData
) {
  SDL_Log("[DBG]: %s\n", pCallbackData->pMessage);
  return VK_FALSE;
}

int init() {
  // Init SDL
  if(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) < 0) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to initialize SDL. Error: %s\n", SDL_GetError());
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
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to initialize window. Error: %s\n", SDL_GetError());
    return -1;
  }

  // Load getInstanceProcAddr so we can load other functions later
  fnGetInstanceProcAddr = (PFN_vkGetInstanceProcAddr) SDL_Vulkan_GetVkGetInstanceProcAddr();
  // Load global functions
  LOAD_VK_FN(NULL, EnumerateInstanceExtensionProperties);
  LOAD_VK_FN(NULL, EnumerateInstanceLayerProperties);
  LOAD_VK_FN(NULL, CreateInstance);

  uint32_t availableInstanceExtensionCount = 0;
  if((res = fnEnumerateInstanceExtensionProperties(NULL, &availableInstanceExtensionCount, NULL)) != VK_SUCCESS) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Error getting extensions: %d\n", res);
    return res;
  };
  SDL_Log("Extension Count: %d\n", availableInstanceExtensionCount);
  VkExtensionProperties* availableInstanceExtensions = (VkExtensionProperties*) malloc(sizeof(VkExtensionProperties) * availableInstanceExtensionCount);
  fnEnumerateInstanceExtensionProperties(NULL, &availableInstanceExtensionCount, availableInstanceExtensions);
  SDL_Log("Extensions written: %d\n", availableInstanceExtensionCount);
  for(int i = 0; i < availableInstanceExtensionCount; i++) {
    SDL_Log("Extension %d: %s\n", i + 1, availableInstanceExtensions[i].extensionName);
  }

  bool instanceExtensionMissing = false;

  for(int i = 0; i < instanceExtensionCount; i++) {
    bool extensionFound = false;
    for(int j = 0; j < availableInstanceExtensionCount; j++) {
      if(strcmp(instanceExtensions[i], availableInstanceExtensions[j].extensionName) == 0) {
        extensionFound = true;
        break;
      }
    }
    if(!extensionFound) {
      SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "Couldn't find extension %s\n", instanceExtensions[i]);
      instanceExtensionMissing = true;
      break;
    }
  }

  // Check extensions needed for SDL
  uint32_t sdlNeededExtensionCount = 0;
  if(SDL_Vulkan_GetInstanceExtensions(window, &sdlNeededExtensionCount, NULL) != SDL_TRUE) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Error getting SDL required extensions\n");
    return -1;
  };
  SDL_Log("SDL Extension Count: %d\n", sdlNeededExtensionCount);
  const char** sdlNeededExtensions = (const char**) malloc(sizeof(const char*) * sdlNeededExtensionCount);
  SDL_Vulkan_GetInstanceExtensions(NULL, &sdlNeededExtensionCount, sdlNeededExtensions);
  SDL_Log("SDL Extensions written: %d\n", sdlNeededExtensionCount);
  for(int i = 0; i < sdlNeededExtensionCount; i++) {
    SDL_Log("SDL Extension %d: %s\n", i + 1, sdlNeededExtensions[i]);
  }

  for(int i = 0; i < sdlNeededExtensionCount; i++) {
    bool extensionFound = false;
    for(int j = 0; j < availableInstanceExtensionCount; j++) {
      if(strcmp(sdlNeededExtensions[i], availableInstanceExtensions[j].extensionName) == 0) {
        extensionFound = true;
        break;
      }
    }
    if(!extensionFound) {
      SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "Couldn't find extension %s\n", sdlNeededExtensions[i]);
      instanceExtensionMissing = true;
      break;
    }
  }

  // Concatenate two extension arrays
  const char** requestedExtensions = (const char**) malloc(sizeof(const char*) * (instanceExtensionCount + sdlNeededExtensionCount));
  for(int i = 0; i < instanceExtensionCount; i++) {
    requestedExtensions[i] = instanceExtensions[i];
  }
  for(int i = 0; i < sdlNeededExtensionCount; i++)  {
    requestedExtensions[i + instanceExtensionCount] = sdlNeededExtensions[i];
  }
  SDL_Log("Requesting Extensions:\n");
  for(int i = 0; i < (instanceExtensionCount + sdlNeededExtensionCount); i++)  {
    SDL_Log("  %s\n", requestedExtensions[i]);
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
  instanceCreateInfo.enabledExtensionCount = (instanceExtensionCount + sdlNeededExtensionCount);
  instanceCreateInfo.ppEnabledExtensionNames = requestedExtensions;
  if(!layerMissing && !instanceExtensionMissing) {
    instanceCreateInfo.enabledLayerCount = layerCount;
    instanceCreateInfo.ppEnabledLayerNames = layers;
    instanceCreateInfo.pNext = (VkDebugUtilsMessengerCreateInfoEXT*) &debugCreateInfo;
  } else {
    SDL_Log("Missing validation layer or extension, skipping debug\n");
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
  LOAD_VK_FN(vulkanInstance, DestroySurfaceKHR);
  LOAD_VK_FN(vulkanInstance, GetPhysicalDeviceSurfaceSupportKHR);
  LOAD_VK_FN(vulkanInstance, EnumerateDeviceExtensionProperties);
  LOAD_VK_FN(vulkanInstance, GetPhysicalDeviceSurfaceCapabilitiesKHR);
  LOAD_VK_FN(vulkanInstance, GetPhysicalDeviceSurfaceFormatsKHR);
  LOAD_VK_FN(vulkanInstance, GetPhysicalDeviceSurfacePresentModesKHR);

  // Create debug messenger
  if(!layerMissing && !instanceExtensionMissing) {
    LOAD_VK_FN(vulkanInstance, CreateDebugUtilsMessengerEXT);
    LOAD_VK_FN(vulkanInstance, DestroyDebugUtilsMessengerEXT);
    if(fnCreateDebugUtilsMessengerEXT(vulkanInstance, &debugCreateInfo, NULL, &debugMessenger) != VK_SUCCESS) {
      SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to make debug messenger\n");
    }
  }

  // Create vulkan surface
  if(SDL_Vulkan_CreateSurface(window, vulkanInstance, &vulkanSurface) != SDL_TRUE) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to initialize Vulkan surface. Error: %s\n", SDL_GetError());
    return -1;
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

  // Check physical device suitability
  for(int i = 0; i < deviceCount; i++) {
    VkPhysicalDeviceProperties deviceProperties;
    VkPhysicalDeviceFeatures deviceFeatures;
    fnGetPhysicalDeviceProperties(devices[i], &deviceProperties);
    fnGetPhysicalDeviceFeatures(devices[i], &deviceFeatures);

    SDL_Log("Checking device: %s\n", deviceProperties.deviceName);

    // Check extensions
    uint32_t availableDeviceExtensionCount = 0;
    if((res = fnEnumerateDeviceExtensionProperties(devices[i], NULL, &availableDeviceExtensionCount, NULL)) != VK_SUCCESS) {
      SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Error getting device extensions: %d\n", res);
      return res;
    };
    SDL_Log("Device extension Count: %d\n", availableDeviceExtensionCount);
    VkExtensionProperties* availableDeviceExtensions = (VkExtensionProperties*) malloc(sizeof(VkExtensionProperties) * availableDeviceExtensionCount);
    fnEnumerateDeviceExtensionProperties(devices[i], NULL, &availableDeviceExtensionCount, availableDeviceExtensions);
    SDL_Log("Device extensions written: %d\n", availableDeviceExtensionCount);
    for(int i = 0; i < availableDeviceExtensionCount; i++) {
      SDL_Log("Device extension %d: %s\n", i + 1, availableDeviceExtensions[i].extensionName);
    }

    bool deviceExtensionMissing = false;
    for(int i = 0; i < deviceExtensionCount; i++) {
      bool extensionFound = false;
      for(int j = 0; j < availableDeviceExtensionCount; j++) {
        if(strcmp(deviceExtensions[i], availableDeviceExtensions[j].extensionName) == 0) {
          extensionFound = true;
          break;
        }
      }
      if(!extensionFound) {
        SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "Couldn't find extension %s\n", deviceExtensions[i]);
        deviceExtensionMissing = true;
        break;
      }
    }
    if(deviceExtensionMissing) {
      SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "Extension missing, skipping...\n");
      continue;
    }

    // Get queue families for the device we found
    uint32_t availableQueueCount = 0;
    fnGetPhysicalDeviceQueueFamilyProperties(devices[i], &availableQueueCount, NULL);
    SDL_Log("Queues count: %d\n", availableQueueCount);
    VkQueueFamilyProperties* availableQueues = (VkQueueFamilyProperties*) malloc(sizeof(VkQueueFamilyProperties) * availableQueueCount);
    fnGetPhysicalDeviceQueueFamilyProperties(devices[i], &availableQueueCount, availableQueues);
    SDL_Log("Queues written: %d\n", availableQueueCount);

    uint32_t graphicsQueueIndex = 0;
    uint32_t presentQueueIndex = 0;

    bool graphicsSupported = false;
    bool presentSupported = false;

    for(int j = 0; j < availableQueueCount; j++) {
      if(availableQueues[j].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
        SDL_Log("Queue %d: Graphics found (Count: %d)\n", j, availableQueues[j].queueCount);
        graphicsQueueIndex = j;
        graphicsSupported = true;
      };
      VkBool32 surfaceSupported;
      if(fnGetPhysicalDeviceSurfaceSupportKHR(devices[i], j, vulkanSurface, &surfaceSupported) != VK_SUCCESS) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to get physical device surface support.\n");
      };
      if(surfaceSupported == VK_TRUE) {
        SDL_Log("Queue %d: Presentation found (Count: %d)\n", j, availableQueues[j].queueCount);
        presentQueueIndex = j; 
        presentSupported = true;
      };
      if(graphicsSupported && presentSupported) {
        // Found needed queues
        break;
      }
    }

    if(graphicsSupported && presentSupported) {
      SDL_Log("Found suitable physical device: %s\n", deviceProperties.deviceName);
      vulkanPhysicalDevice = devices[i];

      // Create logical device from physical queue
      std::set<uint32_t> uniqueQueueFamilyIndex = {graphicsQueueIndex, presentQueueIndex};
      VkDeviceQueueCreateInfo* queueCreateInfos = (VkDeviceQueueCreateInfo*) malloc(sizeof(VkDeviceQueueCreateInfo) * uniqueQueueFamilyIndex.size());
      const float priorities[1] = { 1.0f };

      int i = 0;
      for(uint32_t queueFamilyIndex : uniqueQueueFamilyIndex) {
        queueCreateInfos[i].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueCreateInfos[i].queueFamilyIndex = queueFamilyIndex;
        queueCreateInfos[i].queueCount = 1;
        queueCreateInfos[i].pQueuePriorities = priorities;
        queueCreateInfos[i].pNext = NULL;
        i++;
      }
      
      VkDeviceCreateInfo logicalDeviceCreateInfo = {};
      logicalDeviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
      logicalDeviceCreateInfo.queueCreateInfoCount = uniqueQueueFamilyIndex.size();
      logicalDeviceCreateInfo.pQueueCreateInfos = queueCreateInfos;
      logicalDeviceCreateInfo.enabledExtensionCount = deviceExtensionCount;
      logicalDeviceCreateInfo.ppEnabledExtensionNames = deviceExtensions;
      logicalDeviceCreateInfo.pEnabledFeatures = NULL; // Enable features here if needed later
      logicalDeviceCreateInfo.pNext = NULL;

      if((res = fnCreateDevice(vulkanPhysicalDevice, &logicalDeviceCreateInfo, NULL, &vulkanLogicalDevice)) != VK_SUCCESS) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to initialize logical device: %d\n", res);
      }

      fnGetDeviceQueue(vulkanLogicalDevice, graphicsQueueIndex, 0, &vulkanGraphicsQueue);
      fnGetDeviceQueue(vulkanLogicalDevice, presentQueueIndex, 0, &vulkanPresentQueue);

      // Check swapchain capability
      VkSurfaceCapabilitiesKHR surfaceCapabilities;
      fnGetPhysicalDeviceSurfaceCapabilitiesKHR(vulkanPhysicalDevice, vulkanSurface, &surfaceCapabilities);

      uint32_t formatCount = 0;
      if(fnGetPhysicalDeviceSurfaceFormatsKHR(vulkanPhysicalDevice, vulkanSurface, &formatCount, NULL) != VK_SUCCESS) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Error getting format count: %s\n", SDL_GetError());
        return -1;
      };
      SDL_Log("Format count: %d\n", formatCount);
      VkSurfaceFormatKHR* formats = (VkSurfaceFormatKHR*) malloc(sizeof(VkSurfaceFormatKHR) * formatCount);
      fnGetPhysicalDeviceSurfaceFormatsKHR(vulkanPhysicalDevice, vulkanSurface, &formatCount, formats);
      SDL_Log("Formats written: %d\n", formatCount);

      uint32_t presentModeCount = 0;
      if(fnGetPhysicalDeviceSurfacePresentModesKHR(vulkanPhysicalDevice, vulkanSurface, &presentModeCount, NULL) != VK_SUCCESS) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Error getting present mode count: %s\n", SDL_GetError());
        return -1;
      };
      SDL_Log("Present mode count: %d\n", presentModeCount);
      VkPresentModeKHR* presentModes = (VkPresentModeKHR*) malloc(sizeof(VkPresentModeKHR) * presentModeCount);
      fnGetPhysicalDeviceSurfacePresentModesKHR(vulkanPhysicalDevice, vulkanSurface, &presentModeCount, presentModes);
      SDL_Log("Present modes written: %d\n", presentModeCount);

      if(formatCount > 0 && presentModeCount > 0) {
        SDL_Log("Successfully initialized Vulkan.\n");
        return 0;
      } else {
        SDL_Log("Format and present mode are needed, skipping...\n");
      }
    }

    SDL_Log("Couldn't find needed support, moving on to next...\n");
  }

  if(vulkanPhysicalDevice == VK_NULL_HANDLE) {
    // No suitable device found
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to find suitable physical device.\n");
  }

  return -1;
}

void cleanupVulkan() {
  fnDestroySurfaceKHR(vulkanInstance, vulkanSurface, NULL);
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
  if(init() != 0) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to initialize.\n");
    return -1;
  };

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