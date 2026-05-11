// CryptoBottomPanel.cpp — production trading-terminal bottom panel.
//
// Tabs: POS · ORD · HIST · MY TRADES · FEES · T&S · DEPTH · MKT · STATS
//
// Design goals:
//  • Fully responsive across desktop/laptop/HiDPI: column resize modes are
//    explicit (Stretch / ResizeToContents / Interactive / Fixed) so tables
//    expand into available width without horizontal scrollbars on common
//    layouts and don't collapse below readable widths on narrow ones.
//  • Row heights derive from QFontMetrics so they scale with the user's
//    terminal font setting instead of being pinned to a hard-coded pixel.
//  • Empty states: each table-tab is wrapped in a QStackedWidget that
//    flips to a centered placeholder when the data set is empty — no more
//    "header-only" tables on relaunch before the first WS tick lands.
//  • MKT / STATS use a 3-column card grid (responsive via QGridLayout)
//    instead of a row stack of labels, so they read like a Bloomberg-style
//    dashboard rather than a ledger.

#include "screens/crypto_trading/CryptoBottomPanel.h"

#include "screens/crypto_trading/CryptoDepthChart.h"
#include "screens/crypto_trading/CryptoTimeSales.h"
#include "ui/theme/Theme.h"

#include <QDateTime>
#include <QFontMetrics>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QJsonObject>
#include <QPushButton>
#include <QStackedWidget>
#include <QVBoxLayout>

#include <cmath>

namespace fincept::screens::crypto {

using namespace fincept::ui;

namespace {

inline QColor kRowEven() { return QColor(colors::BG_BASE()); }
inline QColor kRowOdd() { return QColor(colors::ROW_ALT()); }
inline QColor kColorPos() { return QColor(colors::POSITIVE()); }
inline QColor kColorNeg() { return QColor(colors::NEGATIVE()); }
inline QColor kColorSec() { return QColor(colors::TEXT_SECONDARY()); }
inline QColor kColorTert() { return QColor(colors::TEXT_TERTIARY()); }

// Per-column resize policy.
//  Stretch:     fills remaining horizontal space (use for the "primary" column
//               like Symbol — at least one per table or the last col stretches
//               by default).
//  Numeric:     Interactive with a width derived from font metrics (e.g.
//               "1234567890.12" worth of digits). User-resizable.
//  Compact:     ResizeToContents — best for short categorical text like
//               "BUY"/"LONG"/"FILLED".
//  Action:      Fixed pixel width — for cancel/close buttons.
enum class ColMode { Stretch, Numeric, Compact, Action };

struct ColSpec {
    QString header;
    ColMode mode = ColMode::Stretch;
    int width_chars = 8;     // hint for Numeric mode
    int fixed_px = 32;       // for Action mode
    Qt::Alignment align = Qt::AlignLeft | Qt::AlignVCenter;
};

QTableWidget* make_table(const QList<ColSpec>& cols) {
    auto* t = new QTableWidget;
    t->setObjectName("cryptoBottomTable");
    t->setColumnCount(cols.size());

    QStringList headers;
    headers.reserve(cols.size());
    for (const auto& c : cols) headers << c.header;
    t->setHorizontalHeaderLabels(headers);

    t->setEditTriggers(QAbstractItemView::NoEditTriggers);
    t->setSelectionBehavior(QAbstractItemView::SelectRows);
    t->setSelectionMode(QAbstractItemView::SingleSelection);
    t->setFocusPolicy(Qt::NoFocus);
    t->setAlternatingRowColors(false); // we paint zebra stripes ourselves
    t->setShowGrid(false);
    t->setWordWrap(false);
    t->setTextElideMode(Qt::ElideRight);
    t->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
    t->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);

    auto* vh = t->verticalHeader();
    vh->hide();
    // Row height scales with the active font — important for HiDPI and for
    // users who bump the terminal font size in Settings → Appearance.
    const int row_h = std::max(22, QFontMetrics(t->font()).height() + 8);
    vh->setDefaultSectionSize(row_h);

    auto* hh = t->horizontalHeader();
    hh->setSectionsClickable(false);
    hh->setHighlightSections(false);
    hh->setStretchLastSection(false);
    // Don't let any column collapse below ~48px or text becomes ellipsis-only.
    hh->setMinimumSectionSize(48);
    hh->setDefaultAlignment(Qt::AlignLeft | Qt::AlignVCenter);

