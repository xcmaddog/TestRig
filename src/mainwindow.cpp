#include "mainwindow.h"

#include "galilcontroller.h"
#include "printer.h"
#include "mjdriver.h"
#include "printheadwidget.h"
#include "cameracontroller.h"
#include "cameraworker.h"
#include "camerawidget.h"
#include "liveviewwidget.h"
#include "strobewidget.h"
#include "arduinocontroller.h"
#include "arduinowidget.h"

#include <QTabWidget>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>
#include <QDoubleSpinBox>
#include <QSpinBox>
#include <QGroupBox>
#include <QFormLayout>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QSplitter>
#include <QScrollArea>
#include <QStatusBar>
#include <QMessageBox>
#include <QFont>
#include <QFrame>
#include <QStandardPaths>

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_galil(new GalilController(this))
    , m_mjController(new Added_Scientific::Controller("", this))
    , m_camera(new CameraController())
    , m_cameraWorker(new CameraWorker(m_camera))
    , m_cameraThread(new QThread(this))
    , m_arduino(new ArduinoController(this))
{
    setWindowTitle("CREATE Lab Inkjet Test Rig");
    resize(1400, 800);

    // ── Central widget ────────────────────────────────────────────────────
    auto *central = new QWidget(this);
    setCentralWidget(central);
    auto *mainLayout = new QVBoxLayout(central);
    mainLayout->setSpacing(4);
    mainLayout->setContentsMargins(6, 6, 6, 6);

    buildGalilBar();
    mainLayout->addWidget(findChild<QGroupBox*>("galilBar"));

    auto *tabs = new QTabWidget(this);

    auto *axesTab      = new QWidget(this);
    auto *printheadTab = new QWidget(this);
    auto *cameraTab    = new QWidget(this);

    buildAxesTab(axesTab);
    buildPrintheadTab(printheadTab);
    buildCameraTab(cameraTab);

    tabs->addTab(axesTab,      "Axes");
    tabs->addTab(printheadTab, "Printhead");
    tabs->addTab(cameraTab,    "Camera / Strobe");

    mainLayout->addWidget(tabs, 1);

    // ── Galil controller signals ──────────────────────────────────────────
    connect(m_galil, &GalilController::connected,
            this, &MainWindow::onGalilConnected);
    connect(m_galil, &GalilController::disconnected,
            this, &MainWindow::onGalilDisconnected);
    connect(m_galil, &GalilController::statusUpdated,
            this, &MainWindow::onGalilStatusUpdated);
    connect(m_galil, &GalilController::errorOccurred,
            this, &MainWindow::onGalilError);
    connect(m_galil, &GalilController::axisIdle,
            this, &MainWindow::onAxisIdle);

    // ── Camera worker thread ──────────────────────────────────────────────
    m_cameraWorker->moveToThread(m_cameraThread);
    connect(m_cameraThread, &QThread::finished,
            m_cameraWorker, &QObject::deleteLater);
    m_cameraThread->start();

    // Initial UI state (no Galil connection yet)
    setAxesTabEnabled(false);
    statusBar()->showMessage("Not connected");
}

MainWindow::~MainWindow()
{
    // Stop camera acquisition cleanly before destroying the thread
    QMetaObject::invokeMethod(m_cameraWorker, "stopAcquisition", Qt::BlockingQueuedConnection);
    QMetaObject::invokeMethod(m_cameraWorker, "closeCamera",     Qt::BlockingQueuedConnection);
    m_cameraThread->quit();
    m_cameraThread->wait();
    delete m_camera;
}

// ---------------------------------------------------------------------------
// Galil connection bar
// ---------------------------------------------------------------------------

