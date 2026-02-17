include_guard(GLOBAL)

function(_webvulkan_resolve_runtime_shader_registry_paths OUT_SOURCE OUT_INCLUDE)
  set(_source "")
  set(_include "")

  if(TARGET webvulkan::runtime_shader_registry)
    get_target_property(_source webvulkan::runtime_shader_registry WEBVULKAN_RUNTIME_SHADER_REGISTRY_SOURCE)
    get_target_property(_include webvulkan::runtime_shader_registry WEBVULKAN_RUNTIME_SHADER_REGISTRY_INCLUDE_DIR)
  endif()

  if((NOT _source OR _source MATCHES "-NOTFOUND$") AND DEFINED WEBVULKAN_RUNTIME_SHADER_REGISTRY_SOURCE)
    set(_source "${WEBVULKAN_RUNTIME_SHADER_REGISTRY_SOURCE}")
  endif()
  if((NOT _include OR _include MATCHES "-NOTFOUND$") AND DEFINED WEBVULKAN_RUNTIME_SHADER_REGISTRY_INCLUDE_DIR)
    set(_include "${WEBVULKAN_RUNTIME_SHADER_REGISTRY_INCLUDE_DIR}")
  endif()

  if(NOT _source OR _source MATCHES "-NOTFOUND$")
    set(_source "${CMAKE_CURRENT_LIST_DIR}/../runtime/src/webvulkan_shader_runtime_registry.c")
  endif()
  if(NOT _include OR _include MATCHES "-NOTFOUND$")
    set(_include "${CMAKE_CURRENT_LIST_DIR}/../runtime/include")
  endif()

  if(NOT EXISTS "${_source}")
    message(FATAL_ERROR "Runtime shader registry source not found: ${_source}")
  endif()
  if(NOT EXISTS "${_include}/webvulkan/webvulkan_shader_runtime_registry.h")
    message(FATAL_ERROR "Runtime shader registry include directory not found: ${_include}")
  endif()

  set(${OUT_SOURCE} "${_source}" PARENT_SCOPE)
  set(${OUT_INCLUDE} "${_include}" PARENT_SCOPE)
endfunction()

function(webvulkan_attach_runtime_shader_registry)
  set(options)
  set(oneValueArgs TARGET)
  set(multiValueArgs)
  cmake_parse_arguments(WEBVULKAN "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

  if(NOT WEBVULKAN_TARGET)
    message(FATAL_ERROR "webvulkan_attach_runtime_shader_registry requires TARGET")
  endif()
  if(NOT TARGET ${WEBVULKAN_TARGET})
    message(FATAL_ERROR "Target does not exist: ${WEBVULKAN_TARGET}")
  endif()

  _webvulkan_resolve_runtime_shader_registry_paths(_registry_source _registry_include)

  target_sources(${WEBVULKAN_TARGET} PRIVATE "${_registry_source}")
  target_include_directories(${WEBVULKAN_TARGET} PRIVATE "${_registry_include}")

  if(TARGET webvulkan::runtime_shader_registry)
    target_link_libraries(${WEBVULKAN_TARGET} PRIVATE webvulkan::runtime_shader_registry)
  endif()
endfunction()
