#include "webvulkan_shader_runtime_registry.h"

#include <emscripten/emscripten.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#define WEBVULKAN_RUNTIME_MAX_MODULES 16u
#define WEBVULKAN_RUNTIME_ENTRYPOINT_MAX 64u
#define WEBVULKAN_RUNTIME_PROVIDER_MAX 128u

typedef struct WebVulkanRuntimeSpirvEntry_t {
  uint32_t keyLo;
  uint32_t keyHi;
  uint8_t* bytes;
  uint32_t byteCount;
  uint32_t expectedDispatchValue;
  char entrypoint[WEBVULKAN_RUNTIME_ENTRYPOINT_MAX];
} WebVulkanRuntimeSpirvEntry;

typedef struct WebVulkanRuntimeWasmEntry_t {
  uint32_t keyLo;
  uint32_t keyHi;
  uint8_t* bytes;
  uint32_t byteCount;
  char entrypoint[WEBVULKAN_RUNTIME_ENTRYPOINT_MAX];
  char provider[WEBVULKAN_RUNTIME_PROVIDER_MAX];
} WebVulkanRuntimeWasmEntry;

static WebVulkanRuntimeSpirvEntry g_runtime_spirv_entries[WEBVULKAN_RUNTIME_MAX_MODULES];
static uint32_t g_runtime_spirv_count = 0u;
static WebVulkanRuntimeWasmEntry g_runtime_wasm_entries[WEBVULKAN_RUNTIME_MAX_MODULES];
static uint32_t g_runtime_wasm_count = 0u;
static int g_runtime_wasm_used = 0;
static char g_runtime_wasm_provider[WEBVULKAN_RUNTIME_PROVIDER_MAX] = "none";
static uint32_t g_runtime_active_shader_key_lo = WEBVULKAN_RUNTIME_DEFAULT_SHADER_KEY_LO;
static uint32_t g_runtime_active_shader_key_hi = WEBVULKAN_RUNTIME_DEFAULT_SHADER_KEY_HI;
static int g_runtime_captured_shader_key_valid = 0;
static uint32_t g_runtime_captured_shader_key_lo = 0u;
static uint32_t g_runtime_captured_shader_key_hi = 0u;

static void webvulkan_copy_string(char* dst, uint32_t dstSize, const char* src, const char* fallback) {
  if (!dst || dstSize == 0u) {
    return;
  }
  const char* selected = src && src[0] ? src : fallback;
  if (!selected) {
    selected = "";
  }
  size_t len = strlen(selected);
  if (len >= (size_t)dstSize) {
    len = (size_t)dstSize - 1u;
  }
  memcpy(dst, selected, len);
  dst[len] = '\0';
}

static int webvulkan_validate_spirv_bytes(const uint8_t* bytes, uint32_t byteCount) {
  if (!bytes || byteCount < 4u || (byteCount % 4u) != 0u) {
    return -1;
  }
  if (bytes[0] != 0x03u || bytes[1] != 0x02u || bytes[2] != 0x23u || bytes[3] != 0x07u) {
    return -2;
  }
  return 0;
}

static int webvulkan_validate_wasm_bytes(const uint8_t* bytes, uint32_t byteCount) {
  if (!bytes || byteCount < 8u) {
    return -1;
  }
  if (bytes[0] != 0x00u || bytes[1] != 0x61u || bytes[2] != 0x73u || bytes[3] != 0x6du) {
    return -2;
  }
  return 0;
}

static int webvulkan_find_spirv_entry_index(uint32_t keyLo, uint32_t keyHi) {
  for (uint32_t i = 0u; i < g_runtime_spirv_count; ++i) {
    if (g_runtime_spirv_entries[i].keyLo == keyLo && g_runtime_spirv_entries[i].keyHi == keyHi) {
      return (int)i;
    }
  }
  return -1;
}

static int webvulkan_find_wasm_entry_index(uint32_t keyLo, uint32_t keyHi) {
  for (uint32_t i = 0u; i < g_runtime_wasm_count; ++i) {
    if (g_runtime_wasm_entries[i].keyLo == keyLo && g_runtime_wasm_entries[i].keyHi == keyHi) {
      return (int)i;
    }
  }
  return -1;
}

EMSCRIPTEN_KEEPALIVE void webvulkan_reset_runtime_shader_registry(void) {
  for (uint32_t i = 0u; i < g_runtime_spirv_count; ++i) {
    if (g_runtime_spirv_entries[i].bytes) {
      free(g_runtime_spirv_entries[i].bytes);
      g_runtime_spirv_entries[i].bytes = 0;
    }
  }
  g_runtime_spirv_count = 0u;

  for (uint32_t i = 0u; i < g_runtime_wasm_count; ++i) {
    if (g_runtime_wasm_entries[i].bytes) {
      free(g_runtime_wasm_entries[i].bytes);
      g_runtime_wasm_entries[i].bytes = 0;
    }
  }
  g_runtime_wasm_count = 0u;
  g_runtime_wasm_used = 0;
  webvulkan_copy_string(g_runtime_wasm_provider, WEBVULKAN_RUNTIME_PROVIDER_MAX, "none", "none");
  g_runtime_captured_shader_key_valid = 0;
  g_runtime_captured_shader_key_lo = 0u;
  g_runtime_captured_shader_key_hi = 0u;
}

