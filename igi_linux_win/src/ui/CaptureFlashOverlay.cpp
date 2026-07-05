#include "CaptureFlashOverlay.h"

#include <QPainter>
#include <QPen>

namespace igi::ui {

CaptureFlashOverlay::CaptureFlashOverlay(QWidget* parent)
    : QWidget(parent)
{
    // Frameless, always on top, does not steal focus, passes clicks through.
    setWindowFlags(Qt::Window |
                   Qt::FramelessWindowHint |
                   Qt::WindowStaysOnTopHint |
                   Qt::WindowTransparentForInput |
                   Qt::Tool);
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_ShowWithoutActivating);

    // Initialise the fade out animation.
    fadeAnim_ = new QPropertyAnimation(this, "windowOpacity", this);
    fadeAnim_->setDuration(350); // 350 ms fade
    fadeAnim_->setStartValue(1.0);
    fadeAnim_->setEndValue(0.0);
    fadeAnim_->setEasingCurve(QEasingCurve::OutQuad);

    connect(fadeAnim_, &QPropertyAnimation::finished, this, &QWidget::hide);
}

void CaptureFlashOverlay::flash(const QRect& screenRect) {
    if (screenRect.isEmpty()) return;

    setGeometry(screenRect);
    setWindowOpacity(1.0);
    show(); // WA_ShowWithoutActivating prevents focus steal
    raise();

    fadeAnim_->stop();
    fadeAnim_->start();
}

void CaptureFlashOverlay::paintEvent(QPaintEvent*) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, false);

    // T-5 visual feedback: A highly visible amber pulse.
    QPen pen(QColor(255, 180, 0, 220)); // Amber
    pen.setWidth(6); // 6px border
    painter.setPen(pen);
    painter.setBrush(Qt::NoBrush);

    // Draw the border just inside the rect so it's not clipped.
    // 3px inset keeps the 6px stroke fully visible.
    painter.drawRect(rect().adjusted(3, 3, -3, -3));
}

} // namespace igi::ui
