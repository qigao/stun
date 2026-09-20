cmake_minimum_required(VERSION 3.20)

set(_required_variables
    PRODUCER_BINARY_DIR
    CONSUMER_SOURCE_DIR
    CONSUMER_BINARY_DIR
    SDK_ONLY_CONSUMER_BINARY_DIR
    INSTALL_PREFIX
    INSTALL_LIBDIR
    BUILD_CONFIG
    CMAKE_GENERATOR_NAME
    C_COMPILER
    CXX_COMPILER
    ADDRESS_SANITIZER_ENABLED
    SALTS_ROOT_PATH
    SALTS_UTILS_ROOT_PATH
    SALTS_UTILS_PACKAGE_DIR
    SALTS_RUNTIME_DIR
    SALTS_UTILS_RUNTIME_DIR
    COMPILER_RUNTIME_DIR
    EXECUTABLE_SUFFIX)
foreach(_variable IN LISTS _required_variables)
  if(NOT DEFINED ${_variable} OR "${${_variable}}" STREQUAL "")
    message(FATAL_ERROR "${_variable} is required")
  endif()
endforeach()

file(MAKE_DIRECTORY "${INSTALL_PREFIX}" "${CONSUMER_BINARY_DIR}")

execute_process(
  COMMAND "${CMAKE_COMMAND}" --install "${PRODUCER_BINARY_DIR}"
          --config "${BUILD_CONFIG}" --prefix "${INSTALL_PREFIX}"
          --component FlexUIPlugin
  RESULT_VARIABLE _install_result
  OUTPUT_VARIABLE _install_output
  ERROR_VARIABLE _install_error)
if(NOT _install_result EQUAL 0)
  message(FATAL_ERROR
          "FlexUI Plugin package installation failed:\n${_install_output}${_install_error}")
endif()

set(_generator_options -G "${CMAKE_GENERATOR_NAME}")
if(DEFINED CMAKE_GENERATOR_PLATFORM_NAME AND
   NOT CMAKE_GENERATOR_PLATFORM_NAME STREQUAL "")
  list(APPEND _generator_options -A "${CMAKE_GENERATOR_PLATFORM_NAME}")
endif()
if(DEFINED CMAKE_GENERATOR_TOOLSET_NAME AND
   NOT CMAKE_GENERATOR_TOOLSET_NAME STREQUAL "")
  list(APPEND _generator_options -T "${CMAKE_GENERATOR_TOOLSET_NAME}")
endif()

set(_configure_command
    "${CMAKE_COMMAND}" -E env
    "SALTS_ROOT=${SALTS_ROOT_PATH}"
    "SALTS_UTILS_ROOT=${SALTS_UTILS_ROOT_PATH}"
    "${CMAKE_COMMAND}" --fresh
    -S "${CONSUMER_SOURCE_DIR}"
    -B "${CONSUMER_BINARY_DIR}"
    ${_generator_options}
    "-DCMAKE_BUILD_TYPE=${BUILD_CONFIG}"
    "-DCMAKE_C_COMPILER=${C_COMPILER}"
    "-DCMAKE_CXX_COMPILER=${CXX_COMPILER}"
    "-DFlexUI_DIR=${INSTALL_PREFIX}/${INSTALL_LIBDIR}/cmake/FlexUI"
    "-DSaltsUtils_DIR=${SALTS_UTILS_PACKAGE_DIR}"
    "-DFLEXUI_EXAMPLE_ENABLE_ADDRESS_SANITIZER=${ADDRESS_SANITIZER_ENABLED}"
    -DFLEXUI_EXAMPLE_BUILD_HOST=ON)
if(DEFINED CMAKE_MAKE_PROGRAM_PATH AND
   NOT CMAKE_MAKE_PROGRAM_PATH STREQUAL "")
  list(APPEND _configure_command
       "-DCMAKE_MAKE_PROGRAM=${CMAKE_MAKE_PROGRAM_PATH}")
endif()

execute_process(
  COMMAND ${_configure_command}
  RESULT_VARIABLE _configure_result
  OUTPUT_VARIABLE _configure_output
  ERROR_VARIABLE _configure_error)
