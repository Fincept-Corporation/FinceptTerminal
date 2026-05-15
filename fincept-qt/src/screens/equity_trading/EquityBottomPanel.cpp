// EquityBottomPanel.cpp — tabbed portfolio display
#include "screens/equity_trading/EquityBottomPanel.h"

#include "ui/theme/Theme.h"

#include <QDate>
#include <QDateTime>
#include <QDialog>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLineEdit>
#include <QPushButton>
#include <QScrollBar>
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
    setup_time_sales_tab();
    setup_auctions_tab();
    setup_calendar_tab();

    layout->addWidget(tabs_);
}

QTableWidgetItem* EquityBottomPanel::ensure_item(QTableWidget* table, int row, int col) {
    auto* item = table->item(row, col);
    if (!item) {
        item = new QTableWidgetItem;
        item->setForeground(QColor(fincept::ui::colors::TEXT_PRIMARY()));
        table->setItem(row, col, item);
    }
    return item;
}

// ── Positions Tab ──────────────────────────────────────────────────────────

void EquityBottomPanel::setup_positions_tab() {
    positions_table_ = new QTableWidget;
    positions_table_->setObjectName("eqTable");
    positions_table_->setColumnCount(8);
    positions_table_->setHorizontalHeaderLabels(
        {"Symbol", "Opened", "Side", "Qty", "Avg Price", "LTP", "P&L", "P&L %"});
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
    auto* tab = new QWidget;
    auto* v = new QVBoxLayout(tab);
    v->setContentsMargins(0, 0, 0, 0);
    v->setSpacing(0);

    // Summary strip across the top of the Holdings tab.
    auto* strip = new QWidget;
    strip->setObjectName("eqHoldingsStrip");
    strip->setStyleSheet(QString("#eqHoldingsStrip{background:%1;border-bottom:1px solid %2;}")
                             .arg(fincept::ui::colors::PANEL(), fincept::ui::colors::BORDER()));
    auto* strip_layout = new QHBoxLayout(strip);
    strip_layout->setContentsMargins(12, 6, 12, 6);
    strip_layout->setSpacing(24);

    auto make_stat = [&](const QString& caption) -> QLabel* {
        auto* cell = new QWidget;
        auto* cv = new QVBoxLayout(cell);
        cv->setContentsMargins(0, 0, 0, 0);
        cv->setSpacing(1);
        auto* cap = new QLabel(caption);
        cap->setStyleSheet(QString("color:%1;font-size:10px;letter-spacing:0.5px;")
                               .arg(fincept::ui::colors::TEXT_SECONDARY()));
        auto* val = new QLabel("--");
        val->setStyleSheet(QString("color:%1;font-size:13px;font-weight:700;")
                               .arg(fincept::ui::colors::TEXT_PRIMARY()));
        cv->addWidget(cap);
        cv->addWidget(val);
        strip_layout->addWidget(cell);
        return val;
    };
    holdings_count_label_     = make_stat("HOLDINGS");
    holdings_invested_label_  = make_stat("INVESTED");
    holdings_current_label_   = make_stat("CURRENT");
    holdings_pnl_label_       = make_stat("TOTAL P&L");
    holdings_pnl_pct_label_   = make_stat("RETURN %");
    strip_layout->addStretch(1);

    holdings_import_btn_ = new QPushButton("IMPORT TO PORTFOLIO");
    holdings_import_btn_->setCursor(Qt::PointingHandCursor);
    holdings_import_btn_->setEnabled(false);
    holdings_import_btn_->setStyleSheet(
        QString("QPushButton{background:%1;color:%2;border:1px solid %3;padding:4px 12px;"
                "font-size:11px;font-weight:700;letter-spacing:0.5px;}"
                "QPushButton:hover{background:%4;color:#000;}"
                "QPushButton:disabled{color:%5;border-color:%3;}")
            .arg(fincept::ui::colors::PANEL(), fincept::ui::colors::ORANGE(),
                 fincept::ui::colors::BORDER(), fincept::ui::colors::ORANGE(),
                 fincept::ui::colors::TEXT_SECONDARY()));
    connect(holdings_import_btn_, &QPushButton::clicked, this, [this]() {
        if (!last_holdings_.isEmpty())
            emit import_holdings_requested(last_holdings_);
    });
    strip_layout->addWidget(holdings_import_btn_);

    v->addWidget(strip);

    holdings_table_ = new QTableWidget;
    holdings_table_->setObjectName("eqTable");
    holdings_table_->setColumnCount(8);
    holdings_table_->setHorizontalHeaderLabels(
        {"Symbol", "Qty", "Avg Price", "LTP", "Invested", "Current", "P&L", "P&L %"});
    holdings_table_->verticalHeader()->setVisible(false);
    holdings_table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    holdings_table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    holdings_table_->setShowGrid(false);
    holdings_table_->horizontalHeader()->setStretchLastSection(true);
    holdings_table_->verticalHeader()->setDefaultSectionSize(22);
    holdings_table_->setAlternatingRowColors(true);
    holdings_table_->setSortingEnabled(true);
    // Smooth pixel-based scrolling (default is per-item, which feels janky).
    holdings_table_->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    holdings_table_->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
    holdings_table_->verticalScrollBar()->setSingleStep(8);
    holdings_table_->horizontalScrollBar()->setSingleStep(16);

    v->addWidget(holdings_table_, 1);
    tabs_->addTab(tab, "HOLDINGS");
}

