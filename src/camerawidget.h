#ifndef CAMERAWIDGET_H
#define CAMERAWIDGET_H

#pragma once

#include <QWidget>

class QDoubleSpinBox;
class QSpinBox;
class QComboBox;
class QLabel;
class CameraController;

// ---------------------------------------------------------------------------
// CameraWidget
//
// Controls for the basic imaging parameters supported by the UI-3370CP:
//   - Exposure time (µs)
//   - Gain
//   - Pixel format (Mono8 / Mono10 / Mono12)
//   - ROI (offset X/Y, width, height)
//   - Decimation (H and V independently)
//
// Call populate() after the camera opens to read current values and fill
// combo boxes with available entries.  Each control applies its value
// immediately when changed.
//
// Note: auto-exposure, auto-gain, white balance, binning, and mirror/flip
// are not supported by this camera model and are intentionally absent.
// ---------------------------------------------------------------------------
class CameraWidget : public QWidget
{
    Q_OBJECT

public:
    explicit CameraWidget(CameraController* controller, QWidget* parent = nullptr);

    /// Read all current values from the camera and populate the controls.
    /// Call this once after CameraWorker::cameraOpened() is received.
    void populate();

    /// Enable or disable all controls (e.g. disable while acquiring).
    void setControlsEnabled(bool enabled);

private slots:
    void onExposureChanged(double value);
    void onGainChanged(double value);
    void onPixelFormatChanged(int index);
    void onROIChanged();
    void onDecimationHChanged(int index);
    void onDecimationVChanged(int index);

private:
    void buildUI();

    CameraController* m_controller;

    // Exposure
    QDoubleSpinBox* m_exposureSpin  = nullptr;
    QLabel*         m_exposureRange = nullptr;

    // Gain
    QDoubleSpinBox* m_gainSpin      = nullptr;
    QLabel*         m_gainRange     = nullptr;

    // Pixel format
    QComboBox*      m_pixelFmtCombo = nullptr;

    // ROI
    QSpinBox*       m_roiOffsetX    = nullptr;
    QSpinBox*       m_roiOffsetY    = nullptr;
    QSpinBox*       m_roiWidth      = nullptr;
    QSpinBox*       m_roiHeight     = nullptr;

    // Decimation
    QComboBox*      m_decimHCombo   = nullptr;
    QComboBox*      m_decimVCombo   = nullptr;

    // Guard against feedback loops when populate() sets widget values.
    bool m_populating = false;
};

#endif // CAMERAWIDGET_H
