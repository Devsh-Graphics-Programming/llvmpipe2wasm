#include <emscripten/emscripten.h>
#include <stdio.h>
#include <string.h>
#include <vulkan/vulkan.h>

VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL vk_icdGetInstanceProcAddr(VkInstance instance, const char* pName);

EMSCRIPTEN_KEEPALIVE int lavapipe_runtime_smoke(void) {
  PFN_vkCreateInstance createInstance = (PFN_vkCreateInstance)vk_icdGetInstanceProcAddr(VK_NULL_HANDLE, "vkCreateInstance");
  PFN_vkEnumerateInstanceVersion enumerateVersion = (PFN_vkEnumerateInstanceVersion)vk_icdGetInstanceProcAddr(VK_NULL_HANDLE, "vkEnumerateInstanceVersion");

  if (!createInstance || !enumerateVersion) {
    return 21;
  }

  uint32_t apiVersion = 0u;
  VkResult rc = enumerateVersion(&apiVersion);
  if (rc != VK_SUCCESS) {
    return 22;
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

  VkInstance instance = VK_NULL_HANDLE;
  rc = createInstance(&createInfo, 0, &instance);
  if (rc != VK_SUCCESS || instance == VK_NULL_HANDLE) {
    return 23;
  }

  PFN_vkEnumeratePhysicalDevices enumeratePhysicalDevices =
    (PFN_vkEnumeratePhysicalDevices)vk_icdGetInstanceProcAddr(instance, "vkEnumeratePhysicalDevices");
  PFN_vkDestroyInstance destroyInstance =
    (PFN_vkDestroyInstance)vk_icdGetInstanceProcAddr(instance, "vkDestroyInstance");

  if (!enumeratePhysicalDevices || !destroyInstance) {
    return 24;
  }

  uint32_t physicalDeviceCount = 0u;
  rc = enumeratePhysicalDevices(instance, &physicalDeviceCount, 0);
  if (rc != VK_SUCCESS || physicalDeviceCount == 0u) {
    destroyInstance(instance, 0);
    return 25;
  }

  destroyInstance(instance, 0);
  printf("lavapipe runtime smoke ok api=%u devices=%u\n", apiVersion, physicalDeviceCount);
  return 0;
}

int main(void) {
  return lavapipe_runtime_smoke();
}