// ── Orders Tab ─────────────────────────────────────────────────────────────

void EquityBottomPanel::setup_orders_tab() {
    orders_table_ = new QTableWidget;
    orders_table_->setObjectName("eqTable");
    orders_table_->setColumnCount(9);
    orders_table_->setHorizontalHeaderLabels(
        {"Order ID", "Symbol", "Side", "Type", "Qty", "Price", "Status", "Time", "Action"});
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
    auto* widget = new QWidget(this);
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
    auto* widget = new QWidget(this);
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

        // Col 1 — "Opened": parse ISO timestamp, convert to local time, fall back to raw string
        {
            const QDateTime dt = QDateTime::fromString(p.opened_at, Qt::ISODate).toLocalTime();
            ensure_item(positions_table_, i, 1)
                ->setText(dt.isValid() ? dt.toString("yyyy-MM-dd HH:mm") : p.opened_at);
        }

        ensure_item(positions_table_, i, 2)->setText(p.side);
        ensure_item(positions_table_, i, 3)->setText(QString::number(p.quantity, 'f', 0));
        ensure_item(positions_table_, i, 4)->setText(QString::number(p.entry_price, 'f', 2));
        ensure_item(positions_table_, i, 5)->setText(QString::number(p.current_price, 'f', 2));

        // Col 6 — P&L
        auto* pnl_item = ensure_item(positions_table_, i, 6);
        pnl_item->setText(QString::number(p.unrealized_pnl, 'f', 2));
        pnl_item->setForeground(p.unrealized_pnl >= 0 ? QColor(fincept::ui::colors::POSITIVE())
                                                      : QColor(fincept::ui::colors::NEGATIVE()));

        // Col 7 — P&L %
        // Use notional as the divisor so both entry_price == 0 and quantity == 0
        // are safely handled by a single guard (avoids divide-by-zero).
        const double notional = p.entry_price * p.quantity;
        const double pct = notional != 0.0
            ? (p.unrealized_pnl / notional) * 100.0
            : 0.0;
        auto* pct_item = ensure_item(positions_table_, i, 7);
        pct_item->setText(QString::number(pct, 'f', 2) + "%");
        pct_item->setForeground(
            pct >= 0 ? QColor(fincept::ui::colors::POSITIVE())
                     : QColor(fincept::ui::colors::NEGATIVE()));
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
        ensure_item(orders_table_, i, 8)->setText("");
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
    sep_item->setForeground(QColor(fincept::ui::colors::AMBER()));
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
        pnl_item->setForeground(t.pnl >= 0 ? QColor(fincept::ui::colors::POSITIVE())
                                           : QColor(fincept::ui::colors::NEGATIVE()));

        ensure_item(orders_table_, row, 7)->setText(t.timestamp);
    }
}

