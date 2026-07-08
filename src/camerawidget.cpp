#include "camerawidget.h"
#include "cameracontroller.h"

#include <QDoubleSpinBox>
#include <QSpinBox>
#include <QComboBox>
#include <QLabel>
#include <QGroupBox>
#include <QFormLayout>
#include <QVBoxLayout>
#include <QHBoxLayout>

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------

CameraWidget::CameraWidget(CameraController* controller, QWidget* parent)
    : QWidget(parent)
    , m_controller(controller)
{
    buildUI();
}

void CameraWidget::buildUI()
{
    auto* outerLayout = new QVBoxLayout(this);
    outerLayout->setContentsMargins(0, 0, 0, 0);

    // ---- Exposure -------------------------------------------------------
    {
        auto* grp    = new QGroupBox("Exposure", this);
        auto* form   = new QFormLayout(grp);

        m_exposureSpin = new QDoubleSpinBox(this);
        m_exposureSpin->setSuffix(" µs");
        m_exposureSpin->setDecimals(1);
        // Placeholder range — overwritten in populate()
        m_exposureSpin->setRange(0.0, 500000.0);
        m_exposureSpin->setSingleStep(100.0);

        m_exposureRange = new QLabel("– – –", this);
        m_exposureRange->setStyleSheet("color: gray; font-size: 10px;");

        form->addRow("Exposure time:", m_exposureSpin);
        form->addRow("Range:", m_exposureRange);
        outerLayout->addWidget(grp);

        connect(m_exposureSpin,
                QOverload<double>::of(&QDoubleSpinBox::valueChanged),
                this, &CameraWidget::onExposureChanged);
    }

    // ---- Gain -----------------------------------------------------------
    {
        auto* grp  = new QGroupBox("Gain", this);
        auto* form = new QFormLayout(grp);

        m_gainSpin = new QDoubleSpinBox(this);
        m_gainSpin->setDecimals(2);
        m_gainSpin->setRange(1.0, 4.0);   // overwritten in populate()
        m_gainSpin->setSingleStep(0.1);

        m_gainRange = new QLabel("– – –", this);
        m_gainRange->setStyleSheet("color: gray; font-size: 10px;");

        form->addRow("Gain:", m_gainSpin);
        form->addRow("Range:", m_gainRange);
        outerLayout->addWidget(grp);

        connect(m_gainSpin,
                QOverload<double>::of(&QDoubleSpinBox::valueChanged),
                this, &CameraWidget::onGainChanged);
    }

    // ---- Pixel format ---------------------------------------------------
    {
        auto* grp  = new QGroupBox("Pixel Format", this);
        auto* form = new QFormLayout(grp);

        m_pixelFmtCombo = new QComboBox(this);
        form->addRow("Format:", m_pixelFmtCombo);
        outerLayout->addWidget(grp);

        connect(m_pixelFmtCombo,
                QOverload<int>::of(&QComboBox::currentIndexChanged),
                this, &CameraWidget::onPixelFormatChanged);
    }

    // ---- ROI ------------------------------------------------------------
    {
        auto* grp  = new QGroupBox("Region of Interest", this);
        auto* form = new QFormLayout(grp);

        // Sensor is 2048×2048.  Step constraints from datasheet:
        //   Width  step: 16 px   Height step: 2 px
        //   OffsetX grid: 2 px   OffsetY grid: 2 px
        m_roiOffsetX = new QSpinBox(this);
        m_roiOffsetX->setRange(0, 2046);
        m_roiOffsetX->setSingleStep(2);
        m_roiOffsetX->setSuffix(" px");

        m_roiOffsetY = new QSpinBox(this);
        m_roiOffsetY->setRange(0, 2046);
        m_roiOffsetY->setSingleStep(2);
        m_roiOffsetY->setSuffix(" px");

        m_roiWidth = new QSpinBox(this);
        m_roiWidth->setRange(16, 2048);
        m_roiWidth->setSingleStep(16);
        m_roiWidth->setSuffix(" px");

        m_roiHeight = new QSpinBox(this);
        m_roiHeight->setRange(2, 2048);
        m_roiHeight->setSingleStep(2);
        m_roiHeight->setSuffix(" px");

        form->addRow("Offset X:", m_roiOffsetX);
        form->addRow("Offset Y:", m_roiOffsetY);
        form->addRow("Width:",    m_roiWidth);
        form->addRow("Height:",   m_roiHeight);
        outerLayout->addWidget(grp);

        // Apply ROI when any field is committed (Enter or focus lost).
        // Using editingFinished avoids mid-type camera writes.
        connect(m_roiOffsetX, &QSpinBox::editingFinished, this, &CameraWidget::onROIChanged);
        connect(m_roiOffsetY, &QSpinBox::editingFinished, this, &CameraWidget::onROIChanged);
        connect(m_roiWidth,   &QSpinBox::editingFinished, this, &CameraWidget::onROIChanged);
        connect(m_roiHeight,  &QSpinBox::editingFinished, this, &CameraWidget::onROIChanged);
    }

    // ---- Decimation -----------------------------------------------------
    {
        auto* grp  = new QGroupBox("Decimation (Sensor)", this);
        auto* form = new QFormLayout(grp);

        m_decimHCombo = new QComboBox(this);
        m_decimVCombo = new QComboBox(this);

        // Supported factors per datasheet: 2 / 4 / 6 / 8
        // "1" (off) added here; camera may reject it — populate() will
        // replace this list with whatever the camera actually reports.
        for (auto* combo : {m_decimHCombo, m_decimVCombo}) {
            combo->addItem("1 (off)", 1);
            combo->addItem("2",       2);
            combo->addItem("4",       4);
            combo->addItem("6",       6);
            combo->addItem("8",       8);
        }

        form->addRow("Horizontal:", m_decimHCombo);
        form->addRow("Vertical:",   m_decimVCombo);
        outerLayout->addWidget(grp);

        connect(m_decimHCombo,
                QOverload<int>::of(&QComboBox::currentIndexChanged),
                this, &CameraWidget::onDecimationHChanged);
        connect(m_decimVCombo,
                QOverload<int>::of(&QComboBox::currentIndexChanged),
                this, &CameraWidget::onDecimationVChanged);
    }

    outerLayout->addStretch();
}

