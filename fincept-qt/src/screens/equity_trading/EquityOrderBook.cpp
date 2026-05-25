// EquityOrderBook.cpp — custom-painted order book with depth bars
#include "screens/equity_trading/EquityOrderBook.h"

#include "ui/theme/Theme.h"

#include <QHBoxLayout>
#include <QMouseEvent>
#include <QMutexLocker>
#include <QPainter>
#include <QVBoxLayout>

namespace {
using namespace fincept::ui;
static QColor bg_surface() {
    return colors::BG_SURFACE;
}
static QColor color_sell() {
    return colors::NEGATIVE;
}
static QColor color_buy() {
    return colors::POSITIVE;
}
static QColor text_secondary() {
    return colors::TEXT_SECONDARY;
}
static QColor border_med() {
    return colors::BORDER_MED;
}
} // namespace

#include <algorithm>
#include <cmath>

namespace fincept::screens::equity {

EquityOrderBook::EquityOrderBook(QWidget* parent) : QWidget(parent) {
    setObjectName("eqOrderBook");
    connect(&ui::ThemeManager::instance(), &ui::ThemeManager::theme_changed, this, [this]() { update(); });

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    // Header
    auto* header = new QWidget(this);
    header->setObjectName("eqObHeader");
    header->setFixedHeight(HEADER_H);
    auto* h_layout = new QHBoxLayout(header);
    h_layout->setContentsMargins(8, 0, 8, 0);

    auto* title = new QLabel("MARKET DEPTH");
    title->setObjectName("eqObTitle");
    h_layout->addWidget(title);
    h_layout->addStretch();

    layout->addWidget(header);

    // Spread label
    spread_label_ = new QLabel("Spread: --");
    spread_label_->setObjectName("eqObSpread");
    spread_label_->setFixedHeight(SPREAD_H);
    spread_label_->setAlignment(Qt::AlignCenter);

    // Canvas — transparent overlay for layout spacing; painting happens on `this`
    canvas_ = new QWidget(this);
    canvas_->setObjectName("eqObCanvas");
    canvas_->setAttribute(Qt::WA_TransparentForMouseEvents);
    canvas_->setMinimumHeight(100);
    layout->addWidget(canvas_, 1);
    layout->addWidget(spread_label_);

    // Coalesce repaints at max 20fps
    repaint_timer_ = new QTimer(this);
    repaint_timer_->setSingleShot(true);
    repaint_timer_->setInterval(50);
    connect(repaint_timer_, &QTimer::timeout, this, [this]() { update(); });
}

void EquityOrderBook::set_data(const QVector<QPair<double, double>>& bids, const QVector<QPair<double, double>>& asks,
                               double spread, double spread_pct) {
    set_data(bids, asks, spread, spread_pct, {}, {});
}

void EquityOrderBook::set_data(const QVector<QPair<double, double>>& bids, const QVector<QPair<double, double>>& asks,
                               double spread, double spread_pct,
                               const QVector<int>& bid_orders, const QVector<int>& ask_orders) {
    QMutexLocker lock(&mutex_);
    bids_ = bids.mid(0, MAX_LEVELS);
    asks_ = asks.mid(0, MAX_LEVELS);
    bid_orders_ = bid_orders.mid(0, MAX_LEVELS);
    ask_orders_ = ask_orders.mid(0, MAX_LEVELS);
    spread_ = spread;
    spread_pct_ = spread_pct;
    cache_dirty_ = true;

    spread_label_->setText(QString("Spread: %1 (%2%)").arg(spread, 0, 'f', 2).arg(spread_pct, 0, 'f', 3));

    if (!repaint_timer_->isActive())
        repaint_timer_->start();
}

void EquityOrderBook::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    cache_dirty_ = true;
}

void EquityOrderBook::mousePressEvent(QMouseEvent* event) {
    QMutexLocker lock(&mutex_);
    const int canvas_y = event->pos().y() - HEADER_H;
    if (canvas_y < 0)
        return;

    const int canvas_h = height() - HEADER_H - SPREAD_H;
    const int half = canvas_h / 2;
    if (canvas_y < half) {
        const int row = (half - canvas_y) / ROW_H;
        if (row >= 0 && row < asks_.size())
            emit price_clicked(asks_[row].first);
    } else {
        const int row = (canvas_y - half) / ROW_H;
        if (row >= 0 && row < bids_.size())
            emit price_clicked(bids_[row].first);
    }
}

void EquityOrderBook::paintEvent(QPaintEvent* /*event*/) {
    if (cache_dirty_)
        rebuild_cache();
    QPainter p(this);
    p.drawPixmap(0, HEADER_H, cache_);
}

