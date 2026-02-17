<div align="center">
  <img alt="WebVulkan logo" width="200" height="200" src="webvulkan-glow.svg" />
</div>

<p align="center">
  <a href="https://github.com/Devsh-Graphics-Programming/llvmpipe2wasm/actions/workflows/smoke.yml">
    <img src="https://github.com/Devsh-Graphics-Programming/llvmpipe2wasm/actions/workflows/smoke.yml/badge.svg" alt="Build Status" /></a>
  <a href="https://www.gnu.org/licenses/agpl-3.0.en.html">
    <img src="https://img.shields.io/badge/license-AGPL%20v3-blue" alt="License AGPL v3" /></a>
  <a href="https://discord.gg/krsBcABm7u">
    <img src="https://img.shields.io/discord/308323056592486420?label=discord&logo=discord&logoColor=white&color=7289DA" alt="Join our Discord" /></a>
</p>

# WebVulkan - llvmpipe compiled to WASM

This repository provides a CMake-consumable WebAssembly build of Mesa llvmpipe and lavapipe.

## What you get

- CMake targets for wasm llvmpipe and lavapipe
- In-tree consumption with `add_subdirectory`
- Relocatable package consumption with `find_package`
- Runtime shader tooling with DXC in Wasm and Clang in Wasm
- Fast Wasm shader execution path backend (currently validated on compute dispatch)

Public link targets

- `webvulkan::llvmpipe_wasm`
- `webvulkan::lavapipe_wasm`
- `webvulkan::shader_tools`

## What is in Wasm

- Clang in Wasm is available for LLVM IR to Wasm module generation
- DXC in Wasm is available for HLSL to SPIR-V generation

## Why a Mesa fork

Mesa source is fetched from

- `https://github.com/Devsh-Graphics-Programming/mesa`

The fork carries wasm-specific patches required for this flow

- Emscripten build and linking adjustments for lavapipe
- runtime shader key capture and runtime module lookup hooks
- runtime Wasm dispatch integration for shader execution experiments

Non-Emscripten paths are kept intact.

## Why a DXC fork

DXC source is fetched from

- `https://github.com/Devsh-Graphics-Programming/DirectXShaderCompiler/tree/wasm`

The fork carries build-system patches required to compile DXC to Wasm.

## Why no LLVM JIT in Wasm

LLVM maintainers explicitly state LLJIT does not support WebAssembly in this form  
`https://discourse.llvm.org/t/does-lljit-support-webassembly/74864/3`

That blocks the classic in-process ORC LLJIT path inside the Wasm sandbox.

## Why no full NIR to Wasm backend

Mesa does not ship a production NIR to Wasm backend for this use case.
Writing and maintaining one from scratch is a large compiler project.

## Our solution

- Use DXC in Wasm to compile HLSL source to SPIR-V at runtime
- Use Clang in Wasm to compile LLVM IR to Wasm runtime modules
- Build and ship lavapipe and llvmpipe as CMake-consumable wasm targets
- Keep the integration path centered on standard Vulkan API usage through lavapipe

## Wait, but you have a big shader tooling payload

- Yes, that is the point and we are fine with it.
- Tooling artifacts are already distributed in compressed `.zip` bundles.
- Runtime delivery compression is deployment-specific and should be handled by your host or CDN (gzip or brotli).
- If this payload size blocks your use case, sponsor NIR-to-Wasm backend work or LLVM JIT in Wasm R&D and contact Devsh Graphics Programming.

## Backend status disclaimer

This backend is under active development.
Interfaces and behavior can still change while the runtime path is being expanded.

What is implemented now

- Runtime HLSL to SPIR-V compilation in Wasm through DXC
- Runtime LLVM IR to Wasm module compilation in Wasm through Clang
- Driver-side shader key registration and runtime Wasm module lookup
- Fast Wasm execution path validated for Vulkan compute dispatch in CI

What is not implemented yet

- Full Vulkan stage and feature coverage across all real-world shader patterns
- Full conformance and performance tuning for broad hardware and workload sets
- Stable long-term public API for the runtime shader tooling layer
- Shared-driver output mode (current driver artifact is static only)

## How we validate it

We run `runtime_smoke` in CI as the runtime validation test.

The test validates

1. HLSL to SPIR-V compilation with DXC in Wasm.
2. LLVM IR to Wasm module compilation with Clang in Wasm.
3. Runtime registration of SPIR-V and Wasm module for the driver shader key.
4. Vulkan loader flow through Volk using `vk_icdGetInstanceProcAddr` from the wasm ICD path.
5. Vulkan compute pipeline creation and real dispatch execution.
6. Output correctness checks for the dispatched shader.
7. Driver identity and provider checks to confirm the lavapipe wasm path.
8. Timing comparison between both runtime modes on micro and realistic dispatch profiles.

Runtime modes validated in CI

- `lavapipe_runtime_smoke_fast_wasm`
- `lavapipe_runtime_smoke_raw_llvm_ir`

Dispatch profiles validated in CI

