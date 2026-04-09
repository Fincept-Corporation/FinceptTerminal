#include "screens/polymarket/PolymarketOrderBook.h"

#include "ui/theme/Theme.h"
#include "ui/theme/ThemeManager.h"

#include <QMouseEvent>
#include <QPainter>
#include <QVBoxLayout>

#include <algorithm>

namespace fincept::screens::polymarket {

using namespace fincept::ui;
using namespace fincept::services::polymarket;

PolymarketOrderBook::PolymarketOrderBook(QWidget* parent) : QWidget(parent) {
    setMinimumHeight(200);

    connect(&ui::ThemeManager::instance(), &ui::ThemeManager::theme_changed, this, [this](const ui::ThemeTokens&) {
        cache_dirty_ = true;
        update();
    });

    repaint_timer_ = new QTimer(this);
    repaint_timer_->setInterval(50); // 20fps max
    connect(repaint_timer_, &QTimer::timeout, this, [this]() {
        if (cache_dirty_) {
            rebuild_cache();
            update();
        }
    });
}

void PolymarketOrderBook::set_data(const OrderBook& book) {
    QMutexLocker lock(&mutex_);
    bids_ = book.bids;
    asks_ = book.asks;

    // Calculate spread
    double best_bid = bids_.isEmpty() ? 0 : bids_[0].price;
    double best_ask = asks_.isEmpty() ? 0 : asks_[0].price;
    spread_ = (best_bid > 0 && best_ask > 0) ? (best_ask - best_bid) : 0;

    cache_dirty_ = true;
}

void PolymarketOrderBook::clear() {
    QMutexLocker lock(&mutex_);
    bids_.clear();
    asks_.clear();
    spread_ = 0;
    cache_dirty_ = true;
    update();
}

void PolymarketOrderBook::showEvent(QShowEvent* e) {
    QWidget::showEvent(e);
    repaint_timer_->start();
}

void PolymarketOrderBook::hideEvent(QHideEvent* e) {
    QWidget::hideEvent(e);
    repaint_timer_->stop();
}

void PolymarketOrderBook::resizeEvent(QResizeEvent* e) {
    QWidget::resizeEvent(e);
    cache_dirty_ = true;
}

void PolymarketOrderBook::paintEvent(QPaintEvent*) {
    if (cache_dirty_)
        rebuild_cache();
    QPainter p(this);
    p.drawPixmap(0, 0, cache_);
}

void PolymarketOrderBook::mousePressEvent(QMouseEvent* event) {
    int y = event->pos().y();
    if (y < HEADER_HEIGHT)
        return;

    QMutexLocker lock(&mutex_);
    int row_idx = (y - HEADER_HEIGHT) / ROW_HEIGHT;
    int ask_rows = qMin(asks_.size(), 10);

    if (row_idx < ask_rows && row_idx < asks_.size()) {
        int ask_i = ask_rows - 1 - row_idx;
        emit price_clicked(asks_[ask_i].price);
    } else {
        int bid_idx = row_idx - ask_rows - 1; // -1 for spread row
        if (bid_idx >= 0 && bid_idx < bids_.size()) {
            emit price_clicked(bids_[bid_idx].price);
        }
    }
}

void PolymarketOrderBook::rebuild_cache() {
    QMutexLocker lock(&mutex_);
    cache_dirty_ = false;

    int w = width();
    int h = height();
    if (w <= 0 || h <= 0)
        return;

    cache_ = QPixmap(w, h);
    cache_.fill(QColor(colors::BG_BASE()));
    QPainter p(&cache_);
    p.setRenderHint(QPainter::Antialiasing, false);

    QFont header_font(fonts::DATA_FAMILY, 9);
    header_font.setWeight(QFont::Bold);
    QFont data_font(fonts::DATA_FAMILY, 10);

    // Header
    p.fillRect(0, 0, w, HEADER_HEIGHT, QColor(colors::BG_RAISED()));
    p.setFont(header_font);
    p.setPen(QColor(colors::TEXT_SECONDARY()));
    int col_w = w / 3;
    p.drawText(4, 0, col_w, HEADER_HEIGHT, Qt::AlignLeft | Qt::AlignVCenter, "PRICE");
    p.drawText(col_w, 0, col_w, HEADER_HEIGHT, Qt::AlignRight | Qt::AlignVCenter, "SIZE");
    p.drawText(2 * col_w, 0, col_w - 4, HEADER_HEIGHT, Qt::AlignRight | Qt::AlignVCenter, "DEPTH");

    // Separator
    p.setPen(QColor(colors::BORDER_DIM()));
    p.drawLine(0, HEADER_HEIGHT - 1, w, HEADER_HEIGHT - 1);

    int max_rows = (h - HEADER_HEIGHT) / ROW_HEIGHT;
    int ask_rows = qMin(asks_.size(), qMax(1, max_rows / 2 - 1));
    int bid_rows = qMin(bids_.size(), qMax(1, max_rows / 2 - 1));

    // Find max cumulative size for depth bars
    double max_depth = 0;
    double cum = 0;
    for (int i = 0; i < ask_rows; ++i) {
        cum += asks_[i].size;
        max_depth = qMax(max_depth, cum);
    }
    cum = 0;
    for (int i = 0; i < bid_rows; ++i) {
        cum += bids_[i].size;
        max_depth = qMax(max_depth, cum);
    }
    if (max_depth == 0)
        max_depth = 1;

    p.setFont(data_font);
    int y = HEADER_HEIGHT;

    // Asks (highest first → reversed)
    cum = 0;
    for (int i = ask_rows - 1; i >= 0; --i) {
        cum += asks_[i].size;
        double depth_pct = cum / max_depth;
        int bar_w = static_cast<int>(w * depth_pct);

        p.fillRect(w - bar_w, y, bar_w, ROW_HEIGHT, QColor(220, 38, 38, 25));

        p.setPen(QColor(colors::NEGATIVE()));
        p.drawText(4, y, col_w, ROW_HEIGHT, Qt::AlignLeft | Qt::AlignVCenter, QString::number(asks_[i].price, 'f', 2));
        p.setPen(QColor(colors::TEXT_SECONDARY()));
        p.drawText(col_w, y, col_w, ROW_HEIGHT, Qt::AlignRight | Qt::AlignVCenter,
                   QString::number(asks_[i].size, 'f', 1));
        p.drawText(2 * col_w, y, col_w - 4, ROW_HEIGHT, Qt::AlignRight | Qt::AlignVCenter,
                   QString::number(cum, 'f', 1));

        p.setPen(QColor(colors::BORDER_DIM()));
        p.drawLine(0, y + ROW_HEIGHT - 1, w, y + ROW_HEIGHT - 1);
        y += ROW_HEIGHT;
    }

    // Spread row
    p.fillRect(0, y, w, ROW_HEIGHT, QColor(colors::BG_RAISED()));
    p.setPen(QColor(colors::AMBER()));
    p.setFont(header_font);
    p.drawText(4, y, w - 8, ROW_HEIGHT, Qt::AlignCenter, QString("SPREAD %1").arg(QString::number(spread_, 'f', 4)));
    y += ROW_HEIGHT;

    // Bids (highest first)
    p.setFont(data_font);
    cum = 0;
    for (int i = 0; i < bid_rows; ++i) {
        cum += bids_[i].size;
        double depth_pct = cum / max_depth;
        int bar_w = static_cast<int>(w * depth_pct);

        p.fillRect(w - bar_w, y, bar_w, ROW_HEIGHT, QColor(22, 163, 74, 25));

        p.setPen(QColor(colors::POSITIVE()));
        p.drawText(4, y, col_w, ROW_HEIGHT, Qt::AlignLeft | Qt::AlignVCenter, QString::number(bids_[i].price, 'f', 2));
        p.setPen(QColor(colors::TEXT_SECONDARY()));
        p.drawText(col_w, y, col_w, ROW_HEIGHT, Qt::AlignRight | Qt::AlignVCenter,
                   QString::number(bids_[i].size, 'f', 1));
        p.drawText(2 * col_w, y, col_w - 4, ROW_HEIGHT, Qt::AlignRight | Qt::AlignVCenter,
                   QString::number(cum, 'f', 1));

        p.setPen(QColor(colors::BORDER_DIM()));
        p.drawLine(0, y + ROW_HEIGHT - 1, w, y + ROW_HEIGHT - 1);
        y += ROW_HEIGHT;
    }

    if (bids_.isEmpty() && asks_.isEmpty()) {
        p.setPen(QColor(colors::TEXT_DIM()));
        p.drawText(rect(), Qt::AlignCenter, "No order book data");
    }
}

} // namespace fincept::screens::polymarket
