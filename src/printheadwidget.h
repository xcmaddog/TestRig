#pragma once

// printheadwidget.h — JetForge (Added Scientific) printhead control widget.
//
// Adapted from CREATE_LAB_Binder_Jet_Printer MJPrintheadWidget.
//
// ── What was removed ────────────────────────────────────────────────────────
//   • Y and Z axis motion (rig has no Y or Z at this time)
//   • Roller, heat lamp, recoat sequences
//   • STL slicing (QProcess / Python)
//   • Multi-layer full print job management
//   • Encoder position display / timer
//   • Variable test print (batch folder processing)
//   • Multi-head head-gap calculation
//   • createTestBitmaps / checkMaps / moveNozzleOffPlate
//   • PrinterWidget / PrintThread dependency
//
// ── What was kept / adapted ─────────────────────────────────────────────────
//   • Printhead connection, power, voltage, frequency, absolute start
//   • Direct serial command entry and response window
//   • Bitmap file load (QFileDialog) and send to printhead
//   • X-axis jog (uses GalilController directly, no PrintThread)
//   • X-axis home
//   • Single-pass print (immediate mode and encoder mode)
//   • Test jet, purge nozzles, single nozzle, stop printing
//   • Quick purge solenoid (separate from nozzle purge — air pressure)
//
// ── Architecture change ──────────────────────────────────────────────────────
//   The original widget used emit execute_command() → PrintThread for motion.
//   This version calls GalilController::downloadAndRun() directly for compound
//   motion programs and GalilController::jog() / stopMotion() for jogging.
//   A member bool m_motionComplete is set by the axisIdle(Axis::X) signal and
//   read in QCoreApplication::processEvents() busy-wait loops, which is the
//   same pattern the original used (atLocation / printComplete globals).

#ifndef PRINTHEADWIDGET_H
#define PRINTHEADWIDGET_H

#include <QWidget>
#include <QStringList>

class QPlainTextEdit;
class QLineEdit;
class QPushButton;
class QSpinBox;
class QDoubleSpinBox;
class QGroupBox;
class QLabel;
class QCheckBox;

class GalilController;

namespace Added_Scientific { class Controller; }

class PrintheadWidget : public QWidget
{
    Q_OBJECT

public:
    explicit PrintheadWidget(GalilController *galil,
                             Added_Scientific::Controller *mjController,
                             QWidget *parent = nullptr);
    ~PrintheadWidget() override;

public slots:
    // Called by MainWindow when the Galil connects/disconnects.
    void onGalilConnected();
    void onGalilDisconnected();

private slots:
    // ── Printhead connection / power ───────────────────────────────────────
    void connectToJetForge();
    void powerTogglePressed();
    void stopPrintingPressed();

    // ── Parameter setters (applied when spin box editing finishes) ─────────
    void frequencyChanged();
    void voltageChanged();
    void absoluteStartChanged();

    // ── Bitmap / print ─────────────────────────────────────────────────────
    void browseForBitmap();
    void fileNameEntered();
    void testJetPressed();
    void purgeNozzles();
    void singleNozzlePressed();
    void createBitmapPressed();

    // ── Quick purge (solenoid / air pressure valve) ────────────────────────
    void quickPurgePressed();

    // ── Serial command window ──────────────────────────────────────────────
    void commandEntered();
    void clearResponseText();
    void writeToResponseWindow(const QString &text);

    // ── X-axis motion ──────────────────────────────────────────────────────
    void xRightPressed();
    void xLeftPressed();
    void xJogReleased();
    void xHomeClicked();
    void getXPosition();

    // ── Motion complete (connected to GalilController::axisIdle) ──────────
    void onAxisIdle(Axis axis);

private:
    void buildUI();
    void setJetForgeControlsEnabled(bool enabled);
    void setMotionControlsEnabled(bool enabled);

    void sendCommand(const QString &command);

    // Motion helpers (X-axis only)
    void moveToX(double xMm, const QString &endMessage);
    void printImmediate(double accel, double speed, double endMm, const QString &endMessage);
    void printEncoder(double accel, double speed, double endMm, const QString &endMessage);

    void readInFile(const QString &filePath, int headIdx = 1);

    // ── Owned by MainWindow, passed in ───────────────────────────────────
    GalilController              *m_galil;
    Added_Scientific::Controller *m_mj;

    // ── Motion state ─────────────────────────────────────────────────────
    bool m_motionComplete = false;   // set by onAxisIdle(); read by busy-waits

    // ── UI widgets ────────────────────────────────────────────────────────

    // JetForge connection
    QLineEdit      *m_portLineEdit   = nullptr;
    QPushButton    *m_connectBtn     = nullptr;
    QLabel         *m_connStatus     = nullptr;

    // Power / stop
    QPushButton    *m_powerBtn       = nullptr;
    QPushButton    *m_stopBtn        = nullptr;

    // Parameters
    QSpinBox       *m_freqSpin       = nullptr;   // Hz
    QDoubleSpinBox *m_voltageSpin    = nullptr;   // V
    QSpinBox       *m_startSpin      = nullptr;   // encoder counts

    // Bitmap
    QLineEdit      *m_fileLineEdit   = nullptr;
    QPushButton    *m_browseBtn      = nullptr;

    // Test/utility
    QPushButton    *m_testJetBtn     = nullptr;
    QPushButton    *m_purgeBtn       = nullptr;
    QPushButton    *m_singleNozzleBtn = nullptr;
    QSpinBox       *m_nozzleNumSpin  = nullptr;
    QPushButton    *m_createBitmapBtn = nullptr;
    QSpinBox       *m_numLinesSpin   = nullptr;
    QSpinBox       *m_pixelWidthSpin = nullptr;

    // Quick purge (solenoid)
    QPushButton    *m_quickPurgeBtn  = nullptr;
    QSpinBox       *m_purgeMsSpin    = nullptr;   // pulse duration ms
    QCheckBox      *m_useExtIOCheck  = nullptr;   // use ext I/O bit instead of DO1

    // X-axis motion
    QPushButton    *m_xRightBtn      = nullptr;
    QPushButton    *m_xLeftBtn       = nullptr;
    QPushButton    *m_xHomeBtn       = nullptr;
    QPushButton    *m_xGetPosBtn     = nullptr;
    QDoubleSpinBox *m_xVelocitySpin  = nullptr;  // mm/s for jogging

    // Serial command entry + response
    QLineEdit       *m_commandLineEdit = nullptr;
    QPushButton     *m_clearBtn        = nullptr;
    QPlainTextEdit  *m_responseTextEdit = nullptr;

    // Status
    QPushButton    *m_getStatusBtn     = nullptr;
};

#endif // PRINTHEADWIDGET_H