void MainWindow::buildGalilBar()
{
    auto *grp = new QGroupBox("Galil DMC-4080 Connection", this);
    grp->setObjectName("galilBar");
    auto *hbox = new QHBoxLayout(grp);
    hbox->setContentsMargins(6, 4, 6, 4);

    m_galilConnDot = new QLabel(this);
    m_galilConnDot->setFixedSize(14, 14);
    m_galilConnDot->setStyleSheet("background-color: #555; border-radius: 7px;");

    m_galilAddressEdit = new QLineEdit("192.168.42.100", this);
    m_galilAddressEdit->setFixedWidth(200);
    m_galilAddressEdit->setToolTip(
        "gclib connection string.  Format: <IP> <flags>\n"
        "  -d   direct communication (recommended)\n"
        "Example: 192.168.42.100 -d");

    m_galilConnectBtn = new QPushButton("Connect", this);
    m_galilConnectBtn->setFixedWidth(90);

    m_galilStatusLabel = new QLabel("Not connected", this);
    m_galilStatusLabel->setStyleSheet("color: gray;");

    hbox->addWidget(m_galilConnDot);
    hbox->addWidget(new QLabel("Address:", this));
    hbox->addWidget(m_galilAddressEdit);
    hbox->addWidget(m_galilConnectBtn);
    hbox->addWidget(m_galilStatusLabel);
    hbox->addStretch();

    connect(m_galilConnectBtn, &QPushButton::clicked,
            this, &MainWindow::onConnectClicked);
}

void MainWindow::onConnectClicked()
{
    if (m_galil->isConnected())
    {
        m_galil->disconnect();
    }
    else
    {
        const QString addr = m_galilAddressEdit->text().trimmed();
        if (!m_galil->connect(addr))
            statusBar()->showMessage("Connection failed: " + m_galil->lastError());
    }
}

void MainWindow::onGalilConnected()
{
    m_galilConnDot->setStyleSheet("background-color: #22cc44; border-radius: 7px;");
    m_galilStatusLabel->setText("Connected");
    m_galilStatusLabel->setStyleSheet("color: #22cc44;");
    m_galilConnectBtn->setText("Disconnect");
    m_galilAddressEdit->setEnabled(false);

    setAxesTabEnabled(true);
    if (m_printheadWidget) m_printheadWidget->onGalilConnected();

    statusBar()->showMessage("Galil connected — " + m_galilAddressEdit->text());
}

void MainWindow::onGalilDisconnected()
{
    m_galilConnDot->setStyleSheet("background-color: #555; border-radius: 7px;");
    m_galilStatusLabel->setText("Not connected");
    m_galilStatusLabel->setStyleSheet("color: gray;");
    m_galilConnectBtn->setText("Connect");
    m_galilAddressEdit->setEnabled(true);

    setAxesTabEnabled(false);
    if (m_printheadWidget) m_printheadWidget->onGalilDisconnected();

    statusBar()->showMessage("Galil disconnected");
}

void MainWindow::onGalilStatusUpdated()
{
    updateAxisStatus();
}

void MainWindow::onGalilError(const QString &message)
{
    statusBar()->showMessage("Galil error: " + message, 5000);
}

void MainWindow::onAxisIdle(Axis axis)
{
    // If homing completed on either axis, define the (reverse-limit) position as zero.
    // The PrintheadWidget has its own connection to this signal for its own logic.
    Q_UNUSED(axis)
}

// ---------------------------------------------------------------------------
// Axes tab
// ---------------------------------------------------------------------------

void MainWindow::buildAxesTab(QWidget *tab)
{
    auto *layout = new QHBoxLayout(tab);

    layout->addWidget(buildAxisGroup(Axis::X, "X Axis  (Galil A — carriage)"));
    layout->addWidget(buildAxisGroup(Axis::D, "D Axis  (Galil D — reservoir / ink height)"));
    layout->addStretch();
}