EMSCRIPTEN_KEEPALIVE int webvulkan_set_runtime_active_shader_key(uint32_t keyLo, uint32_t keyHi) {
  g_runtime_active_shader_key_lo = keyLo;
  g_runtime_active_shader_key_hi = keyHi;
  return 0;
}

EMSCRIPTEN_KEEPALIVE uint32_t webvulkan_get_runtime_active_shader_key_lo(void) {
  return g_runtime_active_shader_key_lo;
}

EMSCRIPTEN_KEEPALIVE uint32_t webvulkan_get_runtime_active_shader_key_hi(void) {
  return g_runtime_active_shader_key_hi;
}

EMSCRIPTEN_KEEPALIVE int webvulkan_set_runtime_expected_dispatch_value(
  uint32_t keyLo,
  uint32_t keyHi,
  uint32_t expectedValue
) {
  int index = webvulkan_find_spirv_entry_index(keyLo, keyHi);
  if (index < 0) {
    return -1;
  }
  WebVulkanRuntimeSpirvEntry* entry = &g_runtime_spirv_entries[(uint32_t)index];
  entry->expectedDispatchValue = expectedValue;
  return 0;
}

EMSCRIPTEN_KEEPALIVE void webvulkan_runtime_reset_captured_shader_key(void) {
  g_runtime_captured_shader_key_valid = 0;
  g_runtime_captured_shader_key_lo = 0u;
  g_runtime_captured_shader_key_hi = 0u;
}

EMSCRIPTEN_KEEPALIVE int webvulkan_runtime_has_captured_shader_key(void) {
  return g_runtime_captured_shader_key_valid;
}

EMSCRIPTEN_KEEPALIVE uint32_t webvulkan_runtime_get_captured_shader_key_lo(void) {
  return g_runtime_captured_shader_key_lo;
}

EMSCRIPTEN_KEEPALIVE uint32_t webvulkan_runtime_get_captured_shader_key_hi(void) {
  return g_runtime_captured_shader_key_hi;
}

EMSCRIPTEN_KEEPALIVE int webvulkan_register_runtime_shader_spirv(
  uint32_t keyLo,
  uint32_t keyHi,
  const uint8_t* bytes,
  uint32_t byteCount,
  const char* entrypoint
) {
  int validateRc = webvulkan_validate_spirv_bytes(bytes, byteCount);
  if (validateRc != 0) {
    return validateRc;
  }

  uint8_t* copy = (uint8_t*)malloc(byteCount);
  if (!copy) {
    return -3;
  }
  memcpy(copy, bytes, byteCount);

  int existingIndex = webvulkan_find_spirv_entry_index(keyLo, keyHi);
  WebVulkanRuntimeSpirvEntry* entry = 0;
  if (existingIndex >= 0) {
    entry = &g_runtime_spirv_entries[(uint32_t)existingIndex];
    if (entry->bytes) {
      free(entry->bytes);
    }
  } else {
    if (g_runtime_spirv_count >= WEBVULKAN_RUNTIME_MAX_MODULES) {
      free(copy);
      return -4;
    }
    entry = &g_runtime_spirv_entries[g_runtime_spirv_count++];
  }

  entry->keyLo = keyLo;
  entry->keyHi = keyHi;
  entry->bytes = copy;
  entry->byteCount = byteCount;
  entry->expectedDispatchValue = keyLo;
  webvulkan_copy_string(entry->entrypoint, WEBVULKAN_RUNTIME_ENTRYPOINT_MAX, entrypoint, "write_const");
  return 0;
}

EMSCRIPTEN_KEEPALIVE int webvulkan_register_runtime_wasm_module(
  uint32_t keyLo,
  uint32_t keyHi,
  const uint8_t* bytes,
  uint32_t byteCount,
  const char* entrypoint,
  const char* provider
) {
  int validateRc = webvulkan_validate_wasm_bytes(bytes, byteCount);
  if (validateRc != 0) {
    return validateRc;
  }

  uint8_t* copy = (uint8_t*)malloc(byteCount);
  if (!copy) {
    return -3;
  }
  memcpy(copy, bytes, byteCount);

  int existingIndex = webvulkan_find_wasm_entry_index(keyLo, keyHi);
  WebVulkanRuntimeWasmEntry* entry = 0;
  if (existingIndex >= 0) {
    entry = &g_runtime_wasm_entries[(uint32_t)existingIndex];
    if (entry->bytes) {
      free(entry->bytes);
    }
  } else {
    if (g_runtime_wasm_count >= WEBVULKAN_RUNTIME_MAX_MODULES) {
      free(copy);
      return -4;
    }
    entry = &g_runtime_wasm_entries[g_runtime_wasm_count++];
  }

  entry->keyLo = keyLo;
  entry->keyHi = keyHi;
  entry->bytes = copy;
  entry->byteCount = byteCount;
  webvulkan_copy_string(entry->entrypoint, WEBVULKAN_RUNTIME_ENTRYPOINT_MAX, entrypoint, "run");
  webvulkan_copy_string(entry->provider, WEBVULKAN_RUNTIME_PROVIDER_MAX, provider, "runtime-registry");
  return 0;
}

