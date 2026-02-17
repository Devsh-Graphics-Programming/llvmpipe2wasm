cmake_minimum_required(VERSION 4.2)

function(require_var VAR_NAME)
  if(NOT DEFINED ${VAR_NAME} OR "${${VAR_NAME}}" STREQUAL "")
    message(FATAL_ERROR "${VAR_NAME} is required")
  endif()
endfunction()

require_var(EMSDK_ROOT)
require_var(DRIVER_ARCHIVE)
require_var(SMOKE_SOURCE)
require_var(SMOKE_JS_OUT)
require_var(SMOKE_SCRIPT)
require_var(VOLK_INCLUDE_DIR)
require_var(VOLK_SOURCE)
if(NOT DEFINED SMOKE_REQUIRE_RUNTIME_SPIRV OR "${SMOKE_REQUIRE_RUNTIME_SPIRV}" STREQUAL "")
  set(SMOKE_REQUIRE_RUNTIME_SPIRV "0")
endif()
if(NOT DEFINED SMOKE_RUNTIME_MODE OR "${SMOKE_RUNTIME_MODE}" STREQUAL "")
  set(SMOKE_RUNTIME_MODE "fast_wasm")
endif()
if(NOT SMOKE_RUNTIME_MODE STREQUAL "fast_wasm" AND NOT SMOKE_RUNTIME_MODE STREQUAL "raw_llvm_ir")
  message(FATAL_ERROR "SMOKE_RUNTIME_MODE must be fast_wasm or raw_llvm_ir")
endif()
if(NOT DEFINED SMOKE_RUNTIME_BENCH_ITERATIONS OR "${SMOKE_RUNTIME_BENCH_ITERATIONS}" STREQUAL "")
  set(SMOKE_RUNTIME_BENCH_ITERATIONS "5")
endif()
if(NOT DEFINED SMOKE_RUNTIME_WARMUP_ITERATIONS OR "${SMOKE_RUNTIME_WARMUP_ITERATIONS}" STREQUAL "")
  set(SMOKE_RUNTIME_WARMUP_ITERATIONS "1")
endif()
if(NOT DEFINED SMOKE_RUNTIME_BENCH_PROFILE OR "${SMOKE_RUNTIME_BENCH_PROFILE}" STREQUAL "")
  set(SMOKE_RUNTIME_BENCH_PROFILE "micro")
endif()
if(NOT SMOKE_RUNTIME_BENCH_PROFILE STREQUAL "micro"
   AND NOT SMOKE_RUNTIME_BENCH_PROFILE STREQUAL "realistic"
   AND NOT SMOKE_RUNTIME_BENCH_PROFILE STREQUAL "hot_loop_single_dispatch")
  message(FATAL_ERROR "SMOKE_RUNTIME_BENCH_PROFILE must be micro, realistic, or hot_loop_single_dispatch")
endif()
if(NOT DEFINED SMOKE_CLANG_WASM_PACKAGE OR "${SMOKE_CLANG_WASM_PACKAGE}" STREQUAL "")
  set(SMOKE_CLANG_WASM_PACKAGE "clang/clang")
endif()
if(NOT DEFINED SMOKE_RUNTIME_SHADER_WORKLOAD OR "${SMOKE_RUNTIME_SHADER_WORKLOAD}" STREQUAL "")
  set(SMOKE_RUNTIME_SHADER_WORKLOAD "write_const")
endif()
if(NOT SMOKE_RUNTIME_SHADER_WORKLOAD STREQUAL "write_const"
   AND NOT SMOKE_RUNTIME_SHADER_WORKLOAD STREQUAL "atomic_single_counter"
   AND NOT SMOKE_RUNTIME_SHADER_WORKLOAD STREQUAL "atomic_per_workgroup"
   AND NOT SMOKE_RUNTIME_SHADER_WORKLOAD STREQUAL "no_race_unique_writes")
  message(FATAL_ERROR
    "SMOKE_RUNTIME_SHADER_WORKLOAD must be write_const, atomic_single_counter, atomic_per_workgroup, or no_race_unique_writes")
endif()
if(NOT DEFINED SMOKE_SPIRV_WASM_PACKAGE OR "${SMOKE_SPIRV_WASM_PACKAGE}" STREQUAL "")
  set(SMOKE_SPIRV_WASM_PACKAGE "lights0123/llvm-spir")
endif()
if(NOT DEFINED SMOKE_SPIRV_WASM_ENTRYPOINT OR "${SMOKE_SPIRV_WASM_ENTRYPOINT}" STREQUAL "")
  set(SMOKE_SPIRV_WASM_ENTRYPOINT "clspv")
