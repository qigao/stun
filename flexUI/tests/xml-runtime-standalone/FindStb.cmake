# Upstream stb headers; vcpkg normally supplies this finder to the root build.
find_path(Stb_INCLUDE_DIR stb_sprintf.h PATH_SUFFIXES stb REQUIRED)
include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Stb REQUIRED_VARS Stb_INCLUDE_DIR)
