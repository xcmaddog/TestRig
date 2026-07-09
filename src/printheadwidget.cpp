#include "printheadwidget.h"
#include "galilcontroller.h"
#include "mjdriver.h"
#include "printer.h"

#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QGroupBox>
#include <QLabel>
#include <QCheckBox>
#include <QFormLayout>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QFileDialog>
#include <QCoreApplication>
#include <QTimer>
#include <QDebug>
#include <QDir>
#include <QStandardPaths>
#include <QTextCursor>
#include <QFont>

#include <sstream>
#include <cmath>

PrintheadWidget::PrintheadWidget(GalilController *galil,
                                 Added_Scientific::Controller *mjController,
                                 QWidget *parent)
    : QWidget(parent)
    , m_galil(galil)
    , m_mj(mjController)
{
    buildUI();

    // ── Galil motion completion ────────────────────────────────────────────
    connect(m_galil, &GalilController::axisIdle,
            this, &PrintheadWidget::onAxisIdle);

    // ── JetForge serial responses ──────────────────────────────────────────
    connect(m_mj, &Added_Scientific::Controller::response,
            this, &PrintheadWidget::writeToResponseWindow);
    connect(m_mj, qOverload<const QString&>(&Added_Scientific::Controller::error),
            this, [this](const QString &msg)
            { writeToResponseWindow("[ERROR] " + msg); });

    setJetForgeControlsEnabled(false);
    setMotionControlsEnabled(false);
}

PrintheadWidget::~PrintheadWidget() = default;

// ---------------------------------------------------------------------------
// UI construction
// ---------------------------------------------------------------------------

