#pragma once
// MjpegDecoder – decode a single MJPEG frame to BGRA using libjpeg-turbo.
// Without libjpeg-turbo the stub always returns false.

#include <vector>
#include <cstdint>

class MjpegDecoder
{
public:
    MjpegDecoder();
    ~MjpegDecoder();

    // Non-copyable / non-movable (holds opaque handles)
    MjpegDecoder(const MjpegDecoder&)            = delete;
    MjpegDecoder& operator=(const MjpegDecoder&) = delete;

    // Decode one MJPEG frame.
    // jpegData – compressed JPEG bytes
    // jpegSize – byte count
    // bgraOut  – filled with width*height*4 BGRA bytes
    // wOut, hOut – decoded dimensions
    // Returns true on success.
    bool decode(const uint8_t* jpegData, size_t jpegSize,
                std::vector<uint8_t>& bgraOut,
                int& wOut, int& hOut);

private:
#ifdef HAVE_LIBJPEG_TURBO
    void* m_handle = nullptr; // tjhandle
#endif
};
