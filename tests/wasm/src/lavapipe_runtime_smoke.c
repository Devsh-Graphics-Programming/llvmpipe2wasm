#include <emscripten/emscripten.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <volk.h>

VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL vk_icdGetInstanceProcAddr(VkInstance instance, const char* pName);
VKAPI_ATTR VkResult VKAPI_CALL webvulkan_vk_common_CreateShaderModule(
  VkDevice device, const VkShaderModuleCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator,
  VkShaderModule* pShaderModule) __asm__("vk_common_CreateShaderModule");
VKAPI_ATTR void VKAPI_CALL webvulkan_vk_common_DestroyShaderModule(
  VkDevice device, VkShaderModule shaderModule, const VkAllocationCallbacks* pAllocator) __asm__(
  "vk_common_DestroyShaderModule");
VKAPI_ATTR VkResult VKAPI_CALL webvulkan_vk_common_CreatePipelineLayout(
  VkDevice device, const VkPipelineLayoutCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator,
  VkPipelineLayout* pPipelineLayout) __asm__("vk_common_CreatePipelineLayout");
VKAPI_ATTR void VKAPI_CALL webvulkan_vk_common_DestroyPipelineLayout(
  VkDevice device, VkPipelineLayout pipelineLayout, const VkAllocationCallbacks* pAllocator) __asm__(
  "vk_common_DestroyPipelineLayout");
VKAPI_ATTR VkResult VKAPI_CALL webvulkan_vk_common_CreateComputePipelines(
  VkDevice device, VkPipelineCache pipelineCache, uint32_t createInfoCount,
  const VkComputePipelineCreateInfo* pCreateInfos, const VkAllocationCallbacks* pAllocator,
  VkPipeline* pPipelines) __asm__("vk_common_CreateComputePipelines");
VKAPI_ATTR void VKAPI_CALL webvulkan_vk_common_DestroyPipeline(
  VkDevice device, VkPipeline pipeline, const VkAllocationCallbacks* pAllocator) __asm__("vk_common_DestroyPipeline");

static const uint32_t kSmokeComputeSpirv[] = {
  0x07230203u, 0x00010000u, 0x0008000bu, 0x0000000au, 0x00000000u, 0x00020011u, 0x00000001u, 0x0006000bu,
  0x00000001u, 0x4c534c47u, 0x6474732eu, 0x3035342eu, 0x00000000u, 0x0003000eu, 0x00000000u, 0x00000001u,
  0x0005000fu, 0x00000005u, 0x00000004u, 0x6e69616du, 0x00000000u, 0x00060010u, 0x00000004u, 0x00000011u,
  0x00000001u, 0x00000001u, 0x00000001u, 0x00030003u, 0x00000002u, 0x000001c2u, 0x00040005u, 0x00000004u,
  0x6e69616du, 0x00000000u, 0x00040047u, 0x00000009u, 0x0000000bu, 0x00000019u, 0x00020013u, 0x00000002u,
  0x00030021u, 0x00000003u, 0x00000002u, 0x00040015u, 0x00000006u, 0x00000020u, 0x00000000u, 0x00040017u,
  0x00000007u, 0x00000006u, 0x00000003u, 0x0004002bu, 0x00000006u, 0x00000008u, 0x00000001u, 0x0006002cu,
  0x00000007u, 0x00000009u, 0x00000008u, 0x00000008u, 0x00000008u, 0x00050036u, 0x00000002u, 0x00000004u,
  0x00000000u, 0x00000003u, 0x000200f8u, 0x00000005u, 0x000100fdu, 0x00010038u
};

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

