# WebVulkan - llvmpipe compiled to WASM

This repository provides a CMake-consumable WebAssembly build of Mesa llvmpipe/lavapipe.

Public link targets:

- `webvulkan::llvmpipe_wasm`
- `webvulkan::lavapipe_wasm` (alias)

## Use from source (`add_subdirectory`)

```cmake
add_subdirectory(path/to/llvmpipe2wasm)

add_executable(my_app src/main.c)
target_link_libraries(my_app PRIVATE webvulkan::llvmpipe_wasm)
```

Configure/build:

```powershell
cmake -S . -B build -G Ninja
cmake --build build
```

## Use as relocatable package (`find_package`)

Build and install:

```powershell
cmake -S . -B build -G Ninja
cmake --build build
cmake --install build --prefix <install-prefix>
```

`cmake --install` expects build artifacts to already exist and does not trigger a build.

Default configuration uses a prebuilt LLVM bundle from this repository release.

To force LLVM build from source:

```powershell
cmake -S . -B build -G Ninja -DLLVM_PROVIDER=source
```

Consume:

```cmake
find_package(WebVulkanLlvmpipeWasm REQUIRED CONFIG)

add_executable(my_app src/main.c)
target_link_libraries(my_app PRIVATE webvulkan::llvmpipe_wasm)
```
