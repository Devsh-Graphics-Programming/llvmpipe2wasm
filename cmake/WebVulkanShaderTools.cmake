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

function(_webvulkan_resolve_shader_compiler_script OUT_VAR)
  if(NOT DEFINED WEBVULKAN_SHADER_COMPILER_SCRIPT OR WEBVULKAN_SHADER_COMPILER_SCRIPT STREQUAL "")
    set(_default_script "${CMAKE_CURRENT_LIST_DIR}/../tools/webvulkan_compile_spirv.mjs")
    if(EXISTS "${_default_script}")
      set(WEBVULKAN_SHADER_COMPILER_SCRIPT "${_default_script}")
    endif()
  endif()
  if(NOT EXISTS "${WEBVULKAN_SHADER_COMPILER_SCRIPT}")
    message(FATAL_ERROR "WEBVULKAN_SHADER_COMPILER_SCRIPT does not exist: ${WEBVULKAN_SHADER_COMPILER_SCRIPT}")
  endif()
  set(${OUT_VAR} "${WEBVULKAN_SHADER_COMPILER_SCRIPT}" PARENT_SCOPE)
endfunction()

function(_webvulkan_compile_shader_to_spirv)
  set(options)
  set(oneValueArgs
    LANGUAGE
    SOURCE
    OUTPUT
    NODE_BIN
    DXC_WASM_JS
    HLSL_ENTRYPOINT
    HLSL_PROFILE
  )
  set(multiValueArgs DEPENDS)
  cmake_parse_arguments(WEBVULKAN_SHADER "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

  if(NOT WEBVULKAN_SHADER_LANGUAGE)
    set(WEBVULKAN_SHADER_LANGUAGE "hlsl")
  endif()

  if(NOT WEBVULKAN_SHADER_SOURCE)
    message(FATAL_ERROR "_webvulkan_compile_shader_to_spirv requires SOURCE")
  endif()
  if(NOT WEBVULKAN_SHADER_OUTPUT)
    message(FATAL_ERROR "_webvulkan_compile_shader_to_spirv requires OUTPUT")
  endif()

  _webvulkan_resolve_shader_compiler_script(_compiler_script)

  if(WEBVULKAN_SHADER_NODE_BIN)
    set(_node_bin "${WEBVULKAN_SHADER_NODE_BIN}")
  else()
    _webvulkan_resolve_node(_node_bin)
  endif()
  if(NOT EXISTS "${_node_bin}")
    message(FATAL_ERROR "Node.js executable does not exist: ${_node_bin}")
  endif()

  get_filename_component(_output_dir "${WEBVULKAN_SHADER_OUTPUT}" DIRECTORY)
  if(_output_dir)
    set(_make_dir_command COMMAND "${CMAKE_COMMAND}" -E make_directory "${_output_dir}")
  else()
    set(_make_dir_command)
  endif()

  set(_command
    "${_node_bin}" "${_compiler_script}"
    --input "${WEBVULKAN_SHADER_SOURCE}"
    --output "${WEBVULKAN_SHADER_OUTPUT}"
    --language "${WEBVULKAN_SHADER_LANGUAGE}"
  )

  if(WEBVULKAN_SHADER_LANGUAGE STREQUAL "hlsl")
    if(WEBVULKAN_SHADER_DXC_WASM_JS)
      set(_dxc_wasm_js "${WEBVULKAN_SHADER_DXC_WASM_JS}")
    elseif(DEFINED WEBVULKAN_DXC_WASM_JS AND NOT WEBVULKAN_DXC_WASM_JS STREQUAL "")
      set(_dxc_wasm_js "${WEBVULKAN_DXC_WASM_JS}")
    else()
      set(_dxc_wasm_js "")
    endif()
    if(NOT _dxc_wasm_js OR NOT EXISTS "${_dxc_wasm_js}")
      message(FATAL_ERROR "HLSL helper requires DXC Wasm JS. Set DXC_WASM_JS or WEBVULKAN_DXC_WASM_JS.")
    endif()
    list(APPEND _command --dxc-wasm-js "${_dxc_wasm_js}")
    if(WEBVULKAN_SHADER_HLSL_ENTRYPOINT)
      list(APPEND _command --hlsl-entrypoint "${WEBVULKAN_SHADER_HLSL_ENTRYPOINT}")
    endif()
    if(WEBVULKAN_SHADER_HLSL_PROFILE)
      list(APPEND _command --hlsl-profile "${WEBVULKAN_SHADER_HLSL_PROFILE}")
    endif()
  else()
    message(FATAL_ERROR "Unsupported LANGUAGE='${WEBVULKAN_SHADER_LANGUAGE}'. Expected hlsl.")
  endif()

  add_custom_command(
    OUTPUT "${WEBVULKAN_SHADER_OUTPUT}"
    ${_make_dir_command}
    COMMAND ${_command}
    DEPENDS
      "${WEBVULKAN_SHADER_SOURCE}"
      "${_compiler_script}"
      ${WEBVULKAN_SHADER_DEPENDS}
    VERBATIM
  )
endfunction()

function(webvulkan_compile_hlsl_to_spirv)
  _webvulkan_compile_shader_to_spirv(LANGUAGE hlsl ${ARGN})
endfunction()

function(webvulkan_compile_opencl_to_spirv)
  message(FATAL_ERROR "webvulkan_compile_opencl_to_spirv was removed. Use webvulkan_compile_hlsl_to_spirv.")
endfunction()
