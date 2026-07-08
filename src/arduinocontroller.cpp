#include "arduinocontroller.h"

#include <QSerialPort>
#include <QSerialPortInfo>

// ---------------------------------------------------------------------------
// Construction / destruction
// ---------------------------------------------------------------------------

ArduinoController::ArduinoController(QObject *parent)
    : QObject(parent), m_port(new QSerialPort(this))
{
    connect(m_port, &QSerialPort::readyRead,
            this, &ArduinoController::onReadyRead);

    connect(m_port,
            QOverload<QSerialPort::SerialPortError>::of(&QSerialPort::errorOccurred),
            this, [this](QSerialPort::SerialPortError e)
            { onSerialError(static_cast<int>(e)); });
}

ArduinoController::~ArduinoController()
{
    disconnectPort();
}

// ---------------------------------------------------------------------------
// Static detection
// ---------------------------------------------------------------------------

QStringList ArduinoController::detectTeensys()
{
    QStringList found;
    const auto ports = QSerialPortInfo::availablePorts();
    for (const QSerialPortInfo &info : ports)
    {
        const QString desc = info.description().toLower();
        const QString mfr = info.manufacturer().toLower();

        // Teensy 4.0 enumerates with PJRC's USB VID (0x16C0).
        // The description and manufacturer strings vary by OS and driver,
        // but typically contain "teensy" or "pjrc".
        if (desc.contains("teensy") || mfr.contains("teensy") ||
            desc.contains("pjrc") || mfr.contains("pjrc") ||
            info.vendorIdentifier() == 0x16C0)
        {
            found << info.portName();
        }
    }
    return found;
}

// ---------------------------------------------------------------------------
// Connection management
// ---------------------------------------------------------------------------

void ArduinoController::connectToPort(const QString &portName)
{
    if (m_port->isOpen())
        m_port->close();

    m_port->setPortName(portName);
    m_port->setBaudRate(QSerialPort::Baud115200);
    m_port->setDataBits(QSerialPort::Data8);
    m_port->setParity(QSerialPort::NoParity);
    m_port->setStopBits(QSerialPort::OneStop);
    m_port->setFlowControl(QSerialPort::NoFlowControl);

    if (!m_port->open(QIODevice::ReadWrite))
    {
        emit errorOccurred(QString("Failed to open %1: %2")
                               .arg(portName, m_port->errorString()));
        return;
    }

    m_buffer.clear();
    emit connected(portName);
}

void ArduinoController::disconnectPort()
{
    if (m_port->isOpen())
    {
        m_port->close();
        emit disconnected();
    }
}

bool ArduinoController::isConnected() const
{
    return m_port->isOpen();
}

QString ArduinoController::currentPort() const
{
    return m_port->portName();
}

// ---------------------------------------------------------------------------
// Commands
// ---------------------------------------------------------------------------

void ArduinoController::sendMode(const QString &mode)
{
    sendLine(QString("MODE %1").arg(mode.toUpper()));
}

void ArduinoController::sendStart()
{
    sendLine("START");
}

void ArduinoController::sendStop()
{
    sendLine("STOP");
}

void ArduinoController::sendDivider(int n)
{
    sendLine(QString("DIVIDER %1").arg(n));
}

void ArduinoController::sendTriggerDelay(int microseconds)
{
    sendLine(QString("TRIG_DELAY %1").arg(microseconds));
}

void ArduinoController::sendFlashDelay(int microseconds)
{
    sendLine(QString("FLASH_DELAY %1").arg(microseconds));
}

void ArduinoController::sendFlashDuration(int microseconds)
{
    sendLine(QString("FLASH_DUR %1").arg(microseconds));
}

void ArduinoController::sendFlashCount(int n)
{
    sendLine(QString("FLASH_COUNT %1").arg(n));
}

void ArduinoController::sendFlashPeriod(int microseconds)
{
    sendLine(QString("FLASH_PERIOD %1").arg(microseconds));
}

// ---------------------------------------------------------------------------
// Private helpers
// ---------------------------------------------------------------------------

void ArduinoController::sendLine(const QString &line)
{
    if (!m_port->isOpen())
    {
        emit errorOccurred("Cannot send — port not open: " + line);
        return;
    }
    const QByteArray data = (line + "\n").toUtf8();
    m_port->write(data);
    emit commandSent(line);
}

void ArduinoController::onReadyRead()
{
    m_buffer += QString::fromUtf8(m_port->readAll());

    int idx;
    while ((idx = m_buffer.indexOf('\n')) != -1)
    {
        QString line = m_buffer.left(idx).trimmed();
        m_buffer.remove(0, idx + 1);
        if (!line.isEmpty())
            parseLine(line);
    }
}

void ArduinoController::parseLine(const QString &line)
{
    emit rawLineReceived(line);

    if (line.startsWith("OK "))
    {
        const QString rest = line.mid(3).trimmed();
        const int space = rest.indexOf(' ');
        if (space == -1)
            emit ackReceived(rest, QString());
        else
            emit ackReceived(rest.left(space), rest.mid(space + 1).trimmed());
    }
    else if (line.startsWith("ERR "))
    {
        emit errReceived(line.mid(4).trimmed());
    }
    // else: startup banner or debug — rawLineReceived already fired
}

void ArduinoController::onSerialError(int error)
{
    if (error == static_cast<int>(QSerialPort::NoError))
        return;

    emit errorOccurred(QString("Serial port error (%1): %2")
                           .arg(error)
                           .arg(m_port->errorString()));

    if (!m_port->isOpen())
        emit disconnected();
}