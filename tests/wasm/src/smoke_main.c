#include "vulkan/vk_wasm_webgpu_surface.h"

#include <emscripten/emscripten.h>
#include <stdio.h>

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

  printf("smoke runtime ok extension=%s webgpu=%d create=%d\n",
         vkWasmGetWebGpuSurfaceExtensionName(),
         support,
         createResult);
  return 0;
}

int main(void) {
  return wasm_runtime_smoke();
}