    const QFontMetrics fm(hh->font());
    bool any_stretch = false;
    for (int i = 0; i < cols.size(); ++i) {
        const auto& c = cols[i];
        switch (c.mode) {
        case ColMode::Stretch:
            hh->setSectionResizeMode(i, QHeaderView::Stretch);
            any_stretch = true;
            break;
        case ColMode::Numeric: {
            hh->setSectionResizeMode(i, QHeaderView::Interactive);
            // Width: max of header text and `width_chars` worth of '0'.
            const int header_w = fm.horizontalAdvance(c.header) + 24;
            const int data_w = fm.horizontalAdvance(QString(c.width_chars, QLatin1Char('0'))) + 16;
            t->setColumnWidth(i, std::max(header_w, data_w));
            break;
        }
        case ColMode::Compact:
            hh->setSectionResizeMode(i, QHeaderView::ResizeToContents);
            break;
        case ColMode::Action:
            hh->setSectionResizeMode(i, QHeaderView::Fixed);
            t->setColumnWidth(i, c.fixed_px);
            break;
        }
    }
    // Fail-safe: if the caller forgot to mark a column as Stretch, stretch
    // the last numeric/compact column to fill remaining space.
    if (!any_stretch) hh->setStretchLastSection(true);

    return t;
}

QWidget* wrap_with_empty_state(QTableWidget* table, QStackedWidget*& out_stack,
                                const QString& empty_text) {
    out_stack = new QStackedWidget;
    out_stack->setObjectName("cryptoBottomStack");
    out_stack->addWidget(table);

    auto* placeholder_host = new QWidget;
    placeholder_host->setObjectName("cryptoEmptyHost");
    auto* h_layout = new QVBoxLayout(placeholder_host);
    h_layout->setContentsMargins(0, 0, 0, 0);
    auto* lbl = new QLabel(empty_text);
    lbl->setObjectName("cryptoEmptyState");
    lbl->setAlignment(Qt::AlignCenter);
    lbl->setWordWrap(true);
    h_layout->addStretch();
    h_layout->addWidget(lbl);
    h_layout->addStretch();
    out_stack->addWidget(placeholder_host);

    out_stack->setCurrentIndex(1); // start in empty state
    return out_stack;
}

} // namespace

QTableWidgetItem* CryptoBottomPanel::ensure_item(QTableWidget* table, int row, int col) {
    auto* it = table->item(row, col);
    if (!it) {
        it = new QTableWidgetItem;
        table->setItem(row, col, it);
    }
    return it;
}

void CryptoBottomPanel::update_empty_state(QTableWidget* /*table*/, QStackedWidget* stack, int row_count) {
    if (!stack) return;
    const int target = (row_count > 0) ? 0 : 1;
    if (stack->currentIndex() != target)
        stack->setCurrentIndex(target);
}

CryptoBottomPanel::CryptoBottomPanel(QWidget* parent) : QWidget(parent) {
    setObjectName("cryptoBottomPanel");
    setMinimumHeight(180); // keep at least a couple of rows visible on resize

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    tabs_ = new QTabWidget;
    tabs_->setObjectName("cryptoBottomTabs");
    tabs_->setDocumentMode(true);
    tabs_->tabBar()->setExpanding(false);
    tabs_->tabBar()->setUsesScrollButtons(true);
    tabs_->setUsesScrollButtons(true);

    setup_positions_tab();
    setup_orders_tab();
    setup_trades_tab();
    setup_my_trades_tab();
    setup_fees_tab();

    // Time & Sales
    time_sales_ = new CryptoTimeSales;
    tabs_->addTab(time_sales_, "T&S");

    // Depth Chart
    depth_chart_ = new CryptoDepthChart;
    tabs_->addTab(depth_chart_, "DEPTH");

    setup_market_info_tab();
    setup_stats_tab();

    layout->addWidget(tabs_);
}

void CryptoBottomPanel::setup_positions_tab() {
    positions_table_ = make_table({
        {"Symbol", ColMode::Stretch},
        {"Side",   ColMode::Compact},
        {"Qty",    ColMode::Numeric, 10, 0, Qt::AlignRight | Qt::AlignVCenter},
        {"Entry",  ColMode::Numeric, 10, 0, Qt::AlignRight | Qt::AlignVCenter},
        {"Mark",   ColMode::Numeric, 10, 0, Qt::AlignRight | Qt::AlignVCenter},
        {"P&L",    ColMode::Numeric, 11, 0, Qt::AlignRight | Qt::AlignVCenter},
        {"Lev",    ColMode::Compact},
    });
    auto* host = wrap_with_empty_state(positions_table_, positions_stack_, "No open positions.");
    tabs_->addTab(host, "POSITIONS");
}

