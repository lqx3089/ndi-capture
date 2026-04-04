#include "FrameConverter.h"
#include "Logger.h"

#include <cstring>
#include <stdexcept>

// Include libyuv if available
#ifdef HAVE_LIBYUV
#  include <libyuv.h>
#endif

bool FrameConverter::toBGRA(const uint8_t* srcData, size_t srcSize,
                             const std::string& subtype,
                             int w, int h,
                             std::vector<uint8_t>& bgraOut)
{
    bgraOut.resize(static_cast<size_t>(w) * h * 4);
    uint8_t* dst = bgraOut.data();

#ifdef HAVE_LIBYUV
    if (subtype == "YUY2") {
        // YUY2 = YUYV packed, stride = w*2
        return libyuv::YUY2ToARGB(srcData, w * 2,
                                   dst,     w * 4,
                                   w, h) == 0;
    }
    if (subtype == "UYVY") {
        return libyuv::UYVYToARGB(srcData, w * 2,
                                   dst,     w * 4,
                                   w, h) == 0;
    }
    if (subtype == "NV12") {
        // NV12: Y plane then interleaved UV
        const uint8_t* yPlane  = srcData;
        const uint8_t* uvPlane = srcData + (size_t)w * h;
        return libyuv::NV12ToARGB(yPlane, w,
                                   uvPlane, w,
                                   dst, w * 4,
                                   w, h) == 0;
    }
    if (subtype == "I420") {
        const uint8_t* yPlane  = srcData;
        const uint8_t* uPlane  = yPlane  + (size_t)w * h;
        const uint8_t* vPlane  = uPlane  + (size_t)(w / 2) * (h / 2);
        return libyuv::I420ToARGB(yPlane, w,
                                   uPlane, w / 2,
                                   vPlane, w / 2,
                                   dst, w * 4,
                                   w, h) == 0;
    }
    if (subtype == "RGB24") {
        return libyuv::RAWToARGB(srcData, w * 3,
                                  dst,     w * 4,
                                  w, h) == 0;
    }
    if (subtype == "RGB32") {
        // RGB32 = BGRA in DirectShow; just copy
        if (srcSize >= static_cast<size_t>(w) * h * 4) {
            std::memcpy(dst, srcData, static_cast<size_t>(w) * h * 4);
            return true;
        }
        return false;
    }
#else
    // Without libyuv, do a minimal software conversion
    // BT.601 YCbCr -> BGR helper (reused by YUY2 and UYVY paths below)
    auto clampByte = [](int v) -> uint8_t { return (uint8_t)(v < 0 ? 0 : v > 255 ? 255 : v); };
    if (subtype == "YUY2") {
        for (int y = 0; y < h; ++y) {
            const uint8_t* src = srcData + y * w * 2;
            uint8_t*       d   = dst     + y * w * 4;
            for (int x = 0; x < w; x += 2) {
                int Y0 = src[0]; int Cb = src[1] - 128;
                int Y1 = src[2]; int Cr = src[3] - 128;
                // BT.601 coefficients
                d[0] = clampByte(Y0 + (int)(1.772 * Cb));
                d[1] = clampByte(Y0 - (int)(0.344 * Cb) - (int)(0.714 * Cr));
                d[2] = clampByte(Y0 + (int)(1.402 * Cr));
                d[3] = 255;
                d[4] = clampByte(Y1 + (int)(1.772 * Cb));
                d[5] = clampByte(Y1 - (int)(0.344 * Cb) - (int)(0.714 * Cr));
                d[6] = clampByte(Y1 + (int)(1.402 * Cr));
                d[7] = 255;
                src += 4; d += 8;
            }
        }
        return true;
    }
    if (subtype == "UYVY") {
        // UYVY: U Y0 V Y1 (same as YUY2 but byte-swapped)
        for (int y = 0; y < h; ++y) {
            const uint8_t* src = srcData + y * w * 2;
            uint8_t*       d   = dst     + y * w * 4;
            for (int x = 0; x < w; x += 2) {
                int Cb = src[0] - 128;
                int Y0 = src[1];
                int Cr = src[2] - 128;
                int Y1 = src[3];
                d[0] = clampByte(Y0 + (int)(1.772 * Cb));
                d[1] = clampByte(Y0 - (int)(0.344 * Cb) - (int)(0.714 * Cr));
                d[2] = clampByte(Y0 + (int)(1.402 * Cr));
                d[3] = 255;
                d[4] = clampByte(Y1 + (int)(1.772 * Cb));
                d[5] = clampByte(Y1 - (int)(0.344 * Cb) - (int)(0.714 * Cr));
                d[6] = clampByte(Y1 + (int)(1.402 * Cr));
                d[7] = 255;
                src += 4; d += 8;
            }
        }
        return true;
    }
    if (subtype == "RGB24") {
        for (int i = 0; i < w * h; ++i) {
            dst[i*4+0] = srcData[i*3+2];
            dst[i*4+1] = srcData[i*3+1];
            dst[i*4+2] = srcData[i*3+0];
            dst[i*4+3] = 255;
        }
        return true;
    }
    if (subtype == "RGB32") {
        if (srcSize >= static_cast<size_t>(w) * h * 4) {
            std::memcpy(dst, srcData, static_cast<size_t>(w) * h * 4);
            return true;
        }
    }
#endif
    LOG_WARN("FrameConverter: unsupported subtype: " + subtype);
    return false;
}

