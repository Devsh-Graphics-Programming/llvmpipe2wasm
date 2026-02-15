cmake_minimum_required(VERSION 4.2)

project(webvulkan_llvmpipe_wasm LANGUAGES NONE)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

option(WASM_AUTO_FETCH_TOOLS "Auto-download missing tools during configure" ON)
option(WEBVULKAN_BUILD_TESTS "Build maintainer smoke tests" OFF)
option(WEBVULKAN_ALLOW_DEP_UPDATES "Allow dependency git update during configure" OFF)
set(WASM_EMSDK_VERSION "latest" CACHE STRING "emsdk version or tag")
set(WASM_EMSDK_GIT_URL "https://github.com/emscripten-core/emsdk.git" CACHE STRING "emsdk git repository URL")
set(WASM_EMSDK_GIT_REF "14c18b569f55138fe4963924162244251f454fb0" CACHE STRING "Pinned emsdk git ref")
set(WEBVULKAN_THIRDPARTY_DIR "${PROJECT_SOURCE_DIR}/.3rdparty" CACHE PATH "Shared directory for fetched third-party sources")
set(WEBVULKAN_THIRDPARTY_STATE_DIR "${WEBVULKAN_THIRDPARTY_DIR}/.state" CACHE PATH "State directory for fetched third-party metadata")
set(WEBVULKAN_CPM_VERSION "0.40.2" CACHE STRING "CPM.cmake release version")
set(WEBVULKAN_CPM_DIR "${WEBVULKAN_THIRDPARTY_DIR}/cpm" CACHE PATH "Directory where CPM.cmake is stored")
set(WEBVULKAN_CPM_SOURCE_CACHE "${WEBVULKAN_THIRDPARTY_DIR}/cpm-cache" CACHE PATH "Shared CPM source cache")
set(WEBVULKAN_BUILD_DIR "${PROJECT_BINARY_DIR}/_webvulkan" CACHE PATH "Per-build internal artifacts directory")
set(WASM_EMSDK_DIR "${WEBVULKAN_THIRDPARTY_DIR}/emsdk" CACHE PATH "Path where emsdk is cloned")
set(WASM_BUILD_DIR "${WEBVULKAN_BUILD_DIR}/wasm" CACHE PATH "WASM sub-build directory")
set(WEBVULKAN_EMSCRIPTEN_TOOLCHAIN_FILE "${PROJECT_SOURCE_DIR}/cmake/toolchains/WebVulkanEmscripten.cmake" CACHE FILEPATH "Custom Emscripten toolchain file")
set(MESA_SRC_DIR "${WEBVULKAN_THIRDPARTY_DIR}/mesa" CACHE PATH "Path where Mesa is cloned")
set(MESA_GIT_URL "https://github.com/Devsh-Graphics-Programming/mesa.git" CACHE STRING "Mesa git repository URL")
set(MESA_GIT_REF "029fc5ee759c44470a52ecd09f77132581f3a1c7" CACHE STRING "Pinned Mesa git ref")
set(MESA_WASM_BUILD_DIR "${WEBVULKAN_BUILD_DIR}/mesa-wasm" CACHE PATH "Mesa WASM build directory")
set(LLVM_SRC_DIR "${WEBVULKAN_THIRDPARTY_DIR}/llvm-project" CACHE PATH "Path where llvm-project is cloned")
set(LLVM_HOST_BUILD_DIR "${WEBVULKAN_BUILD_DIR}/llvm-host-tools" CACHE PATH "LLVM host tools build directory")
set(LLVM_WASM_BUILD_DIR "${WEBVULKAN_BUILD_DIR}/llvm-wasm" CACHE PATH "LLVM wasm build directory")
set(LLVM_WASM_INSTALL_DIR "${WEBVULKAN_THIRDPARTY_DIR}/llvm-wasm-install" CACHE PATH "LLVM wasm install directory")
set(_WEBVULKAN_DEFAULT_LLVM_PREBUILT_URL "https://github.com/Devsh-Graphics-Programming/llvmpipe2wasm/releases/download/llvm-wasm-prebuilt-latest/llvm-wasm-install.zip")
if(DEFINED ENV{WEBVULKAN_LLVM_PREBUILT_URL} AND NOT "$ENV{WEBVULKAN_LLVM_PREBUILT_URL}" STREQUAL "")
  set(_WEBVULKAN_DEFAULT_LLVM_PREBUILT_URL "$ENV{WEBVULKAN_LLVM_PREBUILT_URL}")