void CryptoBottomPanel::setup_orders_tab() {
    orders_table_ = make_table({
        {"Symbol", ColMode::Stretch},
        {"Side",   ColMode::Compact},
        {"Type",   ColMode::Compact},
        {"Qty",    ColMode::Numeric, 10, 0, Qt::AlignRight | Qt::AlignVCenter},
        {"Price",  ColMode::Numeric, 10, 0, Qt::AlignRight | Qt::AlignVCenter},
        {"Status", ColMode::Compact},
        {"",       ColMode::Action,  0, 36}, // cancel button column
    });
    auto* host = wrap_with_empty_state(orders_table_, orders_stack_, "No active orders.");
    tabs_->addTab(host, "ORDERS");
}

void CryptoBottomPanel::setup_trades_tab() {
    trades_table_ = make_table({
        {"Symbol", ColMode::Stretch},
        {"Side",   ColMode::Compact},
        {"Price",  ColMode::Numeric, 10, 0, Qt::AlignRight | Qt::AlignVCenter},
        {"Qty",    ColMode::Numeric, 10, 0, Qt::AlignRight | Qt::AlignVCenter},
        {"Fee",    ColMode::Numeric,  8, 0, Qt::AlignRight | Qt::AlignVCenter},
        {"P&L",    ColMode::Numeric, 11, 0, Qt::AlignRight | Qt::AlignVCenter},
        {"Time",   ColMode::Compact},
    });
    auto* host = wrap_with_empty_state(trades_table_, trades_stack_, "No trade history yet.");
    tabs_->addTab(host, "HISTORY");
}

void CryptoBottomPanel::setup_my_trades_tab() {
    my_trades_table_ = make_table({
        {"Symbol", ColMode::Stretch},
        {"Side",   ColMode::Compact},
        {"Price",  ColMode::Numeric, 10, 0, Qt::AlignRight | Qt::AlignVCenter},
        {"Amount", ColMode::Numeric, 10, 0, Qt::AlignRight | Qt::AlignVCenter},
        {"Cost",   ColMode::Numeric, 11, 0, Qt::AlignRight | Qt::AlignVCenter},
        {"Fee",    ColMode::Numeric,  9, 0, Qt::AlignRight | Qt::AlignVCenter},
        {"Ccy",    ColMode::Compact},
        {"Time",   ColMode::Compact},
    });
    auto* host = wrap_with_empty_state(my_trades_table_, my_trades_stack_,
                                        "No exchange-side fills.\nConnect an API key in LIVE mode to populate.");
    tabs_->addTab(host, "MY TRADES");
}

void CryptoBottomPanel::setup_fees_tab() {
    fees_table_ = make_table({
        {"Symbol",  ColMode::Stretch},
        {"Maker %", ColMode::Numeric, 8, 0, Qt::AlignRight | Qt::AlignVCenter},
        {"Taker %", ColMode::Numeric, 8, 0, Qt::AlignRight | Qt::AlignVCenter},
    });
    auto* host = wrap_with_empty_state(fees_table_, fees_stack_, "No fee schedule loaded.");
    tabs_->addTab(host, "FEES");
}

namespace {
// Build a single dashboard card and return a pointer to its value-label.
// Used by both the MARKET INFO and STATS tabs so they share visual language.
QLabel* build_card(QGridLayout* grid, int row, int col, const QString& label_text) {
    auto* card = new QFrame;
    card->setObjectName("cryptoCard");
    card->setFrameShape(QFrame::NoFrame);
    auto* v = new QVBoxLayout(card);
    v->setContentsMargins(12, 8, 12, 8);
    v->setSpacing(4);

    auto* lbl = new QLabel(label_text);
    lbl->setObjectName("cryptoCardLabel");
    auto* val = new QLabel(QStringLiteral("--"));
    val->setObjectName("cryptoCardValue");
    val->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);

    v->addWidget(lbl);
    v->addWidget(val);
    grid->addWidget(card, row, col);
    return val;
}
} // namespace