void EquityBottomPanel::set_paper_stats(const trading::PtStats& stats) {
    stat_values_[0]->setText(QString::number(stats.total_pnl, 'f', 2));
    stat_values_[0]->setStyleSheet(
        QString("color: %1;")
            .arg(stats.total_pnl >= 0 ? fincept::ui::colors::POSITIVE() : fincept::ui::colors::NEGATIVE()));
    stat_values_[1]->setText(QString("%1%").arg(stats.win_rate * 100, 0, 'f', 1));
    stat_values_[2]->setText(QString::number(stats.total_trades));
    stat_values_[3]->setText(QString::number(stats.largest_win, 'f', 2));
    stat_values_[3]->setStyleSheet(QString("color: %1;").arg(fincept::ui::colors::POSITIVE()));
    stat_values_[4]->setText(QString::number(stats.largest_loss, 'f', 2));
    stat_values_[4]->setStyleSheet(QString("color: %1;").arg(fincept::ui::colors::NEGATIVE()));
}

void EquityBottomPanel::set_positions(const QVector<trading::BrokerPosition>& positions) {
    positions_table_->setRowCount(positions.size());
    for (int i = 0; i < positions.size(); ++i) {
        const auto& p = positions[i];
        ensure_item(positions_table_, i, 0)->setText(p.symbol);
        ensure_item(positions_table_, i, 1)->setText(p.exchange.isEmpty() ? "--" : p.exchange);
        ensure_item(positions_table_, i, 2)->setText(p.side.toUpper());
        ensure_item(positions_table_, i, 3)->setText(QString::number(p.quantity, 'f', 0));
        ensure_item(positions_table_, i, 4)->setText(QString::number(p.avg_price, 'f', 2));
        ensure_item(positions_table_, i, 5)->setText(QString::number(p.ltp, 'f', 2));

        auto* pnl_item = ensure_item(positions_table_, i, 6);
        pnl_item->setText(QString::number(p.pnl, 'f', 2));
        pnl_item->setForeground(p.pnl >= 0 ? QColor(fincept::ui::colors::POSITIVE())
                                           : QColor(fincept::ui::colors::NEGATIVE()));

        auto* pct_item = ensure_item(positions_table_, i, 7);
        pct_item->setText(QString("%1%").arg(p.pnl_pct, 0, 'f', 2));
        pct_item->setForeground(p.pnl_pct >= 0 ? QColor(fincept::ui::colors::POSITIVE())
                                               : QColor(fincept::ui::colors::NEGATIVE()));
    }
}