endif()

set(_WEBVULKAN_DEFAULT_LLVM_PREBUILT_SHA256 "")
if(DEFINED ENV{WEBVULKAN_LLVM_PREBUILT_SHA256} AND NOT "$ENV{WEBVULKAN_LLVM_PREBUILT_SHA256}" STREQUAL "")
  set(_WEBVULKAN_DEFAULT_LLVM_PREBUILT_SHA256 "$ENV{WEBVULKAN_LLVM_PREBUILT_SHA256}")
endif()

set(LLVM_PREBUILT_URL "${_WEBVULKAN_DEFAULT_LLVM_PREBUILT_URL}" CACHE STRING "URL to prebuilt LLVM wasm archive for LLVM_PROVIDER=prebuilt")
set(LLVM_PREBUILT_SHA256 "${_WEBVULKAN_DEFAULT_LLVM_PREBUILT_SHA256}" CACHE STRING "SHA256 for LLVM_PREBUILT_URL (optional)")
set(LLVM_PROVIDER "prebuilt" CACHE STRING "LLVM provider: source, prebuilt, or system")
set_property(CACHE LLVM_PROVIDER PROPERTY STRINGS source prebuilt system)
set(LLVM_GIT_REF "afd4df07ab0262482829d4410a6bae9f2809d37b" CACHE STRING "Pinned LLVM git ref")
if(LLVM_GIT_REF STREQUAL "ee9aeb4e05aa8edd2077b79f6712ea353ef5be77")
  set(LLVM_GIT_REF "afd4df07ab0262482829d4410a6bae9f2809d37b" CACHE STRING "Pinned LLVM git ref" FORCE)
endif()
if(NOT LLVM_PROVIDER STREQUAL "source"
   AND NOT LLVM_PROVIDER STREQUAL "prebuilt"
   AND NOT LLVM_PROVIDER STREQUAL "system")
  message(FATAL_ERROR "LLVM_PROVIDER must be one of: source, prebuilt, system")
endif()

set(CPM_SOURCE_CACHE "${WEBVULKAN_CPM_SOURCE_CACHE}" CACHE PATH "CPM source cache path" FORCE)
set(FETCHCONTENT_QUIET OFF)
if(WEBVULKAN_ALLOW_DEP_UPDATES)
  set(_WEBVULKAN_FETCH_UPDATES_DISCONNECTED OFF)
else()
  set(_WEBVULKAN_FETCH_UPDATES_DISCONNECTED ON)
endif()
set(FETCHCONTENT_UPDATES_DISCONNECTED ${_WEBVULKAN_FETCH_UPDATES_DISCONNECTED} CACHE BOOL "FetchContent update disconnected mode" FORCE)
set(CPM_UPDATE_DISCONNECTED ${_WEBVULKAN_FETCH_UPDATES_DISCONNECTED} CACHE BOOL "CPM update disconnected mode" FORCE)
include(cmake/BootstrapCpm.cmake)
webvulkan_bootstrap_cpm()
include(cmake/FetchWebVulkanSources.cmake)

include(cmake/BootstrapEmsdk.cmake)
ensure_emsdk_ready(
  EMCC_OUT EMSDK_EMCC
  TOOLCHAIN_OUT EMSCRIPTEN_TOOLCHAIN
  EMSDK_ROOT_OUT EMSDK_ROOT
)

webvulkan_fetch_git_source(webvulkan_mesa
  REPOSITORY "${MESA_GIT_URL}"
  TAG "${MESA_GIT_REF}"
  SOURCE_DIR "${MESA_SRC_DIR}"
)

