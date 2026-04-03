#include "MainWindow.h"
#include "Logger.h"
#include "NdiSenderInterface.h"

#ifdef HAVE_NDI
#  include "NdiSender.h"
#endif

#include <QApplication>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QSplitter>
#include <QMessageBox>
#include <QPainter>
#include <QFont>
#include <QCloseEvent>
#include <QScreen>

#include <sstream>
#include <iomanip>

static constexpr int PREVIEW_W = 640;
static constexpr int PREVIEW_H = 360;

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
{
    setWindowTitle("NDI Capture");
    setupUi();
    onRefreshDevices();
    updateButtonStates();
}

MainWindow::~MainWindow()
{
    stopEverything();
}

void MainWindow::closeEvent(QCloseEvent* event)
{
    stopEverything();
    event->accept();
}

// ── UI setup ──────────────────────────────────────────────────────────────
void MainWindow::setupUi()
{
    auto* central = new QWidget(this);
    setCentralWidget(central);

    auto* mainLayout = new QVBoxLayout(central);
    mainLayout->setSpacing(8);
    mainLayout->setContentsMargins(10, 10, 10, 10);

    // ── Device row ────────────────────────────────────────────────────────
    {
        auto* grp = new QGroupBox("Video Device", this);
        auto* row = new QHBoxLayout(grp);
        m_deviceCombo = new QComboBox(this);
        m_deviceCombo->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        m_refreshBtn  = new QPushButton("Refresh", this);
        m_refreshBtn->setFixedWidth(70);
        row->addWidget(m_deviceCombo);
        row->addWidget(m_refreshBtn);
        mainLayout->addWidget(grp);
    }

    // ── Mode row ──────────────────────────────────────────────────────────
    {
        auto* grp = new QGroupBox("Capture Mode", this);
        auto* col = new QVBoxLayout(grp);
        m_modeCombo    = new QComboBox(this);
        m_showAllCheck = new QCheckBox("Show all modes", this);
        col->addWidget(m_modeCombo);
        col->addWidget(m_showAllCheck);
        mainLayout->addWidget(grp);
    }

    // ── NDI name row ─────────────────────────────────────────────────────
    {
        auto* grp  = new QGroupBox("NDI Source Name", this);
        auto* form = new QFormLayout(grp);
        m_ndiNameEdit = new QLineEdit(this);
        m_ndiNameEdit->setPlaceholderText("Required – must be unique on this machine");
        form->addRow("Name:", m_ndiNameEdit);
        mainLayout->addWidget(grp);
    }

    // ── Preview ───────────────────────────────────────────────────────────
    {
        auto* grp = new QGroupBox("Preview", this);
        auto* row = new QHBoxLayout(grp);
        m_previewLabel = new QLabel(this);
        m_previewLabel->setFixedSize(PREVIEW_W, PREVIEW_H);
        m_previewLabel->setAlignment(Qt::AlignCenter);
        m_previewLabel->setStyleSheet("background: #111; border: 1px solid #555;");
        m_previewLabel->setText("No signal");
        row->addWidget(m_previewLabel, 0, Qt::AlignCenter);
        mainLayout->addWidget(grp);
    }

    // ── Controls ──────────────────────────────────────────────────────────
    {
        auto* grp = new QGroupBox("Controls", this);
        auto* col = new QVBoxLayout(grp);

        // Buttons
        auto* btnRow = new QHBoxLayout;
        m_startNdiBtn = new QPushButton("▶  Start NDI", this);
        m_stopBtn     = new QPushButton("■  Stop",      this);
        m_startNdiBtn->setMinimumHeight(36);
        m_stopBtn    ->setMinimumHeight(36);
        btnRow->addWidget(m_startNdiBtn);
        btnRow->addWidget(m_stopBtn);
        col->addLayout(btnRow);

        // Status grid
        auto* statusRow = new QFormLayout;
        m_statusCapFps = new QLabel("—", this);
        m_statusDecFps = new QLabel("—", this);
        m_statusSndFps = new QLabel("—", this);
        m_statusDrop   = new QLabel("—", this);
        m_statusNdi    = new QLabel("Not running", this);
        statusRow->addRow("Capture FPS:",  m_statusCapFps);
        statusRow->addRow("Decode FPS:",   m_statusDecFps);
        statusRow->addRow("Send FPS:",     m_statusSndFps);
        statusRow->addRow("Dropped:",      m_statusDrop);
        statusRow->addRow("NDI sender:",   m_statusNdi);
        col->addLayout(statusRow);

        // Error/message label
        m_msgLabel = new QLabel(this);
        m_msgLabel->setWordWrap(true);
        m_msgLabel->setStyleSheet("color: #c00;");
        col->addWidget(m_msgLabel);

        mainLayout->addWidget(grp);
    }

    // ── Connections ───────────────────────────────────────────────────────
    connect(m_deviceCombo,  QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MainWindow::onDeviceChanged);
    connect(m_modeCombo,    QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int) { startPreview(); updateButtonStates(); });
    connect(m_showAllCheck, &QCheckBox::toggled,
            this, &MainWindow::onShowAllModesToggled);
    connect(m_startNdiBtn,  &QPushButton::clicked, this, &MainWindow::onStartNdi);
    connect(m_stopBtn,      &QPushButton::clicked, this, &MainWindow::onStop);
    connect(m_refreshBtn,   &QPushButton::clicked, this, &MainWindow::onRefreshDevices);

    // Window sizing – just enough to fit everything
    adjustSize();
    setMinimumWidth(700);
}

