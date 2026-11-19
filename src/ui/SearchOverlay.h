// ==============================================================================
// Program: SearchOverlay.h
// Programmer: Ethan-da-Tech-Wizard
// Last Modified: November 18, 2026
// Purpose: Implements the interactive translucent desktop overlay GUI, managing fuzzy search input, real-time result bounding box highlighting, keyboard navigation, and target click-through.
// ==============================================================================

#pragma once

#include <vector>

#include <QColor>
#include <QLineEdit>
#include <QRect>
#include <QWidget>

class QLabel;

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

    // Replace the current highlight set. Coordinates are LOGICAL global screen
    // pixels: SearchSession converts them from the physical pixels of the
    // capture before calling (the window is placed at screenRect, so the
    // rectangles are drawn relative to it).
    void setHighlights(const std::vector<QRect>& rects);

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    std::vector<QRect> highlights_;
};

// ─── SearchOverlay ───────────────────────────────────────────────────────────
// Frameless, always-on-top search bar shown when the user presses the global
// hotkey (see PlatformHotkey.h). Emits queryChanged() on every keystroke so the
// pipeline can search in real time. Emits dismissed() when the user presses
// Escape or switches to another application. nextMatchRequested() and
// prevMatchRequested() (Down/Enter, Up) are emitted but not connected to
// anything yet.
class SearchOverlay final : public QWidget {
    Q_OBJECT
public:
    explicit SearchOverlay(QWidget* parent = nullptr);

    // Show the overlay and give focus to the search field, centered on targetScreen.
    void activate(const QRect& targetScreen);

    // Update the yellow highlight rectangles painted over the captured window.
    // rects and screenRect are LOGICAL global screen coordinates; screenRect is
    // the area of the captured window and is used to position the HighlightOverlay.
    void setHighlights(const std::vector<QRect>& rects, QRect screenRect);

    // Set match summary counter (e.g., "1 of 14 (Page 42)").
    void setMatchInfo(int matchIndex, int totalMatches, int pageNumber = 0);

    // Returns the current query text.
    QString query() const;

signals:
    void queryChanged(const QString& text);
    void nextMatchRequested();
    void prevMatchRequested();
    void dismissed();

protected:
    bool event(QEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void paintEvent(QPaintEvent* event) override;

private:
    QLineEdit*        searchField_;
    QLabel*           matchCountLabel_;
    HighlightOverlay* highlightOverlay_ = nullptr;
    QPoint            dragPosition_;
};

}  // namespace igi::ui
