#include <SDL2/SDL.h>
#include <SDL2/SDL_vulkan.h>
#include <vulkan/vulkan.h>

#include <glm/glm.hpp>

#include <stdlib.h>
#include <malloc.h>
#include <cstring>
#include <string>
#include <set>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

// Debug macros
#ifdef RAIKA_DEBUG
#define DBG_LOG(...) do { \
    SDL_Log(__VA_ARGS__); \
  } while(0)
#define DBG_LOGERROR(...) do { \
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, __VA_ARGS__); \
  } while(0)
#else
#define DBG_LOG(...) do {} while(0)
#define DBG_LOGERROR(...) do{} while(0)
#endif

// Structs
struct Vertex {
  glm::vec2 pos;
  glm::vec3 color;
};

struct FrameData {
  VkSemaphore imgAvlSem, rndFnsdSem;
  VkFence inFlight;
  VkCommandPool cp;
  VkCommandBuffer cb;

  // Uniform
  VkBuffer ub;
  VkDeviceMemory ubMem;
  void* ubMapped; 
};

struct ViewData {
  glm::vec2 offset;
};

// Constants
static const char *TITLE = "Raika";
#ifdef RAIKA_DEBUG
static const char* ACTIVE_VAL_LAYERS[1] = {"VK_LAYER_KHRONOS_validation"};
static const int ACTIVE_VAL_LAYER_COUNT = 1;
#else
static const char* ACTIVE_VAL_LAYERS[1] = {};
static const int ACTIVE_VAL_LAYER_COUNT = 0;
#endif
static const char* ACTIVE_INST_EXTENSIONS[1] = {
  VK_EXT_DEBUG_UTILS_EXTENSION_NAME
};
static const int ACTIVE_INST_EXTENSION_COUNT = 1;
static const char* ACTIVE_DEV_EXTENSIONS[1] = {
  VK_KHR_SWAPCHAIN_EXTENSION_NAME
};
static const int ACTIVE_DEV_EXTENSION_COUNT = 1;
static const int FPS = 60;
static const Vertex VERTICES[3] = {
  {{0.0f, -0.5f}, {0.5f, 0.5f, 0.0f}},
  {{0.5f, 0.5f}, {0.0f, 1.0f, 0.0f}},
  {{-0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}} 
};
static const uint32_t VERTEX_COUNT = 3;
static const uint32_t FRAME_COUNT = 2;
static const uint32_t HEIGHT = 500;
static const uint32_t WIDTH = 500;

// Globals
static bool running = false;
static bool windowResized = false;
static uint32_t currentFrame = 0;
static std::string basePath = "";
static SDL_Window *window = NULL;
static VkSurfaceKHR vulkanSurface = NULL;
static VkSwapchainKHR vulkanSwapchain = NULL;
static uint32_t vulkanSwapchainImageCount = 0;
static VkFormat swapchainImageFormat = {};
static VkExtent2D vulkanSwapExtent = {};
static VkImage* vulkanSwapchainImages = NULL;
static VkImageView* vulkanImageViews = NULL;
static VkImage vulkanTextureImage = NULL;
static VkShaderModule vertModule = NULL;
static VkShaderModule fragModule = NULL;
static VkRenderPass vulkanRenderPass = NULL;
static VkDescriptorSetLayout vulkanDescriptorSetLayouts[FRAME_COUNT] = {};
static VkDescriptorPool vulkanDescriptorPool = NULL;
static VkDescriptorSet vulkanDescriptorSets[FRAME_COUNT] = {};
static VkPipelineLayout vulkanPipelineLayout = NULL;
static VkPipeline vulkanGraphicsPipeline = NULL;
static VkCommandPool vulkanGlobalCB = NULL;
static FrameData vulkanFrames[FRAME_COUNT] = {};
static VkFramebuffer* vulkanFramebuffers = NULL;
static VkBuffer vulkanStagingBuffer = NULL;
static VkBuffer vulkanVertexBuffer = NULL;
static VkDeviceMemory vulkanStagingDeviceMemory = NULL;
static VkDeviceMemory vulkanVertexDeviceMemory = NULL;
static VkDeviceMemory vulkanTextureImageMemory = NULL;
static VkInstance vulkanInstance = NULL;
static VkDevice vulkanLogicalDevice = NULL;
static VkQueue vulkanGraphicsQueue = NULL;
static VkQueue vulkanPresentQueue = NULL;
static VkDebugUtilsMessengerEXT debugMessenger = NULL;
static VkPhysicalDevice vulkanPhysicalDevice = VK_NULL_HANDLE;
static VkResult res = VK_SUCCESS;
static uint32_t graphicsQueueIndex = 0;
static uint32_t presentQueueIndex = 0;

