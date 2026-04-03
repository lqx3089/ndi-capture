# Findlibyuv.cmake
# Locate libyuv (Google's YUV conversion library).
#
# Usage:
#   set(LIBYUV_DIR "C:/path/to/libyuv")  # or -DLIBYUV_DIR=
#   find_package(libyuv REQUIRED)
#
# Imported target:  libyuv::libyuv
# Variables set:    LIBYUV_FOUND, LIBYUV_INCLUDE_DIR, LIBYUV_LIBRARY

cmake_minimum_required(VERSION 3.15)

if(NOT LIBYUV_DIR)
    set(LIBYUV_DIR "$ENV{LIBYUV_DIR}")
endif()

find_path(LIBYUV_INCLUDE_DIR
    NAMES libyuv.h
    PATHS
        "${LIBYUV_DIR}/include"
        "$ENV{LIBYUV_DIR}/include"
        "${CMAKE_SOURCE_DIR}/third_party/libyuv/include"
)

find_library(LIBYUV_LIBRARY
    NAMES yuv libyuv
    PATHS
        "${LIBYUV_DIR}/lib"
        "${LIBYUV_DIR}/build"
        "$ENV{LIBYUV_DIR}/lib"
        "${CMAKE_SOURCE_DIR}/third_party/libyuv/lib"
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(libyuv DEFAULT_MSG LIBYUV_INCLUDE_DIR LIBYUV_LIBRARY)

if(libyuv_FOUND AND NOT TARGET libyuv::libyuv)
    add_library(libyuv::libyuv UNKNOWN IMPORTED)
    set_target_properties(libyuv::libyuv PROPERTIES
        IMPORTED_LOCATION "${LIBYUV_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${LIBYUV_INCLUDE_DIR}"
    )
endif()

mark_as_advanced(LIBYUV_INCLUDE_DIR LIBYUV_LIBRARY)
