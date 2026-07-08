#include "strobewidget.h"
#include "cameracontroller.h"

#include <QPushButton>
#include <QLabel>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QVBoxLayout>

StrobeWidget::StrobeWidget(CameraController *controller, QWidget *parent)
    : QWidget(parent), m_controller(controller)
{
    buildUI();
}

void StrobeWidget::buildUI()
{
    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);

    auto *grp = new QGroupBox("Acquisition", this);
    auto *hbox = new QHBoxLayout(grp);

    m_statusIndicator = new QLabel(this);
    m_statusIndicator->setFixedSize(16, 16);
    m_statusIndicator->setStyleSheet(
        "background-color: #555; border-radius: 8px;");
    m_statusIndicator->setToolTip("Grey = stopped, Green = acquiring");

    m_startStopBtn = new QPushButton("Start Acquisition", this);
    m_startStopBtn->setMinimumHeight(32);
    m_startStopBtn->setEnabled(false); // enabled once camera opens

    hbox->addWidget(m_statusIndicator);
    hbox->addWidget(m_startStopBtn);
    outer->addWidget(grp);
    outer->addStretch();

    connect(m_startStopBtn, &QPushButton::clicked,
            this, &StrobeWidget::onStartStopClicked);
}

void StrobeWidget::onStartStopClicked()
{
    if (m_acquiring)
        emit acquisitionStopRequested();
    else
        emit acquisitionStartRequested();
}

void StrobeWidget::onAcquisitionStarted()
{
    m_acquiring = true;
    m_startStopBtn->setText("Stop Acquisition");
    m_statusIndicator->setStyleSheet(
        "background-color: #22cc44; border-radius: 8px;");
}

void StrobeWidget::onAcquisitionStopped()
{
    m_acquiring = false;
    m_startStopBtn->setText("Start Acquisition");
    m_statusIndicator->setStyleSheet(
        "background-color: #555; border-radius: 8px;");
}

void StrobeWidget::setStartStopEnabled(bool enabled)
{
    m_startStopBtn->setEnabled(enabled);
}