// Function load macro
#define LOAD_VK_FN(INSTANCE, NAME) do { \
    fn ## NAME = (PFN_vk ## NAME) fnGetInstanceProcAddr(INSTANCE, "vk" #NAME); \
    if(!fn ## NAME) DBG_LOG("Failed to load %s\n", #NAME); \
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
static PFN_vkGetPhysicalDeviceMemoryProperties fnGetPhysicalDeviceMemoryProperties = NULL;
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
static PFN_vkCreateImage fnCreateImage = NULL;
static PFN_vkDestroyImage fnDestroyImage = NULL;
static PFN_vkCreateShaderModule fnCreateShaderModule = NULL;
static PFN_vkDestroyShaderModule fnDestroyShaderModule = NULL;
static PFN_vkCreatePipelineLayout fnCreatePipelineLayout = NULL;
static PFN_vkCreateRenderPass fnCreateRenderPass = NULL;
static PFN_vkDestroyRenderPass fnDestroyRenderPass = NULL;
static PFN_vkCreateGraphicsPipelines fnCreateGraphicsPipelines = NULL;
static PFN_vkCreateDescriptorSetLayout fnCreateDescriptorSetLayout = NULL;
static PFN_vkCreateDescriptorPool fnCreateDescriptorPool = NULL;
static PFN_vkUpdateDescriptorSets fnUpdateDescriptorSets = NULL;
static PFN_vkDestroyPipeline fnDestroyPipeline = NULL;
static PFN_vkDestroyPipelineLayout fnDestroyPipelineLayout = NULL;
static PFN_vkDestroyDescriptorSetLayout fnDestroyDescriptorSetLayout = NULL;
static PFN_vkDestroyDescriptorPool fnDestroyDescriptorPool = NULL;
static PFN_vkCreateFramebuffer fnCreateFramebuffer = NULL;
static PFN_vkDestroyFramebuffer fnDestroyFramebuffer = NULL;
static PFN_vkCreateCommandPool fnCreateCommandPool = NULL;
static PFN_vkDestroyCommandPool fnDestroyCommandPool = NULL;
static PFN_vkAllocateCommandBuffers fnAllocateCommandBuffers = NULL;
static PFN_vkAllocateMemory fnAllocateMemory = NULL;
static PFN_vkAllocateDescriptorSets fnAllocateDescriptorSets = NULL;
static PFN_vkFreeCommandBuffers fnFreeCommandBuffers = NULL;
static PFN_vkFreeMemory fnFreeMemory = NULL;
static PFN_vkMapMemory fnMapMemory = NULL;
static PFN_vkUnmapMemory fnUnmapMemory = NULL;
static PFN_vkBindBufferMemory fnBindBufferMemory = NULL;
static PFN_vkBindImageMemory fnBindImageMemory = NULL;
static PFN_vkBeginCommandBuffer fnBeginCommandBuffer = NULL;
static PFN_vkCmdBeginRenderPass fnCmdBeginRenderPass = NULL;
static PFN_vkCmdBindPipeline fnCmdBindPipeline = NULL;
static PFN_vkCmdBindVertexBuffers fnCmdBindVertexBuffers = NULL;
static PFN_vkCmdBindDescriptorSets fnCmdBindDescriptorSets = NULL;
static PFN_vkCmdSetViewport fnCmdSetViewport = NULL;
static PFN_vkCmdSetScissor fnCmdSetScissor = NULL;
static PFN_vkCmdCopyBuffer fnCmdCopyBuffer = NULL;
static PFN_vkCmdCopyBufferToImage fnCmdCopyBufferToImage = NULL;
static PFN_vkCmdDraw fnCmdDraw = NULL;
static PFN_vkCmdEndRenderPass fnCmdEndRenderPass = NULL;
static PFN_vkCmdPipelineBarrier fnCmdPipelineBarrier = NULL;
static PFN_vkEndCommandBuffer fnEndCommandBuffer = NULL;
static PFN_vkResetCommandBuffer fnResetCommandBuffer = NULL;
static PFN_vkCreateSemaphore fnCreateSemaphore = NULL;
static PFN_vkCreateFence fnCreateFence = NULL;
static PFN_vkDestroySemaphore fnDestroySemaphore = NULL;
static PFN_vkDestroyFence fnDestroyFence = NULL;
static PFN_vkWaitForFences fnWaitForFences = NULL;
static PFN_vkResetFences fnResetFences = NULL;
static PFN_vkAcquireNextImageKHR fnAcquireNextImageKHR = NULL;
static PFN_vkQueueSubmit fnQueueSubmit = NULL;
static PFN_vkQueuePresentKHR fnQueuePresentKHR = NULL;
static PFN_vkDeviceWaitIdle fnDeviceWaitIdle = NULL;
static PFN_vkQueueWaitIdle fnQueueWaitIdle = NULL;
static PFN_vkCreateBuffer fnCreateBuffer = NULL;
static PFN_vkDestroyBuffer fnDestroyBuffer = NULL;
static PFN_vkGetBufferMemoryRequirements fnGetBufferMemoryRequirements = NULL;
static PFN_vkGetImageMemoryRequirements fnGetImageMemoryRequirements = NULL;

int loadVulkanFns() {
  // Load functions
  LOAD_VK_FN(vulkanInstance, DestroyInstance);
  LOAD_VK_FN(vulkanInstance, EnumeratePhysicalDevices);
  LOAD_VK_FN(vulkanInstance, GetPhysicalDeviceProperties);
  LOAD_VK_FN(vulkanInstance, GetPhysicalDeviceMemoryProperties);
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
  LOAD_VK_FN(vulkanInstance, CreateImage);
  LOAD_VK_FN(vulkanInstance, DestroyImage);
  LOAD_VK_FN(vulkanInstance, CreateShaderModule);
  LOAD_VK_FN(vulkanInstance, DestroyShaderModule);
  LOAD_VK_FN(vulkanInstance, CreatePipelineLayout);
  LOAD_VK_FN(vulkanInstance, DestroyPipelineLayout);
  LOAD_VK_FN(vulkanInstance, CreateRenderPass);
  LOAD_VK_FN(vulkanInstance, DestroyRenderPass);
  LOAD_VK_FN(vulkanInstance, CreateGraphicsPipelines);
  LOAD_VK_FN(vulkanInstance, CreateDescriptorSetLayout);
  LOAD_VK_FN(vulkanInstance, DestroyDescriptorSetLayout);
  LOAD_VK_FN(vulkanInstance, UpdateDescriptorSets);
  LOAD_VK_FN(vulkanInstance, CreateDescriptorPool);
  LOAD_VK_FN(vulkanInstance, DestroyDescriptorPool);
  LOAD_VK_FN(vulkanInstance, DestroyPipeline);
  LOAD_VK_FN(vulkanInstance, CreateFramebuffer);
  LOAD_VK_FN(vulkanInstance, DestroyFramebuffer);
  LOAD_VK_FN(vulkanInstance, CreateCommandPool);
  LOAD_VK_FN(vulkanInstance, DestroyCommandPool);
  LOAD_VK_FN(vulkanInstance, AllocateCommandBuffers);
  LOAD_VK_FN(vulkanInstance, AllocateMemory);
  LOAD_VK_FN(vulkanInstance, AllocateDescriptorSets);
  LOAD_VK_FN(vulkanInstance, FreeCommandBuffers);
  LOAD_VK_FN(vulkanInstance, FreeMemory);
  LOAD_VK_FN(vulkanInstance, MapMemory);
  LOAD_VK_FN(vulkanInstance, UnmapMemory);
  LOAD_VK_FN(vulkanInstance, BindBufferMemory);
  LOAD_VK_FN(vulkanInstance, BindImageMemory);
  LOAD_VK_FN(vulkanInstance, BeginCommandBuffer);
  LOAD_VK_FN(vulkanInstance, CmdBeginRenderPass);
  LOAD_VK_FN(vulkanInstance, CmdBindPipeline);
  LOAD_VK_FN(vulkanInstance, CmdBindVertexBuffers);
  LOAD_VK_FN(vulkanInstance, CmdBindDescriptorSets);
  LOAD_VK_FN(vulkanInstance, CmdSetViewport);
  LOAD_VK_FN(vulkanInstance, CmdSetScissor);
  LOAD_VK_FN(vulkanInstance, CmdCopyBuffer);
  LOAD_VK_FN(vulkanInstance, CmdCopyBufferToImage);
  LOAD_VK_FN(vulkanInstance, CmdDraw);
  LOAD_VK_FN(vulkanInstance, CmdEndRenderPass);
  LOAD_VK_FN(vulkanInstance, CmdPipelineBarrier);
  LOAD_VK_FN(vulkanInstance, EndCommandBuffer);
  LOAD_VK_FN(vulkanInstance, ResetCommandBuffer);
  LOAD_VK_FN(vulkanInstance, CreateSemaphore);
  LOAD_VK_FN(vulkanInstance, CreateFence);
  LOAD_VK_FN(vulkanInstance, DestroySemaphore);
  LOAD_VK_FN(vulkanInstance, DestroyFence);
  LOAD_VK_FN(vulkanInstance, WaitForFences);
  LOAD_VK_FN(vulkanInstance, ResetFences);
  LOAD_VK_FN(vulkanInstance, AcquireNextImageKHR);
  LOAD_VK_FN(vulkanInstance, QueueSubmit);
  LOAD_VK_FN(vulkanInstance, QueuePresentKHR);
  LOAD_VK_FN(vulkanInstance, DeviceWaitIdle);
  LOAD_VK_FN(vulkanInstance, QueueWaitIdle);
  LOAD_VK_FN(vulkanInstance, CreateBuffer);
  LOAD_VK_FN(vulkanInstance, DestroyBuffer);
  LOAD_VK_FN(vulkanInstance, GetBufferMemoryRequirements);
  LOAD_VK_FN(vulkanInstance, GetImageMemoryRequirements);
  return 0;
}

#ifdef VULKAN_DEBUG
VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
  VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
  VkDebugUtilsMessageTypeFlagsEXT messageTypes,
  const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
  void* pUserData
) {
  SDL_Log("[DBG]: %s\n", pCallbackData->pMessage);
  return VK_FALSE;
}
#else
VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
  VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
  VkDebugUtilsMessageTypeFlagsEXT messageTypes,
  const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
  void* pUserData
) {
  return VK_FALSE;
}
#endif

uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) {
  VkPhysicalDeviceMemoryProperties pdmp;
  fnGetPhysicalDeviceMemoryProperties(vulkanPhysicalDevice, &pdmp);
  for(uint32_t i = 0; i < pdmp.memoryTypeCount; i++) {
    if(typeFilter & (1 << i) && (pdmp.memoryTypes[i].propertyFlags & properties) == properties) {
      return i;
    }
  }

  DBG_LOGERROR("Failed to find memory type.\n");
  return -1;
}

VkVertexInputBindingDescription* getVibd() {
  VkVertexInputBindingDescription* vibd = (VkVertexInputBindingDescription*) malloc(sizeof(VkVertexInputBindingDescription));
  vibd->binding = 0;
  vibd->stride = sizeof(Vertex);
  vibd->inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
  return vibd;
}

