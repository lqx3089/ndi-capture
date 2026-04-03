# libjpeg-turbo placeholder
#
# libjpeg-turbo is NOT included in this repository.
#
# How to get libjpeg-turbo:
#   Option A – official Windows installer (installs to C:\libjpeg-turbo64):
#     https://libjpeg-turbo.org/Documentation/OfficialBinaries
#
#   Option B – vcpkg:
#     vcpkg install libjpeg-turbo:x64-windows
#
#   Option C – build from source:
#     git clone https://github.com/libjpeg-turbo/libjpeg-turbo
#     cd libjpeg-turbo && cmake -B build -DCMAKE_INSTALL_PREFIX=<this dir>
#     cmake --build build --target install
#
#   Then set: -DLIBJPEG_TURBO_DIR=<this dir>