endif()
if(SMOKE_REQUIRE_RUNTIME_SPIRV STREQUAL "1")
  require_var(SMOKE_WASMER_BIN)
  require_var(SMOKE_DXC_WASM_JS)
endif()
if(NOT DEFINED SMOKE_EXPORT OR "${SMOKE_EXPORT}" STREQUAL "")
  set(SMOKE_EXPORT "_lavapipe_runtime_smoke")
endif()

if(DEFINED SMOKE_INCLUDE_DIRS_SERIALIZED AND NOT "${SMOKE_INCLUDE_DIRS_SERIALIZED}" STREQUAL "")
  string(REPLACE "|" ";" SMOKE_INCLUDE_DIRS "${SMOKE_INCLUDE_DIRS_SERIALIZED}")
endif()
require_var(SMOKE_INCLUDE_DIRS)
if(DEFINED SMOKE_EXTRA_SOURCES_SERIALIZED AND NOT "${SMOKE_EXTRA_SOURCES_SERIALIZED}" STREQUAL "")
  string(REPLACE "|" ";" SMOKE_EXTRA_SOURCES "${SMOKE_EXTRA_SOURCES_SERIALIZED}")
endif()

if(CMAKE_HOST_WIN32)
  set(EMCC_BIN "${EMSDK_ROOT}/upstream/emscripten/emcc.bat")
  file(GLOB NODE_CANDIDATES "${EMSDK_ROOT}/node/*/bin/node.exe")
else()
  set(EMCC_BIN "${EMSDK_ROOT}/upstream/emscripten/emcc")
  file(GLOB NODE_CANDIDATES "${EMSDK_ROOT}/node/*/bin/node")
endif()

if(NOT EXISTS "${EMCC_BIN}")
  message(FATAL_ERROR "emcc not found at ${EMCC_BIN}")
endif()

if(NOT NODE_CANDIDATES)
  message(FATAL_ERROR "Node from emsdk was not found")
endif()
list(SORT NODE_CANDIDATES COMPARE NATURAL ORDER DESCENDING)
list(GET NODE_CANDIDATES 0 NODE_EXE)

if(NOT EXISTS "${DRIVER_ARCHIVE}")
  message(FATAL_ERROR "Missing driver archive ${DRIVER_ARCHIVE}")
endif()
foreach(SMOKE_EXTRA_SOURCE IN LISTS SMOKE_EXTRA_SOURCES)
  if(NOT EXISTS "${SMOKE_EXTRA_SOURCE}")
    message(FATAL_ERROR "Missing smoke extra source ${SMOKE_EXTRA_SOURCE}")
  endif()
endforeach()
if(NOT EXISTS "${VOLK_SOURCE}")
  message(FATAL_ERROR "Missing volk source ${VOLK_SOURCE}")
endif()
if(NOT EXISTS "${VOLK_INCLUDE_DIR}/volk.h")
  message(FATAL_ERROR "Missing volk include directory ${VOLK_INCLUDE_DIR}")
endif()
if(SMOKE_REQUIRE_RUNTIME_SPIRV STREQUAL "1")
  if(NOT EXISTS "${SMOKE_DXC_WASM_JS}")
    message(FATAL_ERROR "SMOKE_DXC_WASM_JS does not exist: ${SMOKE_DXC_WASM_JS}")
  endif()
endif()

get_filename_component(SMOKE_OUT_DIR "${SMOKE_JS_OUT}" DIRECTORY)
file(MAKE_DIRECTORY "${SMOKE_OUT_DIR}")

set(RSP_FILE "${SMOKE_OUT_DIR}/lavapipe_runtime_smoke.rsp")
file(WRITE "${RSP_FILE}" "")

function(append_rsp ARG_VALUE)
  file(APPEND "${RSP_FILE}" "\"${ARG_VALUE}\"\n")
endfunction()

append_rsp("${SMOKE_SOURCE}")
foreach(SMOKE_EXTRA_SOURCE IN LISTS SMOKE_EXTRA_SOURCES)
  append_rsp("${SMOKE_EXTRA_SOURCE}")
endforeach()
append_rsp("${VOLK_SOURCE}")
append_rsp("-o")
append_rsp("${SMOKE_JS_OUT}")
append_rsp("-std=c11")
append_rsp("-I${VOLK_INCLUDE_DIR}")
foreach(SMOKE_INCLUDE_DIR IN LISTS SMOKE_INCLUDE_DIRS)
  if(NOT EXISTS "${SMOKE_INCLUDE_DIR}")
    message(FATAL_ERROR "Missing include directory ${SMOKE_INCLUDE_DIR}")
  endif()
  append_rsp("-I${SMOKE_INCLUDE_DIR}")
