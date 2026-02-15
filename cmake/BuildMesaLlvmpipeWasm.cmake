cmake_minimum_required(VERSION 4.2)

function(run_checked)
  set(options)
  set(oneValueArgs LABEL WORKING_DIRECTORY)
  set(multiValueArgs COMMAND)
  cmake_parse_arguments(RUN "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

  if(NOT RUN_LABEL)
    message(FATAL_ERROR "run_checked requires LABEL")
  endif()
  if(NOT RUN_COMMAND)
    message(FATAL_ERROR "run_checked requires COMMAND")
  endif()

  if(RUN_WORKING_DIRECTORY)
    execute_process(
      COMMAND ${RUN_COMMAND}
      WORKING_DIRECTORY "${RUN_WORKING_DIRECTORY}"
      COMMAND_ECHO STDOUT
      RESULT_VARIABLE RUN_RESULT
    )
  else()
    execute_process(
      COMMAND ${RUN_COMMAND}
      COMMAND_ECHO STDOUT
      RESULT_VARIABLE RUN_RESULT
    )
  endif()

  if(NOT RUN_RESULT EQUAL 0)
    message(FATAL_ERROR "${RUN_LABEL} failed")
  endif()
endfunction()

function(ensure_python_module PYTHON_EXE MODULE_NAME PACKAGE_NAME)
  execute_process(
    COMMAND "${PYTHON_EXE}" -c "import ${MODULE_NAME}"
    RESULT_VARIABLE MODULE_CHECK_RESULT
  )
  if(NOT MODULE_CHECK_RESULT EQUAL 0)
    run_checked(
      LABEL "Install python module ${PACKAGE_NAME}"
      COMMAND "${PYTHON_EXE}" -m pip install --user "${PACKAGE_NAME}"
    )
  endif()
endfunction()

function(make_full_archive INPUT_ARCHIVE OUTPUT_ARCHIVE AR_TOOL)
  if(NOT EXISTS "${INPUT_ARCHIVE}")
    message(FATAL_ERROR "Input archive ${INPUT_ARCHIVE} is missing")
  endif()

  get_filename_component(OUTPUT_DIR "${OUTPUT_ARCHIVE}" DIRECTORY)
  file(MAKE_DIRECTORY "${OUTPUT_DIR}")

  set(MRI_FILE "${OUTPUT_ARCHIVE}.mri")
  file(WRITE "${MRI_FILE}" "CREATE ${OUTPUT_ARCHIVE}\n")
  file(APPEND "${MRI_FILE}" "ADDLIB ${INPUT_ARCHIVE}\n")
  file(APPEND "${MRI_FILE}" "SAVE\n")
  file(APPEND "${MRI_FILE}" "END\n")

  execute_process(
    COMMAND "${AR_TOOL}" -M
    INPUT_FILE "${MRI_FILE}"
    RESULT_VARIABLE ARCHIVE_RESULT
  )
  file(REMOVE "${MRI_FILE}")
  if(NOT ARCHIVE_RESULT EQUAL 0)
    message(FATAL_ERROR "Failed to create full archive ${OUTPUT_ARCHIVE}")
  endif()
endfunction()

function(make_archive_bundle OUTPUT_ARCHIVE AR_TOOL)
  set(INPUT_ARCHIVES ${ARGN})
  if(NOT INPUT_ARCHIVES)
    message(FATAL_ERROR "make_archive_bundle requires at least one input archive")
  endif()

  get_filename_component(OUTPUT_DIR "${OUTPUT_ARCHIVE}" DIRECTORY)
  file(MAKE_DIRECTORY "${OUTPUT_DIR}")

  set(MRI_FILE "${OUTPUT_ARCHIVE}.mri")
  file(WRITE "${MRI_FILE}" "CREATE ${OUTPUT_ARCHIVE}\n")
  foreach(INPUT_ARCHIVE IN LISTS INPUT_ARCHIVES)
    if(EXISTS "${INPUT_ARCHIVE}")
      file(APPEND "${MRI_FILE}" "ADDLIB ${INPUT_ARCHIVE}\n")
    endif()
  endforeach()
  file(APPEND "${MRI_FILE}" "SAVE\n")
  file(APPEND "${MRI_FILE}" "END\n")

  execute_process(
    COMMAND "${AR_TOOL}" -M
    INPUT_FILE "${MRI_FILE}"
    RESULT_VARIABLE ARCHIVE_RESULT
  )
  file(REMOVE "${MRI_FILE}")
  if(NOT ARCHIVE_RESULT EQUAL 0)
    message(FATAL_ERROR "Failed to create archive bundle ${OUTPUT_ARCHIVE}")
  endif()
endfunction()

if(NOT DEFINED EMSDK_ROOT OR NOT EXISTS "${EMSDK_ROOT}")
  message(FATAL_ERROR "EMSDK_ROOT is required and must exist")
endif()
if(NOT DEFINED MESA_SRC_DIR)
  message(FATAL_ERROR "MESA_SRC_DIR is required")
endif()
if(NOT DEFINED MESA_BUILD_DIR)
  message(FATAL_ERROR "MESA_BUILD_DIR is required")
endif()
get_filename_component(PROJECT_ROOT "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
if(NOT DEFINED WEBVULKAN_EMSCRIPTEN_TOOLCHAIN_FILE)
  set(WEBVULKAN_EMSCRIPTEN_TOOLCHAIN_FILE "${PROJECT_ROOT}/cmake/toolchains/WebVulkanEmscripten.cmake")
endif()
if(NOT DEFINED LLVM_PROVIDER)
  set(LLVM_PROVIDER "source")
endif()
if(NOT DEFINED LLVM_GIT_REF)
  set(LLVM_GIT_REF "afd4df07ab0262482829d4410a6bae9f2809d37b")
endif()
if(NOT DEFINED LLVM_PREBUILT_URL)
  set(LLVM_PREBUILT_URL "")
endif()
if(NOT DEFINED LLVM_PREBUILT_SHA256)
  set(LLVM_PREBUILT_SHA256 "")
endif()
if(NOT DEFINED MESA_GIT_REF)
  set(MESA_GIT_REF "unknown")
endif()

if(NOT DEFINED LLVM_SRC_DIR)
  set(LLVM_SRC_DIR "${PROJECT_ROOT}/.3rdparty/llvm-project")
endif()
if(NOT DEFINED LLVM_HOST_BUILD_DIR)
  set(LLVM_HOST_BUILD_DIR "${PROJECT_ROOT}/build/llvm-host-tools")
endif()
if(NOT DEFINED LLVM_WASM_BUILD_DIR)
  set(LLVM_WASM_BUILD_DIR "${PROJECT_ROOT}/build/llvm-wasm")
endif()
if(NOT DEFINED LLVM_WASM_INSTALL_DIR)
  set(LLVM_WASM_INSTALL_DIR "${PROJECT_ROOT}/.3rdparty/llvm-wasm-install")
endif()

find_program(CMAKE_EXECUTABLE NAMES cmake REQUIRED)

if(NOT DEFINED WEBVULKAN_SUBBUILD_GENERATOR OR WEBVULKAN_SUBBUILD_GENERATOR STREQUAL "")
  message(FATAL_ERROR "WEBVULKAN_SUBBUILD_GENERATOR is required")
endif()

set(SUBBUILD_GENERATOR_ARGS -G "${WEBVULKAN_SUBBUILD_GENERATOR}")

if(WIN32)
  set(EMCONFIGURE "${EMSDK_ROOT}/upstream/emscripten/emconfigure.bat")
  set(EMMAKE "${EMSDK_ROOT}/upstream/emscripten/emmake.bat")
  set(EMSCRIPTEN_TOOLCHAIN "${EMSDK_ROOT}/upstream/emscripten/cmake/Modules/Platform/Emscripten.cmake")
  set(EMCC_BIN "${EMSDK_ROOT}/upstream/emscripten/emcc.bat")
  set(EMXX_BIN "${EMSDK_ROOT}/upstream/emscripten/em++.bat")
  set(EMAR_BIN "${EMSDK_ROOT}/upstream/emscripten/emar.bat")
  set(EMRANLIB_BIN "${EMSDK_ROOT}/upstream/emscripten/emranlib.bat")
  set(EMSTRIP_BIN "${EMSDK_ROOT}/upstream/emscripten/emstrip.bat")
  file(GLOB EMSDK_NODE_CANDIDATES "${EMSDK_ROOT}/node/*/bin/node.exe")
  file(GLOB EMSDK_PYTHON_CANDIDATES "${EMSDK_ROOT}/python/*/python.exe")
  set(LLVM_TBLGEN_NAME llvm-tblgen.exe)
else()
  set(EMCONFIGURE "${EMSDK_ROOT}/upstream/emscripten/emconfigure")
  set(EMMAKE "${EMSDK_ROOT}/upstream/emscripten/emmake")
  set(EMSCRIPTEN_TOOLCHAIN "${EMSDK_ROOT}/upstream/emscripten/cmake/Modules/Platform/Emscripten.cmake")
  set(EMCC_BIN "${EMSDK_ROOT}/upstream/emscripten/emcc")
  set(EMXX_BIN "${EMSDK_ROOT}/upstream/emscripten/em++")
  set(EMAR_BIN "${EMSDK_ROOT}/upstream/emscripten/emar")
  set(EMRANLIB_BIN "${EMSDK_ROOT}/upstream/emscripten/emranlib")
  set(EMSTRIP_BIN "${EMSDK_ROOT}/upstream/emscripten/emstrip")
  file(GLOB EMSDK_NODE_CANDIDATES "${EMSDK_ROOT}/node/*/bin/node")
  file(GLOB EMSDK_PYTHON_CANDIDATES "${EMSDK_ROOT}/python/*/python")
  set(LLVM_TBLGEN_NAME llvm-tblgen)
endif()

if(NOT EXISTS "${EMCONFIGURE}")
  message(FATAL_ERROR "emconfigure not found at ${EMCONFIGURE}")
endif()
if(NOT EXISTS "${EMMAKE}")
  message(FATAL_ERROR "emmake not found at ${EMMAKE}")
endif()
if(NOT EXISTS "${WEBVULKAN_EMSCRIPTEN_TOOLCHAIN_FILE}")
  message(FATAL_ERROR "WebVulkan Emscripten toolchain file not found at ${WEBVULKAN_EMSCRIPTEN_TOOLCHAIN_FILE}")
endif()
if(NOT LLVM_PROVIDER STREQUAL "source" AND NOT LLVM_PROVIDER STREQUAL "prebuilt" AND NOT LLVM_PROVIDER STREQUAL "system")
  message(FATAL_ERROR "LLVM_PROVIDER must be one of: source, prebuilt, system")
endif()

if(NOT EMSDK_NODE_CANDIDATES)
  message(FATAL_ERROR "node from emsdk was not found")
endif()
list(SORT EMSDK_NODE_CANDIDATES COMPARE NATURAL ORDER DESCENDING)
list(GET EMSDK_NODE_CANDIDATES 0 EMSDK_NODE_EXE)

set(PYTHON_HINTS)
if(EMSDK_PYTHON_CANDIDATES)
  list(SORT EMSDK_PYTHON_CANDIDATES COMPARE NATURAL ORDER DESCENDING)
  list(GET EMSDK_PYTHON_CANDIDATES 0 EMSDK_PYTHON_EXE)
  get_filename_component(EMSDK_PYTHON_DIR "${EMSDK_PYTHON_EXE}" DIRECTORY)
  list(APPEND PYTHON_HINTS "${EMSDK_PYTHON_DIR}")
endif()

find_program(PYTHON_EXECUTABLE NAMES python python3 HINTS ${PYTHON_HINTS})
if(NOT PYTHON_EXECUTABLE)
  message(FATAL_ERROR "python not found")
endif()

find_program(MESON_EXECUTABLE NAMES meson meson.py)
if(MESON_EXECUTABLE)
  set(MESON_INVOKE "${MESON_EXECUTABLE}")
else()
  ensure_python_module("${PYTHON_EXECUTABLE}" "mesonbuild" "meson")
  set(MESON_INVOKE "${PYTHON_EXECUTABLE}" -m mesonbuild.mesonmain)
endif()

ensure_python_module("${PYTHON_EXECUTABLE}" "mako" "mako")

if(WIN32)
  find_program(WIN_FLEX_BIN NAMES win_flex win_flex.exe)
  find_program(WIN_BISON_BIN NAMES win_bison win_bison.exe)

  if(NOT WIN_FLEX_BIN OR NOT WIN_BISON_BIN)
    ensure_python_module("${PYTHON_EXECUTABLE}" "winflexbison_bin" "winflexbison-bin")
    execute_process(
      COMMAND "${PYTHON_EXECUTABLE}" -c "import sysconfig; print(sysconfig.get_path('scripts', scheme='nt_user'))"
      OUTPUT_VARIABLE PYTHON_USER_SCRIPTS
      OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    if(PYTHON_USER_SCRIPTS)
      if(NOT WIN_FLEX_BIN AND EXISTS "${PYTHON_USER_SCRIPTS}/win_flex.exe")
        set(WIN_FLEX_BIN "${PYTHON_USER_SCRIPTS}/win_flex.exe")
      endif()
      if(NOT WIN_BISON_BIN AND EXISTS "${PYTHON_USER_SCRIPTS}/win_bison.exe")
        set(WIN_BISON_BIN "${PYTHON_USER_SCRIPTS}/win_bison.exe")
      endif()
    endif()
  endif()

  if(NOT WIN_FLEX_BIN OR NOT WIN_BISON_BIN)
    message(FATAL_ERROR "win_flex.exe or win_bison.exe not found")
  endif()

  get_filename_component(MESA_TOOL_PATH_PREFIX "${WIN_FLEX_BIN}" DIRECTORY)
  set(ENV{PATH} "${MESA_TOOL_PATH_PREFIX};$ENV{PATH}")
endif()

if(NOT EXISTS "${MESA_SRC_DIR}/meson.build")
  message(FATAL_ERROR "Mesa source is missing in ${MESA_SRC_DIR}. Re-run configure to fetch dependencies.")
endif()

set(MESA_LLVMPIPE_THIN_ARCHIVE "${MESA_BUILD_DIR}/src/gallium/drivers/llvmpipe/libllvmpipe.a")
set(MESA_LLVMPIPE_FULL_ARCHIVE "${MESA_BUILD_DIR}/src/gallium/drivers/llvmpipe/libllvmpipe.full.a")
set(MESA_LAVAPIPE_THIN_ARCHIVE "${MESA_BUILD_DIR}/src/gallium/frontends/lavapipe/liblavapipe_st.a")
set(MESA_LAVAPIPE_FULL_ARCHIVE "${MESA_BUILD_DIR}/src/gallium/frontends/lavapipe/liblavapipe_st.full.a")
set(MESA_WEBVULKAN_DRIVER_ARCHIVE "${MESA_BUILD_DIR}/libwebvulkan_driver.full.a")
set(MESA_BUILD_STATE_FILE "${MESA_BUILD_DIR}/.webvulkan_mesa_build_state.txt")
set(MESA_BUILD_SIGNATURE_VERSION "2")
set(MESA_SOURCE_REV "${MESA_GIT_REF}")
if(EXISTS "${MESA_SRC_DIR}/.git")
  find_program(GIT_EXECUTABLE NAMES git git.exe)
  if(GIT_EXECUTABLE)
    execute_process(
      COMMAND "${GIT_EXECUTABLE}" -C "${MESA_SRC_DIR}" rev-parse HEAD
      OUTPUT_VARIABLE MESA_SOURCE_REV_FROM_GIT
      OUTPUT_STRIP_TRAILING_WHITESPACE
      ERROR_QUIET
      RESULT_VARIABLE MESA_SOURCE_REV_RESULT
    )
    if(MESA_SOURCE_REV_RESULT EQUAL 0 AND NOT MESA_SOURCE_REV_FROM_GIT STREQUAL "")
      set(MESA_SOURCE_REV "${MESA_SOURCE_REV_FROM_GIT}")
    endif()
  endif()
endif()

set(MESA_BUILD_SIGNATURE
  "format=${MESA_BUILD_SIGNATURE_VERSION}\nmesa_rev=${MESA_SOURCE_REV}\nllvm_provider=${LLVM_PROVIDER}\nllvm_git_ref=${LLVM_GIT_REF}\nllvm_prebuilt_url=${LLVM_PREBUILT_URL}\nllvm_prebuilt_sha256=${LLVM_PREBUILT_SHA256}\nemsdk_root=${EMSDK_ROOT}\n"
)
if(EXISTS "${MESA_LLVMPIPE_FULL_ARCHIVE}" AND EXISTS "${MESA_LAVAPIPE_FULL_ARCHIVE}" AND EXISTS "${MESA_WEBVULKAN_DRIVER_ARCHIVE}" AND EXISTS "${MESA_BUILD_STATE_FILE}")
  file(READ "${MESA_BUILD_STATE_FILE}" MESA_EXISTING_BUILD_SIGNATURE)
  if(MESA_EXISTING_BUILD_SIGNATURE STREQUAL MESA_BUILD_SIGNATURE)
    message(STATUS "Mesa lavapipe wasm build is up to date")
    return()
  endif()
endif()

set(LLVM_PROVIDER_STAMP_FILE "${LLVM_WASM_INSTALL_DIR}/.webvulkan_llvm_provider.txt")

if(LLVM_PROVIDER STREQUAL "source")
  if(NOT EXISTS "${LLVM_SRC_DIR}/llvm/CMakeLists.txt")
    message(FATAL_ERROR "LLVM source is missing in ${LLVM_SRC_DIR}. Re-run configure to fetch dependencies.")
  endif()

  set(LLVM_EXPECTED_STAMP "source:${LLVM_GIT_REF}")
  set(LLVM_REBUILD_REQUIRED OFF)
  if(NOT EXISTS "${LLVM_WASM_INSTALL_DIR}/include/llvm/Config/llvm-config.h")
    set(LLVM_REBUILD_REQUIRED ON)
  endif()
  if(EXISTS "${LLVM_PROVIDER_STAMP_FILE}")
    file(READ "${LLVM_PROVIDER_STAMP_FILE}" LLVM_PROVIDER_STAMP_CONTENT)
  else()
    set(LLVM_PROVIDER_STAMP_CONTENT "")
  endif()
  string(STRIP "${LLVM_PROVIDER_STAMP_CONTENT}" LLVM_PROVIDER_STAMP_CONTENT)
  if(NOT LLVM_PROVIDER_STAMP_CONTENT STREQUAL LLVM_EXPECTED_STAMP)
    set(LLVM_REBUILD_REQUIRED ON)
  endif()

  if(LLVM_REBUILD_REQUIRED)
    file(REMOVE_RECURSE "${LLVM_HOST_BUILD_DIR}" "${LLVM_WASM_BUILD_DIR}" "${LLVM_WASM_INSTALL_DIR}")
  endif()

  set(LLVM_TABLEGEN_BIN "${LLVM_HOST_BUILD_DIR}/bin/${LLVM_TBLGEN_NAME}")

  if(NOT EXISTS "${LLVM_TABLEGEN_BIN}")
    file(GLOB_RECURSE LLVM_TABLEGEN_CANDIDATES "${LLVM_HOST_BUILD_DIR}/*/${LLVM_TBLGEN_NAME}")
    if(LLVM_TABLEGEN_CANDIDATES)
      list(SORT LLVM_TABLEGEN_CANDIDATES COMPARE NATURAL ORDER DESCENDING)
      list(GET LLVM_TABLEGEN_CANDIDATES 0 LLVM_TABLEGEN_BIN)
    endif()
  endif()

  if(NOT EXISTS "${LLVM_TABLEGEN_BIN}")
    file(REMOVE_RECURSE "${LLVM_HOST_BUILD_DIR}")

    run_checked(
      LABEL "Configure LLVM host tools"
      COMMAND "${CMAKE_EXECUTABLE}" -S "${LLVM_SRC_DIR}/llvm" -B "${LLVM_HOST_BUILD_DIR}"
        ${SUBBUILD_GENERATOR_ARGS}
        -DCMAKE_BUILD_TYPE=Release
        -DLLVM_TARGETS_TO_BUILD=WebAssembly
        -DLLVM_INCLUDE_TESTS=OFF
        -DLLVM_INCLUDE_EXAMPLES=OFF
        -DLLVM_INCLUDE_BENCHMARKS=OFF
        -DLLVM_INCLUDE_DOCS=OFF
        -DLLVM_ENABLE_BINDINGS=OFF
        -DLLVM_ENABLE_ZLIB=OFF
        -DLLVM_ENABLE_ZSTD=OFF
        -DLLVM_ENABLE_LIBXML2=OFF
        -DLLVM_BUILD_TOOLS=ON
        -DLLVM_INCLUDE_TOOLS=ON
    )

    run_checked(
      LABEL "Build llvm-tblgen"
      COMMAND "${CMAKE_EXECUTABLE}" --build "${LLVM_HOST_BUILD_DIR}" --target llvm-tblgen --config Release
    )
  endif()

  if(NOT EXISTS "${LLVM_TABLEGEN_BIN}")
    file(GLOB_RECURSE LLVM_TABLEGEN_CANDIDATES "${LLVM_HOST_BUILD_DIR}/*/${LLVM_TBLGEN_NAME}")
    if(LLVM_TABLEGEN_CANDIDATES)
      list(SORT LLVM_TABLEGEN_CANDIDATES COMPARE NATURAL ORDER DESCENDING)
      list(GET LLVM_TABLEGEN_CANDIDATES 0 LLVM_TABLEGEN_BIN)
    endif()
  endif()

  if(NOT EXISTS "${LLVM_TABLEGEN_BIN}")
    message(FATAL_ERROR "llvm-tblgen was not produced")
  endif()

  get_filename_component(LLVM_HOST_BIN_DIR "${LLVM_TABLEGEN_BIN}" DIRECTORY)

  if(NOT EXISTS "${LLVM_WASM_INSTALL_DIR}/include/llvm/Config/llvm-config.h")
    file(REMOVE_RECURSE "${LLVM_WASM_BUILD_DIR}")

    run_checked(
      LABEL "Configure LLVM for wasm"
      COMMAND "${CMAKE_EXECUTABLE}" -S "${LLVM_SRC_DIR}/llvm" -B "${LLVM_WASM_BUILD_DIR}"
        ${SUBBUILD_GENERATOR_ARGS}
        -DCMAKE_BUILD_TYPE=Release
        -DCMAKE_INSTALL_PREFIX=${LLVM_WASM_INSTALL_DIR}
        -DCMAKE_TOOLCHAIN_FILE=${WEBVULKAN_EMSCRIPTEN_TOOLCHAIN_FILE}
        -DEMSDK_ROOT=${EMSDK_ROOT}
        -DLLVM_TARGETS_TO_BUILD=WebAssembly
        -DLLVM_TARGET_ARCH=wasm32
        -DLLVM_HOST_TRIPLE=wasm32-unknown-emscripten
        -DLLVM_DEFAULT_TARGET_TRIPLE=wasm32-unknown-emscripten
        -DLLVM_INCLUDE_TESTS=OFF
        -DLLVM_INCLUDE_EXAMPLES=OFF
        -DLLVM_INCLUDE_BENCHMARKS=OFF
        -DLLVM_INCLUDE_DOCS=OFF
        -DLLVM_ENABLE_BINDINGS=OFF
        -DLLVM_ENABLE_THREADS=OFF
        -DLLVM_ENABLE_ZLIB=OFF
        -DLLVM_ENABLE_ZSTD=OFF
        -DLLVM_ENABLE_LIBXML2=OFF
        -DLLVM_ENABLE_RTTI=ON
        -DLLVM_BUILD_TOOLS=OFF
        -DLLVM_INCLUDE_TOOLS=OFF
        -DLLVM_INCLUDE_UTILS=OFF
        -DLLVM_BUILD_UTILS=OFF
        -DLLVM_NATIVE_TOOL_DIR=${LLVM_HOST_BIN_DIR}
        -DLLVM_TABLEGEN=${LLVM_TABLEGEN_BIN}
    )

    run_checked(
      LABEL "Build and install LLVM for wasm"
      COMMAND "${CMAKE_EXECUTABLE}" --build "${LLVM_WASM_BUILD_DIR}" --target install --config Release
    )
  endif()

  file(MAKE_DIRECTORY "${LLVM_WASM_INSTALL_DIR}")
  file(WRITE "${LLVM_PROVIDER_STAMP_FILE}" "${LLVM_EXPECTED_STAMP}\n")
elseif(LLVM_PROVIDER STREQUAL "prebuilt")
  if(LLVM_PREBUILT_URL STREQUAL "")
    message(FATAL_ERROR "LLVM_PREBUILT_URL is required when LLVM_PROVIDER=prebuilt")
  endif()

  set(LLVM_EXPECTED_STAMP "prebuilt:${LLVM_PREBUILT_URL}:${LLVM_PREBUILT_SHA256}")
  set(LLVM_DOWNLOAD_REQUIRED OFF)
  if(NOT EXISTS "${LLVM_WASM_INSTALL_DIR}/include/llvm/Config/llvm-config.h")
    set(LLVM_DOWNLOAD_REQUIRED ON)
  endif()
  if(EXISTS "${LLVM_PROVIDER_STAMP_FILE}")
    file(READ "${LLVM_PROVIDER_STAMP_FILE}" LLVM_PROVIDER_STAMP_CONTENT)
  else()
    set(LLVM_PROVIDER_STAMP_CONTENT "")
  endif()
  string(STRIP "${LLVM_PROVIDER_STAMP_CONTENT}" LLVM_PROVIDER_STAMP_CONTENT)
  if(NOT LLVM_PROVIDER_STAMP_CONTENT STREQUAL LLVM_EXPECTED_STAMP)
    set(LLVM_DOWNLOAD_REQUIRED ON)
  endif()

  if(LLVM_DOWNLOAD_REQUIRED)
    set(LLVM_PREBUILT_ARCHIVE "${LLVM_WASM_INSTALL_DIR}.archive")
    if(LLVM_PREBUILT_SHA256)
      file(DOWNLOAD "${LLVM_PREBUILT_URL}" "${LLVM_PREBUILT_ARCHIVE}" SHOW_PROGRESS EXPECTED_HASH "SHA256=${LLVM_PREBUILT_SHA256}")
    else()
      file(DOWNLOAD "${LLVM_PREBUILT_URL}" "${LLVM_PREBUILT_ARCHIVE}" SHOW_PROGRESS)
    endif()
    file(REMOVE_RECURSE "${LLVM_WASM_INSTALL_DIR}")
    file(MAKE_DIRECTORY "${LLVM_WASM_INSTALL_DIR}")
    execute_process(
      COMMAND "${CMAKE_COMMAND}" -E tar xvf "${LLVM_PREBUILT_ARCHIVE}"
      WORKING_DIRECTORY "${LLVM_WASM_INSTALL_DIR}"
      RESULT_VARIABLE LLVM_EXTRACT_RESULT
    )
    if(NOT LLVM_EXTRACT_RESULT EQUAL 0)
      message(FATAL_ERROR "Failed to extract prebuilt LLVM archive")
    endif()
    file(GLOB LLVM_EXTRACTED_CHILDREN RELATIVE "${LLVM_WASM_INSTALL_DIR}" "${LLVM_WASM_INSTALL_DIR}/*")
    if(NOT EXISTS "${LLVM_WASM_INSTALL_DIR}/include/llvm/Config/llvm-config.h" AND LLVM_EXTRACTED_CHILDREN)
      list(LENGTH LLVM_EXTRACTED_CHILDREN LLVM_CHILD_COUNT)
      if(LLVM_CHILD_COUNT EQUAL 1)
        list(GET LLVM_EXTRACTED_CHILDREN 0 LLVM_SINGLE_CHILD)
        if(EXISTS "${LLVM_WASM_INSTALL_DIR}/${LLVM_SINGLE_CHILD}/include/llvm/Config/llvm-config.h")
          file(COPY "${LLVM_WASM_INSTALL_DIR}/${LLVM_SINGLE_CHILD}/" DESTINATION "${LLVM_WASM_INSTALL_DIR}")
          file(REMOVE_RECURSE "${LLVM_WASM_INSTALL_DIR}/${LLVM_SINGLE_CHILD}")
        endif()
      endif()
    endif()
    file(WRITE "${LLVM_PROVIDER_STAMP_FILE}" "${LLVM_EXPECTED_STAMP}\n")
  endif()
else()
  if(NOT EXISTS "${LLVM_WASM_INSTALL_DIR}/include/llvm/Config/llvm-config.h")
    message(FATAL_ERROR "LLVM_PROVIDER=system requires llvm-config.h under LLVM_WASM_INSTALL_DIR")
  endif()
endif()

if(NOT EXISTS "${LLVM_WASM_INSTALL_DIR}/include/llvm/Config/llvm-config.h")
  message(FATAL_ERROR "LLVM wasm install is missing llvm-config.h")
endif()

file(GLOB LLVM_WASM_LIBS "${LLVM_WASM_INSTALL_DIR}/lib/libLLVM*.a")
if(NOT LLVM_WASM_LIBS)
  message(FATAL_ERROR "No LLVM static libraries were found in ${LLVM_WASM_INSTALL_DIR}/lib")
endif()
list(SORT LLVM_WASM_LIBS)

file(READ "${LLVM_WASM_INSTALL_DIR}/include/llvm/Config/llvm-config.h" LLVM_CONFIG_H_CONTENT)
string(REGEX MATCH "#define LLVM_VERSION_STRING \"([^\"]+)\"" LLVM_VERSION_MATCH "${LLVM_CONFIG_H_CONTENT}")
if(NOT CMAKE_MATCH_1)
  message(FATAL_ERROR "Failed to read LLVM_VERSION_STRING from llvm-config.h")
endif()
set(LLVM_VERSION "${CMAKE_MATCH_1}")

set(MESA_LLVM_SUBPROJECT_DIR "${MESA_SRC_DIR}/subprojects/llvm")
file(MAKE_DIRECTORY "${MESA_LLVM_SUBPROJECT_DIR}")
set(MESA_LLVM_MESON_FILE "${MESA_LLVM_SUBPROJECT_DIR}/meson.build")

file(TO_CMAKE_PATH "${LLVM_WASM_INSTALL_DIR}" LLVM_WASM_INSTALL_DIR_UNIX)
file(WRITE "${MESA_LLVM_MESON_FILE}" "project('llvm', ['cpp'])\n")
file(APPEND "${MESA_LLVM_MESON_FILE}" "inc = include_directories('${LLVM_WASM_INSTALL_DIR_UNIX}/include')\n")
file(APPEND "${MESA_LLVM_MESON_FILE}" "_llvm_link_args = ['-Wl,--start-group'")
foreach(LLVM_LIB_PATH IN LISTS LLVM_WASM_LIBS)
  file(TO_CMAKE_PATH "${LLVM_LIB_PATH}" LLVM_LIB_PATH_UNIX)
  file(APPEND "${MESA_LLVM_MESON_FILE}" ", '${LLVM_LIB_PATH_UNIX}'")
endforeach()
file(APPEND "${MESA_LLVM_MESON_FILE}" ", '-Wl,--end-group']\n")
file(APPEND "${MESA_LLVM_MESON_FILE}" "dep_llvm = declare_dependency(include_directories : inc, link_args : _llvm_link_args, version : '${LLVM_VERSION}')\n")
file(APPEND "${MESA_LLVM_MESON_FILE}" "has_rtti = true\n")
file(APPEND "${MESA_LLVM_MESON_FILE}" "irbuilder_h = files('${LLVM_WASM_INSTALL_DIR_UNIX}/include/llvm/IR/IRBuilder.h')\n")

if(EXISTS "${MESA_BUILD_DIR}" AND NOT EXISTS "${MESA_BUILD_DIR}/build.ninja")
  file(REMOVE_RECURSE "${MESA_BUILD_DIR}")
endif()
file(MAKE_DIRECTORY "${MESA_BUILD_DIR}")

set(MESON_NATIVE_FILE "${MESA_BUILD_DIR}.native.ini")
file(WRITE "${MESON_NATIVE_FILE}" "[binaries]\n")
if(WIN32)
  file(TO_CMAKE_PATH "${WIN_FLEX_BIN}" WIN_FLEX_BIN_UNIX)
  file(TO_CMAKE_PATH "${WIN_BISON_BIN}" WIN_BISON_BIN_UNIX)
  file(APPEND "${MESON_NATIVE_FILE}" "win_flex = '${WIN_FLEX_BIN_UNIX}'\n")
  file(APPEND "${MESON_NATIVE_FILE}" "win_bison = '${WIN_BISON_BIN_UNIX}'\n")
  file(APPEND "${MESON_NATIVE_FILE}" "flex = '${WIN_FLEX_BIN_UNIX}'\n")
  file(APPEND "${MESON_NATIVE_FILE}" "bison = '${WIN_BISON_BIN_UNIX}'\n")
endif()

set(MESON_CROSS_FILE "${MESA_BUILD_DIR}.emscripten.cross")
file(WRITE "${MESON_CROSS_FILE}" "[binaries]\n")
file(APPEND "${MESON_CROSS_FILE}" "c = '${EMCC_BIN}'\n")
file(APPEND "${MESON_CROSS_FILE}" "cpp = '${EMXX_BIN}'\n")
file(APPEND "${MESON_CROSS_FILE}" "ar = '${EMAR_BIN}'\n")
file(APPEND "${MESON_CROSS_FILE}" "ranlib = '${EMRANLIB_BIN}'\n")
file(APPEND "${MESON_CROSS_FILE}" "strip = '${EMSTRIP_BIN}'\n")
file(APPEND "${MESON_CROSS_FILE}" "exe_wrapper = '${EMSDK_NODE_EXE}'\n")
file(APPEND "${MESON_CROSS_FILE}" "\n")
file(APPEND "${MESON_CROSS_FILE}" "[host_machine]\n")
file(APPEND "${MESON_CROSS_FILE}" "system = 'emscripten'\n")
file(APPEND "${MESON_CROSS_FILE}" "cpu_family = 'wasm32'\n")
file(APPEND "${MESON_CROSS_FILE}" "cpu = 'wasm32'\n")
file(APPEND "${MESON_CROSS_FILE}" "endian = 'little'\n")
file(APPEND "${MESON_CROSS_FILE}" "\n")
file(APPEND "${MESON_CROSS_FILE}" "[properties]\n")
file(APPEND "${MESON_CROSS_FILE}" "needs_exe_wrapper = true\n")
file(APPEND "${MESON_CROSS_FILE}" "\n")
file(APPEND "${MESON_CROSS_FILE}" "[built-in options]\n")
file(APPEND "${MESON_CROSS_FILE}" "c_args = ['-D_GNU_SOURCE=1']\n")
file(APPEND "${MESON_CROSS_FILE}" "cpp_args = ['-D_GNU_SOURCE=1']\n")

if(EXISTS "${MESA_BUILD_DIR}/build.ninja")
  set(MESA_SETUP_COMMAND
    setup "${MESA_BUILD_DIR}" "${MESA_SRC_DIR}" --reconfigure
  )
else()
  set(MESA_SETUP_COMMAND
    setup "${MESA_BUILD_DIR}" "${MESA_SRC_DIR}"
  )
endif()
list(APPEND MESA_SETUP_COMMAND
  --native-file "${MESON_NATIVE_FILE}"
  --cross-file "${MESON_CROSS_FILE}"
  -Dgallium-drivers=llvmpipe
  -Dvulkan-drivers=swrast
  -Dplatforms=[]
  -Dllvm=enabled
  -Dshared-llvm=disabled
  -Ddraw-use-llvm=true
  -Dopengl=false
  -Dgles1=disabled
  -Dgles2=disabled
  -Dgbm=disabled
  -Degl=disabled
  -Dglx=disabled
  -Dglvnd=disabled
  -Dxmlconfig=disabled
  -Dzstd=disabled
  -Dzlib=disabled
  -Dshader-cache=disabled
  -Dexpat=disabled
  -Dbuild-tests=false
  -Dtools=[]
  -Dvideo-codecs=[]
  -Dgallium-va=disabled
  -Dbuildtype=release
)

run_checked(
  LABEL "Configure Mesa lavapipe for wasm"
  COMMAND "${EMCONFIGURE}" ${MESON_INVOKE} ${MESA_SETUP_COMMAND}
)

run_checked(
  LABEL "Build Mesa lavapipe for wasm"
  COMMAND "${EMMAKE}" ${MESON_INVOKE} compile -C "${MESA_BUILD_DIR}"
)

make_full_archive("${MESA_LLVMPIPE_THIN_ARCHIVE}" "${MESA_LLVMPIPE_FULL_ARCHIVE}" "${EMAR_BIN}")
make_full_archive("${MESA_LAVAPIPE_THIN_ARCHIVE}" "${MESA_LAVAPIPE_FULL_ARCHIVE}" "${EMAR_BIN}")

file(GLOB_RECURSE MESA_STATIC_ARCHIVES "${MESA_BUILD_DIR}/src/*.a")
list(SORT MESA_STATIC_ARCHIVES)
make_archive_bundle("${MESA_WEBVULKAN_DRIVER_ARCHIVE}" "${EMAR_BIN}" ${MESA_STATIC_ARCHIVES} ${LLVM_WASM_LIBS})

if(NOT EXISTS "${MESA_LLVMPIPE_FULL_ARCHIVE}")
  message(FATAL_ERROR "Mesa build completed but ${MESA_LLVMPIPE_FULL_ARCHIVE} is missing")
endif()
if(NOT EXISTS "${MESA_LAVAPIPE_FULL_ARCHIVE}")
  message(FATAL_ERROR "Mesa build completed but ${MESA_LAVAPIPE_FULL_ARCHIVE} is missing")
endif()
if(NOT EXISTS "${MESA_WEBVULKAN_DRIVER_ARCHIVE}")
  message(FATAL_ERROR "Mesa build completed but ${MESA_WEBVULKAN_DRIVER_ARCHIVE} is missing")
endif()

file(GLOB MESA_VULKAN_LVP_LIBS
  "${MESA_BUILD_DIR}/src/gallium/targets/lavapipe/libvulkan_lvp.*"
)
if(NOT MESA_VULKAN_LVP_LIBS)
  message(FATAL_ERROR "Mesa build completed but libvulkan_lvp artifact is missing")
endif()

file(WRITE "${MESA_BUILD_STATE_FILE}" "${MESA_BUILD_SIGNATURE}")
message(STATUS "Mesa lavapipe wasm build finished")
