#include "DeviceEnumerator.h"
#include "Logger.h"

#include <sstream>
#include <iomanip>
#include <algorithm>
#include <cstdint>

#ifdef _WIN32
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  include <windows.h>
#  include <dshow.h>
#  include <dvdmedia.h>
#  include <comdef.h>
// I420 fallback + centralised GUID notes (NV12/IYUV/MJPG come from strmiids.lib)
#  include "dshow_guids.h"

#  pragma comment(lib, "strmiids.lib")
#  pragma comment(lib, "ole32.lib")
#  pragma comment(lib, "oleaut32.lib")

// ── COM helpers ──────────────────────────────────────────────────────────
// Simple RAII wrapper for COM pointers
template<class T>
struct ComPtr {
    T* p = nullptr;
    ~ComPtr() { if (p) p->Release(); }
    T** operator&() { return &p; }
    T*  operator->() { return p; }
    operator T*() { return p; }
    bool operator!() { return !p; }
};
#endif // _WIN32

// ── FNV-1a for short IDs ─────────────────────────────────────────────────
static std::string fnv1aHex8(const std::string& s)
{
    uint32_t hash = 0x811c9dc5u;
    for (unsigned char c : s) { hash ^= c; hash *= 0x01000193u; }
    std::ostringstream oss;
    oss << std::hex << std::setw(8) << std::setfill('0') << hash;
    return oss.str();
}

// ── CaptureMode helpers ───────────────────────────────────────────────────
std::string CaptureMode::displayName() const
{
    std::ostringstream oss;
    oss << width << "x" << height << " @ " << (int)fps;
    if (fps - (int)fps > 0.001) oss << "." << (int)((fps - (int)fps) * 100.0);
    oss << " fps - ";
    if (isMjpeg) oss << "MJPEG";
    else         oss << "Uncompressed (" << subtype << ")";
    return oss.str();
}

bool DeviceEnumerator::isCommonMode(int w, int h, double fps)
{
    // Common resolutions (including SD capture-card standards)
    bool commonRes =
        (w == 1920 && h == 1080) ||
        (w == 1280 && h == 720)  ||
        (w == 854  && h == 480)  ||
        (w == 640  && h == 480)  ||
        (w == 720  && h == 576)  ||  // PAL SD
        (w == 720  && h == 480);     // NTSC SD

    // Common frame rates (including 24fps cinematic and 23.976)
    bool commonFps = (fps >= 59.0 && fps <= 61.0)  ||
                     (fps >= 29.0 && fps <= 31.0)   ||
                     (fps >= 24.9 && fps <= 25.1)   ||
                     (fps >= 49.0 && fps <= 51.0)   ||
                     (fps >= 23.9 && fps <= 24.1);   // 24 / 23.976

    return commonRes && commonFps;
}

// ── Windows implementation ────────────────────────────────────────────────
#ifdef _WIN32