void EquityOrderBook::rebuild_cache() {
    QMutexLocker lock(&mutex_);
    const int w = width();
    const int h = height() - HEADER_H - SPREAD_H;
    if (w <= 0 || h <= 0)
        return;

    cache_ = QPixmap(w, h);
    cache_.fill(bg_surface());

    QPainter p(&cache_);
    p.setFont(QFont("Consolas", 10));

    const int half = h / 2;

    if (bids_.isEmpty() && asks_.isEmpty()) {
        p.setPen(text_secondary());
        p.drawText(QRect(0, 0, w, h), Qt::AlignCenter, "No depth data");
        cache_dirty_ = false;
        return;
    }

    const bool has_orders = !bid_orders_.isEmpty() || !ask_orders_.isEmpty();
    const int col_price = 4;
    const int col_qty = has_orders ? w * 2 / 5 : w / 2;
    const int col_ord = w * 7 / 10;
    const int qty_w = (has_orders ? w * 3 / 10 - col_qty : w / 2 - 4);
    const int ord_w = w - col_ord - 4;

    double max_qty = 1.0;
    for (const auto& [price, qty] : bids_)
        max_qty = std::max(max_qty, qty);
    for (const auto& [price, qty] : asks_)
        max_qty = std::max(max_qty, qty);

    p.setPen(text_secondary());
    QFont hdr_font("Consolas", 9);
    hdr_font.setBold(true);
    p.setFont(hdr_font);
    p.drawText(col_price, 0, col_qty - col_price - 4, ROW_H, Qt::AlignLeft | Qt::AlignVCenter, "PRICE");
    p.drawText(col_qty, 0, qty_w, ROW_H, Qt::AlignRight | Qt::AlignVCenter, "QTY");
    if (has_orders)
        p.drawText(col_ord, 0, ord_w, ROW_H, Qt::AlignRight | Qt::AlignVCenter, "ORDERS");
    p.setFont(QFont("Consolas", 10));

    const int data_top = ROW_H;

    // Draw asks (top half, bottom-up so best ask is near spread)
    for (int i = 0; i < asks_.size() && i < MAX_LEVELS; ++i) {
        const auto& [price, qty] = asks_[i];
        const int y = half - (i + 1) * ROW_H;
        if (y < data_top)
            break;

        const double ratio = qty / max_qty;
        const int bar_w = static_cast<int>(w * ratio * 0.4);
        QColor sell_bar(color_sell());
        sell_bar.setAlpha(25);
        p.fillRect(w - bar_w, y, bar_w, ROW_H, sell_bar);

        p.setPen(color_sell());
        p.drawText(col_price, y, col_qty - col_price - 4, ROW_H, Qt::AlignLeft | Qt::AlignVCenter,
                   QString::number(price, 'f', 2));
        p.setPen(text_secondary());
        p.drawText(col_qty, y, qty_w, ROW_H, Qt::AlignRight | Qt::AlignVCenter, QString::number(qty, 'f', 0));
        if (has_orders && i < ask_orders_.size() && ask_orders_[i] > 0) {
            p.drawText(col_ord, y, ord_w, ROW_H, Qt::AlignRight | Qt::AlignVCenter,
                       QString::number(ask_orders_[i]));
        }
    }

    // Draw bids (bottom half, top-down so best bid is near spread)
    for (int i = 0; i < bids_.size() && i < MAX_LEVELS; ++i) {
        const auto& [price, qty] = bids_[i];
        const int y = half + i * ROW_H;
        if (y + ROW_H > h)
            break;

        const double ratio = qty / max_qty;
        const int bar_w = static_cast<int>(w * ratio * 0.4);
        QColor buy_bar(color_buy());
        buy_bar.setAlpha(25);
        p.fillRect(w - bar_w, y, bar_w, ROW_H, buy_bar);

        p.setPen(color_buy());
        p.drawText(col_price, y, col_qty - col_price - 4, ROW_H, Qt::AlignLeft | Qt::AlignVCenter,
                   QString::number(price, 'f', 2));
        p.setPen(text_secondary());
        p.drawText(col_qty, y, qty_w, ROW_H, Qt::AlignRight | Qt::AlignVCenter, QString::number(qty, 'f', 0));
        if (has_orders && i < bid_orders_.size() && bid_orders_[i] > 0) {
            p.drawText(col_ord, y, ord_w, ROW_H, Qt::AlignRight | Qt::AlignVCenter,
                       QString::number(bid_orders_[i]));
        }
    }

    // Center line
    p.setPen(border_med());
    p.drawLine(0, half, w, half);

    cache_dirty_ = false;
}

} // namespace fincept::screens::equity
