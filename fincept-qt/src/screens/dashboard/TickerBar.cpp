#include "screens/dashboard/TickerBar.h"

#include "ui/theme/Theme.h"
#include "ui/theme/ThemeManager.h"

#include <QPaintEvent>
#include <QPainter>

namespace fincept::screens {

TickerBar::TickerBar(QWidget* parent) : QWidget(parent) {
    setFixedHeight(24);

    auto apply_bg = [this]() {
        setStyleSheet(QString("background-color: %1;").arg(ui::colors::BG_BASE()));
    };
    apply_bg();
    connect(&ui::ThemeManager::instance(), &ui::ThemeManager::theme_changed, this,
            [this, apply_bg](const ui::ThemeTokens&) {
                apply_bg();
                update();
            });

    // Dummy data until service provides real data
    entries_ = {
        {"AAPL", 189.84, 1.23}, {"MSFT", 378.91, -0.45}, {"GOOGL", 141.80, 0.67}, {"AMZN", 178.25, -1.12},
        {"NVDA", 875.28, 2.34}, {"META", 485.39, 0.89},  {"TSLA", 175.21, -2.45}, {"JPM", 183.12, 0.56},
        {"V", 275.45, -0.23},   {"BRK.B", 408.92, 1.05}, {"SPY", 511.34, 0.78},   {"QQQ", 435.67, 1.12},
    };

    connect(&scroll_timer_, &QTimer::timeout, this, [this]() {
        offset_ += 1.0;
        update();
    });
    scroll_timer_.setInterval(50); // 20fps — started by resume() / showEvent, not here (P3)
}

void TickerBar::set_data(const QVector<Entry>& entries) {
    entries_ = entries;
    offset_ = 0;
    update();
}

void TickerBar::paintEvent(QPaintEvent*) {
    if (entries_.isEmpty())
        return;

    QPainter p(this);
    p.setRenderHint(QPainter::TextAntialiasing);

    QFont font(ui::fonts::DATA_FAMILY(), ui::fonts::font_px(-2));
    p.setFont(font);
    QFontMetrics fm(font);

    // Calculate total width of one cycle
    int item_spacing = 40;
    int total_width = 0;
    for (const auto& e : entries_) {
        QString text = QString("%1  %2  %3%4%")
                           .arg(e.symbol)
                           .arg(e.price, 0, 'f', 2)
                           .arg(e.change >= 0 ? "+" : "")
                           .arg(e.change, 0, 'f', 2);
        total_width += fm.horizontalAdvance(text) + item_spacing;
    }

    if (total_width > 0 && offset_ >= total_width) {
        offset_ = 0;
    }

    double x = -offset_;
    int passes = (width() / total_width) + 2;

    for (int pass = 0; pass < passes; ++pass) {
        for (const auto& e : entries_) {
            // Symbol in white
            p.setPen(QColor(ui::colors::WHITE()));
            p.drawText(QPointF(x, 17), e.symbol);
            x += fm.horizontalAdvance(e.symbol) + 8;

            // Price in grey
            QString price_str = QString::number(e.price, 'f', 2);
            p.setPen(QColor(ui::colors::GRAY()));
            p.drawText(QPointF(x, 17), price_str);
            x += fm.horizontalAdvance(price_str) + 8;

            // Change in green/red
            QString change_str = QString("%1%2%").arg(e.change >= 0 ? "+" : "").arg(e.change, 0, 'f', 2);
            p.setPen(QColor(ui::change_color(e.change)));
            p.drawText(QPointF(x, 17), change_str);
            x += fm.horizontalAdvance(change_str) + item_spacing;
        }
    }
}

} // namespace fincept::screens
