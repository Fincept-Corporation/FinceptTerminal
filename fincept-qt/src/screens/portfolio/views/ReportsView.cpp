// src/screens/portfolio/views/ReportsView.cpp
#include "screens/portfolio/views/ReportsView.h"

#include "storage/repositories/PortfolioRepository.h"
#include "ui/theme/Theme.h"

#include <QGridLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>

namespace fincept::screens {

ReportsView::ReportsView(QWidget* parent) : QWidget(parent) {
    build_ui();
}

void ReportsView::build_ui() {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    tabs_ = new QTabWidget;
    tabs_->setDocumentMode(true);
    tabs_->setStyleSheet(QString("QTabWidget::pane { border:0; background:%1; }"
                                 "QTabBar::tab { background:%2; color:%3; padding:6px 14px; border:0;"
                                 "  border-bottom:2px solid transparent; font-size:9px; font-weight:700;"
                                 "  letter-spacing:0.5px; }"
                                 "QTabBar::tab:selected { color:%4; border-bottom:2px solid %4; }"
                                 "QTabBar::tab:hover { color:%5; }")
                             .arg(ui::colors::BG_BASE, ui::colors::BG_SURFACE, ui::colors::TEXT_SECONDARY,
                                  ui::colors::AMBER, ui::colors::TEXT_PRIMARY));

    // ── Portfolio Summary Report ─────────────────────────────────────────────
    summary_panel_ = new QWidget(this);
    summary_panel_->setStyleSheet(QString("background:%1;").arg(ui::colors::BG_BASE));
    tabs_->addTab(summary_panel_, "SUMMARY");

    // ── Transaction History ──────────────────────────────────────────────────
    auto* txn_w = new QWidget(this);
    auto* txn_layout = new QVBoxLayout(txn_w);
    txn_layout->setContentsMargins(12, 8, 12, 8);

    auto* txn_title = new QLabel("TRANSACTION HISTORY");
    txn_title->setStyleSheet(
        QString("color:%1; font-size:11px; font-weight:700; letter-spacing:1px;").arg(ui::colors::AMBER));
    txn_layout->addWidget(txn_title);

    txn_table_ = new QTableWidget;
    txn_table_->setColumnCount(7);
    txn_table_->setHorizontalHeaderLabels({"DATE", "SYMBOL", "TYPE", "QTY", "PRICE", "TOTAL", "NOTES"});
    txn_table_->setSelectionMode(QAbstractItemView::NoSelection);
    txn_table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    txn_table_->setShowGrid(false);
    txn_table_->verticalHeader()->setVisible(false);
    txn_table_->horizontalHeader()->setStretchLastSection(true);
    txn_table_->setColumnWidth(0, 140);
    txn_table_->setColumnWidth(1, 80);
    txn_table_->setColumnWidth(2, 60);
    txn_table_->setColumnWidth(3, 70);
    txn_table_->setColumnWidth(4, 80);
    txn_table_->setColumnWidth(5, 100);
    txn_table_->setStyleSheet(QString("QTableWidget { background:%1; color:%2; border:none; font-size:11px; }"
                                      "QTableWidget::item { padding:4px 8px; border-bottom:1px solid %3; }"
                                      "QHeaderView::section { background:%4; color:%5; border:none;"
                                      "  border-bottom:2px solid %6; padding:4px 8px; font-size:9px;"
                                      "  font-weight:700; letter-spacing:0.5px; }")
                                  .arg(ui::colors::BG_BASE, ui::colors::TEXT_PRIMARY, ui::colors::BORDER_DIM,
                                       ui::colors::BG_SURFACE, ui::colors::TEXT_SECONDARY, ui::colors::AMBER));
    txn_layout->addWidget(txn_table_, 1);
    tabs_->addTab(txn_w, "TRANSACTIONS");

    // ── Performance Attribution ──────────────────────────────────────────────
    auto* attr_w = new QWidget(this);
    auto* attr_layout = new QVBoxLayout(attr_w);
    attr_layout->setContentsMargins(12, 8, 12, 8);

    auto* attr_title = new QLabel("PERFORMANCE ATTRIBUTION");
    attr_title->setStyleSheet(
        QString("color:%1; font-size:11px; font-weight:700; letter-spacing:1px;").arg(ui::colors::AMBER));
    attr_layout->addWidget(attr_title);

    attr_table_ = new QTableWidget;
    attr_table_->setColumnCount(6);
    attr_table_->setHorizontalHeaderLabels({"SYMBOL", "WEIGHT", "RETURN", "CONTRIBUTION", "P&L", "STATUS"});
    attr_table_->setSelectionMode(QAbstractItemView::NoSelection);
    attr_table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    attr_table_->setShowGrid(false);
    attr_table_->verticalHeader()->setVisible(false);
    attr_table_->horizontalHeader()->setStretchLastSection(true);
    attr_table_->setStyleSheet(QString("QTableWidget { background:%1; color:%2; border:none; font-size:11px; }"
                                       "QTableWidget::item { padding:4px 8px; border-bottom:1px solid %3; }"
                                       "QHeaderView::section { background:%4; color:%5; border:none;"
                                       "  border-bottom:2px solid %6; padding:4px 8px; font-size:9px;"
                                       "  font-weight:700; letter-spacing:0.5px; }")
                                   .arg(ui::colors::BG_BASE, ui::colors::TEXT_PRIMARY, ui::colors::BORDER_DIM,
                                        ui::colors::BG_SURFACE, ui::colors::TEXT_SECONDARY, ui::colors::AMBER));
    attr_layout->addWidget(attr_table_, 1);
    tabs_->addTab(attr_w, "ATTRIBUTION");

    layout->addWidget(tabs_);
}

void ReportsView::set_data(const portfolio::PortfolioSummary& summary, const QString& currency) {
    summary_ = summary;
    currency_ = currency;
    update_summary();
    update_transactions();
    update_attribution();
}

void ReportsView::update_summary() {
    if (summary_panel_->layout())
        delete summary_panel_->layout();

    auto* layout = new QVBoxLayout(summary_panel_);
    layout->setContentsMargins(16, 12, 16, 12);
    layout->setSpacing(12);

    auto* title = new QLabel("PORTFOLIO SUMMARY REPORT");
    title->setStyleSheet(
        QString("color:%1; font-size:12px; font-weight:700; letter-spacing:1px;").arg(ui::colors::AMBER));
    layout->addWidget(title);

    auto* grid = new QGridLayout;
    grid->setSpacing(8);

    auto add_card = [&](int r, int c, const QString& label, const QString& value, const char* color) {
        auto* card = new QWidget(this);
        card->setStyleSheet(
            QString("background:%1; border:1px solid %2;").arg(ui::colors::BG_RAISED, ui::colors::BORDER_DIM));
        auto* cl = new QVBoxLayout(card);
        cl->setContentsMargins(12, 8, 12, 8);
        cl->setSpacing(2);

        auto* lbl = new QLabel(label);
        lbl->setStyleSheet(
            QString("color:%1; font-size:8px; font-weight:700; letter-spacing:0.5px;").arg(ui::colors::TEXT_TERTIARY));
        cl->addWidget(lbl);

        auto* val = new QLabel(value);
        val->setStyleSheet(QString("color:%1; font-size:16px; font-weight:700;").arg(color));
        cl->addWidget(val);

        grid->addWidget(card, r, c);
    };

    auto fmt = [](double v, int dp = 2) { return QString::number(v, 'f', dp); };

    add_card(0, 0, "PORTFOLIO", summary_.portfolio.name.toUpper(), ui::colors::AMBER);
    add_card(0, 1, "TOTAL VALUE", QString("%1 %2").arg(currency_, fmt(summary_.total_market_value)),
             ui::colors::WARNING);
    add_card(0, 2, "COST BASIS", QString("%1 %2").arg(currency_, fmt(summary_.total_cost_basis)), ui::colors::CYAN);
    add_card(0, 3, "UNREALIZED P&L",
             QString("%1%2").arg(summary_.total_unrealized_pnl >= 0 ? "+" : "").arg(fmt(summary_.total_unrealized_pnl)),
             summary_.total_unrealized_pnl >= 0 ? ui::colors::POSITIVE : ui::colors::NEGATIVE);

    add_card(1, 0, "POSITIONS", QString::number(summary_.total_positions), ui::colors::TEXT_PRIMARY);
    add_card(1, 1, "GAINERS", QString::number(summary_.gainers), ui::colors::POSITIVE);
    add_card(1, 2, "LOSERS", QString::number(summary_.losers), ui::colors::NEGATIVE);
    add_card(1, 3, "RETURN",
             QString("%1%2%")
                 .arg(summary_.total_unrealized_pnl_percent >= 0 ? "+" : "")
                 .arg(fmt(summary_.total_unrealized_pnl_percent)),
             summary_.total_unrealized_pnl_percent >= 0 ? ui::colors::POSITIVE : ui::colors::NEGATIVE);

    layout->addLayout(grid);

    // Holdings breakdown
    auto* breakdown_title = new QLabel("HOLDINGS BREAKDOWN");
    breakdown_title->setStyleSheet(
        QString("color:%1; font-size:10px; font-weight:700; letter-spacing:1px;").arg(ui::colors::TEXT_SECONDARY));
    layout->addWidget(breakdown_title);

    auto* breakdown = new QTableWidget;
    breakdown->setColumnCount(6);
    breakdown->setHorizontalHeaderLabels({"SYMBOL", "QTY", "AVG COST", "CURRENT", "P&L", "WEIGHT"});
    breakdown->setSelectionMode(QAbstractItemView::NoSelection);
    breakdown->setEditTriggers(QAbstractItemView::NoEditTriggers);
    breakdown->setShowGrid(false);
    breakdown->verticalHeader()->setVisible(false);
    breakdown->horizontalHeader()->setStretchLastSection(true);
    breakdown->setStyleSheet(QString("QTableWidget { background:%1; color:%2; border:none; font-size:11px; }"
                                     "QTableWidget::item { padding:3px 6px; border-bottom:1px solid %3; }"
                                     "QHeaderView::section { background:%4; color:%5; border:none;"
                                     "  border-bottom:1px solid %6; padding:3px 6px; font-size:9px;"
                                     "  font-weight:700; }")
                                 .arg(ui::colors::BG_BASE, ui::colors::TEXT_PRIMARY, ui::colors::BORDER_DIM,
                                      ui::colors::BG_SURFACE, ui::colors::TEXT_SECONDARY, ui::colors::AMBER));

    auto sorted = summary_.holdings;
    std::sort(sorted.begin(), sorted.end(), [](const auto& a, const auto& b) { return a.weight > b.weight; });

    breakdown->setRowCount(sorted.size());
    for (int r = 0; r < sorted.size(); ++r) {
        const auto& h = sorted[r];
        breakdown->setRowHeight(r, 26);

        auto set = [&](int col, const QString& text, const char* color = nullptr) {
            auto* item = new QTableWidgetItem(text);
            item->setTextAlignment(col == 0 ? (Qt::AlignLeft | Qt::AlignVCenter) : (Qt::AlignRight | Qt::AlignVCenter));
            if (color)
                item->setForeground(QColor(color));
            breakdown->setItem(r, col, item);
        };

        set(0, h.symbol, ui::colors::CYAN);
        set(1, fmt(h.quantity, h.quantity == std::floor(h.quantity) ? 0 : 2));
        set(2, fmt(h.avg_buy_price));
        set(3, fmt(h.current_price));
        const char* pc = h.unrealized_pnl >= 0 ? ui::colors::POSITIVE : ui::colors::NEGATIVE;
        set(4,
            QString("%1%2 (%3%4%)")
                .arg(h.unrealized_pnl >= 0 ? "+" : "")
                .arg(fmt(h.unrealized_pnl))
                .arg(h.unrealized_pnl_percent >= 0 ? "+" : "")
                .arg(fmt(h.unrealized_pnl_percent)),
            pc);
        set(5, QString("%1%").arg(fmt(h.weight, 1)));
    }
    layout->addWidget(breakdown, 1);
}

void ReportsView::update_transactions() {
    auto txns_r = PortfolioRepository::instance().get_transactions(summary_.portfolio.id, 100);
    if (txns_r.is_err())
        return;

    const auto& txns = txns_r.value();
    txn_table_->setRowCount(txns.size());

    for (int r = 0; r < txns.size(); ++r) {
        const auto& t = txns[r];
        txn_table_->setRowHeight(r, 28);

        auto set = [&](int col, const QString& text, const char* color = nullptr) {
            auto* item = new QTableWidgetItem(text);
            item->setTextAlignment(col <= 1 ? (Qt::AlignLeft | Qt::AlignVCenter) : (Qt::AlignRight | Qt::AlignVCenter));
            if (color)
                item->setForeground(QColor(color));
            txn_table_->setItem(r, col, item);
        };

        const char* type_color = t.transaction_type == "BUY"    ? ui::colors::POSITIVE
                                 : t.transaction_type == "SELL" ? ui::colors::NEGATIVE
                                                                : ui::colors::WARNING;

        set(0, t.transaction_date, ui::colors::TEXT_SECONDARY);
        set(1, t.symbol, ui::colors::CYAN);
        set(2, t.transaction_type, type_color);
        set(3, QString::number(t.quantity, 'f', 2));
        set(4, QString::number(t.price, 'f', 2));
        set(5, QString("%1 %2").arg(currency_, QString::number(t.total_value, 'f', 2)));
        set(6, t.notes, ui::colors::TEXT_TERTIARY);
    }
}

void ReportsView::update_attribution() {
    auto sorted = summary_.holdings;
    std::sort(sorted.begin(), sorted.end(), [](const auto& a, const auto& b) { return a.weight > b.weight; });

    attr_table_->setRowCount(sorted.size());

    double total_pnl = summary_.total_unrealized_pnl;

    for (int r = 0; r < sorted.size(); ++r) {
        const auto& h = sorted[r];
        attr_table_->setRowHeight(r, 28);

        auto set = [&](int col, const QString& text, const char* color = nullptr) {
            auto* item = new QTableWidgetItem(text);
            item->setTextAlignment(col == 0 ? (Qt::AlignLeft | Qt::AlignVCenter) : (Qt::AlignRight | Qt::AlignVCenter));
            if (color)
                item->setForeground(QColor(color));
            attr_table_->setItem(r, col, item);
        };

        double contribution = (total_pnl != 0) ? (h.unrealized_pnl / std::abs(total_pnl)) * 100.0 : 0;

        const char* ret_color = h.unrealized_pnl_percent >= 0 ? ui::colors::POSITIVE : ui::colors::NEGATIVE;
        const char* contrib_color = contribution >= 0 ? ui::colors::POSITIVE : ui::colors::NEGATIVE;
        QString status = h.unrealized_pnl_percent > 5    ? "OUTPERFORM"
                         : h.unrealized_pnl_percent < -5 ? "UNDERPERFORM"
                                                         : "NEUTRAL";
        const char* status_color = status == "OUTPERFORM"     ? ui::colors::POSITIVE
                                   : status == "UNDERPERFORM" ? ui::colors::NEGATIVE
                                                              : ui::colors::TEXT_TERTIARY;

        set(0, h.symbol, ui::colors::CYAN);
        set(1, QString("%1%").arg(QString::number(h.weight, 'f', 1)));
        set(2,
            QString("%1%2%")
                .arg(h.unrealized_pnl_percent >= 0 ? "+" : "")
                .arg(QString::number(h.unrealized_pnl_percent, 'f', 2)),
            ret_color);
        set(3, QString("%1%2%").arg(contribution >= 0 ? "+" : "").arg(QString::number(contribution, 'f', 1)),
            contrib_color);
        set(4, QString("%1 %2").arg(currency_, QString::number(h.unrealized_pnl, 'f', 2)), ret_color);
        set(5, status, status_color);
    }
}

} // namespace fincept::screens