VkVertexInputAttributeDescription* getViad() {
  VkVertexInputAttributeDescription* viads = (VkVertexInputAttributeDescription*) malloc(sizeof(VkVertexInputAttributeDescription) * 2);
  // Vertex
  viads[0].binding = 0;
  viads[0].location = 0;
  viads[0].format = VK_FORMAT_R32G32_SFLOAT;
  viads[0].offset = offsetof(Vertex, pos);
  // Color
  viads[1].binding = 0;
  viads[1].location = 1;
  viads[1].format = VK_FORMAT_R32G32B32_SFLOAT;
  viads[1].offset = offsetof(Vertex, color);

  return viads;
}

int beginSingleCommandBuffer(VkCommandBuffer* cb) {
  VkCommandBufferAllocateInfo cbai = {};
  cbai.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  cbai.pNext = NULL;
  cbai.commandBufferCount = 1;
  cbai.commandPool = vulkanGlobalCB;
  cbai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  if(fnAllocateCommandBuffers(vulkanLogicalDevice, &cbai, cb) != VK_SUCCESS) {
    DBG_LOGERROR("Failed to make copy buffer cb.\n");
  }
  VkCommandBufferBeginInfo cbbi = {};
  cbbi.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  cbbi.pNext = NULL;
  cbbi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
  fnBeginCommandBuffer(*cb, &cbbi);

  return 0;
}

int endSingleCommandBuffer(VkCommandBuffer* cb) {
  fnEndCommandBuffer(*cb);
  VkSubmitInfo si = {};
  si.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
  si.pNext = NULL;
  si.commandBufferCount = 1;
  si.pCommandBuffers = cb;
  if(fnQueueSubmit(vulkanGraphicsQueue, 1, &si, VK_NULL_HANDLE) != VK_SUCCESS) {
    DBG_LOGERROR("Failed to submit copy buffer queue.\n");
    return -1;
  };
  fnQueueWaitIdle(vulkanGraphicsQueue);
  fnFreeCommandBuffers(vulkanLogicalDevice, vulkanGlobalCB, 1, cb);

  return 0;
}

int copyBuffer(VkBuffer src, VkBuffer dst, VkDeviceSize size) {
  VkCommandBuffer cb;
  beginSingleCommandBuffer(&cb);

  VkBufferCopy bc = {};
  bc.size = size;
  bc.srcOffset = 0;
  bc.dstOffset = 0;
  fnCmdCopyBuffer(cb, src, dst, 1, &bc);

  endSingleCommandBuffer(&cb);
  return 0;
}

int copyBufferToImage(VkBuffer src, VkImage dst, uint32_t width, uint32_t height) {
  VkCommandBuffer cb;
  beginSingleCommandBuffer(&cb);

  VkBufferImageCopy bic = {};
  bic.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  bic.imageSubresource.baseArrayLayer = 0;
  bic.imageSubresource.layerCount = 1;
  bic.imageSubresource.mipLevel = 0;

  bic.imageExtent.width = width;
  bic.imageExtent.height = height;
  bic.imageExtent.depth = 1;
  bic.imageOffset.x = 0;
  bic.imageOffset.y = 0;
  bic.imageOffset.z = 0;

  bic.bufferOffset = 0;
  bic.bufferImageHeight = 0;
  bic.bufferRowLength = 0;

  fnCmdCopyBufferToImage(cb, src, dst, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &bic);

  endSingleCommandBuffer(&cb);
  return 0;
}

VkShaderModule createShaderModule(uint32_t* code, size_t size) {
  VkShaderModuleCreateInfo ci = {};
  ci.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
  ci.codeSize = size;
  ci.pCode = code;
  VkShaderModule out;
  if(fnCreateShaderModule(vulkanLogicalDevice, &ci, NULL, &out) != VK_SUCCESS) {
    DBG_LOGERROR("Failed to create shader module\n");
    return NULL;
  }
  return out;
}

int initSwapchain() {
  // Check swapchain capability
  VkSurfaceCapabilitiesKHR surfaceCapabilities;
  fnGetPhysicalDeviceSurfaceCapabilitiesKHR(vulkanPhysicalDevice, vulkanSurface, &surfaceCapabilities);

  uint32_t formatCount = 0;
  if(fnGetPhysicalDeviceSurfaceFormatsKHR(vulkanPhysicalDevice, vulkanSurface, &formatCount, NULL) != VK_SUCCESS) {
    DBG_LOGERROR("Error getting format count: %s\n", SDL_GetError());
    return -1;
  };
  DBG_LOG("Format count: %d\n", formatCount);
  VkSurfaceFormatKHR* formats = (VkSurfaceFormatKHR*) malloc(sizeof(VkSurfaceFormatKHR) * formatCount);
  fnGetPhysicalDeviceSurfaceFormatsKHR(vulkanPhysicalDevice, vulkanSurface, &formatCount, formats);
  DBG_LOG("Formats written: %d\n", formatCount);

  uint32_t presentModeCount = 0;
  if(fnGetPhysicalDeviceSurfacePresentModesKHR(vulkanPhysicalDevice, vulkanSurface, &presentModeCount, NULL) != VK_SUCCESS) {
    DBG_LOGERROR("Error getting present mode count: %s\n", SDL_GetError());
    return -1;
  };
  DBG_LOG("Present mode count: %d\n", presentModeCount);
  VkPresentModeKHR* presentModes = (VkPresentModeKHR*) malloc(sizeof(VkPresentModeKHR) * presentModeCount);
  fnGetPhysicalDeviceSurfacePresentModesKHR(vulkanPhysicalDevice, vulkanSurface, &presentModeCount, presentModes);
  DBG_LOG("Present modes written: %d\n", presentModeCount);

  if(formatCount == 0 ||  presentModeCount == 0) {
    DBG_LOGERROR("Format and present mode not found.\n");
    return -1;
  }
  // Set details for swapchain
  VkSurfaceFormatKHR vulkanSurfaceFormat = {};
  VkPresentModeKHR vulkanPresentMode = VK_PRESENT_MODE_FIFO_KHR;

  // Determine surface format
  bool foundFormat = false;
  for(uint32_t i = 0; i < formatCount; i++) {
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
  for(uint32_t i = 0; i < presentModeCount; i++) {
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
    bool lockHeight = ((uint32_t) pxHeight / (uint32_t) pxWidth) < (HEIGHT / WIDTH);
    if(lockHeight) {
      pxWidth = (pxHeight * WIDTH) / HEIGHT;
    } else {
      pxHeight = (pxWidth * HEIGHT) / WIDTH;
    }
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
    DBG_LOGERROR("Failed to initialize swapchain.\n");
    return -1;
  };
  DBG_LOG("Successfully initalized swapchain.\n");
  return 0;
}

int initImageViews() {
  // Get images
  if(fnGetSwapchainImagesKHR(vulkanLogicalDevice, vulkanSwapchain, &vulkanSwapchainImageCount, NULL) != VK_SUCCESS) {
    DBG_LOGERROR("Failed to retrieve swapchain image count.\n");
    return -1;
  };
  DBG_LOG("Swapchain image count: %d\n", vulkanSwapchainImageCount);
  vulkanSwapchainImages = (VkImage*) malloc(sizeof(VkImage) * vulkanSwapchainImageCount);
  if(fnGetSwapchainImagesKHR(vulkanLogicalDevice, vulkanSwapchain, &vulkanSwapchainImageCount, vulkanSwapchainImages) != VK_SUCCESS) {
    DBG_LOGERROR("Failed to retrieve swapchain images.\n");
    return -1;
  };

  // Get image views from images
  vulkanImageViews = (VkImageView*) malloc(sizeof(VkImageView) * vulkanSwapchainImageCount);
  for(uint32_t i = 0; i < vulkanSwapchainImageCount; i++) {
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
      DBG_LOGERROR("Failed to create image view.\n");
      return -1;
    };
  }
  DBG_LOG("Successfully created image views.\n");
  return 0;
}

int initFramebuffers() {
  vulkanFramebuffers = (VkFramebuffer*) malloc(sizeof(VkFramebuffer) * vulkanSwapchainImageCount);
  for(uint32_t i = 0; i < vulkanSwapchainImageCount; i++) {
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
      DBG_LOGERROR("Failed to make framebuffer.\n");
      return -1;
    }
  }
  DBG_LOG("Successfully initialized framebuffer\n");
  return 0;
}

