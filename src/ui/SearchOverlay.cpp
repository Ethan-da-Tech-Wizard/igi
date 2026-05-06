#include "ui/SearchOverlay.h"

#include <QApplication>
#include <QGraphicsDropShadowEffect>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QPainter>
#include <QPainterPath>
#include <QScreen>

namespace igi::ui {

// ─── HighlightOverlay ────────────────────────────────────────────────────────

HighlightOverlay::HighlightOverlay(QRect screenRect, QWidget* parent)
    : QWidget(parent,
              Qt::Window | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint |
              Qt::WindowTransparentForInput | Qt::Tool)
{
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_ShowWithoutActivating);
    setGeometry(screenRect);
}

void HighlightOverlay::setHighlights(const std::vector<QRect>& rects) {
    highlights_ = rects;
    update();
}

void HighlightOverlay::paintEvent(QPaintEvent* /*event*/) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    // Outer glow: blurred orange border
    const QColor glowColor(255, 200, 50, 80);
    // Fill: semi-transparent amber
    const QColor fillColor(255, 210, 0, 60);
    // Border: solid amber
    const QColor borderColor(255, 190, 0, 200);

    for (const QRect& r : highlights_) {
        // Map from absolute screen coords to widget-local coords.
        QRect local = r.translated(-geometry().topLeft());

        // Glow pass (expanded rect, low alpha)
        const int glow = 4;
        QRectF glowRect = QRectF(local).adjusted(-glow, -glow, glow, glow);
        p.setPen(Qt::NoPen);
        p.setBrush(glowColor);
        p.drawRoundedRect(glowRect, 4, 4);

        // Fill pass
        p.setBrush(fillColor);
        p.setPen(QPen(borderColor, 2));
        p.drawRoundedRect(QRectF(local), 3, 3);
    }
}

// ─── SearchOverlay ───────────────────────────────────────────────────────────

SearchOverlay::SearchOverlay(QWidget* parent)
    : QWidget(parent,
              Qt::Window | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool)
{
    setAttribute(Qt::WA_TranslucentBackground);

    // ── Layout ──
    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(16, 12, 16, 12);
    layout->setSpacing(10);

    // Search icon label
    auto* icon = new QLabel("⌕", this);
    icon->setStyleSheet("color: rgba(255,255,255,0.6); font-size: 20px;");

    searchField_ = new QLineEdit(this);
    searchField_->setPlaceholderText("Search on screen…");
    searchField_->setMinimumWidth(320);
    searchField_->setStyleSheet(R"(
        QLineEdit {
            background: transparent;
            border: none;
            color: white;
            font-size: 17px;
            font-family: -apple-system, 'SF Pro Display', 'Helvetica Neue', sans-serif;
            font-weight: 300;
            letter-spacing: 0.3px;
        }
        QLineEdit::placeholder {
            color: rgba(255,255,255,0.35);
        }
    )");

    // Shortcut hint
    auto* hint = new QLabel("esc to close", this);
    hint->setStyleSheet(
        "color: rgba(255,255,255,0.3); font-size: 11px; font-family: -apple-system;");

    layout->addWidget(icon);
    layout->addWidget(searchField_, 1);
    layout->addWidget(hint);

    setFixedHeight(56);
    setMinimumWidth(420);
    adjustSize();

    // ── Styling: dark glassmorphism pill ──
    setStyleSheet(R"(
        SearchOverlay {
            background: transparent;
        }
    )");

    // Drop shadow on the whole pill — applied via custom paint below.

    connect(searchField_, &QLineEdit::textChanged,
            this, &SearchOverlay::queryChanged);
}

void SearchOverlay::activate() {
    // Centre horizontally near the top of the primary screen.
    const QRect screenGeo = QApplication::primaryScreen()->availableGeometry();
    const int x = screenGeo.center().x() - width() / 2;
    const int y = screenGeo.top() + 80;
    move(x, y);

    show();
    raise();
    activateWindow();
    searchField_->clear();
    searchField_->setFocus();
}

void SearchOverlay::setHighlights(const std::vector<QRect>& rects, QRect screenRect) {
    if (!highlightOverlay_) {
        highlightOverlay_ = new HighlightOverlay(screenRect);
    } else {
        highlightOverlay_->setGeometry(screenRect);
    }
    highlightOverlay_->setHighlights(rects);
    if (!rects.empty()) {
        highlightOverlay_->show();
    } else {
        highlightOverlay_->hide();
    }
}

QString SearchOverlay::query() const {
    return searchField_->text();
}

void SearchOverlay::keyPressEvent(QKeyEvent* event) {
    if (event->key() == Qt::Key_Escape) {
        // Hide our highlight overlay too.
        if (highlightOverlay_) {
            highlightOverlay_->hide();
        }
        hide();
        emit dismissed();
        return;
    }
    QWidget::keyPressEvent(event);
}

void SearchOverlay::paintEvent(QPaintEvent* /*event*/) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    // Outer glow
    {
        const int glow = 20;
        const QRectF glowRect = QRectF(rect()).adjusted(glow, glow, -glow, -glow);
        QColor g(255, 255, 255, 8);
        p.setPen(Qt::NoPen);
        p.setBrush(g);
        p.drawRoundedRect(QRectF(rect()), 28, 28);
    }

    // Dark frosted-glass pill
    p.setPen(QPen(QColor(255, 255, 255, 30), 1));
    p.setBrush(QColor(20, 20, 28, 210));
    p.drawRoundedRect(QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5), 28, 28);
}

}  // namespace igi::ui

