#ifndef WEBVULKAN_SHADER_RUNTIME_REGISTRY_H
#define WEBVULKAN_SHADER_RUNTIME_REGISTRY_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define WEBVULKAN_RUNTIME_DEFAULT_SHADER_KEY_LO 0x12345678u
#define WEBVULKAN_RUNTIME_DEFAULT_SHADER_KEY_HI 0u
#define WEBVULKAN_RUNTIME_DISPATCH_MODE_RAW_LLVM_IR 0u
#define WEBVULKAN_RUNTIME_DISPATCH_MODE_FAST_WASM 1u
#define WEBVULKAN_RUNTIME_SHADER_BUNDLE_HAS_WASM 0x1u
#define WEBVULKAN_RUNTIME_SHADER_BUNDLE_HAS_EXPECTED_VALUE 0x2u

typedef struct WebVulkanRuntimeShaderBundle_t {
  uint32_t keyLo;
  uint32_t keyHi;
  const uint8_t* spirvBytes;
  uint32_t spirvByteCount;
  const char* spirvEntrypoint;
  const uint8_t* wasmBytes;
  uint32_t wasmByteCount;
  const char* wasmEntrypoint;
  const char* wasmProvider;
  uint32_t expectedDispatchValue;
  uint32_t flags;
} WebVulkanRuntimeShaderBundle;

int webvulkan_runtime_register_shader_bundle(const WebVulkanRuntimeShaderBundle* bundle);
int webvulkan_runtime_register_shader_bundles(const WebVulkanRuntimeShaderBundle* bundles, uint32_t bundleCount);
int webvulkan_runtime_register_shader_bundle_params(
  uint32_t keyLo,
  uint32_t keyHi,
  const uint8_t* spirvBytes,
  uint32_t spirvByteCount,
  const char* spirvEntrypoint,
  const uint8_t* wasmBytes,
  uint32_t wasmByteCount,
  const char* wasmEntrypoint,
  const char* wasmProvider,
  uint32_t expectedDispatchValue,
  uint32_t flags
);
int webvulkan_runtime_unregister_shader_bundle(uint32_t keyLo, uint32_t keyHi);
void webvulkan_runtime_clear_shader_bundles(void);
uint32_t webvulkan_runtime_get_registered_spirv_count(void);
uint32_t webvulkan_runtime_get_registered_wasm_count(void);
int webvulkan_runtime_set_active_shader_bundle(uint32_t keyLo, uint32_t keyHi);
int webvulkan_runtime_set_dispatch_mode_fast_wasm(int enabled);

int webvulkan_register_runtime_shader_spirv(
  uint32_t keyLo,
  uint32_t keyHi,
  const uint8_t* bytes,
  uint32_t byteCount,
  const char* entrypoint
);

int webvulkan_register_runtime_wasm_module(
  uint32_t keyLo,
  uint32_t keyHi,
  const uint8_t* bytes,
  uint32_t byteCount,
  const char* entrypoint,
  const char* provider
);

int webvulkan_register_runtime_shader_bundle(
  uint32_t keyLo,
  uint32_t keyHi,
  const uint8_t* spirvBytes,
  uint32_t spirvByteCount,
  const char* spirvEntrypoint,
  const uint8_t* wasmBytes,
  uint32_t wasmByteCount,
  const char* wasmEntrypoint,
  const char* wasmProvider
);

void webvulkan_reset_runtime_shader_registry(void);
int webvulkan_set_runtime_active_shader_key(uint32_t keyLo, uint32_t keyHi);
uint32_t webvulkan_get_runtime_active_shader_key_lo(void);
uint32_t webvulkan_get_runtime_active_shader_key_hi(void);
int webvulkan_set_runtime_dispatch_mode(uint32_t mode);
uint32_t webvulkan_get_runtime_dispatch_mode(void);
int webvulkan_set_runtime_expected_dispatch_value(uint32_t keyLo, uint32_t keyHi, uint32_t expectedValue);
void webvulkan_runtime_reset_captured_shader_key(void);
int webvulkan_runtime_has_captured_shader_key(void);
uint32_t webvulkan_runtime_get_captured_shader_key_lo(void);
uint32_t webvulkan_runtime_get_captured_shader_key_hi(void);
int webvulkan_get_runtime_wasm_used(void);
const char* webvulkan_get_runtime_wasm_provider(void);

bool webvulkan_runtime_lookup_wasm_module(
  uint32_t keyLo,
  uint32_t keyHi,
  const uint8_t** outModuleBytes,
  uint32_t* outModuleSize,
  const char** outEntrypoint,
  const char** outProvider
);

bool webvulkan_runtime_lookup_spirv_module(
  uint32_t keyLo,
  uint32_t keyHi,
  const uint8_t** outModuleBytes,
  uint32_t* outModuleSize,
  const char** outEntrypoint
);

bool webvulkan_runtime_lookup_expected_dispatch_value(
  uint32_t keyLo,
  uint32_t keyHi,
  uint32_t* outExpectedValue
);

void webvulkan_runtime_mark_wasm_usage(int used, const char* provider);
void webvulkan_runtime_capture_shader_key(uint32_t keyLo, uint32_t keyHi);
int webvulkan_runtime_fast_wasm_enabled(void);
int webvulkan_set_runtime_shader_spirv(const uint8_t* bytes, uint32_t byteCount);

#ifdef __cplusplus
}
#endif

#endif
