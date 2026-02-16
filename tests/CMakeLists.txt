cmake_minimum_required(VERSION 4.2)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
  project(webvulkan_tests LANGUAGES NONE)
endif()

if(NOT DEFINED WASM_AUTO_FETCH_TOOLS)
  set(WASM_AUTO_FETCH_TOOLS ON CACHE BOOL "Auto-download missing tools during configure")
endif()

set(WEBVULKAN_TEST_THIRDPARTY_DIR "${CMAKE_CURRENT_LIST_DIR}/../.3rdparty/tests" CACHE PATH "Test-only third-party root directory")
set(WEBVULKAN_THIRDPARTY_STATE_DIR "${CMAKE_CURRENT_LIST_DIR}/../.3rdparty/.state" CACHE PATH "State directory for fetched third-party metadata")
set(WEBVULKAN_VOLK_GIT_URL "https://github.com/zeux/volk.git" CACHE STRING "volk git repository URL for smoke tests")
set(WEBVULKAN_VOLK_GIT_REF "1.4.304" CACHE STRING "Pinned volk git ref for smoke tests")
set(WEBVULKAN_TEST_VOLK_SOURCE_DIR "${WEBVULKAN_TEST_THIRDPARTY_DIR}/volk" CACHE PATH "Path where volk is cloned for smoke tests")

set(WEBVULKAN_TEST_MODE "auto" CACHE STRING "Test mode: auto, in-tree, or package")
set_property(CACHE WEBVULKAN_TEST_MODE PROPERTY STRINGS auto in-tree package)
set(WEBVULKAN_TEST_PACKAGE_PREFIX "" CACHE PATH "Install prefix for WebVulkan package mode")
set(WEBVULKAN_TEST_WASM_BUILD_DIR "${CMAKE_BINARY_DIR}/wasm" CACHE PATH "WASM smoke build directory")
set(WEBVULKAN_TEST_NODE_BIN "" CACHE FILEPATH "Path to node executable used by runtime smoke scripts")
set(WEBVULKAN_WASMER_BIN "" CACHE FILEPATH "Path to wasmer executable used by clang wasm runtime smoke")

