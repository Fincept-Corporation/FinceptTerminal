#include "screens/polymarket/PolymarketOrderBook.h"

#include "ui/theme/Theme.h"
#include "ui/theme/ThemeManager.h"

#include <QFontMetrics>
#include <QMouseEvent>
#include <QPainter>
#include <QVBoxLayout>

#include <algorithm>

namespace fincept::screens::polymarket {

using namespace fincept::ui;
using namespace fincept::services::prediction;

namespace {

QFont book_header_font() {
    return fonts::font(-3, QFont::Bold);
}
QFont book_data_font() {
    return fonts::font(-2);
}
/// `token` at the faint alpha of the depth bars.
QColor depth_color(const char* token) {
    QColor c(token);
    c.setAlpha(25);
    return c;
}

} // namespace

int PolymarketOrderBook::row_height() const {
    return qMax(20, QFontMetrics(book_data_font()).height() + 6);
}

int PolymarketOrderBook::header_height() const {
    return qMax(28, QFontMetrics(book_header_font()).height() + 14);
}

PolymarketOrderBook::PolymarketOrderBook(QWidget* parent) : QWidget(parent) {
    setMinimumHeight(200);
    setAccessibleName(tr("Order book"));
    setToolTip(tr("Click a price level to load it into the trade ticket."));

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

void PolymarketOrderBook::set_data(const PredictionOrderBook& book) {
    QMutexLocker lock(&mutex_);
    loading_ = false;
    error_.clear();
    bids_ = book.bids;
    asks_ = book.asks;
    // Fixed 2 dp rendered a 0.001-tick market's 0.525 / 0.526 levels both as "0.53" and
    // made its click-to-price rounding invisible. 0 = tick unknown: keep the last value.
    if (book.tick_size >= 0.01)
        price_decimals_ = 2;
    else if (book.tick_size >= 0.001)
        price_decimals_ = 3;
    else if (book.tick_size > 0.0)
        price_decimals_ = 4;

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
    loading_ = false;
    error_.clear();
    cache_dirty_ = true;
    update();
}

void PolymarketOrderBook::set_loading() {
    QMutexLocker lock(&mutex_);
    bids_.clear();
    asks_.clear();
    spread_ = 0;
    loading_ = true;
    error_.clear();
    cache_dirty_ = true;
    update();
}

void PolymarketOrderBook::set_error(const QString& message) {
    QMutexLocker lock(&mutex_);
    loading_ = false;
    error_ = message;
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
    if (y < header_height())
        return;

    // Resolve which price level was clicked using the SAME visible-row counts the
    // painter used, otherwise a click lands on a different level than the one
    // drawn (which, for a trading order book, fills the order form with a wrong
    // price). Compute under the lock, then emit outside it so a slot that calls
    // back into set_data()/clear() can't deadlock on mutex_.
    double clicked_price = 0.0;
    bool has_click = false;
    {
        QMutexLocker lock(&mutex_);
        int row_idx = (y - header_height()) / row_height();
        int ask_rows = 0, bid_rows = 0;
        visible_row_counts(ask_rows, bid_rows);

        if (row_idx < ask_rows) {
            // Asks are painted highest-first (reversed), so the top row is the
            // last ask index and the row just above the spread is asks_[0].
            int ask_i = ask_rows - 1 - row_idx;
            if (ask_i >= 0 && ask_i < asks_.size()) {
                clicked_price = asks_[ask_i].price;
                has_click = true;
            }
        } else {
            int bid_idx = row_idx - ask_rows - 1; // -1 for the spread row
            if (bid_idx >= 0 && bid_idx < bid_rows && bid_idx < bids_.size()) {
                clicked_price = bids_[bid_idx].price;
                has_click = true;
            }
        }
    }
    if (has_click)
        emit price_clicked(clicked_price);
}

void PolymarketOrderBook::visible_row_counts(int& ask_rows, int& bid_rows) const {
    const int h = height();
    const int max_rows = (h - header_height()) / row_height();
    ask_rows = qMin(asks_.size(), qMax(1, max_rows / 2 - 1));
    bid_rows = qMin(bids_.size(), qMax(1, max_rows / 2 - 1));
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

    const QFont header_font = book_header_font();
    const QFont data_font = book_data_font();
    const int header_h = header_height();
    const int row_h = row_height();

    // Header
    p.fillRect(0, 0, w, header_h, QColor(colors::BG_RAISED()));
    p.setPen(QColor(colors::BORDER_DIM()));
    p.drawLine(0, header_h - 1, w, header_h - 1);
    p.setFont(header_font);
    p.setPen(QColor(colors::TEXT_SECONDARY()));
    int col_w = w / 3;
    p.drawText(8, 0, col_w, header_h, Qt::AlignLeft | Qt::AlignVCenter, tr("PRICE"));
    p.drawText(col_w, 0, col_w, header_h, Qt::AlignRight | Qt::AlignVCenter, tr("SIZE"));
    p.drawText(2 * col_w, 0, col_w - 8, header_h, Qt::AlignRight | Qt::AlignVCenter, tr("TOTAL"));

    // Separator
    p.setPen(QColor(colors::BORDER_DIM()));
    p.drawLine(0, header_h - 1, w, header_h - 1);

    int ask_rows = 0, bid_rows = 0;
    visible_row_counts(ask_rows, bid_rows);

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
    int y = header_h;

    // Asks (highest first → reversed)
    cum = 0;
    for (int i = ask_rows - 1; i >= 0; --i) {
        cum += asks_[i].size;
        double depth_pct = cum / max_depth;
        int bar_w = static_cast<int>(w * depth_pct);

        p.fillRect(w - bar_w, y, bar_w, row_h, depth_color(colors::NEGATIVE()));

        p.setPen(QColor(colors::NEGATIVE()));
        p.drawText(4, y, col_w, row_h, Qt::AlignLeft | Qt::AlignVCenter,
                   QString::number(asks_[i].price, 'f', price_decimals_));
        p.setPen(QColor(colors::TEXT_SECONDARY()));
        p.drawText(col_w, y, col_w, row_h, Qt::AlignRight | Qt::AlignVCenter, QString::number(asks_[i].size, 'f', 1));
        p.drawText(2 * col_w, y, col_w - 4, row_h, Qt::AlignRight | Qt::AlignVCenter, QString::number(cum, 'f', 1));

        p.setPen(QColor(colors::BORDER_DIM()));
        p.drawLine(0, y + row_h - 1, w, y + row_h - 1);
        y += row_h;
    }

    const bool empty_book = bids_.isEmpty() && asks_.isEmpty();

    // Spread row (meaningless without a book)
    if (!empty_book) {
        p.fillRect(0, y, w, row_h, QColor(colors::BG_RAISED()));
        p.setPen(QColor(colors::AMBER()));
        p.setFont(header_font);
        p.drawText(4, y, w - 8, row_h, Qt::AlignCenter, tr("SPREAD %1").arg(QString::number(spread_, 'f', 4)));
        y += row_h;
    }

    // Bids (highest first)
    p.setFont(data_font);
    cum = 0;
    for (int i = 0; i < bid_rows; ++i) {
        cum += bids_[i].size;
        double depth_pct = cum / max_depth;
        int bar_w = static_cast<int>(w * depth_pct);

        p.fillRect(w - bar_w, y, bar_w, row_h, depth_color(colors::POSITIVE()));

        p.setPen(QColor(colors::POSITIVE()));
        p.drawText(4, y, col_w, row_h, Qt::AlignLeft | Qt::AlignVCenter,
                   QString::number(bids_[i].price, 'f', price_decimals_));
        p.setPen(QColor(colors::TEXT_SECONDARY()));
        p.drawText(col_w, y, col_w, row_h, Qt::AlignRight | Qt::AlignVCenter, QString::number(bids_[i].size, 'f', 1));
        p.drawText(2 * col_w, y, col_w - 4, row_h, Qt::AlignRight | Qt::AlignVCenter, QString::number(cum, 'f', 1));

        p.setPen(QColor(colors::BORDER_DIM()));
        p.drawLine(0, y + row_h - 1, w, y + row_h - 1);
        y += row_h;
    }

    if (empty_book) {
        if (loading_) {
            // Placeholder levels in the book's own columns while the first book loads
            // (painted still: it is on screen for a moment only).
            QColor block(colors::BORDER_DIM());
            block.setAlpha(140);
            const int bar_h = qMax(4, row_h / 3);
            const int rows = qMax(0, (h - header_h) / row_h);
            for (int r = 0; r < rows; ++r) {
                const int ry = header_h + r * row_h + (row_h - bar_h) / 2;
                const int wobble = (r * 37) % 23; // uneven widths read as data, not as a grid
                p.fillRect(8, ry, qMax(12, col_w / 2 - wobble), bar_h, block);
                p.fillRect(col_w + col_w / 3 + wobble / 2, ry, qMax(12, col_w * 2 / 3 - wobble / 2 - 8), bar_h, block);
                p.fillRect(2 * col_w + col_w / 3, ry, qMax(12, col_w * 2 / 3 - 8), bar_h, block);
            }
        }
        // A framed caption in the middle: what is happening, or why there is nothing.
        QString title;
        QString detail;
        QColor title_color(colors::TEXT_SECONDARY());
        if (loading_) {
            title = tr("Loading order book…");
        } else if (!error_.isEmpty()) {
            title = tr("Could not load the order book");
            detail = error_.left(240) + QLatin1Char('\n') + tr("Press ↻ in the toolbar to try again.");
            title_color = QColor(colors::DANGER());
        } else {
            title = tr("No resting orders in this book");
            detail = tr("Bids and asks appear here as soon as someone quotes this market.");
        }
        const QFont title_font = fonts::font(-2, QFont::Bold);
        const QFont detail_font = fonts::font(-3);
        const int box_w = qMin(w - 24, qMax(220, w * 2 / 3));
        const QFontMetrics tfm(title_font);
        const QFontMetrics dfm(detail_font);
        const QRect detail_bounds =
            detail.isEmpty() ? QRect() : dfm.boundingRect(QRect(0, 0, box_w - 24, 1000), Qt::TextWordWrap, detail);
        const int box_h = 16 + tfm.height() + (detail.isEmpty() ? 0 : 6 + detail_bounds.height()) + 16;
        const QRect box((w - box_w) / 2, header_h + qMax(8, (h - header_h - box_h) / 2), box_w, box_h);
        p.fillRect(box, QColor(colors::BG_RAISED()));
        p.setPen(QColor(colors::BORDER_DIM()));
        p.drawRect(box.adjusted(0, 0, -1, -1));
        p.setFont(title_font);
        p.setPen(title_color);
        p.drawText(QRect(box.left() + 12, box.top() + 16, box_w - 24, tfm.height()), Qt::AlignCenter, title);
        if (!detail.isEmpty()) {
            p.setFont(detail_font);
            p.setPen(QColor(colors::TEXT_TERTIARY()));
            p.drawText(QRect(box.left() + 12, box.top() + 16 + tfm.height() + 6, box_w - 24, detail_bounds.height()),
                       Qt::AlignHCenter | Qt::AlignTop | Qt::TextWordWrap, detail);
        }
    }
}

void PolymarketOrderBook::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange) {
        // Painted text reads tr() at paint time; invalidate the cached pixmap
        // so the next repaint re-evaluates the new language.
        cache_dirty_ = true;
        update();
    }
    QWidget::changeEvent(event);
}

} // namespace fincept::screens::polymarket
