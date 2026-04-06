// CryptoOrderBook.cpp — custom-painted dual-column order book with depth bars
#include "screens/crypto_trading/CryptoOrderBook.h"
#include "ui/theme/ThemeManager.h"

#include <QDateTime>
#include <QHBoxLayout>
#include <QMouseEvent>
#include <QMutexLocker>
#include <QPainter>
#include <QShowEvent>
#include <QStyle>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>

namespace fincept::screens::crypto {

namespace {
// Live color accessors — reflect active theme at call time
inline QColor kBgBase()       { return QColor(ui::ThemeManager::instance().tokens().bg_base); }
inline QColor kBgSurface()    { return QColor(ui::ThemeManager::instance().tokens().bg_surface); }
inline QColor kBgRaised()     { return QColor(ui::ThemeManager::instance().tokens().bg_raised); }
inline QColor kBorderDim()    { return QColor(ui::ThemeManager::instance().tokens().border_dim); }
inline QColor kTextPrimary()  { return QColor(ui::ThemeManager::instance().tokens().text_primary); }
inline QColor kTextSecondary(){ return QColor(ui::ThemeManager::instance().tokens().text_secondary); }
inline QColor kTextTertiary() { return QColor(ui::ThemeManager::instance().tokens().text_tertiary); }
inline QColor kTextDim()      { return QColor(ui::ThemeManager::instance().tokens().text_dim); }
inline QColor kColorBid()     { return QColor(ui::ThemeManager::instance().tokens().positive); }
inline QColor kColorAsk()     { return QColor(ui::ThemeManager::instance().tokens().negative); }
inline QColor kColorAmber()   { return QColor(ui::ThemeManager::instance().tokens().accent); }
inline QColor kBidBar()       { auto c = kColorBid();   c.setAlpha(35); return c; }
inline QColor kAskBar()       { auto c = kColorAsk();   c.setAlpha(35); return c; }
inline QColor kBidBarHot()    { auto c = kColorBid();   c.setAlpha(65); return c; }
inline QColor kAskBarHot()    { auto c = kColorAsk();   c.setAlpha(65); return c; }
inline QColor kRowEven()      { return QColor(ui::ThemeManager::instance().tokens().bg_base); }
inline QColor kRowOdd()       { return QColor(ui::ThemeManager::instance().tokens().row_alt); }
} // namespace

CryptoOrderBook::CryptoOrderBook(QWidget* parent) : QWidget(parent) {
    setObjectName("cryptoOrderBook");
    connect(&ui::ThemeManager::instance(), &ui::ThemeManager::theme_changed,
            this, [this](const ui::ThemeTokens&) { cache_dirty_ = true; update(); });

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    // Header with mode toggle buttons
    auto* header = new QWidget;
    header->setObjectName("cryptoObHeader");
    header->setFixedHeight(HEADER_H);
    auto* h_layout = new QHBoxLayout(header);
    h_layout->setContentsMargins(8, 0, 8, 0);
    h_layout->setSpacing(2);

    auto* title = new QLabel("ORDER BOOK");
    title->setObjectName("cryptoObTitle");
    h_layout->addWidget(title);
    h_layout->addStretch();

    const char* mode_labels[] = {"Book", "Vol", "Imb", "Sig"};
    for (int i = 0; i < 4; ++i) {
        mode_btns_[i] = new QPushButton(mode_labels[i]);
        mode_btns_[i]->setObjectName("cryptoObModeBtn");
        mode_btns_[i]->setFixedHeight(22);
        mode_btns_[i]->setCursor(Qt::PointingHandCursor);
        if (i == 0)
            mode_btns_[i]->setProperty("active", true);
        connect(mode_btns_[i], &QPushButton::clicked, this, [this, i]() { set_active_mode(i); });
        h_layout->addWidget(mode_btns_[i]);
    }
    layout->addWidget(header);

    // Spread label
    spread_label_ = new QLabel("Spread: --");
    spread_label_->setObjectName("cryptoObSpread");
    spread_label_->setAlignment(Qt::AlignCenter);
    spread_label_->setFixedHeight(SPREAD_H);
    // Spread is positioned between asks and bids in paintEvent conceptually,
    // but as a real widget it sits between header and the painted area
    layout->addWidget(spread_label_);

    // The rest is custom-painted
    layout->addStretch(1);

    setMinimumHeight(200);

    // Coalesce repaint: max 20fps instead of per-message
    repaint_timer_ = new QTimer(this);
    repaint_timer_->setSingleShot(true);
    repaint_timer_->setInterval(50);
    connect(repaint_timer_, &QTimer::timeout, this, [this]() { update(); });
}

void CryptoOrderBook::showEvent(QShowEvent* e) {
    QWidget::showEvent(e);
    // Resume repaint timer when visible — only if there is dirty data to show
    if (cache_dirty_ && repaint_timer_ && !repaint_timer_->isActive())
        repaint_timer_->start();
}

void CryptoOrderBook::hideEvent(QHideEvent* e) {
    QWidget::hideEvent(e);
    // Stop repaint timer to avoid wasting CPU painting an invisible widget
    if (repaint_timer_)
        repaint_timer_->stop();
}

void CryptoOrderBook::set_active_mode(int idx) {
    view_mode_ = static_cast<ObViewMode>(idx);
    for (int i = 0; i < 4; ++i) {
        mode_btns_[i]->setProperty("active", i == idx);
        mode_btns_[i]->style()->unpolish(mode_btns_[i]);
        mode_btns_[i]->style()->polish(mode_btns_[i]);
    }
    cache_dirty_ = true;
    if (repaint_timer_ && !repaint_timer_->isActive())
        repaint_timer_->start();
}

void CryptoOrderBook::set_data(const QVector<QPair<double, double>>& bids, const QVector<QPair<double, double>>& asks,
                               double spread, double spread_pct) {
    {
        QMutexLocker lock(&mutex_);
        bids_ = bids;
        asks_ = asks;
        spread_ = spread;
        spread_pct_ = spread_pct;
    }
    spread_label_->setText(QString("SPREAD  %1  (%2%)").arg(spread, 0, 'f', 2).arg(spread_pct, 0, 'f', 4));
    cache_dirty_ = true;
    if (repaint_timer_ && !repaint_timer_->isActive())
        repaint_timer_->start();
}

void CryptoOrderBook::add_tick_snapshot(const TickSnapshot& snap) {
    QMutexLocker lock(&mutex_);
    tick_history_.append(snap);
    if (tick_history_.size() > OB_MAX_TICK_HISTORY)
        tick_history_.remove(0, tick_history_.size() - OB_MAX_TICK_HISTORY);
}

void CryptoOrderBook::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    cache_dirty_ = true;
}

