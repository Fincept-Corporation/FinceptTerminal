#include "screens/polymarket/PolymarketActivityFeed.h"

#include "ui/theme/Theme.h"

#include <QBrush>
#include <QCoreApplication>
#include <QDateTime>
#include <QHeaderView>
#include <QPointer>
#include <QTimeZone>
#include <QTimer>
#include <QVBoxLayout>

namespace fincept::screens::polymarket {

using namespace fincept::ui;
using namespace fincept::services::prediction;

namespace {

constexpr int kMaxRows = 200;
constexpr int kFlashMs = 600;

bool pm_feed_same_trade(const PredictionTrade& a, const PredictionTrade& b) {
    return a.ts_ms == b.ts_ms && a.price == b.price && a.size == b.size && a.side == b.side &&
           a.asset_id == b.asset_id && a.maker == b.maker;
}

// The item at (row, col), created on first use. A reused item gets every role
// apply_row() writes and loses any flash tint, so it renders like a fresh one.
QTableWidgetItem* pm_feed_cell(QTableWidget* table, int row, int col) {
    QTableWidgetItem* item = table->item(row, col);
    if (!item) {
        item = new QTableWidgetItem;
        table->setItem(row, col, item);
    } else if (item->data(Qt::BackgroundRole).isValid()) {
        item->setData(Qt::BackgroundRole, QVariant());
    }
    return item;
}

void apply_row(QTableWidget* table, int row, const PredictionTrade& t, int price_decimals) {
    const QString time = QDateTime::fromMSecsSinceEpoch(t.ts_ms, QTimeZone::UTC).toString("HH:mm:ss");
    pm_feed_cell(table, row, 0)->setText(time);
    // Free helper (anonymous namespace) — translate via QCoreApplication.
    pm_feed_cell(table, row, 1)->setText(QCoreApplication::translate("PolymarketActivityFeed", "TRADE"));

    auto* side_item = pm_feed_cell(table, row, 2);
    side_item->setText(t.side);
    side_item->setForeground(
        QColor(t.side.compare("BUY", Qt::CaseInsensitive) == 0 ? colors::POSITIVE() : colors::NEGATIVE()));

    auto* price_item = pm_feed_cell(table, row, 3);
    price_item->setText(QString::number(t.price, 'f', price_decimals));
    price_item->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
    price_item->setForeground(QColor(colors::CYAN()));

    auto* size_item = pm_feed_cell(table, row, 4);
    size_item->setText(QString::number(t.size, 'f', 2));
    size_item->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
}

void flash_row(QTableWidget* table, int row, const QColor& accent) {
    if (row < 0 || row >= table->rowCount())
        return;
    // Tint the row's backgrounds; clear after kFlashMs so the highlight
    // decays without needing per-row animation timers.
    QColor tint = accent;
    tint.setAlpha(90);
    for (int c = 0; c < table->columnCount(); ++c) {
        if (auto* it = table->item(row, c))
            it->setBackground(QBrush(tint));
    }
    QPointer<QTableWidget> guard = table;
    QTimer::singleShot(kFlashMs, table, [guard, row]() {
        if (!guard)
            return;
        if (row >= guard->rowCount())
            return;
        for (int c = 0; c < guard->columnCount(); ++c) {
            if (auto* it = guard->item(row, c))
                it->setBackground(QBrush(Qt::NoBrush));
        }
    });
}

} // namespace

PolymarketActivityFeed::PolymarketActivityFeed(QWidget* parent) : QWidget(parent) {
    auto* vl = new QVBoxLayout(this);
    vl->setContentsMargins(0, 0, 0, 0);

    table_ = new QTableWidget;
    table_->setColumnCount(5);
    table_->setHorizontalHeaderLabels({tr("TIME"), tr("TYPE"), tr("SIDE"), tr("PRICE"), tr("SIZE")});
    table_->horizontalHeader()->setStretchLastSection(true);
    table_->horizontalHeader()->setDefaultAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    table_->verticalHeader()->setVisible(false);
    table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    table_->setShowGrid(false);
    table_->setStyleSheet(QString("QTableWidget {"
                                  "  background: %1; color: %2; border: none; font-size: 10px;"
                                  "}"
                                  "QTableWidget::item {"
                                  "  padding: 3px 8px; border-bottom: 1px solid %3;"
                                  "}"
                                  "QTableWidget::item:selected { background: %4; color: %2; }"
                                  "QHeaderView::section {"
                                  "  background: %5; color: %6; border: none;"
                                  "  border-bottom: 1px solid %3;"
                                  "  padding: 5px 8px; font-size: 8px; font-weight: 700; letter-spacing: 0.5px;"
                                  "}"
                                  "QScrollBar:vertical { background: %1; width: 4px; border: none; }"
                                  "QScrollBar::handle:vertical { background: %3; min-height: 20px; }"
                                  "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }")
                              .arg(colors::BG_BASE(), colors::TEXT_PRIMARY(), colors::BORDER_DIM(), colors::BG_HOVER(),
                                   colors::BG_RAISED(), colors::TEXT_SECONDARY()));
    vl->addWidget(table_);

    // A header click re-sorts the rows away from list order.
    connect(table_->horizontalHeader(), &QHeaderView::sortIndicatorChanged, this,
            [this](int, Qt::SortOrder) { rows_in_list_order_ = false; });
}

void PolymarketActivityFeed::set_trades(const QVector<PredictionTrade>& trades) {
    render_trades(trades, /*allow_incremental=*/true);
}

bool PolymarketActivityFeed::rows_sorted_by_indicator() const {
    const int col = table_->horizontalHeader()->sortIndicatorSection();
    if (col < 0 || col >= table_->columnCount())
        return true; // no sort column — enabling sorting moves nothing
    const bool ascending = table_->horizontalHeader()->sortIndicatorOrder() == Qt::AscendingOrder;
    for (int r = 1; r < table_->rowCount(); ++r) {
        const QTableWidgetItem* prev = table_->item(r - 1, col);
        const QTableWidgetItem* cur = table_->item(r, col);
        if (!prev || !cur)
            return false;
        // Same comparisons as QTableWidget's stable sort.
        if (ascending ? (*cur < *prev) : (*prev < *cur))
            return false;
    }
    return true;
}

void PolymarketActivityFeed::render_trades(const QVector<PredictionTrade>& trades, bool allow_incremental) {
    const QVector<PredictionTrade> old = last_trades_;
    last_trades_ = trades;
    table_->setSortingEnabled(false);
    const int count = qMin(trades.size(), kMaxRows);
    const int price_decimals = qBound(0, presentation_.price_decimal_places, 6);

    // Incremental path: the rows still show `old` in list order and `trades` is
    // `old` with `fresh` newer trades on top — insert those, trim the tail, and
    // leave every other row (and its items) alone.
    int fresh = -1;
    if (allow_incremental && rows_in_list_order_ && !old.isEmpty() &&
        table_->rowCount() == qMin(old.size(), kMaxRows)) {
        for (int k = 0; k < count; ++k) {
            if (pm_feed_same_trade(trades[k], old[0])) {
                fresh = k;
                break;
            }
        }
        for (int i = fresh; fresh >= 0 && i < count; ++i) {
            if (i - fresh >= old.size() || !pm_feed_same_trade(trades[i], old[i - fresh]))
                fresh = -1;
        }
    }

    if (fresh >= 0) {
        if (fresh > 0)
            table_->model()->insertRows(0, fresh);
        const int overflow = table_->rowCount() - count;
        if (overflow > 0)
            table_->model()->removeRows(count, overflow);
        for (int i = 0; i < fresh; ++i)
            apply_row(table_, i, trades[i], price_decimals);
        // Kept rows must look freshly written: drop any flash tint still in
        // progress (flash_row() clears by row index, and the rows just shifted).
        for (int r = fresh; r < count; ++r) {
            for (int c = 0; c < table_->columnCount(); ++c) {
                QTableWidgetItem* item = table_->item(r, c);
                if (item && item->data(Qt::BackgroundRole).isValid())
                    item->setData(Qt::BackgroundRole, QVariant());
            }
        }
    } else {
        table_->setRowCount(count);
        for (int i = 0; i < count; ++i)
            apply_row(table_, i, trades[i], price_decimals);
    }
    table_->resizeColumnsToContents();
    // A stable sort of rows already in order is a no-op; otherwise the rows end
    // up permuted and the next refresh has to rewrite them.
    rows_in_list_order_ = rows_sorted_by_indicator();
    table_->setSortingEnabled(true);
}

void PolymarketActivityFeed::append_trade(const PredictionTrade& trade) {
    // Live WS trade — prepend to the top row, cap at kMaxRows. We disable
    // sorting while mutating so insertRow() keeps the newest on top. The
    // in-memory trade vector is also updated so set_presentation() re-renders
    // identically.
    table_->setSortingEnabled(false);
    last_trades_.prepend(trade);
    if (last_trades_.size() > kMaxRows)
        last_trades_.resize(kMaxRows);

    table_->insertRow(0);
    if (table_->rowCount() > kMaxRows)
        table_->removeRow(table_->rowCount() - 1);

    const int price_decimals = qBound(0, presentation_.price_decimal_places, 6);
    apply_row(table_, 0, trade, price_decimals);

    flash_row(table_, 0, presentation_.accent);
    rows_in_list_order_ = rows_in_list_order_ && rows_sorted_by_indicator();
    table_->setSortingEnabled(true);
}

void PolymarketActivityFeed::clear() {
    last_trades_.clear();
    table_->setRowCount(0);
    rows_in_list_order_ = true;
}

void PolymarketActivityFeed::set_presentation(const ExchangePresentation& p) {
    const bool decimals_changed = p.price_decimal_places != presentation_.price_decimal_places;
    presentation_ = p;
    if (decimals_changed && !last_trades_.isEmpty())
        render_trades(last_trades_, /*allow_incremental=*/false); // every price cell re-formats
}

void PolymarketActivityFeed::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange)
        retranslateUi();
    QWidget::changeEvent(event);
}

void PolymarketActivityFeed::retranslateUi() {
    if (table_)
        table_->setHorizontalHeaderLabels({tr("TIME"), tr("TYPE"), tr("SIDE"), tr("PRICE"), tr("SIZE")});
    // Re-render rows so the per-row "TRADE" type label picks up the new
    // language (side/price/size are data).
    if (!last_trades_.isEmpty())
        render_trades(last_trades_, /*allow_incremental=*/false);
}

} // namespace fincept::screens::polymarket
