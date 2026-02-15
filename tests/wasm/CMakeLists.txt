cmake_minimum_required(VERSION 4.2)

project(wasm_smoke C)

if(NOT CMAKE_TOOLCHAIN_FILE)
  message(FATAL_ERROR "This project must be configured with the Emscripten toolchain")
endif()

add_executable(webgpu_smoke
  src/smoke_main.c
  src/vk_wasm_webgpu_surface.c
)

target_compile_features(webgpu_smoke PRIVATE c_std_11)
target_include_directories(webgpu_smoke PRIVATE include)

target_link_options(webgpu_smoke PRIVATE
  "SHELL:--use-port=emdawnwebgpu"
  "SHELL:-sALLOW_MEMORY_GROWTH=1"
  "SHELL:-sMODULARIZE=1"
  "SHELL:-sEXPORT_ES6=1"
  "SHELL:-sENVIRONMENT=web,worker,node"
  "SHELL:-sEXPORTED_FUNCTIONS=['_main','_wasm_runtime_smoke']"
  "SHELL:-sEXPORTED_RUNTIME_METHODS=['ccall']"
)

set(NODE_HINTS)
if(EMSDK_ROOT)
  if(WIN32)
    file(GLOB EMSDK_NODE_CANDIDATES "${EMSDK_ROOT}/node/*/bin/node.exe")
  else()
    file(GLOB EMSDK_NODE_CANDIDATES "${EMSDK_ROOT}/node/*/bin/node")
  endif()
  if(EMSDK_NODE_CANDIDATES)
    list(SORT EMSDK_NODE_CANDIDATES COMPARE NATURAL ORDER DESCENDING)
    list(GET EMSDK_NODE_CANDIDATES 0 EMSDK_NODE_EXE)
    get_filename_component(EMSDK_NODE_DIR "${EMSDK_NODE_EXE}" DIRECTORY)
    list(APPEND NODE_HINTS "${EMSDK_NODE_DIR}")
  endif()
endif()

find_program(NODE_EXECUTABLE NAMES node node.exe HINTS ${NODE_HINTS})
if(NOT NODE_EXECUTABLE)
  message(FATAL_ERROR "node not found. Install node or use emsdk auto-bootstrap")
endif()

add_custom_command(
  OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/runtime_smoke.ok"
  COMMAND
    "${CMAKE_COMMAND}" -E env
    "SMOKE_MODULE=${CMAKE_CURRENT_BINARY_DIR}/webgpu_smoke.js"
    "${NODE_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tools/smoke_runtime.mjs"
  COMMAND "${CMAKE_COMMAND}" -E touch "${CMAKE_CURRENT_BINARY_DIR}/runtime_smoke.ok"
  DEPENDS webgpu_smoke "${CMAKE_CURRENT_SOURCE_DIR}/tools/smoke_runtime.mjs"
  VERBATIM
)

add_custom_target(runtime_smoke DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/runtime_smoke.ok")
