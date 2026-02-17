#include <emscripten/emscripten.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <volk.h>

VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL vk_icdGetInstanceProcAddr(VkInstance instance, const char* pName);

static const uint32_t kSmokeComputeSpirv[] = {
  0x07230203u, 0x00010000u, 0x0008000bu, 0x00000012u, 0x00000000u, 0x00020011u, 0x00000001u, 0x0006000bu,
  0x00000001u, 0x4c534c47u, 0x6474732eu, 0x3035342eu, 0x00000000u, 0x0003000eu, 0x00000000u, 0x00000001u,
  0x0005000fu, 0x00000005u, 0x00000004u, 0x6e69616du, 0x00000000u, 0x00060010u, 0x00000004u, 0x00000011u,
  0x00000001u, 0x00000001u, 0x00000001u, 0x00030003u, 0x00000002u, 0x000001c2u, 0x00040005u, 0x00000004u,
  0x6e69616du, 0x00000000u, 0x00040005u, 0x00000007u, 0x4274754fu, 0x00006675u, 0x00050006u, 0x00000007u,
  0x00000000u, 0x756c6176u, 0x00000065u, 0x00040005u, 0x00000009u, 0x4274756fu, 0x00006675u, 0x00030047u,
  0x00000007u, 0x00000003u, 0x00050048u, 0x00000007u, 0x00000000u, 0x00000023u, 0x00000000u, 0x00040047u,
  0x00000009u, 0x00000021u, 0x00000000u, 0x00040047u, 0x00000009u, 0x00000022u, 0x00000000u, 0x00040047u,
  0x00000011u, 0x0000000bu, 0x00000019u, 0x00020013u, 0x00000002u, 0x00030021u, 0x00000003u, 0x00000002u,
  0x00040015u, 0x00000006u, 0x00000020u, 0x00000000u, 0x0003001eu, 0x00000007u, 0x00000006u, 0x00040020u,
  0x00000008u, 0x00000002u, 0x00000007u, 0x0004003bu, 0x00000008u, 0x00000009u, 0x00000002u, 0x00040015u,
  0x0000000au, 0x00000020u, 0x00000001u, 0x0004002bu, 0x0000000au, 0x0000000bu, 0x00000000u, 0x0004002bu,
  0x00000006u, 0x0000000cu, 0x12345678u, 0x00040020u, 0x0000000du, 0x00000002u, 0x00000006u, 0x00040017u,
  0x0000000fu, 0x00000006u, 0x00000003u, 0x0004002bu, 0x00000006u, 0x00000010u, 0x00000001u, 0x0006002cu,
  0x0000000fu, 0x00000011u, 0x00000010u, 0x00000010u, 0x00000010u, 0x00050036u, 0x00000002u, 0x00000004u,
  0x00000000u, 0x00000003u, 0x000200f8u, 0x00000005u, 0x00050041u, 0x0000000du, 0x0000000eu, 0x00000009u,
  0x0000000bu, 0x0003003eu, 0x0000000eu, 0x0000000cu, 0x000100fdu, 0x00010038u
};

static uint8_t* g_runtime_shader_spirv = 0;
static uint32_t g_runtime_shader_spirv_size = 0u;

EMSCRIPTEN_KEEPALIVE int webvulkan_set_runtime_shader_spirv(const uint8_t* bytes, uint32_t byteCount) {
  if (!bytes || byteCount < 4u || (byteCount % 4u) != 0u) {
    return -1;
  }
  if (bytes[0] != 0x03u || bytes[1] != 0x02u || bytes[2] != 0x23u || bytes[3] != 0x07u) {
    return -2;
  }

  uint8_t* copy = (uint8_t*)malloc(byteCount);
  if (!copy) {
    return -3;
  }
  memcpy(copy, bytes, byteCount);

  if (g_runtime_shader_spirv) {
    free(g_runtime_shader_spirv);
  }
  g_runtime_shader_spirv = copy;
  g_runtime_shader_spirv_size = byteCount;
  return 0;
}

static int string_contains(const char* haystack, const char* needle) {
  return haystack && needle && strstr(haystack, needle) != 0;
}

static uint32_t find_compute_queue_family(VkPhysicalDevice physicalDevice) {
  uint32_t queueFamilyCount = 0u;
  vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, 0);
  if (queueFamilyCount == 0u) {
    return UINT32_MAX;
  }

  VkQueueFamilyProperties* queueFamilies =
    (VkQueueFamilyProperties*)malloc(sizeof(VkQueueFamilyProperties) * queueFamilyCount);
  if (!queueFamilies) {
    return UINT32_MAX;
  }

  vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, queueFamilies);

  uint32_t queueFamilyIndex = UINT32_MAX;
  for (uint32_t i = 0u; i < queueFamilyCount; ++i) {
    if (queueFamilies[i].queueCount > 0u && (queueFamilies[i].queueFlags & VK_QUEUE_COMPUTE_BIT) != 0u) {
      queueFamilyIndex = i;
      break;
    }
  }

  free(queueFamilies);
  return queueFamilyIndex;
}

static uint32_t find_memory_type_index(
  const VkPhysicalDeviceMemoryProperties* memoryProperties,
  uint32_t memoryTypeBits,
  VkMemoryPropertyFlags requiredFlags,
  VkMemoryPropertyFlags preferredFlags,
  int* hasPreferredFlags
) {
  uint32_t fallbackIndex = UINT32_MAX;
  if (hasPreferredFlags) {
    *hasPreferredFlags = 0;
  }

  for (uint32_t i = 0u; i < memoryProperties->memoryTypeCount; ++i) {
    const uint32_t bit = (1u << i);
    if ((memoryTypeBits & bit) == 0u) {
      continue;
    }
    const VkMemoryPropertyFlags flags = memoryProperties->memoryTypes[i].propertyFlags;
    if ((flags & requiredFlags) != requiredFlags) {
      continue;
    }
    if ((flags & preferredFlags) == preferredFlags) {
      if (hasPreferredFlags) {
        *hasPreferredFlags = 1;
      }
      return i;
    }
    if (fallbackIndex == UINT32_MAX) {
      fallbackIndex = i;
    }
  }

  return fallbackIndex;
}

