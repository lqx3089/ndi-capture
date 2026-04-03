#pragma once
// CaptureSession – manages a DirectShow capture graph.
//
// Single device open → frames distributed to:
//   • Preview callback (scaled BGRA)
//   • NDI sender (when started)
//
// Thread model:
//   • DirectShow delivers frames on its own graph thread (SampleCB).
//   • All Qt UI updates happen via signal/slot queued connections.

#include "DeviceEnumerator.h"
#include "MjpegDecoder.h"
#include "FrameConverter.h"
#include "NdiSenderInterface.h"

#include <QObject>
#include <QImage>
#include <memory>
#include <atomic>
#include <chrono>
#include <mutex>

#ifdef _WIN32
#  define WIN32_LEAN_AND_MEAN
#  include <windows.h>
#  include <dshow.h>

// Forward declare our sample grabber callback
class SampleCallback;
#endif

class CaptureSession : public QObject
{
    Q_OBJECT

public:
    explicit CaptureSession(QObject* parent = nullptr);
    ~CaptureSession() override;

    // ── Preview (no device mutex) ─────────────────────────────────────────
    // Open device and start delivering preview frames.
    // Returns false if the device cannot be opened (hardware busy, etc.)
    bool startPreview(const DeviceInfo& device, const CaptureMode& mode);
    void stopPreview();

    // ── NDI streaming ─────────────────────────────────────────────────────
    // Must be called after startPreview (shares the capture graph).
    // Takes the device mutex before enabling NDI dispatch.
    bool startNdi(std::shared_ptr<INdiSender> sender);
    void stopNdi();

    bool isPreviewRunning() const { return m_previewRunning.load(); }
    bool isNdiRunning()     const { return m_ndiRunning.load();     }

    // Stats (updated atomically by graph thread)
    double captureFps()     const { return m_captureFps;  }
    double decodeFps()      const { return m_decodeFps;   }
    double sendFps()        const { return m_sendFps;     }
    int    droppedCapture() const { return m_droppedCapture.load(); }
    int    droppedSend()    const { return m_droppedSend.load();    }

signals:
    void previewFrame(QImage image);
    void statsUpdated(double capFps, double decFps, double sndFps,
                      int dropCapture, int dropSend);
    void error(QString message);

private:
#ifdef _WIN32
    // ── DirectShow graph ──────────────────────────────────────────────────
    void buildGraph(const DeviceInfo& device, const CaptureMode& mode);
    void teardownGraph();

    // Called by SampleCallback on every captured frame
    friend class SampleCallback;
    void onSample(const uint8_t* data, size_t size,
                  int w, int h, const std::string& subtype, bool isMjpeg);

    // DirectShow COM objects
    IGraphBuilder*          m_graph         = nullptr;
    IMediaControl*          m_mediaControl  = nullptr;
    IBaseFilter*            m_sourceFilter  = nullptr;
    IBaseFilter*            m_grabberFilter = nullptr;
    IBaseFilter*            m_nullRenderer  = nullptr;
    SampleCallback*         m_sampleCb      = nullptr;
#endif

    // ── State ─────────────────────────────────────────────────────────────
    std::atomic<bool>       m_previewRunning{false};
    std::atomic<bool>       m_ndiRunning    {false};

    CaptureMode             m_mode;
    MjpegDecoder            m_mjpegDecoder;

    std::shared_ptr<INdiSender> m_ndiSender;
    std::mutex                  m_ndiMutex;

    // ── Stats ─────────────────────────────────────────────────────────────
    std::atomic<int>        m_droppedCapture{0};
    std::atomic<int>        m_droppedSend   {0};
    double                  m_captureFps    = 0.0;
    double                  m_decodeFps     = 0.0;
    double                  m_sendFps       = 0.0;

    // Rolling window for FPS calculation
    struct FpsCounter {
        static constexpr int WINDOW = 60;
        std::chrono::steady_clock::time_point times[WINDOW] = {};
        int head = 0;
        int count= 0;
        void tick() {
            times[head] = std::chrono::steady_clock::now();
            head = (head + 1) % WINDOW;
            if (count < WINDOW) count++;
        }
        double fps() const {
            if (count < 2) return 0.0;
            int oldest = (head - count + WINDOW) % WINDOW;
            auto dt = std::chrono::duration<double>(times[(head-1+WINDOW)%WINDOW] - times[oldest]).count();
            return dt > 0 ? (count - 1) / dt : 0.0;
        }
    } m_capCounter, m_decCounter, m_sndCounter;
};
