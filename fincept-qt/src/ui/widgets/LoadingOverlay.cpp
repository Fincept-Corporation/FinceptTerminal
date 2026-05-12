// src/ui/widgets/LoadingOverlay.cpp
#include "ui/widgets/LoadingOverlay.h"

#include "ui/theme/ThemeManager.h"

#include <QPainter>
#include <QPainterPath>

#include <cmath>

namespace fincept::ui {

LoadingOverlay::LoadingOverlay(QWidget* parent) : QWidget(parent) {
    setAttribute(Qt::WA_TransparentForMouseEvents, false);
    setAttribute(Qt::WA_NoSystemBackground);
    hide();

    tokens_ = ThemeManager::instance().tokens();

    connect(&ThemeManager::instance(), &ThemeManager::theme_changed, this, [this](const ThemeTokens& t) {
        tokens_ = t;
        update();
    });

    anim_timer_ = new QTimer(this);
    anim_timer_->setInterval(ANIM_INTERVAL_MS);
    connect(anim_timer_, &QTimer::timeout, this, [this]() {
        tick_++;
        update();
    });

    if (parent) {
        setGeometry(parent->rect());
        raise();
    }
}

void LoadingOverlay::show_loading(const QString& message) {
    message_ = message;
    if (parentWidget())
        setGeometry(parentWidget()->rect());
    raise();
    show();
    tick_ = 0;
    anim_timer_->start();
}

void LoadingOverlay::hide_loading() {
    anim_timer_->stop();
    hide();
}

void LoadingOverlay::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, false);

    // Dark overlay using theme bg_base
    QColor overlay(tokens_.bg_base);
    overlay.setAlpha(220);
    p.fillRect(rect(), overlay);

    const int cx = width() / 2;
    const int cy = height() / 2;

    // ── Pulsing bars ─────────────────────────────────────────────────────────
    const int bar_w = 4;
    const int bar_gap = 6;
    const int max_bar_h = 28;
    const int min_bar_h = 8;
    const int total_w = BAR_COUNT * bar_w + (BAR_COUNT - 1) * bar_gap;
    const int start_x = cx - total_w / 2;
    const int base_y = cy + 4;

    QColor accent_color(tokens_.accent);

    for (int i = 0; i < BAR_COUNT; ++i) {
        double phase = (tick_ * 0.3) - i * 0.7;
        double t = (std::sin(phase) + 1.0) / 2.0;
        int bar_h = min_bar_h + static_cast<int>(t * (max_bar_h - min_bar_h));
        int alpha = 100 + static_cast<int>(t * 155);
        QColor col = accent_color;
        col.setAlpha(alpha);
        int bx = start_x + i * (bar_w + bar_gap);
        p.fillRect(bx, base_y - bar_h, bar_w, bar_h, col);
    }

    // ── Message text ─────────────────────────────────────────────────────────
    if (!message_.isEmpty()) {
        QColor text_color = accent_color;
        text_color.setAlpha(200);
        p.setPen(text_color);
        // Use the app's resolved font (ThemeManager owns the proper
        // cross-platform fallback chain). QFont(const QString&, …) takes
        // a single family and fails silently for the CSS-style list
        // stored in tokens_.font_family — on macOS that fell back to a
        // proportional system font, which is the "fragmented" look the
        // user reported. current_font() returns a QFont built via
        // setFamilies() so Qt resolves the first available monospace.
        QFont f = ThemeManager::instance().current_font();
        f.setPixelSize(10);
        f.setBold(true);
        p.setFont(f);
        QRect text_rect(0, base_y + 14, width(), 24);
        p.drawText(text_rect, Qt::AlignHCenter | Qt::AlignTop, message_);
    }

    // ── Subtle border box ─────────────────────────────────────────────────────
    QColor border_color = accent_color;
    border_color.setAlpha(40);
    p.setPen(QPen(border_color, 1));
    int box_w = total_w + 40;
    int box_h = max_bar_h + 60;
    p.drawRect(cx - box_w / 2, cy - max_bar_h - 10, box_w, box_h);
}

} // namespace fincept::ui
