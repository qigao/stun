cmake_minimum_required(VERSION 3.27)
project(FlexUIUnicodeInstallConsumer LANGUAGES CXX)

set(FLEXUI_UNICODE_INSTALL_PREFIX "" CACHE PATH
    "Isolated prefix containing the installed Salts and SaltsUtils packages")
if(NOT IS_ABSOLUTE "${FLEXUI_UNICODE_INSTALL_PREFIX}" OR
   NOT IS_DIRECTORY "${FLEXUI_UNICODE_INSTALL_PREFIX}")
  message(FATAL_ERROR "FLEXUI_UNICODE_INSTALL_PREFIX must be an existing absolute directory")
endif()
file(REAL_PATH "${FLEXUI_UNICODE_INSTALL_PREFIX}" FLEXUI_UNICODE_INSTALL_PREFIX)
if(NOT DEFINED ENV{SALTS_ROOT} OR "$ENV{SALTS_ROOT}" STREQUAL "")
  message(FATAL_ERROR "SALTS_ROOT must name the isolated installed Salts profile")
endif()
file(REAL_PATH "$ENV{SALTS_ROOT}" _salts_root)
if(NOT "${_salts_root}" STREQUAL "${FLEXUI_UNICODE_INSTALL_PREFIX}")
  message(FATAL_ERROR "SALTS_ROOT must match FLEXUI_UNICODE_INSTALL_PREFIX")
endif()

set(CMAKE_FIND_USE_PACKAGE_REGISTRY OFF)
set(CMAKE_FIND_USE_SYSTEM_PACKAGE_REGISTRY OFF)
find_package(Salts CONFIG REQUIRED
             PATHS "${FLEXUI_UNICODE_INSTALL_PREFIX}"
             NO_DEFAULT_PATH NO_CMAKE_FIND_ROOT_PATH)
find_package(SaltsUtils CONFIG REQUIRED
             PATHS "${FLEXUI_UNICODE_INSTALL_PREFIX}"
             NO_DEFAULT_PATH NO_CMAKE_FIND_ROOT_PATH)
foreach(_config IN ITEMS "${Salts_CONFIG}" "${SaltsUtils_CONFIG}")
  file(REAL_PATH "${_config}" _config_path)
  cmake_path(IS_PREFIX FLEXUI_UNICODE_INSTALL_PREFIX "${_config_path}" NORMALIZE _inside_prefix)
  if(NOT _inside_prefix)
    message(FATAL_ERROR "Package config escaped the isolated install prefix: ${_config_path}")
  endif()
endforeach()

add_executable(test_text_util_unicode_install
               ../../src/text_util.cpp
               ../test_text_util_unicode.cpp)
target_include_directories(test_text_util_unicode_install PRIVATE ../../include)
target_compile_features(test_text_util_unicode_install PRIVATE cxx_std_17)
set_target_properties(test_text_util_unicode_install PROPERTIES CXX_EXTENSIONS OFF)
target_link_libraries(test_text_util_unicode_install
                      PRIVATE Salts::Unicode Salts::TinyTest)

option(FLEXUI_UNICODE_ENABLE_SANITIZERS "Enable the Linux ASan/UBSan contract" ON)
if(FLEXUI_UNICODE_ENABLE_SANITIZERS)
  if(NOT CMAKE_SYSTEM_NAME STREQUAL "Linux" OR
     NOT CMAKE_CXX_COMPILER_ID MATCHES "^(GNU|Clang)$")
    message(FATAL_ERROR "The ASan/UBSan contract requires Linux with GCC or Clang")
  endif()
  target_compile_options(test_text_util_unicode_install PRIVATE
                         -g -O1 -Wall -Wextra -Werror
                         -fsanitize=address,undefined -fno-omit-frame-pointer)
  target_link_options(test_text_util_unicode_install PRIVATE
                      -fsanitize=address,undefined -fno-omit-frame-pointer)
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  target_link_options(test_text_util_unicode_install PRIVATE
                      "LINKER:-Map,${CMAKE_CURRENT_BINARY_DIR}/unicode-install-$<CONFIG>.map")
endif()

# These paths complement the link map and a build with dependency sources absent.
file(GENERATE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/installed-targets-$<CONFIG>.txt"
     CONTENT "SaltsConfig=${Salts_CONFIG}\nSaltsUtilsConfig=${SaltsUtils_CONFIG}\nUnicode=$<TARGET_FILE:Salts::Unicode>\nCore=$<TARGET_FILE:Salts::Core>\nTinyTest=$<TARGET_FILE:Salts::TinyTest>\n")

enable_testing()
add_test(NAME test_text_util_unicode_install COMMAND test_text_util_unicode_install)
set_tests_properties(test_text_util_unicode_install PROPERTIES TIMEOUT 120)