EMSCRIPTEN_KEEPALIVE int lavapipe_runtime_smoke(void) {
  printf("lavapipe runtime smoke stage=begin\n");
  fflush(stdout);
  int smokeRc = 0;
  VkResult rc = VK_SUCCESS;
  VkInstance instance = VK_NULL_HANDLE;
  VkDevice device = VK_NULL_HANDLE;
  VkShaderModule shaderModule = VK_NULL_HANDLE;
  VkDescriptorSetLayout descriptorSetLayout = VK_NULL_HANDLE;
  VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
  VkDescriptorSet descriptorSet = VK_NULL_HANDLE;
  VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
  VkPipeline computePipeline = VK_NULL_HANDLE;
  VkBuffer storageBuffer = VK_NULL_HANDLE;
  VkDeviceMemory storageMemory = VK_NULL_HANDLE;
  uint32_t* mappedStorageWord = 0;
  VkCommandPool commandPool = VK_NULL_HANDLE;
  VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
  VkFence submitFence = VK_NULL_HANDLE;
  VkQueue queue = VK_NULL_HANDLE;
  uint32_t dispatchObservedValue = 0u;
  const uint32_t* shaderCodeWords = kSmokeComputeSpirv;
  size_t shaderCodeSizeBytes = sizeof(kSmokeComputeSpirv);
  const char* shaderSource = "embedded_static_spirv";
  const char* shaderEntryPoint = "main";
  PFN_vkDestroyDevice pfnDestroyDevice = 0;
  PFN_vkGetPhysicalDeviceMemoryProperties pfnGetPhysicalDeviceMemoryProperties = 0;
  PFN_vkCreateShaderModule pfnCreateShaderModule = 0;
  PFN_vkDestroyShaderModule pfnDestroyShaderModule = 0;
  PFN_vkCreateDescriptorSetLayout pfnCreateDescriptorSetLayout = 0;
  PFN_vkDestroyDescriptorSetLayout pfnDestroyDescriptorSetLayout = 0;
  PFN_vkCreateDescriptorPool pfnCreateDescriptorPool = 0;
  PFN_vkDestroyDescriptorPool pfnDestroyDescriptorPool = 0;
  PFN_vkAllocateDescriptorSets pfnAllocateDescriptorSets = 0;
  PFN_vkUpdateDescriptorSets pfnUpdateDescriptorSets = 0;
  PFN_vkCreatePipelineLayout pfnCreatePipelineLayout = 0;
  PFN_vkDestroyPipelineLayout pfnDestroyPipelineLayout = 0;
  PFN_vkCreateComputePipelines pfnCreateComputePipelines = 0;
  PFN_vkDestroyPipeline pfnDestroyPipeline = 0;
  PFN_vkCreateBuffer pfnCreateBuffer = 0;
  PFN_vkDestroyBuffer pfnDestroyBuffer = 0;
  PFN_vkGetBufferMemoryRequirements pfnGetBufferMemoryRequirements = 0;
  PFN_vkAllocateMemory pfnAllocateMemory = 0;
  PFN_vkFreeMemory pfnFreeMemory = 0;
  PFN_vkBindBufferMemory pfnBindBufferMemory = 0;
  PFN_vkMapMemory pfnMapMemory = 0;
  PFN_vkUnmapMemory pfnUnmapMemory = 0;
  PFN_vkGetDeviceQueue pfnGetDeviceQueue = 0;
  PFN_vkCreateCommandPool pfnCreateCommandPool = 0;
  PFN_vkDestroyCommandPool pfnDestroyCommandPool = 0;
  PFN_vkAllocateCommandBuffers pfnAllocateCommandBuffers = 0;
  PFN_vkBeginCommandBuffer pfnBeginCommandBuffer = 0;
  PFN_vkEndCommandBuffer pfnEndCommandBuffer = 0;
  PFN_vkCmdBindPipeline pfnCmdBindPipeline = 0;
  PFN_vkCmdBindDescriptorSets pfnCmdBindDescriptorSets = 0;
  PFN_vkCmdDispatch pfnCmdDispatch = 0;
  PFN_vkCreateFence pfnCreateFence = 0;
  PFN_vkDestroyFence pfnDestroyFence = 0;
  PFN_vkQueueSubmit pfnQueueSubmit = 0;
  PFN_vkWaitForFences pfnWaitForFences = 0;


  volkInitializeCustom((PFN_vkGetInstanceProcAddr)vk_icdGetInstanceProcAddr);
  if (!vkCreateInstance || !vkEnumerateInstanceVersion) {
    smokeRc = 21;
    goto cleanup;
  }

  uint32_t apiVersion = 0u;
  rc = vkEnumerateInstanceVersion(&apiVersion);
  if (rc != VK_SUCCESS) {
    smokeRc = 22;
    goto cleanup;
  }

  VkApplicationInfo appInfo;
  memset(&appInfo, 0, sizeof(appInfo));
  appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
  appInfo.pApplicationName = "lavapipe_runtime_smoke";
  appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
  appInfo.pEngineName = "webvulkan";
  appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
  appInfo.apiVersion = apiVersion;

  VkInstanceCreateInfo createInfo;
  memset(&createInfo, 0, sizeof(createInfo));
  createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
  createInfo.pApplicationInfo = &appInfo;

  rc = vkCreateInstance(&createInfo, 0, &instance);
  printf("lavapipe runtime smoke stage=after_vkCreateInstance rc=%d\n", (int)rc);
  fflush(stdout);
  if (rc != VK_SUCCESS || instance == VK_NULL_HANDLE) {
    smokeRc = 23;
    goto cleanup;
  }

  volkLoadInstance(instance);
  if (!vkEnumeratePhysicalDevices || !vkDestroyInstance || !vkGetDeviceProcAddr || !vkCreateDevice ||
      !vkGetPhysicalDeviceQueueFamilyProperties) {
    smokeRc = 24;
    goto cleanup;
  }

  uint32_t physicalDeviceCount = 0u;
  rc = vkEnumeratePhysicalDevices(instance, &physicalDeviceCount, 0);
  printf("lavapipe runtime smoke stage=after_vkEnumeratePhysicalDevices_count rc=%d count=%u\n", (int)rc, physicalDeviceCount);
  fflush(stdout);
  if (rc != VK_SUCCESS || physicalDeviceCount == 0u) {
    smokeRc = 25;
    goto cleanup;
  }

  VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
  uint32_t one = 1u;
  rc = vkEnumeratePhysicalDevices(instance, &one, &physicalDevice);
  if (rc != VK_SUCCESS || one == 0u || physicalDevice == VK_NULL_HANDLE) {
    smokeRc = 26;
    goto cleanup;
  }

  PFN_vkGetPhysicalDeviceProperties2 getPhysicalDeviceProperties2 = vkGetPhysicalDeviceProperties2;
  if (!getPhysicalDeviceProperties2) {
    getPhysicalDeviceProperties2 =
      (PFN_vkGetPhysicalDeviceProperties2)vkGetInstanceProcAddr(instance, "vkGetPhysicalDeviceProperties2KHR");
  }
  if (!getPhysicalDeviceProperties2) {
    smokeRc = 27;
    goto cleanup;
  }

  VkPhysicalDeviceDriverProperties driverProps;
  memset(&driverProps, 0, sizeof(driverProps));
  driverProps.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DRIVER_PROPERTIES;

  VkPhysicalDeviceProperties2 props2;
  memset(&props2, 0, sizeof(props2));
  props2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
  props2.pNext = &driverProps;
  getPhysicalDeviceProperties2(physicalDevice, &props2);
  VkPhysicalDeviceProperties props = props2.properties;

  const int proofDeviceName = string_contains(props.deviceName, "llvmpipe");
  const int proofDriverName = string_contains(driverProps.driverName, "llvmpipe");
  if (!proofDeviceName || !proofDriverName) {
    smokeRc = 28;
    goto cleanup;
  }

  uint32_t queueFamilyIndex = find_compute_queue_family(physicalDevice);
  if (queueFamilyIndex == UINT32_MAX) {
    smokeRc = 29;
    goto cleanup;
  }

  float queuePriority = 1.0f;
  VkDeviceQueueCreateInfo queueCreateInfo;
  memset(&queueCreateInfo, 0, sizeof(queueCreateInfo));
  queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
  queueCreateInfo.queueFamilyIndex = queueFamilyIndex;
  queueCreateInfo.queueCount = 1u;
  queueCreateInfo.pQueuePriorities = &queuePriority;

  VkDeviceCreateInfo deviceCreateInfo;
  memset(&deviceCreateInfo, 0, sizeof(deviceCreateInfo));
  deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
  deviceCreateInfo.queueCreateInfoCount = 1u;
  deviceCreateInfo.pQueueCreateInfos = &queueCreateInfo;
  VkPhysicalDeviceFeatures enabledFeatures;
  memset(&enabledFeatures, 0, sizeof(enabledFeatures));
  deviceCreateInfo.pEnabledFeatures = &enabledFeatures;

  rc = vkCreateDevice(physicalDevice, &deviceCreateInfo, 0, &device);
  printf("lavapipe runtime smoke stage=after_vkCreateDevice rc=%d\n", (int)rc);
  fflush(stdout);
  if (rc != VK_SUCCESS || device == VK_NULL_HANDLE) {
    smokeRc = 30;
    goto cleanup;
  }

  volkLoadDevice(device);
  pfnGetPhysicalDeviceMemoryProperties =
    vkGetPhysicalDeviceMemoryProperties ? vkGetPhysicalDeviceMemoryProperties :
    (PFN_vkGetPhysicalDeviceMemoryProperties)vkGetInstanceProcAddr(instance, "vkGetPhysicalDeviceMemoryProperties");
  pfnDestroyDevice = vkDestroyDevice ? vkDestroyDevice : (PFN_vkDestroyDevice)vkGetDeviceProcAddr(device, "vkDestroyDevice");
  pfnCreateShaderModule = vkCreateShaderModule ? vkCreateShaderModule :
                                                (PFN_vkCreateShaderModule)vkGetDeviceProcAddr(device, "vkCreateShaderModule");
  pfnDestroyShaderModule = vkDestroyShaderModule ? vkDestroyShaderModule :
                                                  (PFN_vkDestroyShaderModule)vkGetDeviceProcAddr(device, "vkDestroyShaderModule");
  pfnCreateDescriptorSetLayout = vkCreateDescriptorSetLayout ? vkCreateDescriptorSetLayout :
                                      (PFN_vkCreateDescriptorSetLayout)vkGetDeviceProcAddr(device, "vkCreateDescriptorSetLayout");
  pfnDestroyDescriptorSetLayout = vkDestroyDescriptorSetLayout ? vkDestroyDescriptorSetLayout :
                                       (PFN_vkDestroyDescriptorSetLayout)vkGetDeviceProcAddr(device, "vkDestroyDescriptorSetLayout");
  pfnCreateDescriptorPool = vkCreateDescriptorPool ? vkCreateDescriptorPool :
                            (PFN_vkCreateDescriptorPool)vkGetDeviceProcAddr(device, "vkCreateDescriptorPool");
  pfnDestroyDescriptorPool = vkDestroyDescriptorPool ? vkDestroyDescriptorPool :
                             (PFN_vkDestroyDescriptorPool)vkGetDeviceProcAddr(device, "vkDestroyDescriptorPool");
  pfnAllocateDescriptorSets = vkAllocateDescriptorSets ? vkAllocateDescriptorSets :
                              (PFN_vkAllocateDescriptorSets)vkGetDeviceProcAddr(device, "vkAllocateDescriptorSets");
  pfnUpdateDescriptorSets = vkUpdateDescriptorSets ? vkUpdateDescriptorSets :
                            (PFN_vkUpdateDescriptorSets)vkGetDeviceProcAddr(device, "vkUpdateDescriptorSets");
  pfnCreatePipelineLayout = vkCreatePipelineLayout ? vkCreatePipelineLayout :
                                                   (PFN_vkCreatePipelineLayout)vkGetDeviceProcAddr(device, "vkCreatePipelineLayout");
  pfnDestroyPipelineLayout = vkDestroyPipelineLayout ? vkDestroyPipelineLayout :
                                                     (PFN_vkDestroyPipelineLayout)vkGetDeviceProcAddr(device, "vkDestroyPipelineLayout");
  pfnCreateComputePipelines = vkCreateComputePipelines ? vkCreateComputePipelines :
                                                      (PFN_vkCreateComputePipelines)vkGetDeviceProcAddr(device, "vkCreateComputePipelines");
  pfnDestroyPipeline = vkDestroyPipeline ? vkDestroyPipeline :
                                          (PFN_vkDestroyPipeline)vkGetDeviceProcAddr(device, "vkDestroyPipeline");
  pfnCreateBuffer = vkCreateBuffer ? vkCreateBuffer :
                    (PFN_vkCreateBuffer)vkGetDeviceProcAddr(device, "vkCreateBuffer");
  pfnDestroyBuffer = vkDestroyBuffer ? vkDestroyBuffer :
                     (PFN_vkDestroyBuffer)vkGetDeviceProcAddr(device, "vkDestroyBuffer");
  pfnGetBufferMemoryRequirements = vkGetBufferMemoryRequirements ? vkGetBufferMemoryRequirements :
                                   (PFN_vkGetBufferMemoryRequirements)vkGetDeviceProcAddr(device, "vkGetBufferMemoryRequirements");
  pfnAllocateMemory = vkAllocateMemory ? vkAllocateMemory :
                      (PFN_vkAllocateMemory)vkGetDeviceProcAddr(device, "vkAllocateMemory");
  pfnFreeMemory = vkFreeMemory ? vkFreeMemory :
                  (PFN_vkFreeMemory)vkGetDeviceProcAddr(device, "vkFreeMemory");
  pfnBindBufferMemory = vkBindBufferMemory ? vkBindBufferMemory :
                        (PFN_vkBindBufferMemory)vkGetDeviceProcAddr(device, "vkBindBufferMemory");
  pfnMapMemory = vkMapMemory ? vkMapMemory :
                 (PFN_vkMapMemory)vkGetDeviceProcAddr(device, "vkMapMemory");
  pfnUnmapMemory = vkUnmapMemory ? vkUnmapMemory :
                   (PFN_vkUnmapMemory)vkGetDeviceProcAddr(device, "vkUnmapMemory");
  pfnGetDeviceQueue = vkGetDeviceQueue ? vkGetDeviceQueue :
                                       (PFN_vkGetDeviceQueue)vkGetDeviceProcAddr(device, "vkGetDeviceQueue");
  pfnCreateCommandPool = vkCreateCommandPool ? vkCreateCommandPool :
                                            (PFN_vkCreateCommandPool)vkGetDeviceProcAddr(device, "vkCreateCommandPool");
  pfnDestroyCommandPool = vkDestroyCommandPool ? vkDestroyCommandPool :
                                             (PFN_vkDestroyCommandPool)vkGetDeviceProcAddr(device, "vkDestroyCommandPool");
  pfnAllocateCommandBuffers = vkAllocateCommandBuffers ? vkAllocateCommandBuffers :
                                                (PFN_vkAllocateCommandBuffers)vkGetDeviceProcAddr(device, "vkAllocateCommandBuffers");
  pfnBeginCommandBuffer = vkBeginCommandBuffer ? vkBeginCommandBuffer :
                                              (PFN_vkBeginCommandBuffer)vkGetDeviceProcAddr(device, "vkBeginCommandBuffer");
  pfnEndCommandBuffer = vkEndCommandBuffer ? vkEndCommandBuffer :
                                          (PFN_vkEndCommandBuffer)vkGetDeviceProcAddr(device, "vkEndCommandBuffer");
  pfnCmdBindPipeline = vkCmdBindPipeline ? vkCmdBindPipeline :
                                        (PFN_vkCmdBindPipeline)vkGetDeviceProcAddr(device, "vkCmdBindPipeline");
  pfnCmdBindDescriptorSets = vkCmdBindDescriptorSets ? vkCmdBindDescriptorSets :
                             (PFN_vkCmdBindDescriptorSets)vkGetDeviceProcAddr(device, "vkCmdBindDescriptorSets");
  pfnCmdDispatch = vkCmdDispatch ? vkCmdDispatch :
                                (PFN_vkCmdDispatch)vkGetDeviceProcAddr(device, "vkCmdDispatch");
  pfnCreateFence = vkCreateFence ? vkCreateFence :
                                (PFN_vkCreateFence)vkGetDeviceProcAddr(device, "vkCreateFence");
  pfnDestroyFence = vkDestroyFence ? vkDestroyFence :
                                  (PFN_vkDestroyFence)vkGetDeviceProcAddr(device, "vkDestroyFence");
  pfnQueueSubmit = vkQueueSubmit ? vkQueueSubmit :
                                (PFN_vkQueueSubmit)vkGetDeviceProcAddr(device, "vkQueueSubmit");
  pfnWaitForFences = vkWaitForFences ? vkWaitForFences :
                                    (PFN_vkWaitForFences)vkGetDeviceProcAddr(device, "vkWaitForFences");

  if (!pfnDestroyDevice || !pfnGetPhysicalDeviceMemoryProperties ||
      !pfnCreateShaderModule || !pfnDestroyShaderModule ||
      !pfnCreatePipelineLayout || !pfnDestroyPipelineLayout ||
      !pfnCreateComputePipelines || !pfnDestroyPipeline ||
      !pfnCreateDescriptorSetLayout || !pfnDestroyDescriptorSetLayout ||
      !pfnCreateDescriptorPool || !pfnDestroyDescriptorPool ||
      !pfnAllocateDescriptorSets || !pfnUpdateDescriptorSets ||
      !pfnCreateBuffer || !pfnDestroyBuffer ||
      !pfnGetBufferMemoryRequirements || !pfnAllocateMemory || !pfnFreeMemory ||
      !pfnBindBufferMemory || !pfnMapMemory || !pfnUnmapMemory) {
    printf("lavapipe runtime smoke missing device entrypoints\n");
    printf("  vkGetPhysicalDeviceMemoryProperties=%s\n", pfnGetPhysicalDeviceMemoryProperties ? "present" : "missing");
    printf("  vkDestroyDevice=%s\n", pfnDestroyDevice ? "present" : "missing");
    printf("  vkCreateShaderModule=%s\n", pfnCreateShaderModule ? "present" : "missing");
    printf("  vkDestroyShaderModule=%s\n", pfnDestroyShaderModule ? "present" : "missing");
    printf("  vkCreateDescriptorSetLayout=%s\n", pfnCreateDescriptorSetLayout ? "present" : "missing");
    printf("  vkDestroyDescriptorSetLayout=%s\n", pfnDestroyDescriptorSetLayout ? "present" : "missing");
    printf("  vkCreateDescriptorPool=%s\n", pfnCreateDescriptorPool ? "present" : "missing");
    printf("  vkDestroyDescriptorPool=%s\n", pfnDestroyDescriptorPool ? "present" : "missing");
    printf("  vkAllocateDescriptorSets=%s\n", pfnAllocateDescriptorSets ? "present" : "missing");
    printf("  vkUpdateDescriptorSets=%s\n", pfnUpdateDescriptorSets ? "present" : "missing");
    printf("  vkCreateBuffer=%s\n", pfnCreateBuffer ? "present" : "missing");
    printf("  vkDestroyBuffer=%s\n", pfnDestroyBuffer ? "present" : "missing");
    printf("  vkGetBufferMemoryRequirements=%s\n", pfnGetBufferMemoryRequirements ? "present" : "missing");
    printf("  vkAllocateMemory=%s\n", pfnAllocateMemory ? "present" : "missing");
    printf("  vkFreeMemory=%s\n", pfnFreeMemory ? "present" : "missing");
    printf("  vkBindBufferMemory=%s\n", pfnBindBufferMemory ? "present" : "missing");
    printf("  vkMapMemory=%s\n", pfnMapMemory ? "present" : "missing");
    printf("  vkUnmapMemory=%s\n", pfnUnmapMemory ? "present" : "missing");
    printf("  vkCreatePipelineLayout=%s\n", pfnCreatePipelineLayout ? "present" : "missing");
    printf("  vkDestroyPipelineLayout=%s\n", pfnDestroyPipelineLayout ? "present" : "missing");
    printf("  vkCreateComputePipelines=%s\n", pfnCreateComputePipelines ? "present" : "missing");
    printf("  vkDestroyPipeline=%s\n", pfnDestroyPipeline ? "present" : "missing");
    smokeRc = 31;
    goto cleanup;
  }
  if (!pfnGetDeviceQueue || !pfnCreateCommandPool || !pfnDestroyCommandPool || !pfnAllocateCommandBuffers ||
      !pfnBeginCommandBuffer || !pfnEndCommandBuffer || !pfnCmdBindPipeline || !pfnCmdBindDescriptorSets || !pfnCmdDispatch ||
      !pfnCreateFence || !pfnDestroyFence || !pfnQueueSubmit || !pfnWaitForFences) {
    printf("lavapipe runtime smoke missing dispatch entrypoints\n");
    printf("  vkGetDeviceQueue=%s\n", pfnGetDeviceQueue ? "present" : "missing");
    printf("  vkCreateCommandPool=%s\n", pfnCreateCommandPool ? "present" : "missing");
    printf("  vkDestroyCommandPool=%s\n", pfnDestroyCommandPool ? "present" : "missing");
    printf("  vkAllocateCommandBuffers=%s\n", pfnAllocateCommandBuffers ? "present" : "missing");
    printf("  vkBeginCommandBuffer=%s\n", pfnBeginCommandBuffer ? "present" : "missing");
    printf("  vkEndCommandBuffer=%s\n", pfnEndCommandBuffer ? "present" : "missing");
    printf("  vkCmdBindPipeline=%s\n", pfnCmdBindPipeline ? "present" : "missing");
    printf("  vkCmdBindDescriptorSets=%s\n", pfnCmdBindDescriptorSets ? "present" : "missing");
    printf("  vkCmdDispatch=%s\n", pfnCmdDispatch ? "present" : "missing");
    printf("  vkCreateFence=%s\n", pfnCreateFence ? "present" : "missing");
    printf("  vkDestroyFence=%s\n", pfnDestroyFence ? "present" : "missing");
    printf("  vkQueueSubmit=%s\n", pfnQueueSubmit ? "present" : "missing");
    printf("  vkWaitForFences=%s\n", pfnWaitForFences ? "present" : "missing");
    smokeRc = 36;
    goto cleanup;
  }

  if (g_runtime_shader_spirv && g_runtime_shader_spirv_size >= 4u && (g_runtime_shader_spirv_size % 4u) == 0u) {
    shaderCodeWords = (const uint32_t*)g_runtime_shader_spirv;
    shaderCodeSizeBytes = (size_t)g_runtime_shader_spirv_size;
    shaderSource = "runtime_injected_spirv";
    shaderEntryPoint = "write_const";
  }

  VkShaderModuleCreateInfo shaderCreateInfo;
  memset(&shaderCreateInfo, 0, sizeof(shaderCreateInfo));
  shaderCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
  shaderCreateInfo.codeSize = shaderCodeSizeBytes;
  shaderCreateInfo.pCode = shaderCodeWords;

  rc = pfnCreateShaderModule(device, &shaderCreateInfo, 0, &shaderModule);
  printf("lavapipe runtime smoke stage=after_vkCreateShaderModule rc=%d\n", (int)rc);
  fflush(stdout);
  if (rc != VK_SUCCESS || shaderModule == VK_NULL_HANDLE) {
    smokeRc = 32;
    goto cleanup;
  }

  VkDescriptorSetLayoutBinding descriptorSetLayoutBinding;
  memset(&descriptorSetLayoutBinding, 0, sizeof(descriptorSetLayoutBinding));
  descriptorSetLayoutBinding.binding = 0u;
  descriptorSetLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
  descriptorSetLayoutBinding.descriptorCount = 1u;
  descriptorSetLayoutBinding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

  VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo;
  memset(&descriptorSetLayoutCreateInfo, 0, sizeof(descriptorSetLayoutCreateInfo));
  descriptorSetLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
  descriptorSetLayoutCreateInfo.bindingCount = 1u;
  descriptorSetLayoutCreateInfo.pBindings = &descriptorSetLayoutBinding;

  rc = pfnCreateDescriptorSetLayout(device, &descriptorSetLayoutCreateInfo, 0, &descriptorSetLayout);
  if (rc != VK_SUCCESS || descriptorSetLayout == VK_NULL_HANDLE) {
    smokeRc = 37;
    goto cleanup;
  }

  VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo;
  memset(&pipelineLayoutCreateInfo, 0, sizeof(pipelineLayoutCreateInfo));
  pipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
  pipelineLayoutCreateInfo.setLayoutCount = 1u;
  pipelineLayoutCreateInfo.pSetLayouts = &descriptorSetLayout;

  rc = pfnCreatePipelineLayout(device, &pipelineLayoutCreateInfo, 0, &pipelineLayout);
  if (rc != VK_SUCCESS || pipelineLayout == VK_NULL_HANDLE) {
    smokeRc = 33;
    goto cleanup;
  }

  VkPipelineShaderStageCreateInfo shaderStage;
  memset(&shaderStage, 0, sizeof(shaderStage));
  shaderStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  shaderStage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
  shaderStage.module = shaderModule;
  shaderStage.pName = shaderEntryPoint;

  VkComputePipelineCreateInfo pipelineCreateInfo;
  memset(&pipelineCreateInfo, 0, sizeof(pipelineCreateInfo));
  pipelineCreateInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
  pipelineCreateInfo.stage = shaderStage;
  pipelineCreateInfo.layout = pipelineLayout;

  rc = pfnCreateComputePipelines(device, VK_NULL_HANDLE, 1u, &pipelineCreateInfo, 0, &computePipeline);
  printf("lavapipe runtime smoke stage=after_vkCreateComputePipelines rc=%d\n", (int)rc);
  fflush(stdout);
  if (rc != VK_SUCCESS || computePipeline == VK_NULL_HANDLE) {
    smokeRc = 34;
    goto cleanup;
  }

  pfnGetDeviceQueue(device, queueFamilyIndex, 0u, &queue);
  if (queue == VK_NULL_HANDLE) {
    smokeRc = 61;
    goto cleanup;
  }

  VkPhysicalDeviceMemoryProperties memoryProperties;
  memset(&memoryProperties, 0, sizeof(memoryProperties));
  pfnGetPhysicalDeviceMemoryProperties(physicalDevice, &memoryProperties);

  VkBufferCreateInfo bufferCreateInfo;
  memset(&bufferCreateInfo, 0, sizeof(bufferCreateInfo));
  bufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
  bufferCreateInfo.size = sizeof(uint32_t);
  bufferCreateInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
  bufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

  rc = pfnCreateBuffer(device, &bufferCreateInfo, 0, &storageBuffer);
  if (rc != VK_SUCCESS || storageBuffer == VK_NULL_HANDLE) {
    smokeRc = 58;
    goto cleanup;
  }

  VkMemoryRequirements bufferMemoryRequirements;
  memset(&bufferMemoryRequirements, 0, sizeof(bufferMemoryRequirements));
  pfnGetBufferMemoryRequirements(device, storageBuffer, &bufferMemoryRequirements);

  int hasHostCoherentMemory = 0;
  uint32_t memoryTypeIndex = find_memory_type_index(
    &memoryProperties,
    bufferMemoryRequirements.memoryTypeBits,
    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
    VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
    &hasHostCoherentMemory
  );
  if (memoryTypeIndex == UINT32_MAX || !hasHostCoherentMemory) {
    smokeRc = 59;
    goto cleanup;
  }

  VkMemoryAllocateInfo memoryAllocateInfo;
  memset(&memoryAllocateInfo, 0, sizeof(memoryAllocateInfo));
  memoryAllocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
  memoryAllocateInfo.allocationSize = bufferMemoryRequirements.size;
  memoryAllocateInfo.memoryTypeIndex = memoryTypeIndex;

  rc = pfnAllocateMemory(device, &memoryAllocateInfo, 0, &storageMemory);
  if (rc != VK_SUCCESS || storageMemory == VK_NULL_HANDLE) {
    smokeRc = 60;
    goto cleanup;
  }

  rc = pfnBindBufferMemory(device, storageBuffer, storageMemory, 0u);
  if (rc != VK_SUCCESS) {
    smokeRc = 69;
    goto cleanup;
  }

  rc = pfnMapMemory(device, storageMemory, 0u, sizeof(uint32_t), 0u, (void**)&mappedStorageWord);
  if (rc != VK_SUCCESS || !mappedStorageWord) {
    smokeRc = 70;
    goto cleanup;
  }
  mappedStorageWord[0] = 0u;

  VkDescriptorPoolSize descriptorPoolSize;
  memset(&descriptorPoolSize, 0, sizeof(descriptorPoolSize));
  descriptorPoolSize.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
  descriptorPoolSize.descriptorCount = 1u;

  VkDescriptorPoolCreateInfo descriptorPoolCreateInfo;
  memset(&descriptorPoolCreateInfo, 0, sizeof(descriptorPoolCreateInfo));
  descriptorPoolCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
  descriptorPoolCreateInfo.maxSets = 1u;
  descriptorPoolCreateInfo.poolSizeCount = 1u;
  descriptorPoolCreateInfo.pPoolSizes = &descriptorPoolSize;

  rc = pfnCreateDescriptorPool(device, &descriptorPoolCreateInfo, 0, &descriptorPool);
  if (rc != VK_SUCCESS || descriptorPool == VK_NULL_HANDLE) {
    smokeRc = 71;
    goto cleanup;
  }

  VkDescriptorSetAllocateInfo descriptorSetAllocateInfo;
  memset(&descriptorSetAllocateInfo, 0, sizeof(descriptorSetAllocateInfo));
  descriptorSetAllocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
  descriptorSetAllocateInfo.descriptorPool = descriptorPool;
  descriptorSetAllocateInfo.descriptorSetCount = 1u;
  descriptorSetAllocateInfo.pSetLayouts = &descriptorSetLayout;

  rc = pfnAllocateDescriptorSets(device, &descriptorSetAllocateInfo, &descriptorSet);
  if (rc != VK_SUCCESS || descriptorSet == VK_NULL_HANDLE) {
    smokeRc = 72;
    goto cleanup;
  }

  VkDescriptorBufferInfo descriptorBufferInfo;
  memset(&descriptorBufferInfo, 0, sizeof(descriptorBufferInfo));
  descriptorBufferInfo.buffer = storageBuffer;
  descriptorBufferInfo.offset = 0u;
  descriptorBufferInfo.range = sizeof(uint32_t);

  VkWriteDescriptorSet writeDescriptorSet;
  memset(&writeDescriptorSet, 0, sizeof(writeDescriptorSet));
  writeDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  writeDescriptorSet.dstSet = descriptorSet;
  writeDescriptorSet.dstBinding = 0u;
  writeDescriptorSet.descriptorCount = 1u;
  writeDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
  writeDescriptorSet.pBufferInfo = &descriptorBufferInfo;

  pfnUpdateDescriptorSets(device, 1u, &writeDescriptorSet, 0u, 0);

  VkCommandPoolCreateInfo commandPoolCreateInfo;
  memset(&commandPoolCreateInfo, 0, sizeof(commandPoolCreateInfo));
  commandPoolCreateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
  commandPoolCreateInfo.queueFamilyIndex = queueFamilyIndex;
  commandPoolCreateInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

  rc = pfnCreateCommandPool(device, &commandPoolCreateInfo, 0, &commandPool);
  if (rc != VK_SUCCESS || commandPool == VK_NULL_HANDLE) {
    smokeRc = 62;
    goto cleanup;
  }

  VkCommandBufferAllocateInfo commandBufferAllocateInfo;
  memset(&commandBufferAllocateInfo, 0, sizeof(commandBufferAllocateInfo));
  commandBufferAllocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  commandBufferAllocateInfo.commandPool = commandPool;
  commandBufferAllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  commandBufferAllocateInfo.commandBufferCount = 1u;

  rc = pfnAllocateCommandBuffers(device, &commandBufferAllocateInfo, &commandBuffer);
  if (rc != VK_SUCCESS || commandBuffer == VK_NULL_HANDLE) {
    smokeRc = 63;
    goto cleanup;
  }

  VkCommandBufferBeginInfo commandBufferBeginInfo;
  memset(&commandBufferBeginInfo, 0, sizeof(commandBufferBeginInfo));
  commandBufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  commandBufferBeginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

  rc = pfnBeginCommandBuffer(commandBuffer, &commandBufferBeginInfo);
  if (rc != VK_SUCCESS) {
    smokeRc = 64;
    goto cleanup;
  }

  pfnCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, computePipeline);
  pfnCmdBindDescriptorSets(
    commandBuffer,
    VK_PIPELINE_BIND_POINT_COMPUTE,
    pipelineLayout,
    0u,
    1u,
    &descriptorSet,
    0u,
    0
  );
  pfnCmdDispatch(commandBuffer, 1u, 1u, 1u);

  rc = pfnEndCommandBuffer(commandBuffer);
  if (rc != VK_SUCCESS) {
    smokeRc = 65;
    goto cleanup;
  }

  VkFenceCreateInfo fenceCreateInfo;
  memset(&fenceCreateInfo, 0, sizeof(fenceCreateInfo));
  fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
  rc = pfnCreateFence(device, &fenceCreateInfo, 0, &submitFence);
  if (rc != VK_SUCCESS || submitFence == VK_NULL_HANDLE) {
    smokeRc = 66;
    goto cleanup;
  }

  VkSubmitInfo submitInfo;
  memset(&submitInfo, 0, sizeof(submitInfo));
  submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
  submitInfo.commandBufferCount = 1u;
  submitInfo.pCommandBuffers = &commandBuffer;

  rc = pfnQueueSubmit(queue, 1u, &submitInfo, submitFence);
  if (rc != VK_SUCCESS) {
    smokeRc = 67;
    goto cleanup;
  }

  rc = pfnWaitForFences(device, 1u, &submitFence, VK_TRUE, UINT64_MAX);
  if (rc != VK_SUCCESS) {
    smokeRc = 68;
    goto cleanup;
  }

  dispatchObservedValue = mappedStorageWord[0];
  if (dispatchObservedValue != 0x12345678u) {
    printf("lavapipe runtime smoke dispatch mismatch\n");
    printf("  shader.dispatch.expected=0x%08x\n", 0x12345678u);
    printf("  shader.dispatch.observed=0x%08x\n", dispatchObservedValue);
    smokeRc = 73;
    goto cleanup;
  }

  printf("lavapipe runtime smoke ok\n");
  printf("  backend=mesa lavapipe (swrast)\n");
  printf("  instance.api=%u.%u.%u (%u)\n",
         VK_API_VERSION_MAJOR(apiVersion),
         VK_API_VERSION_MINOR(apiVersion),
         VK_API_VERSION_PATCH(apiVersion),
         apiVersion);
  printf("  vkCreateInstance=ok\n");
  printf("  vkEnumeratePhysicalDevices=ok\n");
  printf("  physical_devices=%u\n", physicalDeviceCount);
  printf("  device.name=%s\n", props.deviceName);
  printf("  device.vendor=0x%04x device=0x%04x\n", props.vendorID, props.deviceID);
  printf("  device.api=%u.%u.%u\n",
         VK_API_VERSION_MAJOR(props.apiVersion),
         VK_API_VERSION_MINOR(props.apiVersion),
         VK_API_VERSION_PATCH(props.apiVersion));
  printf("  driver.name=%s\n", driverProps.driverName);
  printf("  driver.info=%s\n", driverProps.driverInfo);
  printf("  proof.device_name_contains_llvmpipe=%s\n", proofDeviceName ? "yes" : "no");
  printf("  proof.driver_name_contains_llvmpipe=%s\n", proofDriverName ? "yes" : "no");
  printf("  vulkan_loader=volk (custom vk_icdGetInstanceProcAddr)\n");
  printf("  shader.source=%s\n", shaderSource);
  printf("  shader.entrypoint=%s\n", shaderEntryPoint);
  printf("  shader.code_bytes=%u\n", (unsigned)shaderCodeSizeBytes);
  printf("  shader.create_module=ok\n");
  printf("  shader.create_compute_pipeline=ok\n");
  printf("  shader.dispatch=ok\n");
  printf("  shader.dispatch.expected=0x%08x\n", 0x12345678u);
  printf("  shader.dispatch.observed=0x%08x\n", dispatchObservedValue);

