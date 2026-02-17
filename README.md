<div align="center">
  <img alt="WebVulkan logo" width="200" height="200" src="webvulkan-glow.svg" />
</div>

<p align="center">
  <a href="https://github.com/Devsh-Graphics-Programming/llvmpipe2wasm/actions/workflows/smoke.yml">
    <img src="https://github.com/Devsh-Graphics-Programming/llvmpipe2wasm/actions/workflows/smoke.yml/badge.svg" alt="Build Status" /></a>
  <a href="https://opensource.org/licenses/Apache-2.0">
    <img src="https://img.shields.io/badge/license-Apache%202.0-blue" alt="License: Apache 2.0" /></a>
  <a href="https://discord.gg/krsBcABm7u">
    <img src="https://img.shields.io/discord/308323056592486420?label=discord&logo=discord&logoColor=white&color=7289DA" alt="Join our Discord" /></a>
</p>

# WebVulkan - llvmpipe compiled to WASM

This repository provides a CMake-consumable WebAssembly build of Mesa llvmpipe and lavapipe.

Public link targets:

- `webvulkan::llvmpipe_wasm`
- `webvulkan::lavapipe_wasm`
- `webvulkan::shader_tools`

## Scope

What this repository does:
- Builds and exposes Mesa llvmpipe/lavapipe wasm archives as CMake targets
- Supports in-tree consumption via `add_subdirectory`
- Supports relocatable package consumption via `find_package`
- Provides runtime smoke checks for CI validation

What this repository does not do for consumers:
- It is not a full Vulkan browser runtime
- It does not provide a browser app framework or rendering engine
- It does not replace the consumer's own Vulkan/WebGPU integration layer

Current build mode:
- Driver output is currently static-only (`.a`)
- Shared driver output mode is planned

## Mesa fork

This project fetches Mesa from our fork:
- `https://github.com/Devsh-Graphics-Programming/mesa`

Why we use a fork:
- We carry a small set of wasm-focused patches needed for this project.
- Current patch scope is focused on lavapipe build behavior for Emscripten so the driver can be consumed in our wasm flow.
- Non-Emscripten paths remain unchanged.

## Shader toolchain direction

Why not LLVM JIT-in-Wasm right now:
- LLVM maintainer discussion: `https://discourse.llvm.org/t/rfc-building-llvm-for-webassembly/79073`
- In this project setup, a production-grade in-process LLVM JIT path inside the Wasm sandbox is not currently a practical path.

Why not full NIR->Wasm backend today:
- Mesa does not ship a complete NIR->Wasm backend for this use case.
- A full backend from scratch is a large compiler project and not realistic for a single maintainer effort.

Current strategy:
- Keep `clang_wasm_runtime_smoke` as a clang-in-wasm toolchain proof path.
- Keep Vulkan runtime validation on the current Mesa wasm execution path, while preparing a future direct integration path.

## Default dependency mode

Default configuration uses the latest prebuilt LLVM bundle from this repository:

- `llvm-wasm-prebuilt-latest` release channel

To force source LLVM build:

```powershell
cmake -S . -B build -G Ninja -DLLVM_PROVIDER=source
```

## Use from source (`add_subdirectory`)

```cmake
add_subdirectory(path/to/llvmpipe2wasm)

add_executable(my_app src/main.c)
target_link_libraries(my_app PRIVATE webvulkan::llvmpipe_wasm)
```

```powershell
cmake -S . -B build -G Ninja
cmake --build build
```

## Build-time shader toolchain helper

This repository exposes a reusable CMake helper:
- `webvulkan_compile_opencl_to_spirv(...)`

It compiles OpenCL C source to SPIR-V through Wasmer package runtime (`clspv` entrypoint by default), and can be used directly in consumer builds.

Example:

```cmake
add_subdirectory(path/to/llvmpipe2wasm)

set(MY_SHADER_SPV "${CMAKE_BINARY_DIR}/shaders/write_const.spv")
webvulkan_compile_opencl_to_spirv(
  SOURCE "${CMAKE_SOURCE_DIR}/shaders/write_const.cl"
  OUTPUT "${MY_SHADER_SPV}"
)

add_custom_target(my_shaders DEPENDS "${MY_SHADER_SPV}")
add_executable(my_app src/main.c)
add_dependencies(my_app my_shaders)
target_link_libraries(my_app PRIVATE webvulkan::llvmpipe_wasm)
```

## Use as relocatable package (`find_package`)

Build and install:

```powershell
cmake -S . -B build -G Ninja
cmake --build build
cmake --install build --prefix <install-prefix>
```

Consume:

```cmake
find_package(WebVulkanLlvmpipeWasm REQUIRED CONFIG)

add_executable(my_app src/main.c)
target_link_libraries(my_app PRIVATE webvulkan::llvmpipe_wasm)
```

The same shader helper is available in package mode:

```cmake
find_package(WebVulkanLlvmpipeWasm REQUIRED CONFIG)

set(MY_SHADER_SPV "${CMAKE_BINARY_DIR}/shaders/write_const.spv")
webvulkan_compile_opencl_to_spirv(
  SOURCE "${CMAKE_SOURCE_DIR}/shaders/write_const.cl"
  OUTPUT "${MY_SHADER_SPV}"
)
```

## Volk smoke path

`runtime_smoke` includes a dedicated Volk-based runtime check.

It validates:
- `volkInitializeCustom` is used with `vk_icdGetInstanceProcAddr` from the linked wasm driver archive
- Vulkan instance creation and physical device enumeration succeed
- Required ICD entrypoints are resolved through that same dispatch path

This gives a realistic loader flow for consumers that use Volk, while keeping dispatch pinned to the wasm ICD path.

Current shader-path split:
- `lavapipe_runtime_smoke` injects runtime-generated SPIR-V (from Wasmer `clspv` by default) into the smoke module and validates Vulkan compute dispatch in the Mesa wasm driver.
- `clang_wasm_runtime_smoke` validates the clang-in-wasm toolchain path and runs an SPIR-V probe through a Wasmer runtime command (`clspv` package entrypoint by default).
- If that command is unavailable, smoke falls back to a direct `--target=spirv32` probe and reports the provider and failure reason.

## Release channels

- `llvm-wasm-prebuilt-latest` contains only the LLVM prebuilt bundle
- `webvulkan-package-latest` contains only the relocatable CMake package

Each channel ships:
- bundle `.zip`
- checksum `.sha256`
- Sigstore signature `.sig`
- Sigstore certificate `.pem`

Verify a downloaded bundle:

```bash
sha256sum -c <bundle>.zip.sha256
cosign verify-blob \
  --signature <bundle>.zip.sig \
  --certificate <bundle>.zip.pem \
  --certificate-identity-regexp "https://github.com/Devsh-Graphics-Programming/llvmpipe2wasm/.*" \
  --certificate-oidc-issuer "https://token.actions.githubusercontent.com" \
  <bundle>.zip
```
