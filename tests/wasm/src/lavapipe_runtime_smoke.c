#include <emscripten/emscripten.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <volk.h>

VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL vk_icdGetInstanceProcAddr(VkInstance instance, const char* pName);
int lvp_webvulkan_extract_nir_store_pattern(
  const uint32_t* spirv_words,
  size_t spirv_word_count,
  uint32_t* out_ssbo_index,
  uint32_t* out_store_offset_bytes,
  uint32_t* out_store_value
);

EM_JS(
  int,
  webvulkan_runtime_submodule_smoke_js,
  (const uint8_t* moduleBytes, int moduleSize, uint32_t* outputWord),
  {
    try {
      const memory = (typeof wasmMemory !== "undefined" && wasmMemory)
        ? wasmMemory
        : (typeof Module !== "undefined" ? Module["wasmMemory"] : null);
      if (!memory) {
        return -3;
      }

      const bytes = HEAPU8.slice(moduleBytes, moduleBytes + moduleSize);
      const module = new WebAssembly.Module(bytes);
      const instance = new WebAssembly.Instance(module, {
        env: {
          memory: memory
        }
      });

      if (typeof instance.exports.write_const !== "function") {
        return -4;
      }
      if (typeof instance.exports.add_u32 !== "function") {
        return -5;
      }

      const outPtr = outputWord >>> 0;
      instance.exports.write_const(outPtr);
      const value = instance.exports.add_u32(outPtr, 1) >>> 0;
      return value | 0;
    } catch (e) {
      console.error("runtime_submodule_smoke_js", e);
      return -1;
    }
  });

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

