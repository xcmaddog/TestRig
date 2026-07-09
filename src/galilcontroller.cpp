#include "galilcontroller.h"

#include <gclib.h>
#include <gclibo.h>

#include <cstring>
#include <sstream>

// ---------------------------------------------------------------------------
// Construction / destruction
// ---------------------------------------------------------------------------

GalilController::GalilController(QObject *parent)
    : QObject(parent)
    , m_pollTimer(new QTimer(this))
{
    QObject::connect(m_pollTimer,
                     &QTimer::timeout,
                     this,
                     &GalilController::pollStatus);
}

GalilController::~GalilController()
{
    disconnect();
}

// ---------------------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------------------

char GalilController::axisChar(Axis axis) const
{
    switch (axis)
    {
    case Axis::X:  return 'A';
    case Axis::D:  return 'D';
    default:       return 'A';
    }
}

long GalilController::toSteps(Axis axis, double mm) const
{
    switch (axis)
    {
    case Axis::X:  return static_cast<long>(mm * X_CNTS_PER_MM);
    case Axis::D:  return static_cast<long>(mm * D_CNTS_PER_MM);
    default:       return 0;
    }
}

double GalilController::toMm(Axis axis, long steps) const
{
    switch (axis)
    {
    case Axis::X:  return steps / X_CNTS_PER_MM;
    case Axis::D:  return steps / D_CNTS_PER_MM;
    default:       return 0.0;
    }
}

double GalilController::queryDouble(const QString &mgExpr) const
{
    if (!m_handle) return 0.0;
    char buf[BUF_SIZE] = {};
    GReturn rc = GCommand(m_handle,
                          ("MG " + mgExpr).toLocal8Bit().constData(),
                          buf, sizeof(buf), nullptr);
    if (rc != G_NO_ERROR) return 0.0;
    return QString(buf).trimmed().toDouble();
}

void GalilController::emitError(const QString &context, GReturn rc)
{
    char errBuf[BUF_SIZE] = {};
    GError(rc, errBuf, sizeof(errBuf));
    m_lastError = QString("%1: %2").arg(context, errBuf);
    emit errorOccurred(m_lastError);
}

// ---------------------------------------------------------------------------
// Connection
// ---------------------------------------------------------------------------

bool GalilController::connect(const QString &address)
{
    if (m_handle) disconnect();

    GReturn rc = GOpen(address.toLocal8Bit().constData(), &m_handle);
    if (rc != G_NO_ERROR)
    {
        emitError("GOpen", rc);
        m_handle = nullptr;
        return false;
    }

    m_lastError.clear();
    m_pollTimer->start(POLL_INTERVAL_MS);
    emit connected();
    return true;
}

void GalilController::disconnect()
{
    if (!m_handle) return;
    m_pollTimer->stop();
    GClose(m_handle);
    m_handle = nullptr;
    emit disconnected();
}

// ---------------------------------------------------------------------------
// Raw command interface
// ---------------------------------------------------------------------------

bool GalilController::sendCommand(const QString &cmd, QString *response)
{
    if (!m_handle)
    {
        m_lastError = "Not connected.";
        return false;
    }

    char buf[BUF_SIZE] = {};
    GReturn rc = GCommand(m_handle,
                          cmd.toLocal8Bit().constData(),
                          buf, sizeof(buf), nullptr);
    if (rc != G_NO_ERROR)
    {
        emitError(QString("Command \"%1\"").arg(cmd), rc);
        return false;
    }

    if (response)
        *response = QString(buf).trimmed();

    m_lastError.clear();
    return true;
}

bool GalilController::sendCommand(const std::string &cmd)
{
    return sendCommand(QString::fromStdString(cmd));
}

// ---------------------------------------------------------------------------
// Motor enable / disable
// ---------------------------------------------------------------------------

bool GalilController::enableMotor(Axis axis)
{
    return sendCommand(QString("SH%1").arg(axisChar(axis)));
}

bool GalilController::disableMotor(Axis axis)
{
    return sendCommand(QString("MO%1").arg(axisChar(axis)));
}

bool GalilController::isMotorEnabled(Axis axis)
{
    // _MOA (or _MOD) returns 1 when the motor is OFF (servo off).
    double val = queryDouble(QString("_MO%1").arg(axisChar(axis)));
    return (val == 0.0);
}

// ---------------------------------------------------------------------------
// Motion parameters
// ---------------------------------------------------------------------------

bool GalilController::setSpeed(Axis axis, double mmPerSec)
{
    return sendCommand(QString("SP%1=%2")
                           .arg(axisChar(axis))
                           .arg(toSteps(axis, mmPerSec)));
}

bool GalilController::setAcceleration(Axis axis, double mmPerSecSq)
{
    return sendCommand(QString("AC%1=%2")
                           .arg(axisChar(axis))
                           .arg(toSteps(axis, mmPerSecSq)));
}

bool GalilController::setDeceleration(Axis axis, double mmPerSecSq)
{
    return sendCommand(QString("DC%1=%2")
                           .arg(axisChar(axis))
                           .arg(toSteps(axis, mmPerSecSq)));
}

// ---------------------------------------------------------------------------
// Single-command motion
// ---------------------------------------------------------------------------

bool GalilController::moveRelative(Axis axis, double mm)
{
    if (!sendCommand(QString("PR%1=%2").arg(axisChar(axis)).arg(toSteps(axis, mm))))
        return false;
    return sendCommand(QString("BG%1").arg(axisChar(axis)));
}

