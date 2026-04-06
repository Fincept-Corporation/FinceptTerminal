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
static QColor bg_surface()     { return colors::BG_SURFACE; }
static QColor color_sell()     { return colors::NEGATIVE; }
static QColor color_buy()      { return colors::POSITIVE; }
static QColor text_secondary() { return colors::TEXT_SECONDARY; }
static QColor border_med()     { return colors::BORDER_MED; }
} // namespace

#include <algorithm>
#include <cmath>

namespace fincept::screens::equity {

EquityOrderBook::EquityOrderBook(QWidget* parent) : QWidget(parent) {
    setObjectName("eqOrderBook");

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    // Header
    auto* header = new QWidget;
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

    // Canvas — the actual painted area
    canvas_ = new QWidget;
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
    QMutexLocker lock(&mutex_);
    bids_ = bids.mid(0, MAX_LEVELS);
    asks_ = asks.mid(0, MAX_LEVELS);
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
    // Determine which price level was clicked
    QMutexLocker lock(&mutex_);
    const int canvas_y = event->pos().y() - HEADER_H;
    if (canvas_y < 0)
        return;

    const int half = (canvas_->height()) / 2;
    if (canvas_y < half) {
        // Asks area (top, reversed)
        const int row = (half - canvas_y) / ROW_H;
        if (row >= 0 && row < asks_.size())
            emit price_clicked(asks_[row].first);
    } else {
        // Bids area (bottom)
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
    const int col_price = 4;
    const int col_qty = w / 2;
    const int col_total = w * 3 / 4;

    // Find max quantity for bar scaling
    double max_qty = 1.0;
    for (const auto& [price, qty] : bids_)
        max_qty = std::max(max_qty, qty);
    for (const auto& [price, qty] : asks_)
        max_qty = std::max(max_qty, qty);

    // Draw asks (top half, bottom-up so best ask is near spread)
    for (int i = 0; i < asks_.size() && i < MAX_LEVELS; ++i) {
        const auto& [price, qty] = asks_[i];
        const int y = half - (i + 1) * ROW_H;
        if (y < 0)
            break;

        // Depth bar
        const double ratio = qty / max_qty;
        const int bar_w = static_cast<int>(w * ratio * 0.4);
        p.fillRect(w - bar_w, y, bar_w, ROW_H, QColor(220, 38, 38, 25));

        p.setPen(color_sell());
        p.drawText(col_price, y, w / 2 - 8, ROW_H, Qt::AlignLeft | Qt::AlignVCenter, QString::number(price, 'f', 2));
        p.setPen(text_secondary());
        p.drawText(col_qty, y, w / 4 - 4, ROW_H, Qt::AlignRight | Qt::AlignVCenter, QString::number(qty, 'f', 0));
    }

    // Draw bids (bottom half, top-down so best bid is near spread)
    for (int i = 0; i < bids_.size() && i < MAX_LEVELS; ++i) {
        const auto& [price, qty] = bids_[i];
        const int y = half + i * ROW_H;
        if (y + ROW_H > h)
            break;

        const double ratio = qty / max_qty;
        const int bar_w = static_cast<int>(w * ratio * 0.4);
        p.fillRect(w - bar_w, y, bar_w, ROW_H, QColor(22, 163, 74, 25));

        p.setPen(color_buy());
        p.drawText(col_price, y, w / 2 - 8, ROW_H, Qt::AlignLeft | Qt::AlignVCenter, QString::number(price, 'f', 2));
        p.setPen(text_secondary());
        p.drawText(col_qty, y, w / 4 - 4, ROW_H, Qt::AlignRight | Qt::AlignVCenter, QString::number(qty, 'f', 0));
    }

    // Center line
    p.setPen(border_med());
    p.drawLine(0, half, w, half);

    cache_dirty_ = false;
}

} // namespace fincept::screens::equity