void CryptoOrderBook::mousePressEvent(QMouseEvent* event) {
    if (view_mode_ != ObViewMode::Book)
        return;

    // Calculate which row was clicked in the paint area
    const int paint_y = event->pos().y() - (HEADER_H + SPREAD_H);
    if (paint_y < 0)
        return;

    const int row = paint_y / ROW_H;
    QMutexLocker lock(&mutex_);
    const int ask_count = std::min(static_cast<int>(asks_.size()), OB_MAX_DISPLAY_LEVELS);
    const int bid_count = std::min(static_cast<int>(bids_.size()), OB_MAX_DISPLAY_LEVELS);

    if (row < ask_count) {
        // Clicked an ask row (displayed in reverse)
        const int src = ask_count - 1 - row;
        if (src < asks_.size())
            emit price_clicked(asks_[src].first);
    } else if (row < ask_count + bid_count) {
        const int bid_idx = row - ask_count;
        if (bid_idx < bids_.size())
            emit price_clicked(bids_[bid_idx].first);
    }
}

void CryptoOrderBook::paintEvent(QPaintEvent* /*event*/) {
    if (cache_dirty_)
        rebuild_cache();

    QPainter p(this);
    // Paint cache starting below header + spread
    const int y_offset = HEADER_H + SPREAD_H;
    p.drawPixmap(0, y_offset, cache_);
}