EMSCRIPTEN_KEEPALIVE int lavapipe_runtime_smoke(void) {
  int smokeRc = 0;
  VkResult rc = VK_SUCCESS;
  VkInstance instance = VK_NULL_HANDLE;
  VkDevice device = VK_NULL_HANDLE;
  VkShaderModule shaderModule = VK_NULL_HANDLE;
  VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
  VkPipeline computePipeline = VK_NULL_HANDLE;
  PFN_vkDestroyDevice pfnDestroyDevice = 0;
  PFN_vkCreateShaderModule pfnCreateShaderModule = 0;
  PFN_vkDestroyShaderModule pfnDestroyShaderModule = 0;
  PFN_vkCreatePipelineLayout pfnCreatePipelineLayout = 0;
  PFN_vkDestroyPipelineLayout pfnDestroyPipelineLayout = 0;
  PFN_vkCreateComputePipelines pfnCreateComputePipelines = 0;
  PFN_vkDestroyPipeline pfnDestroyPipeline = 0;

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
  if (rc != VK_SUCCESS || device == VK_NULL_HANDLE) {
    smokeRc = 30;
    goto cleanup;
  }

  volkLoadDevice(device);
  pfnDestroyDevice = vkDestroyDevice ? vkDestroyDevice : (PFN_vkDestroyDevice)vkGetDeviceProcAddr(device, "vkDestroyDevice");
  pfnCreateShaderModule = vkCreateShaderModule ? vkCreateShaderModule :
                                                (PFN_vkCreateShaderModule)vkGetDeviceProcAddr(device, "vkCreateShaderModule");
  pfnDestroyShaderModule = vkDestroyShaderModule ? vkDestroyShaderModule :
                                                  (PFN_vkDestroyShaderModule)vkGetDeviceProcAddr(device, "vkDestroyShaderModule");
  pfnCreatePipelineLayout = vkCreatePipelineLayout ? vkCreatePipelineLayout :
                                                   (PFN_vkCreatePipelineLayout)vkGetDeviceProcAddr(device, "vkCreatePipelineLayout");
  pfnDestroyPipelineLayout = vkDestroyPipelineLayout ? vkDestroyPipelineLayout :
                                                     (PFN_vkDestroyPipelineLayout)vkGetDeviceProcAddr(device, "vkDestroyPipelineLayout");
  pfnCreateComputePipelines = vkCreateComputePipelines ? vkCreateComputePipelines :
                                                       (PFN_vkCreateComputePipelines)vkGetDeviceProcAddr(device, "vkCreateComputePipelines");
  pfnDestroyPipeline = vkDestroyPipeline ? vkDestroyPipeline :
                                          (PFN_vkDestroyPipeline)vkGetDeviceProcAddr(device, "vkDestroyPipeline");

  if (!pfnCreateShaderModule) {
    pfnCreateShaderModule = webvulkan_vk_common_CreateShaderModule;
  }
  if (!pfnDestroyShaderModule) {
    pfnDestroyShaderModule = webvulkan_vk_common_DestroyShaderModule;
  }
  if (!pfnCreatePipelineLayout) {
    pfnCreatePipelineLayout = webvulkan_vk_common_CreatePipelineLayout;
  }
  if (!pfnDestroyPipelineLayout) {
    pfnDestroyPipelineLayout = webvulkan_vk_common_DestroyPipelineLayout;
  }
  if (!pfnCreateComputePipelines) {
    pfnCreateComputePipelines = webvulkan_vk_common_CreateComputePipelines;
  }
  if (!pfnDestroyPipeline) {
    pfnDestroyPipeline = webvulkan_vk_common_DestroyPipeline;
  }

  if (!pfnDestroyDevice || !pfnCreateShaderModule || !pfnDestroyShaderModule || !pfnCreatePipelineLayout ||
      !pfnDestroyPipelineLayout || !pfnCreateComputePipelines || !pfnDestroyPipeline) {
    smokeRc = 31;
    goto cleanup;
  }

  VkShaderModuleCreateInfo shaderCreateInfo;
  memset(&shaderCreateInfo, 0, sizeof(shaderCreateInfo));
  shaderCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
  shaderCreateInfo.codeSize = sizeof(kSmokeComputeSpirv);
  shaderCreateInfo.pCode = kSmokeComputeSpirv;

  rc = pfnCreateShaderModule(device, &shaderCreateInfo, 0, &shaderModule);
  if (rc != VK_SUCCESS || shaderModule == VK_NULL_HANDLE) {
    smokeRc = 32;
    goto cleanup;
  }

  VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo;
  memset(&pipelineLayoutCreateInfo, 0, sizeof(pipelineLayoutCreateInfo));
  pipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;

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
  shaderStage.pName = "main";

  VkComputePipelineCreateInfo pipelineCreateInfo;
  memset(&pipelineCreateInfo, 0, sizeof(pipelineCreateInfo));
  pipelineCreateInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
  pipelineCreateInfo.stage = shaderStage;
  pipelineCreateInfo.layout = pipelineLayout;

  rc = pfnCreateComputePipelines(device, VK_NULL_HANDLE, 1u, &pipelineCreateInfo, 0, &computePipeline);
  if (rc != VK_SUCCESS || computePipeline == VK_NULL_HANDLE) {
    smokeRc = 34;
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
  printf("  shader.create_module=ok\n");
  printf("  shader.create_compute_pipeline=ok\n");

cleanup:
  if (device != VK_NULL_HANDLE) {
    if (computePipeline != VK_NULL_HANDLE && pfnDestroyPipeline) {
      pfnDestroyPipeline(device, computePipeline, 0);
    }
    if (pipelineLayout != VK_NULL_HANDLE && pfnDestroyPipelineLayout) {
      pfnDestroyPipelineLayout(device, pipelineLayout, 0);
    }
    if (shaderModule != VK_NULL_HANDLE && pfnDestroyShaderModule) {
      pfnDestroyShaderModule(device, shaderModule, 0);
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
