#include "CaptureSession.h"
#include "Logger.h"

#ifdef _WIN32
#  define WIN32_LEAN_AND_MEAN
#  include <windows.h>
#  include <dshow.h>
#  pragma comment(lib, "strmiids.lib")
#  pragma comment(lib, "ole32.lib")
#  pragma comment(lib, "oleaut32.lib")
// NV12, IYUV, and MJPG are declared in <uuids.h> (via <dshow.h>) and defined in
// strmiids.lib.  Do NOT re-define them here; doing so causes LNK2005 with modern SDKs.
#  include "dshow_guids.h"

// ── ISampleGrabber callback ───────────────────────────────────────────────
// qedit.h is not present in modern Windows SDKs; define the interfaces manually.
#ifndef __ISampleGrabberCB_INTERFACE_DEFINED__
// {0579154A-2B53-4994-B0D0-E773148EFF85}
static const IID IID_ISampleGrabberCB =
{ 0x0579154a, 0x2b53, 0x4994, { 0xb0, 0xd0, 0xe7, 0x73, 0x14, 0x8e, 0xff, 0x85 } };

MIDL_INTERFACE("0579154A-2B53-4994-B0D0-E773148EFF85")
ISampleGrabberCB : public IUnknown {
public:
    virtual HRESULT STDMETHODCALLTYPE SampleCB(double SampleTime, IMediaSample* pSample) = 0;
    virtual HRESULT STDMETHODCALLTYPE BufferCB(double SampleTime, BYTE* pBuffer, long BufferLen) = 0;
};

// {6B652FFF-11FE-4fce-92AD-0266B5D7C78F}
static const IID IID_ISampleGrabber =
{ 0x6b652fff, 0x11fe, 0x4fce, { 0x92, 0xad, 0x02, 0x66, 0xb5, 0xd7, 0xc7, 0x8f } };

MIDL_INTERFACE("6B652FFF-11FE-4fce-92AD-0266B5D7C78F")
ISampleGrabber : public IUnknown {
public:
    virtual HRESULT STDMETHODCALLTYPE SetOneShot(BOOL OneShot) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetMediaType(const AM_MEDIA_TYPE* pType) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetConnectedMediaType(AM_MEDIA_TYPE* pType) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetBufferSamples(BOOL BufferThem) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetCurrentBuffer(long* pBufferSize, long* pBuffer) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetCurrentSample(IMediaSample** ppSample) = 0;
    virtual HRESULT STDMETHODCALLTYPE SetCallback(ISampleGrabberCB* pCallback, long WhichMethodToCallback) = 0;
};

// CLSID_SampleGrabber {C1F400A0-3F08-11D3-9F0B-006008039E37}
static const CLSID CLSID_SampleGrabber =
{ 0xC1F400A0, 0x3F08, 0x11D3, { 0x9F, 0x0B, 0x00, 0x60, 0x08, 0x03, 0x9E, 0x37 } };

// CLSID_NullRenderer {C1F400A4-3F08-11D3-9F0B-006008039E37}
static const CLSID CLSID_NullRenderer =
{ 0xC1F400A4, 0x3F08, 0x11D3, { 0x9F, 0x0B, 0x00, 0x60, 0x08, 0x03, 0x9E, 0x37 } };

#define __ISampleGrabberCB_INTERFACE_DEFINED__
#endif // __ISampleGrabberCB_INTERFACE_DEFINED__

// ── SampleCallback ─────────────────────────────────────────────────────────
class SampleCallback : public ISampleGrabberCB
{
public:
    explicit SampleCallback(CaptureSession* session)
        : m_session(session) {}

    // IUnknown
    STDMETHODIMP QueryInterface(REFIID riid, void** ppv) override {
        if (riid == IID_IUnknown || riid == IID_ISampleGrabberCB) {
            *ppv = static_cast<ISampleGrabberCB*>(this);
            AddRef();
            return S_OK;
        }
        return E_NOINTERFACE;
    }
    STDMETHODIMP_(ULONG) AddRef()  override { return ++m_refCount; }
    STDMETHODIMP_(ULONG) Release() override {
        ULONG c = --m_refCount;
        if (c == 0) delete this;
        return c;
    }

    // We use BufferCB (method 1) to get a direct pointer to the raw data
    STDMETHODIMP SampleCB(double, IMediaSample*) override { return S_OK; }

