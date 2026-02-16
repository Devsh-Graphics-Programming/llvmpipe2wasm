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
if(NOT DEFINED SMOKE_EXPORT OR "${SMOKE_EXPORT}" STREQUAL "")
  set(SMOKE_EXPORT "_lavapipe_runtime_smoke")
endif()

if(DEFINED SMOKE_INCLUDE_DIRS_SERIALIZED AND NOT "${SMOKE_INCLUDE_DIRS_SERIALIZED}" STREQUAL "")
  string(REPLACE "|" ";" SMOKE_INCLUDE_DIRS "${SMOKE_INCLUDE_DIRS_SERIALIZED}")
endif()
require_var(SMOKE_INCLUDE_DIRS)

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
if(NOT EXISTS "${VOLK_SOURCE}")
  message(FATAL_ERROR "Missing volk source ${VOLK_SOURCE}")
endif()
if(NOT EXISTS "${VOLK_INCLUDE_DIR}/volk.h")
  message(FATAL_ERROR "Missing volk include directory ${VOLK_INCLUDE_DIR}")
endif()

get_filename_component(SMOKE_OUT_DIR "${SMOKE_JS_OUT}" DIRECTORY)
file(MAKE_DIRECTORY "${SMOKE_OUT_DIR}")

set(RSP_FILE "${SMOKE_OUT_DIR}/lavapipe_runtime_smoke.rsp")
file(WRITE "${RSP_FILE}" "")

function(append_rsp ARG_VALUE)
  file(APPEND "${RSP_FILE}" "\"${ARG_VALUE}\"\n")
endfunction()

append_rsp("${SMOKE_SOURCE}")
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
append_rsp("-sEXPORTED_FUNCTIONS=['_main','${SMOKE_EXPORT}']")
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
    "${NODE_EXE}" "${SMOKE_SCRIPT}"
  RESULT_VARIABLE SMOKE_RUN_RESULT
)
if(NOT SMOKE_RUN_RESULT EQUAL 0)
  message(FATAL_ERROR "lavapipe runtime smoke execution failed")
endif()
