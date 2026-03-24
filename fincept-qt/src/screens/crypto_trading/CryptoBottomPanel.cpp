// CryptoBottomPanel.cpp — Bloomberg-style tabbed panel with Time&Sales and Depth Chart
#include "screens/crypto_trading/CryptoBottomPanel.h"

#include "screens/crypto_trading/CryptoDepthChart.h"
#include "screens/crypto_trading/CryptoTimeSales.h"

#include <QDateTime>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QJsonObject>
#include <QPushButton>
#include <QVBoxLayout>

#include <cmath>

namespace fincept::screens::crypto {

namespace {
const QColor kRowEven("#080808");
const QColor kRowOdd("#0c0c0c");
const QColor kColorPos("#16a34a");
const QColor kColorNeg("#dc2626");
const QColor kColorSec("#808080");
const QColor kColorTert("#525252");
} // namespace

// Static helper: ensure QTableWidgetItem exists at (row, col)
QTableWidgetItem* CryptoBottomPanel::ensure_item(QTableWidget* table, int row, int col) {
    auto* it = table->item(row, col);
    if (!it) {
        it = new QTableWidgetItem;
        table->setItem(row, col, it);
    }
    return it;
}

static QTableWidget* make_table(int cols, const QStringList& headers) {
    auto* t = new QTableWidget;
    t->setObjectName("cryptoBottomTable");
    t->setColumnCount(cols);
    t->setHorizontalHeaderLabels(headers);
    t->horizontalHeader()->setStretchLastSection(true);
    t->setEditTriggers(QAbstractItemView::NoEditTriggers);
    t->verticalHeader()->hide();
    t->setShowGrid(false);
    t->setSelectionMode(QAbstractItemView::NoSelection);
    t->setFocusPolicy(Qt::NoFocus);
    t->verticalHeader()->setDefaultSectionSize(22);
    return t;
}

CryptoBottomPanel::CryptoBottomPanel(QWidget* parent) : QWidget(parent) {
    setObjectName("cryptoBottomPanel");

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    tabs_ = new QTabWidget;
    tabs_->setObjectName("cryptoBottomTabs");

    setup_positions_tab();
    setup_orders_tab();
    setup_trades_tab();

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
    positions_table_ = make_table(7, {"Symbol", "Side", "Qty", "Entry", "Current", "P&L", "Lev"});
    tabs_->addTab(positions_table_, "POS");
}

void CryptoBottomPanel::setup_orders_tab() {
    orders_table_ = make_table(7, {"Symbol", "Side", "Type", "Qty", "Price", "Status", ""});
    tabs_->addTab(orders_table_, "ORD");
}

void CryptoBottomPanel::setup_trades_tab() {
    trades_table_ = make_table(7, {"Symbol", "Side", "Price", "Qty", "Fee", "P&L", "Time"});
    tabs_->addTab(trades_table_, "HIST");
}

void CryptoBottomPanel::setup_market_info_tab() {
    auto* widget = new QWidget;
    auto* grid = new QVBoxLayout(widget);
    grid->setContentsMargins(8, 4, 8, 4);
    grid->setSpacing(0);

    auto make_row = [&](const QString& label) -> QLabel* {
        auto* row_widget = new QWidget;
        row_widget->setObjectName("cryptoInfoRow");
        row_widget->setFixedHeight(24);
        auto* row = new QHBoxLayout(row_widget);
        row->setContentsMargins(0, 0, 0, 0);

        auto* lbl = new QLabel(label);
        lbl->setObjectName("cryptoInfoLabel");
        auto* val = new QLabel("--");
        val->setObjectName("cryptoInfoValue");
        val->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        row->addWidget(lbl);
        row->addStretch();
        row->addWidget(val);
        grid->addWidget(row_widget);
        return val;
    };

    funding_label_ = make_row("FUNDING RATE");
    mark_label_ = make_row("MARK PRICE");
    index_label_ = make_row("INDEX PRICE");
    oi_label_ = make_row("OPEN INTEREST");
    fees_label_ = make_row("MAKER / TAKER");
    next_funding_label_ = make_row("NEXT FUNDING");
    grid->addStretch();

    tabs_->addTab(widget, "MKT");
}

void CryptoBottomPanel::setup_stats_tab() {
    auto* widget = new QWidget;
    auto* grid = new QVBoxLayout(widget);
    grid->setContentsMargins(8, 4, 8, 4);
    grid->setSpacing(0);

    const char* labels[] = {"TOTAL P&L", "WIN RATE", "TOTAL TRADES", "BEST TRADE", "WORST TRADE"};
    for (int i = 0; i < 5; ++i) {
        auto* row = new QWidget;
        row->setObjectName("cryptoStatRow");
        row->setFixedHeight(24);
        auto* h = new QHBoxLayout(row);
        h->setContentsMargins(0, 0, 0, 0);

        auto* lbl = new QLabel(labels[i]);
        lbl->setObjectName("cryptoStatLabel");
        stat_values_[i] = new QLabel("--");
        stat_values_[i]->setObjectName("cryptoStatValue");
        stat_values_[i]->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        h->addWidget(lbl);
        h->addStretch();
        h->addWidget(stat_values_[i]);
        grid->addWidget(row);
    }
    grid->addStretch();

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

// ── Data setters (with ensure_item pattern) ─────────────────────────────────

void CryptoBottomPanel::set_positions(const QVector<trading::PtPosition>& positions) {
    const int n = positions.size();
    positions_table_->setUpdatesEnabled(false);
    if (positions_table_->rowCount() != n)
        positions_table_->setRowCount(n);

    for (int i = 0; i < n; ++i) {
        const auto& pos = positions[i];
        const QColor& bg = (i % 2 == 0) ? kRowEven : kRowOdd;

        auto set = [&](int col, const QString& text, const QColor& fg = QColor(),
                       int align = Qt::AlignLeft | Qt::AlignVCenter) {
            auto* it = ensure_item(positions_table_, i, col);
            it->setText(text);
            if (fg.isValid())
                it->setForeground(fg);
            it->setBackground(bg);
            it->setTextAlignment(align);
        };

        set(0, pos.symbol);
        set(1, pos.side.toUpper(), pos.side == "long" ? kColorPos : kColorNeg);
        set(2, QString::number(pos.quantity, 'f', 6), QColor(), Qt::AlignRight | Qt::AlignVCenter);
        set(3, QString::number(pos.entry_price, 'f', 2), QColor(), Qt::AlignRight | Qt::AlignVCenter);
        set(4, QString::number(pos.current_price, 'f', 2), QColor(), Qt::AlignRight | Qt::AlignVCenter);
        set(5, QString::number(pos.unrealized_pnl, 'f', 2), pos.unrealized_pnl >= 0 ? kColorPos : kColorNeg,
            Qt::AlignRight | Qt::AlignVCenter);
        set(6, QString::number(pos.leverage, 'f', 1), QColor(), Qt::AlignRight | Qt::AlignVCenter);
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
        const QColor& bg = (i % 2 == 0) ? kRowEven : kRowOdd;

        auto set = [&](int col, const QString& text, const QColor& fg = QColor(),
                       int align = Qt::AlignLeft | Qt::AlignVCenter) {
            auto* it = ensure_item(orders_table_, i, col);
            it->setText(text);
            if (fg.isValid())
                it->setForeground(fg);
            it->setBackground(bg);
            it->setTextAlignment(align);
        };

        set(0, o.symbol);
        set(1, o.side.toUpper(), o.side == "buy" ? kColorPos : kColorNeg);
        set(2, o.order_type.toUpper());
        set(3, QString::number(o.quantity, 'f', 6), QColor(), Qt::AlignRight | Qt::AlignVCenter);
        set(4, o.price ? QString::number(*o.price, 'f', 2) : "MKT", QColor(), Qt::AlignRight | Qt::AlignVCenter);
        set(5, o.status.toUpper(), kColorSec);

        // Cancel button — reuse existing widget, update its order_id property
        auto* existing = qobject_cast<QPushButton*>(orders_table_->cellWidget(i, 6));
        if (existing) {
            // Update the stored order ID without recreating the widget
            existing->setProperty("order_id", o.id);
        } else {
            auto* cancel_btn = new QPushButton("X");
            cancel_btn->setObjectName("cryptoCancelBtn");
            cancel_btn->setFixedSize(20, 18);
            cancel_btn->setCursor(Qt::PointingHandCursor);
            cancel_btn->setProperty("order_id", o.id);
            connect(cancel_btn, &QPushButton::clicked, this,
                    [this, cancel_btn]() { emit cancel_order_requested(cancel_btn->property("order_id").toString()); });
            orders_table_->setCellWidget(i, 6, cancel_btn);
        }
    }
    orders_table_->setUpdatesEnabled(true);
}

void CryptoBottomPanel::set_trades(const QVector<trading::PtTrade>& trades) {
    const int n = std::min(static_cast<int>(trades.size()), 50);
    trades_table_->setUpdatesEnabled(false);
    if (trades_table_->rowCount() != n)
        trades_table_->setRowCount(n);

    for (int i = 0; i < n; ++i) {
        const auto& t = trades[i];
        const QColor& bg = (i % 2 == 0) ? kRowEven : kRowOdd;

        auto set = [&](int col, const QString& text, const QColor& fg = QColor(),
                       int align = Qt::AlignLeft | Qt::AlignVCenter) {
            auto* it = ensure_item(trades_table_, i, col);
            it->setText(text);
            if (fg.isValid())
                it->setForeground(fg);
            it->setBackground(bg);
            it->setTextAlignment(align);
        };

        set(0, t.symbol);
        set(1, t.side.toUpper(), t.side == "buy" ? kColorPos : kColorNeg);
        set(2, QString::number(t.price, 'f', 2), QColor(), Qt::AlignRight | Qt::AlignVCenter);
        set(3, QString::number(t.quantity, 'f', 6), QColor(), Qt::AlignRight | Qt::AlignVCenter);
        set(4, QString::number(t.fee, 'f', 4), kColorSec, Qt::AlignRight | Qt::AlignVCenter);
        set(5, QString::number(t.pnl, 'f', 2), t.pnl >= 0 ? kColorPos : kColorNeg, Qt::AlignRight | Qt::AlignVCenter);
        set(6, t.timestamp, kColorTert);
    }
    trades_table_->setUpdatesEnabled(true);
}

void CryptoBottomPanel::set_stats(const trading::PtStats& stats) {
    stat_values_[0]->setText(QString("$%1").arg(stats.total_pnl, 0, 'f', 2));
    stat_values_[0]->setProperty("pnl", stats.total_pnl >= 0 ? "positive" : "negative");
    stat_values_[0]->style()->unpolish(stat_values_[0]);
    stat_values_[0]->style()->polish(stat_values_[0]);

    stat_values_[1]->setText(QString("%1%").arg(stats.win_rate, 0, 'f', 1));
    stat_values_[2]->setText(
        QString("%1 (W:%2 L:%3)").arg(stats.total_trades).arg(stats.winning_trades).arg(stats.losing_trades));

    stat_values_[3]->setText(QString("$%1").arg(stats.largest_win, 0, 'f', 2));
    stat_values_[3]->setProperty("pnl", "positive");

    stat_values_[4]->setText(QString("$%1").arg(stats.largest_loss, 0, 'f', 2));
    stat_values_[4]->setProperty("pnl", "negative");
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
        const QColor& bg = (i % 2 == 0) ? kRowEven : kRowOdd;

        auto set = [&](int col, const QString& text, const QColor& fg = QColor(),
                       int align = Qt::AlignLeft | Qt::AlignVCenter) {
            auto* it = ensure_item(positions_table_, i, col);
            it->setText(text);
            if (fg.isValid())
                it->setForeground(fg);
            it->setBackground(bg);
            it->setTextAlignment(align);
        };

        const QString side_str = p.value("side").toString();
        set(0, p.value("symbol").toString());
        set(1, side_str.toUpper(), side_str.contains("long") ? kColorPos : kColorNeg);
        set(2, QString::number(p.value("contracts").toDouble(), 'f', 4), QColor(), Qt::AlignRight | Qt::AlignVCenter);
        set(3, QString::number(p.value("entryPrice").toDouble(), 'f', 2), QColor(), Qt::AlignRight | Qt::AlignVCenter);
        set(4, QString::number(p.value("markPrice").toDouble(), 'f', 2), QColor(), Qt::AlignRight | Qt::AlignVCenter);
        const double pnl = p.value("unrealizedPnl").toDouble();
        set(5, QString::number(pnl, 'f', 2), pnl >= 0 ? kColorPos : kColorNeg, Qt::AlignRight | Qt::AlignVCenter);
        set(6, QString::number(p.value("leverage").toDouble(), 'f', 0), QColor(), Qt::AlignRight | Qt::AlignVCenter);
    }
    positions_table_->setUpdatesEnabled(true);
}

void CryptoBottomPanel::set_live_orders(const QJsonArray& orders) {
    const int n = orders.size();
    orders_table_->setUpdatesEnabled(false);
    if (orders_table_->rowCount() != n)
        orders_table_->setRowCount(n);

    for (int i = 0; i < n; ++i) {
        auto o = orders[i].toObject();
        const QColor& bg = (i % 2 == 0) ? kRowEven : kRowOdd;

        auto set = [&](int col, const QString& text, const QColor& fg = QColor(),
                       int align = Qt::AlignLeft | Qt::AlignVCenter) {
            auto* it = ensure_item(orders_table_, i, col);
            it->setText(text);
            if (fg.isValid())
                it->setForeground(fg);
            it->setBackground(bg);
            it->setTextAlignment(align);
        };

        const QString side_str = o.value("side").toString();
        set(0, o.value("symbol").toString());
        set(1, side_str.toUpper(), side_str == "buy" ? kColorPos : kColorNeg);
        set(2, o.value("type").toString().toUpper());
        set(3, QString::number(o.value("amount").toDouble(), 'f', 4), QColor(), Qt::AlignRight | Qt::AlignVCenter);
        set(4, QString::number(o.value("price").toDouble(), 'f', 2), QColor(), Qt::AlignRight | Qt::AlignVCenter);
        set(5, o.value("status").toString().toUpper(), kColorSec);

        auto* existing_btn = qobject_cast<QPushButton*>(orders_table_->cellWidget(i, 6));
        if (existing_btn) {
            existing_btn->setProperty("order_id", o.value("id").toString());
        } else {
            auto* cancel_btn = new QPushButton("X");
            cancel_btn->setObjectName("cryptoCancelBtn");
            cancel_btn->setFixedSize(20, 18);
            cancel_btn->setCursor(Qt::PointingHandCursor);
            cancel_btn->setProperty("order_id", o.value("id").toString());
            connect(cancel_btn, &QPushButton::clicked, this,
                    [this, cancel_btn]() { emit cancel_order_requested(cancel_btn->property("order_id").toString()); });
            orders_table_->setCellWidget(i, 6, cancel_btn);
        }
    }
    orders_table_->setUpdatesEnabled(true);
}

void CryptoBottomPanel::set_live_balance(double balance, double equity, double used_margin) {
    if (!live_balance_label_)
        return;
    live_balance_label_->setText(QString("$%1").arg(balance, 0, 'f', 2));
    live_equity_label_->setText(QString("$%1").arg(equity, 0, 'f', 2));
    live_margin_label_->setText(QString("$%1").arg(used_margin, 0, 'f', 2));
}

} // namespace fincept::screens::crypto