std::vector<DeviceInfo> DeviceEnumerator::enumerateDevices()
{
    std::vector<DeviceInfo> result;

    CoInitializeEx(nullptr, COINIT_MULTITHREADED);

    ComPtr<ICreateDevEnum> devEnum;
    HRESULT hr = CoCreateInstance(CLSID_SystemDeviceEnum, nullptr, CLSCTX_INPROC_SERVER,
                                  IID_ICreateDevEnum, (void**)&devEnum);
    if (FAILED(hr)) {
        LOG_ERROR("CoCreateInstance(SystemDeviceEnum) failed");
        return result;
    }

    ComPtr<IEnumMoniker> enumMoniker;
    hr = devEnum->CreateClassEnumerator(CLSID_VideoInputDeviceCategory, &enumMoniker, 0);
    if (hr != S_OK) {
        LOG_INFO("No video capture devices found");
        return result;
    }

    ComPtr<IMoniker> moniker;
    while (enumMoniker->Next(1, &moniker, nullptr) == S_OK) {
        ComPtr<IPropertyBag> propBag;
        hr = moniker->BindToStorage(nullptr, nullptr, IID_IPropertyBag, (void**)&propBag);
        if (FAILED(hr)) { continue; }

        DeviceInfo info;

        VARIANT varName = {};
        varName.vt = VT_BSTR;
        if (SUCCEEDED(propBag->Read(L"FriendlyName", &varName, nullptr))) {
            int len = WideCharToMultiByte(CP_UTF8, 0, varName.bstrVal, -1,
                                         nullptr, 0, nullptr, nullptr);
            if (len > 0) {
                std::string tmp(len - 1, '\0');
                WideCharToMultiByte(CP_UTF8, 0, varName.bstrVal, -1,
                                    &tmp[0], len, nullptr, nullptr);
                info.friendlyName = tmp;
            }
            VariantClear(&varName);
        }

        VARIANT varPath = {};
        varPath.vt = VT_BSTR;
        if (SUCCEEDED(propBag->Read(L"DevicePath", &varPath, nullptr))) {
            int len = WideCharToMultiByte(CP_UTF8, 0, varPath.bstrVal, -1,
                                         nullptr, 0, nullptr, nullptr);
            if (len > 0) {
                std::string tmp(len - 1, '\0');
                WideCharToMultiByte(CP_UTF8, 0, varPath.bstrVal, -1,
                                    &tmp[0], len, nullptr, nullptr);
                info.devicePath = tmp;
            }
            VariantClear(&varPath);
        }

        // Fall back: use moniker display name as device path if empty
        if (info.devicePath.empty()) {
            LPOLESTR dispName = nullptr;
            if (SUCCEEDED(moniker->GetDisplayName(nullptr, nullptr, &dispName)) && dispName) {
                int len = WideCharToMultiByte(CP_UTF8, 0, dispName, -1,
                                             nullptr, 0, nullptr, nullptr);
                if (len > 0) {
                    std::string tmp(len - 1, '\0');
                    WideCharToMultiByte(CP_UTF8, 0, dispName, -1,
                                        &tmp[0], len, nullptr, nullptr);
                    info.devicePath = tmp;
                }
                CoTaskMemFree(dispName);
            }
        }

        info.shortId = fnv1aHex8(info.devicePath).substr(0, 8);
        result.push_back(std::move(info));

        moniker.p->Release();
        moniker.p = nullptr;
    }

    LOG_INFO("Found " + std::to_string(result.size()) + " video capture device(s)");
    return result;
}