QGroupBox *MainWindow::buildAxisGroup(Axis axis, const QString &title)
{
    const bool isX = (axis == Axis::X);

    auto *grp = new QGroupBox(title, this);
    auto *vbox = new QVBoxLayout(grp);

    // ── Status indicators ─────────────────────────────────────────────────
    {
        auto *form = new QFormLayout();

        auto *posLabel = new QLabel("—", this);
        posLabel->setFont(QFont("Courier", 10));
        auto *fwdLabel = new QLabel("—", this);
        auto *revLabel = new QLabel("—", this);
        auto *movLabel = new QLabel("—", this);

        form->addRow("Position:", posLabel);
        form->addRow("Fwd limit:", fwdLabel);
        form->addRow("Rev limit:", revLabel);
        form->addRow("Moving:",    movLabel);

        if (isX) { m_xPosLabel = posLabel; m_xFwdLimLabel = fwdLabel;
                   m_xRevLimLabel = revLabel; m_xMovingLabel = movLabel; }
        else      { m_dPosLabel = posLabel; m_dFwdLimLabel = fwdLabel;
                    m_dRevLimLabel = revLabel; m_dMovingLabel = movLabel; }

        vbox->addLayout(form);
    }

    vbox->addWidget(horizontalLine());

    // ── Jog ──────────────────────────────────────────────────────────────
    {
        auto *negBtn = new QPushButton(isX ? "◀ Left" : "▼ Down", this);
        auto *posBtn = new QPushButton(isX ? "Right ▶" : "Up ▲",  this);
        negBtn->setMinimumHeight(40);
        posBtn->setMinimumHeight(40);

        auto *speedSpin = new QDoubleSpinBox(this);
        speedSpin->setRange(0.5, 100.0);
        speedSpin->setValue(10.0);
        speedSpin->setSuffix(" mm/s");

        auto *jogRow = new QHBoxLayout();
        jogRow->addWidget(negBtn);
        jogRow->addWidget(speedSpin);
        jogRow->addWidget(posBtn);
        vbox->addLayout(jogRow);

        // Hold-to-jog; release stops.
        if (isX)
        {
            m_xRightBtn    = posBtn;
            m_xLeftBtn     = negBtn;
            m_xJogSpeedSpin = speedSpin;
            connect(posBtn,      &QPushButton::pressed,  this, &MainWindow::xRightPressed);
            connect(negBtn,      &QPushButton::pressed,  this, &MainWindow::xLeftPressed);
            connect(posBtn,      &QPushButton::released, this, &MainWindow::xJogReleased);
            connect(negBtn,      &QPushButton::released, this, &MainWindow::xJogReleased);
        }
        else
        {
            m_dUpBtn       = posBtn;
            m_dDownBtn     = negBtn;
            m_dJogSpeedSpin = speedSpin;
            connect(posBtn,      &QPushButton::pressed,  this, &MainWindow::dUpPressed);
            connect(negBtn,      &QPushButton::pressed,  this, &MainWindow::dDownPressed);
            connect(posBtn,      &QPushButton::released, this, &MainWindow::dJogReleased);
            connect(negBtn,      &QPushButton::released, this, &MainWindow::dJogReleased);
        }
    }

    vbox->addWidget(horizontalLine());

    // ── Absolute move ─────────────────────────────────────────────────────
    {
        auto *absSpin = new QDoubleSpinBox(this);
        absSpin->setRange(isX ? -5.0 : -5.0, 500.0);
        absSpin->setValue(0.0);
        absSpin->setDecimals(3);
        absSpin->setSuffix(" mm");

        auto *absBtn = new QPushButton("Move To", this);

        auto *absRow = new QHBoxLayout();
        absRow->addWidget(absSpin, 1);
        absRow->addWidget(absBtn);
        vbox->addLayout(absRow);

        if (isX) { m_xAbsPosSpin = absSpin; m_xAbsMoveBtn = absBtn;
                   connect(absBtn, &QPushButton::clicked, this, &MainWindow::xAbsoluteMoveClicked); }
        else      { m_dAbsPosSpin = absSpin; m_dAbsMoveBtn = absBtn;
                    connect(absBtn, &QPushButton::clicked, this, &MainWindow::dAbsoluteMoveClicked); }
    }

    vbox->addWidget(horizontalLine());

    // ── Home / Enable ─────────────────────────────────────────────────────
    {
        auto *homeBtn   = new QPushButton("Home (Rev Limit → Zero)", this);
        auto *enableBtn = new QPushButton("Enable Motor", this);
        enableBtn->setCheckable(false);

        vbox->addWidget(homeBtn);
        vbox->addWidget(enableBtn);

        if (isX) { m_xHomeBtn = homeBtn; m_xEnableBtn = enableBtn;
                   connect(homeBtn,   &QPushButton::clicked, this, &MainWindow::xHomeClicked);
                   connect(enableBtn, &QPushButton::clicked, this, &MainWindow::xEnableClicked); }
        else      { m_dHomeBtn = homeBtn; m_dEnableBtn = enableBtn;
                    connect(homeBtn,   &QPushButton::clicked, this, &MainWindow::dHomeClicked);
                    connect(enableBtn, &QPushButton::clicked, this, &MainWindow::dEnableClicked); }
    }

    vbox->addStretch();
    return grp;
}

