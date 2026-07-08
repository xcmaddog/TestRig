#ifndef STROBEWIDGET_H
#define STROBEWIDGET_H

#pragma once

#include <QWidget>

class QPushButton;
class QLabel;
class CameraController;

// ---------------------------------------------------------------------------
// StrobeWidget
//
// Minimal acquisition-control panel.  All trigger/strobe timing parameters
// have moved to ArduinoWidget.  This widget owns only the Start/Stop button
// and the acquisition status indicator.
//
// The camera is placed in hardware trigger mode (Line0) when Arduino mode is
// active, and in free-run mode otherwise.  MainWindow coordinates this based
// on ArduinoWidget::modeChanged().
// ---------------------------------------------------------------------------
class StrobeWidget : public QWidget
{
    Q_OBJECT

public:
    explicit StrobeWidget(CameraController *controller, QWidget *parent = nullptr);

    /// Reflect that acquisition has started (update button + indicator).
    void onAcquisitionStarted();

    /// Reflect that acquisition has stopped.
    void onAcquisitionStopped();

    /// Enable/disable the start/stop button (e.g. while camera is opening).
    void setStartStopEnabled(bool enabled);

signals:
    void acquisitionStartRequested();
    void acquisitionStopRequested();

private slots:
    void onStartStopClicked();

private:
    void buildUI();

    CameraController *m_controller;
    bool m_acquiring = false;

    QPushButton *m_startStopBtn = nullptr;
    QLabel *m_statusIndicator = nullptr;
};

#endif // STROBEWIDGET_H