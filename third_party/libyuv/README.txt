# libyuv placeholder
#
# libyuv is NOT included in this repository.
#
# How to get libyuv:
#   Option A – vcpkg (recommended):
#     vcpkg install libyuv:x64-windows
#
#   Option B – build from source:
#     git clone https://chromium.googlesource.com/libyuv/libyuv
#     cd libyuv && cmake -B build -DCMAKE_INSTALL_PREFIX=<this dir>
#     cmake --build build --target install
#
#   Then set: -DLIBYUV_DIR=<this dir>
