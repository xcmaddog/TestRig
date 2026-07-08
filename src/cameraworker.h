#ifndef CAMERAWORKER_H
#define CAMERAWORKER_H

#pragma once

#include <QObject>
#include <QImage>
#include <atomic>

class CameraController;

// ---------------------------------------------------------------------------
// CameraWorker
//
// Lives on a dedicated QThread (see MainWindow).  MainWindow creates the
// thread, moves this object onto it, and connects signals/slots.
//
// Acquisition loop:
//   MainWindow calls startAcquisition() via a queued connection.
//   The worker calls CameraController::acquireFrame() in a tight loop and
//   emits frameReady() for each frame.  The loop exits when
//   stopAcquisition() sets m_running = false.
//
// All CameraController calls happen on the worker thread — never on the
// GUI thread.
// ---------------------------------------------------------------------------
class CameraWorker : public QObject
{
    Q_OBJECT

public:
    explicit CameraWorker(CameraController* controller, QObject* parent = nullptr);
    ~CameraWorker() override;

public slots:
    /// Open the camera.  Emits cameraOpened() or error().
    void openCamera();

    /// Close the camera.  Safe to call even if not open.
    void closeCamera();

    /// Begin the acquisition loop.  Emits frameReady() for each frame.
    void startAcquisition();

    /// Signal the acquisition loop to stop.  Returns immediately;
    /// the loop exits asynchronously.
    void stopAcquisition();

signals:
    void cameraOpened();
    void cameraClosed();
    void acquisitionStarted();
    void acquisitionStopped();

    /// Emitted for every successfully decoded frame.
    void frameReady(QImage image);

    /// Emitted when any camera or SDK error occurs.
    void error(QString message);

private:
    CameraController* m_controller;   // owned by MainWindow, not this object
    std::atomic<bool> m_running{false};
    std::atomic<bool> m_errorFired{false};
};

#endif // CAMERAWORKER_H
