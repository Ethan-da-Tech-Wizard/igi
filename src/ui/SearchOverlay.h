#pragma once

#include <vector>

#include <QColor>
#include <QLineEdit>
#include <QRect>
#include <QWidget>

#include "search/BoundingBox.h"

namespace igi::ui {

// ─── HighlightOverlay ────────────────────────────────────────────────────────
// A frameless, transparent, always-on-top window that sits exactly over the
// captured window and paints semi-transparent yellow rectangles around each
// fuzzy-search match.  Created and destroyed by SearchOverlay.
class HighlightOverlay final : public QWidget {
    Q_OBJECT
public:
    explicit HighlightOverlay(QRect screenRect, QWidget* parent = nullptr);

    // Replace the current highlight set.  Coordinates are physical pixels
    // in the coordinate space returned by ScreenCapture (same origin as
    // screenRect passed to the constructor).
    void setHighlights(const std::vector<QRect>& rects);

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    std::vector<QRect> highlights_;
};

// ─── SearchOverlay ───────────────────────────────────────────────────────────
// Frameless, always-on-top search bar shown when the user presses Cmd+Shift+F.
// Emits queryChanged() on every keystroke so the pipeline can search in
// real-time.  Emits dismissed() when the user presses Escape.
class SearchOverlay final : public QWidget {
    Q_OBJECT
public:
    explicit SearchOverlay(QWidget* parent = nullptr);

    // Show the overlay and give focus to the search field.
    void activate();

    // Update the yellow highlight rectangles painted over the captured window.
    // screenRect is the physical-pixel rect of the captured window (used to
    // position the HighlightOverlay).
    void setHighlights(const std::vector<QRect>& rects, QRect screenRect);

    // Returns the current query text.
    QString query() const;

signals:
    void queryChanged(const QString& text);
    void dismissed();

protected:
    void keyPressEvent(QKeyEvent* event) override;
    void paintEvent(QPaintEvent* event) override;

private:
    QLineEdit*       searchField_;
    HighlightOverlay* highlightOverlay_ = nullptr;
};

}  // namespace igi::ui
