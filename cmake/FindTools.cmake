# Use the repository's actual Lemon target and one REQUIRED re2c tool.
# Never detect a system Lemon fallback or continue with a warning.
find_program(RE2C_EXECUTABLE NAMES re2c REQUIRED)
set(LEMON_EXECUTABLE "$<TARGET_FILE:lemon>")
set(LEMON_DEPENDS lemon)
set(LEMPAR "${CMAKE_SOURCE_DIR}/tools/lemon/lempar.c" CACHE FILEPATH
    "Canonical Lemon parser template")