void CryptoBottomPanel::setup_market_info_tab() {
    auto* widget = new QWidget(this);
    widget->setObjectName("cryptoCardHost");
    auto* outer = new QVBoxLayout(widget);
    outer->setContentsMargins(12, 12, 12, 12);
    outer->setSpacing(0);

    auto* grid = new QGridLayout;
    grid->setHorizontalSpacing(10);
    grid->setVerticalSpacing(10);
    // 3 cards per row at standard width — collapses gracefully because each
    // card's QFrame has expanding size policy.
    for (int c = 0; c < 3; ++c) grid->setColumnStretch(c, 1);

    funding_label_      = build_card(grid, 0, 0, "FUNDING RATE");
    mark_label_         = build_card(grid, 0, 1, "MARK PRICE");
    index_label_        = build_card(grid, 0, 2, "INDEX PRICE");
    oi_label_           = build_card(grid, 1, 0, "OPEN INTEREST");
    fees_label_         = build_card(grid, 1, 1, "MAKER / TAKER");
    next_funding_label_ = build_card(grid, 1, 2, "NEXT FUNDING");

    outer->addLayout(grid);
    outer->addStretch();

    tabs_->addTab(widget, "MARKET");
}

void CryptoBottomPanel::setup_stats_tab() {
    auto* widget = new QWidget(this);
    widget->setObjectName("cryptoCardHost");
    auto* outer = new QVBoxLayout(widget);
    outer->setContentsMargins(12, 12, 12, 12);
    outer->setSpacing(0);

    auto* grid = new QGridLayout;
    grid->setHorizontalSpacing(10);
    grid->setVerticalSpacing(10);
    for (int c = 0; c < 3; ++c) grid->setColumnStretch(c, 1);

    static const char* labels[] = {"TOTAL P&L", "WIN RATE", "TOTAL TRADES", "BEST TRADE", "WORST TRADE"};
    // Row 0: P&L card spans 1 col (most prominent metric), then Win Rate, Trades.
    // Row 1: Best / Worst.
    stat_values_[0] = build_card(grid, 0, 0, labels[0]);
    stat_values_[1] = build_card(grid, 0, 1, labels[1]);
    stat_values_[2] = build_card(grid, 0, 2, labels[2]);
    stat_values_[3] = build_card(grid, 1, 0, labels[3]);
    stat_values_[4] = build_card(grid, 1, 1, labels[4]);

    // Mark P&L cards as such so the QSS can colour them via the [pnl] property.
    stat_values_[0]->setProperty("pnl", "neutral");
    stat_values_[3]->setProperty("pnl", "positive");
    stat_values_[4]->setProperty("pnl", "negative");

    outer->addLayout(grid);
    outer->addStretch();

    tabs_->addTab(widget, "STATS");
}

// ── Forwarding methods for new widgets ──────────────────────────────────────

void CryptoBottomPanel::add_trade_entry(const TradeEntry& trade) {
    time_sales_->add_trade(trade);
}

void CryptoBottomPanel::set_depth_data(const QVector<QPair<double, double>>& bids,
                                       const QVector<QPair<double, double>>& asks, double spread, double spread_pct) {
    depth_chart_->set_data(bids, asks, spread, spread_pct);
}

// ── Data setters ────────────────────────────────────────────────────────────

void CryptoBottomPanel::set_positions(const QVector<trading::PtPosition>& positions) {
    const int n = positions.size();
    positions_table_->setUpdatesEnabled(false);
    if (positions_table_->rowCount() != n)
        positions_table_->setRowCount(n);

    for (int i = 0; i < n; ++i) {
        const auto& pos = positions[i];
        const QColor bg = (i % 2 == 0) ? kRowEven() : kRowOdd();

        auto set = [&](int col, const QString& text, const QColor& fg = QColor(),
                       Qt::Alignment align = Qt::AlignLeft | Qt::AlignVCenter) {
            auto* it = ensure_item(positions_table_, i, col);
            if (it->text() != text)
                it->setText(text);
            const QColor cur_fg = it->foreground().color();
            if (fg.isValid() && (it->foreground().style() == Qt::NoBrush || cur_fg != fg))
                it->setForeground(fg);
            if (it->background().color() != bg)
                it->setBackground(bg);
            if (Qt::Alignment(it->textAlignment()) != align)
                it->setTextAlignment(align);
        };

        set(0, pos.symbol);
        set(1, pos.side.toUpper(), pos.side == "long" ? kColorPos() : kColorNeg());
        set(2, QString::number(pos.quantity, 'f', 6), QColor(), Qt::AlignRight | Qt::AlignVCenter);
        set(3, QString::number(pos.entry_price, 'f', 2), QColor(), Qt::AlignRight | Qt::AlignVCenter);
        set(4, QString::number(pos.current_price, 'f', 2), QColor(), Qt::AlignRight | Qt::AlignVCenter);
        set(5, QString::number(pos.unrealized_pnl, 'f', 2), pos.unrealized_pnl >= 0 ? kColorPos() : kColorNeg(),
            Qt::AlignRight | Qt::AlignVCenter);
        set(6, QString::number(pos.leverage, 'f', 1) + QStringLiteral("x"), QColor(), Qt::AlignRight | Qt::AlignVCenter);
    }
    positions_table_->setUpdatesEnabled(true);
    update_empty_state(positions_table_, positions_stack_, n);
}