void recreateSwapchain() {
  DBG_LOG("Recreating swapchain...\n");
  fnDeviceWaitIdle(vulkanLogicalDevice);

  for(uint32_t i = 0; i < vulkanSwapchainImageCount; i++) {
    fnDestroyFramebuffer(vulkanLogicalDevice, vulkanFramebuffers[i], NULL);
    fnDestroyImageView(vulkanLogicalDevice, vulkanImageViews[i], NULL);
  }
  fnDestroySwapchainKHR(vulkanLogicalDevice, vulkanSwapchain, NULL);

  initSwapchain();
  initImageViews();
  initFramebuffers();
}

int createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags propertyFlags, VkBuffer* buffer, VkDeviceMemory* devMem) {
  VkBufferCreateInfo bci = {};
  bci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
  bci.pNext = NULL;
  bci.flags = 0;
  bci.size = size;
  bci.usage = usage;
  bci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
  if(fnCreateBuffer(vulkanLogicalDevice, &bci, NULL, buffer) != VK_SUCCESS) {
    DBG_LOGERROR("Failed to make vertex buffer.\n");
    return -1;
  }
  VkMemoryRequirements mr;
  fnGetBufferMemoryRequirements(vulkanLogicalDevice, *buffer, &mr);
  VkMemoryAllocateInfo ai = {};
  ai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
  ai.pNext = NULL;
  ai.allocationSize = mr.size;
  ai.memoryTypeIndex = findMemoryType(mr.memoryTypeBits, propertyFlags);

  if(fnAllocateMemory(vulkanLogicalDevice, &ai, NULL, devMem) != VK_SUCCESS) {
    DBG_LOGERROR("Failed to allocate memory.\n");
    return -1;
  };
  if(fnBindBufferMemory(vulkanLogicalDevice, *buffer, *devMem, 0) != VK_SUCCESS) {
    DBG_LOGERROR("Failed to bind buffer to memory.\n");
    return -1;
  };
  DBG_LOG("Successfully allocated memory.\n");
  return 0;
}

int transitionImageLayout(VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout) {
  VkCommandBuffer cb = NULL;
  beginSingleCommandBuffer(&cb);

  VkPipelineStageFlags srcStage, dstStage;

  VkImageMemoryBarrier imb = {};
  imb.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
  imb.pNext = NULL;
  imb.image = image;
  imb.oldLayout = oldLayout;
  imb.newLayout = newLayout;
  imb.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  imb.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  imb.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  imb.subresourceRange.baseArrayLayer = 0;
  imb.subresourceRange.baseMipLevel = 0;
  imb.subresourceRange.layerCount = 1;
  imb.subresourceRange.levelCount = 1;
  
  if(oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
    imb.srcAccessMask = 0;
    imb.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

    srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    dstStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
  } else if(oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
    imb.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    imb.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

    srcStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    dstStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
  } else {
    imb.srcAccessMask = 0;
    imb.dstAccessMask = 0;

    srcStage = 0;
    dstStage = 0;
  }

  fnCmdPipelineBarrier(cb, srcStage, dstStage, 0, 0, NULL, 0, NULL, 1, &imb);

  endSingleCommandBuffer(&cb);

  return 0;
}

int createImage(uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling, 
                VkMemoryPropertyFlags properties, VkImageUsageFlags usage, VkImage* image, VkDeviceMemory* imageMem) {
  VkImageCreateInfo ici = {};
  ici.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
  ici.pNext = NULL;
  ici.flags = 0;
  ici.imageType = VK_IMAGE_TYPE_2D;
  ici.extent.width = width;
  ici.extent.height = height;
  ici.extent.depth = 1;
  ici.mipLevels = 1;
  ici.arrayLayers = 1;
  ici.format = format;
  ici.tiling = tiling;
  ici.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  ici.usage = usage;
  ici.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
  ici.samples = VK_SAMPLE_COUNT_1_BIT;

  if(fnCreateImage(vulkanLogicalDevice, &ici, NULL, image) != VK_SUCCESS) {
    DBG_LOGERROR("Failed to create vulkan image texture.\n");
    return -1;
  }

  VkMemoryRequirements mr;
  fnGetImageMemoryRequirements(vulkanLogicalDevice, *image, &mr);
  VkMemoryAllocateInfo mai = {};
  mai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
  mai.pNext = NULL;
  mai.allocationSize = mr.size;
  mai.memoryTypeIndex = findMemoryType(mr.memoryTypeBits, properties);

  if(fnAllocateMemory(vulkanLogicalDevice, &mai, NULL, imageMem) != VK_SUCCESS) {
    DBG_LOGERROR("Failed to allocate image memory.\n");
    return -1;
  };
  fnBindImageMemory(vulkanLogicalDevice, *image, *imageMem, 0);
  DBG_LOG("Successfully created image.\n");
  return 0;
}