bool FrameConverter::scaleBGRA(const uint8_t* src, int srcW, int srcH,
                                uint8_t* dst,       int dstW, int dstH)
{
#ifdef HAVE_LIBYUV
    return libyuv::ARGBScale(src, srcW * 4,
                              srcW, srcH,
                              dst,  dstW * 4,
                              dstW, dstH,
                              libyuv::kFilterBilinear) == 0;
#else
    // Nearest-neighbor fallback
    float sx = (float)srcW / dstW;
    float sy = (float)srcH / dstH;
    for (int dy = 0; dy < dstH; ++dy) {
        for (int dx = 0; dx < dstW; ++dx) {
            int srcX = (int)(dx * sx);
            int srcY = (int)(dy * sy);
            srcX = srcX < srcW ? srcX : srcW - 1;
            srcY = srcY < srcH ? srcY : srcH - 1;
            const uint8_t* s = src + (srcY * srcW + srcX) * 4;
            uint8_t*       d = dst + (dy   * dstW + dx)   * 4;
            d[0] = s[0]; d[1] = s[1]; d[2] = s[2]; d[3] = s[3];
        }
    }
    return true;
#endif
}

bool FrameConverter::bgraToUYVY(const uint8_t* bgra, int w, int h,
                                  std::vector<uint8_t>& uyvyOut)
{
#ifdef HAVE_LIBYUV
    uyvyOut.resize(static_cast<size_t>(w) * h * 2);
    return libyuv::ARGBToUYVY(bgra,          w * 4,
                                uyvyOut.data(), w * 2,
                                w, h) == 0;
#else
    // Software BGRA -> UYVY (BT.601)
    uyvyOut.resize(static_cast<size_t>(w) * h * 2);
    uint8_t* out = uyvyOut.data();
    for (int y = 0; y < h; ++y) {
        const uint8_t* row = bgra + y * w * 4;
        uint8_t*       o   = out  + y * w * 2;
        for (int x = 0; x < w; x += 2) {
            int b0 = row[x*4+0], g0 = row[x*4+1], r0 = row[x*4+2];
            int b1 = row[x*4+4], g1 = row[x*4+5], r1 = row[x*4+6];
            int y0 = (( 66*r0 + 129*g0 +  25*b0 + 128)>>8) + 16;
            int y1 = (( 66*r1 + 129*g1 +  25*b1 + 128)>>8) + 16;
            int cb = ((-38*r0 -  74*g0 + 112*b0 + 128)>>8) + 128;
            int cr = ((112*r0 -  94*g0 -  18*b0 + 128)>>8) + 128;
            auto clamp = [](int v) -> uint8_t { return (uint8_t)(v<0?0:v>255?255:v); };
            // UYVY: U Y0 V Y1
            o[0] = clamp(cb);
            o[1] = clamp(y0);
            o[2] = clamp(cr);
            o[3] = clamp(y1);
            o += 4;
        }
    }
    return true;
#endif
}