void CryptoBottomPanel::update_position_prices(const QHash<QString, double>& last_prices) {
    if (last_prices.isEmpty() || !positions_table_)
        return;
    const int n = positions_table_->rowCount();
    if (n == 0)
        return;
    positions_table_->setUpdatesEnabled(false);
    for (int i = 0; i < n; ++i) {
        auto* sym_item = positions_table_->item(i, 0);
        if (!sym_item)
            continue;
        const QString sym = sym_item->text();
        auto it = last_prices.constFind(sym);
        if (it == last_prices.constEnd())
            continue;
        const double mark = it.value();
        if (mark <= 0.0)
            continue;

        auto* side_item = positions_table_->item(i, 1);
        auto* qty_item = positions_table_->item(i, 2);
        auto* entry_item = positions_table_->item(i, 3);
        auto* cur_item = positions_table_->item(i, 4);
        auto* pnl_item = positions_table_->item(i, 5);
        if (!side_item || !qty_item || !entry_item || !cur_item || !pnl_item)
            continue;

        const bool is_long = side_item->text().compare("LONG", Qt::CaseInsensitive) == 0;
        const double qty = qty_item->text().toDouble();
        const double entry = entry_item->text().toDouble();
        if (qty <= 0.0 || entry <= 0.0)
            continue;

        const double pnl = is_long ? (mark - entry) * qty : (entry - mark) * qty;

        cur_item->setText(QString::number(mark, 'f', 2));
        pnl_item->setText(QString::number(pnl, 'f', 2));
        pnl_item->setForeground(pnl >= 0 ? kColorPos() : kColorNeg());
    }
    positions_table_->setUpdatesEnabled(true);
}

void CryptoBottomPanel::set_orders(const QVector<trading::PtOrder>& orders) {
    QVector<trading::PtOrder> active;
    for (const auto& o : orders) {
        if (o.status == "pending" || o.status == "partial")
            active.append(o);
    }

    const int n = active.size();
    orders_table_->setUpdatesEnabled(false);
    if (orders_table_->rowCount() != n)
        orders_table_->setRowCount(n);

    for (int i = 0; i < n; ++i) {
        const auto& o = active[i];
        const QColor bg = (i % 2 == 0) ? kRowEven() : kRowOdd();

        auto set = [&](int col, const QString& text, const QColor& fg = QColor(),
                       Qt::Alignment align = Qt::AlignLeft | Qt::AlignVCenter) {
            auto* it = ensure_item(orders_table_, i, col);
            it->setText(text);
            if (fg.isValid())
                it->setForeground(fg);
            it->setBackground(bg);
            it->setTextAlignment(align);
        };

        set(0, o.symbol);
        set(1, o.side.toUpper(), o.side == "buy" ? kColorPos() : kColorNeg());
        set(2, o.order_type.toUpper());
        set(3, QString::number(o.quantity, 'f', 6), QColor(), Qt::AlignRight | Qt::AlignVCenter);
        set(4, o.price ? QString::number(*o.price, 'f', 2) : QStringLiteral("MKT"), QColor(),
            Qt::AlignRight | Qt::AlignVCenter);
        set(5, o.status.toUpper(), kColorSec());

        auto* existing = qobject_cast<QPushButton*>(orders_table_->cellWidget(i, 6));
        if (existing) {
            existing->setProperty("order_id", o.id);
        } else {
            auto* cancel_btn = new QPushButton(QStringLiteral("✕"));
            cancel_btn->setObjectName("cryptoCancelBtn");
            cancel_btn->setToolTip("Cancel order");
            cancel_btn->setCursor(Qt::PointingHandCursor);
            cancel_btn->setProperty("order_id", o.id);
            cancel_btn->setFocusPolicy(Qt::NoFocus);
            connect(cancel_btn, &QPushButton::clicked, this,
                    [this, cancel_btn]() { emit cancel_order_requested(cancel_btn->property("order_id").toString()); });
            orders_table_->setCellWidget(i, 6, cancel_btn);
        }
    }
    orders_table_->setUpdatesEnabled(true);
    update_empty_state(orders_table_, orders_stack_, n);
}

