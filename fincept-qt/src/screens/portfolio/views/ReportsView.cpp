// src/screens/portfolio/views/ReportsView.cpp
#include "screens/portfolio/views/ReportsView.h"

#include "storage/repositories/PortfolioRepository.h"
#include "ui/theme/Theme.h"

#include <QEvent>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QTabBar>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>

namespace fincept::screens {

namespace {

/// Recursively empties a layout, destroying the widgets it owns.
///
/// `delete panel->layout()` alone does NOT destroy child widgets — they stay
/// parented to the panel, keep their last geometry and keep painting, so every
/// rebuild stacked another full set of cards + tables on top of the previous
/// one (a visible artefact and an unbounded leak). Always drain the layout
/// first.
void clear_layout(QLayout* layout) {
    if (!layout)
        return;
    QLayoutItem* item = nullptr;
    while ((item = layout->takeAt(0)) != nullptr) {
        if (auto* w = item->widget()) {
            w->hide();
            w->deleteLater();
        } else if (auto* child = item->layout()) {
            clear_layout(child);
        }
        // `item` IS the nested layout when item->layout() is non-null, so a
        // single delete covers both cases — never delete both.
        delete item;
    }
}

} // namespace

ReportsView::ReportsView(QWidget* parent) : QWidget(parent) {
    build_ui();
}

void ReportsView::build_ui() {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    tabs_ = new QTabWidget;
    tabs_->tabBar()->setElideMode(Qt::ElideNone);
    tabs_->tabBar()->setExpanding(false);
    tabs_->tabBar()->setUsesScrollButtons(false);
    tabs_->setDocumentMode(true);
    tabs_->setStyleSheet(QString("QTabWidget::pane { border:0; background:%1; }"
                                 "QTabBar::tab { background:%2; color:%3; padding:6px 14px; border:0;"
                                 "  border-bottom:2px solid transparent; font-size:9px; font-weight:700;"
                                 "  letter-spacing:0.5px; }"
                                 "QTabBar::tab:selected { color:%4; border-bottom:2px solid %4; }"
                                 "QTabBar::tab:hover { color:%5; }")
                             .arg(ui::colors::BG_BASE(), ui::colors::BG_SURFACE(), ui::colors::TEXT_SECONDARY(),
                                  ui::colors::AMBER(), ui::colors::TEXT_PRIMARY()));

    // ── Portfolio Summary Report ─────────────────────────────────────────────
    summary_panel_ = new QWidget(this);
    summary_panel_->setStyleSheet(QString("background:%1;").arg(ui::colors::BG_BASE()));
    summary_tab_index_ = tabs_->addTab(summary_panel_, tr("SUMMARY"));

    // ── Transaction History ──────────────────────────────────────────────────
    auto* txn_w = new QWidget(this);
    auto* txn_layout = new QVBoxLayout(txn_w);
    txn_layout->setContentsMargins(12, 8, 12, 8);

    txn_title_ = new QLabel(tr("TRANSACTION HISTORY"));
    txn_title_->setStyleSheet(
        QString("color:%1; font-size:11px; font-weight:700; letter-spacing:1px;").arg(ui::colors::AMBER()));
    txn_layout->addWidget(txn_title_);

    txn_table_ = new QTableWidget;
    txn_table_->setColumnCount(7);
    txn_table_->setHorizontalHeaderLabels(
        {tr("DATE"), tr("SYMBOL"), tr("TYPE"), tr("QTY"), tr("PRICE"), tr("TOTAL"), tr("NOTES")});
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
                                  .arg(ui::colors::BG_BASE(), ui::colors::TEXT_PRIMARY(), ui::colors::BORDER_DIM(),
                                       ui::colors::BG_SURFACE(), ui::colors::TEXT_SECONDARY(), ui::colors::AMBER()));
    txn_layout->addWidget(txn_table_, 1);
    txn_tab_index_ = tabs_->addTab(txn_w, tr("TRANSACTIONS"));

    // ── Performance Attribution ──────────────────────────────────────────────
    auto* attr_w = new QWidget(this);
    auto* attr_layout = new QVBoxLayout(attr_w);
    attr_layout->setContentsMargins(12, 8, 12, 8);

    attr_title_ = new QLabel(tr("PERFORMANCE ATTRIBUTION"));
    attr_title_->setStyleSheet(
        QString("color:%1; font-size:11px; font-weight:700; letter-spacing:1px;").arg(ui::colors::AMBER()));
    attr_layout->addWidget(attr_title_);

