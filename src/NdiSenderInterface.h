#pragma once
// INdiSender – abstract interface for NDI frame publishing.
// The concrete NdiSender implementation is compiled only when WITH_NDI=ON.
// A NullNdiSender is always available for preview-only mode.

#include <cstdint>
#include <string>

struct NdiFrame
{
    int            width       = 0;
    int            height      = 0;
    const uint8_t* data        = nullptr; // UYVY or BGRA bytes
    int            lineStride  = 0;       // bytes per line
    double         fps         = 60.0;
    bool           isUYVY      = true;    // false => BGRA
};

class INdiSender
{
public:
    virtual ~INdiSender() = default;

    virtual bool start(const std::string& senderName, double fps) = 0;
    virtual void stop()                                            = 0;
    virtual bool sendFrame(const NdiFrame& frame)                 = 0;
    virtual bool isRunning() const                                 = 0;
};

// ── Null sender (no-op) ───────────────────────────────────────────────────
class NullNdiSender : public INdiSender
{
public:
    bool start(const std::string&, double) override { m_running = true; return true; }
    void stop()                            override { m_running = false; }
    bool sendFrame(const NdiFrame&)        override { return true; }
    bool isRunning() const                 override { return m_running; }
private:
    bool m_running = false;
};