void CryptoBottomPanel::set_trades(const QVector<trading::PtTrade>& trades) {
    const int n = std::min(static_cast<int>(trades.size()), 50);
    trades_table_->setUpdatesEnabled(false);
    if (trades_table_->rowCount() != n)
        trades_table_->setRowCount(n);

    for (int i = 0; i < n; ++i) {
        const auto& t = trades[i];
        const QColor bg = (i % 2 == 0) ? kRowEven() : kRowOdd();

        auto set = [&](int col, const QString& text, const QColor& fg = QColor(),
                       Qt::Alignment align = Qt::AlignLeft | Qt::AlignVCenter) {
            auto* it = ensure_item(trades_table_, i, col);
            it->setText(text);
            if (fg.isValid())
                it->setForeground(fg);
            it->setBackground(bg);
            it->setTextAlignment(align);
        };

        set(0, t.symbol);
        set(1, t.side.toUpper(), t.side == "buy" ? kColorPos() : kColorNeg());
        set(2, QString::number(t.price, 'f', 2), QColor(), Qt::AlignRight | Qt::AlignVCenter);
        set(3, QString::number(t.quantity, 'f', 6), QColor(), Qt::AlignRight | Qt::AlignVCenter);
        set(4, QString::number(t.fee, 'f', 4), kColorSec(), Qt::AlignRight | Qt::AlignVCenter);
        set(5, QString::number(t.pnl, 'f', 2), t.pnl >= 0 ? kColorPos() : kColorNeg(),
            Qt::AlignRight | Qt::AlignVCenter);
        set(6, t.timestamp, kColorTert());
    }
    trades_table_->setUpdatesEnabled(true);
    update_empty_state(trades_table_, trades_stack_, n);
}

void CryptoBottomPanel::set_stats(const trading::PtStats& stats) {
    auto repolish = [](QLabel* lbl) {
        if (!lbl || !lbl->style()) return;
        lbl->style()->unpolish(lbl);
        lbl->style()->polish(lbl);
    };

    stat_values_[0]->setText(QString("$%1").arg(stats.total_pnl, 0, 'f', 2));
    stat_values_[0]->setProperty("pnl", stats.total_pnl >= 0 ? "positive" : "negative");
    repolish(stat_values_[0]);

    stat_values_[1]->setText(QString("%1%").arg(stats.win_rate, 0, 'f', 1));
    stat_values_[2]->setText(QString("%1  W:%2 / L:%3").arg(stats.total_trades).arg(stats.winning_trades).arg(stats.losing_trades));

    stat_values_[3]->setText(QString("$%1").arg(stats.largest_win, 0, 'f', 2));
    stat_values_[3]->setProperty("pnl", "positive");
    repolish(stat_values_[3]);

    stat_values_[4]->setText(QString("$%1").arg(stats.largest_loss, 0, 'f', 2));
    stat_values_[4]->setProperty("pnl", "negative");
    repolish(stat_values_[4]);
}

void CryptoBottomPanel::set_market_info(const MarketInfoData& info) {
    if (!info.has_data)
        return;
    funding_label_->setText(QString("%1%").arg(info.funding_rate * 100.0, 0, 'f', 4));
    mark_label_->setText(QString("$%1").arg(info.mark_price, 0, 'f', 2));
    index_label_->setText(QString("$%1").arg(info.index_price, 0, 'f', 2));
    oi_label_->setText(QString("$%1M").arg(info.open_interest_value / 1e6, 0, 'f', 2));
    fees_label_->setText(
        QString("%1% / %2%").arg(info.maker_fee * 100, 0, 'f', 3).arg(info.taker_fee * 100, 0, 'f', 3));
    if (info.next_funding_time > 0)
        next_funding_label_->setText(QDateTime::fromSecsSinceEpoch(info.next_funding_time).toString("HH:mm:ss"));
}

void CryptoBottomPanel::set_mode(bool is_paper) {
    is_paper_ = is_paper;
}

