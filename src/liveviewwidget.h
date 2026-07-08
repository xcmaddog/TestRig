#ifndef LIVEVIEWWIDGET_H
#define LIVEVIEWWIDGET_H

#pragma once

#include <QWidget>
#include <QImage>

// ---------------------------------------------------------------------------
// LiveViewWidget
//
// Displays the most recent frame from the camera.  Scales the image to fit
// the widget while preserving aspect ratio.  Accepts frames via the
// updateFrame() slot, which is connected to CameraWorker::frameReady()
// across the thread boundary (queued connection).
// ---------------------------------------------------------------------------
class LiveViewWidget : public QWidget
{
    Q_OBJECT

public:
    explicit LiveViewWidget(QWidget* parent = nullptr);

    QSize sizeHint() const override;

public slots:
    void updateFrame(const QImage& image);

    /// Show a placeholder message when no camera is connected.
    void showPlaceholder(const QString& message = "No camera connected");

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    QImage  m_frame;
    QString m_placeholder;
};

#endif // LIVEVIEWWIDGET_H
