#include "MjpegDecoder.h"
#include "Logger.h"

#ifdef HAVE_LIBJPEG_TURBO
#  include <turbojpeg.h>
#endif

MjpegDecoder::MjpegDecoder()
{
#ifdef HAVE_LIBJPEG_TURBO
    m_handle = tjInitDecompress();
    if (!m_handle) {
        LOG_ERROR("tjInitDecompress failed");
    }
#endif
}

MjpegDecoder::~MjpegDecoder()
{
#ifdef HAVE_LIBJPEG_TURBO
    if (m_handle) {
        tjDestroy(static_cast<tjhandle>(m_handle));
        m_handle = nullptr;
    }
#endif
}

bool MjpegDecoder::decode(const uint8_t* jpegData, size_t jpegSize,
                           std::vector<uint8_t>& bgraOut,
                           int& wOut, int& hOut)
{
#ifdef HAVE_LIBJPEG_TURBO
    if (!m_handle) return false;

    tjhandle hnd = static_cast<tjhandle>(m_handle);

    int jpegSubsamp = 0, colorspace = 0;
    if (tjDecompressHeader3(hnd,
                             const_cast<uint8_t*>(jpegData),
                             static_cast<unsigned long>(jpegSize),
                             &wOut, &hOut,
                             &jpegSubsamp, &colorspace) != 0)
    {
        LOG_ERROR(std::string("tjDecompressHeader3: ") + tjGetErrorStr2(hnd));
        return false;
    }

    bgraOut.resize(static_cast<size_t>(wOut) * hOut * 4);
    if (tjDecompress2(hnd,
                      const_cast<uint8_t*>(jpegData),
                      static_cast<unsigned long>(jpegSize),
                      bgraOut.data(),
                      wOut, wOut * 4, hOut,
                      TJPF_BGRA, TJFLAG_FASTDCT) != 0)
    {
        LOG_ERROR(std::string("tjDecompress2: ") + tjGetErrorStr2(hnd));
        return false;
    }
    return true;
#else
    (void)jpegData; (void)jpegSize; (void)bgraOut;
    wOut = 0; hOut = 0;
    LOG_WARN("MjpegDecoder: libjpeg-turbo not available; cannot decode MJPEG frame");
    return false;
#endif
}