if(NOT _configure_result EQUAL 0)
  message(FATAL_ERROR
          "FlexUI install-tree consumer configure failed:\n${_configure_output}${_configure_error}")
endif()

set(_sdk_only_configure_command
    "${CMAKE_COMMAND}" -E env
    "SALTS_ROOT=${SALTS_ROOT_PATH}"
    "SALTS_UTILS_ROOT=${SALTS_UTILS_ROOT_PATH}"
    "${CMAKE_COMMAND}" --fresh
    -S "${CONSUMER_SOURCE_DIR}"
    -B "${SDK_ONLY_CONSUMER_BINARY_DIR}"
    ${_generator_options}
    "-DCMAKE_BUILD_TYPE=${BUILD_CONFIG}"
    "-DCMAKE_C_COMPILER=${C_COMPILER}"
    "-DCMAKE_CXX_COMPILER=${CXX_COMPILER}"
    "-DFlexUI_DIR=${INSTALL_PREFIX}/${INSTALL_LIBDIR}/cmake/FlexUI"
    "-DFLEXUI_EXAMPLE_ENABLE_ADDRESS_SANITIZER=${ADDRESS_SANITIZER_ENABLED}"
    -DFLEXUI_EXAMPLE_BUILD_HOST=OFF)
if(DEFINED CMAKE_MAKE_PROGRAM_PATH AND
   NOT CMAKE_MAKE_PROGRAM_PATH STREQUAL "")
  list(APPEND _sdk_only_configure_command
       "-DCMAKE_MAKE_PROGRAM=${CMAKE_MAKE_PROGRAM_PATH}")
endif()
execute_process(
  COMMAND ${_sdk_only_configure_command}
  RESULT_VARIABLE _sdk_only_configure_result
  OUTPUT_VARIABLE _sdk_only_configure_output
  ERROR_VARIABLE _sdk_only_configure_error)
if(NOT _sdk_only_configure_result EQUAL 0)
  message(FATAL_ERROR
          "FlexUI SDK-only install-tree configure failed:\n${_sdk_only_configure_output}${_sdk_only_configure_error}")
endif()
execute_process(
  COMMAND "${CMAKE_COMMAND}" --build "${SDK_ONLY_CONSUMER_BINARY_DIR}"
          --config "${BUILD_CONFIG}" --target flexui_echo_plugin
  RESULT_VARIABLE _sdk_only_build_result
  OUTPUT_VARIABLE _sdk_only_build_output
  ERROR_VARIABLE _sdk_only_build_error)
if(NOT _sdk_only_build_result EQUAL 0)
  message(FATAL_ERROR
          "FlexUI SDK-only install-tree build failed:\n${_sdk_only_build_output}${_sdk_only_build_error}")
endif()

execute_process(
  COMMAND "${CMAKE_COMMAND}" --build "${CONSUMER_BINARY_DIR}"
          --config "${BUILD_CONFIG}"
  RESULT_VARIABLE _build_result
  OUTPUT_VARIABLE _build_output
  ERROR_VARIABLE _build_error)
if(NOT _build_result EQUAL 0)
  message(FATAL_ERROR
          "FlexUI install-tree consumer build failed:\n${_build_output}${_build_error}")
endif()

if(WIN32)
  set(_runtime_separator ";")
else()
  set(_runtime_separator ":")
endif()
set(
  ENV{PATH}
  "${COMPILER_RUNTIME_DIR}${_runtime_separator}${SALTS_RUNTIME_DIR}${_runtime_separator}${SALTS_UTILS_RUNTIME_DIR}${_runtime_separator}$ENV{PATH}")
set(_consumer_executable
    "${CONSUMER_BINARY_DIR}/bin/flexui_plugin_consumer${EXECUTABLE_SUFFIX}")
execute_process(
  COMMAND "${_consumer_executable}"
  RESULT_VARIABLE _run_result
  OUTPUT_VARIABLE _run_output
  ERROR_VARIABLE _run_error)
if(NOT _run_result EQUAL 0)
  message(FATAL_ERROR
          "FlexUI install-tree consumer execution failed:\n${_run_output}${_run_error}")
endif()