if(NOT COMMAND CPMAddPackage)
  if(NOT DEFINED WEBVULKAN_CPM_VERSION OR WEBVULKAN_CPM_VERSION STREQUAL "")
    set(WEBVULKAN_CPM_VERSION "0.40.2" CACHE STRING "CPM.cmake release version for tests")
  endif()
  if(NOT DEFINED WEBVULKAN_CPM_DIR OR WEBVULKAN_CPM_DIR STREQUAL "")
    set(WEBVULKAN_CPM_DIR "${WEBVULKAN_TEST_THIRDPARTY_DIR}/cpm" CACHE PATH "Directory where CPM.cmake is stored for tests")
  endif()
  if(NOT DEFINED CPM_SOURCE_CACHE OR CPM_SOURCE_CACHE STREQUAL "")
    set(CPM_SOURCE_CACHE "${WEBVULKAN_TEST_THIRDPARTY_DIR}/cpm-cache" CACHE PATH "CPM source cache path for tests")
  endif()
  if(NOT DEFINED WEBVULKAN_ALLOW_DEP_UPDATES)
    set(WEBVULKAN_ALLOW_DEP_UPDATES OFF CACHE BOOL "Allow dependency git update during configure")
  endif()
  set(_webvulkan_allow_dep_updates "${WEBVULKAN_ALLOW_DEP_UPDATES}")
  if(NOT DEFINED WEBVULKAN_DEPS_EDITABLE)
    set(WEBVULKAN_DEPS_EDITABLE OFF CACHE BOOL "Use editable local dependency checkouts without automatic git reset/update")
  endif()
  if(WEBVULKAN_DEPS_EDITABLE)
    set(CPM_UPDATE_DISCONNECTED ON CACHE BOOL "CPM update disconnected mode" FORCE)
  elseif(_webvulkan_allow_dep_updates)
    set(CPM_UPDATE_DISCONNECTED OFF CACHE BOOL "CPM update disconnected mode" FORCE)
  else()
    set(CPM_UPDATE_DISCONNECTED ON CACHE BOOL "CPM update disconnected mode" FORCE)
  endif()
  set(_webvulkan_root_bootstrap_cpm "${CMAKE_CURRENT_LIST_DIR}/../cmake/BootstrapCpm.cmake")
  if(EXISTS "${_webvulkan_root_bootstrap_cpm}")
    include("${_webvulkan_root_bootstrap_cpm}")
    webvulkan_bootstrap_cpm()
  else()
    file(MAKE_DIRECTORY "${WEBVULKAN_CPM_DIR}")
    set(_webvulkan_tests_cpm_file "${WEBVULKAN_CPM_DIR}/CPM_${WEBVULKAN_CPM_VERSION}.cmake")
    set(_webvulkan_tests_cpm_url "https://github.com/cpm-cmake/CPM.cmake/releases/download/v${WEBVULKAN_CPM_VERSION}/CPM.cmake")
    if(NOT EXISTS "${_webvulkan_tests_cpm_file}")
      message(STATUS "Downloading CPM.cmake v${WEBVULKAN_CPM_VERSION} for tests")
      file(DOWNLOAD "${_webvulkan_tests_cpm_url}" "${_webvulkan_tests_cpm_file}" STATUS _webvulkan_tests_cpm_download_status TLS_VERIFY ON)
      list(GET _webvulkan_tests_cpm_download_status 0 _webvulkan_tests_cpm_download_code)
      if(NOT _webvulkan_tests_cpm_download_code EQUAL 0)
        list(GET _webvulkan_tests_cpm_download_status 1 _webvulkan_tests_cpm_download_message)
        file(REMOVE "${_webvulkan_tests_cpm_file}")
        message(FATAL_ERROR "Failed to download CPM.cmake from ${_webvulkan_tests_cpm_url}: ${_webvulkan_tests_cpm_download_message}")
      endif()
    endif()
    include("${_webvulkan_tests_cpm_file}")
  endif()
endif()

set(_webvulkan_test_mode "${WEBVULKAN_TEST_MODE}")
if(_webvulkan_test_mode STREQUAL "auto")
  if(TARGET llvmpipe_wasm_spike)
    set(_webvulkan_test_mode "in-tree")
  else()
    set(_webvulkan_test_mode "package")
  endif()
endif()

if(NOT DEFINED EMSDK_ROOT OR EMSDK_ROOT STREQUAL "")
  if(DEFINED ENV{EMSDK} AND NOT "$ENV{EMSDK}" STREQUAL "")
    set(EMSDK_ROOT "$ENV{EMSDK}")
  endif()
endif()

if(NOT DEFINED WEBVULKAN_EMSCRIPTEN_TOOLCHAIN_FILE OR WEBVULKAN_EMSCRIPTEN_TOOLCHAIN_FILE STREQUAL "")
  if(DEFINED EMSDK_ROOT AND NOT EMSDK_ROOT STREQUAL "")
    set(WEBVULKAN_EMSCRIPTEN_TOOLCHAIN_FILE
      "${EMSDK_ROOT}/upstream/emscripten/cmake/Modules/Platform/Emscripten.cmake")
  endif()
endif()

if(NOT DEFINED SUBBUILD_GENERATOR OR SUBBUILD_GENERATOR STREQUAL "")
  set(SUBBUILD_GENERATOR "${CMAKE_GENERATOR}")
endif()

if(NOT DEFINED WASM_BUILD_DIR OR WASM_BUILD_DIR STREQUAL "")
  set(WASM_BUILD_DIR "${WEBVULKAN_TEST_WASM_BUILD_DIR}")
endif()

if(NOT DEFINED EMSDK_ROOT OR EMSDK_ROOT STREQUAL "")
  message(FATAL_ERROR "EMSDK_ROOT is required for runtime smoke tests")
endif()

if(NOT EXISTS "${WEBVULKAN_EMSCRIPTEN_TOOLCHAIN_FILE}")
  message(FATAL_ERROR "Emscripten toolchain file was not found at ${WEBVULKAN_EMSCRIPTEN_TOOLCHAIN_FILE}")