void PrintheadWidget::buildUI()
{
    auto *outerLayout = new QHBoxLayout(this);
    auto *leftCol  = new QVBoxLayout();
    auto *rightCol = new QVBoxLayout();
    outerLayout->addLayout(leftCol,  1);
    outerLayout->addLayout(rightCol, 1);

    // ══ LEFT COLUMN ════════════════════════════════════════════════════════

    // ── JetForge connection ────────────────────────────────────────────────
    {
        auto *grp = new QGroupBox("JetForge Connection", this);
        auto *form = new QFormLayout(grp);

        m_portLineEdit = new QLineEdit("/dev/ttyACM0", this);
        m_portLineEdit->setPlaceholderText("e.g. /dev/ttyACM0 or /dev/ttyUSB0");
        m_connectBtn = new QPushButton("Connect", this);
        m_connStatus = new QLabel("Not connected", this);
        m_connStatus->setStyleSheet("color: gray;");

        auto *portRow = new QHBoxLayout();
        portRow->addWidget(m_portLineEdit);
        portRow->addWidget(m_connectBtn);
        form->addRow("Port:", portRow);
        form->addRow("Status:", m_connStatus);

        connect(m_connectBtn, &QPushButton::clicked,
                this, &PrintheadWidget::connectToJetForge);

        // Update status label when serial connects/disconnects
        connect(m_mj, &Added_Scientific::Controller::response,
                this, [this](const QString &msg) {
                    if (msg.contains("Connected"))
                    {
                        m_connStatus->setText("Connected");
                        m_connStatus->setStyleSheet("color: green;");
                        m_connectBtn->setText("Disconnect");
                        setJetForgeControlsEnabled(true);
                    }
                    else if (msg.contains("Disconnecting"))
                    {
                        m_connStatus->setText("Not connected");
                        m_connStatus->setStyleSheet("color: gray;");
                        m_connectBtn->setText("Connect");
                        setJetForgeControlsEnabled(false);
                    }
                });

        leftCol->addWidget(grp);
    }

    // ── Power and parameters ───────────────────────────────────────────────
    {
        auto *grp = new QGroupBox("Printhead Control", this);
        auto *form = new QFormLayout(grp);

        m_powerBtn = new QPushButton("Power On", this);
        m_powerBtn->setCheckable(true);
        m_stopBtn  = new QPushButton("Stop Printing", this);

        auto *pRow = new QHBoxLayout();
        pRow->addWidget(m_powerBtn);
        pRow->addWidget(m_stopBtn);
        form->addRow(pRow);

        m_freqSpin = new QSpinBox(this);
        m_freqSpin->setRange(100, 10000);
        m_freqSpin->setValue(1000);
        m_freqSpin->setSuffix(" Hz");
        form->addRow("Frequency:", m_freqSpin);

        m_voltageSpin = new QDoubleSpinBox(this);
        m_voltageSpin->setRange(15.0, 36.0);
        m_voltageSpin->setValue(25.0);
        m_voltageSpin->setDecimals(1);
        m_voltageSpin->setSuffix(" V");
        form->addRow("Voltage:", m_voltageSpin);

        m_startSpin = new QSpinBox(this);
        m_startSpin->setRange(1, 999999);
        m_startSpin->setValue(1);
        m_startSpin->setToolTip("Encoder count at which printing begins (absolute start).");
        form->addRow("Abs. Start:", m_startSpin);

        m_getStatusBtn = new QPushButton("Get Status", this);
        form->addRow(m_getStatusBtn);

        connect(m_powerBtn,      &QPushButton::clicked,
                this, &PrintheadWidget::powerTogglePressed);
        connect(m_stopBtn,       &QPushButton::clicked,
                this, &PrintheadWidget::stopPrintingPressed);
        connect(m_freqSpin,      &QSpinBox::editingFinished,
                this, &PrintheadWidget::frequencyChanged);
        connect(m_voltageSpin,   &QDoubleSpinBox::editingFinished,
                this, &PrintheadWidget::voltageChanged);
        connect(m_startSpin,     &QSpinBox::editingFinished,
                this, &PrintheadWidget::absoluteStartChanged);
        connect(m_getStatusBtn,  &QPushButton::clicked,
                this, [this]{ m_mj->report_status(); });

        leftCol->addWidget(grp);
    }

    // ── Bitmap ─────────────────────────────────────────────────────────────
    {
        auto *grp = new QGroupBox("Bitmap", this);
        auto *vbox = new QVBoxLayout(grp);

        auto *fileRow = new QHBoxLayout();
        m_fileLineEdit = new QLineEdit(this);
        m_fileLineEdit->setPlaceholderText("Path to .bmp file");
        m_browseBtn = new QPushButton("Browse…", this);
        m_browseBtn->setFixedWidth(70);
        fileRow->addWidget(m_fileLineEdit);
        fileRow->addWidget(m_browseBtn);
        vbox->addLayout(fileRow);

        auto *btnRow = new QHBoxLayout();
        m_testJetBtn     = new QPushButton("Test Jet", this);
        m_purgeBtn       = new QPushButton("Purge Nozzles", this);
        btnRow->addWidget(m_testJetBtn);
        btnRow->addWidget(m_purgeBtn);
        vbox->addLayout(btnRow);

        auto *singleRow = new QHBoxLayout();
        m_singleNozzleBtn = new QPushButton("Single Nozzle", this);
        m_nozzleNumSpin   = new QSpinBox(this);
        m_nozzleNumSpin->setRange(1, 128);
        m_nozzleNumSpin->setValue(1);
        m_nozzleNumSpin->setFixedWidth(60);
        singleRow->addWidget(m_singleNozzleBtn);
        singleRow->addWidget(new QLabel("Nozzle #:", this));
        singleRow->addWidget(m_nozzleNumSpin);
        vbox->addLayout(singleRow);

        auto *createRow = new QHBoxLayout();
        m_createBitmapBtn = new QPushButton("Create Test Bitmap", this);
        m_numLinesSpin    = new QSpinBox(this);
        m_numLinesSpin->setRange(1, 128);
        m_numLinesSpin->setValue(4);
        m_numLinesSpin->setToolTip("Number of horizontal lines");
        m_pixelWidthSpin  = new QSpinBox(this);
        m_pixelWidthSpin->setRange(1, 10000);
        m_pixelWidthSpin->setValue(1000);
        m_pixelWidthSpin->setToolTip("Bitmap width in pixels");
        createRow->addWidget(m_createBitmapBtn);
        createRow->addWidget(new QLabel("Lines:", this));
        createRow->addWidget(m_numLinesSpin);
        createRow->addWidget(new QLabel("Width:", this));
        createRow->addWidget(m_pixelWidthSpin);
        vbox->addLayout(createRow);

        connect(m_browseBtn,       &QPushButton::clicked,
                this, &PrintheadWidget::browseForBitmap);
        connect(m_fileLineEdit,    &QLineEdit::returnPressed,
                this, &PrintheadWidget::fileNameEntered);
        connect(m_testJetBtn,      &QPushButton::clicked,
                this, &PrintheadWidget::testJetPressed);
        connect(m_purgeBtn,        &QPushButton::clicked,
                this, &PrintheadWidget::purgeNozzles);
        connect(m_singleNozzleBtn, &QPushButton::clicked,
                this, &PrintheadWidget::singleNozzlePressed);
        connect(m_createBitmapBtn, &QPushButton::clicked,
                this, &PrintheadWidget::createBitmapPressed);

        leftCol->addWidget(grp);
    }

    // ── Quick purge (solenoid) ─────────────────────────────────────────────
    {
        auto *grp = new QGroupBox("Quick Purge (Air Solenoid)", this);
        auto *form = new QFormLayout(grp);

        m_purgeMsSpin = new QSpinBox(this);
        m_purgeMsSpin->setRange(50, 5000);
        m_purgeMsSpin->setValue(500);
        m_purgeMsSpin->setSuffix(" ms");
        m_purgeMsSpin->setToolTip("How long to open the solenoid valve.");
        form->addRow("Pulse duration:", m_purgeMsSpin);

        m_quickPurgeBtn = new QPushButton("Open Solenoid", this);
        m_quickPurgeBtn->setMinimumHeight(32);
        form->addRow(m_quickPurgeBtn);

        connect(m_quickPurgeBtn, &QPushButton::clicked,
                this, &PrintheadWidget::quickPurgePressed);

        leftCol->addWidget(grp);
    }

    leftCol->addStretch();

    // ══ RIGHT COLUMN ═══════════════════════════════════════════════════════

    // ── X-axis motion ──────────────────────────────────────────────────────
    {
        auto *grp = new QGroupBox("X Axis Motion", this);
        auto *vbox = new QVBoxLayout(grp);

        auto *jogRow = new QHBoxLayout();
        m_xLeftBtn  = new QPushButton("◀ Left",  this);
        m_xRightBtn = new QPushButton("Right ▶", this);
        m_xLeftBtn->setMinimumHeight(40);
        m_xRightBtn->setMinimumHeight(40);

        m_xVelocitySpin = new QDoubleSpinBox(this);
        m_xVelocitySpin->setRange(1.0, 200.0);
        m_xVelocitySpin->setValue(20.0);
        m_xVelocitySpin->setSuffix(" mm/s");
        m_xVelocitySpin->setToolTip("Jog speed.");

        jogRow->addWidget(m_xLeftBtn);
        jogRow->addWidget(m_xVelocitySpin);
        jogRow->addWidget(m_xRightBtn);
        vbox->addLayout(jogRow);

        auto *utilRow = new QHBoxLayout();
        m_xHomeBtn   = new QPushButton("Home X", this);
        m_xGetPosBtn = new QPushButton("Get Position", this);
        utilRow->addWidget(m_xHomeBtn);
        utilRow->addWidget(m_xGetPosBtn);
        vbox->addLayout(utilRow);

        // Hold-to-jog behaviour
        connect(m_xRightBtn,  &QPushButton::pressed,  this, &PrintheadWidget::xRightPressed);
        connect(m_xLeftBtn,   &QPushButton::pressed,  this, &PrintheadWidget::xLeftPressed);
        connect(m_xRightBtn,  &QPushButton::released, this, &PrintheadWidget::xJogReleased);
        connect(m_xLeftBtn,   &QPushButton::released, this, &PrintheadWidget::xJogReleased);
        connect(m_xHomeBtn,   &QPushButton::clicked,  this, &PrintheadWidget::xHomeClicked);
        connect(m_xGetPosBtn, &QPushButton::clicked,  this, &PrintheadWidget::getXPosition);

        rightCol->addWidget(grp);
    }

    // ── Serial command entry ───────────────────────────────────────────────
    {
        auto *grp = new QGroupBox("Serial Command", this);
        auto *vbox = new QVBoxLayout(grp);

        auto *cmdRow = new QHBoxLayout();
        m_commandLineEdit = new QLineEdit(this);
        m_commandLineEdit->setPlaceholderText("Enter command and press Enter…");
        m_clearBtn = new QPushButton("Clear", this);
        m_clearBtn->setFixedWidth(50);
        cmdRow->addWidget(m_commandLineEdit);
        cmdRow->addWidget(m_clearBtn);
        vbox->addLayout(cmdRow);

        m_responseTextEdit = new QPlainTextEdit(this);
        m_responseTextEdit->setReadOnly(true);
        m_responseTextEdit->setMinimumHeight(200);
        m_responseTextEdit->setFont(QFont("Courier", 9));
        m_responseTextEdit->setPlaceholderText("Printhead responses appear here…");
        vbox->addWidget(m_responseTextEdit);

        connect(m_commandLineEdit, &QLineEdit::returnPressed,
                this, &PrintheadWidget::commandEntered);
        connect(m_clearBtn, &QPushButton::clicked,
                this, &PrintheadWidget::clearResponseText);

        rightCol->addWidget(grp);
    }

    rightCol->addStretch();
}