    attr_table_ = new QTableWidget;
    attr_table_->setColumnCount(7);
    attr_table_->setHorizontalHeaderLabels(
        {tr("SYMBOL"), tr("WEIGHT"), tr("RETURN"), tr("CONTRIBUTION"), tr("DAY CONTRIB"), tr("P&L"), tr("VS BOOK")});
    attr_table_->setToolTip(tr("CONTRIBUTION = the holding's P&L ÷ the book's total cost — percentage points of the "
                               "portfolio's unrealized return; the column sums to the total.\n"
                               "DAY CONTRIB = today's move of the holding ÷ yesterday's book value.\n"
                               "VS BOOK = the holding's return against the portfolio's own return."));
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
                                   .arg(ui::colors::BG_BASE(), ui::colors::TEXT_PRIMARY(), ui::colors::BORDER_DIM(),
                                        ui::colors::BG_SURFACE(), ui::colors::TEXT_SECONDARY(), ui::colors::AMBER()));
    attr_layout->addWidget(attr_table_, 1);
    attr_tab_index_ = tabs_->addTab(attr_w, tr("ATTRIBUTION"));

    layout->addWidget(tabs_);
}

void ReportsView::set_data(const portfolio::PortfolioSummary& summary, const QString& currency) {
    summary_ = summary;
    currency_ = currency;
    has_data_ = true;
    update_summary();
    update_transactions();
    update_attribution();
}

void ReportsView::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange)
        retranslateUi();
    QWidget::changeEvent(event);
}

void ReportsView::retranslateUi() {
    if (tabs_) {
        if (summary_tab_index_ >= 0)
            tabs_->setTabText(summary_tab_index_, tr("SUMMARY"));
        if (txn_tab_index_ >= 0)
            tabs_->setTabText(txn_tab_index_, tr("TRANSACTIONS"));
        if (attr_tab_index_ >= 0)
            tabs_->setTabText(attr_tab_index_, tr("ATTRIBUTION"));
    }
    if (txn_title_)
        txn_title_->setText(tr("TRANSACTION HISTORY"));
    if (attr_title_)
        attr_title_->setText(tr("PERFORMANCE ATTRIBUTION"));

    if (txn_table_)
        txn_table_->setHorizontalHeaderLabels(
            {tr("DATE"), tr("SYMBOL"), tr("TYPE"), tr("QTY"), tr("PRICE"), tr("TOTAL"), tr("NOTES")});
    if (attr_table_)
        attr_table_->setHorizontalHeaderLabels({tr("SYMBOL"), tr("WEIGHT"), tr("RETURN"), tr("CONTRIBUTION"),
                                                tr("DAY CONTRIB"), tr("P&L"), tr("VS BOOK")});

    // re-render dynamic content so tr() row labels and card titles pick up new locale
    if (has_data_) {
        update_summary();
        update_transactions();
        update_attribution();
    }
}