// Helper: a thin horizontal rule
QWidget *MainWindow::horizontalLine()
{
    auto *line = new QFrame(this);
    line->setFrameShape(QFrame::HLine);
    line->setFrameShadow(QFrame::Sunken);
    return line;
}

void MainWindow::setAxesTabEnabled(bool enabled)
{
    const QList<QWidget*> widgets = {
        m_xRightBtn,
        m_xLeftBtn,
        m_xHomeBtn,
        m_xEnableBtn,
        m_xAbsMoveBtn,
        m_dUpBtn,
        m_dDownBtn,
        m_dHomeBtn,
        m_dEnableBtn,
        m_dAbsMoveBtn
    };

    for (QWidget *w : widgets)
    {
        if (w)
            w->setEnabled(enabled);
    }
}

void MainWindow::updateAxisStatus()
{
    if (!m_galil->isConnected()) return;

    auto fmt = [](double v) { return QString::number(v, 'f', 3) + " mm"; };
    auto limStyle = [](bool active) {
        return active ? QString("color: #cc4422; font-weight: bold;")
                      : QString("color: gray;");
    };

    if (m_xPosLabel)
    {
        m_xPosLabel->setText(fmt(m_galil->position(Axis::X)));
        m_xFwdLimLabel->setText(m_galil->isForwardLimitActive(Axis::X) ? "ACTIVE" : "OK");
        m_xFwdLimLabel->setStyleSheet(limStyle(m_galil->isForwardLimitActive(Axis::X)));
        m_xRevLimLabel->setText(m_galil->isReverseLimitActive(Axis::X) ? "ACTIVE" : "OK");
        m_xRevLimLabel->setStyleSheet(limStyle(m_galil->isReverseLimitActive(Axis::X)));
        m_xMovingLabel->setText(m_galil->isMoving(Axis::X) ? "Yes" : "No");
    }

    if (m_dPosLabel)
    {
        m_dPosLabel->setText(fmt(m_galil->position(Axis::D)));
        m_dFwdLimLabel->setText(m_galil->isForwardLimitActive(Axis::D) ? "ACTIVE" : "OK");
        m_dFwdLimLabel->setStyleSheet(limStyle(m_galil->isForwardLimitActive(Axis::D)));
        m_dRevLimLabel->setText(m_galil->isReverseLimitActive(Axis::D) ? "ACTIVE" : "OK");
        m_dRevLimLabel->setStyleSheet(limStyle(m_galil->isReverseLimitActive(Axis::D)));
        m_dMovingLabel->setText(m_galil->isMoving(Axis::D) ? "Yes" : "No");
    }
}

// ---------------------------------------------------------------------------
// Axes tab — X slots
// ---------------------------------------------------------------------------

void MainWindow::xRightPressed()
{
    m_galil->setAcceleration(Axis::X, 800.0);
    m_galil->setDeceleration(Axis::X, 800.0);
    m_galil->jog(Axis::X, m_xJogSpeedSpin->value());
}

void MainWindow::xLeftPressed()
{
    m_galil->setAcceleration(Axis::X, 800.0);
    m_galil->setDeceleration(Axis::X, 800.0);
    m_galil->jog(Axis::X, -m_xJogSpeedSpin->value());
}

void MainWindow::xJogReleased()   { m_galil->stopMotion(Axis::X); }

void MainWindow::xHomeClicked()
{
    m_galil->home(Axis::X, 5.0);
    // onAxisIdle(Axis::X) will fire and define position zero
}

void MainWindow::xAbsoluteMoveClicked()
{
    m_galil->setSpeed(Axis::X, 50.0);
    m_galil->setAcceleration(Axis::X, 800.0);
    m_galil->setDeceleration(Axis::X, 800.0);
    m_galil->moveAbsolute(Axis::X, m_xAbsPosSpin->value());
}

void MainWindow::xEnableClicked()
{
    if (m_galil->isMotorEnabled(Axis::X))
    {
        m_galil->disableMotor(Axis::X);
        m_xEnableBtn->setText("Enable Motor");
    }
    else
    {
        m_galil->enableMotor(Axis::X);
        m_xEnableBtn->setText("Disable Motor");
    }
}

