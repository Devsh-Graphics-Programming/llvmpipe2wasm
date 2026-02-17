include_guard(GLOBAL)

function(_webvulkan_resolve_node OUT_VAR)
  if(DEFINED WEBVULKAN_NODE_BIN AND NOT WEBVULKAN_NODE_BIN STREQUAL "")
    set(_node "${WEBVULKAN_NODE_BIN}")
  else()
    find_program(_node NAMES node node.exe)
  endif()
  if(NOT _node OR NOT EXISTS "${_node}")
    message(FATAL_ERROR "Node.js executable not found. Set WEBVULKAN_NODE_BIN to continue.")
  endif()
  set(${OUT_VAR} "${_node}" PARENT_SCOPE)
endfunction()

function(_webvulkan_resolve_wasmer OUT_VAR)
  if(DEFINED WEBVULKAN_WASMER_BIN AND NOT WEBVULKAN_WASMER_BIN STREQUAL "")
    set(_wasmer "${WEBVULKAN_WASMER_BIN}")
  else()
    find_program(_wasmer NAMES wasmer wasmer.exe)
  endif()
  if(NOT _wasmer OR NOT EXISTS "${_wasmer}")
    message(FATAL_ERROR "Wasmer executable not found. Set WEBVULKAN_WASMER_BIN to continue.")
  endif()
  set(${OUT_VAR} "${_wasmer}" PARENT_SCOPE)
endfunction()

function(webvulkan_compile_opencl_to_spirv)
  set(options)
  set(oneValueArgs SOURCE OUTPUT NODE_BIN WASMER_BIN CLANG_PACKAGE SPIRV_PACKAGE SPIRV_ENTRYPOINT)
  set(multiValueArgs DEPENDS)
  cmake_parse_arguments(WEBVULKAN_SHADER "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

  if(NOT WEBVULKAN_SHADER_SOURCE)
    message(FATAL_ERROR "webvulkan_compile_opencl_to_spirv requires SOURCE")
  endif()
  if(NOT WEBVULKAN_SHADER_OUTPUT)
    message(FATAL_ERROR "webvulkan_compile_opencl_to_spirv requires OUTPUT")
  endif()

  if(NOT DEFINED WEBVULKAN_SHADER_COMPILER_SCRIPT OR WEBVULKAN_SHADER_COMPILER_SCRIPT STREQUAL "")
    set(_default_script "${CMAKE_CURRENT_LIST_DIR}/../tools/webvulkan_compile_spirv.mjs")
    if(EXISTS "${_default_script}")
      set(WEBVULKAN_SHADER_COMPILER_SCRIPT "${_default_script}")
    endif()
  endif()
  if(NOT EXISTS "${WEBVULKAN_SHADER_COMPILER_SCRIPT}")
    message(FATAL_ERROR "WEBVULKAN_SHADER_COMPILER_SCRIPT does not exist: ${WEBVULKAN_SHADER_COMPILER_SCRIPT}")
  endif()

  if(WEBVULKAN_SHADER_NODE_BIN)
    set(_node_bin "${WEBVULKAN_SHADER_NODE_BIN}")
  else()
    _webvulkan_resolve_node(_node_bin)
  endif()
  if(NOT EXISTS "${_node_bin}")
    message(FATAL_ERROR "Node.js executable does not exist: ${_node_bin}")
  endif()

  if(WEBVULKAN_SHADER_WASMER_BIN)
    set(_wasmer_bin "${WEBVULKAN_SHADER_WASMER_BIN}")
  else()
    _webvulkan_resolve_wasmer(_wasmer_bin)
  endif()
  if(NOT EXISTS "${_wasmer_bin}")
    message(FATAL_ERROR "Wasmer executable does not exist: ${_wasmer_bin}")
  endif()

  if(WEBVULKAN_SHADER_CLANG_PACKAGE)
    set(_clang_package "${WEBVULKAN_SHADER_CLANG_PACKAGE}")
  elseif(DEFINED WEBVULKAN_CLANG_WASM_PACKAGE AND NOT WEBVULKAN_CLANG_WASM_PACKAGE STREQUAL "")
    set(_clang_package "${WEBVULKAN_CLANG_WASM_PACKAGE}")
  else()
    set(_clang_package "clang/clang")
  endif()

  if(WEBVULKAN_SHADER_SPIRV_PACKAGE)
    set(_spirv_package "${WEBVULKAN_SHADER_SPIRV_PACKAGE}")
  elseif(DEFINED WEBVULKAN_SPIRV_WASM_PACKAGE AND NOT WEBVULKAN_SPIRV_WASM_PACKAGE STREQUAL "")
    set(_spirv_package "${WEBVULKAN_SPIRV_WASM_PACKAGE}")
  else()
    set(_spirv_package "lights0123/llvm-spir")
  endif()

  if(WEBVULKAN_SHADER_SPIRV_ENTRYPOINT)
    set(_spirv_entrypoint "${WEBVULKAN_SHADER_SPIRV_ENTRYPOINT}")
  elseif(DEFINED WEBVULKAN_SPIRV_WASM_ENTRYPOINT AND NOT WEBVULKAN_SPIRV_WASM_ENTRYPOINT STREQUAL "")
    set(_spirv_entrypoint "${WEBVULKAN_SPIRV_WASM_ENTRYPOINT}")
  else()
    set(_spirv_entrypoint "clspv")
  endif()

  get_filename_component(_output_dir "${WEBVULKAN_SHADER_OUTPUT}" DIRECTORY)
  if(_output_dir)
    set(_make_dir_command COMMAND "${CMAKE_COMMAND}" -E make_directory "${_output_dir}")
  else()
    set(_make_dir_command)
  endif()

  add_custom_command(
    OUTPUT "${WEBVULKAN_SHADER_OUTPUT}"
    ${_make_dir_command}
    COMMAND
      "${_node_bin}" "${WEBVULKAN_SHADER_COMPILER_SCRIPT}"
      --input "${WEBVULKAN_SHADER_SOURCE}"
      --output "${WEBVULKAN_SHADER_OUTPUT}"
      --wasmer "${_wasmer_bin}"
      --clang-package "${_clang_package}"
      --spirv-package "${_spirv_package}"
      --spirv-entrypoint "${_spirv_entrypoint}"
    DEPENDS
      "${WEBVULKAN_SHADER_SOURCE}"
      "${WEBVULKAN_SHADER_COMPILER_SCRIPT}"
      ${WEBVULKAN_SHADER_DEPENDS}
    VERBATIM
  )
endfunction()