void EquityBottomPanel::set_holdings(const QVector<trading::BrokerHolding>& holdings) {
    last_holdings_ = holdings;
    if (holdings_import_btn_)
        holdings_import_btn_->setEnabled(!holdings.isEmpty());

    // Disable sorting during population so setItem assignments stay at intended rows.
    const bool was_sorting = holdings_table_->isSortingEnabled();
    holdings_table_->setSortingEnabled(false);
    holdings_table_->setRowCount(holdings.size());

    double total_invested = 0.0;
    double total_current = 0.0;
    double total_pnl = 0.0;

    const QColor pos_color(fincept::ui::colors::POSITIVE());
    const QColor neg_color(fincept::ui::colors::NEGATIVE());

    auto set_num = [](QTableWidgetItem* it, double v, int precision) {
        it->setData(Qt::DisplayRole, QString::number(v, 'f', precision));
        it->setData(Qt::UserRole, v); // for potential custom sort; Display still orders alphabetically
    };

    for (int i = 0; i < holdings.size(); ++i) {
        const auto& h = holdings[i];
        ensure_item(holdings_table_, i, 0)->setText(h.symbol);
        set_num(ensure_item(holdings_table_, i, 1), h.quantity, 0);
        set_num(ensure_item(holdings_table_, i, 2), h.avg_price, 2);
        set_num(ensure_item(holdings_table_, i, 3), h.ltp, 2);
        set_num(ensure_item(holdings_table_, i, 4), h.invested_value, 2);
        set_num(ensure_item(holdings_table_, i, 5), h.current_value, 2);

        auto* pnl_item = ensure_item(holdings_table_, i, 6);
        pnl_item->setText(QString::number(h.pnl, 'f', 2));
        pnl_item->setForeground(h.pnl >= 0 ? pos_color : neg_color);

        auto* pct_item = ensure_item(holdings_table_, i, 7);
        pct_item->setText(QString("%1%").arg(h.pnl_pct, 0, 'f', 2));
        pct_item->setForeground(h.pnl_pct >= 0 ? pos_color : neg_color);

        total_invested += h.invested_value;
        total_current  += h.current_value;
        total_pnl      += h.pnl;
    }

    const double total_pct = total_invested > 0 ? (total_pnl / total_invested) * 100.0 : 0.0;

    if (holdings_count_label_)
        holdings_count_label_->setText(QString::number(holdings.size()));
    if (holdings_invested_label_)
        holdings_invested_label_->setText(QString::number(total_invested, 'f', 2));
    if (holdings_current_label_)
        holdings_current_label_->setText(QString::number(total_current, 'f', 2));
    if (holdings_pnl_label_) {
        holdings_pnl_label_->setText(QString("%1%2")
                                         .arg(total_pnl >= 0 ? "+" : "")
                                         .arg(QString::number(total_pnl, 'f', 2)));
        holdings_pnl_label_->setStyleSheet(
            QString("color:%1;font-size:13px;font-weight:700;")
                .arg(total_pnl >= 0 ? fincept::ui::colors::POSITIVE() : fincept::ui::colors::NEGATIVE()));
    }
    if (holdings_pnl_pct_label_) {
        holdings_pnl_pct_label_->setText(QString("%1%2%")
                                             .arg(total_pct >= 0 ? "+" : "")
                                             .arg(QString::number(total_pct, 'f', 2)));
        holdings_pnl_pct_label_->setStyleSheet(
            QString("color:%1;font-size:13px;font-weight:700;")
                .arg(total_pct >= 0 ? fincept::ui::colors::POSITIVE() : fincept::ui::colors::NEGATIVE()));
    }

    holdings_table_->setSortingEnabled(was_sorting);
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

        // Action column — MODIFY button for open/pending orders
        const bool modifiable = (o.status == "new" || o.status == "partially_filled" || o.status == "accepted" ||
                                 o.status == "pending_new");
        if (modifiable) {
            auto* btn = new QPushButton("EDIT");
            btn->setObjectName("eqTableBtn");
            btn->setFixedHeight(18);
            btn->setStyleSheet(QString("QPushButton#eqTableBtn { background: rgba(217,119,6,0.15); "
                                       "color: %1; border: 1px solid %2; font-size: 10px; "
                                       "padding: 0 6px; border-radius: 2px; }")
                                   .arg(fincept::ui::colors::AMBER())
                                   .arg(fincept::ui::colors::AMBER_DIM()));
            btn->setCursor(Qt::PointingHandCursor);
            const QString oid = o.order_id;
            const double qty = o.quantity;
            const double prc = o.price;
            connect(btn, &QPushButton::clicked, this, [this, oid, qty, prc]() {
                // Show inline edit dialog
                auto* dlg = new QDialog(this);
                dlg->setWindowTitle("Modify Order");
                dlg->setFixedWidth(280);
                dlg->setStyleSheet(QString("QDialog { background: %1; color: %2; }"
                                           "QLabel { color: %3; font-size: 11px; }"
                                           "QLineEdit { background: %4; border: 1px solid %5; "
                                           "  color: %2; padding: 5px; border-radius: 2px; }"
                                           "QLineEdit:focus { border-color: %6; }"
                                           "QPushButton { padding: 6px 14px; font-weight: 700; border-radius: 2px; }")
                                       .arg(fincept::ui::colors::BG_SURFACE())
                                       .arg(fincept::ui::colors::TEXT_PRIMARY())
                                       .arg(fincept::ui::colors::TEXT_SECONDARY())
                                       .arg(fincept::ui::colors::BG_BASE())
                                       .arg(fincept::ui::colors::BORDER_MED())
                                       .arg(fincept::ui::colors::AMBER()));
                auto* vlay = new QVBoxLayout(dlg);
                vlay->setSpacing(6);
                vlay->setContentsMargins(14, 14, 14, 14);

                auto* qty_lbl = new QLabel("QTY");
                auto* qty_edit = new QLineEdit(QString::number(qty, 'f', 0));
                auto* prc_lbl = new QLabel("LIMIT PRICE");
                auto* prc_edit = new QLineEdit(QString::number(prc, 'f', 2));

                auto* btn_row = new QHBoxLayout;
                auto* ok_btn = new QPushButton("MODIFY");
                ok_btn->setStyleSheet(QString("background: rgba(217,119,6,0.15); color: %1; border: 1px solid %2;")
                                          .arg(fincept::ui::colors::AMBER())
                                          .arg(fincept::ui::colors::AMBER_DIM()));
                auto* cancel_btn = new QPushButton("CANCEL");
                cancel_btn->setStyleSheet(QString("background: rgba(220,38,38,0.1); color: %1; border: 1px solid %2;")
                                              .arg(fincept::ui::colors::NEGATIVE())
                                              .arg(fincept::ui::colors::NEGATIVE_DIM()));

                connect(ok_btn, &QPushButton::clicked, dlg, [dlg, this, oid, qty_edit, prc_edit]() {
                    const double new_qty = qty_edit->text().toDouble();
                    const double new_prc = prc_edit->text().toDouble();
                    if (new_qty > 0)
                        emit modify_order_requested(oid, new_qty, new_prc);
                    dlg->accept();
                });
                connect(cancel_btn, &QPushButton::clicked, dlg, &QDialog::reject);

                vlay->addWidget(qty_lbl);
                vlay->addWidget(qty_edit);
                vlay->addWidget(prc_lbl);
                vlay->addWidget(prc_edit);
                btn_row->addWidget(ok_btn);
                btn_row->addWidget(cancel_btn);
                vlay->addLayout(btn_row);
                dlg->exec();
                dlg->deleteLater();
            });
            orders_table_->setCellWidget(i, 8, btn);
        } else {
            orders_table_->setCellWidget(i, 8, nullptr);
        }
    }
}

