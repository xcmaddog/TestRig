#include "cameraworker.h"
#include "cameracontroller.h"
#include <QCoreApplication>

CameraWorker::CameraWorker(CameraController* controller, QObject* parent)
    : QObject(parent)
    , m_controller(controller)
{}

CameraWorker::~CameraWorker()
{
    stopAcquisition();
}

void CameraWorker::openCamera()
{
    if (!m_controller->open()) {
        emit error(QString::fromStdString(m_controller->errorMessage()));
        return;
    }
    emit cameraOpened();
}

void CameraWorker::closeCamera()
{
    if (m_running)
        stopAcquisition();
    m_controller->close();
    emit cameraClosed();
}

void CameraWorker::startAcquisition()
{
    if (m_running)
        return;

    m_controller->startAcquisition();
    if (!m_controller->isAcquiring()) {
        emit error(QString::fromStdString(m_controller->errorMessage()));
        return;
    }

    m_running     = true;
    m_errorFired  = false;   // reset for this acquisition run
    emit acquisitionStarted();

    while (m_running) {
        QCoreApplication::processEvents();

        QImage frame = m_controller->acquireFrame(200);
        if (!frame.isNull()) {
            emit frameReady(std::move(frame));
        } else if (!m_errorFired) {
            // Only emit the first error — then stop the loop so the GUI
            // can recover cleanly without being spammed.
            const std::string msg = m_controller->errorMessage();
            if (!msg.empty()) {
                m_errorFired = true;
                m_running    = false;
                emit error(QString::fromStdString(msg));
            }
            // If msg is empty it was just a timeout (trigger never came) —
            // keep looping silently.
        }
    }

    m_controller->stopAcquisition();
    emit acquisitionStopped();
}

void CameraWorker::stopAcquisition()
{
    m_running = false;
}
