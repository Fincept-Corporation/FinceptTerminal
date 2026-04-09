// CryptoTimeSales.cpp — custom-painted real-time trade tape
#include "screens/crypto_trading/CryptoTimeSales.h"

#include "ui/theme/Theme.h"

#include <QDateTime>
#include <QMutexLocker>
#include <QPainter>
#include <QShowEvent>

namespace fincept::screens::crypto {

using namespace fincept::ui;

namespace {
inline QColor kBgBase()    { return QColor(colors::BG_BASE()); }
inline QColor kRowEven()   { return QColor(colors::BG_BASE()); }
inline QColor kRowOdd()    { return QColor(colors::ROW_ALT()); }
inline QColor kTextDim()   { return QColor(colors::TEXT_DIM()); }
inline QColor kTextSec()   { return QColor(colors::TEXT_SECONDARY()); }
inline QColor kColorBuy()  { return QColor(colors::POSITIVE()); }
inline QColor kColorSell() { return QColor(colors::NEGATIVE()); }
} // namespace

CryptoTimeSales::CryptoTimeSales(QWidget* parent) : QWidget(parent) {
    setObjectName("cryptoTimeSales");
    trades_.reserve(MAX_TIME_SALES_TRADES);

    repaint_timer_ = new QTimer(this);
    repaint_timer_->setSingleShot(true);
    repaint_timer_->setInterval(50); // max 20fps
    connect(repaint_timer_, &QTimer::timeout, this, [this]() { update(); });
    connect(&ui::ThemeManager::instance(), &ui::ThemeManager::theme_changed, this, [this]() { update(); });
}

void CryptoTimeSales::showEvent(QShowEvent* e) {
    QWidget::showEvent(e);
    if (cache_dirty_ && repaint_timer_ && !repaint_timer_->isActive())
        repaint_timer_->start();
}

void CryptoTimeSales::hideEvent(QHideEvent* e) {
    QWidget::hideEvent(e);
    if (repaint_timer_)
        repaint_timer_->stop();
}

void CryptoTimeSales::add_trade(const TradeEntry& trade) {
    {
        QMutexLocker lock(&mutex_);
        trades_.prepend(trade); // newest first
        if (trades_.size() > MAX_TIME_SALES_TRADES)
            trades_.resize(MAX_TIME_SALES_TRADES);
    }
    cache_dirty_ = true;
    if (repaint_timer_ && !repaint_timer_->isActive())
        repaint_timer_->start();
}

void CryptoTimeSales::set_trades(const QVector<TradeEntry>& trades) {
    {
        QMutexLocker lock(&mutex_);
        trades_ = trades;
        if (trades_.size() > MAX_TIME_SALES_TRADES)
            trades_.resize(MAX_TIME_SALES_TRADES);
    }
    cache_dirty_ = true;
    if (repaint_timer_ && !repaint_timer_->isActive())
        repaint_timer_->start();
}

void CryptoTimeSales::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    cache_dirty_ = true;
}

void CryptoTimeSales::paintEvent(QPaintEvent* /*event*/) {
    if (cache_dirty_)
        rebuild_cache();

    QPainter p(this);
    p.drawPixmap(0, 0, cache_);
}

void CryptoTimeSales::rebuild_cache() {
    const int w = width();
    const int h = height();
    if (w <= 0 || h <= 0)
        return;

    cache_ = QPixmap(w, h);
    cache_.fill(kBgBase());

    QPainter p(&cache_);
    p.setFont(QFont("Consolas", 9));

    QVector<TradeEntry> snapshot;
    {
        QMutexLocker lock(&mutex_);
        snapshot = trades_;
    }

    // Header row
    p.setPen(kTextDim());
    p.drawText(QRect(4, 0, 70, ROW_H), Qt::AlignLeft | Qt::AlignVCenter, "TIME");
    p.drawText(QRect(74, 0, w / 3, ROW_H), Qt::AlignRight | Qt::AlignVCenter, "PRICE");
    p.drawText(QRect(74 + w / 3, 0, w / 3, ROW_H), Qt::AlignRight | Qt::AlignVCenter, "QTY");
    p.drawText(QRect(w - 30, 0, 26, ROW_H), Qt::AlignCenter | Qt::AlignVCenter, "S");

    const int max_rows = (h - ROW_H) / ROW_H;
    const int count = std::min(static_cast<int>(snapshot.size()), max_rows);

    for (int i = 0; i < count; ++i) {
        const auto& t = snapshot[i];
        const int y = (i + 1) * ROW_H;
        const QColor bg = (i % 2 == 0) ? kRowEven() : kRowOdd();
        p.fillRect(0, y, w, ROW_H, bg);

        const bool is_buy = (t.side == "buy");
        const QColor price_color = is_buy ? kColorBuy() : kColorSell();

        // Time
        p.setPen(kTextDim());
        p.drawText(QRect(4, y, 70, ROW_H), Qt::AlignLeft | Qt::AlignVCenter,
                   QDateTime::fromMSecsSinceEpoch(t.timestamp).toString("HH:mm:ss"));

        // Price
        p.setPen(price_color);
        p.drawText(QRect(74, y, w / 3, ROW_H), Qt::AlignRight | Qt::AlignVCenter, QString::number(t.price, 'f', 2));

        // Quantity
        p.setPen(kTextSec());
        p.drawText(QRect(74 + w / 3, y, w / 3, ROW_H), Qt::AlignRight | Qt::AlignVCenter,
                   QString::number(t.amount, 'f', 4));

        // Side indicator
        p.setPen(price_color);
        p.drawText(QRect(w - 30, y, 26, ROW_H), Qt::AlignCenter | Qt::AlignVCenter, is_buy ? "B" : "S");
    }

    if (count == 0) {
        p.setPen(kTextDim());
        p.drawText(rect(), Qt::AlignCenter, "Waiting for trades...");
    }

    cache_dirty_ = false;
}

} // namespace fincept::screens::crypto
