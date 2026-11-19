// ==============================================================================
// Program: SearchOverlay.cpp
// Programmer: Ethan-da-Tech-Wizard
// Last Modified: November 18, 2026
// Purpose: Implements the interactive translucent desktop overlay GUI, managing fuzzy search input, real-time result bounding box highlighting, keyboard navigation, and target click-through.
//
// How it fits: SearchSession owns the interaction logic; this file is only the
// view. SearchSession calls activate(), setHighlights(), setMatchInfo() and
// query(), and listens to queryChanged() and dismissed(). Two windows are
// involved: SearchOverlay (the small search bar) and HighlightOverlay (a
// full-size transparent window drawing the match rectangles). Both are
// frameless, always on top and top-level windows (Qt::Window) so they float
// over whatever app is being searched.
//
// Coordinates: every rectangle handed to this class is in LOGICAL screen
// pixels (device-independent). SearchSession converts from physical image
// pixels before calling.
//
// Known limits: nextMatchRequested()/prevMatchRequested() are emitted for
// Down/Enter and Up but nothing in the program is connected to them yet, so
// those keys currently do nothing; the match counter always shows "1 of N".
// ==============================================================================

#include "ui/SearchOverlay.h"

#include <QApplication>
#include <QGraphicsDropShadowEffect>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QScreen>

namespace igi::ui {

// HighlightOverlay
// ─── HighlightOverlay ────────────────────────────────────────────────────────

/**
 * Creates the transparent highlight window.
 * @param screenRect Screen area to cover, in logical global coordinates.
 * @param parent     Optional parent (normally none: it is a top-level window).
 */
HighlightOverlay::HighlightOverlay(QRect screenRect, QWidget* parent)
    // Window flags: frameless + stay on top + transparent for input, so mouse and
    // keyboard events pass through to the application underneath.
    : QWidget(parent,
              Qt::Window | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint |
              Qt::WindowTransparentForInput)
{
    // Translucent so only the painted rectangles are visible; ShowWithoutActivating
    // so showing it does not steal focus from the search bar.
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_ShowWithoutActivating);
    setGeometry(screenRect);
}

/** Replaces the highlight set (logical global screen coordinates) and repaints. */
void HighlightOverlay::setHighlights(const std::vector<QRect>& rects) {
    highlights_ = rects;
    update();
}

/** Paints a glow, a translucent fill and a border for every highlight. */
void HighlightOverlay::paintEvent(QPaintEvent* /*event*/) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    // Colours: soft outer glow, translucent amber fill, solid amber border.
    const QColor glowColor(255, 200, 50, 80);
    const QColor fillColor(255, 210, 0, 60);
    const QColor borderColor(255, 190, 0, 200);

    // Highlights arrive in global screen coordinates; this widget's origin is its own
    // top-left, so subtract the widget position.
    for (const QRect& r : highlights_) {
        QRect local = r.translated(-geometry().topLeft());

        // Glow pass: slightly larger rectangle with low alpha, drawn first so the
        // fill and border sit on top of it.
        const int glow = 4;
        QRectF glowRect = QRectF(local).adjusted(-glow, -glow, glow, glow);
        p.setPen(Qt::NoPen);
        p.setBrush(glowColor);
        p.drawRoundedRect(glowRect, 4, 4);

        // Fill and border pass.
        p.setBrush(fillColor);
        p.setPen(QPen(borderColor, 2));
        p.drawRoundedRect(QRectF(local), 3, 3);
    }
}

// SearchOverlay
// ─── SearchOverlay ───────────────────────────────────────────────────────────

/**
 * Builds the search bar: icon, text field, match-count badge and the
 * "esc to close" hint in a fixed 56 px high pill.
 */
