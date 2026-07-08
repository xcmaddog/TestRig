#pragma once

#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QThread>

#include "printer.h"   // Axis enum

class QTabWidget;
class QLineEdit;
class QPushButton;
class QLabel;
class QDoubleSpinBox;
class QSpinBox;
class QGroupBox;

class GalilController;
class PrintheadWidget;
class CameraController;
class CameraWorker;
class CameraWidget;
class LiveViewWidget;
class StrobeWidget;
class ArduinoController;
class ArduinoWidget;

namespace Added_Scientific { class Controller; }

// ---------------------------------------------------------------------------
// MainWindow
//
// Layout:
//   ┌─────────────────────────────────────────────────────────┐
//   │  Galil connection bar (always visible)                  │
//   ├─────────────────────────────────────────────────────────┤
//   │  QTabWidget                                             │
//   │  ┌──────────┬──────────────┬──────────────────────────┐│
//   │  │  Axes    │  Printhead   │  Camera / Strobe         ││
//   │  └──────────┴──────────────┴──────────────────────────┘│
//   └─────────────────────────────────────────────────────────┘
//
// The Axes tab is built inline here (simple enough not to need its own class).
// The Printhead tab is PrintheadWidget.
// The Camera/Strobe tab contains the widgets from StroboscopicCameraApp.

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

private slots:
    // ── Galil connection bar ──────────────────────────────────────────────
    void onConnectClicked();
    void onGalilConnected();
    void onGalilDisconnected();
    void onGalilStatusUpdated();
    void onGalilError(const QString &message);
    void onAxisIdle(Axis axis);

    // ── Axes tab — X-axis ─────────────────────────────────────────────────
    void xRightPressed();
    void xLeftPressed();
    void xJogReleased();
    void xHomeClicked();
    void xAbsoluteMoveClicked();
    void xEnableClicked();

    // ── Axes tab — D-axis ─────────────────────────────────────────────────
    void dUpPressed();
    void dDownPressed();
    void dJogReleased();
    void dHomeClicked();
    void dAbsoluteMoveClicked();
    void dEnableClicked();

    // ── Camera / strobe ───────────────────────────────────────────────────
    void onCameraOpened();
    void onCameraClosed();
    void onAcquisitionStarted();
    void onAcquisitionStopped();
    void onAcquisitionStartRequested();
    void onAcquisitionStopRequested();
    void onArduinoModeChanged(bool arduinoMode);
    void onCameraError(const QString &message);

private:
    void buildGalilBar();
    void buildAxesTab(QWidget *tab);
    QGroupBox *buildAxisGroup(Axis axis, const QString &title);
    QWidget   *horizontalLine();
    void buildPrintheadTab(QWidget *tab);
    void buildCameraTab(QWidget *tab);

    void updateAxisStatus();
    void setAxesTabEnabled(bool enabled);

    // ── Galil ─────────────────────────────────────────────────────────────
    GalilController *m_galil = nullptr;

    // Connection bar widgets
    QLineEdit   *m_galilAddressEdit = nullptr;
    QPushButton *m_galilConnectBtn  = nullptr;
    QLabel      *m_galilStatusLabel = nullptr;
    QLabel      *m_galilConnDot     = nullptr;

    // ── Axes tab — X ──────────────────────────────────────────────────────
    QPushButton    *m_xRightBtn    = nullptr;
    QPushButton    *m_xLeftBtn     = nullptr;
    QPushButton    *m_xHomeBtn     = nullptr;
    QPushButton    *m_xEnableBtn   = nullptr;
    QPushButton    *m_xAbsMoveBtn  = nullptr;
    QDoubleSpinBox *m_xAbsPosSpin  = nullptr;
    QDoubleSpinBox *m_xJogSpeedSpin = nullptr;
    QLabel         *m_xPosLabel    = nullptr;
    QLabel         *m_xFwdLimLabel = nullptr;
    QLabel         *m_xRevLimLabel = nullptr;
    QLabel         *m_xMovingLabel = nullptr;

    // ── Axes tab — D ──────────────────────────────────────────────────────
    QPushButton    *m_dUpBtn       = nullptr;
    QPushButton    *m_dDownBtn     = nullptr;
    QPushButton    *m_dHomeBtn     = nullptr;
    QPushButton    *m_dEnableBtn   = nullptr;
    QPushButton    *m_dAbsMoveBtn  = nullptr;
    QDoubleSpinBox *m_dAbsPosSpin  = nullptr;
    QDoubleSpinBox *m_dJogSpeedSpin = nullptr;
    QLabel         *m_dPosLabel    = nullptr;
    QLabel         *m_dFwdLimLabel = nullptr;
    QLabel         *m_dRevLimLabel = nullptr;
    QLabel         *m_dMovingLabel = nullptr;

    // ── JetForge / printhead ──────────────────────────────────────────────
    Added_Scientific::Controller *m_mjController = nullptr;
    PrintheadWidget              *m_printheadWidget = nullptr;

    // ── Camera / strobe ───────────────────────────────────────────────────
    CameraController *m_camera       = nullptr;
    CameraWorker     *m_cameraWorker = nullptr;
    QThread          *m_cameraThread = nullptr;

    ArduinoController *m_arduino       = nullptr;

    // Camera tab widgets (owned by the tab, stored here for cross-widget signals)
    CameraWidget   *m_cameraWidget   = nullptr;
    LiveViewWidget *m_liveView       = nullptr;
    StrobeWidget   *m_strobeWidget   = nullptr;
    ArduinoWidget  *m_arduinoWidget  = nullptr;

    // True if IDS peak initialised at least one camera
    bool m_cameraAvailable = false;
};

#endif // MAINWINDOW_H