// ---------------------------------------------------------------------------
// Public slots (called by MainWindow on Galil connect/disconnect)
// ---------------------------------------------------------------------------

void PrintheadWidget::onGalilConnected()
{
    setMotionControlsEnabled(true);
}

void PrintheadWidget::onGalilDisconnected()
{
    setMotionControlsEnabled(false);
}

// ---------------------------------------------------------------------------
// Enable / disable helpers
// ---------------------------------------------------------------------------

void PrintheadWidget::setJetForgeControlsEnabled(bool enabled)
{
    m_powerBtn->setEnabled(enabled);
    m_stopBtn->setEnabled(enabled);
    m_freqSpin->setEnabled(enabled);
    m_voltageSpin->setEnabled(enabled);
    m_startSpin->setEnabled(enabled);
    m_getStatusBtn->setEnabled(enabled);
    m_fileLineEdit->setEnabled(enabled);
    m_browseBtn->setEnabled(enabled);
    m_testJetBtn->setEnabled(enabled);
    m_purgeBtn->setEnabled(enabled);
    m_singleNozzleBtn->setEnabled(enabled);
    m_nozzleNumSpin->setEnabled(enabled);
    m_createBitmapBtn->setEnabled(enabled);
    m_commandLineEdit->setEnabled(enabled);
}