SearchOverlay::SearchOverlay(QWidget* parent)
    : QWidget(parent,
              Qt::Window | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint)
{
    // Translucent window; the rounded dark background is painted by paintEvent().
    setAttribute(Qt::WA_TranslucentBackground);
    // Needed so the line edit can take keyboard focus in a frameless window.
    setFocusPolicy(Qt::StrongFocus);

    // Layout: [icon] [search field (stretches)] [match count (hidden until there are matches)] [hint].
    // ── Layout ──
    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(16, 12, 16, 12);
    layout->setSpacing(10);

    // Search icon label
    auto* icon = new QLabel("⌕", this);
    icon->setStyleSheet("color: rgba(255,255,255,0.6); font-size: 20px;");

    searchField_ = new QLineEdit(this);
    searchField_->setPlaceholderText("Search on screen…");
    searchField_->setMinimumWidth(400);
    searchField_->setStyleSheet(R"(
        QLineEdit {
            background: transparent;
            border: none;
            color: white;
            font-size: 22px;
            font-family: -apple-system, 'SF Pro Display', 'Helvetica Neue', sans-serif;
            font-weight: 500;
            letter-spacing: 0.5px;
        }
        QLineEdit::placeholder {
            color: rgba(255, 255, 255, 0.5);
        }
    )");

    matchCountLabel_ = new QLabel(this);
    matchCountLabel_->setStyleSheet(
        "color: #ffca28; font-size: 12px; font-weight: 500; background: rgba(255,202,40,0.15); "
        "border: 1px solid rgba(255,202,40,0.3); border-radius: 10px; padding: 2px 8px;");
    matchCountLabel_->hide();

    // Shortcut hint
    auto* hint = new QLabel("esc to close", this);
    hint->setStyleSheet(
        "color: rgba(255,255,255,0.3); font-size: 11px; font-family: -apple-system;");

    layout->addWidget(icon);
    layout->addWidget(searchField_, 1);
    layout->addWidget(matchCountLabel_);
    layout->addWidget(hint);

    setFixedHeight(56);
    setMinimumWidth(440);
    adjustSize();

    // Styling: dark glassmorphism pill. The widget itself stays transparent; the
    // background is drawn by paintEvent().
    // ── Styling: dark glassmorphism pill ──
    setStyleSheet(R"(
        SearchOverlay {
            background: transparent;
        }
    )");

    // Every edit of the text field is forwarded as queryChanged (search as you type).
    connect(searchField_, &QLineEdit::textChanged,
            this, &SearchOverlay::queryChanged);
}

/**
 * Shows the bar, centred horizontally and 100 px above the bottom edge of the
 * captured screen, clears the previous query and focuses the text field.
 * @param targetScreen Logical rectangle of the captured window.
 */
void SearchOverlay::activate(const QRect& targetScreen) {
    // Bottom placement keeps the bar clear of the text the user is probably looking for
    // and floats above a taskbar or dock.
    const int x = targetScreen.center().x() - width() / 2;
    const int y = targetScreen.bottom() - height() - 100;
    move(x, y);

    // show, raise and activateWindow are all needed: on some platforms a
    // frameless window is not focused by show() alone.
    show();
    raise();
    activateWindow();
    // A fresh session starts with an empty query and no match badge.
    searchField_->clear();
    searchField_->setFocus();
    if (matchCountLabel_) matchCountLabel_->hide();
}

/** Draws the rounded dark translucent background and a 1 px light border. */
void SearchOverlay::paintEvent(QPaintEvent*) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    
    // Dark glassmorphism background: 86 % opaque dark grey (alpha 220 of 255).
    QPainterPath path;
    path.addRoundedRect(rect(), 16, 16);
    
    painter.fillPath(path, QColor(30, 30, 30, 220));
    painter.setPen(QPen(QColor(255, 255, 255, 40), 1));
    painter.drawPath(path);
}

/**
 * Updates the badge text, e.g. "1 of 14 matches" or "1 of 14 (Page 42)".
 * Hidden when there are no matches or the query is blank.
 * @param matchIndex   Zero-based index of the current match (shown +1).
 * @param totalMatches Number of matches.
 * @param pageNumber   1-based PDF page, or 0 to omit the page.
 */
