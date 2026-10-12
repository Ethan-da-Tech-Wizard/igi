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
                   Qt::BypassWindowManagerHint |
                   Qt::Tool);
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_ShowWithoutActivating);

    // Initialise the fade out animation with keyframe hold for Wayland compositor sync
    fadeAnim_ = new QPropertyAnimation(this, "flashOpacity", this);
    fadeAnim_->setDuration(400); // 400 ms total camera flash pulse
    fadeAnim_->setKeyValueAt(0.0, 1.0);
    fadeAnim_->setKeyValueAt(0.2, 1.0); // Hold full brightness briefly
    fadeAnim_->setKeyValueAt(1.0, 0.0);
    fadeAnim_->setEasingCurve(QEasingCurve::OutQuad);

    connect(fadeAnim_, &QPropertyAnimation::finished, this, &QWidget::hide);
}

void CaptureFlashOverlay::setFlashOpacity(qreal opacity) {
    flashOpacity_ = opacity;
    update();
}

void CaptureFlashOverlay::flash(const QRect& screenRect) {
    if (screenRect.isEmpty()) return;

    setGeometry(screenRect);
    setFlashOpacity(1.0);
    show(); // WA_ShowWithoutActivating prevents focus steal
    raise();

    fadeAnim_->stop();
    fadeAnim_->start();
}

void CaptureFlashOverlay::paintEvent(QPaintEvent*) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);

    const QRect r = rect();

    // 1. Full semi-transparent glowing amber camera flash over captured window/screen
    const QColor fillColor(255, 175, 0, static_cast<int>(85 * flashOpacity_));
    painter.fillRect(r, fillColor);

    // 2. Thick 16px glowing golden-amber border frame
    QPen borderPen(QColor(255, 215, 0, static_cast<int>(240 * flashOpacity_)));
    borderPen.setWidth(16);
    painter.setPen(borderPen);
    painter.setBrush(Qt::NoBrush);
    painter.drawRect(r.adjusted(8, 8, -8, -8));

    // 3. Crisp inner white accent line
    QPen innerPen(QColor(255, 255, 255, static_cast<int>(200 * flashOpacity_)));
    innerPen.setWidth(2);
    painter.setPen(innerPen);
    painter.drawRect(r.adjusted(16, 16, -16, -16));
}

} // namespace igi::ui