EMSCRIPTEN_KEEPALIVE int webvulkan_register_runtime_shader_bundle(
  uint32_t keyLo,
  uint32_t keyHi,
  const uint8_t* spirvBytes,
  uint32_t spirvByteCount,
  const char* spirvEntrypoint,
  const uint8_t* wasmBytes,
  uint32_t wasmByteCount,
  const char* wasmEntrypoint,
  const char* wasmProvider
) {
  int spirvRc = webvulkan_register_runtime_shader_spirv(
    keyLo,
    keyHi,
    spirvBytes,
    spirvByteCount,
    spirvEntrypoint
  );
  if (spirvRc != 0) {
    return spirvRc;
  }

  int wasmRc = webvulkan_register_runtime_wasm_module(
    keyLo,
    keyHi,
    wasmBytes,
    wasmByteCount,
    wasmEntrypoint,
    wasmProvider
  );
  if (wasmRc != 0) {
    return wasmRc;
  }

  return 0;
}

EMSCRIPTEN_KEEPALIVE int webvulkan_get_runtime_wasm_used(void) {
  return g_runtime_wasm_used;
}

EMSCRIPTEN_KEEPALIVE const char* webvulkan_get_runtime_wasm_provider(void) {
  return g_runtime_wasm_provider;
}

EMSCRIPTEN_KEEPALIVE int webvulkan_set_runtime_shader_spirv(const uint8_t* bytes, uint32_t byteCount) {
  return webvulkan_register_runtime_shader_spirv(
    WEBVULKAN_RUNTIME_DEFAULT_SHADER_KEY_LO,
    WEBVULKAN_RUNTIME_DEFAULT_SHADER_KEY_HI,
    bytes,
    byteCount,
    "write_const"
  );
}

bool webvulkan_runtime_lookup_wasm_module(
  uint32_t keyLo,
  uint32_t keyHi,
  const uint8_t** outModuleBytes,
  uint32_t* outModuleSize,
  const char** outEntrypoint,
  const char** outProvider
) {
  if (!outModuleBytes || !outModuleSize || !outEntrypoint || !outProvider) {
    return false;
  }
  int index = webvulkan_find_wasm_entry_index(keyLo, keyHi);
  if (index < 0) {
    return false;
  }
  const WebVulkanRuntimeWasmEntry* entry = &g_runtime_wasm_entries[(uint32_t)index];
  if (!entry->bytes || entry->byteCount == 0u) {
    return false;
  }
  *outModuleBytes = entry->bytes;
  *outModuleSize = entry->byteCount;
  *outEntrypoint = entry->entrypoint;
  *outProvider = entry->provider;
  return true;
}

bool webvulkan_runtime_lookup_spirv_module(
  uint32_t keyLo,
  uint32_t keyHi,
  const uint8_t** outModuleBytes,
  uint32_t* outModuleSize,
  const char** outEntrypoint
) {
  if (!outModuleBytes || !outModuleSize || !outEntrypoint) {
    return false;
  }
  int index = webvulkan_find_spirv_entry_index(keyLo, keyHi);
  if (index < 0) {
    return false;
  }
  const WebVulkanRuntimeSpirvEntry* entry = &g_runtime_spirv_entries[(uint32_t)index];
  if (!entry->bytes || entry->byteCount == 0u) {
    return false;
  }
  *outModuleBytes = entry->bytes;
  *outModuleSize = entry->byteCount;
  *outEntrypoint = entry->entrypoint;
  return true;
}

bool webvulkan_runtime_lookup_expected_dispatch_value(
  uint32_t keyLo,
  uint32_t keyHi,
  uint32_t* outExpectedValue
) {
  if (!outExpectedValue) {
    return false;
  }
  int index = webvulkan_find_spirv_entry_index(keyLo, keyHi);
  if (index < 0) {
    return false;
  }
  const WebVulkanRuntimeSpirvEntry* entry = &g_runtime_spirv_entries[(uint32_t)index];
  *outExpectedValue = entry->expectedDispatchValue;
  return true;
}

void webvulkan_runtime_mark_wasm_usage(int used, const char* provider) {
  g_runtime_wasm_used = used ? 1 : 0;
  webvulkan_copy_string(g_runtime_wasm_provider, WEBVULKAN_RUNTIME_PROVIDER_MAX, provider, "none");
}

void webvulkan_runtime_capture_shader_key(uint32_t keyLo, uint32_t keyHi) {
  g_runtime_captured_shader_key_valid = 1;
  g_runtime_captured_shader_key_lo = keyLo;
  g_runtime_captured_shader_key_hi = keyHi;
}