void EquityBottomPanel::set_funds(const trading::BrokerFunds& funds) {
    available_label_->setText(QString::number(funds.available_balance, 'f', 2));
    used_margin_label_->setText(QString::number(funds.used_margin, 'f', 2));
    total_label_->setText(QString::number(funds.total_balance, 'f', 2));
    collateral_label_->setText(QString::number(funds.collateral, 'f', 2));
}

// ── Auctions Tab ────────────────────────────────────────────────────────────

void EquityBottomPanel::setup_auctions_tab() {
    auctions_table_ = new QTableWidget;
    auctions_table_->setObjectName("eqTable");
    auctions_table_->setColumnCount(6);
    auctions_table_->setHorizontalHeaderLabels({"Date", "Type", "Time", "Price", "Size", "Exchange"});
    auctions_table_->verticalHeader()->setVisible(false);
    auctions_table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    auctions_table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    auctions_table_->setShowGrid(false);
    auctions_table_->horizontalHeader()->setStretchLastSection(true);
    auctions_table_->verticalHeader()->setDefaultSectionSize(20);
    auctions_table_->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    tabs_->addTab(auctions_table_, "AUCTIONS");
}

void EquityBottomPanel::set_auctions(const QVector<trading::BrokerAuction>& auctions) {
    auctions_table_->setUpdatesEnabled(false);
    auctions_table_->setRowCount(0);
    for (const auto& auction : auctions) {
        for (const auto& entry : auction.entries) {
            const int row = auctions_table_->rowCount();
            auctions_table_->insertRow(row);
            const bool is_open = (entry.auction_type == "O");
            const QColor type_color =
                is_open ? QColor(fincept::ui::colors::AMBER()) : QColor(fincept::ui::colors::INFO());

            auto set = [&](int col, const QString& text,
                           const QColor& fg = QColor(fincept::ui::colors::TEXT_PRIMARY())) {
                auto* item = new QTableWidgetItem(text);
                item->setForeground(fg);
                item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
                auctions_table_->setItem(row, col, item);
            };

            const QString time_str =
                QDateTime::fromString(entry.timestamp, Qt::ISODateWithMs).toLocalTime().toString("hh:mm:ss");
            set(0, auction.date, QColor(fincept::ui::colors::TEXT_SECONDARY()));
            set(1, is_open ? "OPEN" : "CLOSE", type_color);
            set(2, time_str, QColor(fincept::ui::colors::TEXT_SECONDARY()));
            set(3, QString::number(entry.price, 'f', 2));
            set(4, QString::number(entry.size, 'f', 0), QColor(fincept::ui::colors::TEXT_SECONDARY()));
            set(5, entry.exchange, QColor(fincept::ui::colors::TEXT_SECONDARY()));
        }
    }
    auctions_table_->setUpdatesEnabled(true);
}

void EquityBottomPanel::set_condition_codes(const QMap<QString, QString>& codes) {
    condition_codes_ = codes;
}