std::vector<CaptureMode> DeviceEnumerator::enumerateModes(const std::string& devicePath)
{
    std::vector<CaptureMode> result;

    CoInitializeEx(nullptr, COINIT_MULTITHREADED);

    // Re-enumerate to find the moniker matching devicePath
    ComPtr<ICreateDevEnum> devEnum;
    HRESULT hr = CoCreateInstance(CLSID_SystemDeviceEnum, nullptr, CLSCTX_INPROC_SERVER,
                                  IID_ICreateDevEnum, (void**)&devEnum);
    if (FAILED(hr)) return result;

    ComPtr<IEnumMoniker> enumMoniker;
    hr = devEnum->CreateClassEnumerator(CLSID_VideoInputDeviceCategory, &enumMoniker, 0);
    if (hr != S_OK) return result;

    ComPtr<IMoniker> targetMoniker;
    ComPtr<IMoniker> moniker;
    while (enumMoniker->Next(1, &moniker, nullptr) == S_OK) {
        ComPtr<IPropertyBag> propBag;
        if (FAILED(moniker->BindToStorage(nullptr, nullptr, IID_IPropertyBag, (void**)&propBag))) {
            moniker.p->Release(); moniker.p = nullptr;
            continue;
        }

        // Check DevicePath
        bool match = false;
        VARIANT varPath = {};
        varPath.vt = VT_BSTR;
        if (SUCCEEDED(propBag->Read(L"DevicePath", &varPath, nullptr))) {
            int len = WideCharToMultiByte(CP_UTF8, 0, varPath.bstrVal, -1, nullptr, 0, nullptr, nullptr);
            if (len > 0) {
                std::string dp(len - 1, '\0');
                WideCharToMultiByte(CP_UTF8, 0, varPath.bstrVal, -1, &dp[0], len, nullptr, nullptr);
                match = (dp == devicePath);
            }
            VariantClear(&varPath);
        }

        // Also try display name
        if (!match) {
            LPOLESTR dispName = nullptr;
            if (SUCCEEDED(moniker->GetDisplayName(nullptr, nullptr, &dispName)) && dispName) {
                int len = WideCharToMultiByte(CP_UTF8, 0, dispName, -1, nullptr, 0, nullptr, nullptr);
                if (len > 0) {
                    std::string dn(len - 1, '\0');
                    WideCharToMultiByte(CP_UTF8, 0, dispName, -1, &dn[0], len, nullptr, nullptr);
                    match = (dn == devicePath);
                }
                CoTaskMemFree(dispName);
            }
        }

        if (match) {
            targetMoniker.p = moniker.p;
            moniker.p->AddRef();
        }
        moniker.p->Release();
        moniker.p = nullptr;
        if (match) break;
    }

    if (!targetMoniker) {
        LOG_WARN("Device not found for path: " + devicePath);
        return result;
    }

    // Bind to the filter
    ComPtr<IBaseFilter> filter;
    hr = targetMoniker->BindToObject(nullptr, nullptr, IID_IBaseFilter, (void**)&filter);
    if (FAILED(hr)) {
        LOG_ERROR("BindToObject failed for device: " + devicePath);
        return result;
    }

    // Find the capture pin
    ComPtr<IEnumPins> enumPins;
    hr = filter->EnumPins(&enumPins);
    if (FAILED(hr)) return result;

    ComPtr<IPin> pin;
    ComPtr<IAMStreamConfig> streamConfig;
    while (enumPins->Next(1, &pin, nullptr) == S_OK) {
        PIN_DIRECTION dir;
        if (SUCCEEDED(pin->QueryDirection(&dir)) && dir == PINDIR_OUTPUT) {
            if (SUCCEEDED(pin->QueryInterface(IID_IAMStreamConfig, (void**)&streamConfig))) {
                break;
            }
        }
        pin.p->Release(); pin.p = nullptr;
    }

    if (!streamConfig) {
        LOG_WARN("No IAMStreamConfig on device: " + devicePath);
        return result;
    }

    int count = 0, size = 0;
    hr = streamConfig->GetNumberOfCapabilities(&count, &size);
    if (FAILED(hr) || size != sizeof(VIDEO_STREAM_CONFIG_CAPS)) {
        LOG_WARN("GetNumberOfCapabilities failed or wrong size");
        return result;
    }

    for (int i = 0; i < count; ++i) {
        VIDEO_STREAM_CONFIG_CAPS caps = {};
        AM_MEDIA_TYPE* pmt = nullptr;
        hr = streamConfig->GetStreamCaps(i, &pmt, (BYTE*)&caps);
        if (FAILED(hr) || !pmt) continue;

        // Handle both FORMAT_VideoInfo (VIDEOINFOHEADER) and
        // FORMAT_VideoInfo2 (VIDEOINFOHEADER2 – used by many capture cards/webcams)
        bool isVih = (pmt->formattype == FORMAT_VideoInfo  && pmt->pbFormat);
        bool isVih2= (pmt->formattype == FORMAT_VideoInfo2 && pmt->pbFormat);

        if ((isVih || isVih2) && pmt->pbFormat) {
            CaptureMode mode;
            if (isVih2) {
                auto* vi2 = reinterpret_cast<VIDEOINFOHEADER2*>(pmt->pbFormat);
                mode.width  = vi2->bmiHeader.biWidth;
                mode.height = std::abs(vi2->bmiHeader.biHeight);
                if (vi2->AvgTimePerFrame > 0)
                    mode.fps = 10000000.0 / vi2->AvgTimePerFrame;
                else if (caps.MinFrameInterval > 0)
                    mode.fps = 10000000.0 / caps.MinFrameInterval;
                mode.isVih2 = true;
            } else {
                auto* vi = reinterpret_cast<VIDEOINFOHEADER*>(pmt->pbFormat);
                mode.width  = vi->bmiHeader.biWidth;
                mode.height = std::abs(vi->bmiHeader.biHeight);
                if (vi->AvgTimePerFrame > 0)
                    mode.fps = 10000000.0 / vi->AvgTimePerFrame;
                else if (caps.MinFrameInterval > 0)
                    mode.fps = 10000000.0 / caps.MinFrameInterval;
            }

            // Sub-type
            const GUID& st = pmt->subtype;
            if      (st == MEDIASUBTYPE_YUY2)  { mode.subtype = "YUY2"; }
            else if (st == MEDIASUBTYPE_NV12)  { mode.subtype = "NV12"; }
            else if (st == MEDIASUBTYPE_IYUV || st == MEDIASUBTYPE_I420) { mode.subtype = "I420"; }
            else if (st == MEDIASUBTYPE_MJPG)  { mode.subtype = "MJPG"; mode.isMjpeg = true; }
            else if (st == MEDIASUBTYPE_RGB24) { mode.subtype = "RGB24"; }
            else if (st == MEDIASUBTYPE_RGB32) { mode.subtype = "RGB32"; }
            else if (st == MEDIASUBTYPE_UYVY)  { mode.subtype = "UYVY"; }
            else {
                // HDYC: Blackmagic / AJA capture cards – same UYVY byte layout, BT.709 colour space
                if (st == MEDIASUBTYPE_HDYC) {
                    mode.subtype = "UYVY"; // treat as UYVY (same layout)
                } else {
                    // Convert GUID to string for unknown subtypes
                    OLECHAR guidStr[64];
                    StringFromGUID2(st, guidStr, 64);
                    int len = WideCharToMultiByte(CP_UTF8, 0, guidStr, -1, nullptr, 0, nullptr, nullptr);
                    if (len > 0) { mode.subtype.resize(len - 1); WideCharToMultiByte(CP_UTF8, 0, guidStr, -1, &mode.subtype[0], len, nullptr, nullptr); }
                    mode.subtype = "UNK:" + mode.subtype;
                }
            }

            mode.isCommon = isCommonMode(mode.width, mode.height, mode.fps);

            if (mode.width > 0 && mode.height > 0 && mode.fps > 0) {
                result.push_back(mode);
            }
        }

        // Free media type
        if (pmt->cbFormat && pmt->pbFormat) CoTaskMemFree(pmt->pbFormat);
        if (pmt->pUnk) pmt->pUnk->Release();
        CoTaskMemFree(pmt);
    }

    // Sort: prefer higher fps first, then higher resolution, then uncompressed
    std::sort(result.begin(), result.end(), [](const CaptureMode& a, const CaptureMode& b) {
        if (a.fps   != b.fps)    return a.fps > b.fps;
        int resA = a.width * a.height;
        int resB = b.width * b.height;
        if (resA != resB) return resA > resB;
        // Uncompressed before MJPEG
        if (a.isMjpeg != b.isMjpeg) return !a.isMjpeg;
        return false;
    });

    // Deduplicate
    result.erase(std::unique(result.begin(), result.end(), [](const CaptureMode& a, const CaptureMode& b){
        return a.width == b.width && a.height == b.height &&
               std::abs(a.fps - b.fps) < 0.5 && a.subtype == b.subtype;
    }), result.end());

    LOG_INFO("Found " + std::to_string(result.size()) + " modes for device: " + devicePath);
    return result;
}

#else // ── Non-Windows stubs ──────────────────────────────────────────────

std::vector<DeviceInfo>  DeviceEnumerator::enumerateDevices()  { return {}; }
std::vector<CaptureMode> DeviceEnumerator::enumerateModes(const std::string&) { return {}; }

#endif
