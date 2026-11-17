// CryptoTimeSales.cpp — custom-painted real-time trade tape
#include "screens/crypto_trading/CryptoTimeSales.h"

#include "ui/formatting/FormatWidgets.h"
#include "ui/formatting/NumberFormat.h"
#include "ui/theme/Theme.h"

#include <QDateTime>
#include <QFontMetrics>
#include <QMutexLocker>
#include <QPainter>
#include <QShowEvent>

#include <algorithm>

namespace fincept::screens::crypto {

using namespace fincept::ui;

namespace {
inline QColor kBgBase() {
    return QColor(colors::BG_BASE());
}
inline QColor kRowEven() {
    return QColor(colors::BG_BASE());
}
inline QColor kRowOdd() {
    return QColor(colors::ROW_ALT());
}
inline QColor kTextDim() {
    return QColor(colors::TEXT_DIM());
}
inline QColor kTextSec() {
    return QColor(colors::TEXT_SECONDARY());
}
inline QColor kColorBuy() {
    return QColor(colors::POSITIVE());
}
inline QColor kColorSell() {
    return QColor(colors::NEGATIVE());
}

// The same exchange print, seen once from the REST history and once from the
// stream. Trade ids decide when both sides have one; otherwise time, price
// and size together.
bool same_print(const TradeEntry& a, const TradeEntry& b) {
    if (!a.id.isEmpty() && !b.id.isEmpty())
        return a.id == b.id;
    return a.timestamp == b.timestamp && a.price == b.price && a.amount == b.amount;
}
} // namespace

CryptoTimeSales::CryptoTimeSales(QWidget* parent) : QWidget(parent) {
    setObjectName("cryptoTimeSales");
    trades_.reserve(MAX_TIME_SALES_TRADES);

    repaint_timer_ = new QTimer(this);
    repaint_timer_->setSingleShot(true);
    repaint_timer_->setInterval(50); // max 20fps
    connect(repaint_timer_, &QTimer::timeout, this, [this]() { update(); });
    // The tape is a cached QPixmap: without marking the cache dirty a theme
    // change re-blitted the OLD pixmap and the tape kept the previous
    // palette until the next trade arrived.
    connect(&ui::ThemeManager::instance(), &ui::ThemeManager::theme_changed, this, [this]() {
        cache_dirty_ = true;
        update();
    });
    // A currency / number-format change repaints the held tape (no fetch).
    fincept::ui::formatting::on_format_changed(this, [this]() {
        cache_dirty_ = true;
        update();
    });
}

void CryptoTimeSales::set_price_market(const QString& market) {
    if (market == price_market_)
        return;
    price_market_ = market;
    cache_dirty_ = true;
    if (repaint_timer_ && !repaint_timer_->isActive())
        repaint_timer_->start();
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
        for (const auto& t : trades_) {
            if (same_print(t, trade))
                return; // already on the tape (seeded from the REST history)
        }
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

void CryptoTimeSales::merge_history(const QVector<TradeEntry>& history) {
    {
        QMutexLocker lock(&mutex_);
        QVector<TradeEntry> merged = trades_;
        merged.reserve(trades_.size() + history.size());
        for (const auto& h : history) {
            const bool on_tape = std::any_of(merged.cbegin(), merged.cend(),
                                             [&h](const TradeEntry& t) { return same_print(t, h); });
            if (!on_tape)
                merged.append(h);
        }
        std::stable_sort(merged.begin(), merged.end(),
                         [](const TradeEntry& a, const TradeEntry& b) { return a.timestamp > b.timestamp; });
        if (merged.size() > MAX_TIME_SALES_TRADES)
            merged.resize(MAX_TIME_SALES_TRADES);
        trades_ = std::move(merged);
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

    const QFont tape_font = ui::fonts::font(-2);
    QPainter p(&cache_);
    p.setFont(tape_font);

    // Row height and the fixed TIME / side columns follow the font size.
    const QFontMetrics fm(tape_font);
    const int row_h = std::max(ROW_H, fm.height() + 4);
    const int time_w = std::max(70, fm.horizontalAdvance(QStringLiteral("00:00:00")) + 4);
    const int side_w = std::max(26, fm.horizontalAdvance(QStringLiteral("W")) + 8);
    const int price_x = time_w + 4;

    QVector<TradeEntry> snapshot;
    {
        QMutexLocker lock(&mutex_);
        snapshot = trades_;
    }

    // Header row
    p.setPen(kTextDim());
    p.drawText(QRect(4, 0, time_w, row_h), Qt::AlignLeft | Qt::AlignVCenter, tr("TIME"));
    const QString quote = pair_quote_ccy(price_market_);
    p.drawText(QRect(price_x, 0, w / 3, row_h), Qt::AlignRight | Qt::AlignVCenter,
               quote.isEmpty() ? tr("PRICE") : fincept::ui::formatting::header(tr("PRICE"), quote));
    p.drawText(QRect(price_x + w / 3, 0, w / 3, row_h), Qt::AlignRight | Qt::AlignVCenter, tr("QTY"));
    p.drawText(QRect(w - side_w - 4, 0, side_w, row_h), Qt::AlignCenter | Qt::AlignVCenter,
               tr("S", "column header: trade side (buy/sell)"));

    const int max_rows = (h - row_h) / row_h;
    const int count = std::min(static_cast<int>(snapshot.size()), max_rows);

    for (int i = 0; i < count; ++i) {
        const auto& t = snapshot[i];
        const int y = (i + 1) * row_h;
        const QColor bg = (i % 2 == 0) ? kRowEven() : kRowOdd();
        p.fillRect(0, y, w, row_h, bg);

        const bool is_buy = (t.side == "buy");
        const QColor price_color = is_buy ? kColorBuy() : kColorSell();

        // Time
        p.setPen(kTextDim());
        p.drawText(QRect(4, y, time_w, row_h), Qt::AlignLeft | Qt::AlignVCenter,
                   QDateTime::fromMSecsSinceEpoch(t.timestamp).toString("HH:mm:ss"));

        // Price — magnitude-scaled precision; a fixed 2 dp printed every fill
        // on a sub-cent pair as "0.00", making the tape useless.
        p.setPen(price_color);
        p.drawText(QRect(price_x, y, w / 3, row_h), Qt::AlignRight | Qt::AlignVCenter,
                   pair_price(t.price, price_market_));

        // Quantity
        p.setPen(kTextSec());
        p.drawText(QRect(price_x + w / 3, y, w / 3, row_h), Qt::AlignRight | Qt::AlignVCenter, format_size(t.amount));

        // Side indicator
        p.setPen(price_color);
        p.drawText(QRect(w - side_w - 4, y, side_w, row_h), Qt::AlignCenter | Qt::AlignVCenter,
                   is_buy ? tr("B", "trade side: buy") : tr("S", "trade side: sell"));
    }

    if (count == 0) {
        p.setPen(kTextDim());
        p.drawText(rect().adjusted(8, row_h, -8, 0), Qt::AlignCenter | Qt::TextWordWrap,
                   status_text_.isEmpty() ? tr("Waiting for the first trades…") : status_text_);
    }

    cache_dirty_ = false;
}

void CryptoTimeSales::set_status_text(const QString& text) {
    if (text == status_text_)
        return;
    status_text_ = text;
    cache_dirty_ = true;
    update();
}

void CryptoTimeSales::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange) {
        // The only text is the painted empty-state message — force a repaint
        // so it re-renders in the new language.
        cache_dirty_ = true;
        update();
    }
    QWidget::changeEvent(event);
}

} // namespace fincept::screens::crypto