void CryptoBottomPanel::set_live_positions(const QJsonArray& positions) {
    const int n = positions.size();
    positions_table_->setUpdatesEnabled(false);
    if (positions_table_->rowCount() != n)
        positions_table_->setRowCount(n);

    for (int i = 0; i < n; ++i) {
        auto p = positions[i].toObject();
        const QColor bg = (i % 2 == 0) ? kRowEven() : kRowOdd();

        auto set = [&](int col, const QString& text, const QColor& fg = QColor(),
                       Qt::Alignment align = Qt::AlignLeft | Qt::AlignVCenter) {
            auto* it = ensure_item(positions_table_, i, col);
            it->setText(text);
            if (fg.isValid())
                it->setForeground(fg);
            it->setBackground(bg);
            it->setTextAlignment(align);
        };

        const QString side_str = p.value("side").toString();
        set(0, p.value("symbol").toString());
        set(1, side_str.toUpper(), side_str.contains("long") ? kColorPos() : kColorNeg());
        set(2, QString::number(p.value("contracts").toDouble(), 'f', 4), QColor(), Qt::AlignRight | Qt::AlignVCenter);
        set(3, QString::number(p.value("entryPrice").toDouble(), 'f', 2), QColor(), Qt::AlignRight | Qt::AlignVCenter);
        set(4, QString::number(p.value("markPrice").toDouble(), 'f', 2), QColor(), Qt::AlignRight | Qt::AlignVCenter);
        const double pnl = p.value("unrealizedPnl").toDouble();
        set(5, QString::number(pnl, 'f', 2), pnl >= 0 ? kColorPos() : kColorNeg(), Qt::AlignRight | Qt::AlignVCenter);
        set(6, QString::number(p.value("leverage").toDouble(), 'f', 0) + QStringLiteral("x"), QColor(),
            Qt::AlignRight | Qt::AlignVCenter);
    }
    positions_table_->setUpdatesEnabled(true);
    update_empty_state(positions_table_, positions_stack_, n);
}

void CryptoBottomPanel::set_live_orders(const QJsonArray& orders) {
    const int n = orders.size();
    orders_table_->setUpdatesEnabled(false);
    if (orders_table_->rowCount() != n)
        orders_table_->setRowCount(n);

    for (int i = 0; i < n; ++i) {
        auto o = orders[i].toObject();
        const QColor bg = (i % 2 == 0) ? kRowEven() : kRowOdd();

        auto set = [&](int col, const QString& text, const QColor& fg = QColor(),
                       Qt::Alignment align = Qt::AlignLeft | Qt::AlignVCenter) {
            auto* it = ensure_item(orders_table_, i, col);
            it->setText(text);
            if (fg.isValid())
                it->setForeground(fg);
            it->setBackground(bg);
            it->setTextAlignment(align);
        };

        const QString side_str = o.value("side").toString();
        set(0, o.value("symbol").toString());
        set(1, side_str.toUpper(), side_str == "buy" ? kColorPos() : kColorNeg());
        set(2, o.value("type").toString().toUpper());
        set(3, QString::number(o.value("amount").toDouble(), 'f', 4), QColor(), Qt::AlignRight | Qt::AlignVCenter);
        set(4, QString::number(o.value("price").toDouble(), 'f', 2), QColor(), Qt::AlignRight | Qt::AlignVCenter);
        set(5, o.value("status").toString().toUpper(), kColorSec());

        auto* existing_btn = qobject_cast<QPushButton*>(orders_table_->cellWidget(i, 6));
        if (existing_btn) {
            existing_btn->setProperty("order_id", o.value("id").toString());
        } else {
            auto* cancel_btn = new QPushButton(QStringLiteral("✕"));
            cancel_btn->setObjectName("cryptoCancelBtn");
            cancel_btn->setToolTip("Cancel order");
            cancel_btn->setCursor(Qt::PointingHandCursor);
            cancel_btn->setProperty("order_id", o.value("id").toString());
            cancel_btn->setFocusPolicy(Qt::NoFocus);
            connect(cancel_btn, &QPushButton::clicked, this,
                    [this, cancel_btn]() { emit cancel_order_requested(cancel_btn->property("order_id").toString()); });
            orders_table_->setCellWidget(i, 6, cancel_btn);
        }
    }
    orders_table_->setUpdatesEnabled(true);
    update_empty_state(orders_table_, orders_stack_, n);
}

