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
- Runtime shader tooling path with DXC in Wasm and Clang in Wasm
- Fast Wasm shader execution path backend (currently validated on compute dispatch)

Public link targets

- `webvulkan::llvmpipe_wasm`
- `webvulkan::lavapipe_wasm`
- `webvulkan::shader_tools`
- `webvulkan::runtime_shader_registry`

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

- `webvulkan_compile_hlsl_to_spirv(...)`

Example

```cmake
add_subdirectory(path/to/llvmpipe2wasm)

set(MY_SHADER_SPV "${CMAKE_BINARY_DIR}/shaders/write_const.spv")
set(WEBVULKAN_DXC_WASM_JS "${CMAKE_SOURCE_DIR}/tools/dxc-wasm/dxc.js")
webvulkan_compile_hlsl_to_spirv(
  SOURCE "${CMAKE_SOURCE_DIR}/shaders/write_const.hlsl"
  OUTPUT "${MY_SHADER_SPV}"
  HLSL_ENTRYPOINT "main"
  HLSL_PROFILE "cs_6_0"
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
webvulkan_compile_hlsl_to_spirv(
  SOURCE "${CMAKE_SOURCE_DIR}/shaders/write_const.hlsl"
  OUTPUT "${MY_SHADER_SPV}"
  DXC_WASM_JS "$ENV{WEBVULKAN_DXC_WASM_JS}"
)
```

## Shader compilation modes

Ahead-of-time mode

- Use `webvulkan_compile_hlsl_to_spirv(...)` in CMake to generate SPIR-V during build.

Runtime mode

- The runtime smoke path compiles HLSL to SPIR-V in runtime through DXC in Wasm.
- The same smoke flow then compiles and registers the runtime Wasm module for the same shader key before Vulkan dispatch.
- See `tests/wasm/tools/smoke_runtime.mjs` for the reference runtime orchestration path.

## Runtime shader registry helper

The package exports a CMake helper that attaches the runtime shader registry C source to your target.

- `webvulkan_attach_runtime_shader_registry(TARGET <your_target>)`

Example

```cmake
add_subdirectory(path/to/llvmpipe2wasm)

add_executable(my_app src/main.c)
webvulkan_attach_runtime_shader_registry(TARGET my_app)
target_link_libraries(my_app PRIVATE webvulkan::llvmpipe_wasm)
```

In package mode the same helper is available after `find_package(WebVulkanLlvmpipeWasm REQUIRED CONFIG)`.

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
8. Timing comparison between both runtime modes on CI-validated dispatch profiles.

Runtime modes validated in CI

- `lavapipe_runtime_smoke_fast_wasm`
- `lavapipe_runtime_smoke_raw_llvm_ir`

Dispatch profiles validated in CI

- `dispatch_overhead` profile to stress dispatch call overhead
- `balanced_grid` profile to run a balanced dispatch-grid layout

Extended dispatch profile used in local and explicit smoke runs

- `large_grid` profile to stress large grid coverage per dispatch

## Gains

On the current local setup we measured

- `dispatch_overhead` profile `fast_wasm` vs `raw_llvm_ir` about `6.13x` lower wall time
- `balanced_grid` profile `fast_wasm` vs `raw_llvm_ir` about `12.79x` lower wall time
- `large_grid` profile `fast_wasm` vs `raw_llvm_ir` about `153.10x` lower wall time

CI coverage note

- Default CI smoke validates `dispatch_overhead` and `balanced_grid`.
- `large_grid` is measured through explicit extended smoke target runs.

Shader behavior in this benchmark is intentionally simple and deterministic.
It runs integer mixing ops and writes `0x12345678` into a storage buffer.

Dispatch details in this run

- Workgroup size in this shader is `numthreads(1,1,1)`
- `dispatch_overhead` records `1024` times `vkCmdDispatch(1,1,1)` per submit and runs `16` submits so total dispatch calls are `16384`
- `balanced_grid` records `256` times `vkCmdDispatch(4,1,1)` per submit and runs `16` submits so total dispatch calls are `4096`
- `large_grid` records `1` time `vkCmdDispatch(256,1,1)` per submit and runs `64` submits so total dispatch calls are `64`
- All profiles execute `16384` total shader invocations per run for fair cross-profile comparison
- The shader writes and validates one 32-bit value in the bound storage buffer

<details>
<summary>Log excerpt from extended local run (click to expand)</summary>