void SearchOverlay::setMatchInfo(int matchIndex, int totalMatches, int pageNumber) {
    if (!matchCountLabel_) return;

    if (totalMatches <= 0 || searchField_->text().trimmed().isEmpty()) {
        matchCountLabel_->hide();
        return;
    }

    if (pageNumber > 0) {
        matchCountLabel_->setText(QStringLiteral("%1 of %2 (Page %3)")
            .arg(matchIndex + 1)
            .arg(totalMatches)
            .arg(pageNumber));
    } else {
        matchCountLabel_->setText(QStringLiteral("%1 of %2 matches")
            .arg(matchIndex + 1)
            .arg(totalMatches));
    }
    matchCountLabel_->show();
    // The badge changed size; shrink/grow the window to fit.
    adjustSize();
}

/**
 * Shows or hides the highlight window with the given rectangles.
 * The HighlightOverlay is created lazily on first use and resized to
 * @p screenRect every time (the captured window can be on another screen).
 * @param rects      Match rectangles, logical global coordinates.
 * @param screenRect Area the highlight window should cover, logical coordinates.
 */
void SearchOverlay::setHighlights(const std::vector<QRect>& rects, QRect screenRect) {
    if (!highlightOverlay_) {
        highlightOverlay_ = new HighlightOverlay(screenRect);
    } else {
        highlightOverlay_->setGeometry(screenRect);
    }
    highlightOverlay_->setHighlights(rects);
    // Nothing to highlight: hide the window so it does not cover the screen needlessly.
    if (!rects.empty()) {
        highlightOverlay_->show();
        highlightOverlay_->raise();
    } else {
        highlightOverlay_->hide();
    }
}

/** @return Current text of the search field. */
QString SearchOverlay::query() const {
    return searchField_->text();
}

/**
 * Handles window deactivation: if the user clicks into another application,
 * the bar dismisses itself so highlights never float over unrelated windows.
 */
bool SearchOverlay::event(QEvent* event) {
    // Another window of this application got focus (for example the highlight
    // window on Wayland): not a real focus loss, ignore it.
    if (event->type() == QEvent::WindowDeactivate) {
        if (QApplication::activeWindow() != nullptr) {
            return QWidget::event(event);
        }
        
        // activeWindow is null: the user moved to another application, so hide both
        // windows and tell the session to wipe the corpus.
        if (highlightOverlay_) {
            highlightOverlay_->hide();
        }
        hide();
        emit dismissed();
    }
    return QWidget::event(event);
}

/**
 * Keys: Escape hides both windows and emits dismissed(); Down/Enter and Up emit
 * nextMatchRequested()/prevMatchRequested() (not connected yet); everything
 * else goes to the text field.
 */
void SearchOverlay::keyPressEvent(QKeyEvent* event) {
    if (event->key() == Qt::Key_Escape) {
        if (highlightOverlay_) {
            highlightOverlay_->hide();
        }
        hide();
        emit dismissed();
        return;
    } else if (event->key() == Qt::Key_Down || event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
        emit nextMatchRequested();
        return;
    } else if (event->key() == Qt::Key_Up) {
        emit prevMatchRequested();
        return;
    }
    QWidget::keyPressEvent(event);
}

void SearchOverlay::mousePressEvent(QMouseEvent* event) {
    /** Starts a window drag: remembers the offset between the mouse and the window corner. */
    if (event->button() == Qt::LeftButton) {
        dragPosition_ = event->globalPosition().toPoint() - frameGeometry().topLeft();
        event->accept();
    }
}

void SearchOverlay::mouseMoveEvent(QMouseEvent* event) {
    /** Moves the frameless window while the left button is held, so the user can reposition it. */
    if (event->buttons() & Qt::LeftButton) {
        move(event->globalPosition().toPoint() - dragPosition_);
        event->accept();
    }
}

}  // namespace igi::ui

