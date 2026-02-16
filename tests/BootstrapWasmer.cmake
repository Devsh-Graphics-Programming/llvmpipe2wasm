cmake_minimum_required(VERSION 4.2)

if(WIN32)
  find_program(_WEBVULKAN_POWERSHELL_EXE NAMES pwsh powershell REQUIRED)
  execute_process(
    COMMAND "${_WEBVULKAN_POWERSHELL_EXE}" -NoProfile -ExecutionPolicy Bypass -Command "$ProgressPreference='SilentlyContinue'; iwr https://win.wasmer.io -useb | iex"
    RESULT_VARIABLE _WEBVULKAN_BOOTSTRAP_RESULT
  )
else()
  find_program(_WEBVULKAN_SH_EXE NAMES sh REQUIRED)
  execute_process(
    COMMAND "${_WEBVULKAN_SH_EXE}" -c "curl -sSfL https://get.wasmer.io | sh"
    RESULT_VARIABLE _WEBVULKAN_BOOTSTRAP_RESULT
  )
endif()

if(NOT _WEBVULKAN_BOOTSTRAP_RESULT EQUAL 0)
  message(FATAL_ERROR "Unable to install wasmer")
endif()