static const uint32_t kDispatchNoopSpirv[] = {
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

struct webvulkan_nir_wasm_pattern {
  uint32_t ssbo_index;
  uint32_t store_offset_bytes;
  uint32_t store_value;
};

struct webvulkan_wasm_buffer {
  uint8_t* data;
  size_t size;
  size_t capacity;
};

static void webvulkan_wasm_buffer_free(struct webvulkan_wasm_buffer* buffer) {
  if (buffer && buffer->data) {
    free(buffer->data);
    buffer->data = 0;
  }
  if (buffer) {
    buffer->size = 0;
    buffer->capacity = 0;
  }
}

static int webvulkan_wasm_buffer_reserve(struct webvulkan_wasm_buffer* buffer, size_t requiredCapacity) {
  if (requiredCapacity <= buffer->capacity) {
    return 0;
  }
  size_t newCapacity = buffer->capacity ? buffer->capacity : 64u;
  while (newCapacity < requiredCapacity) {
    if (newCapacity > (SIZE_MAX / 2u)) {
      return -1;
    }
    newCapacity *= 2u;
  }
  uint8_t* newData = (uint8_t*)realloc(buffer->data, newCapacity);
  if (!newData) {
    return -1;
  }
  buffer->data = newData;
  buffer->capacity = newCapacity;
  return 0;
}

static int webvulkan_wasm_buffer_push_byte(struct webvulkan_wasm_buffer* buffer, uint8_t value) {
  if (webvulkan_wasm_buffer_reserve(buffer, buffer->size + 1u) != 0) {
    return -1;
  }
  buffer->data[buffer->size++] = value;
  return 0;
}

static int webvulkan_wasm_buffer_push_bytes(
  struct webvulkan_wasm_buffer* buffer,
  const uint8_t* values,
  size_t valueCount
) {
  if (valueCount == 0u) {
    return 0;
  }
  if (!values) {
    return -1;
  }
  if (webvulkan_wasm_buffer_reserve(buffer, buffer->size + valueCount) != 0) {
    return -1;
  }
  memcpy(buffer->data + buffer->size, values, valueCount);
  buffer->size += valueCount;
  return 0;
}

static int webvulkan_wasm_push_u32_leb(struct webvulkan_wasm_buffer* buffer, uint32_t value) {
  do {
    uint8_t byte = (uint8_t)(value & 0x7fu);
    value >>= 7u;
    if (value != 0u) {
      byte |= 0x80u;
    }
    if (webvulkan_wasm_buffer_push_byte(buffer, byte) != 0) {
      return -1;
    }
  } while (value != 0u);
  return 0;
}

static int webvulkan_wasm_push_i32_sleb(struct webvulkan_wasm_buffer* buffer, int32_t value) {
  int more = 1;
  while (more) {
    uint8_t byte = (uint8_t)(value & 0x7f);
    int signBitSet = (byte & 0x40) != 0;
    value >>= 7;
    if ((value == 0 && !signBitSet) || (value == -1 && signBitSet)) {
      more = 0;
    } else {
      byte |= 0x80;
    }
    if (webvulkan_wasm_buffer_push_byte(buffer, byte) != 0) {
      return -1;
    }
  }
  return 0;
}

static int webvulkan_wasm_push_string(struct webvulkan_wasm_buffer* buffer, const char* text) {
  const size_t textLen = strlen(text);
  if (webvulkan_wasm_push_u32_leb(buffer, (uint32_t)textLen) != 0) {
    return -1;
  }
  return webvulkan_wasm_buffer_push_bytes(buffer, (const uint8_t*)text, textLen);
}

static int webvulkan_wasm_emit_section(
  struct webvulkan_wasm_buffer* module,
  uint8_t sectionId,
  const struct webvulkan_wasm_buffer* payload
) {
  if (webvulkan_wasm_buffer_push_byte(module, sectionId) != 0) {
    return -1;
  }
  if (webvulkan_wasm_push_u32_leb(module, (uint32_t)payload->size) != 0) {
    return -1;
  }
  return webvulkan_wasm_buffer_push_bytes(module, payload->data, payload->size);
}

static int webvulkan_generate_wasm_module(
  const struct webvulkan_nir_wasm_pattern* pattern,
  uint8_t** outModuleBytes,
  size_t* outModuleSize
) {
  static const uint8_t wasmMagicAndVersion[] = { 0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00 };
  struct webvulkan_wasm_buffer module = { 0 };
  struct webvulkan_wasm_buffer payload = { 0 };
  struct webvulkan_wasm_buffer functionBody = { 0 };
  int rc = -1;

  if (!outModuleBytes || !outModuleSize || !pattern) {
    return -1;
  }
  *outModuleBytes = 0;
  *outModuleSize = 0u;

  if (webvulkan_wasm_buffer_push_bytes(&module, wasmMagicAndVersion, sizeof(wasmMagicAndVersion)) != 0) {
    goto cleanup;
  }

  if (webvulkan_wasm_push_u32_leb(&payload, 2u) != 0 ||
      webvulkan_wasm_buffer_push_byte(&payload, 0x60) != 0 ||
      webvulkan_wasm_push_u32_leb(&payload, 1u) != 0 ||
      webvulkan_wasm_buffer_push_byte(&payload, 0x7f) != 0 ||
      webvulkan_wasm_push_u32_leb(&payload, 0u) != 0 ||
      webvulkan_wasm_buffer_push_byte(&payload, 0x60) != 0 ||
      webvulkan_wasm_push_u32_leb(&payload, 2u) != 0 ||
      webvulkan_wasm_buffer_push_byte(&payload, 0x7f) != 0 ||
      webvulkan_wasm_buffer_push_byte(&payload, 0x7f) != 0 ||
      webvulkan_wasm_push_u32_leb(&payload, 1u) != 0 ||
      webvulkan_wasm_buffer_push_byte(&payload, 0x7f) != 0 ||
      webvulkan_wasm_emit_section(&module, 1u, &payload) != 0) {
    goto cleanup;
  }
  webvulkan_wasm_buffer_free(&payload);

  if (webvulkan_wasm_push_u32_leb(&payload, 1u) != 0 ||
      webvulkan_wasm_push_string(&payload, "env") != 0 ||
      webvulkan_wasm_push_string(&payload, "memory") != 0 ||
      webvulkan_wasm_buffer_push_byte(&payload, 0x02) != 0 ||
      webvulkan_wasm_push_u32_leb(&payload, 0u) != 0 ||
      webvulkan_wasm_push_u32_leb(&payload, 0u) != 0 ||
      webvulkan_wasm_emit_section(&module, 2u, &payload) != 0) {
    goto cleanup;
  }
  webvulkan_wasm_buffer_free(&payload);

  if (webvulkan_wasm_push_u32_leb(&payload, 2u) != 0 ||
      webvulkan_wasm_push_u32_leb(&payload, 0u) != 0 ||
      webvulkan_wasm_push_u32_leb(&payload, 1u) != 0 ||
      webvulkan_wasm_emit_section(&module, 3u, &payload) != 0) {
    goto cleanup;
  }
  webvulkan_wasm_buffer_free(&payload);

  if (webvulkan_wasm_push_u32_leb(&payload, 2u) != 0 ||
      webvulkan_wasm_push_string(&payload, "write_const") != 0 ||
      webvulkan_wasm_buffer_push_byte(&payload, 0x00) != 0 ||
      webvulkan_wasm_push_u32_leb(&payload, 0u) != 0 ||
      webvulkan_wasm_push_string(&payload, "add_u32") != 0 ||
      webvulkan_wasm_buffer_push_byte(&payload, 0x00) != 0 ||
      webvulkan_wasm_push_u32_leb(&payload, 1u) != 0 ||
      webvulkan_wasm_emit_section(&module, 7u, &payload) != 0) {
    goto cleanup;
  }
  webvulkan_wasm_buffer_free(&payload);

  if (webvulkan_wasm_push_u32_leb(&payload, 2u) != 0) {
    goto cleanup;
  }

  if (webvulkan_wasm_push_u32_leb(&functionBody, 0u) != 0 ||
      webvulkan_wasm_buffer_push_byte(&functionBody, 0x20) != 0 ||
      webvulkan_wasm_push_u32_leb(&functionBody, 0u) != 0 ||
      webvulkan_wasm_buffer_push_byte(&functionBody, 0x41) != 0 ||
      webvulkan_wasm_push_i32_sleb(&functionBody, (int32_t)pattern->store_value) != 0 ||
      webvulkan_wasm_buffer_push_byte(&functionBody, 0x36) != 0 ||
      webvulkan_wasm_push_u32_leb(&functionBody, 2u) != 0 ||
      webvulkan_wasm_push_u32_leb(&functionBody, pattern->store_offset_bytes) != 0 ||
      webvulkan_wasm_buffer_push_byte(&functionBody, 0x0b) != 0 ||
      webvulkan_wasm_push_u32_leb(&payload, (uint32_t)functionBody.size) != 0 ||
      webvulkan_wasm_buffer_push_bytes(&payload, functionBody.data, functionBody.size) != 0) {
    goto cleanup;
  }
  webvulkan_wasm_buffer_free(&functionBody);

  if (webvulkan_wasm_push_u32_leb(&functionBody, 0u) != 0 ||
      webvulkan_wasm_buffer_push_byte(&functionBody, 0x20) != 0 ||
      webvulkan_wasm_push_u32_leb(&functionBody, 0u) != 0 ||
      webvulkan_wasm_buffer_push_byte(&functionBody, 0x20) != 0 ||
      webvulkan_wasm_push_u32_leb(&functionBody, 0u) != 0 ||
      webvulkan_wasm_buffer_push_byte(&functionBody, 0x28) != 0 ||
      webvulkan_wasm_push_u32_leb(&functionBody, 2u) != 0 ||
      webvulkan_wasm_push_u32_leb(&functionBody, pattern->store_offset_bytes) != 0 ||
      webvulkan_wasm_buffer_push_byte(&functionBody, 0x20) != 0 ||
      webvulkan_wasm_push_u32_leb(&functionBody, 1u) != 0 ||
      webvulkan_wasm_buffer_push_byte(&functionBody, 0x6a) != 0 ||
      webvulkan_wasm_buffer_push_byte(&functionBody, 0x36) != 0 ||
      webvulkan_wasm_push_u32_leb(&functionBody, 2u) != 0 ||
      webvulkan_wasm_push_u32_leb(&functionBody, pattern->store_offset_bytes) != 0 ||
      webvulkan_wasm_buffer_push_byte(&functionBody, 0x20) != 0 ||
      webvulkan_wasm_push_u32_leb(&functionBody, 0u) != 0 ||
      webvulkan_wasm_buffer_push_byte(&functionBody, 0x28) != 0 ||
      webvulkan_wasm_push_u32_leb(&functionBody, 2u) != 0 ||
      webvulkan_wasm_push_u32_leb(&functionBody, pattern->store_offset_bytes) != 0 ||
      webvulkan_wasm_buffer_push_byte(&functionBody, 0x0b) != 0 ||
      webvulkan_wasm_push_u32_leb(&payload, (uint32_t)functionBody.size) != 0 ||
      webvulkan_wasm_buffer_push_bytes(&payload, functionBody.data, functionBody.size) != 0 ||
      webvulkan_wasm_emit_section(&module, 10u, &payload) != 0) {
    goto cleanup;
  }
  webvulkan_wasm_buffer_free(&functionBody);
  webvulkan_wasm_buffer_free(&payload);

  *outModuleBytes = module.data;
  *outModuleSize = module.size;
  module.data = 0;
  module.size = 0;
  module.capacity = 0;
  rc = 0;

cleanup:
  webvulkan_wasm_buffer_free(&module);
  webvulkan_wasm_buffer_free(&payload);
  webvulkan_wasm_buffer_free(&functionBody);
  return rc;
}

static int webvulkan_compile_spirv_nir_to_wasm(
  const uint32_t* spirvWords,
  size_t spirvWordCount,
  struct webvulkan_nir_wasm_pattern* outPattern,
  uint8_t** outModuleBytes,
  size_t* outModuleSize
) {
  int rc = -1;

  if (!spirvWords || !outPattern || !outModuleBytes || !outModuleSize || spirvWordCount == 0u) {
    return -1;
  }

  rc = lvp_webvulkan_extract_nir_store_pattern(
    spirvWords,
    spirvWordCount,
    &outPattern->ssbo_index,
    &outPattern->store_offset_bytes,
    &outPattern->store_value
  );
  if (rc != 0) {
    return rc;
  }

  if (outPattern->ssbo_index != 0u) {
    return -6;
  }

  rc = webvulkan_generate_wasm_module(outPattern, outModuleBytes, outModuleSize);
  if (rc != 0) {
    return -7;
  }
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
  int smokeRc = 0;
  VkResult rc = VK_SUCCESS;
  VkInstance instance = VK_NULL_HANDLE;
  VkDevice device = VK_NULL_HANDLE;
  VkShaderModule shaderModule = VK_NULL_HANDLE;
  VkShaderModule dispatchShaderModule = VK_NULL_HANDLE;
  VkDescriptorSetLayout descriptorSetLayout = VK_NULL_HANDLE;
  VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
  VkPipeline computePipeline = VK_NULL_HANDLE;
  VkPipelineLayout dispatchPipelineLayout = VK_NULL_HANDLE;
  VkPipeline dispatchPipeline = VK_NULL_HANDLE;
  VkCommandPool commandPool = VK_NULL_HANDLE;
  VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
  VkFence submitFence = VK_NULL_HANDLE;
  VkQueue queue = VK_NULL_HANDLE;
  struct webvulkan_nir_wasm_pattern nirWasmPattern;
  uint8_t* nirWasmModuleBytes = 0;
  size_t nirWasmModuleSize = 0u;
  int nirWasmCompileResult = 0;
  uint32_t nirWasmExpectedValue = 0u;
  uint32_t runtimeSubmoduleValue = 0u;
  int runtimeSubmoduleResult = 0;
  PFN_vkDestroyDevice pfnDestroyDevice = 0;
  PFN_vkCreateShaderModule pfnCreateShaderModule = 0;
  PFN_vkDestroyShaderModule pfnDestroyShaderModule = 0;
  PFN_vkCreateDescriptorSetLayout pfnCreateDescriptorSetLayout = 0;
  PFN_vkDestroyDescriptorSetLayout pfnDestroyDescriptorSetLayout = 0;
  PFN_vkCreatePipelineLayout pfnCreatePipelineLayout = 0;
  PFN_vkDestroyPipelineLayout pfnDestroyPipelineLayout = 0;
  PFN_vkCreateComputePipelines pfnCreateComputePipelines = 0;
  PFN_vkDestroyPipeline pfnDestroyPipeline = 0;
  PFN_vkGetDeviceQueue pfnGetDeviceQueue = 0;
  PFN_vkCreateCommandPool pfnCreateCommandPool = 0;
  PFN_vkDestroyCommandPool pfnDestroyCommandPool = 0;
  PFN_vkAllocateCommandBuffers pfnAllocateCommandBuffers = 0;
  PFN_vkBeginCommandBuffer pfnBeginCommandBuffer = 0;
  PFN_vkEndCommandBuffer pfnEndCommandBuffer = 0;
  PFN_vkCmdBindPipeline pfnCmdBindPipeline = 0;
  PFN_vkCmdDispatch pfnCmdDispatch = 0;
  PFN_vkCreateFence pfnCreateFence = 0;
  PFN_vkDestroyFence pfnDestroyFence = 0;
  PFN_vkQueueSubmit pfnQueueSubmit = 0;
  PFN_vkWaitForFences pfnWaitForFences = 0;

  memset(&nirWasmPattern, 0, sizeof(nirWasmPattern));

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
  pfnCreateDescriptorSetLayout = vkCreateDescriptorSetLayout ? vkCreateDescriptorSetLayout :
                                      (PFN_vkCreateDescriptorSetLayout)vkGetDeviceProcAddr(device, "vkCreateDescriptorSetLayout");
  pfnDestroyDescriptorSetLayout = vkDestroyDescriptorSetLayout ? vkDestroyDescriptorSetLayout :
                                       (PFN_vkDestroyDescriptorSetLayout)vkGetDeviceProcAddr(device, "vkDestroyDescriptorSetLayout");
  pfnCreatePipelineLayout = vkCreatePipelineLayout ? vkCreatePipelineLayout :
                                                   (PFN_vkCreatePipelineLayout)vkGetDeviceProcAddr(device, "vkCreatePipelineLayout");
  pfnDestroyPipelineLayout = vkDestroyPipelineLayout ? vkDestroyPipelineLayout :
                                                     (PFN_vkDestroyPipelineLayout)vkGetDeviceProcAddr(device, "vkDestroyPipelineLayout");
  pfnCreateComputePipelines = vkCreateComputePipelines ? vkCreateComputePipelines :
                                                       (PFN_vkCreateComputePipelines)vkGetDeviceProcAddr(device, "vkCreateComputePipelines");
  pfnDestroyPipeline = vkDestroyPipeline ? vkDestroyPipeline :
                                          (PFN_vkDestroyPipeline)vkGetDeviceProcAddr(device, "vkDestroyPipeline");
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

  if (!pfnDestroyDevice || !pfnCreateShaderModule || !pfnDestroyShaderModule || !pfnCreatePipelineLayout ||
      !pfnDestroyPipelineLayout || !pfnCreateComputePipelines || !pfnDestroyPipeline ||
      !pfnCreateDescriptorSetLayout || !pfnDestroyDescriptorSetLayout) {
    printf("lavapipe runtime smoke missing device entrypoints\n");
    printf("  vkDestroyDevice=%s\n", pfnDestroyDevice ? "present" : "missing");
    printf("  vkCreateShaderModule=%s\n", pfnCreateShaderModule ? "present" : "missing");
    printf("  vkDestroyShaderModule=%s\n", pfnDestroyShaderModule ? "present" : "missing");
    printf("  vkCreateDescriptorSetLayout=%s\n", pfnCreateDescriptorSetLayout ? "present" : "missing");
    printf("  vkDestroyDescriptorSetLayout=%s\n", pfnDestroyDescriptorSetLayout ? "present" : "missing");
    printf("  vkCreatePipelineLayout=%s\n", pfnCreatePipelineLayout ? "present" : "missing");
    printf("  vkDestroyPipelineLayout=%s\n", pfnDestroyPipelineLayout ? "present" : "missing");
    printf("  vkCreateComputePipelines=%s\n", pfnCreateComputePipelines ? "present" : "missing");
    printf("  vkDestroyPipeline=%s\n", pfnDestroyPipeline ? "present" : "missing");
    smokeRc = 31;
    goto cleanup;
  }
  if (!pfnGetDeviceQueue || !pfnCreateCommandPool || !pfnDestroyCommandPool || !pfnAllocateCommandBuffers ||
      !pfnBeginCommandBuffer || !pfnEndCommandBuffer || !pfnCmdBindPipeline || !pfnCmdDispatch ||
      !pfnCreateFence || !pfnDestroyFence || !pfnQueueSubmit || !pfnWaitForFences) {
    printf("lavapipe runtime smoke missing dispatch entrypoints\n");
    printf("  vkGetDeviceQueue=%s\n", pfnGetDeviceQueue ? "present" : "missing");
    printf("  vkCreateCommandPool=%s\n", pfnCreateCommandPool ? "present" : "missing");
    printf("  vkDestroyCommandPool=%s\n", pfnDestroyCommandPool ? "present" : "missing");
    printf("  vkAllocateCommandBuffers=%s\n", pfnAllocateCommandBuffers ? "present" : "missing");
    printf("  vkBeginCommandBuffer=%s\n", pfnBeginCommandBuffer ? "present" : "missing");
    printf("  vkEndCommandBuffer=%s\n", pfnEndCommandBuffer ? "present" : "missing");
    printf("  vkCmdBindPipeline=%s\n", pfnCmdBindPipeline ? "present" : "missing");
    printf("  vkCmdDispatch=%s\n", pfnCmdDispatch ? "present" : "missing");
    printf("  vkCreateFence=%s\n", pfnCreateFence ? "present" : "missing");
    printf("  vkDestroyFence=%s\n", pfnDestroyFence ? "present" : "missing");
    printf("  vkQueueSubmit=%s\n", pfnQueueSubmit ? "present" : "missing");
    printf("  vkWaitForFences=%s\n", pfnWaitForFences ? "present" : "missing");
    smokeRc = 36;
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

  VkShaderModuleCreateInfo dispatchShaderCreateInfo;
  memset(&dispatchShaderCreateInfo, 0, sizeof(dispatchShaderCreateInfo));
  dispatchShaderCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
  dispatchShaderCreateInfo.codeSize = sizeof(kDispatchNoopSpirv);
  dispatchShaderCreateInfo.pCode = kDispatchNoopSpirv;

  rc = pfnCreateShaderModule(device, &dispatchShaderCreateInfo, 0, &dispatchShaderModule);
  if (rc != VK_SUCCESS || dispatchShaderModule == VK_NULL_HANDLE) {
    smokeRc = 58;
    goto cleanup;
  }

  VkPipelineLayoutCreateInfo dispatchPipelineLayoutCreateInfo;
  memset(&dispatchPipelineLayoutCreateInfo, 0, sizeof(dispatchPipelineLayoutCreateInfo));
  dispatchPipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
  dispatchPipelineLayoutCreateInfo.setLayoutCount = 0u;
  dispatchPipelineLayoutCreateInfo.pSetLayouts = 0;

  rc = pfnCreatePipelineLayout(device, &dispatchPipelineLayoutCreateInfo, 0, &dispatchPipelineLayout);
  if (rc != VK_SUCCESS || dispatchPipelineLayout == VK_NULL_HANDLE) {
    smokeRc = 59;
    goto cleanup;
  }

  VkPipelineShaderStageCreateInfo dispatchShaderStage;
  memset(&dispatchShaderStage, 0, sizeof(dispatchShaderStage));
  dispatchShaderStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  dispatchShaderStage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
  dispatchShaderStage.module = dispatchShaderModule;
  dispatchShaderStage.pName = "main";

  VkComputePipelineCreateInfo dispatchPipelineCreateInfo;
  memset(&dispatchPipelineCreateInfo, 0, sizeof(dispatchPipelineCreateInfo));
  dispatchPipelineCreateInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
  dispatchPipelineCreateInfo.stage = dispatchShaderStage;
  dispatchPipelineCreateInfo.layout = dispatchPipelineLayout;

  rc = pfnCreateComputePipelines(device, VK_NULL_HANDLE, 1u, &dispatchPipelineCreateInfo, 0, &dispatchPipeline);
  if (rc != VK_SUCCESS || dispatchPipeline == VK_NULL_HANDLE) {
    smokeRc = 60;
    goto cleanup;
  }

  pfnGetDeviceQueue(device, queueFamilyIndex, 0u, &queue);
  if (queue == VK_NULL_HANDLE) {
    smokeRc = 61;
    goto cleanup;
  }

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

  pfnCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, dispatchPipeline);
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

  nirWasmCompileResult = webvulkan_compile_spirv_nir_to_wasm(
    kSmokeComputeSpirv,
    sizeof(kSmokeComputeSpirv) / sizeof(kSmokeComputeSpirv[0]),
    &nirWasmPattern,
    &nirWasmModuleBytes,
    &nirWasmModuleSize
  );
  if (nirWasmCompileResult != 0 || !nirWasmModuleBytes || nirWasmModuleSize == 0u) {
    printf("nir->wasm compile failed rc=%d\n", nirWasmCompileResult);
    smokeRc = 57;
    goto cleanup;
  }

  nirWasmExpectedValue = nirWasmPattern.store_value + 1u;
  runtimeSubmoduleResult = webvulkan_runtime_submodule_smoke_js(
    nirWasmModuleBytes, (int)nirWasmModuleSize, &runtimeSubmoduleValue);
  if (runtimeSubmoduleResult < 0 || (uint32_t)runtimeSubmoduleResult != nirWasmExpectedValue ||
      runtimeSubmoduleValue != nirWasmExpectedValue) {
    smokeRc = 35;
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
  printf("  shader.dispatch=ok\n");
  printf("  nir_to_wasm.spirv_to_nir=ok\n");
  printf("  nir_to_wasm.ssbo_index=%u\n", nirWasmPattern.ssbo_index);
  printf("  nir_to_wasm.store_offset=%u\n", nirWasmPattern.store_offset_bytes);
  printf("  nir_to_wasm.store_value=0x%08x\n", nirWasmPattern.store_value);
  printf("  nir_to_wasm.module_bytes=%u\n", (unsigned)nirWasmModuleSize);
  printf("  nir_to_wasm.execute=ok\n");
  printf("  nir_to_wasm.output=0x%08x\n", runtimeSubmoduleValue);
  printf("  nir_to_wasm.output_matches_expected=yes\n");

cleanup:
  if (nirWasmModuleBytes) {
    free(nirWasmModuleBytes);
    nirWasmModuleBytes = 0;
  }
  if (device != VK_NULL_HANDLE) {
    if (submitFence != VK_NULL_HANDLE && pfnDestroyFence) {
      pfnDestroyFence(device, submitFence, 0);
    }
    if (commandPool != VK_NULL_HANDLE && pfnDestroyCommandPool) {
      pfnDestroyCommandPool(device, commandPool, 0);
    }
    if (dispatchPipeline != VK_NULL_HANDLE && pfnDestroyPipeline) {
      pfnDestroyPipeline(device, dispatchPipeline, 0);
    }
    if (dispatchPipelineLayout != VK_NULL_HANDLE && pfnDestroyPipelineLayout) {
      pfnDestroyPipelineLayout(device, dispatchPipelineLayout, 0);
    }
    if (dispatchShaderModule != VK_NULL_HANDLE && pfnDestroyShaderModule) {
      pfnDestroyShaderModule(device, dispatchShaderModule, 0);
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
