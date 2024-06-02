#[==[

Provides the following variables:
    * `LIBV4L2_INCLUDE_DIRS`: Include directories necessary to use v4l2 library.
    * `LIBV4L2_LIBRARIES`: Libraries necessary to use v4l2 library.

Provides the following target to link with:
    * `LibV4l2::LibV4l2`

#]==]

find_path(LIBV4L2_INCLUDE_DIR
    "libv4l2.h")
find_library(LIBV4L2_LIBRARY
    "v4l2")
find_library(LIBV4L2_CONVERT_LIBRARY
    "v4lconvert")

set(LIBV4L2_INCLUDE_DIRS)
set(LIBV4L2_LIBRARIES)
list(APPEND LIBV4L2_INCLUDE_DIRS LIBV4L2_INCLUDE_DIR)
list(APPEND LIBV4L2_LIBRARIES LIBV4L2_LIBRARY LIBV4L2_CONVERT_LIBRARY)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(
    LibV4l2 DEFAULT_MSG
    LIBV4L2_INCLUDE_DIRS LIBV4L2_LIBRARIES)

if(LibV4l2_FOUND)
    add_library(LibV4l2 UNKNOWN IMPORTED)
    add_library(LibV4l2::LibV4l2 ALIAS LibV4l2)
    set_target_properties(LibV4l2 PROPERTIES
        IMPORTED_LOCATION "${LIBV4L2_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${LIBV4L2_INCLUDE_DIR}"
        IMPORTED_LINK_INTERFACE_LIBRARIES "${LIBV4L2_CONVERT_LIBRARY}")
endif()

mark_as_advanced(
        LIBV4L2_INCLUDE_DIR
        LIBV4L2_LIBRARY
        LIBV4L2_CONVERT_LIBRARY)