void ReportsView::update_summary() {
    if (auto* old = summary_panel_->layout()) {
        clear_layout(old);
        delete old;
    }

    auto* layout = new QVBoxLayout(summary_panel_);
    layout->setContentsMargins(16, 12, 16, 12);
    layout->setSpacing(12);

    auto* title = new QLabel(tr("PORTFOLIO SUMMARY REPORT"));
    title->setStyleSheet(
        QString("color:%1; font-size:12px; font-weight:700; letter-spacing:1px;").arg(ui::colors::AMBER()));
    layout->addWidget(title);

    auto* grid = new QGridLayout;
    grid->setSpacing(8);

    auto add_card = [&](int r, int c, const QString& label, const QString& value, const char* color) {
        auto* card = new QWidget(this);
        card->setStyleSheet(
            QString("background:%1; border:1px solid %2;").arg(ui::colors::BG_RAISED(), ui::colors::BORDER_DIM()));
        auto* cl = new QVBoxLayout(card);
        cl->setContentsMargins(12, 8, 12, 8);
        cl->setSpacing(2);

        auto* lbl = new QLabel(label);
        lbl->setStyleSheet(QString("color:%1; font-size:8px; font-weight:700; letter-spacing:0.5px;")
                               .arg(ui::colors::TEXT_TERTIARY()));
        cl->addWidget(lbl);

        auto* val = new QLabel(value);
        val->setStyleSheet(QString("color:%1; font-size:16px; font-weight:700;").arg(color));
        cl->addWidget(val);

        grid->addWidget(card, r, c);
    };

    auto fmt = [](double v, int dp = 2) { return QString::number(v, 'f', dp); };

    add_card(0, 0, tr("PORTFOLIO"), summary_.portfolio.name.toUpper(), ui::colors::AMBER);
    add_card(0, 1, tr("TOTAL VALUE"), QString("%1 %2").arg(currency_, fmt(summary_.total_market_value)),
             ui::colors::WARNING);
    add_card(0, 2, tr("COST BASIS"), QString("%1 %2").arg(currency_, fmt(summary_.total_cost_basis)), ui::colors::CYAN);
    add_card(0, 3, tr("UNREALIZED P&L"),
             QString("%1%2").arg(summary_.total_unrealized_pnl >= 0 ? "+" : "").arg(fmt(summary_.total_unrealized_pnl)),
             summary_.total_unrealized_pnl >= 0 ? ui::colors::POSITIVE : ui::colors::NEGATIVE);

    add_card(1, 0, tr("POSITIONS"), QString::number(summary_.total_positions), ui::colors::TEXT_PRIMARY);
    add_card(1, 1, tr("GAINERS"), QString::number(summary_.gainers), ui::colors::POSITIVE);
    add_card(1, 2, tr("LOSERS"), QString::number(summary_.losers), ui::colors::NEGATIVE);
    add_card(1, 3, tr("UNREALIZED RETURN"),
             QString("%1%2%")
                 .arg(summary_.total_unrealized_pnl_percent >= 0 ? "+" : "")
                 .arg(fmt(summary_.total_unrealized_pnl_percent)),
             summary_.total_unrealized_pnl_percent >= 0 ? ui::colors::POSITIVE : ui::colors::NEGATIVE);

    // What the open positions alone do not show: P&L already banked, income received,
    // and the all-in result. The ledger totals were computed but never reported here.
    auto signed_money = [&](double v) {
        return QString("%1%2 %3").arg(v >= 0 ? "+" : "-", currency_, fmt(std::abs(v)));
    };
    const double realized = summary_.total_realized_pnl;
    const double dividends = summary_.total_dividends;
    const double total_ret = summary_.total_unrealized_pnl + realized + dividends;
    add_card(2, 0, tr("REALIZED P&L"), signed_money(realized),
             realized >= 0 ? ui::colors::POSITIVE : ui::colors::NEGATIVE);
    add_card(2, 1, tr("DIVIDENDS RECEIVED"), signed_money(dividends), ui::colors::CYAN);
    add_card(2, 2, tr("TOTAL RETURN (ALL-IN)"), signed_money(total_ret),
             total_ret >= 0 ? ui::colors::POSITIVE : ui::colors::NEGATIVE);
    add_card(2, 3, tr("TODAY"),
             QString("%1  (%2%3%)")
                 .arg(signed_money(summary_.total_day_change))
                 .arg(summary_.total_day_change_percent >= 0 ? "+" : "")
                 .arg(fmt(summary_.total_day_change_percent)),
             summary_.total_day_change >= 0 ? ui::colors::POSITIVE : ui::colors::NEGATIVE);

    layout->addLayout(grid);

    // Holdings breakdown
    auto* breakdown_title = new QLabel(tr("HOLDINGS BREAKDOWN"));
    breakdown_title->setStyleSheet(
        QString("color:%1; font-size:10px; font-weight:700; letter-spacing:1px;").arg(ui::colors::TEXT_SECONDARY()));
    layout->addWidget(breakdown_title);

    auto* breakdown = new QTableWidget;
    breakdown->setColumnCount(6);
    breakdown->setHorizontalHeaderLabels(
        {tr("SYMBOL"), tr("QTY"), tr("AVG COST"), tr("CURRENT"), tr("P&L"), tr("WEIGHT")});
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
                                 .arg(ui::colors::BG_BASE(), ui::colors::TEXT_PRIMARY(), ui::colors::BORDER_DIM(),
                                      ui::colors::BG_SURFACE(), ui::colors::TEXT_SECONDARY(), ui::colors::AMBER()));

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
        // Fractional lots (crypto, funds) and sub-unit prices lost their digits at 2 dp.
        set(3, QString::number(t.quantity, 'f', t.quantity == std::floor(t.quantity) ? 0 : 6));
        set(4, QString::number(t.price, 'f', std::abs(t.price) < 1.0 ? 6 : 2));
        set(5, QString("%1 %2").arg(currency_, QString::number(t.total_value, 'f', 2)));
        set(6, t.notes, ui::colors::TEXT_TERTIARY);
    }
}

