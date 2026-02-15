#ifndef VK_WASM_WEBGPU_SURFACE_H
#define VK_WASM_WEBGPU_SURFACE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define VK_WASM_webgpu_surface 1
#define VK_WASM_WEBGPU_SURFACE_SPEC_VERSION 1
#define VK_WASM_WEBGPU_SURFACE_EXTENSION_NAME "VK_WASM_webgpu_surface"

typedef struct VkWasmWebGpuSurfaceCreateInfoKHR {
  uint32_t sType;
  const void* pNext;
  const char* canvasSelector;
} VkWasmWebGpuSurfaceCreateInfoKHR;

enum {
  VK_WASM_WEBGPU_SURFACE_OK = 0,
  VK_WASM_WEBGPU_SURFACE_NOT_AVAILABLE = 1,
  VK_WASM_WEBGPU_SURFACE_BAD_ARGUMENT = 2
};

const char* vkWasmGetWebGpuSurfaceExtensionName(void);
int vkWasmQueryWebGpuSupport(void);
int vkWasmCreateWebGpuSurface(const VkWasmWebGpuSurfaceCreateInfoKHR* createInfo);

#ifdef __cplusplus
}
#endif

#endif

