#pragma once
// DeviceEnumerator – DirectShow-based enumeration of video capture devices
// and their supported capture modes.
//
// Windows-only.  On non-Windows platforms the stubs return empty lists so
// the project still compiles for development/IDE work on other OSes.

#include <string>
#include <vector>

struct DeviceInfo
{
    std::string friendlyName;   // Human-readable name (may be duplicated)
    std::string devicePath;     // Unique device path / moniker display name
    std::string shortId;        // First 8 chars of FNV-1a hash (for UI display)
};

struct CaptureMode
{
    int    width      = 0;
    int    height     = 0;
    double fps        = 0.0;
    std::string subtype;        // e.g. "YUY2", "UYVY", "NV12", "MJPG"
    bool   isMjpeg    = false;
    bool   isCommon   = false;  // Matches a "common" resolution/fps
    bool   isVih2     = false;  // Format uses VIDEOINFOHEADER2 (FORMAT_VideoInfo2)

    // Display string shown in the UI combo box
    std::string displayName() const;
};

class DeviceEnumerator
{
public:
    // Returns all DirectShow video capture devices.
    static std::vector<DeviceInfo> enumerateDevices();

    // Returns all modes reported by IAMStreamConfig for the given device path.
    static std::vector<CaptureMode> enumerateModes(const std::string& devicePath);

private:
    // Mark a mode as "common" if its resolution/fps matches our whitelist.
    static bool isCommonMode(int w, int h, double fps);
};