// ── Time & Sales Tab ────────────────────────────────────────────────────────

void EquityBottomPanel::setup_time_sales_tab() {
    time_sales_table_ = new QTableWidget;
    time_sales_table_->setObjectName("eqTable");
    time_sales_table_->setColumnCount(6);
    time_sales_table_->setHorizontalHeaderLabels({"Time", "Price", "Size", "Exchange", "Conditions", "Tape"});
    time_sales_table_->verticalHeader()->setVisible(false);
    time_sales_table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    time_sales_table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    time_sales_table_->setShowGrid(false);
    time_sales_table_->horizontalHeader()->setStretchLastSection(true);
    time_sales_table_->verticalHeader()->setDefaultSectionSize(20);
    time_sales_table_->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    time_sales_table_->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
    tabs_->addTab(time_sales_table_, "TIME & SALES");
}

static void fill_trade_row(QTableWidget* table, int row, const trading::BrokerTrade& t,
                           const QMap<QString, QString>& codes = {}) {
    // Parse ISO timestamp → time only
    const QString time_str =
        QDateTime::fromString(t.timestamp, Qt::ISODateWithMs).toLocalTime().toString("hh:mm:ss.zzz");

    auto set = [&](int col, const QString& text, const QColor& fg = QColor(fincept::ui::colors::TEXT_PRIMARY())) {
        auto* item = new QTableWidgetItem(text);
        item->setForeground(fg);
        item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
        table->setItem(row, col, item);
    };

    set(0, time_str.isEmpty() ? t.timestamp.right(12) : time_str, QColor(fincept::ui::colors::TEXT_SECONDARY()));
    set(1, QString::number(t.price, 'f', 2),
        t.price > 0 ? QColor(fincept::ui::colors::TEXT_PRIMARY()) : QColor(fincept::ui::colors::TEXT_SECONDARY()));
    set(2, QString::number(t.size, 'f', 0), QColor(fincept::ui::colors::TEXT_SECONDARY()));
    set(3, t.exchange, QColor(fincept::ui::colors::TEXT_SECONDARY()));
    QStringList decoded;
    for (const auto& c : t.conditions)
        decoded.append(codes.contains(c) ? codes[c] : c);
    set(4, decoded.join(", "), QColor(fincept::ui::colors::TEXT_TERTIARY()));
    set(5, t.tape, QColor(fincept::ui::colors::TEXT_TERTIARY()));
}

void EquityBottomPanel::set_time_sales(const QVector<trading::BrokerTrade>& trades) {
    time_sales_table_->setUpdatesEnabled(false);
    time_sales_table_->setRowCount(trades.size());
    for (int i = 0; i < trades.size(); ++i)
        fill_trade_row(time_sales_table_, i, trades[i], condition_codes_);
    time_sales_table_->setUpdatesEnabled(true);
    // Scroll to most recent (bottom — trades arrive oldest-first)
    if (!trades.isEmpty())
        time_sales_table_->scrollToBottom();
}

void EquityBottomPanel::prepend_trade(const trading::BrokerTrade& trade) {
    // Insert at row 0 (newest at top), cap at 500 rows to avoid unbounded growth
    time_sales_table_->insertRow(0);
    fill_trade_row(time_sales_table_, 0, trade, condition_codes_);
    if (time_sales_table_->rowCount() > 500)
        time_sales_table_->removeRow(500);
}

// ── Calendar Tab ───────────────────────────────────────────────────────────

