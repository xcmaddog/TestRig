#ifndef ARDUINOWIDGET_H
#define ARDUINOWIDGET_H

#pragma once

#include <QWidget>

class QComboBox;
class QSpinBox;
class QPushButton;
class QLabel;
class QPlainTextEdit;
class ArduinoController;

// ---------------------------------------------------------------------------
// ArduinoWidget
//
// Controls panel for the Teensy-mediated trigger/strobe path.
//
// Sections:
//   Connection  — port selector (auto-detected, user-overridable), rescan,
//                 connect/disconnect button, status indicator
//
//   Trigger mode — Free-run  : camera acquires at its own rate; Teensy idle
//                  Arduino   : Teensy watches nFire, sequences camera + strobe
//
//   Timing (Arduino mode only)
//     Divider        — fire camera every Nth nFire edge (both edges counted)
//     Trigger delay  — µs from nFire edge to camera trigger pulse
//     Flash delay    — µs from camera trigger pulse to first strobe pulse
//     Flash duration — µs each strobe pulse stays on
//     Flash count    — number of strobe pulses per camera trigger (≥ 1)
//     Flash period   — µs from start of one pulse to start of next
//                      Must be > flash duration.  Ignored when count = 1.
//
//   Debug log — last N lines received from / sent to the Teensy
// ---------------------------------------------------------------------------
class ArduinoWidget : public QWidget
{
    Q_OBJECT

public:
    explicit ArduinoWidget(ArduinoController *arduino, QWidget *parent = nullptr);

    void setAcquiring(bool acquiring);
    bool isArduinoMode() const;

signals:
    void modeChanged(bool arduinoMode);

public slots:
    void appendLog(const QString &line);

private slots:
    void onRescanClicked();
    void onConnectClicked();
    void onPortComboChanged(int index);
    void onModeChanged(int index);
    void onDividerChanged(int value);
    void onTriggerDelayChanged(int value);
    void onFlashDelayChanged(int value);
    void onFlashDurationChanged(int value);
    void onFlashCountChanged(int value);
    void onFlashPeriodChanged(int value);

    void onArduinoConnected(const QString &port);
    void onArduinoDisconnected();
    void onArduinoError(const QString &message);

private:
    void buildUI();
    void updateConnectionUI(bool connected);
    void updateTimingEnabled();
    void rescanPorts();

    ArduinoController *m_arduino;

    // Connection
    QComboBox *m_portCombo = nullptr;
    QPushButton *m_rescanBtn = nullptr;
    QPushButton *m_connectBtn = nullptr;
    QLabel *m_connIndicator = nullptr;
    QLabel *m_connLabel = nullptr;

    // Mode
    QComboBox *m_modeCombo = nullptr;

    // Timing
    QSpinBox *m_dividerSpin = nullptr;
    QSpinBox *m_trigDelaySpin = nullptr;
    QSpinBox *m_flashDelaySpin = nullptr;
    QSpinBox *m_flashDurSpin = nullptr;
    QSpinBox *m_flashCountSpin = nullptr;
    QSpinBox *m_flashPeriodSpin = nullptr;

    // Derived info label
    QLabel *m_minExposureLabel = nullptr;

    // Debug log
    QPlainTextEdit *m_log = nullptr;

    bool m_acquiring = false;
    bool m_populating = false;

    static constexpr int kMaxLogLines = 200;

    void updateMinExposureLabel();
};

#endif // ARDUINOWIDGET_H