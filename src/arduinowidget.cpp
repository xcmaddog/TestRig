#include "arduinowidget.h"
#include "arduinocontroller.h"

#include <QSerialPortInfo>
#include <QComboBox>
#include <QSpinBox>
#include <QPushButton>
#include <QLabel>
#include <QPlainTextEdit>
#include <QGroupBox>
#include <QFormLayout>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QDateTime>
#include <QFont>

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------

ArduinoWidget::ArduinoWidget(ArduinoController *arduino, QWidget *parent)
    : QWidget(parent), m_arduino(arduino)
{
    buildUI();

    connect(m_arduino, &ArduinoController::connected,
            this, &ArduinoWidget::onArduinoConnected);
    connect(m_arduino, &ArduinoController::disconnected,
            this, &ArduinoWidget::onArduinoDisconnected);
    connect(m_arduino, &ArduinoController::errorOccurred,
            this, &ArduinoWidget::onArduinoError);

    connect(m_arduino, &ArduinoController::commandSent,
            this, [this](const QString &line)
            { appendLog("→ " + line); });
    connect(m_arduino, &ArduinoController::rawLineReceived,
            this, [this](const QString &line)
            { appendLog("← " + line); });

    rescanPorts();
    updateConnectionUI(false);
    updateTimingEnabled();
}

// ---------------------------------------------------------------------------
// buildUI
// ---------------------------------------------------------------------------

