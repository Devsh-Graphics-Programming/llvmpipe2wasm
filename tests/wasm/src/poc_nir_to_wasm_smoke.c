#include <emscripten/emscripten.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

static int webvulkan_compile_spirv_poc_nir_to_wasm(
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

EMSCRIPTEN_KEEPALIVE int poc_nir_to_wasm_smoke(void) {
  struct webvulkan_nir_wasm_pattern pattern;
  uint8_t* moduleBytes = 0;
  size_t moduleSize = 0u;
  uint32_t outputValue = 0u;
  uint32_t expectedValue = 0u;
  int compileRc = 0;
  int execRc = 0;

  memset(&pattern, 0, sizeof(pattern));

  compileRc = webvulkan_compile_spirv_poc_nir_to_wasm(
    kSmokeComputeSpirv,
    sizeof(kSmokeComputeSpirv) / sizeof(kSmokeComputeSpirv[0]),
    &pattern,
    &moduleBytes,
    &moduleSize
  );
  if (compileRc != 0 || !moduleBytes || moduleSize == 0u) {
    printf("poc nir->wasm smoke failed\n");
    printf("  compile.rc=%d\n", compileRc);
    if (moduleBytes) {
      free(moduleBytes);
    }
    return 1;
  }

  expectedValue = pattern.store_value + 1u;
  execRc = webvulkan_runtime_submodule_smoke_js(moduleBytes, (int)moduleSize, &outputValue);
  if (execRc < 0 || (uint32_t)execRc != expectedValue || outputValue != expectedValue) {
    printf("poc nir->wasm smoke failed\n");
    printf("  execute.rc=%d\n", execRc);
    printf("  output=0x%08x expected=0x%08x\n", outputValue, expectedValue);
    free(moduleBytes);
    return 2;
  }

  printf("poc nir->wasm smoke ok\n");
  printf("  spirv_to_nir=ok\n");
  printf("  ssbo_index=%u\n", pattern.ssbo_index);
  printf("  store_offset=%u\n", pattern.store_offset_bytes);
  printf("  store_value=0x%08x\n", pattern.store_value);
  printf("  module_bytes=%u\n", (unsigned)moduleSize);
  printf("  execute=ok\n");
  printf("  output=0x%08x\n", outputValue);
  printf("  output_matches_expected=yes\n");

  free(moduleBytes);
  return 0;
}

int main(void) {
  return 0;
}
