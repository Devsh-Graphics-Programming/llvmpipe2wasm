if(NOT TARGET llvmpipe_wasm_spike)
  message(FATAL_ERROR "tests require llvmpipe_wasm_spike target from the parent project")
endif()

set(WEBVULKAN_WASM_SMOKE_OK "${WASM_BUILD_DIR}/runtime_smoke.ok")
add_custom_command(
  OUTPUT "${WEBVULKAN_WASM_SMOKE_OK}"
  COMMAND
    "${CMAKE_COMMAND}" -S "${CMAKE_CURRENT_LIST_DIR}/wasm" -B "${WASM_BUILD_DIR}"
    -G "${SUBBUILD_GENERATOR}"
    -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
    -DCMAKE_TOOLCHAIN_FILE=${WEBVULKAN_EMSCRIPTEN_TOOLCHAIN_FILE}
    -DEMSDK_ROOT=${EMSDK_ROOT}
  COMMAND "${CMAKE_COMMAND}" --build "${WASM_BUILD_DIR}" --target runtime_smoke
  DEPENDS
    "${CMAKE_CURRENT_LIST_DIR}/wasm/CMakeLists.txt"
    "${CMAKE_CURRENT_LIST_DIR}/wasm/src/smoke_main.c"
    "${CMAKE_CURRENT_LIST_DIR}/wasm/src/vk_wasm_webgpu_surface.c"
    "${CMAKE_CURRENT_LIST_DIR}/wasm/tools/smoke_runtime.mjs"
  USES_TERMINAL
  VERBATIM
)
add_custom_target(wasm_runtime_smoke DEPENDS "${WEBVULKAN_WASM_SMOKE_OK}")

set(WEBVULKAN_LAVAPIPE_SMOKE_OK "${CMAKE_BINARY_DIR}/lavapipe_runtime_smoke.ok")
set(WEBVULKAN_LAVAPIPE_SMOKE_JS "${CMAKE_BINARY_DIR}/lavapipe-smoke/lavapipe_runtime_smoke.js")
add_custom_command(
  OUTPUT "${WEBVULKAN_LAVAPIPE_SMOKE_OK}"
  COMMAND
    "${CMAKE_COMMAND}"
    -DEMSDK_ROOT=${EMSDK_ROOT}
    -DMESA_SRC_DIR=${MESA_SRC_DIR}
    -DMESA_BUILD_DIR=${MESA_WASM_BUILD_DIR}
    -DDRIVER_ARCHIVE=${WEBVULKAN_DRIVER_ARCHIVE}
    -DSMOKE_SOURCE=${CMAKE_CURRENT_LIST_DIR}/wasm/src/lavapipe_runtime_smoke.c
    -DSMOKE_JS_OUT=${WEBVULKAN_LAVAPIPE_SMOKE_JS}
    -DSMOKE_SCRIPT=${CMAKE_CURRENT_LIST_DIR}/wasm/tools/smoke_runtime.mjs
    -P "${CMAKE_CURRENT_LIST_DIR}/RunLavapipeRuntimeSmoke.cmake"
  COMMAND "${CMAKE_COMMAND}" -E touch "${WEBVULKAN_LAVAPIPE_SMOKE_OK}"
  DEPENDS
    llvmpipe_wasm_spike
    "${CMAKE_CURRENT_LIST_DIR}/RunLavapipeRuntimeSmoke.cmake"
    "${CMAKE_CURRENT_LIST_DIR}/wasm/src/lavapipe_runtime_smoke.c"
    "${CMAKE_CURRENT_LIST_DIR}/wasm/tools/smoke_runtime.mjs"
  USES_TERMINAL
  VERBATIM
)
add_custom_target(lavapipe_runtime_smoke DEPENDS "${WEBVULKAN_LAVAPIPE_SMOKE_OK}")

add_custom_target(runtime_smoke)
add_dependencies(runtime_smoke wasm_runtime_smoke lavapipe_runtime_smoke)