void ArduinoWidget::buildUI()
{
    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(4);

    // ── Connection ───────────────────────────────────────────────────────
    {
        auto *grp = new QGroupBox("Teensy Connection", this);
        auto *vbox = new QVBoxLayout(grp);

        auto *portRow = new QHBoxLayout();
        m_portCombo = new QComboBox(this);
        m_portCombo->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        m_rescanBtn = new QPushButton("Rescan", this);
        m_rescanBtn->setFixedWidth(60);
        portRow->addWidget(m_portCombo);
        portRow->addWidget(m_rescanBtn);

        auto *statusRow = new QHBoxLayout();
        m_connIndicator = new QLabel(this);
        m_connIndicator->setFixedSize(14, 14);
        m_connLabel = new QLabel("Not connected", this);
        m_connLabel->setStyleSheet("color: gray; font-size: 10px;");
        m_connectBtn = new QPushButton("Connect", this);
        statusRow->addWidget(m_connIndicator);
        statusRow->addWidget(m_connLabel, 1);
        statusRow->addWidget(m_connectBtn);

        vbox->addLayout(portRow);
        vbox->addLayout(statusRow);
        outer->addWidget(grp);

        connect(m_rescanBtn, &QPushButton::clicked,
                this, &ArduinoWidget::onRescanClicked);
        connect(m_connectBtn, &QPushButton::clicked,
                this, &ArduinoWidget::onConnectClicked);
        connect(m_portCombo,
                QOverload<int>::of(&QComboBox::currentIndexChanged),
                this, &ArduinoWidget::onPortComboChanged);
    }

    // ── Trigger mode ─────────────────────────────────────────────────────
    {
        auto *grp = new QGroupBox("Trigger Mode", this);
        auto *form = new QFormLayout(grp);

        m_modeCombo = new QComboBox(this);
        m_modeCombo->addItem("Free-run (camera self-triggered)", "FREERUN");
        m_modeCombo->addItem("Arduino (nFire → camera + strobe)", "ARDUINO");
        form->addRow("Mode:", m_modeCombo);
        outer->addWidget(grp);

        connect(m_modeCombo,
                QOverload<int>::of(&QComboBox::currentIndexChanged),
                this, &ArduinoWidget::onModeChanged);
    }

    // ── Timing ───────────────────────────────────────────────────────────
    {
        auto *grp = new QGroupBox("Timing (Arduino mode)", this);
        auto *form = new QFormLayout(grp);

        m_dividerSpin = new QSpinBox(this);
        m_dividerSpin->setRange(1, 9999);
        m_dividerSpin->setValue(13);
        m_dividerSpin->setToolTip(
            "Fire the camera every Nth nFire edge.\n"
            "Both rising and falling edges are counted.\n"
            "At 500 Hz nFire → 1000 events/s.\n"
            "N=13 gives ~77 fps (below the 80 fps camera limit).");

        m_trigDelaySpin = new QSpinBox(this);
        m_trigDelaySpin->setRange(0, 1000000);
        m_trigDelaySpin->setSuffix(" µs");
        m_trigDelaySpin->setSingleStep(10);
        m_trigDelaySpin->setToolTip(
            "Delay from nFire edge to camera trigger pulse.\n"
            "Use this to phase-shift the capture relative to jetting.");

        m_flashDelaySpin = new QSpinBox(this);
        m_flashDelaySpin->setRange(0, 1000000);
        m_flashDelaySpin->setSuffix(" µs");
        m_flashDelaySpin->setSingleStep(10);
        m_flashDelaySpin->setToolTip(
            "Delay from camera trigger pulse to first strobe pulse.\n"
            "Sweep this to track droplet position across frames.");

        m_flashDurSpin = new QSpinBox(this);
        m_flashDurSpin->setRange(1, 10000);
        m_flashDurSpin->setValue(5);
        m_flashDurSpin->setSuffix(" µs");
        m_flashDurSpin->setSingleStep(1);
        m_flashDurSpin->setToolTip(
            "How long each strobe pulse stays on.\n"
            "Shorter pulses freeze faster droplets.");

        m_flashCountSpin = new QSpinBox(this);
        m_flashCountSpin->setRange(1, 20);
        m_flashCountSpin->setValue(1);
        m_flashCountSpin->setToolTip(
            "Number of strobe pulses per camera trigger event.\n"
            "Multiple pulses brighten the image without increasing\n"
            "peak LED current. All pulses must fit within the\n"
            "camera exposure window (see Min. Exposure below).");

        m_flashPeriodSpin = new QSpinBox(this);
        m_flashPeriodSpin->setRange(2, 100000);
        m_flashPeriodSpin->setValue(50);
        m_flashPeriodSpin->setSuffix(" µs");
        m_flashPeriodSpin->setSingleStep(5);
        m_flashPeriodSpin->setToolTip(
            "Time from the start of one strobe pulse to the start\n"
            "of the next. Must be greater than Flash Duration.\n"
            "Ignored when Flash Count = 1.");

        // Calculated minimum exposure display
        m_minExposureLabel = new QLabel("– – –", this);
        m_minExposureLabel->setStyleSheet("color: gray; font-size: 10px;");
        m_minExposureLabel->setToolTip(
            "Minimum camera exposure time needed to contain all strobe\n"
            "pulses: Flash Delay + (Count-1)×Period + Duration.\n"
            "Set the camera exposure to at least this value.");

        form->addRow("nFire divider:", m_dividerSpin);
        form->addRow("Trigger delay:", m_trigDelaySpin);
        form->addRow("Flash delay:", m_flashDelaySpin);
        form->addRow("Flash duration:", m_flashDurSpin);
        form->addRow("Flash count:", m_flashCountSpin);
        form->addRow("Flash period:", m_flashPeriodSpin);
        form->addRow("Min. exposure:", m_minExposureLabel);
        outer->addWidget(grp);

        connect(m_dividerSpin,
                QOverload<int>::of(&QSpinBox::valueChanged),
                this, &ArduinoWidget::onDividerChanged);
        connect(m_trigDelaySpin,
                QOverload<int>::of(&QSpinBox::valueChanged),
                this, &ArduinoWidget::onTriggerDelayChanged);
        connect(m_flashDelaySpin,
                QOverload<int>::of(&QSpinBox::valueChanged),
                this, &ArduinoWidget::onFlashDelayChanged);
        connect(m_flashDurSpin,
                QOverload<int>::of(&QSpinBox::valueChanged),
                this, &ArduinoWidget::onFlashDurationChanged);
        connect(m_flashCountSpin,
                QOverload<int>::of(&QSpinBox::valueChanged),
                this, &ArduinoWidget::onFlashCountChanged);
        connect(m_flashPeriodSpin,
                QOverload<int>::of(&QSpinBox::valueChanged),
                this, &ArduinoWidget::onFlashPeriodChanged);

        // Keep min-exposure label live whenever relevant spinners change
        connect(m_flashDelaySpin,
                QOverload<int>::of(&QSpinBox::valueChanged),
                this, &ArduinoWidget::updateMinExposureLabel);
        connect(m_flashDurSpin,
                QOverload<int>::of(&QSpinBox::valueChanged),
                this, &ArduinoWidget::updateMinExposureLabel);
        connect(m_flashCountSpin,
                QOverload<int>::of(&QSpinBox::valueChanged),
                this, &ArduinoWidget::updateMinExposureLabel);
        connect(m_flashPeriodSpin,
                QOverload<int>::of(&QSpinBox::valueChanged),
                this, &ArduinoWidget::updateMinExposureLabel);
    }

    // ── Debug log ────────────────────────────────────────────────────────
    {
        auto *grp = new QGroupBox("Serial Debug Log", this);
        auto *vbox = new QVBoxLayout(grp);

        m_log = new QPlainTextEdit(this);
        m_log->setReadOnly(true);
        m_log->setMaximumBlockCount(kMaxLogLines);
        m_log->setFixedHeight(120);
        m_log->setFont(QFont("Courier", 8));
        m_log->setPlaceholderText("Serial traffic will appear here…");

        auto *clearBtn = new QPushButton("Clear", this);
        clearBtn->setFixedWidth(50);
        connect(clearBtn, &QPushButton::clicked, m_log, &QPlainTextEdit::clear);

        auto *logHeader = new QHBoxLayout();
        logHeader->addStretch();
        logHeader->addWidget(clearBtn);

        vbox->addLayout(logHeader);
        vbox->addWidget(m_log);
        outer->addWidget(grp);
    }

    outer->addStretch();
    updateMinExposureLabel();
}

