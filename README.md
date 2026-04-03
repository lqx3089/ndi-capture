# NDI Capture

[English](README.md) | [简体中文](README.zh-CN.md)

A Windows-focused, portable (no installer) Qt + CMake C++ application for capturing video from DirectShow/UVC devices and publishing them as **NDI Full** streams.

---

## Features

- **DirectShow device enumeration** – lists all video capture devices, each with a unique short ID to distinguish identical hardware names.
- **Mode picker** – shows common modes (1080p/720p at 60/30 fps) by default; a "Show all modes" checkbox reveals every mode the device reports.
- **MJPEG decode** – uses **libjpeg-turbo** for hardware-accelerated MJPEG frames, enabling 60 fps capture on typical UVC capture cards.
- **Pixel conversion** – uses **libyuv** (SIMD) to convert YUY2/NV12/I420 to NDI-native UYVY or BGRA.
- **Embedded preview panel** – 640 × 360 live preview inside the main window; shows capture FPS, mode label, and device ID overlay.
- **Preview without locking** – the preview does not acquire the per-device mutex; the device is shared with the OS/driver.
- **Exclusive NDI lock** – clicking *Start NDI* acquires two Win32 named mutexes:
  - `Global\NDICaptureLock_<fnv1a-hash>` (per device path) – prevents two instances streaming the same card.
  - `Global\NDISenderNameLock_<fnv1a-hash>` (per NDI name) – prevents duplicate NDI source names on the same machine.
- **Multi-process safe** – run one instance per capture card; conflicts are caught immediately with a clear error message.
- **Preview-only mode** – builds and runs without the NDI SDK (set `WITH_NDI=OFF`).
- **File logging** – writes timestamped logs to `logs/ndi-capture.log` alongside the executable.

---

## Prerequisites

| Component | Where to get it |
|-----------|----------------|
| **Windows 10/11** | — |
| **Visual Studio 2019/2022** (Desktop C++ workload) or **LLVM/Clang on Windows** | [visualstudio.microsoft.com](https://visualstudio.microsoft.com/) |
| **CMake ≥ 3.20** | [cmake.org](https://cmake.org/download/) |
| **Qt 6.x** (Widgets module) | [qt.io/download-open-source](https://www.qt.io/download-open-source) |
| **libyuv** | [chromium.googlesource.com/libyuv/libyuv](https://chromium.googlesource.com/libyuv/libyuv/) — build with CMake and install, *or* use a pre-built binary |
| **libjpeg-turbo** | [libjpeg-turbo.org](https://libjpeg-turbo.org/) — download the official Windows installer (installs to `C:\libjpeg-turbo64`) |
| **NDI Runtime** (for receiving NDI on the same machine) | [ndi.video/tools/](https://ndi.video/tools/) → *NDI Tools* |
| **NDI SDK** (for building with NDI sending support) | [ndi.video/for-developers/ndi-sdk/](https://ndi.video/for-developers/ndi-sdk/) |

---

## Build

### 1. Install dependencies

```powershell
# Example using vcpkg (recommended)
vcpkg install libyuv:x64-windows libjpeg-turbo:x64-windows

# Or set LIBYUV_DIR / LIBJPEG_TURBO_DIR manually – see cmake/ find modules.
```

### 2. Configure (preview-only, no NDI SDK required)

```powershell
cmake -S . -B build ^
  -G "Visual Studio 17 2022" -A x64 ^
  -DCMAKE_BUILD_TYPE=Release ^
  -DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake
```

### 3. Configure (with NDI SDK)

Download and install the NDI SDK, then:

```powershell
cmake -S . -B build ^
  -G "Visual Studio 17 2022" -A x64 ^
  -DCMAKE_BUILD_TYPE=Release ^
  -DWITH_NDI=ON ^
  -DNDI_SDK_DIR="C:/Program Files/NDI/NDI 6 SDK" ^
  -DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake
```

### 4. Build

```powershell
cmake --build build --config Release
```

The executable is at `build/src/Release/NDICapture.exe`.

### 5. Deploy (portable)

Copy the following to a single folder:

```
NDICapture.exe
Qt6Core.dll  Qt6Gui.dll  Qt6Widgets.dll  (from Qt bin/)
platforms/qwindows.dll                   (from Qt plugins/platforms/)
yuv.dll  (or libyuv.dll)
turbojpeg.dll  (from libjpeg-turbo)
Processing.NDI.Lib.x64.dll              (from NDI SDK / NDI Runtime)
logs/                                    (auto-created on first run)
```

> Use `windeployqt NDICapture.exe` to copy all required Qt DLLs automatically.

---

## Run

Double-click `NDICapture.exe` (no installation needed).

1. **Select a device** from the *Video Device* dropdown.
2. **Select a capture mode** – 60 fps modes appear at the top; common modes are shown by default.
3. **Type an NDI source name** in the *NDI Source Name* field.
4. Click **▶ Start NDI** to begin streaming.
5. Open NDI Studio Monitor, OBS (NDI plugin), vMix, or any NDI receiver to find the source.

---

## Multi-instance / Multi-card Notes

- Launch one `NDICapture.exe` process per capture card.
- Each process uses a different device and a different NDI source name.
- If two processes try to claim the **same device**:  
  → The second one shows *"Device busy (used by another instance)"* and refuses to start.
- If two processes try to use the **same NDI name**:  
  → The second one shows *"NDI name already in use on this machine"* and refuses to start.
- Preview does **not** lock the device – you can open multiple windows to preview the same card before deciding which instance will stream it. Clicking *Start NDI* is when the exclusive lock is taken.

---

## Status Fields

| Field | Description |
|-------|-------------|
| **Capture FPS** | Raw frames received from DirectShow |
| **Decode FPS** | MJPEG frames decoded per second (shown only for MJPEG modes) |
| **Send FPS** | Frames sent to NDI per second |
| **Dropped** | `cap N / send M` – frames dropped at capture stage vs. NDI send stage |
| **NDI sender** | Running / Not running |

---

## Project Structure

```
ndi-capture/
├── cmake/
│   ├── FindNDI.cmake
│   ├── Findlibyuv.cmake
│   └── Findlibjpeg-turbo.cmake
├── src/
│   ├── main.cpp
│   ├── MainWindow.{h,cpp}     ← Qt GUI
│   ├── CaptureSession.{h,cpp} ← DirectShow graph + frame dispatch
│   ├── DeviceEnumerator.{h,cpp}
│   ├── FrameConverter.{h,cpp} ← libyuv pixel conversion
│   ├── MjpegDecoder.{h,cpp}   ← libjpeg-turbo MJPEG decode
│   ├── NdiSenderInterface.h   ← abstract INdiSender + NullNdiSender
│   ├── NdiSender.{h,cpp}      ← NDI SDK implementation (WITH_NDI=ON)
│   ├── MutexGuard.{h,cpp}     ← Win32 named-mutex RAII
│   └── Logger.{h,cpp}
├── CMakeLists.txt
└── README.md
```

---

## License

This project is released under the **MIT License**.  
The NDI SDK is subject to NewTek's separate license agreement – it is **not** included in this repository.