endif()

if(NOT SUBBUILD_GENERATOR)
  message(FATAL_ERROR "No CMake generator is available for wasm smoke sub-build")
endif()

set(_webvulkan_node_hints)
file(GLOB _webvulkan_node_candidates
  "${EMSDK_ROOT}/node/*/bin/node"
  "${EMSDK_ROOT}/node/*/bin/node.exe"
)
if(_webvulkan_node_candidates)
  list(SORT _webvulkan_node_candidates COMPARE NATURAL ORDER DESCENDING)
  list(GET _webvulkan_node_candidates 0 _webvulkan_emsdk_node_exe)
  get_filename_component(_webvulkan_emsdk_node_dir "${_webvulkan_emsdk_node_exe}" DIRECTORY)
  list(APPEND _webvulkan_node_hints "${_webvulkan_emsdk_node_dir}")
endif()
if(CMAKE_HOST_WIN32)
  if(DEFINED ENV{ProgramFiles} AND NOT "$ENV{ProgramFiles}" STREQUAL "")
    list(APPEND _webvulkan_node_hints "$ENV{ProgramFiles}/nodejs")
  endif()
  if(DEFINED ENV{ProgramW6432} AND NOT "$ENV{ProgramW6432}" STREQUAL "")
    list(APPEND _webvulkan_node_hints "$ENV{ProgramW6432}/nodejs")
  endif()
  if(DEFINED ENV{LOCALAPPDATA} AND NOT "$ENV{LOCALAPPDATA}" STREQUAL "")
    list(APPEND _webvulkan_node_hints "$ENV{LOCALAPPDATA}/Programs/nodejs")
  endif()
endif()

if(NOT WEBVULKAN_TEST_NODE_BIN OR WEBVULKAN_TEST_NODE_BIN MATCHES "-NOTFOUND$")
  unset(WEBVULKAN_TEST_NODE_BIN CACHE)
  find_program(WEBVULKAN_TEST_NODE_BIN NAMES node node.exe nodejs nodejs.exe HINTS ${_webvulkan_node_hints})
endif()
if(NOT WEBVULKAN_TEST_NODE_BIN OR NOT EXISTS "${WEBVULKAN_TEST_NODE_BIN}")
  message(FATAL_ERROR "node executable is required for runtime smoke scripts")
endif()
set(WEBVULKAN_TEST_NODE_BIN "${WEBVULKAN_TEST_NODE_BIN}" CACHE FILEPATH "Path to node executable used by runtime smoke scripts" FORCE)

set(_webvulkan_home_dir "")
if(WIN32 AND DEFINED ENV{USERPROFILE} AND NOT "$ENV{USERPROFILE}" STREQUAL "")
  set(_webvulkan_home_dir "$ENV{USERPROFILE}")
elseif(DEFINED ENV{HOME} AND NOT "$ENV{HOME}" STREQUAL "")
  set(_webvulkan_home_dir "$ENV{HOME}")
endif()

set(_webvulkan_wasmer_hints)
if(_webvulkan_home_dir)
  list(APPEND _webvulkan_wasmer_hints "${_webvulkan_home_dir}/.wasmer/bin")
endif()
if(WIN32)
  list(APPEND _webvulkan_wasmer_hints
    "C:/Program Files/Wasmer/bin"
    "C:/Program Files (x86)/Wasmer/bin"
  )
endif()

if(NOT WEBVULKAN_WASMER_BIN OR WEBVULKAN_WASMER_BIN MATCHES "-NOTFOUND$")
  unset(WEBVULKAN_WASMER_BIN CACHE)
  find_program(WEBVULKAN_WASMER_BIN NAMES wasmer wasmer.exe HINTS ${_webvulkan_wasmer_hints})
