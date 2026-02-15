#include "vulkan/vk_wasm_webgpu_surface.h"

#include <emscripten/emscripten.h>

EM_JS(int, wasm_js_has_webgpu, (), {
  if (typeof globalThis === "undefined") {
    return 0;
  }
  const nav = globalThis.navigator;
  if (nav && nav.gpu) {
    return 1;
  }
  if (globalThis.gpu) {
    return 1;
  }
  return 0;
});

const char* vkWasmGetWebGpuSurfaceExtensionName(void) {
  return VK_WASM_WEBGPU_SURFACE_EXTENSION_NAME;
}

int vkWasmQueryWebGpuSupport(void) {
  return wasm_js_has_webgpu();
}

int vkWasmCreateWebGpuSurface(const VkWasmWebGpuSurfaceCreateInfoKHR* createInfo) {
  if (!createInfo || !createInfo->canvasSelector || createInfo->canvasSelector[0] == '\0') {
    return VK_WASM_WEBGPU_SURFACE_BAD_ARGUMENT;
  }
  if (!vkWasmQueryWebGpuSupport()) {
    return VK_WASM_WEBGPU_SURFACE_NOT_AVAILABLE;
  }
  return VK_WASM_WEBGPU_SURFACE_OK;
}