if(LLVM_PROVIDER STREQUAL "source")
  webvulkan_fetch_git_source(webvulkan_llvm
    REPOSITORY "https://github.com/llvm/llvm-project.git"
    TAG "${LLVM_GIT_REF}"
    SOURCE_DIR "${LLVM_SRC_DIR}"
  )
endif()

message(STATUS "Using emcc: ${EMSDK_EMCC}")
message(STATUS "Using Emscripten toolchain: ${WEBVULKAN_EMSCRIPTEN_TOOLCHAIN_FILE}")
message(STATUS "Using LLVM provider: ${LLVM_PROVIDER}")

if(NOT CMAKE_INSTALL_LIBDIR)
  set(CMAKE_INSTALL_LIBDIR "lib" CACHE PATH "Object code libraries")
endif()
if(NOT CMAKE_INSTALL_INCLUDEDIR)
  set(CMAKE_INSTALL_INCLUDEDIR "include" CACHE PATH "C header files")
endif()
include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

set(SUBBUILD_GENERATOR "${CMAKE_GENERATOR}")
if(NOT SUBBUILD_GENERATOR)
  message(FATAL_ERROR "No CMake generator selected")
endif()

set(WEBVULKAN_DRIVER_ARCHIVE "${MESA_WASM_BUILD_DIR}/libwebvulkan_driver.full.a")
set(WEBVULKAN_LLVMPIPE_ARCHIVE "${MESA_WASM_BUILD_DIR}/src/gallium/drivers/llvmpipe/libllvmpipe.full.a")
set(WEBVULKAN_LAVAPIPE_ARCHIVE "${MESA_WASM_BUILD_DIR}/src/gallium/frontends/lavapipe/liblavapipe_st.full.a")
add_custom_target(llvmpipe_wasm_spike
  COMMAND
    "${CMAKE_COMMAND}"
    -DEMSDK_ROOT=${EMSDK_ROOT}
    -DMESA_SRC_DIR=${MESA_SRC_DIR}
    -DMESA_GIT_REF=${MESA_GIT_REF}
    -DMESA_BUILD_DIR=${MESA_WASM_BUILD_DIR}
    -DWEBVULKAN_EMSCRIPTEN_TOOLCHAIN_FILE=${WEBVULKAN_EMSCRIPTEN_TOOLCHAIN_FILE}
    -DWEBVULKAN_SUBBUILD_GENERATOR=${SUBBUILD_GENERATOR}
    -DLLVM_SRC_DIR=${LLVM_SRC_DIR}
    -DLLVM_HOST_BUILD_DIR=${LLVM_HOST_BUILD_DIR}
    -DLLVM_WASM_BUILD_DIR=${LLVM_WASM_BUILD_DIR}
    -DLLVM_WASM_INSTALL_DIR=${LLVM_WASM_INSTALL_DIR}
    -DLLVM_PROVIDER=${LLVM_PROVIDER}
    -DLLVM_GIT_REF=${LLVM_GIT_REF}
    -DLLVM_PREBUILT_URL=${LLVM_PREBUILT_URL}
    -DLLVM_PREBUILT_SHA256=${LLVM_PREBUILT_SHA256}
    -P "${PROJECT_SOURCE_DIR}/cmake/BuildMesaLlvmpipeWasm.cmake"
  BYPRODUCTS "${WEBVULKAN_DRIVER_ARCHIVE}" "${WEBVULKAN_LLVMPIPE_ARCHIVE}" "${WEBVULKAN_LAVAPIPE_ARCHIVE}"
  DEPENDS "${PROJECT_SOURCE_DIR}/cmake/BuildMesaLlvmpipeWasm.cmake"
  USES_TERMINAL
  VERBATIM
)
add_custom_target(lavapipe_wasm_spike DEPENDS llvmpipe_wasm_spike)