- `micro` profile for dispatch-overhead sensitivity
- `realistic` profile for larger dispatch-grid usage

## Gains

On the current local setup we measured

- `micro` profile `fast_wasm` vs `raw_llvm_ir` about `10.55x` lower wall time
- `realistic` profile `fast_wasm` vs `raw_llvm_ir` about `8.04x` lower wall time

Shader behavior in this benchmark is intentionally simple and deterministic.
It runs integer mixing ops and writes `0x12345678` into a storage buffer.

Dispatch details in this run

- Workgroup size in this shader is `numthreads(1,1,1)`
- `micro` profile records `1024` times `vkCmdDispatch(1,1,1)` per submit and runs `16` submits so total dispatch calls are `16384`
- `realistic` profile records `64` times `vkCmdDispatch(4,1,1)` per submit and runs `8` submits so total dispatch calls are `512`
- The shader writes and validates one 32-bit value in the bound storage buffer

Log excerpt

```text
dispatch timing summary
  mode=fast_wasm
  profile=micro
  samples=5
  min_ms=0.000723
  avg_ms=0.001163
  max_ms=0.001605
proof.execute_path=fast_wasm
proof.interpreter=disabled_for_dispatch
proof.llvm_ir_wasm_provider=clang/clang llvm-ir+shared-memory-shim

dispatch timing summary
  mode=raw_llvm_ir
  profile=micro
  samples=5
  min_ms=0.010564
  avg_ms=0.012268
  max_ms=0.013631
proof.execute_path=inline_wasm_module
proof.interpreter=not_used
proof.llvm_ir_wasm_provider=inline-wasm-module

dispatch timing summary
  mode=fast_wasm
  profile=realistic
  samples=5
  min_ms=0.001315
  avg_ms=0.001992
  max_ms=0.002637
proof.execute_path=fast_wasm
proof.interpreter=disabled_for_dispatch
proof.llvm_ir_wasm_provider=clang/clang llvm-ir+shared-memory-shim

dispatch timing summary
  mode=raw_llvm_ir
  profile=realistic
  samples=5
  min_ms=0.012064
  avg_ms=0.016007
  max_ms=0.023143
proof.execute_path=inline_wasm_module
proof.interpreter=not_used
proof.llvm_ir_wasm_provider=inline-wasm-module
```

Fairness note for this measurement

- Local dev PC `AMD Ryzen 5 5600G` `6C/12T`, `Windows 11 Pro 64-bit (10.0.26200)`
- The same benchmark output can be verified in this repository GitHub Actions CI logs

## Licensing

This project is licensed under GNU AGPL v3.

For dual licensing including proprietary or commercial terms contact Devsh Graphics Programming.

## Default dependency mode

Default configuration uses the latest prebuilt LLVM bundle from this repository.

- `llvm-wasm-prebuilt-latest`

To force source LLVM build

```powershell
cmake -S . -B build -G Ninja -DLLVM_PROVIDER=source
```

## Use from source with add_subdirectory

```cmake
add_subdirectory(path/to/llvmpipe2wasm)

add_executable(my_app src/main.c)
target_link_libraries(my_app PRIVATE webvulkan::llvmpipe_wasm)
```

```powershell
cmake -S . -B build -G Ninja
cmake --build build
```

## Use as relocatable package with find_package

Build and install

```powershell
cmake -S . -B build -G Ninja
cmake --build build
cmake --install build --prefix <install-prefix>
```

Consume

```cmake
find_package(WebVulkanLlvmpipeWasm REQUIRED CONFIG)

add_executable(my_app src/main.c)
target_link_libraries(my_app PRIVATE webvulkan::llvmpipe_wasm)
```

## Build-time shader helper

Reusable helper

- `webvulkan_compile_opencl_to_spirv(...)`

Example

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

The same helper is available in package mode.

```cmake
find_package(WebVulkanLlvmpipeWasm REQUIRED CONFIG)

set(MY_SHADER_SPV "${CMAKE_BINARY_DIR}/shaders/write_const.spv")
webvulkan_compile_opencl_to_spirv(
  SOURCE "${CMAKE_SOURCE_DIR}/shaders/write_const.cl"
  OUTPUT "${MY_SHADER_SPV}"
)
```

## Current limitations

- Driver output is static only `.a`
- Shared driver output mode is planned
- This repository is not a full browser rendering framework

## Release channels

- `llvm-wasm-prebuilt-latest` includes LLVM prebuilt bundle only
- `webvulkan-package-latest` includes relocatable CMake package only

Each channel ships

- bundle `.zip`
- checksum `.sha256`
- Sigstore signature `.sig`
- Sigstore certificate `.pem`

Verify a downloaded bundle

```bash
sha256sum -c <bundle>.zip.sha256
cosign verify-blob \
  --signature <bundle>.zip.sig \
  --certificate <bundle>.zip.pem \
  --certificate-identity-regexp "https://github.com/Devsh-Graphics-Programming/llvmpipe2wasm/.*" \
  --certificate-oidc-issuer "https://token.actions.githubusercontent.com" \
  <bundle>.zip
```