endif()
if((NOT WEBVULKAN_WASMER_BIN OR NOT EXISTS "${WEBVULKAN_WASMER_BIN}") AND _webvulkan_home_dir)
  if(CMAKE_HOST_WIN32)
    set(_webvulkan_wasmer_default "${_webvulkan_home_dir}/.wasmer/bin/wasmer.exe")
  else()
    set(_webvulkan_wasmer_default "${_webvulkan_home_dir}/.wasmer/bin/wasmer")
  endif()
  if(EXISTS "${_webvulkan_wasmer_default}")
    set(WEBVULKAN_WASMER_BIN "${_webvulkan_wasmer_default}")
  endif()
endif()

if((NOT WEBVULKAN_WASMER_BIN OR NOT EXISTS "${WEBVULKAN_WASMER_BIN}") AND WASM_AUTO_FETCH_TOOLS)
  execute_process(
    COMMAND "${CMAKE_COMMAND}" -P "${CMAKE_CURRENT_LIST_DIR}/BootstrapWasmer.cmake"
    RESULT_VARIABLE _webvulkan_bootstrap_wasmer_result
  )
  if(NOT _webvulkan_bootstrap_wasmer_result EQUAL 0)
    message(FATAL_ERROR "Failed to bootstrap wasmer runtime")
  endif()
  find_program(WEBVULKAN_WASMER_BIN NAMES wasmer wasmer.exe HINTS ${_webvulkan_wasmer_hints})
endif()

if(NOT WEBVULKAN_WASMER_BIN OR NOT EXISTS "${WEBVULKAN_WASMER_BIN}")
  message(FATAL_ERROR "wasmer executable is required for clang wasm runtime smoke")
endif()
set(WEBVULKAN_WASMER_BIN "${WEBVULKAN_WASMER_BIN}" CACHE FILEPATH "Path to wasmer executable used by clang wasm runtime smoke" FORCE)

if(NOT COMMAND webvulkan_fetch_git_source)
  set(_webvulkan_root_fetch_sources "${CMAKE_CURRENT_LIST_DIR}/../cmake/FetchWebVulkanSources.cmake")
  if(EXISTS "${_webvulkan_root_fetch_sources}")
    include("${_webvulkan_root_fetch_sources}")
  endif()
endif()

if(COMMAND webvulkan_fetch_git_source)
  webvulkan_fetch_git_source(webvulkan_volk
    REPOSITORY "${WEBVULKAN_VOLK_GIT_URL}"
    TAG "${WEBVULKAN_VOLK_GIT_REF}"
    SOURCE_DIR "${WEBVULKAN_TEST_VOLK_SOURCE_DIR}"
  )
  set(webvulkan_volk_SOURCE_DIR "${WEBVULKAN_TEST_VOLK_SOURCE_DIR}")
else()
  CPMAddPackage(
    NAME webvulkan_volk
    GIT_REPOSITORY "${WEBVULKAN_VOLK_GIT_URL}"
    GIT_TAG "${WEBVULKAN_VOLK_GIT_REF}"
    GIT_SHALLOW TRUE
    GIT_REMOTE_UPDATE_STRATEGY CHECKOUT
    SOURCE_DIR "${WEBVULKAN_TEST_VOLK_SOURCE_DIR}"
    DOWNLOAD_ONLY YES
  )
endif()
set(_webvulkan_volk_include_dir "${webvulkan_volk_SOURCE_DIR}")
set(_webvulkan_volk_source "${webvulkan_volk_SOURCE_DIR}/volk.c")
set(_webvulkan_volk_header "${webvulkan_volk_SOURCE_DIR}/volk.h")
if(NOT EXISTS "${_webvulkan_volk_source}" OR NOT EXISTS "${_webvulkan_volk_header}")
  message(FATAL_ERROR "volk sources are missing under ${webvulkan_volk_SOURCE_DIR}")
endif()

