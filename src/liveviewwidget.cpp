#include "LiveViewWidget.h"

#include <QPainter>
#include <QPaintEvent>

LiveViewWidget::LiveViewWidget(QWidget* parent)
    : QWidget(parent)
    , m_placeholder("No camera connected")
{
    setMinimumSize(320, 320);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    // Dark background makes it obvious when no frame has arrived yet.
    QPalette pal = palette();
    pal.setColor(QPalette::Window, Qt::black);
    setPalette(pal);
    setAutoFillBackground(true);
}

QSize LiveViewWidget::sizeHint() const
{
    return QSize(640, 640);
}

void LiveViewWidget::updateFrame(const QImage& image)
{
    m_frame       = image;
    m_placeholder.clear();
    update();   // schedules a repaint on the GUI thread
}

void LiveViewWidget::showPlaceholder(const QString& message)
{
    m_frame = QImage{};
    m_placeholder = message;
    update();
}

void LiveViewWidget::paintEvent(QPaintEvent* /*event*/)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);

    if (m_frame.isNull()) {
        // Draw placeholder text centred in the widget.
        painter.setPen(Qt::gray);
        painter.drawText(rect(), Qt::AlignCenter, m_placeholder);
        return;
    }

    // Scale the frame to fit while preserving aspect ratio.
    QSize scaled = m_frame.size().scaled(size(), Qt::KeepAspectRatio);
    QPoint origin((width()  - scaled.width())  / 2,
                  (height() - scaled.height()) / 2);

    painter.drawImage(QRect(origin, scaled), m_frame);
}
