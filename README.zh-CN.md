# NDI Capture

[English](README.md) | [简体中文](README.zh-CN.md)

这是一个面向 Windows 的便携式（免安装）Qt + CMake C++ 应用，用于从 DirectShow/UVC 设备采集视频并发布为 **NDI Full** 流。

---

## 功能特性

- **DirectShow 设备枚举**：列出所有视频采集设备，并为同名硬件分配唯一短 ID 以便区分。
- **模式选择器**：默认展示常用模式（1080p/720p 的 60/30 fps）；勾选“Show all modes”可显示设备上报的全部模式。
- **MJPEG 解码**：使用 **libjpeg-turbo** 解码 MJPEG 帧，在常见 UVC 采集卡上可实现 60 fps 采集。
- **像素格式转换**：使用 **libyuv**（SIMD）将 YUY2/NV12/I420 转换为 NDI 原生 UYVY 或 BGRA。
- **内嵌预览面板**：主窗口内提供 640 × 360 实时预览，显示采集 FPS、模式标签和设备 ID 叠加信息。
- **预览不加锁**：预览阶段不会获取设备互斥锁；设备由操作系统/驱动共享。
- **独占 NDI 锁**：点击 *Start NDI* 时会获取两个 Win32 命名互斥体：
  - `Global\NDICaptureLock_<fnv1a-hash>`（按设备路径）——防止两个实例同时推流同一张采集卡。
  - `Global\NDISenderNameLock_<fnv1a-hash>`（按 NDI 名称）——防止同一台机器出现重复 NDI 源名称。
- **多进程安全**：可按“每张采集卡一个进程”运行；冲突会即时检测并给出清晰错误提示。
- **仅预览模式**：在不安装 NDI SDK 的情况下也可构建运行（设置 `WITH_NDI=OFF`）。
- **文件日志**：日志写入可执行文件同目录下的 `logs/ndi-capture.log`（带时间戳）。

---

## 依赖环境

| 组件 | 获取地址 |
|------|----------|
| **Windows 10/11** | — |
| **Visual Studio 2019/2022**（Desktop C++ workload）或 **Windows 上的 LLVM/Clang** | [visualstudio.microsoft.com](https://visualstudio.microsoft.com/) |
| **CMake ≥ 3.20** | [cmake.org](https://cmake.org/download/) |
| **Qt 6.x**（Widgets 模块，**LGPL**） | [qt.io/download-open-source](https://www.qt.io/download-open-source) |
| **libyuv** | [chromium.googlesource.com/libyuv/libyuv](https://chromium.googlesource.com/libyuv/libyuv/) —— 使用 CMake 构建并安装，或使用预编译版本 |
| **libjpeg-turbo** | [libjpeg-turbo.org](https://libjpeg-turbo.org/) —— 下载官方 Windows 安装包（默认安装到 `C:\libjpeg-turbo64`） |
| **NDI Runtime**（用于在同机接收 NDI） | [ndi.video/tools/](https://ndi.video/tools/) → *NDI Tools* |
| **NDI SDK**（用于构建 NDI 发送功能，需自行下载） | [ndi.video/for-developers/ndi-sdk/](https://ndi.video/for-developers/ndi-sdk/) |

---

## 构建

### 1. 安装依赖

```powershell
# 使用 vcpkg 的示例（推荐）
vcpkg install libyuv:x64-windows libjpeg-turbo:x64-windows

# 或手动设置 LIBYUV_DIR / LIBJPEG_TURBO_DIR，参见 cmake/ 下的 find 模块。
```

### 2. 配置（仅预览，不需要 NDI SDK）

```powershell
cmake -S . -B build ^
  -G "Visual Studio 17 2022" -A x64 ^
  -DCMAKE_BUILD_TYPE=Release ^
  -DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake
```

### 3. 配置（启用 NDI SDK）

请先从 NDI 开发者官网自行下载并安装 NDI SDK，然后将 `NDI_SDK_DIR` 指向本机 SDK 路径：

```powershell
cmake -S . -B build ^
  -G "Visual Studio 17 2022" -A x64 ^
  -DCMAKE_BUILD_TYPE=Release ^
  -DWITH_NDI=ON ^
  -DNDI_SDK_DIR="C:/Program Files/NDI/NDI 6 SDK" ^
  -DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake
```

### 4. 构建

```powershell
cmake --build build --config Release
```

可执行文件位于 `build/src/Release/NDICapture.exe`。

### 5. 部署（便携）

将以下文件复制到同一目录：

```
NDICapture.exe
Qt6Core.dll  Qt6Gui.dll  Qt6Widgets.dll  (来自 Qt bin/)
platforms/qwindows.dll                   (来自 Qt plugins/platforms/)
yuv.dll  (或 libyuv.dll)
turbojpeg.dll  (来自 libjpeg-turbo)
Processing.NDI.Lib.x64.dll              (来自 NDI SDK / NDI Runtime)
logs/                                    (首次运行自动创建)
```

> 可使用 `windeployqt NDICapture.exe` 自动复制所需 Qt DLL。

---

## 运行

双击 `NDICapture.exe`（无需安装）。

1. 在 *Video Device* 下拉框中选择设备。
2. 选择采集模式：60 fps 模式会优先显示；默认显示常见模式。
3. 在 *NDI Source Name* 输入框填写 NDI 源名称。
4. 点击 **▶ Start NDI** 开始推流。
5. 在 NDI Studio Monitor、OBS（NDI 插件）、vMix 或其他 NDI 接收端中查找该源。

---

## 多实例 / 多采集卡说明

- 每张采集卡建议启动一个 `NDICapture.exe` 进程。
- 每个进程使用不同设备和不同 NDI 源名称。
- 若两个进程尝试占用**同一设备**：  
  → 第二个会提示 *"Device busy (used by another instance)"* 并拒绝启动。
- 若两个进程尝试使用**相同 NDI 名称**：  
  → 第二个会提示 *"NDI name already in use on this machine"* 并拒绝启动。
- 预览阶段**不会**锁定设备：可同时打开多个窗口预览同一采集卡；点击 *Start NDI* 时才会获取独占锁。

---

## 状态字段说明

| 字段 | 说明 |
|------|------|
| **Capture FPS** | DirectShow 接收到的原始帧率 |
| **Decode FPS** | 每秒 MJPEG 解码帧数（仅 MJPEG 模式显示） |
| **Send FPS** | 每秒发送到 NDI 的帧数 |
| **Dropped** | `cap N / send M` —— 采集阶段丢帧数 vs NDI 发送阶段丢帧数 |
| **NDI sender** | Running / Not running |

---

## 项目结构

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
├── README.md
└── README.zh-CN.md
```

---

## 许可证

本项目基于 **LGPL-3.0-or-later** 发布。
详见 `LICENSE/LICENSE`。

Qt 以 **LGPL** 方式使用。为满足 LGPL 要求，本项目按动态链接 Qt 库的方式进行分发。
Qt 许可证文本请放入 `LICENSE/` 目录（见 `LICENSE/QT-LICENSE-PLACEHOLDER.txt`）。
第三方依赖的许可证与声明见 `LICENSE/THIRD-PARTY-NOTICES.md`。

NDI SDK 受 NewTek 单独许可协议约束，**不**包含在本仓库中。使用者在启用 `-DWITH_NDI=ON` 构建前，需自行下载并安装 NDI SDK。
