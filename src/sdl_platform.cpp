#include "raika.cpp"

#include <SDL2/SDL.h>
#include <SDL2/SDL_vulkan.h>
#include <vulkan/vulkan.h>

#include <glm/vec4.hpp>
#include <glm/mat4x4.hpp>

#include <stdlib.h>
#include <malloc.h>
#include <cstring>
#include <string>
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
static std::string basePath = "";
static SDL_Window *window = NULL;
static VkSurfaceKHR vulkanSurface = NULL;
static VkSwapchainKHR vulkanSwapchain = NULL;
static uint32_t vulkanSwapchainImageCount = 0;
static VkFormat swapchainImageFormat = {};
static VkImage* vulkanSwapchainImages = NULL;
static VkImageView* vulkanImageViews = NULL;
static VkShaderModule vertModule = NULL;
static VkShaderModule fragModule = NULL;
static VkRenderPass vulkanRenderPass = NULL;
static VkPipelineLayout vulkanPipelineLayout = NULL;
static VkPipeline vulkanGraphicsPipeline = NULL;
static VkFramebuffer* vulkanFramebuffers = NULL;
static VkCommandPool vulkanCommandPool = NULL;
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
static PFN_vkCreateSwapchainKHR fnCreateSwapchainKHR = NULL;
static PFN_vkDestroySwapchainKHR fnDestroySwapchainKHR = NULL;
static PFN_vkGetSwapchainImagesKHR fnGetSwapchainImagesKHR = NULL;
static PFN_vkCreateImageView fnCreateImageView = NULL;
static PFN_vkDestroyImageView fnDestroyImageView = NULL;
static PFN_vkCreateShaderModule fnCreateShaderModule = NULL;
static PFN_vkDestroyShaderModule fnDestroyShaderModule = NULL;
static PFN_vkCreatePipelineLayout fnCreatePipelineLayout = NULL;
static PFN_vkDestroyPipelineLayout fnDestroyPipelineLayout = NULL;
static PFN_vkCreateRenderPass fnCreateRenderPass = NULL;
static PFN_vkDestroyRenderPass fnDestroyRenderPass = NULL;
static PFN_vkCreateGraphicsPipelines fnCreateGraphicsPipelines = NULL;
static PFN_vkDestroyPipeline fnDestroyPipeline = NULL;
static PFN_vkCreateFramebuffer fnCreateFramebuffer = NULL;
static PFN_vkDestroyFramebuffer fnDestroyFramebuffer = NULL;
static PFN_vkCreateCommandPool fnCreateCommandPool = NULL;
static PFN_vkDestroyCommandPool fnDestroyCommandPool = NULL;


VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
  VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
  VkDebugUtilsMessageTypeFlagsEXT messageTypes,
  const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
  void* pUserData
) {
  SDL_Log("[DBG]: %s\n", pCallbackData->pMessage);
  return VK_FALSE;
}

VkShaderModule createShaderModule(uint32_t* code, size_t size) {
  VkShaderModuleCreateInfo ci = {};
  ci.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
  ci.codeSize = size;
  ci.pCode = code;
  VkShaderModule out;
  if(fnCreateShaderModule(vulkanLogicalDevice, &ci, NULL, &out) != VK_SUCCESS) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to create shader module\n");
    return NULL;
  }
  return out;
}