// ---------------------------------------------------------------------------
// Axes tab — D slots
// ---------------------------------------------------------------------------

void MainWindow::dUpPressed()
{
    m_galil->setAcceleration(Axis::D, 800.0);
    m_galil->setDeceleration(Axis::D, 800.0);
    m_galil->jog(Axis::D, m_dJogSpeedSpin->value());
}

void MainWindow::dDownPressed()
{
    m_galil->setAcceleration(Axis::D, 800.0);
    m_galil->setDeceleration(Axis::D, 800.0);
    m_galil->jog(Axis::D, -m_dJogSpeedSpin->value());
}

void MainWindow::dJogReleased()   { m_galil->stopMotion(Axis::D); }

void MainWindow::dHomeClicked()
{
    m_galil->home(Axis::D, 5.0);
}

void MainWindow::dAbsoluteMoveClicked()
{
    m_galil->setSpeed(Axis::D, 20.0);
    m_galil->setAcceleration(Axis::D, 400.0);
    m_galil->setDeceleration(Axis::D, 400.0);
    m_galil->moveAbsolute(Axis::D, m_dAbsPosSpin->value());
}

void MainWindow::dEnableClicked()
{
    if (m_galil->isMotorEnabled(Axis::D))
    {
        m_galil->disableMotor(Axis::D);
        m_dEnableBtn->setText("Enable Motor");
    }
    else
    {
        m_galil->enableMotor(Axis::D);
        m_dEnableBtn->setText("Disable Motor");
    }
}

// ---------------------------------------------------------------------------
// Printhead tab
// ---------------------------------------------------------------------------

void MainWindow::buildPrintheadTab(QWidget *tab)
{
    auto *layout = new QVBoxLayout(tab);
    m_printheadWidget = new PrintheadWidget(m_galil, m_mjController, this);
    layout->addWidget(m_printheadWidget);
}

// ---------------------------------------------------------------------------
// Camera / Strobe tab
// ---------------------------------------------------------------------------

void MainWindow::buildCameraTab(QWidget *tab)
{
    auto *outerLayout = new QHBoxLayout(tab);

    // ── Live view (left, expands) ─────────────────────────────────────────
    m_liveView = new LiveViewWidget(this);
    outerLayout->addWidget(m_liveView, 3);

    // ── Controls (right, scrollable) ──────────────────────────────────────
    auto *controlsWidget = new QWidget(this);
    auto *controlsLayout = new QVBoxLayout(controlsWidget);
    controlsLayout->setContentsMargins(0, 0, 0, 0);

    // Try to open the camera at startup; gracefully disable if not available.
    {
        auto *statusGrp = new QGroupBox("Camera Status", this);
        auto *statusLayout = new QVBoxLayout(statusGrp);
        auto *camStatusLabel = new QLabel(this);
        camStatusLabel->setWordWrap(true);

        QMetaObject::invokeMethod(m_cameraWorker, "openCamera", Qt::QueuedConnection);

        // Wire up worker signals for status label
        connect(m_cameraWorker, &CameraWorker::cameraOpened, this, [this, camStatusLabel] {
            m_cameraAvailable = true;
            camStatusLabel->setText("IDS peak camera connected.");
            camStatusLabel->setStyleSheet("color: green;");
            onCameraOpened();
        });
        connect(m_cameraWorker, &CameraWorker::cameraClosed, this, [camStatusLabel] {
            camStatusLabel->setText("Camera closed.");
            camStatusLabel->setStyleSheet("color: gray;");
        });
        connect(m_cameraWorker, &CameraWorker::error, this, [this, camStatusLabel](const QString &msg) {
            m_cameraAvailable = false;
            camStatusLabel->setText("Camera unavailable: " + msg +
                                    "\n\nInstall IDS peak SDK and restart to enable camera controls.");
            camStatusLabel->setStyleSheet("color: #cc4422;");
            if (m_strobeWidget) m_strobeWidget->setStartStopEnabled(false);
        });

        statusLayout->addWidget(camStatusLabel);
        controlsLayout->addWidget(statusGrp);

        // Note about the two Teensys
        auto *teensyNote = new QLabel(
            "<i>Note: The port scan in 'Teensy Connection' will show <b>two</b> Teensy devices — "
            "the strobe Teensy and the JetForge Teensy.  Identify them by port name or by "
            "unplugging one at a time.</i>", this);
        teensyNote->setWordWrap(true);
        teensyNote->setStyleSheet("color: #888; font-size: 10px;");
        controlsLayout->addWidget(teensyNote);
    }

    m_cameraWidget  = new CameraWidget(m_camera, this);
    m_strobeWidget  = new StrobeWidget(m_camera, this);
    m_arduinoWidget = new ArduinoWidget(m_arduino, this);

    m_strobeWidget->setStartStopEnabled(false);

    controlsLayout->addWidget(m_strobeWidget);
    controlsLayout->addWidget(m_cameraWidget);
    controlsLayout->addWidget(m_arduinoWidget);
    controlsLayout->addStretch();

    auto *scrollArea = new QScrollArea(this);
    scrollArea->setWidget(controlsWidget);
    scrollArea->setWidgetResizable(true);
    scrollArea->setFixedWidth(380);
    outerLayout->addWidget(scrollArea);

    // ── Camera worker signals ─────────────────────────────────────────────
    connect(m_cameraWorker, &CameraWorker::frameReady,
            m_liveView, &LiveViewWidget::updateFrame,
            Qt::QueuedConnection);
    connect(m_cameraWorker, &CameraWorker::acquisitionStarted,
            this, &MainWindow::onAcquisitionStarted,
            Qt::QueuedConnection);
    connect(m_cameraWorker, &CameraWorker::acquisitionStopped,
            this, &MainWindow::onAcquisitionStopped,
            Qt::QueuedConnection);

    // ── Strobe / arduino widget signals ───────────────────────────────────
    connect(m_strobeWidget, &StrobeWidget::acquisitionStartRequested,
            this, &MainWindow::onAcquisitionStartRequested);
    connect(m_strobeWidget, &StrobeWidget::acquisitionStopRequested,
            this, &MainWindow::onAcquisitionStopRequested);
    connect(m_arduinoWidget, &ArduinoWidget::modeChanged,
            this, &MainWindow::onArduinoModeChanged);

    m_liveView->showPlaceholder("Connecting to camera…");
}

