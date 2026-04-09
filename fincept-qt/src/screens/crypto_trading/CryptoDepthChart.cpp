// CryptoDepthChart.cpp — custom-painted cumulative bid/ask depth area chart
#include "screens/crypto_trading/CryptoDepthChart.h"

#include "ui/theme/ThemeManager.h"

#include <QMutexLocker>
#include <QPainter>
#include <QPainterPath>
#include <QShowEvent>

#include <algorithm>
#include <cmath>

namespace fincept::screens::crypto {

namespace {
inline QColor kBgBase() {
    return QColor(ui::ThemeManager::instance().tokens().bg_base);
}
inline QColor kBorderDim() {
    return QColor(ui::ThemeManager::instance().tokens().border_dim);
}
inline QColor kTextDim() {
    return QColor(ui::ThemeManager::instance().tokens().text_dim);
}
inline QColor kBidLine() {
    return QColor(ui::ThemeManager::instance().tokens().positive);
}
inline QColor kAskLine() {
    return QColor(ui::ThemeManager::instance().tokens().negative);
}
inline QColor kBidFill() {
    auto c = kBidLine();
    c.setAlpha(40);
    return c;
}
inline QColor kAskFill() {
    auto c = kAskLine();
    c.setAlpha(40);
    return c;
}
inline QColor kAmber() {
    return QColor(ui::ThemeManager::instance().tokens().accent);
}
} // namespace

CryptoDepthChart::CryptoDepthChart(QWidget* parent) : QWidget(parent) {
    setObjectName("cryptoDepthChart");

    connect(&ui::ThemeManager::instance(), &ui::ThemeManager::theme_changed, this, [this](const ui::ThemeTokens&) {
        cache_dirty_ = true;
        update();
    });

    repaint_timer_ = new QTimer(this);
    repaint_timer_->setSingleShot(true);
    repaint_timer_->setInterval(100); // max 10fps — depth chart changes slowly
    connect(repaint_timer_, &QTimer::timeout, this, [this]() { update(); });
}

void CryptoDepthChart::showEvent(QShowEvent* e) {
    QWidget::showEvent(e);
    if (cache_dirty_ && repaint_timer_ && !repaint_timer_->isActive())
        repaint_timer_->start();
}

void CryptoDepthChart::hideEvent(QHideEvent* e) {
    QWidget::hideEvent(e);
    if (repaint_timer_)
        repaint_timer_->stop();
}

void CryptoDepthChart::set_data(const QVector<QPair<double, double>>& bids, const QVector<QPair<double, double>>& asks,
                                double spread, double /*spread_pct*/) {
    {
        QMutexLocker lock(&mutex_);
        bids_ = bids;
        asks_ = asks;
        spread_ = spread;
    }
    cache_dirty_ = true;
    if (repaint_timer_ && !repaint_timer_->isActive())
        repaint_timer_->start();
}

void CryptoDepthChart::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    cache_dirty_ = true;
}

void CryptoDepthChart::paintEvent(QPaintEvent* /*event*/) {
    if (cache_dirty_)
        rebuild_cache();

    QPainter p(this);
    p.drawPixmap(0, 0, cache_);
}

