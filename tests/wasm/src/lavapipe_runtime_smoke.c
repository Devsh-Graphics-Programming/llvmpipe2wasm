#include <emscripten/emscripten.h>
#include <volk.h>
#include <stdio.h>
#include <string.h>

VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL vk_icdGetInstanceProcAddr(VkInstance instance, const char* pName);
VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL vk_icdGetPhysicalDeviceProcAddr(VkInstance instance, const char* pName);

static int string_contains(const char* haystack, const char* needle) {
  return haystack && needle && strstr(haystack, needle) != 0;
}

EMSCRIPTEN_KEEPALIVE int lavapipe_runtime_smoke(void) {
  volkInitializeCustom((PFN_vkGetInstanceProcAddr)vk_icdGetInstanceProcAddr);
  if (!vkCreateInstance || !vkEnumerateInstanceVersion) {
    return 21;
  }

  uint32_t apiVersion = 0u;
  VkResult rc = vkEnumerateInstanceVersion(&apiVersion);
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
  rc = vkCreateInstance(&createInfo, 0, &instance);
  if (rc != VK_SUCCESS || instance == VK_NULL_HANDLE) {
    return 23;
  }

  volkLoadInstance(instance);
  if (!vkEnumeratePhysicalDevices || !vkDestroyInstance || !vkGetDeviceProcAddr) {
    vkDestroyInstance(instance, 0);
    return 24;
  }

  uint32_t physicalDeviceCount = 0u;
  rc = vkEnumeratePhysicalDevices(instance, &physicalDeviceCount, 0);
  if (rc != VK_SUCCESS || physicalDeviceCount == 0u) {
    vkDestroyInstance(instance, 0);
    return 25;
  }

  VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
  uint32_t one = 1u;
  rc = vkEnumeratePhysicalDevices(instance, &one, &physicalDevice);
  if (rc != VK_SUCCESS || one == 0u || physicalDevice == VK_NULL_HANDLE) {
    vkDestroyInstance(instance, 0);
    return 26;
  }

  PFN_vkVoidFunction fpEnumeratePhysicalDevices =
    vk_icdGetInstanceProcAddr(instance, "vkEnumeratePhysicalDevices");
  PFN_vkVoidFunction fpGetPhysicalDeviceProperties =
    vk_icdGetInstanceProcAddr(instance, "vkGetPhysicalDeviceProperties");
  PFN_vkVoidFunction fpGetPhysicalDeviceProperties2 =
    vk_icdGetInstanceProcAddr(instance, "vkGetPhysicalDeviceProperties2");

  if (!fpEnumeratePhysicalDevices || !fpGetPhysicalDeviceProperties || !fpGetPhysicalDeviceProperties2) {
    vkDestroyInstance(instance, 0);
    return 27;
  }

  PFN_vkGetPhysicalDeviceProperties2 pfnGetPhysicalDeviceProperties2 =
    (PFN_vkGetPhysicalDeviceProperties2)vk_icdGetPhysicalDeviceProcAddr(instance, "vkGetPhysicalDeviceProperties2");
  if (!pfnGetPhysicalDeviceProperties2) {
    pfnGetPhysicalDeviceProperties2 =
      (PFN_vkGetPhysicalDeviceProperties2)vk_icdGetPhysicalDeviceProcAddr(instance, "vkGetPhysicalDeviceProperties2KHR");
  }
  if (!pfnGetPhysicalDeviceProperties2) {
    vkDestroyInstance(instance, 0);
    return 28;
  }

  VkPhysicalDeviceDriverProperties driverProps;
  memset(&driverProps, 0, sizeof(driverProps));
  driverProps.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DRIVER_PROPERTIES;

  VkPhysicalDeviceProperties2 props2;
  memset(&props2, 0, sizeof(props2));
  props2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
  props2.pNext = &driverProps;
  pfnGetPhysicalDeviceProperties2(physicalDevice, &props2);
  VkPhysicalDeviceProperties props = props2.properties;

  const int proofDeviceName = string_contains(props.deviceName, "llvmpipe");
  const int proofDriverName = string_contains(driverProps.driverName, "llvmpipe");
  if (!proofDeviceName || !proofDriverName) {
    vkDestroyInstance(instance, 0);
    return 29;
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
  vkDestroyInstance(instance, 0);
  return 0;
}

int main(void) {
  return 0;
}