// ---------------------------------------------------------------------------
// Camera / strobe slots
// ---------------------------------------------------------------------------

void MainWindow::onCameraOpened()
{
    m_cameraWidget->populate();
    m_strobeWidget->setStartStopEnabled(true);
    m_liveView->showPlaceholder("Press Start Acquisition");
}

void MainWindow::onCameraClosed()
{
    m_strobeWidget->setStartStopEnabled(false);
    m_liveView->showPlaceholder("Camera closed");
}

void MainWindow::onAcquisitionStartRequested()
{
    // Configure trigger based on Arduino widget mode
    if (m_arduinoWidget->isArduinoMode())
    {
        m_camera->setTriggerSource("Line0");
        m_arduino->sendStart();
    }
    else
    {
        m_camera->setTriggerEnabled(false);
    }

    QMetaObject::invokeMethod(m_cameraWorker, "startAcquisition", Qt::QueuedConnection);
    m_arduinoWidget->setAcquiring(true);
}

void MainWindow::onAcquisitionStopRequested()
{
    m_arduino->sendStop();
    QMetaObject::invokeMethod(m_cameraWorker, "stopAcquisition", Qt::QueuedConnection);
    m_arduinoWidget->setAcquiring(false);
}

void MainWindow::onAcquisitionStarted()
{
    m_strobeWidget->onAcquisitionStarted();
    m_cameraWidget->setControlsEnabled(false);
}

void MainWindow::onAcquisitionStopped()
{
    m_strobeWidget->onAcquisitionStopped();
    m_cameraWidget->setControlsEnabled(true);
    m_liveView->showPlaceholder("Acquisition stopped");
}

void MainWindow::onArduinoModeChanged(bool arduinoMode)
{
    // If we're not acquiring, reconfigure the trigger now so the camera
    // is ready before the user presses Start.
    if (!m_cameraAvailable) return;
    if (arduinoMode)
        m_camera->setTriggerSource("Line0");
    else
        m_camera->setTriggerEnabled(false);
}

void MainWindow::onCameraError(const QString &message)
{
    statusBar()->showMessage("Camera error: " + message, 5000);
}
