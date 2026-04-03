#pragma once
#include <QMainWindow>
#include <QLabel>
#include <QComboBox>
#include <QLineEdit>
#include <QPushButton>
#include <QCheckBox>
#include <QGroupBox>
#include <QStatusBar>
#include <QTimer>

#include "DeviceEnumerator.h"
#include "CaptureSession.h"
#include "MutexGuard.h"

#include <memory>
#include <vector>

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

protected:
    void closeEvent(QCloseEvent* event) override;

private slots:
    void onDeviceChanged(int index);
    void onShowAllModesToggled(bool checked);
    void onStartNdi();
    void onStop();
    void onPreviewFrame(QImage image);
    void onStatsUpdated(double capFps, double decFps, double sndFps,
                        int dropCapture, int dropSend);
    void onCaptureError(QString message);
    void onRefreshDevices();

private:
    void setupUi();
    void populateModeCombo(bool showAll);
    void updateButtonStates();
    void startPreview();
    void stopEverything();
    void drawOverlay(QImage& img);

    // ── Widgets ──────────────────────────────────────────────────────────
    QComboBox*   m_deviceCombo    = nullptr;
    QPushButton* m_refreshBtn     = nullptr;
    QComboBox*   m_modeCombo      = nullptr;
    QCheckBox*   m_showAllCheck   = nullptr;
    QLineEdit*   m_ndiNameEdit    = nullptr;
    QLabel*      m_previewLabel   = nullptr;
    QPushButton* m_startNdiBtn    = nullptr;
    QPushButton* m_stopBtn        = nullptr;
    QLabel*      m_statusCapFps   = nullptr;
    QLabel*      m_statusDecFps   = nullptr;
    QLabel*      m_statusSndFps   = nullptr;
    QLabel*      m_statusDrop     = nullptr;
    QLabel*      m_statusNdi      = nullptr;
    QLabel*      m_msgLabel       = nullptr;

    // ── Data ─────────────────────────────────────────────────────────────
    std::vector<DeviceInfo>   m_devices;
    std::vector<CaptureMode>  m_allModes;    // All modes for current device
    std::vector<CaptureMode>  m_shownModes;  // Currently displayed subset

    // ── Session ───────────────────────────────────────────────────────────
    std::unique_ptr<CaptureSession> m_session;

    // ── Multi-process mutexes ─────────────────────────────────────────────
    MutexGuard  m_deviceMutex;
    MutexGuard  m_senderMutex;

    bool m_ndiActive = false;
};