if(_webvulkan_test_mode STREQUAL "in-tree")
  if(NOT TARGET llvmpipe_wasm_spike)
    message(FATAL_ERROR "in-tree test mode requires llvmpipe_wasm_spike target")
  endif()
  if(NOT DEFINED WEBVULKAN_DRIVER_ARCHIVE OR WEBVULKAN_DRIVER_ARCHIVE STREQUAL "")
    message(FATAL_ERROR "in-tree test mode requires WEBVULKAN_DRIVER_ARCHIVE")
  endif()
  if(NOT DEFINED MESA_SRC_DIR OR MESA_SRC_DIR STREQUAL "")
    message(FATAL_ERROR "in-tree test mode requires MESA_SRC_DIR")
  endif()
  set(_webvulkan_driver_archive "${WEBVULKAN_DRIVER_ARCHIVE}")
  set(_webvulkan_smoke_include_dirs
    "${MESA_SRC_DIR}/include"
    "${MESA_SRC_DIR}/src"
  )
elseif(_webvulkan_test_mode STREQUAL "package")
  if(NOT WEBVULKAN_TEST_PACKAGE_PREFIX)
    message(FATAL_ERROR "package test mode requires WEBVULKAN_TEST_PACKAGE_PREFIX")
  endif()
  set(_webvulkan_driver_archive "${WEBVULKAN_TEST_PACKAGE_PREFIX}/lib/libwebvulkan_driver.a")
  set(_webvulkan_smoke_include_dirs
    "${WEBVULKAN_TEST_PACKAGE_PREFIX}/include/mesa"
    "${WEBVULKAN_TEST_PACKAGE_PREFIX}/include/mesa/src"
    "${WEBVULKAN_TEST_PACKAGE_PREFIX}/include/llvm"
  )
else()
  message(FATAL_ERROR "Unsupported WEBVULKAN_TEST_MODE='${WEBVULKAN_TEST_MODE}'")
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
set(_webvulkan_lavapipe_depends
  "${CMAKE_CURRENT_LIST_DIR}/RunLavapipeRuntimeSmoke.cmake"
  "${CMAKE_CURRENT_LIST_DIR}/wasm/src/lavapipe_runtime_smoke.c"
  "${CMAKE_CURRENT_LIST_DIR}/wasm/tools/smoke_runtime.mjs"
  "${_webvulkan_volk_source}"
  "${_webvulkan_volk_header}"
  "${_webvulkan_driver_archive}"
)
if(_webvulkan_test_mode STREQUAL "in-tree")
  list(APPEND _webvulkan_lavapipe_depends llvmpipe_wasm_spike)
endif()
list(JOIN _webvulkan_smoke_include_dirs "|" _webvulkan_smoke_include_dirs_serialized)

add_custom_command(
  OUTPUT "${WEBVULKAN_LAVAPIPE_SMOKE_OK}"
  COMMAND
    "${CMAKE_COMMAND}"
    -DEMSDK_ROOT=${EMSDK_ROOT}
    -DDRIVER_ARCHIVE=${_webvulkan_driver_archive}
    -DSMOKE_INCLUDE_DIRS_SERIALIZED=${_webvulkan_smoke_include_dirs_serialized}
    -DSMOKE_SOURCE=${CMAKE_CURRENT_LIST_DIR}/wasm/src/lavapipe_runtime_smoke.c
    -DSMOKE_JS_OUT=${WEBVULKAN_LAVAPIPE_SMOKE_JS}
    -DSMOKE_EXPORT=_lavapipe_runtime_smoke
    -DSMOKE_SCRIPT=${CMAKE_CURRENT_LIST_DIR}/wasm/tools/smoke_runtime.mjs
    -DVOLK_INCLUDE_DIR=${_webvulkan_volk_include_dir}
    -DVOLK_SOURCE=${_webvulkan_volk_source}
    -P "${CMAKE_CURRENT_LIST_DIR}/RunLavapipeRuntimeSmoke.cmake"
  COMMAND "${CMAKE_COMMAND}" -E touch "${WEBVULKAN_LAVAPIPE_SMOKE_OK}"
  DEPENDS ${_webvulkan_lavapipe_depends}
  USES_TERMINAL
  VERBATIM
)
add_custom_target(lavapipe_runtime_smoke DEPENDS "${WEBVULKAN_LAVAPIPE_SMOKE_OK}")

