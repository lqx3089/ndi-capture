cmake_minimum_required(VERSION 3.20)
project(NDICapture VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ────────────────────────────────────────────────────────────────────────────
# Options
# ────────────────────────────────────────────────────────────────────────────
option(WITH_NDI "Build with NDI SDK support (requires NDI_SDK_DIR)" OFF)

# ────────────────────────────────────────────────────────────────────────────
# Qt
# ────────────────────────────────────────────────────────────────────────────
find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets)
qt_standard_project_setup()

# ────────────────────────────────────────────────────────────────────────────
# Third-party find modules
# ────────────────────────────────────────────────────────────────────────────
list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")

find_package(libyuv REQUIRED)
find_package(libjpeg-turbo REQUIRED)

if(WITH_NDI)
    find_package(NDI REQUIRED)
endif()

# ────────────────────────────────────────────────────────────────────────────
# Subdirectories
# ────────────────────────────────────────────────────────────────────────────
add_subdirectory(src)