// ---------------------------------------------------------------------------
// populate() — read current camera state into the widgets
// ---------------------------------------------------------------------------

void CameraWidget::populate()
{
    m_populating = true;

    // Exposure
    double expMin = m_controller->getExposureMin();
    double expMax = m_controller->getExposureMax();
    m_exposureSpin->setRange(expMin, expMax);
    m_exposureSpin->setValue(m_controller->getExposure());
    m_exposureRange->setText(QString("%1 – %2 µs")
                                 .arg(expMin, 0, 'f', 1)
                                 .arg(expMax, 0, 'f', 1));

    // Gain
    double gainMin = m_controller->getGainMin();
    double gainMax = m_controller->getGainMax();
    m_gainSpin->setRange(gainMin, gainMax);
    m_gainSpin->setValue(m_controller->getGain());
    m_gainRange->setText(QString("%1 – %2")
                             .arg(gainMin, 0, 'f', 2)
                             .arg(gainMax, 0, 'f', 2));

    // Pixel format — populate from camera's available list
    m_pixelFmtCombo->clear();
    auto fmts = m_controller->availablePixelFormats();
    for (const auto& f : fmts)
        m_pixelFmtCombo->addItem(QString::fromStdString(f));
    auto curFmt = m_controller->getPixelFormat();
    m_pixelFmtCombo->setCurrentText(QString::fromStdString(curFmt));

    // ROI
    m_roiOffsetX->setValue(m_controller->getOffsetX());
    m_roiOffsetY->setValue(m_controller->getOffsetY());
    m_roiWidth->setValue(m_controller->getWidth());
    m_roiHeight->setValue(m_controller->getHeight());

    // Decimation — leave the hardcoded list but select the current value.
    auto setDeciCombo = [](QComboBox* combo, int current) {
        for (int i = 0; i < combo->count(); ++i) {
            if (combo->itemData(i).toInt() == current) {
                combo->setCurrentIndex(i);
                return;
            }
        }
    };
    setDeciCombo(m_decimHCombo, m_controller->getDecimationH());
    setDeciCombo(m_decimVCombo, m_controller->getDecimationV());

    m_populating = false;
}

void CameraWidget::setControlsEnabled(bool enabled)
{
    m_exposureSpin->setEnabled(enabled);
    m_gainSpin->setEnabled(enabled);
    m_pixelFmtCombo->setEnabled(enabled);
    m_roiOffsetX->setEnabled(enabled);
    m_roiOffsetY->setEnabled(enabled);
    m_roiWidth->setEnabled(enabled);
    m_roiHeight->setEnabled(enabled);
    m_decimHCombo->setEnabled(enabled);
    m_decimVCombo->setEnabled(enabled);
}

// ---------------------------------------------------------------------------
// Slots
// ---------------------------------------------------------------------------

void CameraWidget::onExposureChanged(double value)
{
    if (m_populating) return;
    m_controller->setExposure(value);
}

void CameraWidget::onGainChanged(double value)
{
    if (m_populating) return;
    m_controller->setGain(value);
}

void CameraWidget::onPixelFormatChanged(int /*index*/)
{
    if (m_populating) return;
    m_controller->setPixelFormat(m_pixelFmtCombo->currentText().toStdString());
}

void CameraWidget::onROIChanged()
{
    if (m_populating) return;
    m_controller->setROI(
        m_roiOffsetX->value(),
        m_roiOffsetY->value(),
        m_roiWidth->value(),
        m_roiHeight->value());

    // Re-read back from camera in case alignment clamped the values.
    m_populating = true;
    m_roiOffsetX->setValue(m_controller->getOffsetX());
    m_roiOffsetY->setValue(m_controller->getOffsetY());
    m_roiWidth->setValue(m_controller->getWidth());
    m_roiHeight->setValue(m_controller->getHeight());
    m_populating = false;
}

void CameraWidget::onDecimationHChanged(int index)
{
    if (m_populating) return;
    m_controller->setDecimationH(m_decimHCombo->itemData(index).toInt());
}

void CameraWidget::onDecimationVChanged(int index)
{
    if (m_populating) return;
    m_controller->setDecimationV(m_decimVCombo->itemData(index).toInt());
}