void CryptoDepthChart::rebuild_cache() {
    const int w = width();
    const int h = height();
    if (w <= 0 || h <= 0)
        return;

    cache_ = QPixmap(w, h);
    cache_.fill(kBgBase());

    QVector<QPair<double, double>> bids, asks;
    {
        QMutexLocker lock(&mutex_);
        bids = bids_;
        asks = asks_;
    }

    if (bids.isEmpty() && asks.isEmpty()) {
        QPainter p(&cache_);
        p.setPen(kTextDim());
        p.setFont(QFont("Consolas", 10));
        p.drawText(QRect(0, 0, w, h), Qt::AlignCenter, "Waiting for order book data...");
        cache_dirty_ = false;
        return;
    }

    QPainter p(&cache_);
    // Antialiasing disabled for performance — depth chart has many line segments
    // and the visual difference is negligible at typical widget sizes (P9)
    p.setFont(QFont("Consolas", 8));

    const int margin_l = 50;
    const int margin_r = 10;
    const int margin_t = 10;
    const int margin_b = 20;
    const int plot_w = w - margin_l - margin_r;
    const int plot_h = h - margin_t - margin_b;

    // Build cumulative volumes
    QVector<QPair<double, double>> bid_cum, ask_cum; // (price, cumVol)
    {
        double cum = 0;
        for (const auto& b : bids) {
            cum += b.second;
            bid_cum.append({b.first, cum});
        }
    }
    {
        double cum = 0;
        for (const auto& a : asks) {
            cum += a.second;
            ask_cum.append({a.first, cum});
        }
    }

    // Find price and volume ranges
    double min_price = 1e18, max_price = 0;
    double max_vol = 0;
    for (const auto& b : bid_cum) {
        min_price = std::min(min_price, b.first);
        max_vol = std::max(max_vol, b.second);
    }
    for (const auto& a : ask_cum) {
        max_price = std::max(max_price, a.first);
        max_vol = std::max(max_vol, a.second);
    }
    if (!bid_cum.isEmpty())
        max_price = std::max(max_price, bid_cum.first().first);
    if (!ask_cum.isEmpty())
        min_price = std::min(min_price, ask_cum.first().first);

    if (max_price <= min_price || max_vol <= 0) {
        cache_dirty_ = false;
        return;
    }

    // Map functions
    auto map_x = [&](double price) -> int {
        return margin_l + static_cast<int>((price - min_price) / (max_price - min_price) * plot_w);
    };
    auto map_y = [&](double vol) -> int { return margin_t + plot_h - static_cast<int>(vol / max_vol * plot_h); };

    // Grid lines
    p.setPen(QPen(kBorderDim(), 1));
    for (int i = 0; i <= 4; ++i) {
        const int y = margin_t + plot_h * i / 4;
        p.drawLine(margin_l, y, w - margin_r, y);
    }

    // Bid area (left side, green)
    if (!bid_cum.isEmpty()) {
        QPainterPath bid_path;
        bid_path.moveTo(map_x(bid_cum.first().first), map_y(0));
        for (const auto& pt : bid_cum)
            bid_path.lineTo(map_x(pt.first), map_y(pt.second));
        bid_path.lineTo(map_x(bid_cum.last().first), map_y(0));
        bid_path.closeSubpath();

        p.fillPath(bid_path, kBidFill());
        p.setPen(QPen(kBidLine(), 1.5));
        // Draw just the top edge
        for (int i = 0; i + 1 < bid_cum.size(); ++i)
            p.drawLine(map_x(bid_cum[i].first), map_y(bid_cum[i].second), map_x(bid_cum[i + 1].first),
                       map_y(bid_cum[i + 1].second));
    }

    // Ask area (right side, red)
    if (!ask_cum.isEmpty()) {
        QPainterPath ask_path;
        ask_path.moveTo(map_x(ask_cum.first().first), map_y(0));
        for (const auto& pt : ask_cum)
            ask_path.lineTo(map_x(pt.first), map_y(pt.second));
        ask_path.lineTo(map_x(ask_cum.last().first), map_y(0));
        ask_path.closeSubpath();

        p.fillPath(ask_path, kAskFill());
        p.setPen(QPen(kAskLine(), 1.5));
        for (int i = 0; i + 1 < ask_cum.size(); ++i)
            p.drawLine(map_x(ask_cum[i].first), map_y(ask_cum[i].second), map_x(ask_cum[i + 1].first),
                       map_y(ask_cum[i + 1].second));
    }

    // Spread vertical line (amber dashed)
    if (!bid_cum.isEmpty() && !ask_cum.isEmpty()) {
        const double mid = (bid_cum.first().first + ask_cum.first().first) / 2.0;
        const int x_mid = map_x(mid);
        p.setPen(QPen(kAmber(), 1, Qt::DashLine));
        p.drawLine(x_mid, margin_t, x_mid, margin_t + plot_h);
    }

    // Volume axis labels
    p.setPen(kTextDim());
    for (int i = 0; i <= 4; ++i) {
        const double vol = max_vol * (4 - i) / 4;
        const int y = margin_t + plot_h * i / 4;
        QString label;
        if (vol >= 1e6)
            label = QString("%1M").arg(vol / 1e6, 0, 'f', 1);
        else if (vol >= 1e3)
            label = QString("%1K").arg(vol / 1e3, 0, 'f', 1);
        else
            label = QString::number(vol, 'f', 1);
        p.drawText(QRect(0, y - 8, margin_l - 4, 16), Qt::AlignRight | Qt::AlignVCenter, label);
    }

    // Price axis labels (bottom)
    for (int i = 0; i <= 4; ++i) {
        const double price = min_price + (max_price - min_price) * i / 4;
        const int x = map_x(price);
        p.drawText(QRect(x - 30, margin_t + plot_h + 2, 60, 16), Qt::AlignCenter, QString::number(price, 'f', 0));
    }

    cache_dirty_ = false;
}

} // namespace fincept::screens::crypto