void ReportsView::update_attribution() {
    // Contribution to return = the holding's P&L over the book's total cost, i.e. its
    // cost weight × its own return. The column then sums to the portfolio's unrealized
    // return. It used to be P&L ÷ |total P&L|: a share of the P&L, not of the return,
    // which ran to ±1000% whenever the book was near break-even.
    const double total_cost = summary_.total_cost_basis;
    const double book_ret = summary_.total_unrealized_pnl_percent;
    const double prev_nav = summary_.total_market_value - summary_.total_day_change;

    struct Row {
        const portfolio::HoldingWithQuote* h;
        double contribution; // percentage points of the book's return
        double day_contrib;  // % of yesterday's book value
    };
    QVector<Row> rows;
    rows.reserve(summary_.holdings.size());
    for (const auto& h : summary_.holdings) {
        const double c = total_cost > 0 ? h.unrealized_pnl / total_cost * 100.0 : 0.0;
        const double d = prev_nav > 0 ? h.day_change * h.quantity / prev_nav * 100.0 : 0.0;
        rows.append({&h, c, d});
    }
    // Largest contributors first, biggest detractors last — how attribution reads.
    std::sort(rows.begin(), rows.end(), [](const Row& a, const Row& b) { return a.contribution > b.contribution; });

    attr_table_->setRowCount(static_cast<int>(rows.size()) + (rows.isEmpty() ? 0 : 1));
    double sum_contrib = 0.0;
    double sum_day = 0.0;

    auto set = [&](int r, int col, const QString& text, const char* color = nullptr) {
        auto* item = new QTableWidgetItem(text);
        item->setTextAlignment(col == 0 ? (Qt::AlignLeft | Qt::AlignVCenter) : (Qt::AlignRight | Qt::AlignVCenter));
        if (color)
            item->setForeground(QColor(color));
        attr_table_->setItem(r, col, item);
    };
    auto signed_pct = [](double v, int dp) {
        return QString("%1%2%").arg(v >= 0 ? "+" : "").arg(QString::number(v, 'f', dp));
    };

    for (int r = 0; r < rows.size(); ++r) {
        const auto& h = *rows[r].h;
        const double contribution = rows[r].contribution;
        sum_contrib += contribution;
        sum_day += rows[r].day_contrib;
        attr_table_->setRowHeight(r, 28);

        const char* ret_color = h.unrealized_pnl_percent >= 0 ? ui::colors::POSITIVE : ui::colors::NEGATIVE;
        const char* contrib_color = contribution >= 0 ? ui::colors::POSITIVE : ui::colors::NEGATIVE;
        const char* day_color = rows[r].day_contrib >= 0 ? ui::colors::POSITIVE : ui::colors::NEGATIVE;
        // Judged against the book's own return, not against a fixed ±5%.
        const double rel = h.unrealized_pnl_percent - book_ret;
        const QString status = rel > 5.0 ? tr("OUTPERFORM") : rel < -5.0 ? tr("UNDERPERFORM") : tr("IN LINE");
        const char* status_color = rel > 5.0    ? ui::colors::POSITIVE
                                   : rel < -5.0 ? ui::colors::NEGATIVE
                                                : ui::colors::TEXT_TERTIARY;

        set(r, 0, h.symbol, ui::colors::CYAN);
        set(r, 1, QString("%1%").arg(QString::number(h.weight, 'f', 1)));
        set(r, 2, signed_pct(h.unrealized_pnl_percent, 2), ret_color);
        set(r, 3, QString("%1 pp").arg(signed_pct(contribution, 2).chopped(1)), contrib_color);
        set(r, 4, signed_pct(rows[r].day_contrib, 2), day_color);
        set(r, 5, QString("%1 %2").arg(currency_, QString::number(h.unrealized_pnl, 'f', 2)), ret_color);
        set(r, 6, QString("%1 (%2 pp)").arg(status, signed_pct(rel, 1).chopped(1)), status_color);
    }

    if (!rows.isEmpty()) {
        // Totals row: the contributions add up to the book's return.
        const int r = static_cast<int>(rows.size());
        attr_table_->setRowHeight(r, 28);
        set(r, 0, tr("TOTAL"), ui::colors::AMBER);
        set(r, 1, QStringLiteral("100.0%"), ui::colors::AMBER);
        set(r, 2, signed_pct(book_ret, 2), book_ret >= 0 ? ui::colors::POSITIVE : ui::colors::NEGATIVE);
        set(r, 3, QString("%1 pp").arg(signed_pct(sum_contrib, 2).chopped(1)),
            sum_contrib >= 0 ? ui::colors::POSITIVE : ui::colors::NEGATIVE);
        set(r, 4, signed_pct(sum_day, 2), sum_day >= 0 ? ui::colors::POSITIVE : ui::colors::NEGATIVE);
        set(r, 5, QString("%1 %2").arg(currency_, QString::number(summary_.total_unrealized_pnl, 'f', 2)),
            summary_.total_unrealized_pnl >= 0 ? ui::colors::POSITIVE : ui::colors::NEGATIVE);
        set(r, 6, QString(), ui::colors::TEXT_TERTIARY);
    }
}

} // namespace fincept::screens