void CryptoOrderBook::rebuild_cache() {
    const int w = width();
    const int h = height() - HEADER_H - SPREAD_H;
    if (w <= 0 || h <= 0)
        return;

    cache_ = QPixmap(w, h);
    cache_.fill(kBgBase());

    QPainter p(&cache_);
    p.setFont(QFont("Consolas", 9));

    QVector<QPair<double, double>> bids, asks;
    QVector<TickSnapshot> history;
    {
        QMutexLocker lock(&mutex_);
        bids = bids_;
        asks = asks_;
        history = tick_history_;
    }

    if (view_mode_ == ObViewMode::Book || view_mode_ == ObViewMode::Volume) {
        // ── Dual-column order book with depth bars ──
        const int ask_count = std::min(static_cast<int>(asks.size()), OB_MAX_DISPLAY_LEVELS);
        const int bid_count = std::min(static_cast<int>(bids.size()), OB_MAX_DISPLAY_LEVELS);
        const int half_w = w / 2;
        const int price_col_w = half_w - 10;

        // Calculate cumulative volumes for bar scaling
        double max_cum = 0;
        QVector<double> ask_cum(ask_count, 0), bid_cum(bid_count, 0);
        {
            double cum = 0;
            for (int i = 0; i < ask_count; ++i) {
                cum += asks[i].second;
                ask_cum[i] = cum;
            }
            max_cum = std::max(max_cum, cum);
        }
        {
            double cum = 0;
            for (int i = 0; i < bid_count; ++i) {
                cum += bids[i].second;
                bid_cum[i] = cum;
            }
            max_cum = std::max(max_cum, cum);
        }
        if (max_cum <= 0)
            max_cum = 1;

        // Calculate 75th percentile for heat coloring
        QVector<double> all_vols;
        all_vols.reserve(ask_count + bid_count);
        for (int i = 0; i < ask_count; ++i)
            all_vols.append(asks[i].second);
        for (int i = 0; i < bid_count; ++i)
            all_vols.append(bids[i].second);
        std::sort(all_vols.begin(), all_vols.end());
        const double p75 = all_vols.isEmpty() ? 0 : all_vols[all_vols.size() * 3 / 4];

        // Column headers
        p.setPen(kTextDim());
        p.drawText(QRect(4, 0, price_col_w, ROW_H), Qt::AlignLeft | Qt::AlignVCenter, "BID");
        p.drawText(QRect(4, 0, half_w - 4, ROW_H), Qt::AlignRight | Qt::AlignVCenter, "QTY");
        p.drawText(QRect(half_w + 4, 0, price_col_w, ROW_H), Qt::AlignLeft | Qt::AlignVCenter, "ASK");
        p.drawText(QRect(half_w + 4, 0, half_w - 8, ROW_H), Qt::AlignRight | Qt::AlignVCenter, "QTY");

        // Draw bid side (left) — prices descending from center
        const int rows_start_y = ROW_H;
        for (int i = 0; i < bid_count && rows_start_y + i * ROW_H < h; ++i) {
            const int y = rows_start_y + i * ROW_H;
            const QColor row_bg = (i % 2 == 0) ? kRowEven() : kRowOdd();
            p.fillRect(0, y, half_w - 1, ROW_H, row_bg);

            // Depth bar (grows from right to left)
            const double ratio = bid_cum[i] / max_cum;
            const int bar_w = static_cast<int>((half_w - 4) * ratio);
            const bool hot = bids[i].second >= p75 && p75 > 0;
            p.fillRect(half_w - 1 - bar_w, y, bar_w, ROW_H, hot ? kBidBarHot() : kBidBar());

            // Price text
            p.setPen(kColorBid());
            p.drawText(QRect(4, y, price_col_w, ROW_H), Qt::AlignLeft | Qt::AlignVCenter,
                       QString::number(bids[i].first, 'f', 2));

            // Amount text
            p.setPen(kTextSecondary());
            p.drawText(QRect(4, y, half_w - 8, ROW_H), Qt::AlignRight | Qt::AlignVCenter,
                       QString::number(bids[i].second, 'f', 4));
        }

        // Draw ask side (right) — prices ascending from center
        for (int i = 0; i < ask_count && rows_start_y + i * ROW_H < h; ++i) {
            const int y = rows_start_y + i * ROW_H;
            const QColor row_bg = (i % 2 == 0) ? kRowEven() : kRowOdd();
            p.fillRect(half_w + 1, y, half_w - 1, ROW_H, row_bg);

            // Depth bar (grows from left to right)
            const double ratio = ask_cum[i] / max_cum;
            const int bar_w = static_cast<int>((half_w - 4) * ratio);
            const bool hot = asks[i].second >= p75 && p75 > 0;
            p.fillRect(half_w + 1, y, bar_w, ROW_H, hot ? kAskBarHot() : kAskBar());

            // Price text
            p.setPen(kColorAsk());
            p.drawText(QRect(half_w + 4, y, price_col_w, ROW_H), Qt::AlignLeft | Qt::AlignVCenter,
                       QString::number(asks[i].first, 'f', 2));

            // Amount text
            p.setPen(kTextSecondary());
            p.drawText(QRect(half_w + 4, y, half_w - 8, ROW_H), Qt::AlignRight | Qt::AlignVCenter,
                       QString::number(asks[i].second, 'f', 4));
        }

        // Center divider line
        p.setPen(kBorderDim());
        p.drawLine(half_w, 0, half_w, h);

    } else {
        // ── Imbalance / Signals mode — list view ──
        const int count = std::min(static_cast<int>(history.size()), 30);
        const bool is_signals = (view_mode_ == ObViewMode::Signals);

        // Header
        p.setPen(kTextDim());
        if (is_signals) {
            p.drawText(QRect(4, 0, w / 3, ROW_H), Qt::AlignLeft | Qt::AlignVCenter, "TIME");
            p.drawText(QRect(w / 3, 0, w / 3, ROW_H), Qt::AlignRight | Qt::AlignVCenter, "RISE%");
            p.drawText(QRect(2 * w / 3, 0, w / 3 - 4, ROW_H), Qt::AlignRight | Qt::AlignVCenter, "ACTION");
        } else {
            p.drawText(QRect(4, 0, w / 3, ROW_H), Qt::AlignLeft | Qt::AlignVCenter, "TIME");
            p.drawText(QRect(w / 3, 0, w / 3, ROW_H), Qt::AlignRight | Qt::AlignVCenter, "IMBALANCE");
            p.drawText(QRect(2 * w / 3, 0, w / 3 - 4, ROW_H), Qt::AlignRight | Qt::AlignVCenter, "SIGNAL");
        }

        for (int i = 0; i < count && (i + 1) * ROW_H < h; ++i) {
            const int idx = history.size() - 1 - i;
            const auto& snap = history[idx];
            const int y = (i + 1) * ROW_H;
            const QColor row_bg = (i % 2 == 0) ? kRowEven() : kRowOdd();
            p.fillRect(0, y, w, ROW_H, row_bg);

            // Time
            p.setPen(kTextDim());
            p.drawText(QRect(4, y, w / 3, ROW_H), Qt::AlignLeft | Qt::AlignVCenter,
                       QDateTime::fromMSecsSinceEpoch(snap.timestamp).toString("HH:mm:ss"));

            if (is_signals) {
                // Rise %
                p.setPen(snap.rise_ratio_60 >= 0 ? kColorBid() : kColorAsk());
                p.drawText(QRect(w / 3, y, w / 3, ROW_H), Qt::AlignRight | Qt::AlignVCenter,
                           QString("%1%").arg(snap.rise_ratio_60 * 100.0, 0, 'f', 2));

                // Action
                QString action = "HOLD";
                QColor c = kTextTertiary();
                if (snap.imbalance > OB_IMBALANCE_BUY_THRESHOLD && snap.rise_ratio_60 > 0.001) {
                    action = "STRONG BUY";
                    c = kColorBid();
                } else if (snap.imbalance < OB_IMBALANCE_SELL_THRESHOLD && snap.rise_ratio_60 < -0.001) {
                    action = "STRONG SELL";
                    c = kColorAsk();
                } else if (snap.imbalance > OB_IMBALANCE_BUY_THRESHOLD) {
                    action = "BUY";
                    c = kColorBid();
                } else if (snap.imbalance < OB_IMBALANCE_SELL_THRESHOLD) {
                    action = "SELL";
                    c = kColorAsk();
                }
                p.setPen(c);
                p.drawText(QRect(2 * w / 3, y, w / 3 - 4, ROW_H), Qt::AlignRight | Qt::AlignVCenter, action);
            } else {
                // Imbalance
                p.setPen(snap.imbalance > 0 ? kColorBid() : kColorAsk());
                p.drawText(QRect(w / 3, y, w / 3, ROW_H), Qt::AlignRight | Qt::AlignVCenter,
                           QString("%1").arg(snap.imbalance, 0, 'f', 3));

                // Signal
                QString signal = "NEUTRAL";
                QColor c = kTextTertiary();
                if (snap.imbalance > OB_IMBALANCE_BUY_THRESHOLD) {
                    signal = "BUY PRESSURE";
                    c = kColorBid();
                } else if (snap.imbalance < OB_IMBALANCE_SELL_THRESHOLD) {
                    signal = "SELL PRESSURE";
                    c = kColorAsk();
                }
                p.setPen(c);
                p.drawText(QRect(2 * w / 3, y, w / 3 - 4, ROW_H), Qt::AlignRight | Qt::AlignVCenter, signal);
            }
        }
    }

    cache_dirty_ = false;
}

} // namespace fincept::screens::crypto