void EquityBottomPanel::setup_calendar_tab() {
    auto* container = new QWidget(this);
    auto* vlay = new QVBoxLayout(container);
    vlay->setContentsMargins(0, 0, 0, 0);
    vlay->setSpacing(0);

    // Clock status banner
    auto* banner = new QWidget(this);
    banner->setObjectName("calClockBanner");
    banner->setFixedHeight(28);
    banner->setStyleSheet(QString("QWidget#calClockBanner { background: %1; border-bottom: 1px solid %2; }")
                              .arg(fincept::ui::colors::BG_BASE())
                              .arg(fincept::ui::colors::BORDER_DIM()));
    auto* banner_lay = new QHBoxLayout(banner);
    banner_lay->setContentsMargins(10, 0, 10, 0);
    banner_lay->setSpacing(16);

    clock_status_label_ = new QLabel("● MARKET --");
    clock_status_label_->setObjectName("calClockStatus");
    clock_status_label_->setStyleSheet(
        QString("color: %1; font-size: 11px; font-weight: 700;").arg(fincept::ui::colors::TEXT_TERTIARY()));

    clock_next_label_ = new QLabel("");
    clock_next_label_->setObjectName("calClockNext");
    clock_next_label_->setStyleSheet(QString("color: %1; font-size: 10px;").arg(fincept::ui::colors::TEXT_TERTIARY()));

    banner_lay->addWidget(clock_status_label_);
    banner_lay->addWidget(clock_next_label_);
    banner_lay->addStretch();
    vlay->addWidget(banner);

    // Calendar table
    calendar_table_ = new QTableWidget;
    calendar_table_->setObjectName("eqTable");
    calendar_table_->setColumnCount(5);
    calendar_table_->setHorizontalHeaderLabels({"Date", "Open (ET)", "Close (ET)", "Pre-Market", "After-Hours"});
    calendar_table_->verticalHeader()->setVisible(false);
    calendar_table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    calendar_table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    calendar_table_->setShowGrid(false);
    calendar_table_->horizontalHeader()->setStretchLastSection(true);
    calendar_table_->verticalHeader()->setDefaultSectionSize(22);
    // Pixel-level smooth scrolling
    calendar_table_->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    calendar_table_->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
    vlay->addWidget(calendar_table_);

    tabs_->addTab(container, "CALENDAR");
}

void EquityBottomPanel::set_calendar(const QVector<trading::MarketCalendarDay>& days) {
    calendar_table_->setRowCount(days.size());
    const QString today = QDate::currentDate().toString("yyyy-MM-dd");
    for (int i = 0; i < days.size(); ++i) {
        const auto& d = days[i];
        const bool is_today = (d.date == today);
        const QColor row_color = is_today ? QColor(217, 119, 6, 40) : QColor(0, 0, 0, 0);

        auto set = [&](int col, const QString& text) {
            auto* item = ensure_item(calendar_table_, i, col);
            item->setText(text);
            if (is_today) {
                item->setForeground(QColor(fincept::ui::colors::AMBER()));
                item->setBackground(row_color);
            }
        };
        set(0, d.date);
        set(1, d.open);
        set(2, d.close);
        set(3, d.session_open.isEmpty() ? "--" : d.session_open);
        set(4, d.session_close.isEmpty() ? "--" : d.session_close);
    }
    // Scroll to today (or nearest future day) centred in the view
    for (int i = 0; i < days.size(); ++i) {
        if (days[i].date >= today) {
            calendar_table_->scrollToItem(calendar_table_->item(i, 0), QAbstractItemView::PositionAtCenter);
            break;
        }
    }
}

void EquityBottomPanel::set_clock(const trading::MarketClock& clock) {
    if (!clock_status_label_)
        return;

    if (clock.is_open) {
        clock_status_label_->setText("● MARKET OPEN");
        clock_status_label_->setStyleSheet(
            QString("color: %1; font-size: 11px; font-weight: 700;").arg(fincept::ui::colors::POSITIVE()));
    } else {
        clock_status_label_->setText("● MARKET CLOSED");
        clock_status_label_->setStyleSheet(
            QString("color: %1; font-size: 11px; font-weight: 700;").arg(fincept::ui::colors::NEGATIVE()));
    }

    // Parse ISO timestamps and show local-friendly next event
    if (!clock.next_open.isEmpty() || !clock.next_close.isEmpty()) {
        const QString next_event = clock.is_open ? QString("Closes %1")
                                                       .arg(QDateTime::fromString(clock.next_close, Qt::ISODateWithMs)
                                                                .toLocalTime()
                                                                .toString("MMM d h:mm ap"))
                                                 : QString("Opens %1")
                                                       .arg(QDateTime::fromString(clock.next_open, Qt::ISODateWithMs)
                                                                .toLocalTime()
                                                                .toString("MMM d h:mm ap"));
        clock_next_label_->setText(next_event);
        clock_next_label_->setStyleSheet(
            QString("color: %1; font-size: 10px;").arg(fincept::ui::colors::TEXT_SECONDARY()));
    }
}

} // namespace fincept::screens::equity