void PrintheadWidget::setMotionControlsEnabled(bool enabled)
{
    m_xLeftBtn->setEnabled(enabled);
    m_xRightBtn->setEnabled(enabled);
    m_xHomeBtn->setEnabled(enabled);
    m_xGetPosBtn->setEnabled(enabled);
    m_xVelocitySpin->setEnabled(enabled);
    m_quickPurgeBtn->setEnabled(enabled);
}

// ---------------------------------------------------------------------------
// JetForge connection / power
// ---------------------------------------------------------------------------

void PrintheadWidget::connectToJetForge()
{
    if (m_mj->is_connected())
    {
        m_mj->disconnect_serial();
    }
    else
    {
        m_mj->set_port_name(m_portLineEdit->text().trimmed());
        m_mj->connect_board();
    }
}

void PrintheadWidget::powerTogglePressed()
{
    if (m_powerBtn->isChecked())
    {
        m_mj->power_off();
        m_powerBtn->setText("Power On");
    }
    else
    {
        m_mj->power_on();
        m_powerBtn->setText("Power Off");
    }
}

void PrintheadWidget::stopPrintingPressed()
{
    m_mj->clear_nozzles();
}

// ---------------------------------------------------------------------------
// Parameter setters
// ---------------------------------------------------------------------------

void PrintheadWidget::frequencyChanged()
{
    m_mj->set_printing_frequency(m_freqSpin->value());
}

void PrintheadWidget::voltageChanged()
{
    double v = m_voltageSpin->value();
    m_mj->set_head_voltage(Added_Scientific::Controller::HEAD1, v);
    m_mj->set_head_voltage(Added_Scientific::Controller::HEAD2, v);
}

void PrintheadWidget::absoluteStartChanged()
{
    m_mj->set_absolute_start(m_startSpin->value());
}

// ---------------------------------------------------------------------------
// Bitmap / print actions
// ---------------------------------------------------------------------------

void PrintheadWidget::browseForBitmap()
{
    const QString path = QFileDialog::getOpenFileName(
        this, "Select Bitmap", QDir::homePath(),
        "Bitmap images (*.bmp);;All Files (*)");
    if (!path.isEmpty())
        m_fileLineEdit->setText(path);
}

void PrintheadWidget::fileNameEntered()
{
    readInFile(m_fileLineEdit->text().trimmed());
}

void PrintheadWidget::readInFile(const QString &filePath, int headIdx)
{
    QImage image(filePath);
    if (image.isNull())
    {
        writeToResponseWindow("Failed to load image: " + filePath);
        return;
    }
    m_mj->send_image_data(headIdx, image, 0);
}

