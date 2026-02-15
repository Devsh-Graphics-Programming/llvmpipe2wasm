include_guard(GLOBAL)

if(NOT COMMAND CPMAddPackage)
  message(FATAL_ERROR "CPMAddPackage is unavailable. Include BootstrapCpm.cmake before this module.")
endif()

if(NOT DEFINED WEBVULKAN_THIRDPARTY_STATE_DIR OR WEBVULKAN_THIRDPARTY_STATE_DIR STREQUAL "")
  set(WEBVULKAN_THIRDPARTY_STATE_DIR "${CMAKE_SOURCE_DIR}/.3rdparty/.state")
endif()
file(MAKE_DIRECTORY "${WEBVULKAN_THIRDPARTY_STATE_DIR}")

function(_webvulkan_state_file_for_source NAME OUT_VAR)
  set(${OUT_VAR} "${WEBVULKAN_THIRDPARTY_STATE_DIR}/${NAME}.state" PARENT_SCOPE)
endfunction()

function(_webvulkan_source_state_key REPOSITORY TAG OUT_VAR)
  set(${OUT_VAR} "${REPOSITORY}|${TAG}" PARENT_SCOPE)
endfunction()

function(_webvulkan_git_source_matches_tag SOURCE_DIR TAG OUT_VAR)
  set(_matches OFF)
  if(EXISTS "${SOURCE_DIR}/.git")
    find_package(Git QUIET)
    if(Git_FOUND)
      execute_process(
        COMMAND "${GIT_EXECUTABLE}" -C "${SOURCE_DIR}" rev-parse HEAD
        OUTPUT_VARIABLE _head_commit
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
        RESULT_VARIABLE _head_result
      )
      execute_process(
        COMMAND "${GIT_EXECUTABLE}" -C "${SOURCE_DIR}" rev-parse --verify "${TAG}^{commit}"
        OUTPUT_VARIABLE _tag_commit
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
        RESULT_VARIABLE _tag_result
      )
      if(_head_result EQUAL 0 AND _tag_result EQUAL 0 AND _head_commit STREQUAL _tag_commit)
        set(_matches ON)
      elseif(_head_result EQUAL 0 AND _head_commit STREQUAL "${TAG}")
        set(_matches ON)
      endif()
    endif()
  endif()
  set(${OUT_VAR} "${_matches}" PARENT_SCOPE)
endfunction()

function(webvulkan_fetch_git_source NAME)
  set(options)
  set(oneValueArgs REPOSITORY TAG SOURCE_DIR)
  set(multiValueArgs)
  cmake_parse_arguments(FETCH "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

  if(NOT FETCH_REPOSITORY)
    message(FATAL_ERROR "webvulkan_fetch_git_source(${NAME}) requires REPOSITORY")
  endif()
  if(NOT FETCH_TAG)
    message(FATAL_ERROR "webvulkan_fetch_git_source(${NAME}) requires TAG")
  endif()
  if(NOT FETCH_SOURCE_DIR)
    message(FATAL_ERROR "webvulkan_fetch_git_source(${NAME}) requires SOURCE_DIR")
  endif()

  _webvulkan_state_file_for_source("${NAME}" _state_file)
  _webvulkan_source_state_key("${FETCH_REPOSITORY}" "${FETCH_TAG}" _state_key)

  set(_should_fetch ON)
  if(EXISTS "${FETCH_SOURCE_DIR}/.git")
    set(_should_fetch OFF)
    if(EXISTS "${_state_file}")
      file(READ "${_state_file}" _state_content)
      string(STRIP "${_state_content}" _state_content)
      if(NOT _state_content STREQUAL _state_key)
        set(_should_fetch ON)
      endif()
    else()
      _webvulkan_git_source_matches_tag("${FETCH_SOURCE_DIR}" "${FETCH_TAG}" _matches_tag)
      if(NOT _matches_tag)
        set(_should_fetch ON)
      endif()
    endif()
  elseif(EXISTS "${FETCH_SOURCE_DIR}")
    message(FATAL_ERROR "Source directory exists but is not a git checkout: ${FETCH_SOURCE_DIR}")
  endif()

  if(NOT _should_fetch)
    if(NOT EXISTS "${_state_file}")
      file(WRITE "${_state_file}" "${_state_key}\n")
    endif()
    message(STATUS "Using cached source for ${NAME} from ${FETCH_SOURCE_DIR}")
    return()
  endif()

  CPMAddPackage(
    NAME "${NAME}"
    GIT_REPOSITORY "${FETCH_REPOSITORY}"
    GIT_TAG "${FETCH_TAG}"
    GIT_SHALLOW TRUE
    GIT_PROGRESS TRUE
    GIT_REMOTE_UPDATE_STRATEGY CHECKOUT
    SOURCE_DIR "${FETCH_SOURCE_DIR}"
    DOWNLOAD_ONLY YES
  )

  if(NOT EXISTS "${FETCH_SOURCE_DIR}/.git")
    message(FATAL_ERROR "Failed to fetch ${NAME} into ${FETCH_SOURCE_DIR}")
  endif()
  file(WRITE "${_state_file}" "${_state_key}\n")
endfunction()