int createTextureImage(VkImage image, VkDeviceMemory imageMem) {
  int texW, texH, texCh;
  VkBuffer stagingBuffer;
  VkDeviceMemory stagingBufferMemory;

  stbi_uc* pixels = stbi_load((basePath + "../textures/texture.png").c_str(), &texW, &texH, &texCh, STBI_rgb_alpha);
  VkDeviceSize imageSize = texW * texH * 4;

  if(!pixels) {
    DBG_LOGERROR("Failed to load image texture.\n");
    return -1;
  }

  createBuffer(
    imageSize, 
    VK_BUFFER_USAGE_TRANSFER_SRC_BIT, 
    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
    &stagingBuffer,
    &stagingBufferMemory
  );

  void* data;
  fnMapMemory(vulkanLogicalDevice, stagingBufferMemory, 0, imageSize, 0, &data);
  memcpy(data, pixels, (size_t) imageSize);
  fnUnmapMemory(vulkanLogicalDevice, stagingBufferMemory);
  stbi_image_free(pixels);

  createImage(
    (uint32_t) texW, (uint32_t) texH, 
    VK_FORMAT_R8G8B8A8_SRGB, 
    VK_IMAGE_TILING_OPTIMAL, 
    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, 
    VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, 
    &image, 
    &imageMem
  );

  transitionImageLayout(image, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
  copyBufferToImage(stagingBuffer, image, (uint32_t) texW, (uint32_t) texH);
  transitionImageLayout(image, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
  
  fnDestroyBuffer(vulkanLogicalDevice, stagingBuffer, NULL);
  fnFreeMemory(vulkanLogicalDevice, stagingBufferMemory, NULL);

  DBG_LOG("Successfully created texture image.\n");
  return 0;
}

int init() {
  // Init SDL
  if(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) < 0) {
    DBG_LOGERROR("Failed to initialize SDL. Error: %s\n", SDL_GetError());
    return -1;
  }

  basePath = std::string(SDL_GetBasePath());
  DBG_LOG("Base path: %s\n", basePath.c_str());

  // Load vulkan driver
  SDL_Vulkan_LoadLibrary(NULL);

  // Create window
  window = SDL_CreateWindow(
    TITLE, 
    SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
    400, 400,
    SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE
  );

  if(window == NULL) { 
    DBG_LOGERROR("Failed to initialize window. Error: %s\n", SDL_GetError());
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
    DBG_LOGERROR("Error getting extensions: %d\n", res);
    return res;
  };
  DBG_LOG("Extension Count: %d\n", availableInstanceExtensionCount);
  VkExtensionProperties* availableInstanceExtensions = (VkExtensionProperties*) malloc(sizeof(VkExtensionProperties) * availableInstanceExtensionCount);
  fnEnumerateInstanceExtensionProperties(NULL, &availableInstanceExtensionCount, availableInstanceExtensions);
  DBG_LOG("Extensions written: %d\n", availableInstanceExtensionCount);
  for(uint32_t i = 0; i < availableInstanceExtensionCount; i++) {
    DBG_LOG("Extension %d: %s\n", i + 1, availableInstanceExtensions[i].extensionName);
  }

  bool instanceExtensionMissing = false;

  for(uint32_t i = 0; i < ACTIVE_INST_EXTENSION_COUNT; i++) {
    bool extensionFound = false;
    for(uint32_t j = 0; j < availableInstanceExtensionCount; j++) {
      if(strcmp(ACTIVE_INST_EXTENSIONS[i], availableInstanceExtensions[j].extensionName) == 0) {
        extensionFound = true;
        break;
      }
    }
    if(!extensionFound) {
      SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "Couldn't find extension %s\n", ACTIVE_INST_EXTENSIONS[i]);
      instanceExtensionMissing = true;
      break;
    }
  }

  // Check extensions needed for SDL
  uint32_t sdlNeededExtensionCount = 0;
  if(SDL_Vulkan_GetInstanceExtensions(window, &sdlNeededExtensionCount, NULL) != SDL_TRUE) {
    DBG_LOGERROR("Error getting SDL required extensions\n");
    return -1;
  };
  DBG_LOG("SDL Extension Count: %d\n", sdlNeededExtensionCount);
  const char** sdlNeededExtensions = (const char**) malloc(sizeof(const char*) * sdlNeededExtensionCount);
  SDL_Vulkan_GetInstanceExtensions(NULL, &sdlNeededExtensionCount, sdlNeededExtensions);
  DBG_LOG("SDL Extensions written: %d\n", sdlNeededExtensionCount);
  for(uint32_t i = 0; i < sdlNeededExtensionCount; i++) {
    DBG_LOG("SDL Extension %d: %s\n", i + 1, sdlNeededExtensions[i]);
  }

  for(uint32_t i = 0; i < sdlNeededExtensionCount; i++) {
    bool extensionFound = false;
    for(uint32_t j = 0; j < availableInstanceExtensionCount; j++) {
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
  const char** requestedExtensions = (const char**) malloc(sizeof(const char*) * (ACTIVE_INST_EXTENSION_COUNT + sdlNeededExtensionCount));
  for(uint32_t i = 0; i < ACTIVE_INST_EXTENSION_COUNT; i++) {
    requestedExtensions[i] = ACTIVE_INST_EXTENSIONS[i];
  }
  for(uint32_t i = 0; i < sdlNeededExtensionCount; i++)  {
    requestedExtensions[i + ACTIVE_INST_EXTENSION_COUNT] = sdlNeededExtensions[i];
  }
  DBG_LOG("Requesting Extensions:\n");
  for(uint32_t i = 0; i < (ACTIVE_INST_EXTENSION_COUNT + sdlNeededExtensionCount); i++)  {
    DBG_LOG("  %s\n", requestedExtensions[i]);
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
    DBG_LOGERROR("Error getting layer count: %s\n", SDL_GetError());
    return -1;
  };
  DBG_LOG("Layers count: %d\n", availableLayerCount);
  VkLayerProperties* availableLayers = (VkLayerProperties*) malloc(sizeof(VkLayerProperties) * availableLayerCount);
  fnEnumerateInstanceLayerProperties(&availableLayerCount, availableLayers);
  DBG_LOG("Layers written: %d\n", availableLayerCount);
  for(uint32_t i = 0; i < availableLayerCount; i++) {
    DBG_LOG("Layer %d: %s\n", i + 1, availableLayers[i].layerName);
  }

  bool layerMissing = false;

  for(uint32_t i = 0; i < ACTIVE_VAL_LAYER_COUNT; i++) {
    bool layerFound = false;
    for(uint32_t j = 0; j < availableLayerCount; j++) {
      if(strcmp(ACTIVE_VAL_LAYERS[i], availableLayers[j].layerName) == 0) {
        layerFound = true;
        break;
      }
    }
    if(!layerFound) {
      SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "Couldn't find layer %s\n", ACTIVE_VAL_LAYERS[i]);
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
  ici.enabledExtensionCount = (ACTIVE_INST_EXTENSION_COUNT + sdlNeededExtensionCount);
  ici.ppEnabledExtensionNames = requestedExtensions;
  if(!layerMissing && !instanceExtensionMissing) {
    ici.enabledLayerCount = ACTIVE_VAL_LAYER_COUNT;
    ici.ppEnabledLayerNames = ACTIVE_VAL_LAYERS;
    ici.pNext = (VkDebugUtilsMessengerCreateInfoEXT*) &dci;
  } else {
    DBG_LOG("Missing validation layer or extension, skipping debug\n");
    ici.enabledLayerCount = 0;
    ici.ppEnabledLayerNames = NULL;
  }

  if(fnCreateInstance(&ici, nullptr, &vulkanInstance) != VK_SUCCESS) {
    DBG_LOGERROR("Failed to make vulkan instance\n");
  };

  // Load Vulkan Functions
  if(loadVulkanFns() != 0) {
    DBG_LOGERROR("Failed to load all vulkan functions.\n");
    return -1;
  };
  
  // Create debug messenger
  if(!layerMissing && !instanceExtensionMissing) {
    LOAD_VK_FN(vulkanInstance, CreateDebugUtilsMessengerEXT);
    LOAD_VK_FN(vulkanInstance, DestroyDebugUtilsMessengerEXT);
    if(fnCreateDebugUtilsMessengerEXT(vulkanInstance, &dci, NULL, &debugMessenger) != VK_SUCCESS) {
      DBG_LOGERROR("Failed to make debug messenger\n");
    }
  }

  // Create vulkan surface
  if(SDL_Vulkan_CreateSurface(window, vulkanInstance, &vulkanSurface) != SDL_TRUE) {
    DBG_LOGERROR("Failed to initialize Vulkan surface. Error: %s\n", SDL_GetError());
    return -1;
  }

  // Get physical devices
  uint32_t deviceCount = 0;
  if(fnEnumeratePhysicalDevices(vulkanInstance, &deviceCount, NULL) != VK_SUCCESS) {
    DBG_LOGERROR("Error getting device count: %s\n", SDL_GetError());
    return -1;
  };
  DBG_LOG("Devices count: %d\n", deviceCount);
  VkPhysicalDevice* devices = (VkPhysicalDevice*) malloc(sizeof(VkPhysicalDevice) * deviceCount);
  fnEnumeratePhysicalDevices(vulkanInstance, &deviceCount, devices);
  DBG_LOG("Devices written: %d\n", deviceCount);

  // Check physical device suitability
  for(uint32_t i = 0; i < deviceCount; i++) {
    VkPhysicalDeviceProperties deviceProperties;
    VkPhysicalDeviceFeatures deviceFeatures;
    fnGetPhysicalDeviceProperties(devices[i], &deviceProperties);
    fnGetPhysicalDeviceFeatures(devices[i], &deviceFeatures);

    DBG_LOG("Checking device: %s\n", deviceProperties.deviceName);

    // Check extensions
    uint32_t availableDeviceExtensionCount = 0;
    if((res = fnEnumerateDeviceExtensionProperties(devices[i], NULL, &availableDeviceExtensionCount, NULL)) != VK_SUCCESS) {
      DBG_LOGERROR("Error getting device extensions: %d\n", res);
      return res;
    };
    DBG_LOG("Device extension Count: %d\n", availableDeviceExtensionCount);
    VkExtensionProperties* availableDeviceExtensions = (VkExtensionProperties*) malloc(sizeof(VkExtensionProperties) * availableDeviceExtensionCount);
    fnEnumerateDeviceExtensionProperties(devices[i], NULL, &availableDeviceExtensionCount, availableDeviceExtensions);
    DBG_LOG("Device extensions written: %d\n", availableDeviceExtensionCount);
    for(uint32_t i = 0; i < availableDeviceExtensionCount; i++) {
      DBG_LOG("Device extension %d: %s\n", i + 1, availableDeviceExtensions[i].extensionName);
    }

    bool deviceExtensionMissing = false;
    for(uint32_t i = 0; i < ACTIVE_DEV_EXTENSION_COUNT; i++) {
      bool extensionFound = false;
      for(uint32_t j = 0; j < availableDeviceExtensionCount; j++) {
        if(strcmp(ACTIVE_DEV_EXTENSIONS
                 [i], availableDeviceExtensions[j].extensionName) == 0) {
          extensionFound = true;
          break;
        }
      }
      if(!extensionFound) {
        SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "Couldn't find extension %s\n", ACTIVE_DEV_EXTENSIONS
       [i]);
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
    DBG_LOG("Queues count: %d\n", availableQueueCount);
    VkQueueFamilyProperties* availableQueues = (VkQueueFamilyProperties*) malloc(sizeof(VkQueueFamilyProperties) * availableQueueCount);
    fnGetPhysicalDeviceQueueFamilyProperties(devices[i], &availableQueueCount, availableQueues);
    DBG_LOG("Queues written: %d\n", availableQueueCount);

    bool graphicsSupported = false;
    bool presentSupported = false;

    for(uint32_t j = 0; j < availableQueueCount; j++) {
      if(availableQueues[j].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
        DBG_LOG("Queue %d: Graphics found (Count: %d)\n", j, availableQueues[j].queueCount);
        graphicsQueueIndex = j;
        graphicsSupported = true;
      };
      VkBool32 surfaceSupported;
      if(fnGetPhysicalDeviceSurfaceSupportKHR(devices[i], j, vulkanSurface, &surfaceSupported) != VK_SUCCESS) {
        DBG_LOGERROR("Failed to get physical device surface support.\n");
      };
      if(surfaceSupported == VK_TRUE) {
        DBG_LOG("Queue %d: Presentation found (Count: %d)\n", j, availableQueues[j].queueCount);
        presentQueueIndex = j; 
        presentSupported = true;
      };
      if(graphicsSupported && presentSupported) {
        // Found needed queues
        vulkanPhysicalDevice = devices[i];
        break;
      }
    }
    if(vulkanPhysicalDevice != VK_NULL_HANDLE) {
      DBG_LOG("Found device!\n");
      break;
    }
  }

  if(vulkanPhysicalDevice == VK_NULL_HANDLE) {
    // No suitable device found
    DBG_LOGERROR("Failed to find suitable physical device.\n");
    return -1;
  }

  // Create logical device from physical queue
  std::set<uint32_t> uniqueQueueFamilyIndex = {graphicsQueueIndex, presentQueueIndex};
  VkDeviceQueueCreateInfo* qci = (VkDeviceQueueCreateInfo*) malloc(sizeof(VkDeviceQueueCreateInfo) * uniqueQueueFamilyIndex.size());
  const float priorities[1] = { 1.0f };

  int j = 0;
  for(uint32_t queueFamilyIndex : uniqueQueueFamilyIndex) {
    qci[j].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    qci[j].queueFamilyIndex = queueFamilyIndex;
    qci[j].queueCount = 1;
    qci[j].pQueuePriorities = priorities;
    qci[j].pNext = NULL;
    qci[j].flags = 0;
    j++;
  }

  VkDeviceCreateInfo ldci = {};
  ldci.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
  ldci.queueCreateInfoCount = uniqueQueueFamilyIndex.size();
  ldci.pQueueCreateInfos = qci;
  ldci.enabledExtensionCount = ACTIVE_DEV_EXTENSION_COUNT;
  ldci.ppEnabledExtensionNames = ACTIVE_DEV_EXTENSIONS
 ;
  ldci.pEnabledFeatures = NULL; // Enable features here if needed later
  ldci.pNext = NULL;

  if((res = fnCreateDevice(vulkanPhysicalDevice, &ldci, NULL, &vulkanLogicalDevice)) != VK_SUCCESS) {
    DBG_LOGERROR("Failed to initialize logical device: %d\n", res);
  }

  fnGetDeviceQueue(vulkanLogicalDevice, graphicsQueueIndex, 0, &vulkanGraphicsQueue);
  fnGetDeviceQueue(vulkanLogicalDevice, presentQueueIndex, 0, &vulkanPresentQueue);

  if(initSwapchain() != 0) {
    DBG_LOGERROR("Failed to initialize swapchain.\n");
    return -1;
  }    

  // Image views
  if(initImageViews() != 0) {
    DBG_LOGERROR("Failed to initialize image views.\n");
    return -1;
  }

  // Uniform buffers
  for(uint32_t i = 0; i < FRAME_COUNT; i++) {
    if(createBuffer(
        sizeof(ViewData),
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        &(vulkanFrames[i].ub),
        &(vulkanFrames[i].ubMem)
       ) != 0) {
      DBG_LOGERROR("Failed to initialize uniform buffer.\n");
      return -1;
    }
    fnMapMemory(vulkanLogicalDevice, vulkanFrames[i].ubMem, 0, sizeof(ViewData), 0, &(vulkanFrames[i].ubMapped));
  }

  // Descriptor Pool
  VkDescriptorPoolSize dps = {};
  dps.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
  dps.descriptorCount = FRAME_COUNT;
  VkDescriptorPoolCreateInfo dpci = {};
  dpci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
  dpci.pNext = NULL;
  dpci.poolSizeCount = 1;
  dpci.pPoolSizes = &dps;
  dpci.maxSets = FRAME_COUNT;

  if(fnCreateDescriptorPool(vulkanLogicalDevice, &dpci, NULL, &vulkanDescriptorPool) != VK_SUCCESS) {
    DBG_LOGERROR("Failed to initialize desc pool.\n");
    return -1;
  }
  DBG_LOG("Successfully initialized desc pool.\n");

  // Descriptor Set
  VkDescriptorSetLayoutBinding dslb = {};
  dslb.binding = 0;
  dslb.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
  dslb.descriptorCount = 1;
  dslb.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

  VkDescriptorSetLayoutCreateInfo dslci = {};
  dslci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
  dslci.pNext = NULL;
  dslci.bindingCount = 1;
  dslci.pBindings = &dslb;

  for(uint32_t i = 0; i < FRAME_COUNT; i++) {
    if(fnCreateDescriptorSetLayout(vulkanLogicalDevice, &dslci, NULL, &(vulkanDescriptorSetLayouts[i])) != VK_SUCCESS) {
      DBG_LOGERROR("Failed to create desc set layout.\n");
      return -1;
    };
  }

  VkDescriptorSetAllocateInfo dsai = {};
  dsai.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
  dsai.pNext = NULL;
  dsai.descriptorPool = vulkanDescriptorPool;
  dsai.descriptorSetCount = FRAME_COUNT;
  dsai.pSetLayouts = vulkanDescriptorSetLayouts;

  if(fnAllocateDescriptorSets(vulkanLogicalDevice, &dsai, vulkanDescriptorSets) != VK_SUCCESS) {
    DBG_LOGERROR("Failed to initialize desc sets.\n");
    return -1;
  }

  for(uint32_t i = 0; i < FRAME_COUNT; i++) {
    VkDescriptorBufferInfo dbi = {};
    dbi.buffer = vulkanFrames[i].ub;
    dbi.offset = 0;
    dbi.range = VK_WHOLE_SIZE;

    VkWriteDescriptorSet wds = {};
    wds.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    wds.pNext = NULL;
    wds.dstSet = vulkanDescriptorSets[i];
    wds.dstBinding = 0;
    wds.dstArrayElement = 0;
    wds.descriptorCount = 1;
    wds.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    wds.pBufferInfo = &dbi;

    fnUpdateDescriptorSets(vulkanLogicalDevice, 1, &wds, 0, NULL);
  }

  // Create the graphics pipeline
  // Load shaders
  size_t fragSize, vertSize; 
  uint32_t* shaderFrag = (uint32_t*) SDL_LoadFile((basePath + "/frag.spv").c_str(), &fragSize);
  uint32_t* shaderVert = (uint32_t*) SDL_LoadFile((basePath + "/vert.spv").c_str(), &vertSize);

  DBG_LOG("Frag size: %zu\n", fragSize);
  DBG_LOG("Vert size: %zu\n", vertSize);

  fragModule = createShaderModule(shaderFrag, fragSize);
  vertModule = createShaderModule(shaderVert, vertSize);

  SDL_free(shaderFrag); 
  SDL_free(shaderVert); 

  if(fragModule == NULL || vertModule == NULL) {
    DBG_LOGERROR("Failed to initialize shaders.\n");
    return -1;
  }

  DBG_LOG("Successfully initialized shaders.\n");
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
  pvisci.vertexAttributeDescriptionCount = 2;
  pvisci.pVertexAttributeDescriptions = getViad();
  pvisci.vertexBindingDescriptionCount = 1;
  pvisci.pVertexBindingDescriptions = getVibd();

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
  plci.setLayoutCount = 1;
  plci.pSetLayouts = vulkanDescriptorSetLayouts;
  plci.pushConstantRangeCount = 0;

  if(fnCreatePipelineLayout(vulkanLogicalDevice, &plci, NULL, &vulkanPipelineLayout) != VK_SUCCESS) {
    DBG_LOGERROR("Failed to make pipeline layout.\n");
    return -1;
  }

  DBG_LOG("Successfully initialized pipeline layout.\n");

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
  VkSubpassDependency dep = {};
  dep.srcSubpass = VK_SUBPASS_EXTERNAL;
  dep.dstSubpass = 0;
  dep.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
  dep.srcAccessMask = 0;
  dep.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
  dep.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
  rpci.dependencyCount = 1;
  rpci.pDependencies = &dep;

  if(fnCreateRenderPass(vulkanLogicalDevice, &rpci, NULL, &vulkanRenderPass) != VK_SUCCESS) {
    DBG_LOGERROR("Failed to make render pass.\n");
    return -1;
  }

  DBG_LOG("Successfully initialized render pass.\n");

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
    DBG_LOGERROR("Failed to make graphics pipeline.\n");
    return -1;
  }

  DBG_LOG("Successfully initialized graphics pipeline.\n");

  // Framebuffer
  if(initFramebuffers() != 0) {
    DBG_LOGERROR("Failed to initialize framebuffer.\n");
    return -1;
  }
  
  // Command pool
  VkCommandPoolCreateInfo cpci = {};
  cpci.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
  cpci.pNext = NULL;
  cpci.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
  cpci.queueFamilyIndex = graphicsQueueIndex;
  for(uint32_t i = 0; i < FRAME_COUNT; i++) {
    if(fnCreateCommandPool(vulkanLogicalDevice, &cpci, NULL, &(vulkanFrames[i].cp)) != VK_SUCCESS) {
      DBG_LOGERROR("Failed to make command pool.\n");
      return -1;
    }
  }
  DBG_LOG("Successfully initialized command pool.\n");

  // Command buffer
  for(uint32_t i = 0; i < FRAME_COUNT; i++) {
    VkCommandBufferAllocateInfo cbai = {};
    cbai.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    cbai.pNext = NULL;
    cbai.commandPool = vulkanFrames[i].cp;
    cbai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cbai.commandBufferCount = 1;

    if(fnAllocateCommandBuffers(vulkanLogicalDevice, &cbai, &(vulkanFrames[i].cb)) != VK_SUCCESS) {
      DBG_LOGERROR("Failed to make command buffer.\n");
      return -1;
    }
  }
  // Global Command Pool
  if(fnCreateCommandPool(vulkanLogicalDevice, &cpci, NULL, &vulkanGlobalCB)!= VK_SUCCESS) {
    DBG_LOGERROR("Failed to make global command pool.\n");
    return -1;
  }
  DBG_LOG("Successfully initialized command buffer.\n");

  // Vertex Buffer
  VkDeviceSize vertBufferSize = sizeof(Vertex) * VERTEX_COUNT;
  if(createBuffer(
      vertBufferSize,
      VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
      VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
      &vulkanStagingBuffer,
      &vulkanStagingDeviceMemory
     ) != 0) {
    DBG_LOGERROR("Failed to initialize staging buffer.\n");
    return -1;
  }
  if(createBuffer(
      vertBufferSize,
      VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
      VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
      &vulkanVertexBuffer,
      &vulkanVertexDeviceMemory
     ) != 0) {
    DBG_LOGERROR("Failed to initialize vertex buffer.\n");
    return -1;
  }

  void* stgMem;
  fnMapMemory(vulkanLogicalDevice, vulkanStagingDeviceMemory, 0, vertBufferSize, 0, &stgMem);
  memcpy(stgMem, VERTICES, (size_t) vertBufferSize);
  fnUnmapMemory(vulkanLogicalDevice, vulkanStagingDeviceMemory);
  copyBuffer(vulkanStagingBuffer, vulkanVertexBuffer, vertBufferSize);
  fnDestroyBuffer(vulkanLogicalDevice, vulkanStagingBuffer, NULL);
  fnFreeMemory(vulkanLogicalDevice, vulkanStagingDeviceMemory, NULL);

  // Sync
  VkSemaphoreCreateInfo sci = {};
  sci.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

  VkFenceCreateInfo fci = {};
  fci.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
  fci.flags = VK_FENCE_CREATE_SIGNALED_BIT; // Prevent locked immediately

  for(uint32_t i = 0; i < FRAME_COUNT; i++) {
    if(vkCreateSemaphore(vulkanLogicalDevice, &sci, NULL, &(vulkanFrames[i].imgAvlSem)) != VK_SUCCESS ||
       vkCreateSemaphore(vulkanLogicalDevice, &sci, NULL, &(vulkanFrames[i].rndFnsdSem)) != VK_SUCCESS ||
       vkCreateFence(vulkanLogicalDevice, &fci, NULL, &(vulkanFrames[i].inFlight)) != VK_SUCCESS) {
      DBG_LOGERROR("Failed to create sync objects.\n");
      return -1;
    }
  }
  DBG_LOG("Successfully initialized sync objects.\n");

  // Textures
  createTextureImage(vulkanTextureImage, vulkanTextureImageMemory);
  DBG_LOG("Successfully initialized texture image objects.\n");

  DBG_LOG("Successfully initialized Vulkan.\n");
  return 0;
}

int recordCommandBuffer(uint32_t index, uint32_t frame) {
  VkCommandBufferBeginInfo cbbi = {};
  cbbi.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  cbbi.pNext = NULL;
  cbbi.flags = 0;
  if(fnBeginCommandBuffer(vulkanFrames[frame].cb, &cbbi) != VK_SUCCESS) {
    DBG_LOG("Failed to begin buffer\n");
    return -1;
  }
  VkRenderPassBeginInfo rpbi = {};
  rpbi.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
  rpbi.pNext = NULL;
  rpbi.renderPass = vulkanRenderPass;
  rpbi.framebuffer = vulkanFramebuffers[index];
  rpbi.renderArea.offset = {0, 0};
  rpbi.renderArea.extent = vulkanSwapExtent;
  rpbi.clearValueCount = 1;
  VkClearValue clearColor = {0.0f, 0.0f, 0.0f, 1.0f};
  rpbi.pClearValues = &clearColor;

  fnCmdBeginRenderPass(vulkanFrames[frame].cb, &rpbi, VK_SUBPASS_CONTENTS_INLINE);
  fnCmdBindPipeline(vulkanFrames[frame].cb, VK_PIPELINE_BIND_POINT_GRAPHICS, vulkanGraphicsPipeline);

  VkBuffer vertBuffers[1] = {vulkanVertexBuffer};
  VkDeviceSize offsets[1] = {0};
  fnCmdBindVertexBuffers(vulkanFrames[frame].cb, 0, 1, vertBuffers, offsets);

  VkViewport viewport = {};
  viewport.x = 0.0f;
  viewport.y = 0.0f;
  viewport.width = (float) vulkanSwapExtent.width;
  viewport.height = (float) vulkanSwapExtent.height;
  viewport.minDepth = 0.0f;
  viewport.maxDepth = 1.0f;
  fnCmdSetViewport(vulkanFrames[frame].cb, 0, 1, &viewport);

  VkRect2D scissor = {};
  scissor.offset = {0, 0};
  scissor.extent = vulkanSwapExtent;
  fnCmdSetScissor(vulkanFrames[frame].cb, 0, 1, &scissor);
  fnCmdBindDescriptorSets(vulkanFrames[frame].cb, VK_PIPELINE_BIND_POINT_GRAPHICS, vulkanPipelineLayout, 0, 1, &vulkanDescriptorSets[frame], 0, NULL);
  fnCmdDraw(vulkanFrames[frame].cb, VERTEX_COUNT, 1, 0, 0);

  fnCmdEndRenderPass(vulkanFrames[frame].cb);
  if(fnEndCommandBuffer(vulkanFrames[frame].cb) != VK_SUCCESS) {
    DBG_LOGERROR("Failed to write to buffer.\n");
    return -1;
  }
  return 0;
}

int drawFrame(uint32_t frame) {
  // Wait for previous frame
  fnWaitForFences(vulkanLogicalDevice, 1, &(vulkanFrames[frame].inFlight), VK_TRUE, UINT64_MAX);
  uint32_t imageIndex;
  res = fnAcquireNextImageKHR(
    vulkanLogicalDevice, vulkanSwapchain, 
    UINT64_MAX, vulkanFrames[frame].imgAvlSem, VK_NULL_HANDLE, &imageIndex);
  if(res == VK_ERROR_OUT_OF_DATE_KHR ) {
    recreateSwapchain();
    return 0;
  }
  fnResetFences(vulkanLogicalDevice, 1, &(vulkanFrames[frame].inFlight));
  fnResetCommandBuffer(vulkanFrames[frame].cb, 0);
  recordCommandBuffer(imageIndex, frame);
  // Update uniform
  ViewData vd = {};
  vd.offset = { -1.0f + (0.02f * (currentFrame % 100)), 0.0f };
  DBG_LOG("Offset: %f, %f", vd.offset[0], vd.offset[1]);
  memcpy(vulkanFrames[frame].ubMapped, &vd, sizeof(vd));

  VkSubmitInfo submitInfo = {};
  submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
  VkSemaphore waitSemaphores[1] = {vulkanFrames[frame].imgAvlSem};
  VkPipelineStageFlags waitStages[1] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
  submitInfo.pNext = NULL;
  submitInfo.waitSemaphoreCount = 1;
  submitInfo.pWaitSemaphores = waitSemaphores;
  submitInfo.pWaitDstStageMask = waitStages;
  submitInfo.commandBufferCount = 1;
  submitInfo.pCommandBuffers = &(vulkanFrames[frame].cb);
  VkSemaphore signalSemaphores[1] = {vulkanFrames[frame].rndFnsdSem};
  submitInfo.signalSemaphoreCount = 1;
  submitInfo.pSignalSemaphores = signalSemaphores;
  if(fnQueueSubmit(vulkanGraphicsQueue, 1, &submitInfo, vulkanFrames[frame].inFlight) != VK_SUCCESS) {
    DBG_LOGERROR("Failed to submit to graphics queue.\n");
    return -1;
  }
  VkPresentInfoKHR presentInfo = {};
  presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
  presentInfo.pNext = NULL;
  presentInfo.waitSemaphoreCount = 1;
  presentInfo.pWaitSemaphores = signalSemaphores;
  VkSwapchainKHR swapchains[1] = {vulkanSwapchain};
  presentInfo.swapchainCount = 1;
  presentInfo.pSwapchains = swapchains;
  presentInfo.pImageIndices = &imageIndex;
  res = fnQueuePresentKHR(vulkanPresentQueue, &presentInfo);
  if(res == VK_ERROR_OUT_OF_DATE_KHR || res == VK_SUBOPTIMAL_KHR || windowResized) {
    recreateSwapchain();
    if(windowResized) {
      windowResized = false;
    }
    return 0;
  }
  return 0;
}

void cleanupVulkan() {
  fnDestroyImage(vulkanLogicalDevice, vulkanTextureImage, NULL);
  fnFreeMemory(vulkanLogicalDevice, vulkanTextureImageMemory, NULL);
  for(uint32_t i = 0; i < FRAME_COUNT; i++) {
    fnDestroySemaphore(vulkanLogicalDevice, vulkanFrames[i].imgAvlSem, NULL);
    fnDestroySemaphore(vulkanLogicalDevice, vulkanFrames[i].rndFnsdSem, NULL);
    fnDestroyFence(vulkanLogicalDevice, vulkanFrames[i].inFlight, NULL);
    fnDestroyCommandPool(vulkanLogicalDevice, vulkanFrames[i].cp, NULL);
    fnDestroyBuffer(vulkanLogicalDevice, vulkanFrames[i].ub, NULL);
    fnFreeMemory(vulkanLogicalDevice, vulkanFrames[i].ubMem, NULL);
  }
  fnDestroyCommandPool(vulkanLogicalDevice, vulkanGlobalCB, NULL);
  fnDestroyBuffer(vulkanLogicalDevice, vulkanVertexBuffer, NULL);
  fnFreeMemory(vulkanLogicalDevice, vulkanVertexDeviceMemory, NULL);
  for(uint32_t i = 0; i < vulkanSwapchainImageCount; i++) {
    fnDestroyFramebuffer(vulkanLogicalDevice, vulkanFramebuffers[i], NULL);
  }
  free(vulkanFramebuffers);
  fnDestroyDescriptorPool(vulkanLogicalDevice, vulkanDescriptorPool, NULL);
  for(uint32_t i = 0; i < FRAME_COUNT; i++) {
    fnDestroyDescriptorSetLayout(vulkanLogicalDevice, vulkanDescriptorSetLayouts[i], NULL);
  }
  fnDestroyPipeline(vulkanLogicalDevice, vulkanGraphicsPipeline, NULL);
  fnDestroyRenderPass(vulkanLogicalDevice, vulkanRenderPass, NULL);
  fnDestroyPipelineLayout(vulkanLogicalDevice, vulkanPipelineLayout, NULL);
  fnDestroyShaderModule(vulkanLogicalDevice, vertModule, NULL);
  fnDestroyShaderModule(vulkanLogicalDevice, fragModule, NULL);
  for(uint32_t i = 0; i < vulkanSwapchainImageCount; i++) {
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
    DBG_LOGERROR("Failed to initialize.\n");
    return -1;
  };

  SDL_Event event;

  running = true;
  uint64_t perfFreq = SDL_GetPerformanceFrequency();
  uint64_t perfCountPerFrame = (perfFreq / FPS);
  recordCommandBuffer(0, currentFrame % FRAME_COUNT);
  uint64_t startTime;
  while(running) {
    startTime = SDL_GetPerformanceCounter();
    // Handle events
    while(SDL_PollEvent(&event)) {
      switch(event.type) {
        case SDL_WINDOWEVENT: {
          switch(event.window.event) {
            case SDL_WINDOWEVENT_SIZE_CHANGED: {
              DBG_LOG("Window size changed.\n");
              windowResized = true;
              break;
            }
          };
          break;
        }
        case SDL_QUIT: {
          DBG_LOG("Quitting...\n");
          running = false;
          break;
        }
        default: {
          DBG_LOG("Unhandled Event: %d\n", event.type);
          break;
        }
      }
    }
    drawFrame(currentFrame % FRAME_COUNT);
    currentFrame++;
    uint64_t counterSpent = SDL_GetPerformanceCounter() - startTime;
    DBG_LOG("Frame %d: Finished in %.2f/%.2fms\n", currentFrame, counterSpent * 1000.0f / perfFreq, perfCountPerFrame * 1000.0f / perfFreq);
    while(counterSpent < perfCountPerFrame) {
      uint64_t counterLeft = (perfCountPerFrame - counterSpent);
      uint64_t msToWait = ((1000 * counterLeft) / perfFreq);
      // TODO: Make this OS Specific and use OS Specific ways to wait exact amounts of time
      // Also benchmark how bad this spin actually is, it should be <1ms so maybe its ok?
      if(msToWait > 0) { // Wait if within ms range, otherwise just spin
        SDL_Delay(msToWait);
      };
      counterSpent = SDL_GetPerformanceCounter() - startTime;
    }
  }

  // Wait for vulkan to finish
  fnDeviceWaitIdle(vulkanLogicalDevice);
  cleanupSDL();
  return 0;
}