// ---------------------------------------------------------------------------
// Public interface
// ---------------------------------------------------------------------------

void ArduinoWidget::setAcquiring(bool acquiring)
{
    m_acquiring = acquiring;
    updateTimingEnabled();
    m_portCombo->setEnabled(!acquiring);
    m_rescanBtn->setEnabled(!acquiring);
    m_connectBtn->setEnabled(!acquiring);
    m_modeCombo->setEnabled(!acquiring);
}

bool ArduinoWidget::isArduinoMode() const
{
    return m_modeCombo->currentData().toString() == "ARDUINO";
}

void ArduinoWidget::appendLog(const QString &line)
{
    const QString ts = QDateTime::currentDateTime().toString("hh:mm:ss.zzz");
    m_log->appendPlainText(QString("[%1] %2").arg(ts, line));
}

// ---------------------------------------------------------------------------
// Private helpers
// ---------------------------------------------------------------------------

void ArduinoWidget::rescanPorts()
{
    m_populating = true;
    const QString current = m_portCombo->currentText();
    m_portCombo->clear();

    // detectTeensys() replaces the old detectArduinos()
    const QStringList detected = ArduinoController::detectTeensys();

    QStringList added;
    for (const QString &p : detected)
    {
        m_portCombo->addItem(p + "  [Teensy]", p);
        added << p;
    }

    const auto allPorts = QSerialPortInfo::availablePorts();
    for (const QSerialPortInfo &info : allPorts)
    {
        if (!added.contains(info.portName()))
            m_portCombo->addItem(info.portName(), info.portName());
    }

    const int idx = m_portCombo->findText(current, Qt::MatchStartsWith);
    if (idx >= 0)
        m_portCombo->setCurrentIndex(idx);

    m_populating = false;

    if (detected.size() == 1 && !m_arduino->isConnected())
        m_arduino->connectToPort(detected.first());
}

void ArduinoWidget::updateConnectionUI(bool connected)
{
    if (connected)
    {
        m_connIndicator->setStyleSheet(
            "background-color: #22cc44; border-radius: 7px;");
        m_connLabel->setText("Connected: " + m_arduino->currentPort());
        m_connLabel->setStyleSheet("color: #22cc44; font-size: 10px;");
        m_connectBtn->setText("Disconnect");
    }
    else
    {
        m_connIndicator->setStyleSheet(
            "background-color: #555; border-radius: 7px;");
        m_connLabel->setText("Not connected");
        m_connLabel->setStyleSheet("color: gray; font-size: 10px;");
        m_connectBtn->setText("Connect");
    }
    updateTimingEnabled();
}

void ArduinoWidget::updateTimingEnabled()
{
    const bool enable = isArduinoMode() && m_arduino->isConnected() && !m_acquiring;
    m_dividerSpin->setEnabled(enable);
    m_trigDelaySpin->setEnabled(enable);
    m_flashDelaySpin->setEnabled(enable);
    m_flashDurSpin->setEnabled(enable);
    m_flashCountSpin->setEnabled(enable);

    // Flash period only meaningful when count > 1
    m_flashPeriodSpin->setEnabled(enable && m_flashCountSpin->value() > 1);
}