// ── Device / Mode population ──────────────────────────────────────────────
void MainWindow::onRefreshDevices()
{
    stopEverything();
    m_devices = DeviceEnumerator::enumerateDevices();

    m_deviceCombo->blockSignals(true);
    m_deviceCombo->clear();
    m_deviceCombo->addItem("— Select device —");
    for (const auto& d : m_devices) {
        QString label = QString::fromStdString(d.friendlyName)
                      + "  [" + QString::fromStdString(d.shortId) + "]";
        m_deviceCombo->addItem(label);
    }
    m_deviceCombo->blockSignals(false);

    m_modeCombo->clear();
    m_allModes.clear();
    m_shownModes.clear();
    updateButtonStates();
}

void MainWindow::onDeviceChanged(int index)
{
    stopEverything();

    if (index <= 0 || index - 1 >= (int)m_devices.size()) {
        m_modeCombo->clear();
        m_allModes.clear();
        m_shownModes.clear();
        updateButtonStates();
        return;
    }

    const auto& dev = m_devices[index - 1];
    m_allModes = DeviceEnumerator::enumerateModes(dev.devicePath);
    populateModeCombo(m_showAllCheck->isChecked());

    // Auto-start preview once a device is selected
    startPreview();
    updateButtonStates();
}

void MainWindow::onShowAllModesToggled(bool checked)
{
    populateModeCombo(checked);
}

void MainWindow::populateModeCombo(bool showAll)
{
    m_modeCombo->blockSignals(true);
    m_modeCombo->clear();
    m_shownModes.clear();

    m_modeCombo->addItem("— Select mode —");
    for (const auto& m : m_allModes) {
        if (showAll || m.isCommon) {
            m_shownModes.push_back(m);
            m_modeCombo->addItem(QString::fromStdString(m.displayName()));
        }
    }

    // Inform user if common-mode list is empty
    if (!showAll && m_shownModes.empty() && !m_allModes.empty()) {
        m_msgLabel->setText("No common modes found – check 'Show all modes'");
    } else {
        m_msgLabel->clear();
    }

    m_modeCombo->blockSignals(false);
    updateButtonStates();
}

// ── Preview start/stop ────────────────────────────────────────────────────
void MainWindow::startPreview()
{
    int devIdx = m_deviceCombo->currentIndex() - 1;
    int modeIdx = m_modeCombo->currentIndex() - 1;

    if (devIdx < 0 || devIdx >= (int)m_devices.size()) return;
    if (modeIdx < 0 || modeIdx >= (int)m_shownModes.size()) {
        // No mode selected yet but device is known: show "waiting for mode" message
        return;
    }

    const auto& dev  = m_devices[devIdx];
    const auto& mode = m_shownModes[modeIdx];

    if (!m_session) {
        m_session = std::make_unique<CaptureSession>(this);
        connect(m_session.get(), &CaptureSession::previewFrame,
                this, &MainWindow::onPreviewFrame, Qt::QueuedConnection);
        connect(m_session.get(), &CaptureSession::statsUpdated,
                this, &MainWindow::onStatsUpdated, Qt::QueuedConnection);
        connect(m_session.get(), &CaptureSession::error,
                this, &MainWindow::onCaptureError, Qt::QueuedConnection);
    }

    m_previewLabel->setText("Opening device…");
    bool ok = m_session->startPreview(dev, mode);
    if (!ok) {
        m_msgLabel->setText("Preview unavailable (device may be in use by another app)");
    }
}

void MainWindow::stopEverything()
{
    m_ndiActive = false;
    m_deviceMutex.release();
    m_senderMutex.release();

    if (m_session) {
        m_session->stopNdi();
        m_session->stopPreview();
        m_session.reset();
    }

    m_previewLabel->setText("No signal");
    m_statusNdi->setText("Not running");
    updateButtonStates();
}

