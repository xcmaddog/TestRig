#pragma once

// galilcontroller.h — Qt wrapper around gclib for the test rig DMC-4080.
//
// Extended from the original TestRig single-axis GalilController to support
// two independent axes (X and D) and compound DMC program download/run.
//
// ── Axis mapping ──────────────────────────────────────────────────────────
//   Axis::X  →  Galil axis A (X carriage)
//   Axis::D  →  Galil axis D (reservoir / ink bottle height)
//
// ── Thread safety ─────────────────────────────────────────────────────────
//   All public methods must be called from the GUI thread.
//   The polling timer runs on the GUI thread (QTimer, not a QThread).
//   downloadAndRun() calls GProgramDownload + GCmd("XQ") synchronously;
//   these return quickly (< 1 ms).  GProgramComplete is NOT called here —
//   the polling timer detects motion end and emits axisIdle().

#ifndef GALILCONTROLLER_H
#define GALILCONTROLLER_H

#include <QObject>
#include <QTimer>
#include <QString>

#include <gclib.h>

#include "printer.h"   // Axis enum

class GalilController : public QObject
{
    Q_OBJECT

public:
    explicit GalilController(QObject *parent = nullptr);
    ~GalilController() override;

    // ── Connection ─────────────────────────────────────────────────────────
    //
    // address: gclib connection string, e.g. "192.168.42.100 -d"
    //   The "-d" flag enables direct (non-timeout) communication.
    //   See the gclib docs for other options.

    bool connect(const QString &address);
    void disconnect();
    bool isConnected() const { return m_handle != nullptr; }

    // ── Motor enable / disable ────────────────────────────────────────────

    bool enableMotor(Axis axis);
    bool disableMotor(Axis axis);
    bool isMotorEnabled(Axis axis);

    // ── Motion parameters ─────────────────────────────────────────────────

    bool setSpeed(Axis axis, double mmPerSec);
    bool setAcceleration(Axis axis, double mmPerSecSq);
    bool setDeceleration(Axis axis, double mmPerSecSq);

    // ── Single-command motion ─────────────────────────────────────────────
    //
    // These issue Galil commands directly and return immediately.  The Galil
    // runs the motion asynchronously; watch axisIdle() or poll isMoving()
    // to know when it finishes.

    bool moveRelative(Axis axis, double mm);
    bool moveAbsolute(Axis axis, double mm);
    bool jog(Axis axis, double mmPerSec);    // JG + BG; negative = reverse
    bool stopMotion(Axis axis);              // decelerated stop (ST)
    bool abortMotion();                      // immediate all-axis stop (AB)
    bool definePositionZero(Axis axis);      // DP=0

    // ── Homing ───────────────────────────────────────────────────────────
    //
    // Jogs toward the reverse limit at homeSpeed_mm_s (negative direction),
    // waits for the reverse limit to trip (FE), then defines that position
    // as zero.  Neither axis has a separate home switch, so FE-based homing
    // is the correct approach.
    //
    // Runs asynchronously — watch axisIdle(axis) for completion.

    bool home(Axis axis, double homeSpeed_mm_s = 5.0);

    // ── Compound motion programs ─────────────────────────────────────────
    //
    // Downloads a raw DMC program string (output of CMD::cmd_buf_to_dmc())
    // to the controller and executes it with XQ.  Any currently running
    // program is aborted first.
    //
    // Returns false if download or XQ fails.  Motion runs asynchronously;
    // watch axisIdle() or isMoving() to detect completion.

    bool downloadAndRun(const std::string &dmcProgram);

    // ── Solenoid ─────────────────────────────────────────────────────────
    //
    // setSolenoid: turns a digital output on (SB) or off (CB).
    // quickPurse:  downloads a DMC program that does SB → WT(ms) → CB,
    //              so the pulse timing is handled on the controller.
    //
    // useExtIO: if true, uses SOLENOID_EXTIO_BIT (extended I/O bit 30);
    //           if false, uses SOLENOID_DO1_BIT (high-power output DO1).

    bool setSolenoid(bool on, bool useExtIO = false);
    bool quickPurse(int pulseMs, bool useExtIO = false);

    // ── Raw command interface ────────────────────────────────────────────
    //
    // Used by PrintheadWidget for CMD strings that are simpler to send
    // directly than to build a full program for.

    bool sendCommand(const QString &cmd, QString *response = nullptr);
    bool sendCommand(const std::string &cmd);

    // ── State queries ─────────────────────────────────────────────────────

    double position(Axis axis) const;           // mm
    bool   isMoving(Axis axis) const;
    bool   isForwardLimitActive(Axis axis) const;
    bool   isReverseLimitActive(Axis axis) const;

    QString lastError() const { return m_lastError; }

    // ── Raw handle ────────────────────────────────────────────────────────
    //
    // Exposed for the rare cases where a caller needs to call gclib functions
    // directly (e.g. GProgramDownload in the printhead widget).
    // Do not cache this pointer across connect/disconnect cycles.

    GCon gcHandle() const { return m_handle; }

signals:
    void connected();
    void disconnected();
    void statusUpdated();                   // emitted each poll cycle
    void axisIdle(Axis axis);              // axis transitioned moving → idle
    void errorOccurred(const QString &message);

private slots:
    void pollStatus();

private:
    // helpers
    char        axisChar(Axis axis) const;
    long        toSteps(Axis axis, double mm) const;
    double      toMm(Axis axis, long steps) const;
    double      queryDouble(const QString &mgExpr) const;
    void        emitError(const QString &context, GReturn rc);

    GCon    m_handle     = nullptr;
    QTimer *m_pollTimer  = nullptr;

    // Cached state updated by pollStatus()
    double m_posX  = 0.0,   m_posD  = 0.0;
    bool   m_movingX = false, m_movingD = false;
    bool   m_fwdLimX = false, m_revLimX = false;
    bool   m_fwdLimD = false, m_revLimD = false;

    QString m_lastError;

    static constexpr int POLL_INTERVAL_MS = 100;
    static constexpr unsigned int BUF_SIZE = 1024;
};

#endif // GALILCONTROLLER_H
