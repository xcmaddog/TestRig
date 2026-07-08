#ifndef ARDUINOCONTROLLER_H
#define ARDUINOCONTROLLER_H

#pragma once

#include <QObject>
#include <QStringList>

class QSerialPort;

// ---------------------------------------------------------------------------
// ArduinoController
//
// Owns a QSerialPort connection to the Teensy 4.0 and exposes a clean
// command interface.  All commands use a simple ASCII protocol:
//
//   Host  →  Teensy :  "KEY VALUE\n"   (e.g. "FLASH_DELAY 200\n")
//   Teensy → Host   :  "OK KEY VALUE\n"  on success
//                       "ERR MESSAGE\n"   on failure
//
// Full command set:
//   MODE FREERUN      — Teensy stops triggering; ignores nFire
//   MODE ARDUINO      — Teensy starts watching nFire edges
//   START             — Begin triggering (Arduino mode)
//   STOP              — Stop triggering  (sent on stop-acquisition or freerun)
//   DIVIDER N         — Fire camera every Nth nFire edge (N ≥ 1)
//   TRIG_DELAY us     — µs from nFire edge → camera trigger pulse
//   FLASH_DELAY us    — µs from camera trigger → first strobe pulse on
//   FLASH_DUR us      — µs each strobe pulse stays on
//   FLASH_COUNT N     — Number of strobe pulses per camera trigger (N ≥ 1)
//   FLASH_PERIOD us   — µs from start of one strobe pulse to start of next
//                       Must be > FLASH_DUR.  Ignored when FLASH_COUNT = 1.
//
// Auto-detection: call detectTeensys() to get a list of candidate port
// names, then connectToPort() to open one.  The port stays open until
// the object is destroyed or disconnectPort() is called.
//
// All methods are safe to call from the GUI thread.
// ---------------------------------------------------------------------------
class ArduinoController : public QObject
{
    Q_OBJECT

public:
    explicit ArduinoController(QObject *parent = nullptr);
    ~ArduinoController() override;

    // ------------------------------------------------------------------
    // Connection
    // ------------------------------------------------------------------

    /// Scan all serial ports and return names of ports that look like
    /// Teensy devices (description or manufacturer contains "teensy"
    /// or "pjrc").  Never blocks.
    static QStringList detectTeensys();

    /// Open the named port at 115200 8N1.
    /// Emits connected() on success, errorOccurred() on failure.
    void connectToPort(const QString &portName);

    /// Close the serial port.  Emits disconnected().
    void disconnectPort();

    bool isConnected() const;
    QString currentPort() const;

    // ------------------------------------------------------------------
    // Commands  (all emit commandSent; ACK/ERR come back via signals)
    // ------------------------------------------------------------------

    void sendMode(const QString &mode); // "FREERUN" or "ARDUINO"
    void sendStart();
    void sendStop();
    void sendDivider(int n);
    void sendTriggerDelay(int microseconds);
    void sendFlashDelay(int microseconds);
    void sendFlashDuration(int microseconds);
    void sendFlashCount(int n);             // pulses per trigger event
    void sendFlashPeriod(int microseconds); // µs from pulse start to next pulse start

signals:
    /// Port opened successfully.
    void connected(const QString &portName);

    /// Port closed (intentionally or due to error).
    void disconnected();

    /// A raw line was sent to the Teensy.
    void commandSent(const QString &line);

    /// Teensy replied OK — key and value as parsed.
    void ackReceived(const QString &key, const QString &value);

    /// Teensy replied ERR.
    void errReceived(const QString &message);

    /// Any unrecognised line from the Teensy (for debug log).
    void rawLineReceived(const QString &line);

    /// Emitted on port open/read/write errors.
    void errorOccurred(const QString &message);

private slots:
    void onReadyRead();
    void onSerialError(int error);

private:
    void sendLine(const QString &line);
    void parseLine(const QString &line);

    QSerialPort *m_port = nullptr;
    QString m_buffer;
};

#endif // ARDUINOCONTROLLER_H