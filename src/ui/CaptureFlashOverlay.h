#pragma once

#include <QWidget>
#include <QPropertyAnimation>

namespace igi::ui {

// Briefly flashes an amber border around the captured window to mitigate
// T-5 (TOCTOU window-swap attacks). It gives the user immediate visual feedback
// of exactly which screen area was ingested into the PHI corpus.
class CaptureFlashOverlay : public QWidget {
    Q_OBJECT
public:
    explicit CaptureFlashOverlay(QWidget* parent = nullptr);

    // Position the overlay over the given rect and trigger the flash animation.
    void flash(const QRect& screenRect);

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    QPropertyAnimation* fadeAnim_;
};

} // namespace igi::ui