set(WEBVULKAN_POC_NIR_TO_WASM_SMOKE_OK "${CMAKE_BINARY_DIR}/poc_nir_to_wasm_smoke.ok")
set(WEBVULKAN_POC_NIR_TO_WASM_SMOKE_JS "${CMAKE_BINARY_DIR}/lavapipe-smoke/poc_nir_to_wasm_smoke.js")
set(_webvulkan_poc_nir_to_wasm_depends
  "${CMAKE_CURRENT_LIST_DIR}/RunLavapipeRuntimeSmoke.cmake"
  "${CMAKE_CURRENT_LIST_DIR}/wasm/src/poc_nir_to_wasm_smoke.c"
  "${CMAKE_CURRENT_LIST_DIR}/wasm/tools/smoke_runtime.mjs"
  "${_webvulkan_volk_source}"
  "${_webvulkan_volk_header}"
  "${_webvulkan_driver_archive}"
)
if(_webvulkan_test_mode STREQUAL "in-tree")
  list(APPEND _webvulkan_poc_nir_to_wasm_depends llvmpipe_wasm_spike)
endif()
add_custom_command(
  OUTPUT "${WEBVULKAN_POC_NIR_TO_WASM_SMOKE_OK}"
  COMMAND
    "${CMAKE_COMMAND}"
    -DEMSDK_ROOT=${EMSDK_ROOT}
    -DDRIVER_ARCHIVE=${_webvulkan_driver_archive}
    -DSMOKE_INCLUDE_DIRS_SERIALIZED=${_webvulkan_smoke_include_dirs_serialized}
    -DSMOKE_SOURCE=${CMAKE_CURRENT_LIST_DIR}/wasm/src/poc_nir_to_wasm_smoke.c
    -DSMOKE_JS_OUT=${WEBVULKAN_POC_NIR_TO_WASM_SMOKE_JS}
    -DSMOKE_EXPORT=_poc_nir_to_wasm_smoke
    -DSMOKE_SCRIPT=${CMAKE_CURRENT_LIST_DIR}/wasm/tools/smoke_runtime.mjs
    -DVOLK_INCLUDE_DIR=${_webvulkan_volk_include_dir}
    -DVOLK_SOURCE=${_webvulkan_volk_source}
    -P "${CMAKE_CURRENT_LIST_DIR}/RunLavapipeRuntimeSmoke.cmake"
  COMMAND "${CMAKE_COMMAND}" -E touch "${WEBVULKAN_POC_NIR_TO_WASM_SMOKE_OK}"
  DEPENDS ${_webvulkan_poc_nir_to_wasm_depends}
  USES_TERMINAL
  VERBATIM
)
add_custom_target(smoke_poc_nir_to_wasm DEPENDS "${WEBVULKAN_POC_NIR_TO_WASM_SMOKE_OK}")

set(WEBVULKAN_CLANG_WASM_SMOKE_OK "${CMAKE_BINARY_DIR}/clang_wasm_runtime_smoke.ok")
add_custom_command(
  OUTPUT "${WEBVULKAN_CLANG_WASM_SMOKE_OK}"
  COMMAND
    "${CMAKE_COMMAND}" -E env
    "WEBVULKAN_WASMER_BIN=${WEBVULKAN_WASMER_BIN}"
    "${WEBVULKAN_TEST_NODE_BIN}" "${CMAKE_CURRENT_LIST_DIR}/wasm/tools/clang_wasm_runtime_smoke.mjs"
  COMMAND "${CMAKE_COMMAND}" -E touch "${WEBVULKAN_CLANG_WASM_SMOKE_OK}"
  DEPENDS "${CMAKE_CURRENT_LIST_DIR}/wasm/tools/clang_wasm_runtime_smoke.mjs"
  USES_TERMINAL
  VERBATIM
)
add_custom_target(clang_wasm_runtime_smoke DEPENDS "${WEBVULKAN_CLANG_WASM_SMOKE_OK}")

add_custom_target(runtime_smoke)
add_dependencies(runtime_smoke wasm_runtime_smoke lavapipe_runtime_smoke clang_wasm_runtime_smoke)
