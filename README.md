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
