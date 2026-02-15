set(CMAKE_SYSTEM_NAME Emscripten)
set(CMAKE_SYSTEM_VERSION 1)

list(APPEND CMAKE_TRY_COMPILE_PLATFORM_VARIABLES EMSDK_ROOT)

if(NOT EMSDK_ROOT AND DEFINED ENV{EMSDK} AND NOT "$ENV{EMSDK}" STREQUAL "")
  set(EMSDK_ROOT "$ENV{EMSDK}")
endif()

if(NOT EMSDK_ROOT)
  message(FATAL_ERROR "EMSDK_ROOT must be set for WebVulkanEmscripten.cmake")
endif()

set(EMSCRIPTEN_ROOT_PATH "${EMSDK_ROOT}/upstream/emscripten")
if(NOT EXISTS "${EMSCRIPTEN_ROOT_PATH}/cmake/Modules/Platform/Emscripten.cmake")
  message(FATAL_ERROR "Emscripten.cmake not found under ${EMSCRIPTEN_ROOT_PATH}")
endif()

include("${EMSCRIPTEN_ROOT_PATH}/cmake/Modules/Platform/Emscripten.cmake")
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE BOTH)