// Test jet: encoder-based single pass at a fixed X location.
// Adapt xLocation, frequency, and speed for your rig.
void PrintheadWidget::testJetPressed()
{
    const double xLocation  = 10.0;    // mm — adjust to your rig
    const double frequency  = 1000.0;  // Hz
    const double printSpeed = 10.0;    // mm/s

    if (!m_mj->is_connected()) { writeToResponseWindow("JetForge not connected."); return; }
    if (!m_galil->isConnected()) { writeToResponseWindow("Galil not connected."); return; }

    writeToResponseWindow("--- Test Jet ---");
    m_mj->write_line("M 4"); // encoder mode

    const double acceleration = 1000.0;
    const double safetyFactor = 2.0;
    const double backUpDist   = (std::pow(printSpeed, 2.0) / (2.0 * acceleration)) * safetyFactor;
    const double backedUpStartX = xLocation - backUpDist;
    const double imageWidth     = 1000;   // pixels — adjust for your bitmap
    const double printDist      = (imageWidth / frequency) * printSpeed;
    const double endTarget      = xLocation + printDist + backUpDist;
    const int    backUpDistEnc  = static_cast<int>(backUpDist * X_CNTS_PER_MM);

    m_mj->set_printing_frequency(static_cast<int>(frequency));
    readInFile(m_fileLineEdit->text().trimmed());

    moveToX(backedUpStartX, "Test Jet Start");
    m_mj->set_absolute_start(backUpDistEnc);
    printEncoder(acceleration, printSpeed, endTarget, "Test Jet Complete");
}

void PrintheadWidget::purgeNozzles()
{
    const double purgeStartX   = 10.0;
    const double purgeDistance = 5.0;
    const double purgeSpeed    = 10.0;
    const double accel         = 1000.0;
    const double purgeFreq     = 1000.0;
    const double backUpDist    = 1.0;

    if (!m_mj->is_connected()) { writeToResponseWindow("JetForge not connected."); return; }
    if (!m_galil->isConnected()) { writeToResponseWindow("Galil not connected."); return; }

    writeToResponseWindow("--- Purge Sequence ---");
    if (!m_powerBtn->isChecked())
    {
        m_mj->power_on();
        QTimer::singleShot(200, []{});   // brief pause
    }

    m_mj->write_line("M 4");
    m_mj->set_printing_frequency(static_cast<int>(purgeFreq));
    readInFile(QDir(QStandardPaths::writableLocation(QStandardPaths::HomeLocation))
                   .filePath("purge.bmp"));

    double backedUp = purgeStartX - backUpDist;
    double endTarget = backedUp + backUpDist + purgeDistance + backUpDist;

    moveToX(backedUp, "Arrived at Purge Start");
    m_mj->set_absolute_start(1);
    printEncoder(accel, purgeSpeed, endTarget, "Purge Complete");
    writeToResponseWindow("--- Purge Done ---");
}

void PrintheadWidget::singleNozzlePressed()
{
    m_mj->write_line("M 2");
    QByteArray cmd = "n 1 " + QByteArray::number(m_nozzleNumSpin->value());
    m_mj->write_line(cmd);
}

void PrintheadWidget::createBitmapPressed()
{
    m_mj->create_bitmap_lines(m_numLinesSpin->value(), m_pixelWidthSpin->value());
}

// ---------------------------------------------------------------------------
// Quick purge (solenoid)
// ---------------------------------------------------------------------------

void PrintheadWidget::quickPurgePressed()
{
    if (!m_galil->isConnected())
    {
        writeToResponseWindow("Galil not connected — cannot operate solenoid.");
        return;
    }
    int  pulseMs  = m_purgeMsSpin->value();
    m_galil->quickPurge(pulseMs);
    writeToResponseWindow(QString("Quick purge: %1 ms pulse on %2.")
                              .arg(pulseMs)
                              .arg(SOLENOID_BIT));
}

// ---------------------------------------------------------------------------
// Serial command entry
// ---------------------------------------------------------------------------

void PrintheadWidget::commandEntered()
{
    sendCommand(m_commandLineEdit->text());
    m_commandLineEdit->clear();
}

void PrintheadWidget::clearResponseText()
{
    m_responseTextEdit->clear();
}

void PrintheadWidget::sendCommand(const QString &command)
{
    m_responseTextEdit->appendPlainText("> " + command);
    m_mj->write_line(command.toUtf8());
}

void PrintheadWidget::writeToResponseWindow(const QString &text)
{
    m_responseTextEdit->appendPlainText(text);
    m_responseTextEdit->moveCursor(QTextCursor::End);
}

// ---------------------------------------------------------------------------
// X-axis motion
// ---------------------------------------------------------------------------