    STDMETHODIMP BufferCB(double /*sampleTime*/, BYTE* pBuffer, long bufLen) override {
        if (m_session && pBuffer && bufLen > 0) {
            m_session->onSample(
                reinterpret_cast<const uint8_t*>(pBuffer),
                static_cast<size_t>(bufLen),
                m_session->m_mode.width,
                m_session->m_mode.height,
                m_session->m_mode.subtype,
                m_session->m_mode.isMjpeg
            );
        }
        return S_OK;
    }

    void setMode(int w, int h, const std::string& subtype, bool isMjpeg) {
        m_w = w; m_h = h; m_subtype = subtype; m_isMjpeg = isMjpeg;
    }

private:
    CaptureSession*   m_session  = nullptr;
    std::atomic<ULONG> m_refCount{1};
    int               m_w = 0, m_h = 0;
    std::string       m_subtype;
    bool              m_isMjpeg = false;
};

#endif // _WIN32

// ── CaptureSession ────────────────────────────────────────────────────────
CaptureSession::CaptureSession(QObject* parent)
    : QObject(parent)
{
#ifdef _WIN32
    CoInitializeEx(nullptr, COINIT_MULTITHREADED);
#endif
}

CaptureSession::~CaptureSession()
{
    stopNdi();
    stopPreview();
}

// ── Preview ────────────────────────────────────────────────────────────────
bool CaptureSession::startPreview(const DeviceInfo& device, const CaptureMode& mode)
{
#ifdef _WIN32
    stopPreview();
    m_mode = mode;
    try {
        buildGraph(device, mode);
    } catch (const std::exception& e) {
        LOG_ERROR(std::string("startPreview: ") + e.what());
        emit error(QString::fromStdString(e.what()));
        return false;
    }
    m_previewRunning = true;
    LOG_INFO("Preview started: " + device.friendlyName + " " + mode.displayName());
    return true;
#else
    (void)device; (void)mode;
    LOG_WARN("CaptureSession: DirectShow not available on this platform");
    return false;
#endif
}

void CaptureSession::stopPreview()
{
#ifdef _WIN32
    if (!m_previewRunning) return;
    stopNdi();
    teardownGraph();
    m_previewRunning = false;
    LOG_INFO("Preview stopped");
#endif
}

// ── NDI ────────────────────────────────────────────────────────────────────
bool CaptureSession::startNdi(std::shared_ptr<INdiSender> sender)
{
    std::lock_guard<std::mutex> lk(m_ndiMutex);
    m_ndiSender  = std::move(sender);
    m_ndiRunning = true;
    LOG_INFO("NDI dispatch enabled");
    return true;
}

void CaptureSession::stopNdi()
{
    std::lock_guard<std::mutex> lk(m_ndiMutex);
    if (m_ndiSender) {
        m_ndiSender->stop();
        m_ndiSender.reset();
    }
    m_ndiRunning = false;
}

// ── Frame callback (called from DS graph thread) ───────────────────────────
void CaptureSession::onSample(const uint8_t* data, size_t size,
                               int w, int h,
                               const std::string& subtype, bool isMjpeg)
{
    m_capCounter.tick();
    m_captureFps = m_capCounter.fps();

    std::vector<uint8_t> bgra;
    int frameW = w, frameH = h;

    if (isMjpeg) {
        if (!m_mjpegDecoder.decode(data, size, bgra, frameW, frameH)) {
            m_droppedCapture++;
            return;
        }
        m_decCounter.tick();
        m_decodeFps = m_decCounter.fps();
    } else {
        if (!FrameConverter::toBGRA(data, size, subtype, w, h, bgra)) {
            m_droppedCapture++;
            return;
        }
    }

    // ── Preview (always, scale to 480×270) ────────────────────────────────
    constexpr int PW = 480, PH = 270;
    std::vector<uint8_t> previewBuf(PW * PH * 4);
    FrameConverter::scaleBGRA(bgra.data(), frameW, frameH,
                               previewBuf.data(), PW, PH);
    QImage img(previewBuf.data(), PW, PH, PW * 4, QImage::Format_ARGB32);
    emit previewFrame(img.copy()); // .copy() to detach from raw buffer

    // ── NDI ───────────────────────────────────────────────────────────────
    if (m_ndiRunning) {
        std::lock_guard<std::mutex> lk(m_ndiMutex);
        if (m_ndiSender && m_ndiSender->isRunning()) {
            // Convert BGRA → UYVY for NDI
            std::vector<uint8_t> uyvy;
            FrameConverter::bgraToUYVY(bgra.data(), frameW, frameH, uyvy);

            NdiFrame ndiFrame;
            ndiFrame.width      = frameW;
            ndiFrame.height     = frameH;
            ndiFrame.data       = uyvy.data();
            ndiFrame.lineStride = frameW * 2;
            ndiFrame.fps        = m_mode.fps;
            ndiFrame.isUYVY     = true;

            if (!m_ndiSender->sendFrame(ndiFrame)) {
                m_droppedSend++;
            } else {
                m_sndCounter.tick();
                m_sendFps = m_sndCounter.fps();
            }
        }
    }

    // Emit stats periodically (every ~30 frames)
    static int statTick = 0;
    if (++statTick >= 30) {
        statTick = 0;
        emit statsUpdated(m_captureFps, m_decodeFps, m_sendFps,
                          m_droppedCapture.load(), m_droppedSend.load());
    }
}

