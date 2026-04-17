// src/screens/portfolio/views/QuantStatsView.cpp
#include "screens/portfolio/views/QuantStatsView.h"

#include "core/logging/Logger.h"
#include "python/PythonRunner.h"
#include "ui/theme/Theme.h"

#include <QBarSeries>
#include <QBarSet>
#include <QChart>
#include <QChartView>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLineSeries>
#include <QPointer>
#include <QScrollArea>
#include <QStackedWidget>
#include <QVBoxLayout>
#include <QValueAxis>

#include <algorithm>
#include <cmath>

using fincept::python::PythonResult;
using fincept::python::PythonRunner;

namespace fincept::screens {

// ── Helpers ───────────────────────────────────────────────────────────────────

static QString pct_str(double v, int dp = 2) {
    return QString("%1%2%").arg(v >= 0 ? "+" : "").arg(QString::number(v, 'f', dp));
}

static QString ratio_str(double v, int dp = 2) {
    return QString::number(v, 'f', dp);
}

static QTableWidgetItem* make_item(const QString& text, Qt::Alignment align, const QColor& color) {
    auto* item = new QTableWidgetItem(text);
    item->setTextAlignment(align);
    item->setForeground(color);
    return item;
}

// Apply standard dark styling to a QTableWidget.
static void style_table(QTableWidget* t) {
    t->setSelectionMode(QAbstractItemView::NoSelection);
    t->setEditTriggers(QAbstractItemView::NoEditTriggers);
    t->setShowGrid(false);
    t->verticalHeader()->setVisible(false);
    t->horizontalHeader()->setStretchLastSection(true);
    t->setStyleSheet(QString("QTableWidget { background:%1; color:%2; border:none; font-size:11px; }"
                             "QTableWidget::item { padding:4px 8px; border-bottom:1px solid %3; }"
                             "QHeaderView::section { background:%4; color:%5; border:none;"
                             "  border-bottom:2px solid %6; padding:4px 8px; font-size:9px;"
                             "  font-weight:700; letter-spacing:0.5px; }")
                         .arg(ui::colors::BG_BASE(), ui::colors::TEXT_PRIMARY(), ui::colors::BORDER_DIM(),
                              ui::colors::BG_SURFACE(), ui::colors::TEXT_SECONDARY(), ui::colors::AMBER()));
}

// Build a centred placeholder label inside a parent widget.
static QWidget* make_placeholder(const QString& msg, QWidget* parent = nullptr) {
    auto* w = new QWidget(parent);
    auto* lay = new QVBoxLayout(w);
    lay->setContentsMargins(24, 32, 24, 32);
    auto* lbl = new QLabel(msg);
    lbl->setAlignment(Qt::AlignCenter);
    lbl->setWordWrap(true);
    lbl->setStyleSheet(QString("color:%1; font-size:11px;").arg(ui::colors::TEXT_TERTIARY()));
    lay->addStretch();
    lay->addWidget(lbl);
    lay->addStretch();
    return w;
}

// ── Constructor ───────────────────────────────────────────────────────────────

QuantStatsView::QuantStatsView(QWidget* parent) : QWidget(parent) {
    build_ui();
}

// ── build_ui ─────────────────────────────────────────────────────────────────

void QuantStatsView::build_ui() {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // ── Header bar ────────────────────────────────────────────────────────────
    auto* header = new QWidget(this);
    header->setFixedHeight(36);
    header->setStyleSheet(
        QString("background:%1; border-bottom:1px solid %2;").arg(ui::colors::BG_SURFACE(), ui::colors::AMBER()));

    auto* h_lay = new QHBoxLayout(header);
    h_lay->setContentsMargins(12, 0, 12, 0);
    h_lay->setSpacing(8);

    auto* title_lbl = new QLabel("QUANTSTATS ANALYSIS");
    title_lbl->setStyleSheet(
        QString("color:%1; font-size:11px; font-weight:700; letter-spacing:1px;").arg(ui::colors::AMBER()));
    h_lay->addWidget(title_lbl);
    h_lay->addStretch();

    qs_status_ = new QLabel;
    qs_status_->setStyleSheet(QString("color:%1; font-size:9px;").arg(ui::colors::TEXT_TERTIARY()));
    h_lay->addWidget(qs_status_);

    qs_run_btn_ = new QPushButton("\u25B6 RUN QUANTSTATS");
    qs_run_btn_->setFixedHeight(22);
    qs_run_btn_->setCursor(Qt::PointingHandCursor);
    qs_run_btn_->setStyleSheet(
        QString("QPushButton { background:%1; color:#000; border:none;"
                "  padding:0 12px; font-size:9px; font-weight:700; }"
                "QPushButton:hover { background:%2; }"
                "QPushButton:disabled { background:%3; color:%4; }")
            .arg(ui::colors::AMBER(), ui::colors::WARNING(), ui::colors::BG_RAISED(), ui::colors::TEXT_TERTIARY()));
    connect(qs_run_btn_, &QPushButton::clicked, this, &QuantStatsView::run_quantstats);
    h_lay->addWidget(qs_run_btn_);

    root->addWidget(header);

    // ── Tab widget ────────────────────────────────────────────────────────────
    tabs_ = new QTabWidget;
    tabs_->setDocumentMode(true);
    tabs_->setStyleSheet(QString("QTabWidget::pane { border:0; background:%1; }"
                                 "QTabBar::tab { background:%2; color:%3; padding:6px 14px; border:0;"
                                 "  border-bottom:2px solid transparent; font-size:9px; font-weight:700;"
                                 "  letter-spacing:0.5px; }"
                                 "QTabBar::tab:selected { color:%4; border-bottom:2px solid %4; }"
                                 "QTabBar::tab:hover { color:%5; }")
                             .arg(ui::colors::BG_BASE(), ui::colors::BG_SURFACE(), ui::colors::TEXT_SECONDARY(),
                                  ui::colors::AMBER(), ui::colors::TEXT_PRIMARY()));

    // ── METRICS tab ───────────────────────────────────────────────────────────
    {
        auto* metrics_w = new QWidget(this);
        auto* ml = new QVBoxLayout(metrics_w);
        ml->setContentsMargins(12, 8, 12, 8);
        ml->setSpacing(4);

        auto* section_lbl = new QLabel("KEY PERFORMANCE INDICATORS");
        section_lbl->setStyleSheet(
            QString("color:%1; font-size:11px; font-weight:700; letter-spacing:1px;").arg(ui::colors::AMBER()));
        ml->addWidget(section_lbl);

        metrics_table_ = new QTableWidget;
        metrics_table_->setColumnCount(3);
        metrics_table_->setHorizontalHeaderLabels({"METRIC", "VALUE", "BENCHMARK"});
        metrics_table_->setColumnWidth(0, 220);
        metrics_table_->setColumnWidth(1, 130);
        style_table(metrics_table_);
        ml->addWidget(metrics_table_, 1);

        tabs_->addTab(metrics_w, "METRICS");
    }

    // ── RETURNS tab ───────────────────────────────────────────────────────────
    {
        auto* returns_w = new QWidget(this);
        returns_w->setStyleSheet(QString("background:%1;").arg(ui::colors::BG_BASE()));
        auto* rl = new QVBoxLayout(returns_w);
        rl->setContentsMargins(0, 0, 0, 0);
        rl->setSpacing(0);

        returns_stack_ = new QStackedWidget;
        returns_stack_->addWidget(make_placeholder("Run QuantStats Analysis for return distribution"));
        // Index 1 is added lazily in update_returns()
        rl->addWidget(returns_stack_);

        tabs_->addTab(returns_w, "RETURNS");
    }

    // ── DRAWDOWN tab ──────────────────────────────────────────────────────────
    {
        auto* dd_w = new QWidget(this);
        dd_w->setStyleSheet(QString("background:%1;").arg(ui::colors::BG_BASE()));
        auto* dl = new QVBoxLayout(dd_w);
        dl->setContentsMargins(0, 0, 0, 0);
        dl->setSpacing(0);

        drawdown_stack_ = new QStackedWidget;
        drawdown_stack_->addWidget(make_placeholder("Run QuantStats Analysis for drawdown metrics"));
        dl->addWidget(drawdown_stack_);

        tabs_->addTab(dd_w, "DRAWDOWN");
    }

    // ── ROLLING tab ───────────────────────────────────────────────────────────
    {
        auto* roll_w = new QWidget(this);
        roll_w->setStyleSheet(QString("background:%1;").arg(ui::colors::BG_BASE()));
        auto* rll = new QVBoxLayout(roll_w);
        rll->setContentsMargins(0, 0, 0, 0);
        rll->setSpacing(0);

        rolling_stack_ = new QStackedWidget;
        rolling_stack_->addWidget(make_placeholder("Run QuantStats Analysis for rolling metrics"));
        rll->addWidget(rolling_stack_);

        tabs_->addTab(roll_w, "ROLLING");
    }

    // ── MONTE CARLO tab ───────────────────────────────────────────────────────
    {
        auto* mc_w = new QWidget(this);
        mc_w->setStyleSheet(QString("background:%1;").arg(ui::colors::BG_BASE()));
        auto* mcl = new QVBoxLayout(mc_w);
        mcl->setContentsMargins(16, 12, 16, 12);
        mcl->setSpacing(8);

        auto* mc_title = new QLabel("MONTE CARLO SIMULATION");
        mc_title->setStyleSheet(
            QString("color:%1; font-size:12px; font-weight:700; letter-spacing:1px;").arg(ui::colors::AMBER()));
        mcl->addWidget(mc_title);

        auto* mc_desc = new QLabel("Simulate 1,000 portfolio return paths using GBM to estimate probability\n"
                                   "distributions of future returns, drawdowns, and terminal wealth.");
        mc_desc->setWordWrap(true);
        mc_desc->setStyleSheet(QString("color:%1; font-size:11px;").arg(ui::colors::TEXT_TERTIARY()));
        mcl->addWidget(mc_desc);

        // Button row
        auto* mc_btn_row = new QHBoxLayout;
        mc_btn_row->setSpacing(8);

        mc_run_btn_ = new QPushButton("\u25B6 RUN MONTE CARLO (1000 paths)");
        mc_run_btn_->setFixedHeight(28);
        mc_run_btn_->setCursor(Qt::PointingHandCursor);
        mc_run_btn_->setStyleSheet(
            QString("QPushButton { background:%1; color:#000; border:none;"
                    "  padding:0 16px; font-size:10px; font-weight:700; }"
                    "QPushButton:hover { background:%2; }"
                    "QPushButton:disabled { background:%3; color:%4; }")
                .arg(ui::colors::AMBER(), ui::colors::WARNING(), ui::colors::BG_RAISED(), ui::colors::TEXT_TERTIARY()));
        connect(mc_run_btn_, &QPushButton::clicked, this, &QuantStatsView::run_monte_carlo);
        mc_btn_row->addWidget(mc_run_btn_);

        mc_status_ = new QLabel;
        mc_status_->setStyleSheet(QString("color:%1; font-size:10px;").arg(ui::colors::TEXT_TERTIARY()));
        mc_btn_row->addWidget(mc_status_);
        mc_btn_row->addStretch();
        mcl->addLayout(mc_btn_row);

        // Stacked area: placeholder vs live content
        mc_stack_ = new QStackedWidget;
        mc_stack_->addWidget(make_placeholder("Press RUN MONTE CARLO to simulate 1,000 return paths"));
        // Index 1 is built lazily in update_monte_carlo_chart()
        mcl->addWidget(mc_stack_, 1);

        // mc_results_ kept for backward compatibility (not used in display logic)
        mc_results_ = new QWidget(this);
        mc_results_->setVisible(false);

        tabs_->addTab(mc_w, "MONTE CARLO");
    }

    root->addWidget(tabs_, 1);
}

// ── set_data ──────────────────────────────────────────────────────────────────

void QuantStatsView::set_data(const portfolio::PortfolioSummary& summary, const QString& currency) {
    summary_ = summary;
    currency_ = currency;

    // Populate metrics immediately from live summary data
    update_metrics();

    // Kick off Python fetch automatically
    run_quantstats();
}

// ── update_metrics ────────────────────────────────────────────────────────────

void QuantStatsView::update_metrics() {
    metrics_table_->clearContents();

    // ── Build row definitions ─────────────────────────────────────────────────
    struct Row {
        QString section; // non-empty marks a section separator row
        QString name;
        QString value;
        QString benchmark = "--";
        bool is_positive = true; // drives colour of value cell
        bool is_section = false;
    };

    QVector<Row> rows;

    auto add_section = [&](const QString& label) { rows.push_back({label, {}, {}, {}, false, true}); };

    const bool have_qs = !qs_data_.isEmpty();

    // Helper lambdas for reading qs_data_
    auto qs_pct = [&](const QString& group, const QString& key) -> double {
        if (!have_qs)
            return 0.0;
        return qs_data_[group].toObject()[key].toDouble() * 100.0;
    };
    auto qs_val = [&](const QString& group, const QString& key) -> double {
        if (!have_qs)
            return 0.0;
        return qs_data_[group].toObject()[key].toDouble();
    };
    auto qs_int = [&](const QString& group, const QString& key) -> int {
        if (!have_qs)
            return 0;
        return qs_data_[group].toObject()[key].toInt();
    };

    // ── PERFORMANCE ──────────────────────────────────────────────────────────
    add_section("PERFORMANCE");

    if (have_qs) {
        double total_ret = qs_pct("performance", "total_return");
        double ann_ret = qs_pct("performance", "annualized_return");
        int t_days = qs_int("performance", "trading_days");
        double best_day = qs_pct("performance", "best_day");
        double worst_day = qs_pct("performance", "worst_day");
        double avg_daily = qs_pct("performance", "avg_daily_return");

        rows.push_back({"", "Total Return", pct_str(total_ret), "--", total_ret >= 0});
        rows.push_back({"", "Annualized Return", pct_str(ann_ret), "--", ann_ret >= 0});
        rows.push_back({"", "Trading Days", QString::number(t_days), "--", true});
        rows.push_back({"", "Best Day", pct_str(best_day), "--", true});
        rows.push_back({"", "Worst Day", pct_str(worst_day), "--", worst_day >= 0});
        rows.push_back({"", "Avg Daily Return", pct_str(avg_daily, 4), "--", avg_daily >= 0});
    } else {
        // Pre-run: show live summary values
        double pnl_pct = summary_.total_unrealized_pnl_percent;
        double day_pct = summary_.total_day_change_percent;
        rows.push_back({"", "Unrealized P&L %", pct_str(pnl_pct), "--", pnl_pct >= 0});
        rows.push_back({"", "Day Change %", pct_str(day_pct), "--", day_pct >= 0});
        rows.push_back({"", "Total Positions", QString::number(summary_.total_positions), "--", true});
        rows.push_back({"", "Gainers", QString::number(summary_.gainers), "--", true});
        rows.push_back({"", "Losers", QString::number(summary_.losers), "--", summary_.losers == 0});
    }

    // ── RISK ─────────────────────────────────────────────────────────────────
    add_section("RISK");

    if (have_qs) {
        double ann_vol = qs_pct("risk", "annualized_volatility");
        double max_dd = qs_pct("risk", "max_drawdown");
        double var95 = qs_pct("risk", "var_95_daily");
        double cvar95 = qs_pct("risk", "cvar_95_daily");
        double down_dev = qs_pct("risk", "downside_deviation");

        rows.push_back({"", "Annualized Volatility", pct_str(ann_vol), "--", ann_vol < 20.0});
        rows.push_back({"", "Max Drawdown", pct_str(max_dd), "--", max_dd >= 0});
        rows.push_back({"", "VaR 95% (Daily)", pct_str(var95), "--", var95 >= 0});
        rows.push_back({"", "CVaR 95% (Daily)", pct_str(cvar95), "--", cvar95 >= 0});
        rows.push_back({"", "Downside Deviation", pct_str(down_dev), "--", down_dev < 15.0});
    } else {
        rows.push_back({"", "Annualized Volatility", "--", "--", true});
        rows.push_back({"", "Max Drawdown", "--", "--", true});
        rows.push_back({"", "VaR 95% (Daily)", "--", "--", true});
        rows.push_back({"", "CVaR 95% (Daily)", "--", "--", true});
        rows.push_back({"", "Downside Deviation", "--", "--", true});
    }

    // ── RATIOS ────────────────────────────────────────────────────────────────
    add_section("RATIOS");

    if (have_qs) {
        double sharpe = qs_val("ratios", "sharpe_ratio");
        double sortino = qs_val("ratios", "sortino_ratio");
        double calmar = qs_val("ratios", "calmar_ratio");
        double pf = qs_val("ratios", "profit_factor");

        rows.push_back({"", "Sharpe Ratio", ratio_str(sharpe), "--", sharpe >= 0});
        rows.push_back({"", "Sortino Ratio", ratio_str(sortino), "--", sortino >= 0});
        rows.push_back({"", "Calmar Ratio", ratio_str(calmar), "--", calmar >= 0});
        rows.push_back({"", "Profit Factor", ratio_str(pf), "--", pf >= 1.0});
    } else {
        rows.push_back({"", "Sharpe Ratio", "--", "--", true});
        rows.push_back({"", "Sortino Ratio", "--", "--", true});
        rows.push_back({"", "Calmar Ratio", "--", "--", true});
        rows.push_back({"", "Profit Factor", "--", "--", true});
    }

    // ── DISTRIBUTION ─────────────────────────────────────────────────────────
    add_section("DISTRIBUTION");

    if (have_qs) {
        double skew = qs_val("distribution", "skewness");
        double kurt = qs_val("distribution", "kurtosis");
        double win_rate = qs_pct("distribution", "win_rate");
        int win_days = qs_int("distribution", "win_days");
        int los_days = qs_int("distribution", "loss_days");
        double avg_win = qs_pct("distribution", "avg_win");
        double avg_loss = qs_pct("distribution", "avg_loss");

        rows.push_back({"", "Skewness", ratio_str(skew, 3), "--", skew >= 0});
        rows.push_back({"", "Kurtosis", ratio_str(kurt, 3), "--", kurt <= 3.5});
        rows.push_back({"", "Win Rate", pct_str(win_rate), "--", win_rate >= 50.0});
        rows.push_back({"", "Win Days", QString::number(win_days), "--", true});
        rows.push_back({"", "Loss Days", QString::number(los_days), "--", los_days == 0});
        rows.push_back({"", "Avg Win", pct_str(avg_win), "--", true});
        rows.push_back({"", "Avg Loss", pct_str(avg_loss), "--", avg_loss >= 0});
    } else {
        rows.push_back({"", "Skewness", "--", "--", true});
        rows.push_back({"", "Kurtosis", "--", "--", true});
        rows.push_back({"", "Win Rate", "--", "--", true});
        rows.push_back({"", "Win Days", "--", "--", true});
        rows.push_back({"", "Loss Days", "--", "--", true});
        rows.push_back({"", "Avg Win", "--", "--", true});
        rows.push_back({"", "Avg Loss", "--", "--", true});

        // Prompt row when pre-run
        rows.push_back({"", "Run QuantStats for full metrics \u2192", "", "--", true});
    }

    // ── Populate table ────────────────────────────────────────────────────────
    metrics_table_->setRowCount(rows.size());

    for (int r = 0; r < rows.size(); ++r) {
        metrics_table_->setRowHeight(r, 26);
        const auto& row = rows[r];

        if (row.is_section) {
            // Section separator spanning all columns
            auto* item = new QTableWidgetItem(row.section);
            item->setTextAlignment(Qt::AlignLeft | Qt::AlignVCenter);
            item->setForeground(QColor(ui::colors::AMBER()));
            item->setBackground(QColor(ui::colors::BG_RAISED()));
            QFont f = item->font();
            f.setPointSize(8);
            f.setBold(true);
            item->setFont(f);
            metrics_table_->setItem(r, 0, item);
            metrics_table_->setSpan(r, 0, 1, 3);
            continue;
        }

        metrics_table_->setItem(
            r, 0, make_item(row.name, Qt::AlignLeft | Qt::AlignVCenter, QColor(ui::colors::TEXT_SECONDARY())));

        const char* val_color = row.value.isEmpty() ? ui::colors::TEXT_TERTIARY
                                : row.is_positive   ? ui::colors::POSITIVE
                                                    : ui::colors::NEGATIVE();
        // Special: neutral colour for counters and "--"
        if (row.value == "--" || row.name.contains("Days") || row.name.contains("Positions") ||
            row.name == "Trading Days") {
            val_color = ui::colors::TEXT_PRIMARY();
        }

        metrics_table_->setItem(
            r, 1,
            make_item(row.value.isEmpty() ? "--" : row.value, Qt::AlignRight | Qt::AlignVCenter, QColor(val_color)));

        metrics_table_->setItem(
            r, 2, make_item(row.benchmark, Qt::AlignRight | Qt::AlignVCenter, QColor(ui::colors::TEXT_TERTIARY())));
    }
}

// ── update_returns ────────────────────────────────────────────────────────────

void QuantStatsView::update_returns() {
    if (qs_data_.isEmpty())
        return;

    // Remove any previously injected widget at index 1
    if (returns_stack_->count() > 1) {
        QWidget* old = returns_stack_->widget(1);
        returns_stack_->removeWidget(old);
        old->deleteLater();
    }

    auto* content = new QWidget(this);
    content->setStyleSheet(QString("background:%1;").arg(ui::colors::BG_BASE()));
    auto* cl = new QVBoxLayout(content);
    cl->setContentsMargins(16, 12, 16, 12);
    cl->setSpacing(12);

    auto* title = new QLabel("RETURN DISTRIBUTION");
    title->setStyleSheet(
        QString("color:%1; font-size:11px; font-weight:700; letter-spacing:1px;").arg(ui::colors::AMBER()));
    cl->addWidget(title);

    // Win/loss summary cards — 2-column grid
    const QJsonObject dist = qs_data_["distribution"].toObject();
    double win_rate = dist["win_rate"].toDouble() * 100.0;
    int win_days = dist["win_days"].toInt();
    int loss_days = dist["loss_days"].toInt();
    double avg_win = dist["avg_win"].toDouble() * 100.0;
    double avg_loss = dist["avg_loss"].toDouble() * 100.0;
    double skew = dist["skewness"].toDouble();
    double kurt = dist["kurtosis"].toDouble();

    struct Card {
        QString label;
        QString value;
        const char* color;
    };
    QVector<Card> cards = {
        {"WIN RATE", pct_str(win_rate), ui::colors::POSITIVE()},
        {"LOSS RATE", pct_str(100.0 - win_rate), ui::colors::NEGATIVE()},
        {"WIN DAYS", QString::number(win_days), ui::colors::POSITIVE()},
        {"LOSS DAYS", QString::number(loss_days), ui::colors::NEGATIVE()},
        {"AVG WIN", pct_str(avg_win), ui::colors::POSITIVE()},
        {"AVG LOSS", pct_str(avg_loss), ui::colors::NEGATIVE()},
        {"SKEWNESS", ratio_str(skew, 3), ui::colors::TEXT_PRIMARY()},
        {"KURTOSIS", ratio_str(kurt, 3), ui::colors::TEXT_PRIMARY()},
    };

    auto* grid_w = new QWidget(this);
    auto* grid = new QGridLayout(grid_w);
    grid->setContentsMargins(0, 0, 0, 0);
    grid->setSpacing(8);

    for (int i = 0; i < cards.size(); ++i) {
        const auto& c = cards[i];
        int row_i = i / 2;
        int col_i = i % 2;

        auto* card_w = new QWidget(this);
        card_w->setStyleSheet(
            QString("background:%1; border:1px solid %2;").arg(ui::colors::BG_RAISED(), ui::colors::BORDER_MED()));
        auto* card_l = new QVBoxLayout(card_w);
        card_l->setContentsMargins(12, 10, 12, 10);
        card_l->setSpacing(4);

        auto* lbl = new QLabel(c.label);
        lbl->setStyleSheet(
            QString("color:%1; font-size:9px; font-weight:700; letter-spacing:0.5px;").arg(ui::colors::TEXT_TERTIARY()));
        card_l->addWidget(lbl);

        auto* val_lbl = new QLabel(c.value);
        val_lbl->setStyleSheet(QString("color:%1; font-size:18px; font-weight:700;").arg(c.color));
        card_l->addWidget(val_lbl);

        grid->addWidget(card_w, row_i, col_i);
    }

    cl->addWidget(grid_w);

    // Win/Loss bar chart (simple table-based visual)
    auto* dist_table = new QTableWidget;
    dist_table->setColumnCount(2);
    dist_table->setHorizontalHeaderLabels({"METRIC", "VALUE"});
    dist_table->setColumnWidth(0, 200);
    style_table(dist_table);

    const QJsonObject perf = qs_data_["performance"].toObject();
    double best_day = perf["best_day"].toDouble() * 100.0;
    double worst_day = perf["worst_day"].toDouble() * 100.0;
    double avg_daily = perf["avg_daily_return"].toDouble() * 100.0;

    struct TableRow {
        QString name;
        QString val;
        bool positive;
    };
    QVector<TableRow> trows = {
        {"Best Day", pct_str(best_day), true},
        {"Worst Day", pct_str(worst_day), worst_day >= 0},
        {"Avg Daily Return", pct_str(avg_daily, 4), avg_daily >= 0},
        {"Profit Factor", ratio_str(qs_data_["ratios"].toObject()["profit_factor"].toDouble()),
         qs_data_["ratios"].toObject()["profit_factor"].toDouble() >= 1.0},
    };
    dist_table->setRowCount(trows.size());
    for (int i = 0; i < trows.size(); ++i) {
        dist_table->setRowHeight(i, 26);
        dist_table->setItem(
            i, 0, make_item(trows[i].name, Qt::AlignLeft | Qt::AlignVCenter, QColor(ui::colors::TEXT_SECONDARY())));
        dist_table->setItem(i, 1,
                            make_item(trows[i].val, Qt::AlignRight | Qt::AlignVCenter,
                                      QColor(trows[i].positive ? ui::colors::POSITIVE() : ui::colors::NEGATIVE())));
    }
    cl->addWidget(dist_table);
    cl->addStretch();

    returns_stack_->addWidget(content);
    returns_stack_->setCurrentIndex(1);
}

// ── update_drawdown ───────────────────────────────────────────────────────────

void QuantStatsView::update_drawdown() {
    if (qs_data_.isEmpty())
        return;

    if (drawdown_stack_->count() > 1) {
        QWidget* old = drawdown_stack_->widget(1);
        drawdown_stack_->removeWidget(old);
        old->deleteLater();
    }

    auto* content = new QWidget(this);
    content->setStyleSheet(QString("background:%1;").arg(ui::colors::BG_BASE()));
    auto* cl = new QVBoxLayout(content);
    cl->setContentsMargins(16, 12, 16, 12);
    cl->setSpacing(12);

    auto* title = new QLabel("DRAWDOWN & RISK METRICS");
    title->setStyleSheet(
        QString("color:%1; font-size:11px; font-weight:700; letter-spacing:1px;").arg(ui::colors::AMBER()));
    cl->addWidget(title);

    // Prominent Max Drawdown display
    const QJsonObject risk = qs_data_["risk"].toObject();
    double max_dd = risk["max_drawdown"].toDouble() * 100.0;

    auto* hero_w = new QWidget(this);
    hero_w->setStyleSheet(
        QString("background:%1; border:1px solid %2;").arg(ui::colors::BG_RAISED(), ui::colors::NEGATIVE()));
    auto* hero_l = new QVBoxLayout(hero_w);
    hero_l->setContentsMargins(16, 12, 16, 12);
    hero_l->setSpacing(2);

    auto* hero_label = new QLabel("MAX DRAWDOWN");
    hero_label->setStyleSheet(
        QString("color:%1; font-size:9px; font-weight:700; letter-spacing:1px;").arg(ui::colors::TEXT_TERTIARY()));
    hero_l->addWidget(hero_label);

    auto* hero_value = new QLabel(pct_str(max_dd));
    hero_value->setStyleSheet(QString("color:%1; font-size:28px; font-weight:700;")
                                  .arg(max_dd < 0 ? ui::colors::NEGATIVE() : ui::colors::POSITIVE()));
    hero_l->addWidget(hero_value);
    cl->addWidget(hero_w);

    // Drawdown metrics table
    auto* dd_table = new QTableWidget;
    dd_table->setColumnCount(2);
    dd_table->setHorizontalHeaderLabels({"RISK METRIC", "VALUE"});
    dd_table->setColumnWidth(0, 220);
    style_table(dd_table);

    struct DdRow {
        QString name;
        QString val;
        bool positive;
    };
    double var95 = risk["var_95_daily"].toDouble() * 100.0;
    double cvar95 = risk["cvar_95_daily"].toDouble() * 100.0;
    double ann_vol = risk["annualized_volatility"].toDouble() * 100.0;
    double down_dev = risk["downside_deviation"].toDouble() * 100.0;

    QVector<DdRow> drows = {
        {"Max Drawdown", pct_str(max_dd), max_dd >= 0},
        {"Annualized Volatility", pct_str(ann_vol), ann_vol < 20.0},
        {"VaR 95% (Daily)", pct_str(var95), var95 >= 0},
        {"CVaR 95% (Daily)", pct_str(cvar95), cvar95 >= 0},
        {"Downside Deviation", pct_str(down_dev), down_dev < 15.0},
    };
    dd_table->setRowCount(drows.size());
    for (int i = 0; i < drows.size(); ++i) {
        dd_table->setRowHeight(i, 26);
        dd_table->setItem(
            i, 0, make_item(drows[i].name, Qt::AlignLeft | Qt::AlignVCenter, QColor(ui::colors::TEXT_SECONDARY())));
        dd_table->setItem(i, 1,
                          make_item(drows[i].val, Qt::AlignRight | Qt::AlignVCenter,
                                    QColor(drows[i].positive ? ui::colors::POSITIVE : ui::colors::NEGATIVE())));
    }
    cl->addWidget(dd_table);
    cl->addStretch();

    drawdown_stack_->addWidget(content);
    drawdown_stack_->setCurrentIndex(1);
}

// ── update_rolling ────────────────────────────────────────────────────────────

void QuantStatsView::update_rolling() {
    if (qs_data_.isEmpty())
        return;

    if (rolling_stack_->count() > 1) {
        QWidget* old = rolling_stack_->widget(1);
        rolling_stack_->removeWidget(old);
        old->deleteLater();
    }

    auto* content = new QWidget(this);
    content->setStyleSheet(QString("background:%1;").arg(ui::colors::BG_BASE()));
    auto* cl = new QVBoxLayout(content);
    cl->setContentsMargins(16, 12, 16, 12);
    cl->setSpacing(12);

    auto* title = new QLabel("RISK-ADJUSTED RATIOS & WIN/LOSS BREAKDOWN");
    title->setStyleSheet(
        QString("color:%1; font-size:11px; font-weight:700; letter-spacing:1px;").arg(ui::colors::AMBER()));
    cl->addWidget(title);

    // Ratios table
    auto* ratios_table = new QTableWidget;
    ratios_table->setColumnCount(2);
    ratios_table->setHorizontalHeaderLabels({"RATIO", "VALUE"});
    ratios_table->setColumnWidth(0, 220);
    style_table(ratios_table);

    const QJsonObject ratios = qs_data_["ratios"].toObject();
    double sharpe = ratios["sharpe_ratio"].toDouble();
    double sortino = ratios["sortino_ratio"].toDouble();
    double calmar = ratios["calmar_ratio"].toDouble();
    double pf = ratios["profit_factor"].toDouble();

    struct RatioRow {
        QString name;
        QString val;
        bool positive;
    };
    QVector<RatioRow> rrows = {
        {"Sharpe Ratio", ratio_str(sharpe), sharpe >= 0},
        {"Sortino Ratio", ratio_str(sortino), sortino >= 0},
        {"Calmar Ratio", ratio_str(calmar), calmar >= 0},
        {"Profit Factor", ratio_str(pf), pf >= 1.0},
    };
    ratios_table->setRowCount(rrows.size());
    for (int i = 0; i < rrows.size(); ++i) {
        ratios_table->setRowHeight(i, 26);
        ratios_table->setItem(
            i, 0, make_item(rrows[i].name, Qt::AlignLeft | Qt::AlignVCenter, QColor(ui::colors::TEXT_SECONDARY())));
        ratios_table->setItem(i, 1,
                              make_item(rrows[i].val, Qt::AlignRight | Qt::AlignVCenter,
                                        QColor(rrows[i].positive ? ui::colors::POSITIVE : ui::colors::NEGATIVE())));
    }
    cl->addWidget(ratios_table);

    // Win/Loss breakdown
    auto* wl_title = new QLabel("WIN / LOSS BREAKDOWN");
    wl_title->setStyleSheet(QString("color:%1; font-size:10px; font-weight:700; letter-spacing:0.5px;"
                                    " margin-top:8px;")
                                .arg(ui::colors::TEXT_SECONDARY()));
    cl->addWidget(wl_title);

    const QJsonObject dist = qs_data_["distribution"].toObject();
    int win_days = dist["win_days"].toInt();
    int loss_days = dist["loss_days"].toInt();
    double win_rate = dist["win_rate"].toDouble() * 100.0;
    double avg_win = dist["avg_win"].toDouble() * 100.0;
    double avg_loss = dist["avg_loss"].toDouble() * 100.0;

    auto* wl_table = new QTableWidget;
    wl_table->setColumnCount(2);
    wl_table->setHorizontalHeaderLabels({"METRIC", "VALUE"});
    wl_table->setColumnWidth(0, 220);
    style_table(wl_table);

    QVector<RatioRow> wrows = {
        {"Win Rate", pct_str(win_rate), win_rate >= 50.0},         {"Win Days", QString::number(win_days), true},
        {"Loss Days", QString::number(loss_days), loss_days == 0}, {"Avg Win/Day", pct_str(avg_win), true},
        {"Avg Loss/Day", pct_str(avg_loss), avg_loss >= 0},
    };
    wl_table->setRowCount(wrows.size());
    for (int i = 0; i < wrows.size(); ++i) {
        wl_table->setRowHeight(i, 26);
        wl_table->setItem(
            i, 0, make_item(wrows[i].name, Qt::AlignLeft | Qt::AlignVCenter, QColor(ui::colors::TEXT_SECONDARY())));
        const char* vc = (wrows[i].name.contains("Days") || wrows[i].name == "Win Days")
                             ? ui::colors::TEXT_PRIMARY
                             : (wrows[i].positive ? ui::colors::POSITIVE() : ui::colors::NEGATIVE);
        wl_table->setItem(i, 1, make_item(wrows[i].val, Qt::AlignRight | Qt::AlignVCenter, QColor(vc)));
    }
    cl->addWidget(wl_table);
    cl->addStretch();

    rolling_stack_->addWidget(content);
    rolling_stack_->setCurrentIndex(1);
}

// ── update_monte_carlo_chart ──────────────────────────────────────────────────

void QuantStatsView::update_monte_carlo_chart() {
    if (mc_data_.isEmpty())
        return;

    // Remove previously built live widget if any
    if (mc_stack_->count() > 1) {
        QWidget* old = mc_stack_->widget(1);
        mc_stack_->removeWidget(old);
        old->deleteLater();
        mc_chart_ = nullptr;
    }

    auto* content = new QWidget(this);
    content->setStyleSheet(QString("background:%1;").arg(ui::colors::BG_BASE()));
    auto* cl = new QVBoxLayout(content);
    cl->setContentsMargins(0, 0, 0, 0);
    cl->setSpacing(0);

    // ── Summary stats strip ───────────────────────────────────────────────────
    auto* stats_bar = new QWidget(this);
    stats_bar->setFixedHeight(56);
    stats_bar->setStyleSheet(
        QString("background:%1; border-bottom:1px solid %2;").arg(ui::colors::BG_RAISED(), ui::colors::BORDER_MED()));
    auto* sb_lay = new QHBoxLayout(stats_bar);
    sb_lay->setContentsMargins(16, 4, 16, 4);
    sb_lay->setSpacing(24);

    double median_ret = mc_data_["median_return"].toDouble();
    double pct5 = mc_data_["percentile_5"].toDouble();
    double pct95 = mc_data_["percentile_95"].toDouble();
    double prob_loss = mc_data_["prob_loss"].toDouble();
    double exp_max_dd = mc_data_["expected_max_dd"].toDouble();

    struct StatItem {
        QString label;
        QString value;
        const char* color;
    };
    QVector<StatItem> stats = {
        {"MEDIAN RETURN", pct_str(median_ret), median_ret >= 0 ? ui::colors::POSITIVE() : ui::colors::NEGATIVE()},
        {"5TH PERCENTILE", pct_str(pct5), pct5 >= 0 ? ui::colors::POSITIVE() : ui::colors::NEGATIVE()},
        {"95TH PERCENTILE", pct_str(pct95), ui::colors::POSITIVE()},
        {"PROB OF LOSS", pct_str(prob_loss), ui::colors::WARNING()},
        {"EXP MAX DRAWDOWN", pct_str(exp_max_dd), ui::colors::NEGATIVE()},
    };

    for (const auto& s : stats) {
        auto* col = new QWidget(this);
        auto* col_l = new QVBoxLayout(col);
        col_l->setContentsMargins(0, 0, 0, 0);
        col_l->setSpacing(2);
        auto* lbl = new QLabel(s.label);
        lbl->setStyleSheet(
            QString("color:%1; font-size:8px; font-weight:700; letter-spacing:0.5px;").arg(ui::colors::TEXT_TERTIARY()));
        col_l->addWidget(lbl);
        auto* val = new QLabel(s.value);
        val->setStyleSheet(QString("color:%1; font-size:14px; font-weight:700;").arg(s.color));
        col_l->addWidget(val);
        sb_lay->addWidget(col);
    }
    sb_lay->addStretch();
    cl->addWidget(stats_bar);

    // ── Monte Carlo paths chart ───────────────────────────────────────────────
    auto* chart = new QChart;
    chart->setBackgroundBrush(QBrush(QColor(ui::colors::BG_BASE())));
    chart->setBackgroundRoundness(0);
    chart->setMargins(QMargins(8, 8, 8, 8));
    chart->legend()->hide();
    chart->setTitle("");

    // Parse paths array
    const QJsonArray paths_arr = mc_data_["paths"].toArray();
    const int n_paths = paths_arr.size();

    // Compute median path for highlighting
    const int n_steps = (n_paths > 0) ? paths_arr[0].toArray().size() : 0;
    QVector<double> median_path(n_steps, 0.0);
    if (n_paths > 0 && n_steps > 0) {
        for (int t = 0; t < n_steps; ++t) {
            QVector<double> vals;
            vals.reserve(n_paths);
            for (int p = 0; p < n_paths; ++p) {
                vals.append(paths_arr[p].toArray()[t].toDouble());
            }
            std::sort(vals.begin(), vals.end());
            median_path[t] = vals[vals.size() / 2];
        }
    }

    // Dim cyan for background paths
    QColor path_color(ui::colors::CYAN());
    path_color.setAlpha(40);
    QPen path_pen(path_color);
    path_pen.setWidth(1);

    for (int p = 0; p < n_paths; ++p) {
        const QJsonArray path_data = paths_arr[p].toArray();
        auto* series = new QLineSeries;
        series->setPen(path_pen);
        const int pts = path_data.size();
        for (int t = 0; t < pts; ++t) {
            series->append(static_cast<qreal>(t + 1), path_data[t].toDouble());
        }
        chart->addSeries(series);
    }

    // Median path — bright cyan
    if (n_steps > 0) {
        auto* median_series = new QLineSeries;
        QPen med_pen{QColor(ui::colors::CYAN())};
        med_pen.setWidth(2);
        median_series->setPen(med_pen);
        for (int t = 0; t < n_steps; ++t) {
            median_series->append(static_cast<qreal>(t + 1), median_path[t]);
        }
        chart->addSeries(median_series);
    }

    // Zero line
    auto* zero_series = new QLineSeries;
    QPen zero_pen{QColor(ui::colors::BORDER_BRIGHT())};
    zero_pen.setStyle(Qt::DashLine);
    zero_pen.setWidth(1);
    zero_series->setPen(zero_pen);
    zero_series->append(1, 0.0);
    zero_series->append(n_steps > 0 ? n_steps : 252, 0.0);
    chart->addSeries(zero_series);

    // Axes
    auto* x_axis = new QValueAxis;
    x_axis->setRange(1, n_steps > 0 ? n_steps : 252);
    x_axis->setTitleText("Trading Days");
    x_axis->setTitleBrush(QBrush(QColor(ui::colors::TEXT_SECONDARY())));
    x_axis->setLabelsBrush(QBrush(QColor(ui::colors::TEXT_TERTIARY())));
    x_axis->setGridLineColor(QColor(ui::colors::BORDER_DIM()));
    x_axis->setLinePenColor(QColor(ui::colors::BORDER_MED()));
    x_axis->setLabelsFont(QFont("Consolas", 8));

    auto* y_axis = new QValueAxis;
    y_axis->setTitleText("Cumulative Return (%)");
    y_axis->setTitleBrush(QBrush(QColor(ui::colors::TEXT_SECONDARY())));
    y_axis->setLabelsBrush(QBrush(QColor(ui::colors::TEXT_TERTIARY())));
    y_axis->setGridLineColor(QColor(ui::colors::BORDER_DIM()));
    y_axis->setLinePenColor(QColor(ui::colors::BORDER_MED()));
    y_axis->setLabelsFont(QFont("Consolas", 8));

    chart->addAxis(x_axis, Qt::AlignBottom);
    chart->addAxis(y_axis, Qt::AlignLeft);

    const auto& all_series = chart->series();
    for (auto* s : all_series) {
        s->attachAxis(x_axis);
        s->attachAxis(y_axis);
    }

    auto* chart_view = new QChartView(chart);
    chart_view->setRenderHint(QPainter::Antialiasing, false);
    chart_view->setStyleSheet(QString("background:%1; border:none;").arg(ui::colors::BG_BASE()));
    mc_chart_ = chart_view;

    cl->addWidget(chart_view, 1);

    // Caption
    int n_shown = mc_data_["num_paths_shown"].toInt();
    auto* caption = new QLabel(QString("Showing %1 of 1000 simulated paths over 252 trading days (GBM)."
                                       " Bright line = median path.")
                                   .arg(n_shown));
    caption->setStyleSheet(QString("color:%1; font-size:9px; padding:4px 16px;").arg(ui::colors::TEXT_TERTIARY()));
    cl->addWidget(caption);

    mc_stack_->addWidget(content);
    mc_stack_->setCurrentIndex(1);
}

// ── make_chart_view ───────────────────────────────────────────────────────────

QChartView* QuantStatsView::make_chart_view(const QString& title) {
    auto* chart = new QChart;
    chart->setBackgroundBrush(QBrush(QColor(ui::colors::BG_BASE())));
    chart->setBackgroundRoundness(0);
    chart->setMargins(QMargins(8, 8, 8, 8));
    chart->legend()->hide();
    if (!title.isEmpty()) {
        chart->setTitle(title);
        chart->setTitleBrush(QBrush(QColor(ui::colors::AMBER())));
    }
    auto* view = new QChartView(chart, this);
    view->setRenderHint(QPainter::Antialiasing, false);
    view->setStyleSheet(QString("background:%1; border:none;").arg(ui::colors::BG_BASE()));
    return view;
}

// ── run_quantstats ────────────────────────────────────────────────────────────

void QuantStatsView::run_quantstats() {
    if (qs_running_ || summary_.holdings.isEmpty())
        return;
    qs_running_ = true;
    qs_run_btn_->setEnabled(false);
    qs_status_->setText("Fetching 1-year price history...");
    qs_status_->setStyleSheet(QString("color:%1; font-size:9px;").arg(ui::colors::AMBER()));

    QStringList symbols;
    QJsonArray weights_arr;
    for (const auto& h : summary_.holdings) {
        symbols.append(h.symbol);
        weights_arr.append(h.weight / 100.0);
    }

    QJsonObject args;
    args["symbols"] = QJsonArray::fromStringList(symbols);
    args["weights"] = weights_arr;
    const QString args_str = QJsonDocument(args).toJson(QJsonDocument::Compact);

    QPointer<QuantStatsView> self = this;
    PythonRunner::instance().run("quantstats_analysis", {args_str}, [self](PythonResult result) {
        if (!self)
            return;
        QMetaObject::invokeMethod(
            self,
            [self, result]() {
                if (!self)
                    return;
                self->qs_running_ = false;
                self->qs_run_btn_->setEnabled(true);

                if (!result.success || result.output.trimmed().isEmpty()) {
                    self->qs_status_->setText("QuantStats failed");
                    self->qs_status_->setStyleSheet(QString("color:%1; font-size:9px;").arg(ui::colors::NEGATIVE()));
                    LOG_ERROR("QuantStats", "Script failed: " + result.error.left(200));
                    return;
                }

                const auto doc = QJsonDocument::fromJson(result.output.trimmed().toUtf8());
                if (!doc.isObject()) {
                    self->qs_status_->setText("Invalid JSON response");
                    self->qs_status_->setStyleSheet(QString("color:%1; font-size:9px;").arg(ui::colors::NEGATIVE()));
                    LOG_ERROR("QuantStats", "Bad JSON: " + result.output.left(200));
                    return;
                }

                const QJsonObject root = doc.object();
                if (root.contains("error")) {
                    self->qs_status_->setText("Error: " + root["error"].toString());
                    self->qs_status_->setStyleSheet(QString("color:%1; font-size:9px;").arg(ui::colors::NEGATIVE()));
                    return;
                }

                self->qs_data_ = root;
                self->qs_status_->setText("Complete");
                self->qs_status_->setStyleSheet(QString("color:%1; font-size:9px;").arg(ui::colors::POSITIVE()));
                self->update_metrics();
                self->update_returns();
                self->update_drawdown();
                self->update_rolling();
            },
            Qt::QueuedConnection);
    });
}

// ── run_monte_carlo ───────────────────────────────────────────────────────────

void QuantStatsView::run_monte_carlo() {
    if (mc_running_ || summary_.holdings.isEmpty())
        return;
    mc_running_ = true;
    mc_run_btn_->setEnabled(false);
    mc_status_->setText("Running 1000 simulation paths...");
    mc_status_->setStyleSheet(QString("color:%1; font-size:10px;").arg(ui::colors::AMBER()));

    QStringList symbols;
    QJsonArray weights_arr;
    for (const auto& h : summary_.holdings) {
        symbols.append(h.symbol);
        weights_arr.append(h.weight / 100.0);
    }

    QJsonObject args;
    args["symbols"] = QJsonArray::fromStringList(symbols);
    args["weights"] = weights_arr;
    args["num_simulations"] = 1000;
    const QString args_str = QJsonDocument(args).toJson(QJsonDocument::Compact);

    QPointer<QuantStatsView> self = this;
    PythonRunner::instance().run("quantstats_monte_carlo", {args_str}, [self](PythonResult result) {
        if (!self)
            return;
        QMetaObject::invokeMethod(
            self,
            [self, result]() {
                if (!self)
                    return;
                self->mc_running_ = false;
                self->mc_run_btn_->setEnabled(true);

                if (!result.success || result.output.trimmed().isEmpty()) {
                    self->mc_status_->setText("Monte Carlo simulation failed.");
                    self->mc_status_->setStyleSheet(QString("color:%1; font-size:10px;").arg(ui::colors::NEGATIVE()));
                    LOG_ERROR("MonteCarlo", "Script failed: " + result.error.left(200));
                    return;
                }

                const auto doc = QJsonDocument::fromJson(result.output.trimmed().toUtf8());
                if (!doc.isObject()) {
                    self->mc_status_->setText("Invalid JSON response");
                    self->mc_status_->setStyleSheet(QString("color:%1; font-size:10px;").arg(ui::colors::NEGATIVE()));
                    LOG_ERROR("MonteCarlo", "Bad JSON: " + result.output.left(200));
                    return;
                }

                const QJsonObject root = doc.object();
                if (root.contains("error")) {
                    self->mc_status_->setText("Error: " + root["error"].toString());
                    self->mc_status_->setStyleSheet(QString("color:%1; font-size:10px;").arg(ui::colors::NEGATIVE()));
                    return;
                }

                self->mc_data_ = root;
                self->mc_status_->setText(
                    QString("Complete — %1 paths simulated").arg(root["num_paths_shown"].toInt() > 0 ? "1000" : "0"));
                self->mc_status_->setStyleSheet(QString("color:%1; font-size:10px;").arg(ui::colors::POSITIVE()));
                self->update_monte_carlo_chart();
            },
            Qt::QueuedConnection);
    });
}

} // namespace fincept::screens