void ArduinoWidget::updateMinExposureLabel()
{
    const int delay = m_flashDelaySpin->value();
    const int dur = m_flashDurSpin->value();
    const int count = m_flashCountSpin->value();
    const int period = m_flashPeriodSpin->value();

    // Minimum exposure = flash_delay + (count-1)*period + duration
    // This is the time from camera trigger to end of last strobe pulse.
    const int minExp = delay + (count - 1) * period + dur;
    m_minExposureLabel->setText(QString("%1 µs").arg(minExp));

    // Warn if period ≤ duration (pulses would overlap)
    if (count > 1 && period <= dur)
    {
        m_minExposureLabel->setStyleSheet("color: #cc4422; font-size: 10px;");
        m_minExposureLabel->setToolTip(
            "WARNING: Flash Period must be greater than Flash Duration.\n"
            "Increase Flash Period or decrease Flash Duration.");
    }
    else
    {
        m_minExposureLabel->setStyleSheet("color: gray; font-size: 10px;");
    }
}

// ---------------------------------------------------------------------------
// Slots — connection
// ---------------------------------------------------------------------------

void ArduinoWidget::onRescanClicked() { rescanPorts(); }

void ArduinoWidget::onConnectClicked()
{
    if (m_arduino->isConnected())
    {
        m_arduino->disconnectPort();
    }
    else
    {
        const QString port = m_portCombo->currentData().toString();
        if (!port.isEmpty())
            m_arduino->connectToPort(port);
    }
}

void ArduinoWidget::onPortComboChanged(int /*index*/) {}

void ArduinoWidget::onArduinoConnected(const QString &port)
{
    Q_UNUSED(port)
    updateConnectionUI(true);
    appendLog(QString("=== Connected to %1 ===").arg(port));

    // Push all current UI state to the Teensy on connect
    m_arduino->sendMode(m_modeCombo->currentData().toString());
    m_arduino->sendDivider(m_dividerSpin->value());
    m_arduino->sendTriggerDelay(m_trigDelaySpin->value());
    m_arduino->sendFlashDelay(m_flashDelaySpin->value());
    m_arduino->sendFlashDuration(m_flashDurSpin->value());
    m_arduino->sendFlashCount(m_flashCountSpin->value());
    m_arduino->sendFlashPeriod(m_flashPeriodSpin->value());
}

void ArduinoWidget::onArduinoDisconnected()
{
    updateConnectionUI(false);
    appendLog("=== Disconnected ===");
}

void ArduinoWidget::onArduinoError(const QString &message)
{
    appendLog("!! ERROR: " + message);
    updateConnectionUI(m_arduino->isConnected());
}

// ---------------------------------------------------------------------------
// Slots — mode & timing
// ---------------------------------------------------------------------------

void ArduinoWidget::onModeChanged(int /*index*/)
{
    updateTimingEnabled();
    const QString mode = m_modeCombo->currentData().toString();
    if (m_arduino->isConnected())
    {
        m_arduino->sendMode(mode);
        if (mode == "FREERUN")
            m_arduino->sendStop();
    }
    emit modeChanged(isArduinoMode());
}

void ArduinoWidget::onDividerChanged(int value)
{
    if (m_populating || !m_arduino->isConnected())
        return;
    m_arduino->sendDivider(value);
}

void ArduinoWidget::onTriggerDelayChanged(int value)
{
    if (m_populating || !m_arduino->isConnected())
        return;
    m_arduino->sendTriggerDelay(value);
}

void ArduinoWidget::onFlashDelayChanged(int value)
{
    if (m_populating || !m_arduino->isConnected())
        return;
    m_arduino->sendFlashDelay(value);
}

void ArduinoWidget::onFlashDurationChanged(int value)
{
    if (m_populating || !m_arduino->isConnected())
        return;
    m_arduino->sendFlashDuration(value);
}

void ArduinoWidget::onFlashCountChanged(int value)
{
    if (m_populating || !m_arduino->isConnected())
        return;
    m_flashPeriodSpin->setEnabled(isArduinoMode() &&
                                  m_arduino->isConnected() &&
                                  !m_acquiring &&
                                  value > 1);
    m_arduino->sendFlashCount(value);
}

void ArduinoWidget::onFlashPeriodChanged(int value)
{
    if (m_populating || !m_arduino->isConnected())
        return;
    m_arduino->sendFlashPeriod(value);
}