// ── DirectShow graph build/teardown (Windows only) ───────────────────────
#ifdef _WIN32

template<class T>
static void safeRelease(T*& p) { if (p) { p->Release(); p = nullptr; } }

void CaptureSession::buildGraph(const DeviceInfo& device, const CaptureMode& mode)
{
    HRESULT hr;

    // Create filter graph
    hr = CoCreateInstance(CLSID_FilterGraph, nullptr, CLSCTX_INPROC_SERVER,
                          IID_IGraphBuilder, (void**)&m_graph);
    if (FAILED(hr)) throw std::runtime_error("Failed to create filter graph");

    hr = m_graph->QueryInterface(IID_IMediaControl, (void**)&m_mediaControl);
    if (FAILED(hr)) throw std::runtime_error("Failed to get IMediaControl");

    // ── Source filter ──────────────────────────────────────────────────────
    // Re-bind moniker for this device path
    {
        ICreateDevEnum* devEnum = nullptr;
        CoCreateInstance(CLSID_SystemDeviceEnum, nullptr, CLSCTX_INPROC_SERVER,
                         IID_ICreateDevEnum, (void**)&devEnum);
        IEnumMoniker* enumMon = nullptr;
        devEnum->CreateClassEnumerator(CLSID_VideoInputDeviceCategory, &enumMon, 0);
        devEnum->Release();

        IMoniker* mon = nullptr;
        while (enumMon->Next(1, &mon, nullptr) == S_OK) {
            IPropertyBag* pb = nullptr;
            mon->BindToStorage(nullptr, nullptr, IID_IPropertyBag, (void**)&pb);
            bool match = false;
            if (pb) {
                VARIANT v = {}; v.vt = VT_BSTR;
                if (SUCCEEDED(pb->Read(L"DevicePath", &v, nullptr))) {
                    int len = WideCharToMultiByte(CP_UTF8, 0, v.bstrVal, -1, nullptr, 0, nullptr, nullptr);
                    if (len > 0) {
                        std::string dp(len-1, '\0');
                        WideCharToMultiByte(CP_UTF8, 0, v.bstrVal, -1, &dp[0], len, nullptr, nullptr);
                        match = (dp == device.devicePath);
                    }
                    VariantClear(&v);
                }
                pb->Release();
            }
            if (!match) {
                // Also check display name
                LPOLESTR dn = nullptr;
                if (SUCCEEDED(mon->GetDisplayName(nullptr, nullptr, &dn)) && dn) {
                    int len = WideCharToMultiByte(CP_UTF8, 0, dn, -1, nullptr, 0, nullptr, nullptr);
                    if (len > 0) {
                        std::string s(len-1, '\0');
                        WideCharToMultiByte(CP_UTF8, 0, dn, -1, &s[0], len, nullptr, nullptr);
                        match = (s == device.devicePath);
                    }
                    CoTaskMemFree(dn);
                }
            }
            if (match) {
                mon->BindToObject(nullptr, nullptr, IID_IBaseFilter, (void**)&m_sourceFilter);
                mon->Release();
                break;
            }
            mon->Release();
        }
        if (enumMon) enumMon->Release();
    }

    if (!m_sourceFilter) throw std::runtime_error("Device not found: " + device.devicePath);
    m_graph->AddFilter(m_sourceFilter, L"Source");

    // ── Set capture mode ───────────────────────────────────────────────────
    {
        IEnumPins* enumPins = nullptr;
        m_sourceFilter->EnumPins(&enumPins);
        IPin* pin = nullptr;
        IAMStreamConfig* sc = nullptr;
        while (enumPins->Next(1, &pin, nullptr) == S_OK) {
            PIN_DIRECTION dir;
            if (SUCCEEDED(pin->QueryDirection(&dir)) && dir == PINDIR_OUTPUT) {
                if (SUCCEEDED(pin->QueryInterface(IID_IAMStreamConfig, (void**)&sc)))
                    break;
            }
            pin->Release(); pin = nullptr;
        }
        enumPins->Release();

        if (sc && pin) {
            int count = 0, size = 0;
            sc->GetNumberOfCapabilities(&count, &size);
            for (int i = 0; i < count; ++i) {
                VIDEO_STREAM_CONFIG_CAPS caps = {};
                AM_MEDIA_TYPE* pmt = nullptr;
                if (FAILED(sc->GetStreamCaps(i, &pmt, (BYTE*)&caps))) continue;

                bool isVih  = (pmt->formattype == FORMAT_VideoInfo  && pmt->pbFormat);
                bool isVih2 = (pmt->formattype == FORMAT_VideoInfo2 && pmt->pbFormat);

                if ((isVih || isVih2) && pmt->pbFormat) {
                    int mw = 0, mh = 0;
                    double mfps = 0.0;
                    if (isVih2) {
                        auto* vi2 = reinterpret_cast<VIDEOINFOHEADER2*>(pmt->pbFormat);
                        mw   = vi2->bmiHeader.biWidth;
                        mh   = std::abs(vi2->bmiHeader.biHeight);
                        mfps = vi2->AvgTimePerFrame > 0 ? 10000000.0 / vi2->AvgTimePerFrame : 0;
                    } else {
                        auto* vi = reinterpret_cast<VIDEOINFOHEADER*>(pmt->pbFormat);
                        mw   = vi->bmiHeader.biWidth;
                        mh   = std::abs(vi->bmiHeader.biHeight);
                        mfps = vi->AvgTimePerFrame > 0 ? 10000000.0 / vi->AvgTimePerFrame : 0;
                    }

                    bool fpsMatch = std::abs(mfps - mode.fps) < 1.0 ||
                                    (mode.fps > 59 && mfps > 59);
                    bool dimMatch = (mw == mode.width && mh == mode.height);
                    // Also require the format type (VIH vs VIH2) to match
                    bool vihMatch = (isVih2 == mode.isVih2);
                    bool subMatch = false;
                    if      (pmt->subtype == MEDIASUBTYPE_YUY2 && mode.subtype == "YUY2")  subMatch = true;
                    else if (pmt->subtype == MEDIASUBTYPE_NV12 && mode.subtype == "NV12")  subMatch = true;
                    else if (pmt->subtype == MEDIASUBTYPE_MJPG && mode.subtype == "MJPG")  subMatch = true;
                    else if (pmt->subtype == MEDIASUBTYPE_IYUV && mode.subtype == "I420")  subMatch = true;
                    else if (pmt->subtype == MEDIASUBTYPE_RGB24&& mode.subtype == "RGB24") subMatch = true;
                    else if (pmt->subtype == MEDIASUBTYPE_RGB32&& mode.subtype == "RGB32") subMatch = true;
                    else if (pmt->subtype == MEDIASUBTYPE_UYVY && mode.subtype == "UYVY")  subMatch = true;
                    else {
                        // HDYC (same layout as UYVY)
                        if (pmt->subtype == MEDIASUBTYPE_HDYC && mode.subtype == "UYVY") subMatch = true;
                        // Fallback: unrecognized subtype – skip
                    }

                    if (dimMatch && fpsMatch && vihMatch && subMatch) {
                        sc->SetFormat(pmt);
                        if (pmt->cbFormat && pmt->pbFormat) CoTaskMemFree(pmt->pbFormat);
                        if (pmt->pUnk) pmt->pUnk->Release();
                        CoTaskMemFree(pmt);
                        break;
                    }
                }
                if (pmt->cbFormat && pmt->pbFormat) CoTaskMemFree(pmt->pbFormat);
                if (pmt->pUnk) pmt->pUnk->Release();
                CoTaskMemFree(pmt);
            }
        }
        if (sc)  sc ->Release();
        if (pin) pin->Release();
    }

    // ── SampleGrabber ─────────────────────────────────────────────────────
    hr = CoCreateInstance(CLSID_SampleGrabber, nullptr, CLSCTX_INPROC_SERVER,
                          IID_IBaseFilter, (void**)&m_grabberFilter);
    if (FAILED(hr)) throw std::runtime_error("Failed to create SampleGrabber");
    m_graph->AddFilter(m_grabberFilter, L"SampleGrabber");

    ISampleGrabber* sg = nullptr;
    m_grabberFilter->QueryInterface(IID_ISampleGrabber, (void**)&sg);
    if (sg) {
        sg->SetOneShot(FALSE);
        sg->SetBufferSamples(FALSE);

        // Allow any video media type
        AM_MEDIA_TYPE mt = {};
        mt.majortype = MEDIATYPE_Video;
        sg->SetMediaType(&mt);

        m_sampleCb = new SampleCallback(this);
        sg->SetCallback(m_sampleCb, 1); // 1 = BufferCB
        sg->Release();
    }

    // ── Null renderer ─────────────────────────────────────────────────────
    hr = CoCreateInstance(CLSID_NullRenderer, nullptr, CLSCTX_INPROC_SERVER,
                          IID_IBaseFilter, (void**)&m_nullRenderer);
    if (FAILED(hr)) throw std::runtime_error("Failed to create NullRenderer");
    m_graph->AddFilter(m_nullRenderer, L"NullRenderer");

    // ── Connect: Source → SampleGrabber → NullRenderer ───────────────────
    {
        // Source output pin
        auto getPin = [](IBaseFilter* f, PIN_DIRECTION dir, int idx) -> IPin* {
            IEnumPins* ep = nullptr; f->EnumPins(&ep);
            if (!ep) return nullptr;
            IPin* p = nullptr; int n = 0;
            while (ep->Next(1, &p, nullptr) == S_OK) {
                PIN_DIRECTION d; p->QueryDirection(&d);
                if (d == dir) { if (n++ == idx) { ep->Release(); return p; } }
                p->Release(); p = nullptr;
            }
            ep->Release(); return nullptr;
        };

        IPin* srcOut  = getPin(m_sourceFilter,  PINDIR_OUTPUT, 0);
        IPin* sgIn    = getPin(m_grabberFilter,  PINDIR_INPUT,  0);
        IPin* sgOut   = getPin(m_grabberFilter,  PINDIR_OUTPUT, 0);
        IPin* nullIn  = getPin(m_nullRenderer,   PINDIR_INPUT,  0);

        if (srcOut && sgIn)   hr = m_graph->Connect(srcOut, sgIn);
        else                  hr = E_FAIL;
        if (FAILED(hr)) LOG_WARN("Failed to connect Source -> SampleGrabber: " + std::to_string(hr));

        if (sgOut && nullIn)  hr = m_graph->Connect(sgOut, nullIn);
        else                  hr = E_FAIL;
        if (FAILED(hr)) LOG_WARN("Failed to connect SampleGrabber -> NullRenderer: " + std::to_string(hr));

        if (srcOut)  srcOut ->Release();
        if (sgIn)    sgIn   ->Release();
        if (sgOut)   sgOut  ->Release();
        if (nullIn)  nullIn ->Release();
    }

    // Run the graph
    hr = m_mediaControl->Run();
    if (FAILED(hr)) {
        LOG_WARN("IMediaControl::Run() returned non-success: " + std::to_string(hr));
    }
}

void CaptureSession::teardownGraph()
{
    if (m_mediaControl) {
        m_mediaControl->Stop();
        safeRelease(m_mediaControl);
    }
    safeRelease(m_nullRenderer);
    safeRelease(m_grabberFilter);
    safeRelease(m_sourceFilter);
    safeRelease(m_graph);
    // m_sampleCb is COM-ref-counted and will be released by the grabber
    m_sampleCb = nullptr;
}

#endif // _WIN32