bool GalilController::moveAbsolute(Axis axis, double mm)
{
    if (!sendCommand(QString("PA%1=%2").arg(axisChar(axis)).arg(toSteps(axis, mm))))
        return false;
    return sendCommand(QString("BG%1").arg(axisChar(axis)));
}

bool GalilController::jog(Axis axis, double mmPerSec)
{
    if (!sendCommand(QString("JG%1=%2").arg(axisChar(axis)).arg(toSteps(axis, mmPerSec))))
        return false;
    return sendCommand(QString("BG%1").arg(axisChar(axis)));
}

bool GalilController::stopMotion(Axis axis)
{
    return sendCommand(QString("ST%1").arg(axisChar(axis)));
}

bool GalilController::abortMotion()
{
    return sendCommand(QStringLiteral("AB"));
}

bool GalilController::definePositionZero(Axis axis)
{
    return sendCommand(QString("DP%1=0").arg(axisChar(axis)));
}

// ---------------------------------------------------------------------------
// Homing — FE-based (find edge on reverse limit switch)
// ---------------------------------------------------------------------------

bool GalilController::home(Axis axis, double homeSpeed_mm_s)
{
    // 1. Set a slow negative jog speed toward the reverse limit.
    if (!sendCommand(QString("JG%1=%2").arg(axisChar(axis)).arg(-toSteps(axis, homeSpeed_mm_s))))
        return false;

    // 2. FE: jog until reverse limit trips, then stop automatically.
    if (!sendCommand(QString("FE%1").arg(axisChar(axis))))
        return false;

    // 3. Begin motion.  The polling timer will detect idle and emit axisIdle().
    //    After that, the widget (or whoever called home()) should call
    //    definePositionZero() to set the limit position as 0.
    return sendCommand(QString("BG%1").arg(axisChar(axis)));
}

// ---------------------------------------------------------------------------
// Compound motion programs
// ---------------------------------------------------------------------------

bool GalilController::downloadAndRun(const std::string &dmcProgram)
{
    if (!m_handle)
    {
        m_lastError = "Not connected.";
        return false;
    }

    // Abort any running program / motion before downloading.
    GCommand(m_handle, "AB", nullptr, 0, nullptr);

    GReturn rc = GProgramDownload(m_handle, dmcProgram.c_str(), "");
    if (rc != G_NO_ERROR)
    {
        emitError("GProgramDownload", rc);
        return false;
    }

    rc = GCommand(m_handle, "XQ", nullptr, 0, nullptr);
    if (rc != G_NO_ERROR)
    {
        emitError("XQ", rc);
        return false;
    }

    m_lastError.clear();
    return true;
}

// ---------------------------------------------------------------------------
// Solenoid
// ---------------------------------------------------------------------------

bool GalilController::setSolenoid(bool on)
{
    return sendCommand(on ? QString("SB %1").arg(SOLENOID_BIT)
                           : QString("CB %1").arg(SOLENOID_BIT));
}

bool GalilController::quickPurge(int pulseMs)
{
    // Build a tiny DMC program:  SB → WT → CB
    std::stringstream s;
    s << CMD::set_bit(SOLENOID_BIT);
    s << CMD::sleep(pulseMs);
    s << CMD::clear_bit(SOLENOID_BIT);
    return downloadAndRun(CMD::cmd_buf_to_dmc(s));
}

// ---------------------------------------------------------------------------
// State queries
// ---------------------------------------------------------------------------

double GalilController::position(Axis axis) const
{
    long steps = static_cast<long>(queryDouble(QString("_TP%1").arg(axisChar(axis))));
    return toMm(axis, steps);
}

bool GalilController::isMoving(Axis axis) const
{
    return (queryDouble(QString("_BG%1").arg(axisChar(axis))) != 0.0);
}

bool GalilController::isForwardLimitActive(Axis axis) const
{
    // _LF returns 0 when the forward limit is tripped.
    return (queryDouble(QString("_LF%1").arg(axisChar(axis))) == 0.0);
}

bool GalilController::isReverseLimitActive(Axis axis) const
{
    // _LR returns 0 when the reverse limit is tripped.
    return (queryDouble(QString("_LR%1").arg(axisChar(axis))) == 0.0);
}

// ---------------------------------------------------------------------------
// Status polling
// ---------------------------------------------------------------------------

void GalilController::pollStatus()
{
    if (!m_handle) return;

    // ── X axis ─────────────────────────────────────────────────────────────
    {
        long   rawPos = static_cast<long>(queryDouble("_TPA"));
        bool   nowMoving  = (queryDouble("_BGA") != 0.0);
        bool   fwdLim     = (queryDouble("_LFA") == 0.0);
        bool   revLim     = (queryDouble("_LRA") == 0.0);

        m_posX     = toMm(Axis::X, rawPos);
        m_fwdLimX  = fwdLim;
        m_revLimX  = revLim;

        if (m_movingX && !nowMoving)
            emit axisIdle(Axis::X);

        m_movingX = nowMoving;
    }

    // ── D axis ─────────────────────────────────────────────────────────────
    {
        long   rawPos = static_cast<long>(queryDouble("_TPD"));
        bool   nowMoving  = (queryDouble("_BGD") != 0.0);
        bool   fwdLim     = (queryDouble("_LFD") == 0.0);
        bool   revLim     = (queryDouble("_LRD") == 0.0);

        m_posD     = toMm(Axis::D, rawPos);
        m_fwdLimD  = fwdLim;
        m_revLimD  = revLim;

        if (m_movingD && !nowMoving)
            emit axisIdle(Axis::D);

        m_movingD = nowMoving;
    }

    emit statusUpdated();
}
