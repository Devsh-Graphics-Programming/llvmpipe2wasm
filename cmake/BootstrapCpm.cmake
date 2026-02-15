include_guard(GLOBAL)

function(webvulkan_bootstrap_cpm)
  if(COMMAND CPMAddPackage)
    return()
  endif()

  if(NOT DEFINED WEBVULKAN_CPM_VERSION OR WEBVULKAN_CPM_VERSION STREQUAL "")
    set(WEBVULKAN_CPM_VERSION "0.40.2")
  endif()
  if(NOT DEFINED WEBVULKAN_CPM_DIR OR WEBVULKAN_CPM_DIR STREQUAL "")
    set(WEBVULKAN_CPM_DIR "${CMAKE_CURRENT_LIST_DIR}/../.3rdparty/cpm")
  endif()

  file(MAKE_DIRECTORY "${WEBVULKAN_CPM_DIR}")

  set(CPM_FILE "${WEBVULKAN_CPM_DIR}/CPM_${WEBVULKAN_CPM_VERSION}.cmake")
  set(CPM_URL "https://github.com/cpm-cmake/CPM.cmake/releases/download/v${WEBVULKAN_CPM_VERSION}/CPM.cmake")

  if(NOT EXISTS "${CPM_FILE}")
    message(STATUS "Downloading CPM.cmake v${WEBVULKAN_CPM_VERSION}")
    file(DOWNLOAD "${CPM_URL}" "${CPM_FILE}" STATUS CPM_DOWNLOAD_STATUS TLS_VERIFY ON)
    list(GET CPM_DOWNLOAD_STATUS 0 CPM_DOWNLOAD_CODE)
    if(NOT CPM_DOWNLOAD_CODE EQUAL 0)
      list(GET CPM_DOWNLOAD_STATUS 1 CPM_DOWNLOAD_MESSAGE)
      file(REMOVE "${CPM_FILE}")
      message(FATAL_ERROR "Failed to download CPM.cmake from ${CPM_URL}: ${CPM_DOWNLOAD_MESSAGE}")
    endif()
  endif()

  include("${CPM_FILE}")
endfunction()
