#pragma once
// FrameConverter – converts raw pixel data to BGRA (for Qt) or UYVY/BGRA (for NDI).
// Uses libyuv for efficient SIMD-accelerated conversion.

#include <vector>
#include <cstdint>
#include <string>

class FrameConverter
{
public:
    // Convert a frame buffer to BGRA.
    // srcData  – raw bytes from DirectShow
    // subtype  – e.g. "YUY2", "NV12", "I420", "RGB24", "RGB32"
    // w, h     – frame dimensions
    // bgraOut  – output, resized to w*h*4 bytes
    // Returns true on success.
    static bool toBGRA(const uint8_t* srcData, size_t srcSize,
                       const std::string& subtype,
                       int w, int h,
                       std::vector<uint8_t>& bgraOut);

    // Scale BGRA down to target dimensions (for preview).
    static bool scaleBGRA(const uint8_t* src, int srcW, int srcH,
                          uint8_t* dst,       int dstW, int dstH);

    // Convert BGRA to UYVY for NDI Full (NDI prefers UYVY or BGRA).
    // Returns false if libyuv is unavailable.
    static bool bgraToUYVY(const uint8_t* bgra, int w, int h,
                            std::vector<uint8_t>& uyvyOut);
};