```text
dispatch timing summary
  mode=fast_wasm
  profile=dispatch_overhead
  samples=5
  dispatches_per_submit=1024
  submit_iterations=16
  total_dispatches_per_run=16384
  invocations_per_dispatch=1
  total_invocations_per_run=16384
  min_ms=0.001102
  avg_ms=0.001275
  max_ms=0.001553
  min_ns_per_invocation=1101.575
  avg_ns_per_invocation=1274.652
  max_ns_per_invocation=1552.795
proof.execute_path=fast_wasm
proof.interpreter=disabled_for_dispatch
proof.llvm_ir_wasm_provider=clang/clang c-runtime+shared-memory-shim

dispatch timing summary
  mode=raw_llvm_ir
  profile=dispatch_overhead
  samples=5
  dispatches_per_submit=1024
  submit_iterations=16
  total_dispatches_per_run=16384
  invocations_per_dispatch=1
  total_invocations_per_run=16384
  min_ms=0.007003
  avg_ms=0.007820
  max_ms=0.008926
  min_ns_per_invocation=7002.985
  avg_ns_per_invocation=7819.901
  max_ns_per_invocation=8926.306
proof.execute_path=raw_llvm_ir
proof.fast_wasm_provider=raw_llvm_ir

dispatch timing summary
  mode=fast_wasm
  profile=balanced_grid
  samples=5
  dispatches_per_submit=256
  submit_iterations=16
  total_dispatches_per_run=4096
  invocations_per_dispatch=4
  total_invocations_per_run=16384
  min_ms=0.001533
  avg_ms=0.002188
  max_ms=0.002730
  min_ns_per_invocation=383.160
  avg_ns_per_invocation=546.995
  max_ns_per_invocation=682.477
proof.execute_path=fast_wasm
proof.interpreter=disabled_for_dispatch
proof.llvm_ir_wasm_provider=clang/clang c-runtime+shared-memory-shim

dispatch timing summary
  mode=raw_llvm_ir
  profile=balanced_grid
  samples=5
  dispatches_per_submit=256
  submit_iterations=16
  total_dispatches_per_run=4096
  invocations_per_dispatch=4
  total_invocations_per_run=16384
  min_ms=0.026282
  avg_ms=0.027976
  max_ms=0.031003
  min_ns_per_invocation=6570.514
  avg_ns_per_invocation=6994.097
  max_ns_per_invocation=7750.800
proof.execute_path=raw_llvm_ir
proof.fast_wasm_provider=raw_llvm_ir

dispatch timing summary
  mode=fast_wasm
  profile=large_grid
  samples=5
  dispatches_per_submit=1
  submit_iterations=64
  total_dispatches_per_run=64
  invocations_per_dispatch=256
  total_invocations_per_run=16384
  min_ms=0.010723
  avg_ms=0.017264
  max_ms=0.024392
  min_ns_per_invocation=41.888
  avg_ns_per_invocation=67.439
  max_ns_per_invocation=95.282
proof.execute_path=fast_wasm
proof.interpreter=disabled_for_dispatch
proof.llvm_ir_wasm_provider=clang/clang c-runtime+shared-memory-shim

dispatch timing summary
  mode=raw_llvm_ir
  profile=large_grid
  samples=5
  dispatches_per_submit=1
  submit_iterations=64
  total_dispatches_per_run=64
  invocations_per_dispatch=256
  total_invocations_per_run=16384
  min_ms=2.298788
  avg_ms=2.643183
  max_ms=3.019331
  min_ns_per_invocation=8979.639
  avg_ns_per_invocation=10324.934
  max_ns_per_invocation=11794.263
proof.execute_path=raw_llvm_ir
proof.fast_wasm_provider=raw_llvm_ir
```
</details>

Fairness note for this measurement

- Local dev PC `AMD Ryzen 5 5600G` `6C/12T`, `Windows 11 Pro 64-bit (10.0.26200)`
- CI logs validate the same mode comparison for `dispatch_overhead` and `balanced_grid`
- `large_grid` numbers come from explicit extended local smoke run

## Current limitations

- Driver output is static only `.a`
- Shared driver output mode is planned
- This repository is not a full browser rendering framework

## Release channels

- `llvm-wasm-prebuilt-latest` includes LLVM prebuilt bundle only
- `webvulkan-package-latest` includes relocatable CMake package only
- `dxc-wasm-prebuilt-latest` includes DXC-in-Wasm prebuilt bundle only

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

## Licensing

This project is licensed under GNU AGPL v3.

For dual licensing including proprietary or commercial terms contact Devsh Graphics Programming.