cleanup:
#if defined(__EMSCRIPTEN__)
  if (smokeRc == 0) {
    return 0;
  }
#endif
  if (device != VK_NULL_HANDLE) {
    if (mappedStorageWord && pfnUnmapMemory && storageMemory != VK_NULL_HANDLE) {
      pfnUnmapMemory(device, storageMemory);
      mappedStorageWord = 0;
    }
    if (submitFence != VK_NULL_HANDLE && pfnDestroyFence) {
      pfnDestroyFence(device, submitFence, 0);
    }
    if (commandPool != VK_NULL_HANDLE && pfnDestroyCommandPool) {
      pfnDestroyCommandPool(device, commandPool, 0);
    }
    if (descriptorPool != VK_NULL_HANDLE && pfnDestroyDescriptorPool) {
      pfnDestroyDescriptorPool(device, descriptorPool, 0);
    }
    if (computePipeline != VK_NULL_HANDLE && pfnDestroyPipeline) {
      pfnDestroyPipeline(device, computePipeline, 0);
    }
    if (pipelineLayout != VK_NULL_HANDLE && pfnDestroyPipelineLayout) {
      pfnDestroyPipelineLayout(device, pipelineLayout, 0);
    }
    if (descriptorSetLayout != VK_NULL_HANDLE && pfnDestroyDescriptorSetLayout) {
      pfnDestroyDescriptorSetLayout(device, descriptorSetLayout, 0);
    }
    if (shaderModule != VK_NULL_HANDLE && pfnDestroyShaderModule) {
      pfnDestroyShaderModule(device, shaderModule, 0);
    }
    if (storageBuffer != VK_NULL_HANDLE && pfnDestroyBuffer) {
      pfnDestroyBuffer(device, storageBuffer, 0);
    }
    if (storageMemory != VK_NULL_HANDLE && pfnFreeMemory) {
      pfnFreeMemory(device, storageMemory, 0);
    }
    if (pfnDestroyDevice) {
      pfnDestroyDevice(device, 0);
    } else if (vkDestroyDevice) {
      vkDestroyDevice(device, 0);
    }
  }

  if (instance != VK_NULL_HANDLE) {
    vkDestroyInstance(instance, 0);
  }

  return smokeRc;
}

int main(void) {
  return 0;
}
