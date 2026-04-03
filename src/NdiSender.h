#pragma once
// NdiSender – concrete INdiSender backed by the NewTek NDI SDK.
// Only compiled when WITH_NDI=ON (i.e. HAVE_NDI is defined).

#include "NdiSenderInterface.h"

#ifdef HAVE_NDI

#include <Processing.NDI.Lib.h>

class NdiSender : public INdiSender
{
public:
    NdiSender();
    ~NdiSender() override;

    bool start(const std::string& senderName, double fps) override;
    void stop()                                            override;
    bool sendFrame(const NdiFrame& frame)                 override;
    bool isRunning() const                                 override { return m_running; }

private:
    NDIlib_send_instance_t  m_sendInstance = nullptr;
    bool                    m_running      = false;
    double                  m_fps          = 60.0;
};

#endif // HAVE_NDI