endforeach()
append_rsp("-sALLOW_MEMORY_GROWTH=1")
append_rsp("-sMODULARIZE=1")
append_rsp("-sEXPORT_ES6=1")
append_rsp("-sENVIRONMENT=web,worker,node")
if(SMOKE_REQUIRE_RUNTIME_SPIRV STREQUAL "1")
  append_rsp("-sEXPORTED_FUNCTIONS=['_main','${SMOKE_EXPORT}','_webvulkan_reset_runtime_shader_registry','_webvulkan_set_runtime_active_shader_key','_webvulkan_set_runtime_dispatch_mode','_webvulkan_get_runtime_dispatch_mode','_webvulkan_set_runtime_expected_dispatch_value','_webvulkan_runtime_reset_captured_shader_key','_webvulkan_runtime_has_captured_shader_key','_webvulkan_runtime_get_captured_shader_key_lo','_webvulkan_runtime_get_captured_shader_key_hi','_webvulkan_set_runtime_shader_spirv','_webvulkan_register_runtime_shader_spirv','_webvulkan_register_runtime_wasm_module','_webvulkan_register_runtime_shader_bundle','_webvulkan_get_runtime_wasm_used','_webvulkan_get_runtime_wasm_provider','_webvulkan_set_runtime_bench_profile','_webvulkan_get_runtime_bench_profile','_webvulkan_set_runtime_shader_workload','_webvulkan_get_runtime_shader_workload','_webvulkan_get_last_dispatch_ms','_malloc','_free']")
else()
  append_rsp("-sEXPORTED_FUNCTIONS=['_main','${SMOKE_EXPORT}']")
endif()
append_rsp("-sEXPORTED_RUNTIME_METHODS=['ccall']")
append_rsp("-sMAIN_MODULE=2")
append_rsp("-sALLOW_TABLE_GROWTH=1")
append_rsp("-Wl,--allow-multiple-definition")
append_rsp("-sASSERTIONS=2")
append_rsp("-sSTACK_OVERFLOW_CHECK=2")
append_rsp("--profiling-funcs")
append_rsp("${DRIVER_ARCHIVE}")

execute_process(
  COMMAND "${EMCC_BIN}" "@${RSP_FILE}"
  RESULT_VARIABLE SMOKE_BUILD_RESULT
)
if(NOT SMOKE_BUILD_RESULT EQUAL 0)
  message(FATAL_ERROR "Failed to build lavapipe runtime smoke")
endif()

execute_process(
  COMMAND "${CMAKE_COMMAND}" -E env
    "SMOKE_MODULE=${SMOKE_JS_OUT}"
    "SMOKE_EXPORT=${SMOKE_EXPORT}"
    "SMOKE_REQUIRE_RUNTIME_SPIRV=${SMOKE_REQUIRE_RUNTIME_SPIRV}"
    "WEBVULKAN_RUNTIME_EXECUTION_MODE=${SMOKE_RUNTIME_MODE}"
    "WEBVULKAN_RUNTIME_BENCH_ITERATIONS=${SMOKE_RUNTIME_BENCH_ITERATIONS}"
    "WEBVULKAN_RUNTIME_WARMUP_ITERATIONS=${SMOKE_RUNTIME_WARMUP_ITERATIONS}"
    "WEBVULKAN_RUNTIME_BENCH_PROFILE=${SMOKE_RUNTIME_BENCH_PROFILE}"
    "WEBVULKAN_RUNTIME_SHADER_WORKLOAD=${SMOKE_RUNTIME_SHADER_WORKLOAD}"
    "WEBVULKAN_CLANG_WASM_PACKAGE=${SMOKE_CLANG_WASM_PACKAGE}"
    "WEBVULKAN_SPIRV_WASM_PACKAGE=${SMOKE_SPIRV_WASM_PACKAGE}"
    "WEBVULKAN_SPIRV_WASM_ENTRYPOINT=${SMOKE_SPIRV_WASM_ENTRYPOINT}"
    "WEBVULKAN_WASMER_BIN=${SMOKE_WASMER_BIN}"
    "WEBVULKAN_DXC_WASM_JS=${SMOKE_DXC_WASM_JS}"
    "${NODE_EXE}" "${SMOKE_SCRIPT}"
  RESULT_VARIABLE SMOKE_RUN_RESULT
)
if(NOT SMOKE_RUN_RESULT EQUAL 0)
  message(FATAL_ERROR "lavapipe runtime smoke execution failed")
endif()