add_library(webvulkan_llvmpipe_wasm INTERFACE)
add_library(webvulkan::llvmpipe_wasm ALIAS webvulkan_llvmpipe_wasm)
add_library(webvulkan::lavapipe_wasm ALIAS webvulkan_llvmpipe_wasm)

add_dependencies(webvulkan_llvmpipe_wasm llvmpipe_wasm_spike)

target_include_directories(webvulkan_llvmpipe_wasm INTERFACE
  "$<BUILD_INTERFACE:${MESA_SRC_DIR}/include>"
  "$<BUILD_INTERFACE:${MESA_SRC_DIR}/src>"
  "$<BUILD_INTERFACE:${LLVM_WASM_INSTALL_DIR}/include>"
  "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/mesa>"
  "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/mesa/src>"
  "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/llvm>"
)

target_link_libraries(webvulkan_llvmpipe_wasm INTERFACE
  "$<BUILD_INTERFACE:${WEBVULKAN_DRIVER_ARCHIVE}>"
)

set(WEBVULKAN_PACKAGE_NAME "WebVulkanLlvmpipeWasm")
set(WEBVULKAN_CONFIG_DIR "${CMAKE_INSTALL_LIBDIR}/cmake/${WEBVULKAN_PACKAGE_NAME}")
set(WEBVULKAN_CONFIG_FILE "${PROJECT_BINARY_DIR}/${WEBVULKAN_PACKAGE_NAME}Config.cmake")
set(WEBVULKAN_VERSION_FILE "${PROJECT_BINARY_DIR}/${WEBVULKAN_PACKAGE_NAME}ConfigVersion.cmake")

configure_package_config_file(
  "${PROJECT_SOURCE_DIR}/cmake/${WEBVULKAN_PACKAGE_NAME}Config.cmake.in"
  "${WEBVULKAN_CONFIG_FILE}"
  INSTALL_DESTINATION "${WEBVULKAN_CONFIG_DIR}"
  PATH_VARS CMAKE_INSTALL_LIBDIR CMAKE_INSTALL_INCLUDEDIR
  NO_SET_AND_CHECK_MACRO
  NO_CHECK_REQUIRED_COMPONENTS_MACRO
)

write_basic_package_version_file(
  "${WEBVULKAN_VERSION_FILE}"
  VERSION "0.1.0"
  COMPATIBILITY SameMajorVersion
)

install(CODE
  "if(NOT EXISTS \"${WEBVULKAN_LLVMPIPE_ARCHIVE}\" OR NOT EXISTS \"${WEBVULKAN_LAVAPIPE_ARCHIVE}\" OR NOT EXISTS \"${WEBVULKAN_DRIVER_ARCHIVE}\")\n  message(FATAL_ERROR \"Build artifacts are missing. Run: cmake --build <build-dir> --target llvmpipe_wasm_spike\")\nendif()"
)

install(FILES "${WEBVULKAN_LLVMPIPE_ARCHIVE}" DESTINATION "${CMAKE_INSTALL_LIBDIR}" RENAME "libllvmpipe.a")
install(FILES "${WEBVULKAN_LAVAPIPE_ARCHIVE}" DESTINATION "${CMAKE_INSTALL_LIBDIR}" RENAME "liblavapipe_st.a")
install(FILES "${WEBVULKAN_DRIVER_ARCHIVE}" DESTINATION "${CMAKE_INSTALL_LIBDIR}" RENAME "libwebvulkan_driver.a")
install(DIRECTORY "${MESA_SRC_DIR}/include/" DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/mesa")
install(DIRECTORY "${MESA_SRC_DIR}/src/" DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/mesa/src")
install(DIRECTORY "${LLVM_WASM_INSTALL_DIR}/include/" DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/llvm")
install(FILES "${WEBVULKAN_CONFIG_FILE}" "${WEBVULKAN_VERSION_FILE}" DESTINATION "${WEBVULKAN_CONFIG_DIR}")

if(WEBVULKAN_BUILD_TESTS)
  add_subdirectory(tests)
endif()
