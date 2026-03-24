// EquityBottomPanel.cpp — tabbed portfolio display
#include "screens/equity_trading/EquityBottomPanel.h"

#include <QHBoxLayout>
#include <QHeaderView>
#include <QVBoxLayout>

namespace fincept::screens::equity {

EquityBottomPanel::EquityBottomPanel(QWidget* parent) : QWidget(parent) {
    setObjectName("eqBottomPanel");

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    tabs_ = new QTabWidget;
    tabs_->setObjectName("eqBottomTabs");
    tabs_->setDocumentMode(true);

    setup_positions_tab();
    setup_holdings_tab();
    setup_orders_tab();
    setup_funds_tab();
    setup_stats_tab();

    layout->addWidget(tabs_);
}

QTableWidgetItem* EquityBottomPanel::ensure_item(QTableWidget* table, int row, int col) {
    auto* item = table->item(row, col);
    if (!item) {
        item = new QTableWidgetItem;
        item->setForeground(TEXT_PRIMARY);
        table->setItem(row, col, item);
    }
    return item;
}

// ── Positions Tab ──────────────────────────────────────────────────────────

void EquityBottomPanel::setup_positions_tab() {
    positions_table_ = new QTableWidget;
    positions_table_->setObjectName("eqTable");
    positions_table_->setColumnCount(7);
    positions_table_->setHorizontalHeaderLabels({"Symbol", "Exchange", "Side", "Qty", "Avg Price", "LTP", "P&L"});
    positions_table_->verticalHeader()->setVisible(false);
    positions_table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    positions_table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    positions_table_->setShowGrid(false);
    positions_table_->horizontalHeader()->setStretchLastSection(true);
    positions_table_->verticalHeader()->setDefaultSectionSize(22);
    tabs_->addTab(positions_table_, "POSITIONS");
}

// ── Holdings Tab ───────────────────────────────────────────────────────────

void EquityBottomPanel::setup_holdings_tab() {
    holdings_table_ = new QTableWidget;
    holdings_table_->setObjectName("eqTable");
    holdings_table_->setColumnCount(7);
    holdings_table_->setHorizontalHeaderLabels({"Symbol", "Qty", "Avg Price", "LTP", "Invested", "Current", "P&L %"});
    holdings_table_->verticalHeader()->setVisible(false);
    holdings_table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    holdings_table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    holdings_table_->setShowGrid(false);
    holdings_table_->horizontalHeader()->setStretchLastSection(true);
    holdings_table_->verticalHeader()->setDefaultSectionSize(22);
    tabs_->addTab(holdings_table_, "HOLDINGS");
}

// ── Orders Tab ─────────────────────────────────────────────────────────────

void EquityBottomPanel::setup_orders_tab() {
    orders_table_ = new QTableWidget;
    orders_table_->setObjectName("eqTable");
    orders_table_->setColumnCount(8);
    orders_table_->setHorizontalHeaderLabels({"Order ID", "Symbol", "Side", "Type", "Qty", "Price", "Status", "Time"});
    orders_table_->verticalHeader()->setVisible(false);
    orders_table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    orders_table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    orders_table_->setShowGrid(false);
    orders_table_->horizontalHeader()->setStretchLastSection(true);
    orders_table_->verticalHeader()->setDefaultSectionSize(22);
    tabs_->addTab(orders_table_, "ORDERS");
}

// ── Funds Tab ──────────────────────────────────────────────────────────────

void EquityBottomPanel::setup_funds_tab() {
    auto* widget = new QWidget;
    auto* layout = new QVBoxLayout(widget);
    layout->setContentsMargins(12, 12, 12, 12);
    layout->setSpacing(8);

    auto make_row = [&](const QString& label) -> QLabel* {
        auto* row = new QHBoxLayout;
        auto* lbl = new QLabel(label);
        lbl->setObjectName("eqOeLabel");
        auto* val = new QLabel("--");
        val->setObjectName("eqOeBalance");
        val->setAlignment(Qt::AlignRight);
        row->addWidget(lbl);
        row->addStretch();
        row->addWidget(val);
        layout->addLayout(row);
        return val;
    };

    available_label_ = make_row("Available Balance");
    used_margin_label_ = make_row("Used Margin");
    total_label_ = make_row("Total Balance");
    collateral_label_ = make_row("Collateral");

    layout->addStretch();
    tabs_->addTab(widget, "FUNDS");
}

// ── Stats Tab ──────────────────────────────────────────────────────────────

void EquityBottomPanel::setup_stats_tab() {
    auto* widget = new QWidget;
    auto* layout = new QVBoxLayout(widget);
    layout->setContentsMargins(12, 12, 12, 12);
    layout->setSpacing(8);

    const char* stat_labels[] = {"Total P&L", "Win Rate", "Total Trades", "Largest Win", "Largest Loss"};
    for (int i = 0; i < 5; ++i) {
        auto* row = new QHBoxLayout;
        auto* lbl = new QLabel(stat_labels[i]);
        lbl->setObjectName("eqOeLabel");
        stat_values_[i] = new QLabel("--");
        stat_values_[i]->setObjectName("eqOeBalance");
        stat_values_[i]->setAlignment(Qt::AlignRight);
        row->addWidget(lbl);
        row->addStretch();
        row->addWidget(stat_values_[i]);
        layout->addLayout(row);
    }

    layout->addStretch();
    tabs_->addTab(widget, "STATS");
}

// ── Data Setters ───────────────────────────────────────────────────────────

void EquityBottomPanel::set_mode(bool is_paper) {
    is_paper_ = is_paper;
}

void EquityBottomPanel::set_paper_positions(const QVector<trading::PtPosition>& positions) {
    positions_table_->setRowCount(positions.size());
    for (int i = 0; i < positions.size(); ++i) {
        const auto& p = positions[i];
        ensure_item(positions_table_, i, 0)->setText(p.symbol);
        ensure_item(positions_table_, i, 1)->setText("--");
        ensure_item(positions_table_, i, 2)->setText(p.side);
        ensure_item(positions_table_, i, 3)->setText(QString::number(p.quantity, 'f', 0));
        ensure_item(positions_table_, i, 4)->setText(QString::number(p.entry_price, 'f', 2));
        ensure_item(positions_table_, i, 5)->setText(QString::number(p.current_price, 'f', 2));

        auto* pnl_item = ensure_item(positions_table_, i, 6);
        pnl_item->setText(QString::number(p.unrealized_pnl, 'f', 2));
        pnl_item->setForeground(p.unrealized_pnl >= 0 ? COLOR_BUY : COLOR_SELL);
    }
}

void EquityBottomPanel::set_paper_orders(const QVector<trading::PtOrder>& orders) {
    orders_table_->setRowCount(orders.size());
    for (int i = 0; i < orders.size(); ++i) {
        const auto& o = orders[i];
        ensure_item(orders_table_, i, 0)->setText(o.id.left(8));
        ensure_item(orders_table_, i, 1)->setText(o.symbol);
        ensure_item(orders_table_, i, 2)->setText(o.side.toUpper());
        ensure_item(orders_table_, i, 3)->setText(o.order_type.toUpper());
        ensure_item(orders_table_, i, 4)->setText(QString::number(o.quantity, 'f', 0));
        ensure_item(orders_table_, i, 5)->setText(o.price ? QString::number(*o.price, 'f', 2) : "MKT");
        ensure_item(orders_table_, i, 6)->setText(o.status.toUpper());
        ensure_item(orders_table_, i, 7)->setText(o.created_at);
    }
}

void EquityBottomPanel::set_paper_trades(const QVector<trading::PtTrade>& trades) {
    // Append trade rows below orders in the orders table (separated by a visual break)
    // We use a dedicated approach: show last 20 trades after orders
    const int base = orders_table_->rowCount();
    if (trades.isEmpty())
        return;

    // Add a separator row
    orders_table_->setRowCount(base + 1 + trades.size());
    auto* sep_item = ensure_item(orders_table_, base, 0);
    sep_item->setText("--- RECENT TRADES ---");
    sep_item->setForeground(COLOR_ACCENT);
    for (int c = 1; c < 8; ++c)
        ensure_item(orders_table_, base, c)->setText("");

    for (int i = 0; i < trades.size(); ++i) {
        const auto& t = trades[i];
        const int row = base + 1 + i;
        ensure_item(orders_table_, row, 0)->setText(t.id.left(8));
        ensure_item(orders_table_, row, 1)->setText(t.symbol);
        ensure_item(orders_table_, row, 2)->setText(t.side.toUpper());
        ensure_item(orders_table_, row, 3)->setText("TRADE");
        ensure_item(orders_table_, row, 4)->setText(QString::number(t.quantity, 'f', 0));
        ensure_item(orders_table_, row, 5)->setText(QString::number(t.price, 'f', 2));

        auto* pnl_item = ensure_item(orders_table_, row, 6);
        pnl_item->setText(QString::number(t.pnl, 'f', 2));
        pnl_item->setForeground(t.pnl >= 0 ? COLOR_BUY : COLOR_SELL);

        ensure_item(orders_table_, row, 7)->setText(t.timestamp);
    }
}

void EquityBottomPanel::set_paper_stats(const trading::PtStats& stats) {
    stat_values_[0]->setText(QString::number(stats.total_pnl, 'f', 2));
    stat_values_[0]->setStyleSheet(QString("color: %1;").arg(stats.total_pnl >= 0 ? "#16a34a" : "#dc2626"));
    stat_values_[1]->setText(QString("%1%").arg(stats.win_rate * 100, 0, 'f', 1));
    stat_values_[2]->setText(QString::number(stats.total_trades));
    stat_values_[3]->setText(QString::number(stats.largest_win, 'f', 2));
    stat_values_[3]->setStyleSheet("color: #16a34a;");
    stat_values_[4]->setText(QString::number(stats.largest_loss, 'f', 2));
    stat_values_[4]->setStyleSheet("color: #dc2626;");
}

void EquityBottomPanel::set_positions(const QVector<trading::BrokerPosition>& positions) {
    positions_table_->setRowCount(positions.size());
    for (int i = 0; i < positions.size(); ++i) {
        const auto& p = positions[i];
        ensure_item(positions_table_, i, 0)->setText(p.symbol);
        ensure_item(positions_table_, i, 1)->setText(p.exchange);
        ensure_item(positions_table_, i, 2)->setText(p.side.toUpper());
        ensure_item(positions_table_, i, 3)->setText(QString::number(p.quantity, 'f', 0));
        ensure_item(positions_table_, i, 4)->setText(QString::number(p.avg_price, 'f', 2));
        ensure_item(positions_table_, i, 5)->setText(QString::number(p.ltp, 'f', 2));

        auto* pnl_item = ensure_item(positions_table_, i, 6);
        pnl_item->setText(QString::number(p.pnl, 'f', 2));
        pnl_item->setForeground(p.pnl >= 0 ? COLOR_BUY : COLOR_SELL);
    }
}

void EquityBottomPanel::set_holdings(const QVector<trading::BrokerHolding>& holdings) {
    holdings_table_->setRowCount(holdings.size());
    for (int i = 0; i < holdings.size(); ++i) {
        const auto& h = holdings[i];
        ensure_item(holdings_table_, i, 0)->setText(h.symbol);
        ensure_item(holdings_table_, i, 1)->setText(QString::number(h.quantity, 'f', 0));
        ensure_item(holdings_table_, i, 2)->setText(QString::number(h.avg_price, 'f', 2));
        ensure_item(holdings_table_, i, 3)->setText(QString::number(h.ltp, 'f', 2));
        ensure_item(holdings_table_, i, 4)->setText(QString::number(h.invested_value, 'f', 2));
        ensure_item(holdings_table_, i, 5)->setText(QString::number(h.current_value, 'f', 2));

        auto* pnl_item = ensure_item(holdings_table_, i, 6);
        pnl_item->setText(QString("%1%").arg(h.pnl_pct, 0, 'f', 2));
        pnl_item->setForeground(h.pnl_pct >= 0 ? COLOR_BUY : COLOR_SELL);
    }
}

void EquityBottomPanel::set_orders(const QVector<trading::BrokerOrderInfo>& orders) {
    orders_table_->setRowCount(orders.size());
    for (int i = 0; i < orders.size(); ++i) {
        const auto& o = orders[i];
        ensure_item(orders_table_, i, 0)->setText(o.order_id.left(12));
        ensure_item(orders_table_, i, 1)->setText(o.symbol);
        ensure_item(orders_table_, i, 2)->setText(o.side.toUpper());
        ensure_item(orders_table_, i, 3)->setText(o.order_type.toUpper());
        ensure_item(orders_table_, i, 4)->setText(QString::number(o.quantity, 'f', 0));
        ensure_item(orders_table_, i, 5)->setText(QString::number(o.price, 'f', 2));
        ensure_item(orders_table_, i, 6)->setText(o.status.toUpper());
        ensure_item(orders_table_, i, 7)->setText(o.timestamp);
    }
}

void EquityBottomPanel::set_funds(const trading::BrokerFunds& funds) {
    available_label_->setText(QString::number(funds.available_balance, 'f', 2));
    used_margin_label_->setText(QString::number(funds.used_margin, 'f', 2));
    total_label_->setText(QString::number(funds.total_balance, 'f', 2));
    collateral_label_->setText(QString::number(funds.collateral, 'f', 2));
}

} // namespace fincept::screens::equity
