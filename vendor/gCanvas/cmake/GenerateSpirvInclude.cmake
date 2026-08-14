foreach(_required IN ITEMS GLSLANG_VALIDATOR SHADER OUTPUT)
  if(NOT DEFINED ${_required} OR "${${_required}}" STREQUAL "")
    message(FATAL_ERROR "${_required} is required")
  endif()
endforeach()

if(NOT EXISTS "${GLSLANG_VALIDATOR}")
  message(FATAL_ERROR "glslangValidator was not found: ${GLSLANG_VALIDATOR}")
endif()
if(NOT EXISTS "${SHADER}")
  message(FATAL_ERROR "shader source was not found: ${SHADER}")
endif()

get_filename_component(_output_directory "${OUTPUT}" DIRECTORY)
file(MAKE_DIRECTORY "${_output_directory}")
set(_spirv "${OUTPUT}.spv")

execute_process(
  COMMAND "${GLSLANG_VALIDATOR}" -V "${SHADER}" -o "${_spirv}"
  RESULT_VARIABLE _compile_result
  OUTPUT_VARIABLE _compile_stdout
  ERROR_VARIABLE _compile_stderr)
if(NOT _compile_result EQUAL 0)
  file(REMOVE "${_spirv}")
  message(FATAL_ERROR
    "shader compilation failed (${_compile_result})\n${_compile_stdout}${_compile_stderr}")
endif()

file(READ "${_spirv}" _hex HEX)
file(REMOVE "${_spirv}")
string(TOUPPER "${_hex}" _hex)
string(LENGTH "${_hex}" _hex_length)

set(_generated "")
set(_offset 0)
set(_column 0)
while(_offset LESS _hex_length)
  if(_column EQUAL 0)
    string(APPEND _generated "        ")
  endif()
  string(SUBSTRING "${_hex}" ${_offset} 2 _byte)
  string(APPEND _generated "0x${_byte}")
  math(EXPR _offset "${_offset} + 2")
  math(EXPR _column "${_column} + 1")
  if(_offset LESS _hex_length)
    string(APPEND _generated ", ")
  endif()
  if(_column EQUAL 12 OR _offset EQUAL _hex_length)
    string(APPEND _generated "\n")
    set(_column 0)
  endif()
endwhile()

file(WRITE "${OUTPUT}" "${_generated}")