void CryptoBottomPanel::update_my_trades(const QJsonObject& json) {
    const QJsonArray trades = json.value("trades").toArray();
    const int n = trades.size();
    my_trades_table_->setUpdatesEnabled(false);
    if (my_trades_table_->rowCount() != n)
        my_trades_table_->setRowCount(n);

    for (int i = 0; i < n; ++i) {
        const auto t = trades[i].toObject();
        const QColor bg = (i % 2 == 0) ? kRowEven() : kRowOdd();

        auto set = [&](int col, const QString& text, const QColor& fg = QColor(),
                       Qt::Alignment align = Qt::AlignLeft | Qt::AlignVCenter) {
            auto* it = ensure_item(my_trades_table_, i, col);
            it->setText(text);
            if (fg.isValid())
                it->setForeground(fg);
            it->setBackground(bg);
            it->setTextAlignment(align);
        };

        const QString side = t.value("side").toString();
        const qint64 ts_ms = t.value("timestamp").toVariant().toLongLong();
        const QString time_str = ts_ms > 0 ? QDateTime::fromMSecsSinceEpoch(ts_ms).toString("MM-dd HH:mm:ss") : "--";

        set(0, t.value("symbol").toString());
        set(1, side.toUpper(), side == "buy" ? kColorPos() : kColorNeg());
        set(2, QString::number(t.value("price").toDouble(), 'f', 2), QColor(), Qt::AlignRight | Qt::AlignVCenter);
        set(3, QString::number(t.value("amount").toDouble(), 'f', 6), QColor(), Qt::AlignRight | Qt::AlignVCenter);
        set(4, QString::number(t.value("cost").toDouble(), 'f', 2), QColor(), Qt::AlignRight | Qt::AlignVCenter);
        set(5, QString::number(t.value("fee").toDouble(), 'f', 6), kColorSec(), Qt::AlignRight | Qt::AlignVCenter);
        set(6, t.value("fee_currency").toString(), kColorTert());
        set(7, time_str, kColorTert());
    }
    my_trades_table_->setUpdatesEnabled(true);
    update_empty_state(my_trades_table_, my_trades_stack_, n);
}

void CryptoBottomPanel::update_fees(const QJsonObject& json) {
    auto write_row = [&](int row, const QColor& bg, const QString& sym, double maker, double taker) {
        auto set = [&](int col, const QString& text, const QColor& fg = QColor(),
                       Qt::Alignment align = Qt::AlignRight | Qt::AlignVCenter) {
            auto* it = ensure_item(fees_table_, row, col);
            it->setText(text);
            if (fg.isValid())
                it->setForeground(fg);
            it->setBackground(bg);
            it->setTextAlignment(align);
        };
        set(0, sym, QColor(), Qt::AlignLeft | Qt::AlignVCenter);
        set(1, QString::number(maker * 100.0, 'f', 4) + "%", kColorSec());
        set(2, QString::number(taker * 100.0, 'f', 4) + "%", kColorSec());
    };

    if (json.contains("symbol")) {
        fees_table_->setUpdatesEnabled(false);
        fees_table_->setRowCount(1);
        write_row(0, kRowEven(), json.value("symbol").toString(),
                  json.value("maker").toDouble(), json.value("taker").toDouble());
        fees_table_->setUpdatesEnabled(true);
        update_empty_state(fees_table_, fees_stack_, 1);
        return;
    }

    const QJsonArray fees = json.value("fees").toArray();
    const int n = fees.size();
    fees_table_->setUpdatesEnabled(false);
    if (fees_table_->rowCount() != n)
        fees_table_->setRowCount(n);

    for (int i = 0; i < n; ++i) {
        const auto f = fees[i].toObject();
        const QColor bg = (i % 2 == 0) ? kRowEven() : kRowOdd();
        write_row(i, bg, f.value("symbol").toString(),
                  f.value("maker").toDouble(), f.value("taker").toDouble());
    }
    fees_table_->setUpdatesEnabled(true);
    update_empty_state(fees_table_, fees_stack_, n);
}

void CryptoBottomPanel::set_live_balance(double balance, double equity, double used_margin) {
    if (!live_balance_label_)
        return;
    live_balance_label_->setText(QString("$%1").arg(balance, 0, 'f', 2));
    live_equity_label_->setText(QString("$%1").arg(equity, 0, 'f', 2));
    live_margin_label_->setText(QString("$%1").arg(used_margin, 0, 'f', 2));
}

} // namespace fincept::screens::crypto