void PrintheadWidget::xRightPressed()
{
    double speed = m_xVelocitySpin->value();
    m_galil->setAcceleration(Axis::X, 800.0);
    m_galil->setDeceleration(Axis::X, 800.0);
    m_galil->jog(Axis::X, speed);
}

void PrintheadWidget::xLeftPressed()
{
    double speed = m_xVelocitySpin->value();
    m_galil->setAcceleration(Axis::X, 800.0);
    m_galil->setDeceleration(Axis::X, 800.0);
    m_galil->jog(Axis::X, -speed);
}

void PrintheadWidget::xJogReleased()
{
    m_galil->stopMotion(Axis::X);
}

void PrintheadWidget::xHomeClicked()
{
    writeToResponseWindow("Homing X axis...");
    m_galil->home(Axis::X, 5.0);
    // axisIdle(Axis::X) will fire when the limit trips; at that point
    // we define position zero.  We connect this via onAxisIdle().
}

void PrintheadWidget::getXPosition()
{
    double pos = m_galil->position(Axis::X);
    writeToResponseWindow(QString("X position: %1 mm").arg(pos, 0, 'f', 3));
}

// ---------------------------------------------------------------------------
// Motion complete
// ---------------------------------------------------------------------------

void PrintheadWidget::onAxisIdle(Axis axis)
{
    if (axis == Axis::X)
    {
        // If we were homing, define the reverse-limit position as zero.
        // (The flag check is simplified — in a full implementation you'd
        //  track whether a home was in progress vs. a regular move.)
        if (m_galil->isReverseLimitActive(Axis::X))
            m_galil->definePositionZero(Axis::X);

        m_motionComplete = true;
    }
}

// ---------------------------------------------------------------------------
// Internal motion helpers (X-axis only)
// ---------------------------------------------------------------------------

void PrintheadWidget::moveToX(double xMm, const QString &endMessage)
{
    m_motionComplete = false;
    writeToResponseWindow(QString("Moving to X: %1 mm").arg(xMm));

    std::stringstream s;
    s << CMD::set_accleration(Axis::X, 600.0);
    s << CMD::set_deceleration(Axis::X, 600.0);
    s << CMD::set_speed(Axis::X, 60.0);
    s << CMD::position_absolute(Axis::X, xMm);
    s << CMD::begin_motion(Axis::X);
    s << CMD::after_motion(Axis::X);
    s << CMD::display_message("Arrived at Location");

    m_galil->downloadAndRun(CMD::cmd_buf_to_dmc(s));

    while (!m_motionComplete)
        QCoreApplication::processEvents();

    m_motionComplete = false;
    writeToResponseWindow(endMessage);
}

// Immediate (non-encoder) print pass.
void PrintheadWidget::printImmediate(double accel, double speed,
                                     double endMm, const QString &endMessage)
{
    m_motionComplete = false;

    std::stringstream s;
    s << CMD::set_accleration(Axis::X, accel);
    s << CMD::set_deceleration(Axis::X, accel);
    s << CMD::set_speed(Axis::X, speed);
    s << CMD::position_absolute(Axis::X, endMm);
    s << CMD::start_MJ_print();
    s << CMD::start_MJ_dir();
    s << CMD::begin_motion(Axis::X);
    s << CMD::after_motion(Axis::X);
    s << CMD::disable_MJ_dir();
    s << CMD::disable_MJ_start();
    s << CMD::display_message("Print Complete");

    m_galil->downloadAndRun(CMD::cmd_buf_to_dmc(s));

    while (!m_motionComplete)
        QCoreApplication::processEvents();

    m_motionComplete = false;
    writeToResponseWindow(endMessage);
}

// Encoder-based print pass (printhead fires on encoder pulses).
void PrintheadWidget::printEncoder(double accel, double speed,
                                   double endMm, const QString &endMessage)
{
    m_motionComplete = false;

    std::stringstream s;
    s << CMD::set_accleration(Axis::X, accel);
    s << CMD::set_deceleration(Axis::X, accel);
    s << CMD::set_speed(Axis::X, speed);
    s << CMD::position_absolute(Axis::X, endMm);
    s << CMD::begin_motion(Axis::X);
    s << CMD::after_motion(Axis::X);
    s << CMD::display_message("Print Complete");

    m_galil->downloadAndRun(CMD::cmd_buf_to_dmc(s));

    while (!m_motionComplete)
        QCoreApplication::processEvents();

    m_motionComplete = false;
    writeToResponseWindow(endMessage);
}
