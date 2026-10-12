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

// ─── HighlightOverlay ────────────────────────────────────────────────────────

HighlightOverlay::HighlightOverlay(QRect screenRect, QWidget* parent)
    : QWidget(parent,
              Qt::Window | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint |
              Qt::WindowTransparentForInput)
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
              Qt::Window | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint)
{
    setAttribute(Qt::WA_TranslucentBackground);
    // Explicitly request focus
    setFocusPolicy(Qt::StrongFocus);

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

    // ── Styling: dark glassmorphism pill ──
    setStyleSheet(R"(
        SearchOverlay {
            background: transparent;
        }
    )");

    connect(searchField_, &QLineEdit::textChanged,
            this, &SearchOverlay::queryChanged);
}

void SearchOverlay::activate(const QRect& targetScreen) {
    // Centre horizontally near the bottom of the captured screen (floating above taskbar/panel).
    const int x = targetScreen.center().x() - width() / 2;
    const int y = targetScreen.bottom() - height() - 100;
    move(x, y);

    show();
    raise();
    activateWindow();
    searchField_->clear();
    searchField_->setFocus();
    if (matchCountLabel_) matchCountLabel_->hide();
}

void SearchOverlay::paintEvent(QPaintEvent*) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    
    // Draw a beautiful dark glassmorphism background
    QPainterPath path;
    path.addRoundedRect(rect(), 16, 16);
    
    painter.fillPath(path, QColor(30, 30, 30, 220));
    painter.setPen(QPen(QColor(255, 255, 255, 40), 1));
    painter.drawPath(path);
}

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
    adjustSize();
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
        highlightOverlay_->raise();
    } else {
        highlightOverlay_->hide();
    }
}

QString SearchOverlay::query() const {
    return searchField_->text();
}

bool SearchOverlay::event(QEvent* event) {
    if (event->type() == QEvent::WindowDeactivate) {
        // If focus shifted to another window within our app (e.g. HighlightOverlay on Wayland), ignore it.
        if (QApplication::activeWindow() != nullptr) {
            return QWidget::event(event);
        }
        
        // If the user clicks away or opens another window (activeWindow is null), dismiss the search bar
        // to prevent highlights from floating over unrelated windows.
        if (highlightOverlay_) {
            highlightOverlay_->hide();
        }
        hide();
        emit dismissed();
    }
    return QWidget::event(event);
}

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
    if (event->button() == Qt::LeftButton) {
        dragPosition_ = event->globalPosition().toPoint() - frameGeometry().topLeft();
        event->accept();
    }
}

void SearchOverlay::mouseMoveEvent(QMouseEvent* event) {
    if (event->buttons() & Qt::LeftButton) {
        move(event->globalPosition().toPoint() - dragPosition_);
        event->accept();
    }
}

}  // namespace igi::ui