// ── NDI Start/Stop ────────────────────────────────────────────────────────
void MainWindow::onStartNdi()
{
    const QString ndiName = m_ndiNameEdit->text().trimmed();
    if (ndiName.isEmpty()) {
        QMessageBox::warning(this, "NDI Name Required",
            "Please enter a unique NDI source name before starting.");
        return;
    }

    int devIdx  = m_deviceCombo->currentIndex() - 1;
    int modeIdx = m_modeCombo->currentIndex() - 1;
    if (devIdx < 0 || devIdx >= (int)m_devices.size()) {
        QMessageBox::warning(this, "No Device", "Please select a video device.");
        return;
    }
    if (modeIdx < 0 || modeIdx >= (int)m_shownModes.size()) {
        QMessageBox::warning(this, "No Mode", "Please select a capture mode.");
        return;
    }

    const auto& dev  = m_devices[devIdx];
    const auto& mode = m_shownModes[modeIdx];

    // ── Acquire device mutex ──────────────────────────────────────────────
    const std::string devMutex = MutexGuard::deviceMutexName(dev.devicePath);
    if (!m_deviceMutex.acquire(devMutex)) {
        QMessageBox::critical(this, "Device Busy",
            "Cannot start NDI: this device is already in use by another instance.\n\n"
            "Close the other instance that is streaming this device, then try again.");
        return;
    }

    // ── Acquire sender-name mutex ─────────────────────────────────────────
    const std::string sndMutex = MutexGuard::senderMutexName(ndiName.toStdString());
    if (!m_senderMutex.acquire(sndMutex)) {
        m_deviceMutex.release();
        QMessageBox::critical(this, "NDI Name In Use",
            "Cannot start NDI: the name \"" + ndiName + "\" is already in use "
            "by another instance on this machine.\n\n"
            "Choose a different NDI source name and try again.");
        return;
    }

    // Ensure preview is running
    if (!m_session || !m_session->isPreviewRunning()) {
        startPreview();
        if (!m_session || !m_session->isPreviewRunning()) {
            m_deviceMutex.release();
            m_senderMutex.release();
            QMessageBox::critical(this, "Preview Failed",
                "Could not open the capture device. It may be in use by another application.");
            return;
        }
    }

    // Build NDI sender
    std::shared_ptr<INdiSender> sender;
#ifdef HAVE_NDI
    auto real = std::make_shared<NdiSender>();
    if (!real->start(ndiName.toStdString(), mode.fps)) {
        m_deviceMutex.release();
        m_senderMutex.release();
        QMessageBox::critical(this, "NDI Error",
            "Failed to create NDI sender. Check NDI Runtime is installed.");
        return;
    }
    sender = real;
#else
    auto null = std::make_shared<NullNdiSender>();
    null->start(ndiName.toStdString(), mode.fps);
    sender = null;
    m_msgLabel->setText("⚠  Built without NDI SDK – preview only");
#endif

    m_session->startNdi(sender);
    m_ndiActive = true;
    m_statusNdi->setText("Running: " + ndiName);
    LOG_INFO("NDI streaming started: " + ndiName.toStdString());
    updateButtonStates();
}

void MainWindow::onStop()
{
    stopEverything();
    m_msgLabel->clear();
}

// ── Frame / Stats callbacks ───────────────────────────────────────────────
void MainWindow::onPreviewFrame(QImage image)
{
    // Draw overlay text on the preview image
    drawOverlay(image);
    m_previewLabel->setPixmap(QPixmap::fromImage(image));
}

void MainWindow::drawOverlay(QImage& img)
{
    QPainter p(&img);
    p.setFont(QFont("Arial", 9));
    p.setPen(Qt::yellow);

    int devIdx  = m_deviceCombo->currentIndex() - 1;
    int modeIdx = m_modeCombo->currentIndex() - 1;

    QString overlay;
    if (devIdx >= 0 && devIdx < (int)m_devices.size())
        overlay += QString("[%1] ").arg(QString::fromStdString(m_devices[devIdx].shortId));
    if (modeIdx >= 0 && modeIdx < (int)m_shownModes.size())
        overlay += QString::fromStdString(m_shownModes[modeIdx].displayName());
    if (m_session)
        overlay += QString("  Cap: %1 fps").arg(m_session->captureFps(), 0, 'f', 1);

    p.drawText(4, 14, overlay);
    p.end();
}

void MainWindow::onStatsUpdated(double capFps, double decFps, double sndFps,
                                 int dropCapture, int dropSend)
{
    m_statusCapFps->setText(QString("%1 fps").arg(capFps, 0, 'f', 1));
    if (decFps > 0)
        m_statusDecFps->setText(QString("%1 fps").arg(decFps, 0, 'f', 1));
    else
        m_statusDecFps->setText("—  (not MJPEG)");
    m_statusSndFps->setText(sndFps > 0 ? QString("%1 fps").arg(sndFps, 0, 'f', 1) : "—");
    m_statusDrop->setText(QString("cap %1  /  send %2")
                              .arg(dropCapture).arg(dropSend));
}

void MainWindow::onCaptureError(QString message)
{
    m_msgLabel->setText("Error: " + message);
    LOG_ERROR("Capture error: " + message.toStdString());
}

// ── Button state management ───────────────────────────────────────────────
void MainWindow::updateButtonStates()
{
    bool deviceSelected = m_deviceCombo->currentIndex() > 0;
    bool modeSelected   = m_modeCombo->currentIndex() > 0;
    bool ready = deviceSelected && modeSelected;

    m_startNdiBtn->setEnabled(ready && !m_ndiActive);
    m_stopBtn    ->setEnabled(m_ndiActive ||
                              (m_session && m_session->isPreviewRunning()));
}