int init() {
  // Init SDL
  if(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) < 0) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to initialize SDL. Error: %s\n", SDL_GetError());
    return -1;
  }

  basePath = std::string(SDL_GetBasePath());
  SDL_Log("Base path: %s\n", basePath.c_str());

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
    
  VkDebugUtilsMessengerCreateInfoEXT dci = {};
    
  // This is linked to the instance createInfo to enable debug for the instance creation
  dci.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
  dci.messageSeverity = // All messages
    VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT |
    VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
    VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT |
    VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT;
  dci.messageType = 
    VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
    VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
    VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
  dci.pfnUserCallback = debugCallback;

  VkInstanceCreateInfo ici = {};
  ici.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
  ici.pApplicationInfo = &appInfo;
  ici.enabledExtensionCount = (instanceExtensionCount + sdlNeededExtensionCount);
  ici.ppEnabledExtensionNames = requestedExtensions;
  if(!layerMissing && !instanceExtensionMissing) {
    ici.enabledLayerCount = layerCount;
    ici.ppEnabledLayerNames = layers;
    ici.pNext = (VkDebugUtilsMessengerCreateInfoEXT*) &dci;
  } else {
    SDL_Log("Missing validation layer or extension, skipping debug\n");
    ici.enabledLayerCount = 0;
    ici.ppEnabledLayerNames = NULL;
  }

  if(fnCreateInstance(&ici, nullptr, &vulkanInstance) != VK_SUCCESS) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to make vulkan instance\n");
  };

  // Load functions
  LOAD_VK_FN(vulkanInstance, DestroyInstance);
  LOAD_VK_FN(vulkanInstance, EnumeratePhysicalDevices);
  LOAD_VK_FN(vulkanInstance, GetPhysicalDeviceProperties);
  LOAD_VK_FN(vulkanInstance, GetPhysicalDeviceFeatures);
  LOAD_VK_FN(vulkanInstance, GetPhysicalDeviceQueueFamilyProperties);
  LOAD_VK_FN(vulkanInstance, CreateDevice);
  LOAD_VK_FN(vulkanInstance, DestroyDevice);
  LOAD_VK_FN(vulkanInstance, GetDeviceQueue);
  LOAD_VK_FN(vulkanInstance, DestroySurfaceKHR);
  LOAD_VK_FN(vulkanInstance, GetPhysicalDeviceSurfaceSupportKHR);
  LOAD_VK_FN(vulkanInstance, EnumerateDeviceExtensionProperties);
  LOAD_VK_FN(vulkanInstance, GetPhysicalDeviceSurfaceCapabilitiesKHR);
  LOAD_VK_FN(vulkanInstance, GetPhysicalDeviceSurfaceFormatsKHR);
  LOAD_VK_FN(vulkanInstance, GetPhysicalDeviceSurfacePresentModesKHR);
  LOAD_VK_FN(vulkanInstance, CreateSwapchainKHR);
  LOAD_VK_FN(vulkanInstance, DestroySwapchainKHR);
  LOAD_VK_FN(vulkanInstance, GetSwapchainImagesKHR);
  LOAD_VK_FN(vulkanInstance, CreateImageView);
  LOAD_VK_FN(vulkanInstance, DestroyImageView);
  LOAD_VK_FN(vulkanInstance, CreateShaderModule);
  LOAD_VK_FN(vulkanInstance, DestroyShaderModule);
  LOAD_VK_FN(vulkanInstance, CreatePipelineLayout);
  LOAD_VK_FN(vulkanInstance, DestroyPipelineLayout);
  LOAD_VK_FN(vulkanInstance, CreateRenderPass);
  LOAD_VK_FN(vulkanInstance, DestroyRenderPass);
  LOAD_VK_FN(vulkanInstance, CreateGraphicsPipelines);
  LOAD_VK_FN(vulkanInstance, DestroyPipeline);
  LOAD_VK_FN(vulkanInstance, CreateFramebuffer);
  LOAD_VK_FN(vulkanInstance, DestroyFramebuffer);
  LOAD_VK_FN(vulkanInstance, CreateCommandPool);
  LOAD_VK_FN(vulkanInstance, DestroyCommandPool);

  // Create debug messenger
  if(!layerMissing && !instanceExtensionMissing) {
    LOAD_VK_FN(vulkanInstance, CreateDebugUtilsMessengerEXT);
    LOAD_VK_FN(vulkanInstance, DestroyDebugUtilsMessengerEXT);
    if(fnCreateDebugUtilsMessengerEXT(vulkanInstance, &dci, NULL, &debugMessenger) != VK_SUCCESS) {
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
      VkDeviceQueueCreateInfo* qci = (VkDeviceQueueCreateInfo*) malloc(sizeof(VkDeviceQueueCreateInfo) * uniqueQueueFamilyIndex.size());
      const float priorities[1] = { 1.0f };

      int i = 0;
      for(uint32_t queueFamilyIndex : uniqueQueueFamilyIndex) {
        qci[i].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        qci[i].queueFamilyIndex = queueFamilyIndex;
        qci[i].queueCount = 1;
        qci[i].pQueuePriorities = priorities;
        qci[i].pNext = NULL;
        i++;
      }
      
      VkDeviceCreateInfo ldci = {};
      ldci.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
      ldci.queueCreateInfoCount = uniqueQueueFamilyIndex.size();
      ldci.pQueueCreateInfos = qci;
      ldci.enabledExtensionCount = deviceExtensionCount;
      ldci.ppEnabledExtensionNames = deviceExtensions;
      ldci.pEnabledFeatures = NULL; // Enable features here if needed later
      ldci.pNext = NULL;

      if((res = fnCreateDevice(vulkanPhysicalDevice, &ldci, NULL, &vulkanLogicalDevice)) != VK_SUCCESS) {
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
        // Set details for swapchain
        VkSurfaceFormatKHR vulkanSurfaceFormat = {};
        VkPresentModeKHR vulkanPresentMode = VK_PRESENT_MODE_FIFO_KHR;
        VkExtent2D vulkanSwapExtent = {};

        // Determine surface format
        bool foundFormat = false;
        for(int i = 0; i < formatCount; i++) {
          if(
            formats[i].format == VK_FORMAT_B8G8R8_SRGB &&
            formats[i].colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR
          ) {
            vulkanSurfaceFormat = formats[i];
            foundFormat = true;
          }
        }
        if(!foundFormat) {
          vulkanSurfaceFormat = formats[0]; // Default to first
        }
        // Set format to be used in imageviews
        swapchainImageFormat = vulkanSurfaceFormat.format;

        // Presentation mode
        for(int i = 0; i < presentModeCount; i++) {
          if(presentModes[i] == VK_PRESENT_MODE_MAILBOX_KHR) {
            vulkanPresentMode = presentModes[i];
          }
        }
        
        // Swap extent
        if(surfaceCapabilities.currentExtent.width != UINT32_MAX) {
          vulkanSwapExtent = surfaceCapabilities.currentExtent;
        } else {
          int pxWidth, pxHeight;
          SDL_GetWindowSizeInPixels(window, &pxWidth, &pxHeight);
          vulkanSwapExtent.width = SDL_clamp(
              (uint32_t) pxWidth, 
              surfaceCapabilities.minImageExtent.width,
              surfaceCapabilities.maxImageExtent.width
            );
          vulkanSwapExtent.height = SDL_clamp(
              (uint32_t) pxHeight, 
              surfaceCapabilities.minImageExtent.height,
              surfaceCapabilities.maxImageExtent.height
            );
        }

        // Image count
        if(surfaceCapabilities.maxImageCount == 0) {
          vulkanSwapchainImageCount = surfaceCapabilities.minImageCount + 1;
        } else {
          vulkanSwapchainImageCount = SDL_clamp(
              surfaceCapabilities.minImageCount + 1, 
              0,
              surfaceCapabilities.maxImageCount 
            );
        }

        // Make swapchain
        VkSwapchainCreateInfoKHR scci = {};
        scci.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        scci.pNext = NULL;
        scci.flags = 0;
        scci.surface = vulkanSurface;
        scci.minImageCount = vulkanSwapchainImageCount;
        scci.imageFormat = vulkanSurfaceFormat.format;
        scci.imageColorSpace = vulkanSurfaceFormat.colorSpace;
        scci.imageExtent = vulkanSwapExtent;
        scci.imageArrayLayers = 1;
        scci.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        if(graphicsQueueIndex != presentQueueIndex) {
          scci.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
          scci.queueFamilyIndexCount = 2;
          uint32_t queueFamilyIndices[] = {graphicsQueueIndex, presentQueueIndex};
          scci.pQueueFamilyIndices = queueFamilyIndices;
        } else {
          scci.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        }
        scci.preTransform = surfaceCapabilities.currentTransform;
        scci.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        scci.presentMode = vulkanPresentMode;
        scci.clipped = VK_TRUE;
        scci.oldSwapchain = VK_NULL_HANDLE;

        if(fnCreateSwapchainKHR(vulkanLogicalDevice, &scci, NULL, &vulkanSwapchain) != VK_SUCCESS) {
          SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to initialize swapchain.\n");
          return -1;
        };
        SDL_Log("Successfully initalized swapchain.\n");

        // Get images
        if(fnGetSwapchainImagesKHR(vulkanLogicalDevice, vulkanSwapchain, &vulkanSwapchainImageCount, NULL) != VK_SUCCESS) {
          SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to retrieve swapchain image count.\n");
          return -1;
        };
        SDL_Log("Swapchain image count: %d\n", vulkanSwapchainImageCount);
        vulkanSwapchainImages = (VkImage*) malloc(sizeof(VkImage) * vulkanSwapchainImageCount);
        if(fnGetSwapchainImagesKHR(vulkanLogicalDevice, vulkanSwapchain, &vulkanSwapchainImageCount, vulkanSwapchainImages) != VK_SUCCESS) {
          SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to retrieve swapchain images.\n");
          return -1;
        };

        // Get image views from images
        vulkanImageViews = (VkImageView*) malloc(sizeof(VkImageView) * vulkanSwapchainImageCount);
        for(int i = 0; i < vulkanSwapchainImageCount; i++) {
          VkImageViewCreateInfo ivci = {};
          ivci.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
          ivci.pNext = NULL;
          ivci.flags = 0;
          ivci.image = vulkanSwapchainImages[i];
          ivci.viewType = VK_IMAGE_VIEW_TYPE_2D;
          ivci.format = swapchainImageFormat;
          ivci.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
          ivci.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
          ivci.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
          ivci.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
          ivci.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
          ivci.subresourceRange.baseMipLevel = 0;
          ivci.subresourceRange.levelCount = 1;
          ivci.subresourceRange.baseArrayLayer = 0;
          ivci.subresourceRange.layerCount = 1;

          if(fnCreateImageView(vulkanLogicalDevice, &ivci, NULL, &vulkanImageViews[i]) != VK_SUCCESS) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to create image view.\n");
            return -1;
          };
        }
        SDL_Log("Successfully created image views.\n");

        // Create the graphics pipeline
        // Load shaders
        size_t fragSize, vertSize; 
        uint32_t* shaderFrag = (uint32_t*) SDL_LoadFile((basePath + "/frag.spv").c_str(), &fragSize);
        uint32_t* shaderVert = (uint32_t*) SDL_LoadFile((basePath + "/vert.spv").c_str(), &vertSize);

        SDL_Log("Frag size: %zu\n", fragSize);
        SDL_Log("Vert size: %zu\n", vertSize);

        fragModule = createShaderModule(shaderFrag, fragSize);
        vertModule = createShaderModule(shaderVert, vertSize);

        SDL_free(shaderFrag); 
        SDL_free(shaderVert); 

        if(fragModule != NULL && vertModule != NULL) {
          SDL_Log("Successfully initialized shaders.\n");
          // Shader stages
          VkPipelineShaderStageCreateInfo pssci[2] = {};
          pssci[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
          pssci[0].pNext = NULL;
          pssci[0].flags = 0;
          pssci[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
          pssci[0].module = vertModule;
          pssci[0].pName = "main";
          pssci[0].pSpecializationInfo = NULL;

          pssci[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
          pssci[1].pNext = NULL;
          pssci[1].flags = 0;
          pssci[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
          pssci[1].module = fragModule;
          pssci[1].pName = "main";
          pssci[1].pSpecializationInfo = NULL;

          // Create generic pipeline (consider pulling this whole process out)
          // Dynamic states
          VkDynamicState dynamicStates[2] = { 
            VK_DYNAMIC_STATE_VIEWPORT,
            VK_DYNAMIC_STATE_SCISSOR
          }; // Hardcoded for now 
          VkPipelineDynamicStateCreateInfo pdsci = {};
          pdsci.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
          pdsci.pNext = NULL;
          pdsci.flags = 0;
          pdsci.dynamicStateCount = 2;
          pdsci.pDynamicStates = dynamicStates;

          VkPipelineVertexInputStateCreateInfo pvisci = {};
          pvisci.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
          pvisci.pNext = NULL;
          pvisci.flags = 0;
          pvisci.vertexAttributeDescriptionCount = 0;
          pvisci.pVertexAttributeDescriptions = NULL;
          pvisci.vertexBindingDescriptionCount = 0;
          pvisci.pVertexBindingDescriptions = NULL;

          VkPipelineInputAssemblyStateCreateInfo piasci = {};
          piasci.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
          piasci.pNext = NULL;
          piasci.flags = 0;
          piasci.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
          piasci.primitiveRestartEnable = VK_FALSE;

          VkPipelineViewportStateCreateInfo pvsci = {};
          pvsci.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
          pvsci.pNext = NULL;
          pvsci.flags = 0;
          pvsci.viewportCount = 1;
          pvsci.scissorCount = 1;

          VkPipelineRasterizationStateCreateInfo prsci = {};
          prsci.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
          prsci.pNext = NULL;
          prsci.flags = 0;
          prsci.depthClampEnable = VK_FALSE;
          prsci.rasterizerDiscardEnable = VK_FALSE;
          prsci.polygonMode = VK_POLYGON_MODE_FILL;
          prsci.lineWidth = 1.0f;
          prsci.cullMode = VK_CULL_MODE_BACK_BIT;
          prsci.frontFace = VK_FRONT_FACE_CLOCKWISE;
          prsci.depthBiasEnable = VK_FALSE;

          VkPipelineMultisampleStateCreateInfo pmsci = {};
          pmsci.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
          pmsci.pNext = NULL;
          pmsci.flags = 0;
          pmsci.sampleShadingEnable = VK_FALSE; // disabled for now
          pmsci.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

          VkPipelineColorBlendAttachmentState pcbas = {};
          pcbas.colorWriteMask = 
            VK_COLOR_COMPONENT_R_BIT |
            VK_COLOR_COMPONENT_G_BIT |
            VK_COLOR_COMPONENT_B_BIT |
            VK_COLOR_COMPONENT_A_BIT;
          pcbas.blendEnable = VK_FALSE;
          VkPipelineColorBlendStateCreateInfo pcbsci = {};
          pcbsci.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
          pcbsci.pNext = NULL;
          pcbsci.flags = 0;
          pcbsci.logicOpEnable = VK_FALSE;
          pcbsci.attachmentCount = 1;
          pcbsci.pAttachments = &pcbas; // We only have 1 so the address will suffice 

          VkPipelineLayoutCreateInfo plci = {};
          plci.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
          plci.pNext = NULL;
          plci.flags = 0;

          if(fnCreatePipelineLayout(vulkanLogicalDevice, &plci, NULL, &vulkanPipelineLayout) != VK_SUCCESS) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to make pipeline layout.\n");
            return -1;
          }

          SDL_Log("Successfully initialized pipeline layout.\n");

          // Render pass
          VkAttachmentDescription colorAttachment = {};
          colorAttachment.flags = 0;
          colorAttachment.format = swapchainImageFormat;
          colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
          colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
          colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
          colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
          colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
          colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
          colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

          VkAttachmentReference colorAttachmentRef = {};
          colorAttachmentRef.attachment = 0;
          colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

          VkSubpassDescription subpass = {};
          subpass.flags = 0;
          subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
          subpass.colorAttachmentCount = 1;
          subpass.pColorAttachments = &colorAttachmentRef;

          VkRenderPassCreateInfo rpci = {};
          rpci.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
          rpci.pNext = NULL;
          rpci.flags = 0;
          rpci.attachmentCount = 1;
          rpci.pAttachments = &colorAttachment;
          rpci.subpassCount = 1;
          rpci.pSubpasses = &subpass;

          if(fnCreateRenderPass(vulkanLogicalDevice, &rpci, NULL, &vulkanRenderPass) != VK_SUCCESS) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to make render pass.\n");
            return -1;
          }

          SDL_Log("Successfully initialized render pass.\n");

          // Finally we can make the pipeline
          VkGraphicsPipelineCreateInfo gpci = {};
          gpci.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
          gpci.pNext = NULL;
          gpci.flags = 0;
          gpci.stageCount = 2;
          gpci.pStages = pssci;
          gpci.pVertexInputState = &pvisci;
          gpci.pInputAssemblyState = &piasci;
          gpci.pViewportState = &pvsci;
          gpci.pRasterizationState = &prsci;
          gpci.pMultisampleState = &pmsci;
          gpci.pColorBlendState = &pcbsci;
          gpci.pDynamicState = &pdsci;
          gpci.layout = vulkanPipelineLayout;
          gpci.renderPass = vulkanRenderPass;
          gpci.subpass = 0;

          if(fnCreateGraphicsPipelines(vulkanLogicalDevice, NULL, 1, &gpci, NULL, &vulkanGraphicsPipeline) != VK_SUCCESS) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to make graphics pipeline.\n");
            return -1;
          }

          SDL_Log("Successfully initialized graphics pipeline.\n");

          // Framebuffer
          vulkanFramebuffers = (VkFramebuffer*) malloc(sizeof(VkFramebuffer) * vulkanSwapchainImageCount);
          for(int i = 0; i < vulkanSwapchainImageCount; i++) {
            VkImageView attachments[1] = {
              vulkanImageViews[i]
            };

            VkFramebufferCreateInfo fci = {};
            fci.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
            fci.pNext = NULL;
            fci.flags = 0;
            fci.renderPass = vulkanRenderPass;
            fci.attachmentCount = 1;
            fci.pAttachments = attachments;
            fci.width = vulkanSwapExtent.width;
            fci.height = vulkanSwapExtent.height;
            fci.layers = 1;

            if(fnCreateFramebuffer(vulkanLogicalDevice, &fci, NULL, &(vulkanFramebuffers[i])) != VK_SUCCESS) {
              SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to make framebuffer.\n");
              return -1;
            }
          }
          SDL_Log("Successfully initialized framebuffer\n");

          // Command pool
          VkCommandPoolCreateInfo cpci = {};
          cpci.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
          cpci.pNext = NULL;
          cpci.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
          cpci.queueFamilyIndex = graphicsQueueIndex;
          if(fnCreateCommandPool(vulkanLogicalDevice, &cpci, NULL, &vulkanCommandPool) != VK_SUCCESS) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to make command pool.\n");
            return -1;
          }

          SDL_Log("Successfully initialized Vulkan.\n");
          return 0;
        } else {
          SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to initialize shaders.\n");
          return -1;
        }
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
  fnDestroyCommandPool(vulkanLogicalDevice, vulkanCommandPool, NULL);
  for(int i = 0; i < vulkanSwapchainImageCount; i++) {
    fnDestroyFramebuffer(vulkanLogicalDevice, vulkanFramebuffers[i], NULL);
  }
  free(vulkanFramebuffers);
  fnDestroyPipeline(vulkanLogicalDevice, vulkanGraphicsPipeline, NULL);
  fnDestroyRenderPass(vulkanLogicalDevice, vulkanRenderPass, NULL);
  fnDestroyPipelineLayout(vulkanLogicalDevice, vulkanPipelineLayout, NULL);
  fnDestroyShaderModule(vulkanLogicalDevice, vertModule, NULL);
  fnDestroyShaderModule(vulkanLogicalDevice, fragModule, NULL);
  for(int i = 0; i < vulkanSwapchainImageCount; i++) {
    fnDestroyImageView(vulkanLogicalDevice, vulkanImageViews[i], NULL);
  }
  fnDestroySwapchainKHR(vulkanLogicalDevice, vulkanSwapchain, NULL);
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

  running = false;
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
