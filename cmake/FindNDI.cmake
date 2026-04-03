# FindNDI.cmake
# Locate the NewTek NDI SDK.
#
# Usage:
#   set(NDI_SDK_DIR "C:/Program Files/NDI/NDI 6 SDK")  # or set via -DNDI_SDK_DIR=
#   find_package(NDI REQUIRED)
#
# Imported target:  NDI::NDI
# Variables set:    NDI_FOUND, NDI_INCLUDE_DIR, NDI_LIBRARY

cmake_minimum_required(VERSION 3.15)

# Allow user to set NDI_SDK_DIR via environment or CMake variable
if(NOT NDI_SDK_DIR)
    set(NDI_SDK_DIR "$ENV{NDI_SDK_DIR}")
endif()

find_path(NDI_INCLUDE_DIR
    NAMES Processing.NDI.Lib.h
    PATHS
        "${NDI_SDK_DIR}/include"
        "$ENV{NDI_SDK_DIR}/include"
        "C:/Program Files/NDI/NDI 6 SDK/include"
        "C:/Program Files/NDI/NDI 5 SDK/include"
)

find_library(NDI_LIBRARY
    NAMES Processing.NDI.Lib.x64 Processing.NDI.Lib.x86
    PATHS
        "${NDI_SDK_DIR}/lib/x64"
        "${NDI_SDK_DIR}/lib/x86"
        "$ENV{NDI_SDK_DIR}/lib/x64"
        "C:/Program Files/NDI/NDI 6 SDK/lib/x64"
        "C:/Program Files/NDI/NDI 5 SDK/lib/x64"
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(NDI DEFAULT_MSG NDI_INCLUDE_DIR NDI_LIBRARY)

if(NDI_FOUND AND NOT TARGET NDI::NDI)
    add_library(NDI::NDI UNKNOWN IMPORTED)
    set_target_properties(NDI::NDI PROPERTIES
        IMPORTED_LOCATION "${NDI_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${NDI_INCLUDE_DIR}"
    )
endif()

mark_as_advanced(NDI_INCLUDE_DIR NDI_LIBRARY)
