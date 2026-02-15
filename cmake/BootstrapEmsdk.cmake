include_guard(GLOBAL)

function(_webvulkan_populate_emsdk)
  if(NOT DEFINED WASM_EMSDK_GIT_URL OR WASM_EMSDK_GIT_URL STREQUAL "")
    set(WASM_EMSDK_GIT_URL "https://github.com/emscripten-core/emsdk.git")
  endif()
  if(NOT DEFINED WASM_EMSDK_GIT_REF OR WASM_EMSDK_GIT_REF STREQUAL "")
    set(WASM_EMSDK_GIT_REF "main")
  endif()

  webvulkan_fetch_git_source(webvulkan_emsdk_src
    REPOSITORY "${WASM_EMSDK_GIT_URL}"
    TAG "${WASM_EMSDK_GIT_REF}"
    SOURCE_DIR "${WASM_EMSDK_DIR}"
  )
endfunction()

function(ensure_emsdk_ready)
  set(options)
  set(oneValueArgs EMCC_OUT TOOLCHAIN_OUT EMSDK_ROOT_OUT)
  set(multiValueArgs)
  cmake_parse_arguments(WASM "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

  if(NOT WASM_EMCC_OUT OR NOT WASM_TOOLCHAIN_OUT OR NOT WASM_EMSDK_ROOT_OUT)
    message(FATAL_ERROR "ensure_emsdk_ready requires EMCC_OUT TOOLCHAIN_OUT and EMSDK_ROOT_OUT")
  endif()

  set(EMCC_CANDIDATES)
  if(CMAKE_C_COMPILER)
    list(APPEND EMCC_CANDIDATES "${CMAKE_C_COMPILER}")
  endif()

  find_program(EMCC_FROM_PATH NAMES emcc emcc.bat)
  if(EMCC_FROM_PATH)
    list(APPEND EMCC_CANDIDATES "${EMCC_FROM_PATH}")
  endif()

  set(EMCC_FROM_PATH)
  foreach(EMCC_CANDIDATE IN LISTS EMCC_CANDIDATES)
    if(EXISTS "${EMCC_CANDIDATE}" AND EMCC_CANDIDATE MATCHES "emcc(\\.bat)?$")
      set(EMCC_FROM_PATH "${EMCC_CANDIDATE}")
      break()
    endif()
  endforeach()

  if(EMCC_FROM_PATH)
    get_filename_component(EMCC_DIR "${EMCC_FROM_PATH}" DIRECTORY)
    get_filename_component(EMSCRIPTEN_DIR "${EMCC_DIR}" DIRECTORY)
    set(TOOLCHAIN_FROM_PATH "${EMSCRIPTEN_DIR}/cmake/Modules/Platform/Emscripten.cmake")
    if(EXISTS "${TOOLCHAIN_FROM_PATH}")
      get_filename_component(EMSDK_ROOT_FROM_PATH "${EMSCRIPTEN_DIR}/../.." ABSOLUTE)
      set(${WASM_EMCC_OUT} "${EMCC_FROM_PATH}" PARENT_SCOPE)
      set(${WASM_TOOLCHAIN_OUT} "${TOOLCHAIN_FROM_PATH}" PARENT_SCOPE)
      set(${WASM_EMSDK_ROOT_OUT} "${EMSDK_ROOT_FROM_PATH}" PARENT_SCOPE)
      return()
    endif()
  endif()

  if(DEFINED ENV{EMSDK} AND NOT "$ENV{EMSDK}" STREQUAL "")
    set(EMSDK_FROM_ENV "$ENV{EMSDK}")
    if(CMAKE_HOST_WIN32)
      set(EMCC_FROM_ENV "${EMSDK_FROM_ENV}/upstream/emscripten/emcc.bat")
    else()
      set(EMCC_FROM_ENV "${EMSDK_FROM_ENV}/upstream/emscripten/emcc")
    endif()
    set(TOOLCHAIN_FROM_ENV "${EMSDK_FROM_ENV}/upstream/emscripten/cmake/Modules/Platform/Emscripten.cmake")
    if(EXISTS "${EMCC_FROM_ENV}" AND EXISTS "${TOOLCHAIN_FROM_ENV}")
      set(${WASM_EMCC_OUT} "${EMCC_FROM_ENV}" PARENT_SCOPE)
      set(${WASM_TOOLCHAIN_OUT} "${TOOLCHAIN_FROM_ENV}" PARENT_SCOPE)
      set(${WASM_EMSDK_ROOT_OUT} "${EMSDK_FROM_ENV}" PARENT_SCOPE)
      return()
    endif()
  endif()

  if(CMAKE_HOST_WIN32)
    set(EMCC_FROM_WASM_EMSDK_DIR "${WASM_EMSDK_DIR}/upstream/emscripten/emcc.bat")
  else()
    set(EMCC_FROM_WASM_EMSDK_DIR "${WASM_EMSDK_DIR}/upstream/emscripten/emcc")
  endif()
  set(TOOLCHAIN_FROM_WASM_EMSDK_DIR "${WASM_EMSDK_DIR}/upstream/emscripten/cmake/Modules/Platform/Emscripten.cmake")
  if(EXISTS "${EMCC_FROM_WASM_EMSDK_DIR}" AND EXISTS "${TOOLCHAIN_FROM_WASM_EMSDK_DIR}")
    set(${WASM_EMCC_OUT} "${EMCC_FROM_WASM_EMSDK_DIR}" PARENT_SCOPE)
    set(${WASM_TOOLCHAIN_OUT} "${TOOLCHAIN_FROM_WASM_EMSDK_DIR}" PARENT_SCOPE)
    set(${WASM_EMSDK_ROOT_OUT} "${WASM_EMSDK_DIR}" PARENT_SCOPE)
    return()
  endif()

  if(NOT WASM_AUTO_FETCH_TOOLS)
    message(FATAL_ERROR "emcc not found and WASM_AUTO_FETCH_TOOLS=OFF")
  endif()

  if(EXISTS "${WASM_EMSDK_DIR}" AND NOT EXISTS "${WASM_EMSDK_DIR}/.git")
    message(FATAL_ERROR "WASM_EMSDK_DIR exists but is not a git checkout: ${WASM_EMSDK_DIR}")
  endif()

  _webvulkan_populate_emsdk()

  if(CMAKE_HOST_WIN32)
    set(EMSDK_CMD "${WASM_EMSDK_DIR}/emsdk.bat")
    set(EMCC_PATH "${WASM_EMSDK_DIR}/upstream/emscripten/emcc.bat")
  else()
    set(EMSDK_CMD "${WASM_EMSDK_DIR}/emsdk")
    set(EMCC_PATH "${WASM_EMSDK_DIR}/upstream/emscripten/emcc")
  endif()

  set(TOOLCHAIN_PATH "${WASM_EMSDK_DIR}/upstream/emscripten/cmake/Modules/Platform/Emscripten.cmake")
  if(NOT EXISTS "${TOOLCHAIN_PATH}" OR NOT EXISTS "${EMCC_PATH}")
    execute_process(
      COMMAND "${EMSDK_CMD}" install "${WASM_EMSDK_VERSION}"
      WORKING_DIRECTORY "${WASM_EMSDK_DIR}"
      RESULT_VARIABLE INSTALL_RESULT
    )
    if(NOT INSTALL_RESULT EQUAL 0)
      message(FATAL_ERROR "Failed to install emsdk version ${WASM_EMSDK_VERSION}")
    endif()

    execute_process(
      COMMAND "${EMSDK_CMD}" activate "${WASM_EMSDK_VERSION}"
      WORKING_DIRECTORY "${WASM_EMSDK_DIR}"
      RESULT_VARIABLE ACTIVATE_RESULT
    )
    if(NOT ACTIVATE_RESULT EQUAL 0)
      message(FATAL_ERROR "Failed to activate emsdk version ${WASM_EMSDK_VERSION}")
    endif()
  endif()

  if(NOT EXISTS "${TOOLCHAIN_PATH}")
    message(FATAL_ERROR "Emscripten toolchain file not found at ${TOOLCHAIN_PATH}")
  endif()
  if(NOT EXISTS "${EMCC_PATH}")
    message(FATAL_ERROR "emcc not found at ${EMCC_PATH}")
  endif()

  set(${WASM_EMCC_OUT} "${EMCC_PATH}" PARENT_SCOPE)
  set(${WASM_TOOLCHAIN_OUT} "${TOOLCHAIN_PATH}" PARENT_SCOPE)
  set(${WASM_EMSDK_ROOT_OUT} "${WASM_EMSDK_DIR}" PARENT_SCOPE)
endfunction()
