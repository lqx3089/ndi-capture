# Findlibjpeg-turbo.cmake
# Locate libjpeg-turbo.
#
# Usage:
#   set(LIBJPEG_TURBO_DIR "C:/libjpeg-turbo64")  # or -DLIBJPEG_TURBO_DIR=
#   find_package(libjpeg-turbo REQUIRED)
#
# Imported target:  libjpeg-turbo::turbojpeg-static  (or shared)
# Variables set:    LIBJPEG_TURBO_FOUND, LIBJPEG_TURBO_INCLUDE_DIR, LIBJPEG_TURBO_LIBRARY

cmake_minimum_required(VERSION 3.15)

if(NOT LIBJPEG_TURBO_DIR)
    set(LIBJPEG_TURBO_DIR "$ENV{LIBJPEG_TURBO_DIR}")
endif()

find_path(LIBJPEG_TURBO_INCLUDE_DIR
    NAMES turbojpeg.h
    PATHS
        "${LIBJPEG_TURBO_DIR}/include"
        "$ENV{LIBJPEG_TURBO_DIR}/include"
        "C:/libjpeg-turbo64/include"
        "${CMAKE_SOURCE_DIR}/third_party/libjpeg-turbo/include"
)

find_library(LIBJPEG_TURBO_LIBRARY
    NAMES turbojpeg-static turbojpeg
    PATHS
        "${LIBJPEG_TURBO_DIR}/lib"
        "${LIBJPEG_TURBO_DIR}/lib64"
        "$ENV{LIBJPEG_TURBO_DIR}/lib"
        "C:/libjpeg-turbo64/lib"
        "${CMAKE_SOURCE_DIR}/third_party/libjpeg-turbo/lib"
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(libjpeg-turbo DEFAULT_MSG
    LIBJPEG_TURBO_INCLUDE_DIR LIBJPEG_TURBO_LIBRARY)

if(libjpeg-turbo_FOUND AND NOT TARGET libjpeg-turbo::turbojpeg)
    add_library(libjpeg-turbo::turbojpeg UNKNOWN IMPORTED)
    set_target_properties(libjpeg-turbo::turbojpeg PROPERTIES
        IMPORTED_LOCATION "${LIBJPEG_TURBO_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${LIBJPEG_TURBO_INCLUDE_DIR}"
    )
endif()

mark_as_advanced(LIBJPEG_TURBO_INCLUDE_DIR LIBJPEG_TURBO_LIBRARY)
