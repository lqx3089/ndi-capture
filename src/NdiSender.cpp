#include "NdiSender.h"
#include "Logger.h"

#include <cmath>

#ifdef HAVE_NDI

NdiSender::NdiSender()
{
    if (!NDIlib_initialize()) {
        LOG_ERROR("NDIlib_initialize() failed – check NDI Runtime is installed");
    }
}

NdiSender::~NdiSender()
{
    stop();
    NDIlib_destroy();
}

bool NdiSender::start(const std::string& senderName, double fps)
{
    if (m_running) stop();

    NDIlib_send_create_t desc = {};
    desc.p_ndi_name     = senderName.c_str();
    desc.p_groups       = nullptr;
    desc.clock_video    = false; // we handle timing
    desc.clock_audio    = false;

    m_sendInstance = NDIlib_send_create(&desc);
    if (!m_sendInstance) {
        LOG_ERROR("NDIlib_send_create failed for: " + senderName);
        return false;
    }
    m_fps     = fps;
    m_running = true;
    LOG_INFO("NDI sender started: " + senderName);
    return true;
}

void NdiSender::stop()
{
    if (m_sendInstance) {
        NDIlib_send_destroy(m_sendInstance);
        m_sendInstance = nullptr;
    }
    m_running = false;
}

bool NdiSender::sendFrame(const NdiFrame& frame)
{
    if (!m_running || !m_sendInstance) return false;

    NDIlib_video_frame_v2_t ndiFrame = {};
    ndiFrame.xres          = frame.width;
    ndiFrame.yres          = frame.height;
    ndiFrame.line_stride_in_bytes = frame.lineStride;
    ndiFrame.p_data        = const_cast<uint8_t*>(frame.data);

    // Frame rate as rational
    // Common rates: 60=60000/1000, 59.94=60000/1001, 30=30000/1000 etc.
    if (std::abs(frame.fps - 59.94) < 0.1) {
        ndiFrame.frame_rate_N = 60000; ndiFrame.frame_rate_D = 1001;
    } else if (std::abs(frame.fps - 29.97) < 0.1) {
        ndiFrame.frame_rate_N = 30000; ndiFrame.frame_rate_D = 1001;
    } else {
        ndiFrame.frame_rate_N = static_cast<int>(frame.fps + 0.5) * 1000;
        ndiFrame.frame_rate_D = 1000;
    }

    if (frame.isUYVY) {
        ndiFrame.FourCC = NDIlib_FourCC_type_UYVY;
    } else {
        ndiFrame.FourCC = NDIlib_FourCC_type_BGRA;
    }

    NDIlib_send_send_video_v2(m_sendInstance, &ndiFrame);
    return true;
}

#endif // HAVE_NDI
