#include "vulkan/vk_wasm_webgpu_surface.h"

#include <emscripten/emscripten.h>
#include <stdio.h>

static const char* webgpu_support_string(int support) {
  if (support == 1) {
    return "available";
  }
  if (support == 0) {
    return "unavailable";
  }
  return "unknown";
}

static const char* webgpu_create_result_string(int createResult) {
  switch (createResult) {
    case VK_WASM_WEBGPU_SURFACE_OK:
      return "surface_created";
    case VK_WASM_WEBGPU_SURFACE_NOT_AVAILABLE:
      return "webgpu_unavailable";
    case VK_WASM_WEBGPU_SURFACE_BAD_ARGUMENT:
      return "bad_argument";
    default:
      return "unknown";
  }
}

EMSCRIPTEN_KEEPALIVE int wasm_runtime_smoke(void) {
  VkWasmWebGpuSurfaceCreateInfoKHR createInfo;
  createInfo.sType = 0u;
  createInfo.pNext = 0;
  createInfo.canvasSelector = "#canvas";

  if (vkWasmGetWebGpuSurfaceExtensionName() == 0) {
    return 11;
  }

  const int support = vkWasmQueryWebGpuSupport();
  const int createResult = vkWasmCreateWebGpuSurface(&createInfo);

  if (createResult != VK_WASM_WEBGPU_SURFACE_OK &&
      createResult != VK_WASM_WEBGPU_SURFACE_NOT_AVAILABLE) {
    return 12;
  }

  printf("wasm surface smoke ok\n");
  printf("  extension=%s\n", vkWasmGetWebGpuSurfaceExtensionName());
  printf("  webgpu_support=%s (%d)\n", webgpu_support_string(support), support);
  printf("  surface_create=%s (%d)\n", webgpu_create_result_string(createResult), createResult);
  printf("  canvas_selector=%s\n", createInfo.canvasSelector);
  return 0;
}

int main(void) {
  return 0;
}
