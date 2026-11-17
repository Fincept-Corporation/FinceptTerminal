#include "screens/polymarket/PolymarketActivityFeed.h"

#include "ui/formatting/FormatWidgets.h"
#include "ui/tables/NumericTableWidgetItem.h"
#include "ui/theme/Theme.h"

#include <QBrush>
#include <QCoreApplication>
#include <QDateTime>
#include <QHeaderView>
#include <QLocale>
#include <QPointer>
#include <QTimeZone>
#include <QTimer>
#include <QVBoxLayout>

namespace pm_fmt = fincept::ui::formatting;

namespace fincept::screens::polymarket {

using namespace fincept::ui;
using namespace fincept::services::prediction;

namespace {

constexpr int kMaxRows = 200;
constexpr int kFlashMs = 600;

bool pm_feed_same_trade(const PredictionTrade& a, const PredictionTrade& b) {
    return a.ts_ms == b.ts_ms && a.price == b.price && a.size == b.size && a.side == b.side &&
           a.asset_id == b.asset_id && a.maker == b.maker && a.outcome == b.outcome;
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

// Numeric cells are replaced rather than reused: NumericTableWidgetItem fixes
// its sort value at construction, and a fresh item carries no flash tint.
void pm_feed_numeric_cell(QTableWidget* table, int row, int col, const QString& text, double value) {
    auto* item = new ui::NumericTableWidgetItem(text, value);
    item->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
    table->setItem(row, col, item);
}

void apply_row(QTableWidget* table, int row, const PredictionTrade& t, const ExchangePresentation& pres) {
    const QString time = QDateTime::fromMSecsSinceEpoch(t.ts_ms, QTimeZone::UTC).toString("HH:mm:ss");
    pm_feed_cell(table, row, 0)->setText(time);
    // OUTCOME (was a constant "TRADE"): a binary market's tape mixes both outcome
    // tokens, so "BUY 41¢" next to "BUY 59¢" is one trade in each — unreadable
    // without saying which. Kalshi trades are all quoted in YES terms.
    auto* outcome_item = pm_feed_cell(table, row, 1);
    outcome_item->setText(t.outcome.isEmpty() ? QStringLiteral("—") : t.outcome);

    auto* side_item = pm_feed_cell(table, row, 2);
    side_item->setText(t.side);
    side_item->setForeground(
        QColor(t.side.compare("BUY", Qt::CaseInsensitive) == 0 ? colors::POSITIVE() : colors::NEGATIVE()));

    // Same price convention as the overview / book (cents on Polymarket, dollars
    // on Kalshi); sorts by value, not text.
    pm_feed_numeric_cell(table, row, 3, pres.format_price(t.price), t.price);
    if (auto* price_item = table->item(row, 3))
        price_item->setForeground(QColor(colors::CYAN()));

    pm_feed_numeric_cell(table, row, 4, QLocale().toString(t.size, 'f', 2), t.size);
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

    stack_ = new QStackedWidget(this);
    table_ = new QTableWidget(stack_);
    table_->setColumnCount(5);
    table_->setHorizontalHeaderLabels({tr("TIME"), tr("OUTCOME"), tr("SIDE"), tr("PRICE"), tr("SIZE")});
    table_->horizontalHeader()->setStretchLastSection(true);
    table_->horizontalHeader()->setDefaultAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    table_->verticalHeader()->setVisible(false);
    table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    table_->setShowGrid(false);
    table_->setStyleSheet(QString("QTableWidget {"
                                  "  background: %1; color: %2; border: none; font-size: " + fincept::ui::fonts::px(-4) + "px;"
                                  "}"
                                  "QTableWidget::item {"
                                  "  padding: 3px 8px; border-bottom: 1px solid %3;"
                                  "}"
                                  "QTableWidget::item:selected { background: %4; color: %2; }"
                                  "QHeaderView::section {"
                                  "  background: %5; color: %6; border: none;"
                                  "  border-bottom: 1px solid %3;"
                                  "  padding: 5px 8px; font-size: " + fincept::ui::fonts::px(-6) + "px; font-weight: 700; letter-spacing: 0.5px;"
                                  "}"
                                  "QScrollBar:vertical { background: %1; width: 4px; border: none; }"
                                  "QScrollBar::handle:vertical { background: %3; min-height: 20px; }"
                                  "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }")
                              .arg(colors::BG_BASE(), colors::TEXT_PRIMARY(), colors::BORDER_DIM(), colors::BG_HOVER(),
                                   colors::BG_RAISED(), colors::TEXT_SECONDARY()));
    stack_->addWidget(table_);
    state_ = PmViewState(stack_, table_, ui::LoadingSkeleton::Shape::Table, tr("Loading recent trades…"));
    state_.show_loading(); // the screen requests trades as soon as a market is selected
    vl->addWidget(stack_);

    // A header click re-sorts the rows away from list order.
    connect(table_->horizontalHeader(), &QHeaderView::sortIndicatorChanged, this,
            [this](int, Qt::SortOrder) { rows_in_list_order_ = false; });

    // SIDE / PRICE cells carry item foreground colours fixed when written. The
    // re-render rewrites every row and re-measures the columns, so it runs once
    // the appearance change has settled — never inside theme_changed.
    connect(&ui::ThemeManager::instance(), &ui::ThemeManager::appearance_settled, this, [this]() {
        if (!last_trades_.isEmpty())
            render_trades(last_trades_, /*allow_incremental=*/false);
    });
    // A number / currency format change (prices on Kalshi carry a currency unit)
    // re-renders the trades already held (no re-fetch).
    pm_fmt::on_format_changed(this, [this]() {
        if (!last_trades_.isEmpty())
            render_trades(last_trades_, /*allow_incremental=*/false);
    });
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
    drop_empty_state();
    if (trades.isEmpty()) {
        // A designed empty state instead of a blank grid.
        table_->setRowCount(0);
        rows_in_list_order_ = true;
        state_.show_state(ui::EmptyState::no_data_spec(tr("No recent trades for this market"),
                                                       tr("Trades appear here as they happen — nobody has traded "
                                                          "this market recently.")),
                          {}, {}, /*compact=*/true);
        return; // sorting stays off — there is nothing to sort
    }
    state_.show_content();
    const int count = qMin(trades.size(), kMaxRows);
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
            apply_row(table_, i, trades[i], presentation_);
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
            apply_row(table_, i, trades[i], presentation_);
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
    drop_empty_state();
    state_.show_content();
    last_trades_.prepend(trade);
    if (last_trades_.size() > kMaxRows)
        last_trades_.resize(kMaxRows);

    table_->insertRow(0);
    if (table_->rowCount() > kMaxRows)
        table_->removeRow(table_->rowCount() - 1);

    apply_row(table_, 0, trade, presentation_);

    flash_row(table_, 0, presentation_.accent);
    rows_in_list_order_ = rows_in_list_order_ && rows_sorted_by_indicator();
    table_->setSortingEnabled(true);
}

void PolymarketActivityFeed::clear() {
    last_trades_.clear();
    table_->clearSpans();
    table_->setRowCount(0);
    rows_in_list_order_ = true;
    state_.show_loading(); // cleared for the next market, whose trades are on their way
}

void PolymarketActivityFeed::set_loading() {
    last_trades_.clear();
    table_->setSortingEnabled(false);
    table_->clearSpans();
    table_->setRowCount(0);
    rows_in_list_order_ = true;
    state_.show_loading();
}

void PolymarketActivityFeed::set_error(const QString& message) {
    if (!last_trades_.isEmpty())
        return; // rows on screen stay; the next refresh replaces them
    state_.show_state(ui::EmptyState::error_spec(message, tr("Could not load the trades")),
                      [this]() { emit retry_requested(); }, {}, /*compact=*/true);
}

void PolymarketActivityFeed::drop_empty_state() {
    // The "no trades" placeholder is one full-width span in row 0; remove it
    // before real rows are written so its item / span cannot leak into them.
    if (table_->rowCount() > 0 && table_->columnSpan(0, 0) > 1) {
        table_->clearSpans();
        table_->setRowCount(0);
    }
}

void PolymarketActivityFeed::set_presentation(const ExchangePresentation& p) {
    // Prices render through p.format_price(), so any change of price style or
    // exchange re-formats every price cell.
    const bool format_changed = p.price_style != presentation_.price_style || p.exchange_id != presentation_.exchange_id ||
                                p.price_decimal_places != presentation_.price_decimal_places;
    presentation_ = p;
    if (format_changed && !last_trades_.isEmpty())
        render_trades(last_trades_, /*allow_incremental=*/false); // every price cell re-formats
}

void PolymarketActivityFeed::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange)
        retranslateUi();
    QWidget::changeEvent(event);
}

void PolymarketActivityFeed::retranslateUi() {
    if (table_)
        table_->setHorizontalHeaderLabels({tr("TIME"), tr("OUTCOME"), tr("SIDE"), tr("PRICE"), tr("SIZE")});
    // Re-render rows so locale-formatted sizes pick up the new language.
    if (!last_trades_.isEmpty())
        render_trades(last_trades_, /*allow_incremental=*/false);
}

} // namespace fincept::screens::polymarket
