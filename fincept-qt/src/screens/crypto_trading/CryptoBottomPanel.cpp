#include "screens/crypto_trading/CryptoBottomPanel.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QPushButton>
#include <QDateTime>
#include <QJsonObject>
#include <cmath>

namespace fincept::screens::crypto {

CryptoBottomPanel::CryptoBottomPanel(QWidget* parent) : QWidget(parent) {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    tabs_ = new QTabWidget;
    tabs_->setStyleSheet(
        "QTabWidget::pane { border: 1px solid #222; background: #0d1117; }"
        "QTabBar::tab { background: #1a1a2e; color: #888; padding: 4px 12px; border: 1px solid #222; }"
        "QTabBar::tab:selected { background: #0d1117; color: #00aaff; border-bottom: 2px solid #00aaff; }");

    setup_positions_tab();
    setup_orders_tab();
    setup_trades_tab();
    setup_market_info_tab();
    setup_stats_tab();

    layout->addWidget(tabs_);
}

static QTableWidget* make_table(int cols, const QStringList& headers) {
    auto* t = new QTableWidget;
    t->setColumnCount(cols);
    t->setHorizontalHeaderLabels(headers);
    t->horizontalHeader()->setStretchLastSection(true);
    t->setEditTriggers(QAbstractItemView::NoEditTriggers);
    t->verticalHeader()->hide();
    t->setShowGrid(false);
    t->setStyleSheet(
        "QTableWidget { background: #0d1117; border: none; color: #ccc; font-size: 12px; }"
        "QTableWidget::item { padding: 2px 4px; }"
        "QTableWidget::item:selected { background: #1a2940; }");
    return t;
}

void CryptoBottomPanel::setup_positions_tab() {
    positions_table_ = make_table(7, {"Symbol", "Side", "Qty", "Entry", "Current", "P&L", "Leverage"});
    tabs_->addTab(positions_table_, "Positions");
}

void CryptoBottomPanel::setup_orders_tab() {
    orders_table_ = make_table(7, {"Symbol", "Side", "Type", "Qty", "Price", "Status", "Action"});
    tabs_->addTab(orders_table_, "Orders");
}

void CryptoBottomPanel::setup_trades_tab() {
    trades_table_ = make_table(7, {"Symbol", "Side", "Price", "Qty", "Fee", "P&L", "Time"});
    tabs_->addTab(trades_table_, "Trades");
}

void CryptoBottomPanel::setup_market_info_tab() {
    auto* widget = new QWidget;
    auto* grid = new QVBoxLayout(widget);
    grid->setSpacing(6);

    auto make_row = [&](const QString& label) -> QLabel* {
        auto* row = new QHBoxLayout;
        auto* lbl = new QLabel(label);
        lbl->setStyleSheet("color: #888; font-size: 12px;");
        auto* val = new QLabel("--");
        val->setStyleSheet("color: #ccc; font-size: 12px; font-weight: bold;");
        row->addWidget(lbl);
        row->addStretch();
        row->addWidget(val);
        grid->addLayout(row);
        return val;
    };

    funding_label_      = make_row("Funding Rate:");
    mark_label_         = make_row("Mark Price:");
    index_label_        = make_row("Index Price:");
    oi_label_           = make_row("Open Interest:");
    fees_label_         = make_row("Maker/Taker Fee:");
    next_funding_label_ = make_row("Next Funding:");
    grid->addStretch();

    tabs_->addTab(widget, "Market Info");
}

void CryptoBottomPanel::setup_stats_tab() {
    auto* widget = new QWidget;
    auto* layout = new QHBoxLayout(widget);
    layout->setSpacing(24);

    auto make_stat = [&](const QString& label) -> QLabel* {
        auto* box = new QVBoxLayout;
        auto* lbl = new QLabel(label);
        lbl->setStyleSheet("color: #666; font-size: 11px;");
        auto* val = new QLabel("--");
        val->setStyleSheet("color: #ccc; font-size: 16px; font-weight: bold;");
        box->addWidget(lbl);
        box->addWidget(val);
        layout->addLayout(box);
        return val;
    };

    pnl_label_          = make_stat("Total P&L");
    winrate_label_       = make_stat("Win Rate");
    trades_count_label_ = make_stat("Total Trades");
    best_trade_label_   = make_stat("Best Trade");
    worst_trade_label_  = make_stat("Worst Trade");
    layout->addStretch();

    tabs_->addTab(widget, "Stats");
}

// ============================================================================
// Data setters
// ============================================================================

void CryptoBottomPanel::set_positions(const QVector<trading::PtPosition>& positions) {
    positions_table_->setRowCount(positions.size());
    for (int i = 0; i < positions.size(); ++i) {
        const auto& p = positions[i];
        positions_table_->setItem(i, 0, new QTableWidgetItem(p.symbol));

        auto* side = new QTableWidgetItem(p.side);
        side->setForeground(p.side == "long" ? QColor("#00ff88") : QColor("#ff4444"));
        positions_table_->setItem(i, 1, side);

        positions_table_->setItem(i, 2, new QTableWidgetItem(QString::number(p.quantity, 'f', 6)));
        positions_table_->setItem(i, 3, new QTableWidgetItem(QString::number(p.entry_price, 'f', 2)));
        positions_table_->setItem(i, 4, new QTableWidgetItem(QString::number(p.current_price, 'f', 2)));

        auto* pnl = new QTableWidgetItem(QString::number(p.unrealized_pnl, 'f', 2));
        pnl->setForeground(p.unrealized_pnl >= 0 ? QColor("#00ff88") : QColor("#ff4444"));
        positions_table_->setItem(i, 5, pnl);

        positions_table_->setItem(i, 6, new QTableWidgetItem(QString::number(p.leverage, 'f', 1)));
    }
}

void CryptoBottomPanel::set_orders(const QVector<trading::PtOrder>& orders) {
    // Filter active orders
    QVector<trading::PtOrder> active;
    for (const auto& o : orders) {
        if (o.status == "pending" || o.status == "partial") active.append(o);
    }

    orders_table_->setRowCount(active.size());
    for (int i = 0; i < active.size(); ++i) {
        const auto& o = active[i];
        orders_table_->setItem(i, 0, new QTableWidgetItem(o.symbol));

        auto* side = new QTableWidgetItem(o.side);
        side->setForeground(o.side == "buy" ? QColor("#00ff88") : QColor("#ff4444"));
        orders_table_->setItem(i, 1, side);

        orders_table_->setItem(i, 2, new QTableWidgetItem(o.order_type));
        orders_table_->setItem(i, 3, new QTableWidgetItem(QString::number(o.quantity, 'f', 6)));
        orders_table_->setItem(i, 4, new QTableWidgetItem(
            o.price ? QString::number(*o.price, 'f', 2) : "Market"));
        orders_table_->setItem(i, 5, new QTableWidgetItem(o.status));

        auto* cancel_btn = new QPushButton("Cancel");
        cancel_btn->setStyleSheet("background: #5c1a1a; color: #ff4444; border: none; padding: 2px 8px; "
                                    "font-size: 11px; border-radius: 2px;");
        QString oid = o.id;
        connect(cancel_btn, &QPushButton::clicked, this, [this, oid]() {
            emit cancel_order_requested(oid);
        });
        orders_table_->setCellWidget(i, 6, cancel_btn);
    }
}

void CryptoBottomPanel::set_trades(const QVector<trading::PtTrade>& trades) {
    int count = std::min(static_cast<int>(trades.size()), 50);
    trades_table_->setRowCount(count);

    for (int i = 0; i < count; ++i) {
        const auto& t = trades[i];
        trades_table_->setItem(i, 0, new QTableWidgetItem(t.symbol));

        auto* side = new QTableWidgetItem(t.side);
        side->setForeground(t.side == "buy" ? QColor("#00ff88") : QColor("#ff4444"));
        trades_table_->setItem(i, 1, side);

        trades_table_->setItem(i, 2, new QTableWidgetItem(QString::number(t.price, 'f', 2)));
        trades_table_->setItem(i, 3, new QTableWidgetItem(QString::number(t.quantity, 'f', 6)));
        trades_table_->setItem(i, 4, new QTableWidgetItem(QString::number(t.fee, 'f', 4)));

        auto* pnl = new QTableWidgetItem(QString::number(t.pnl, 'f', 2));
        pnl->setForeground(t.pnl >= 0 ? QColor("#00ff88") : QColor("#ff4444"));
        trades_table_->setItem(i, 5, pnl);

        trades_table_->setItem(i, 6, new QTableWidgetItem(t.timestamp));
    }
}

void CryptoBottomPanel::set_stats(const trading::PtStats& stats) {
    pnl_label_->setText(QString("$%1").arg(stats.total_pnl, 0, 'f', 2));
    pnl_label_->setStyleSheet(
        QString("color: %1; font-size: 16px; font-weight: bold;")
            .arg(stats.total_pnl >= 0 ? "#00ff88" : "#ff4444"));

    winrate_label_->setText(QString("%1%").arg(stats.win_rate, 0, 'f', 1));
    trades_count_label_->setText(QString("%1 (W:%2 / L:%3)")
        .arg(stats.total_trades).arg(stats.winning_trades).arg(stats.losing_trades));
    best_trade_label_->setText(QString("$%1").arg(stats.largest_win, 0, 'f', 2));
    best_trade_label_->setStyleSheet("color: #00ff88; font-size: 16px; font-weight: bold;");
    worst_trade_label_->setText(QString("$%1").arg(stats.largest_loss, 0, 'f', 2));
    worst_trade_label_->setStyleSheet("color: #ff4444; font-size: 16px; font-weight: bold;");
}

void CryptoBottomPanel::set_market_info(const MarketInfoData& info) {
    if (!info.has_data) return;
    funding_label_->setText(QString("%1%").arg(info.funding_rate * 100.0, 0, 'f', 4));
    mark_label_->setText(QString("$%1").arg(info.mark_price, 0, 'f', 2));
    index_label_->setText(QString("$%1").arg(info.index_price, 0, 'f', 2));
    oi_label_->setText(QString("$%1M").arg(info.open_interest_value / 1e6, 0, 'f', 2));
    fees_label_->setText(QString("%1% / %2%")
        .arg(info.maker_fee * 100, 0, 'f', 3).arg(info.taker_fee * 100, 0, 'f', 3));

    if (info.next_funding_time > 0) {
        QDateTime dt = QDateTime::fromSecsSinceEpoch(info.next_funding_time);
        next_funding_label_->setText(dt.toString("HH:mm:ss"));
    }
}

void CryptoBottomPanel::set_mode(bool is_paper) {
    is_paper_ = is_paper;
}

void CryptoBottomPanel::set_live_positions(const QJsonArray& positions) {
    positions_table_->setRowCount(positions.size());
    for (int i = 0; i < positions.size(); ++i) {
        auto p = positions[i].toObject();
        positions_table_->setItem(i, 0, new QTableWidgetItem(p.value("symbol").toString()));
        auto* side = new QTableWidgetItem(p.value("side").toString());
        side->setForeground(p.value("side").toString().contains("long") ? QColor("#00ff88") : QColor("#ff4444"));
        positions_table_->setItem(i, 1, side);
        positions_table_->setItem(i, 2, new QTableWidgetItem(QString::number(p.value("contracts").toDouble(), 'f', 4)));
        positions_table_->setItem(i, 3, new QTableWidgetItem(QString::number(p.value("entryPrice").toDouble(), 'f', 2)));
        positions_table_->setItem(i, 4, new QTableWidgetItem(QString::number(p.value("markPrice").toDouble(), 'f', 2)));
        double pnl = p.value("unrealizedPnl").toDouble();
        auto* pnl_item = new QTableWidgetItem(QString::number(pnl, 'f', 2));
        pnl_item->setForeground(pnl >= 0 ? QColor("#00ff88") : QColor("#ff4444"));
        positions_table_->setItem(i, 5, pnl_item);
        positions_table_->setItem(i, 6, new QTableWidgetItem(QString::number(p.value("leverage").toDouble(), 'f', 0)));
    }
}

void CryptoBottomPanel::set_live_orders(const QJsonArray& orders) {
    orders_table_->setRowCount(orders.size());
    for (int i = 0; i < orders.size(); ++i) {
        auto o = orders[i].toObject();
        orders_table_->setItem(i, 0, new QTableWidgetItem(o.value("symbol").toString()));
        auto* side = new QTableWidgetItem(o.value("side").toString());
        side->setForeground(o.value("side").toString() == "buy" ? QColor("#00ff88") : QColor("#ff4444"));
        orders_table_->setItem(i, 1, side);
        orders_table_->setItem(i, 2, new QTableWidgetItem(o.value("type").toString()));
        orders_table_->setItem(i, 3, new QTableWidgetItem(QString::number(o.value("amount").toDouble(), 'f', 4)));
        orders_table_->setItem(i, 4, new QTableWidgetItem(QString::number(o.value("price").toDouble(), 'f', 2)));
        orders_table_->setItem(i, 5, new QTableWidgetItem(o.value("status").toString()));

        auto* cancel_btn = new QPushButton("Cancel");
        cancel_btn->setStyleSheet("background: #5c1a1a; color: #ff4444; border: none; padding: 2px 8px; font-size: 11px;");
        QString oid = o.value("id").toString();
        connect(cancel_btn, &QPushButton::clicked, this, [this, oid]() { emit cancel_order_requested(oid); });
        orders_table_->setCellWidget(i, 6, cancel_btn);
    }
}

void CryptoBottomPanel::set_live_balance(double balance, double equity, double used_margin) {
    if (!live_balance_label_) return;
    live_balance_label_->setText(QString("$%1").arg(balance, 0, 'f', 2));
    live_equity_label_->setText(QString("$%1").arg(equity, 0, 'f', 2));
    live_margin_label_->setText(QString("$%1").arg(used_margin, 0, 'f', 2));
}

} // namespace fincept::screens::crypto
