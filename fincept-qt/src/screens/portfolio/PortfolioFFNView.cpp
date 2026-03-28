// src/screens/portfolio/PortfolioFFNView.cpp
#include "screens/portfolio/PortfolioFFNView.h"

#include "core/logging/Logger.h"
#include "python/PythonRunner.h"
#include "ui/theme/Theme.h"

#include <QAreaSeries>
#include <QChart>
#include <QChartView>
#include <QDateTimeAxis>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLineSeries>
#include <QStackedWidget>
#include <QValueAxis>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>

QT_CHARTS_USE_NAMESPACE

using fincept::python::PythonResult;
using fincept::python::PythonRunner;

namespace fincept::screens {

// ── Palette for multi-series charts ──────────────────────────────────────────
static const QStringList k_series_colors = {
    "#0891b2",  // cyan
    "#d97706",  // amber
    "#16a34a",  // green
    "#8b5cf6",  // purple
    "#f97316",  // orange
    "#ec4899",  // pink
    "#14b8a6",  // teal
    "#eab308",  // yellow
};

static QColor series_color(int index) {
    return QColor(k_series_colors[index % k_series_colors.size()]);
}

// ── Helpers ───────────────────────────────────────────────────────────────────
static QString pct_str(double v, int dp = 2) {
    return QString("%1%2%").arg(v >= 0 ? "+" : "").arg(QString::number(v * 100.0, 'f', dp));
}
static QString fmt(double v, int dp = 2) { return QString::number(v, 'f', dp); }

static QTableWidgetItem* make_item(const QString& text, Qt::Alignment align,
                                   const QColor& color) {
    auto* item = new QTableWidgetItem(text);
    item->setTextAlignment(align);
    item->setForeground(color);
    return item;
}

// ── Constructor / build_ui ────────────────────────────────────────────────────

PortfolioFFNView::PortfolioFFNView(QWidget* parent) : QWidget(parent) {
    build_ui();
}

void PortfolioFFNView::build_ui() {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    // ── Header ────────────────────────────────────────────────────────────────
    auto* header = new QWidget;
    header->setFixedHeight(36);
    header->setStyleSheet(
        QString("background:%1; border-bottom:1px solid %2;")
            .arg(ui::colors::BG_SURFACE, ui::colors::AMBER));

    auto* h_layout = new QHBoxLayout(header);
    h_layout->setContentsMargins(8, 0, 8, 0);
    h_layout->setSpacing(8);

    back_btn_ = new QPushButton("\u2190 BACK");
    back_btn_->setFixedHeight(24);
    back_btn_->setCursor(Qt::PointingHandCursor);
    back_btn_->setStyleSheet(
        QString("QPushButton { background:transparent; color:%1; border:1px solid %1;"
                "  padding:0 10px; font-size:9px; font-weight:700; }"
                "QPushButton:hover { background:%1; color:#000; }")
            .arg(ui::colors::AMBER));
    connect(back_btn_, &QPushButton::clicked, this, &PortfolioFFNView::back_requested);
    h_layout->addWidget(back_btn_);

    auto* sep = new QWidget;
    sep->setFixedSize(1, 20);
    sep->setStyleSheet(QString("background:%1;").arg(ui::colors::BORDER_MED));
    h_layout->addWidget(sep);

    auto* title = new QLabel("FFN ANALYTICS");
    title->setStyleSheet(
        QString("color:%1; font-size:11px; font-weight:700; letter-spacing:1px;")
            .arg(ui::colors::AMBER));
    h_layout->addWidget(title);

    h_layout->addStretch();

    status_label_ = new QLabel;
    status_label_->setStyleSheet(
        QString("color:%1; font-size:9px;").arg(ui::colors::TEXT_TERTIARY));
    h_layout->addWidget(status_label_);

    run_btn_ = new QPushButton("RUN FFN ANALYSIS");
    run_btn_->setFixedHeight(24);
    run_btn_->setCursor(Qt::PointingHandCursor);
    run_btn_->setStyleSheet(
        QString("QPushButton { background:%1; color:#000; border:none;"
                "  padding:0 12px; font-size:9px; font-weight:700; letter-spacing:0.5px; }"
                "QPushButton:hover { background:#22c55e; }"
                "QPushButton:disabled { background:%2; color:%3; }")
            .arg(ui::colors::POSITIVE, ui::colors::BG_SURFACE, ui::colors::TEXT_TERTIARY));
    connect(run_btn_, &QPushButton::clicked, this, &PortfolioFFNView::run_ffn);
    h_layout->addWidget(run_btn_);

    layout->addWidget(header);

    // ── Tabs ──────────────────────────────────────────────────────────────────
    tabs_ = new QTabWidget;
    tabs_->setDocumentMode(true);
    tabs_->setStyleSheet(
        QString("QTabWidget::pane { border:0; background:%1; }"
                "QTabBar::tab { background:%2; color:%3; padding:6px 14px; border:0;"
                "  border-bottom:2px solid transparent; font-size:9px; font-weight:700;"
                "  letter-spacing:0.5px; }"
                "QTabBar::tab:selected { color:%4; border-bottom:2px solid %4; }"
                "QTabBar::tab:hover { color:%5; }")
            .arg(ui::colors::BG_BASE, ui::colors::BG_SURFACE, ui::colors::TEXT_SECONDARY,
                 ui::colors::AMBER, ui::colors::TEXT_PRIMARY));

    const QString table_ss =
        QString("QTableWidget { background:%1; color:%2; border:none; font-size:11px; }"
                "QTableWidget::item { padding:3px 8px; border-bottom:1px solid %3; }"
                "QHeaderView::section { background:%4; color:%5; border:none;"
                "  border-bottom:2px solid %6; padding:3px 8px;"
                "  font-size:9px; font-weight:700; }")
            .arg(ui::colors::BG_BASE, ui::colors::TEXT_PRIMARY, ui::colors::BORDER_DIM,
                 ui::colors::BG_SURFACE, ui::colors::TEXT_SECONDARY, ui::colors::AMBER);

    // helper to create a configured QTableWidget
    auto make_table = [&](int cols, const QStringList& headers) -> QTableWidget* {
        auto* t = new QTableWidget;
        t->setColumnCount(cols);
        t->setHorizontalHeaderLabels(headers);
        t->setSelectionMode(QAbstractItemView::NoSelection);
        t->setEditTriggers(QAbstractItemView::NoEditTriggers);
        t->setShowGrid(false);
        t->verticalHeader()->setVisible(false);
        t->horizontalHeader()->setStretchLastSection(true);
        t->setStyleSheet(table_ss);
        return t;
    };

    // helper to create a placeholder label
    auto make_placeholder_label = [this](const QString& text) -> QLabel* {
        auto* lbl = new QLabel(text);
        lbl->setAlignment(Qt::AlignCenter);
        lbl->setWordWrap(true);
        lbl->setStyleSheet(
            QString("color:%1; font-size:11px;").arg(ui::colors::TEXT_TERTIARY));
        return lbl;
    };

    // ── OVERVIEW tab ─────────────────────────────────────────────────────────
    {
        auto* w = new QWidget;
        auto* vl = new QVBoxLayout(w);
        vl->setContentsMargins(12, 8, 12, 8);

        auto* lbl = new QLabel("PORTFOLIO METRICS OVERVIEW");
        lbl->setStyleSheet(
            QString("color:%1; font-size:11px; font-weight:700; letter-spacing:1px;")
                .arg(ui::colors::AMBER));
        vl->addWidget(lbl);

        overview_table_ = make_table(3, {"METRIC", "PORTFOLIO", "BENCHMARK (SPY)"});
        overview_table_->setColumnWidth(0, 220);
        overview_table_->setColumnWidth(1, 140);
        vl->addWidget(overview_table_, 1);

        tabs_->addTab(w, "OVERVIEW");
    }

    // ── BENCHMARK tab ────────────────────────────────────────────────────────
    {
        benchmark_panel_ = new QWidget;
        benchmark_panel_->setStyleSheet(
            QString("background:%1;").arg(ui::colors::BG_BASE));
        auto* vl = new QVBoxLayout(benchmark_panel_);
        vl->setContentsMargins(16, 12, 16, 12);
        vl->setSpacing(10);

        auto* hdr = new QLabel("BENCHMARK COMPARISON");
        hdr->setStyleSheet(
            QString("color:%1; font-size:11px; font-weight:700; letter-spacing:1px;")
                .arg(ui::colors::AMBER));
        vl->addWidget(hdr);

        benchmark_table_ = make_table(3, {"METRIC", "PORTFOLIO", "BENCHMARK"});
        benchmark_table_->setColumnWidth(0, 200);
        benchmark_table_->setColumnWidth(1, 150);
        vl->addWidget(benchmark_table_);

        benchmark_info_label_ = new QLabel(
            "Portfolio metrics computed from 1-year price history via yfinance.\n"
            "Connect a live benchmark feed to populate the Benchmark column.");
        benchmark_info_label_->setWordWrap(true);
        benchmark_info_label_->setStyleSheet(
            QString("color:%1; font-size:10px; padding:6px 0;")
                .arg(ui::colors::TEXT_TERTIARY));
        vl->addWidget(benchmark_info_label_);

        vl->addStretch();
        tabs_->addTab(benchmark_panel_, "BENCHMARK");
    }

    // ── OPTIMISATION tab ─────────────────────────────────────────────────────
    {
        optimization_panel_ = new QWidget;
        optimization_panel_->setStyleSheet(
            QString("background:%1;").arg(ui::colors::BG_BASE));
        auto* vl = new QVBoxLayout(optimization_panel_);
        vl->setContentsMargins(12, 8, 12, 8);
        vl->setSpacing(10);

        auto* hdr = new QLabel("PORTFOLIO OPTIMISATION — WEIGHT COMPARISON");
        hdr->setStyleSheet(
            QString("color:%1; font-size:11px; font-weight:700; letter-spacing:1px;")
                .arg(ui::colors::AMBER));
        vl->addWidget(hdr);

        opt_stack_ = new QStackedWidget;

        // index 0 — placeholder
        auto* placeholder_w = new QWidget;
        auto* pl_vl = new QVBoxLayout(placeholder_w);
        pl_vl->setAlignment(Qt::AlignCenter);
        pl_vl->addWidget(make_placeholder_label(
            "EFFICIENT FRONTIER\n\nRun FFN Analysis to compute optimal weights\n"
            "(ERC, Inverse-Vol, Equal, Current)."));
        opt_stack_->addWidget(placeholder_w);

        // index 1 — tables
        auto* tables_w = new QWidget;
        auto* tvl = new QVBoxLayout(tables_w);
        tvl->setContentsMargins(0, 0, 0, 0);
        tvl->setSpacing(12);

        auto* weights_hdr = new QLabel("ALLOCATION WEIGHTS BY STRATEGY");
        weights_hdr->setStyleSheet(
            QString("color:%1; font-size:10px; font-weight:700;")
                .arg(ui::colors::TEXT_SECONDARY));
        tvl->addWidget(weights_hdr);

        opt_weights_table_ = make_table(
            5, {"SYMBOL", "CURRENT", "ERC", "INV-VOL", "EQUAL"});
        opt_weights_table_->setColumnWidth(0, 100);
        tvl->addWidget(opt_weights_table_);

        auto* stats_hdr = new QLabel("STRATEGY PERFORMANCE STATS");
        stats_hdr->setStyleSheet(
            QString("color:%1; font-size:10px; font-weight:700;")
                .arg(ui::colors::TEXT_SECONDARY));
        tvl->addWidget(stats_hdr);

        opt_stats_table_ = make_table(
            5, {"STRATEGY", "TOTAL RETURN", "VOLATILITY", "SHARPE", "MAX DRAWDOWN"});
        opt_stats_table_->setColumnWidth(0, 120);
        tvl->addWidget(opt_stats_table_);

        opt_stack_->addWidget(tables_w);
        vl->addWidget(opt_stack_, 1);

        tabs_->addTab(optimization_panel_, "OPTIMISATION");
    }

    // ── REBASED tab ───────────────────────────────────────────────────────────
    {
        rebased_panel_ = new QWidget;
        rebased_panel_->setStyleSheet(
            QString("background:%1;").arg(ui::colors::BG_BASE));
        auto* vl = new QVBoxLayout(rebased_panel_);
        vl->setContentsMargins(12, 8, 12, 8);
        vl->setSpacing(0);

        rebased_stack_ = new QStackedWidget;
        // index 0 — placeholder
        auto* ph = new QWidget;
        auto* phl = new QVBoxLayout(ph);
        phl->setAlignment(Qt::AlignCenter);
        phl->addWidget(make_placeholder_label(
            "REBASED PRICE CHARTS\n\nRun FFN Analysis to compare holdings\n"
            "on a common base of 100."));
        rebased_stack_->addWidget(ph);

        // index 1 — chart (allocated lazily in update_rebased)
        rebased_chart_view_ = make_chart_view("Rebased Performance (Base = 100)");
        rebased_stack_->addWidget(rebased_chart_view_);

        vl->addWidget(rebased_stack_, 1);
        tabs_->addTab(rebased_panel_, "REBASED");
    }

    // ── DRAWDOWNS tab ─────────────────────────────────────────────────────────
    {
        drawdowns_panel_ = new QWidget;
        drawdowns_panel_->setStyleSheet(
            QString("background:%1;").arg(ui::colors::BG_BASE));
        auto* vl = new QVBoxLayout(drawdowns_panel_);
        vl->setContentsMargins(12, 8, 12, 8);
        vl->setSpacing(0);

        drawdowns_stack_ = new QStackedWidget;
        auto* ph = new QWidget;
        auto* phl = new QVBoxLayout(ph);
        phl->setAlignment(Qt::AlignCenter);
        phl->addWidget(make_placeholder_label(
            "DRAWDOWN ANALYSIS\n\nRun FFN Analysis to visualise historical\n"
            "drawdowns for each holding."));
        drawdowns_stack_->addWidget(ph);

        drawdowns_chart_view_ = make_chart_view("Drawdown Analysis");
        drawdowns_stack_->addWidget(drawdowns_chart_view_);

        vl->addWidget(drawdowns_stack_, 1);
        tabs_->addTab(drawdowns_panel_, "DRAWDOWNS");
    }

    // ── ROLLING CORRELATIONS tab ──────────────────────────────────────────────
    {
        rolling_panel_ = new QWidget;
        rolling_panel_->setStyleSheet(
            QString("background:%1;").arg(ui::colors::BG_BASE));
        auto* vl = new QVBoxLayout(rolling_panel_);
        vl->setContentsMargins(12, 8, 12, 8);
        vl->setSpacing(0);

        rolling_stack_ = new QStackedWidget;
        auto* ph = new QWidget;
        auto* phl = new QVBoxLayout(ph);
        phl->setAlignment(Qt::AlignCenter);
        phl->addWidget(make_placeholder_label(
            "ROLLING CORRELATIONS\n\nAdd more holdings and run FFN Analysis\n"
            "to track 60-day rolling correlations."));
        rolling_stack_->addWidget(ph);

        rolling_chart_view_ = make_chart_view("Rolling 60-Day Correlations");
        rolling_stack_->addWidget(rolling_chart_view_);

        vl->addWidget(rolling_stack_, 1);
        tabs_->addTab(rolling_panel_, "ROLLING");
    }

    layout->addWidget(tabs_);
}

// ── Chart helper ──────────────────────────────────────────────────────────────

QChartView* PortfolioFFNView::make_chart_view(const QString& title) {
    auto* chart = new QChart;
    chart->setTitle(title);
    chart->setBackgroundBrush(QBrush(QColor(ui::colors::BG_BASE)));
    chart->setTitleBrush(QBrush(QColor(ui::colors::AMBER)));
    chart->setTitleFont(QFont("monospace", 9, QFont::Bold));
    chart->legend()->setVisible(true);
    chart->legend()->setAlignment(Qt::AlignBottom);
    chart->legend()->setLabelColor(QColor(ui::colors::TEXT_SECONDARY));
    chart->legend()->setBackgroundVisible(false);
    chart->setPlotAreaBackgroundBrush(QBrush(QColor(ui::colors::BG_SURFACE)));
    chart->setPlotAreaBackgroundVisible(true);

    auto* cv = new QChartView(chart);
    cv->setRenderHint(QPainter::Antialiasing, false);
    cv->setStyleSheet(QString("background:%1; border:none;").arg(ui::colors::BG_BASE));
    return cv;
}

// ── set_data ──────────────────────────────────────────────────────────────────

void PortfolioFFNView::set_data(const portfolio::PortfolioSummary& summary,
                                const QString& currency) {
    summary_  = summary;
    currency_ = currency;
    update_overview();
}

// ── update_overview ───────────────────────────────────────────────────────────

void PortfolioFFNView::update_overview() {
    bool has_ffn = !ffn_data_.isEmpty();

    double total_ann_ret = 0, total_ann_vol = 0, total_sharpe = 0, total_max_dd = 0;
    double total_best_day = 0, total_worst_day = 0;
    int pos_days = 0, neg_days = 0;
    QString best_sym, worst_sym;
    double best_ret = -1e9, worst_ret = 1e9;

    if (has_ffn) {
        for (const auto& h : summary_.holdings) {
            if (!ffn_data_.contains(h.symbol))
                continue;
            auto s  = ffn_data_[h.symbol].toObject();
            double w = h.weight / 100.0;
            total_ann_ret  += s["annualized_return"].toDouble()     * w;
            total_ann_vol  += s["annualized_volatility"].toDouble() * w;
            total_sharpe   += s["sharpe_ratio"].toDouble()          * w;
            total_max_dd   += s["max_drawdown"].toDouble()          * w;
            total_best_day  = std::max(total_best_day,  s["best_day"].toDouble());
            total_worst_day = std::min(total_worst_day, s["worst_day"].toDouble());
            pos_days       += s["positive_days"].toInt();
            neg_days       += s["negative_days"].toInt();
            double sym_ret  = s["total_return"].toDouble();
            if (sym_ret > best_ret)  { best_ret  = sym_ret; best_sym  = h.symbol; }
            if (sym_ret < worst_ret) { worst_ret = sym_ret; worst_sym = h.symbol; }
        }
    }

    double pnl_pct  = summary_.total_unrealized_pnl_percent;
    double win_rate = summary_.total_positions > 0
                          ? summary_.gainers * 100.0 / summary_.total_positions : 0.0;

    struct Row { QString name; QString value; QString benchmark; const char* color; };
    QVector<Row> rows;

    if (has_ffn) {
        rows = {
            {"Annualized Return",
             pct_str(total_ann_ret),
             "--",
             total_ann_ret >= 0 ? ui::colors::POSITIVE : ui::colors::NEGATIVE},
            {"Annualized Volatility",
             pct_str(total_ann_vol),
             "--",
             ui::colors::CYAN},
            {"Sharpe Ratio",
             fmt(total_sharpe),
             "--",
             total_sharpe >= 0 ? ui::colors::POSITIVE : ui::colors::NEGATIVE},
            {"Max Drawdown",
             pct_str(total_max_dd),
             "--",
             ui::colors::NEGATIVE},
            {"Best Day (any)",
             pct_str(total_best_day),
             "--",
             ui::colors::POSITIVE},
            {"Worst Day (any)",
             pct_str(total_worst_day),
             "--",
             ui::colors::NEGATIVE},
            {"Positive Days",
             QString::number(pos_days),
             "--",
             ui::colors::POSITIVE},
            {"Negative Days",
             QString::number(neg_days),
             "--",
             ui::colors::NEGATIVE},
            {"Best Holding",
             best_sym.isEmpty()  ? "--" : best_sym  + " (" + pct_str(best_ret)  + ")",
             "--",
             ui::colors::POSITIVE},
            {"Worst Holding",
             worst_sym.isEmpty() ? "--" : worst_sym + " (" + pct_str(worst_ret) + ")",
             "--",
             ui::colors::NEGATIVE},
            {"Total Return (cost)",
             pct_str(pnl_pct / 100.0),
             "--",
             pnl_pct >= 0 ? ui::colors::POSITIVE : ui::colors::NEGATIVE},
            {"Win Rate",
             fmt(win_rate) + "%",
             "--",
             ui::colors::CYAN},
            {"Positions",
             QString::number(summary_.total_positions),
             "--",
             ui::colors::CYAN},
            {"Total Value",
             currency_ + " " + fmt(summary_.total_market_value),
             "--",
             ui::colors::WARNING},
            {"Cost Basis",
             currency_ + " " + fmt(summary_.total_cost_basis),
             "--",
             ui::colors::TEXT_SECONDARY},
        };
    } else {
        // Pre-run: show live data and prompt user
        double vol = 0; int vn = 0;
        for (const auto& h : summary_.holdings)
            if (std::abs(h.day_change_percent) > 0.001) {
                vol += std::abs(h.day_change_percent);
                ++vn;
            }
        double daily_vol = vn > 0 ? vol / vn : 0.0;
        double ann_vol   = daily_vol * std::sqrt(252.0);
        double sharpe    = ann_vol > 0.01 ? (pnl_pct - 4.0) / ann_vol : 0.0;

        rows = {
            {"Total Return (unrealized)",
             pct_str(pnl_pct / 100.0),
             "--",
             pnl_pct >= 0 ? ui::colors::POSITIVE : ui::colors::NEGATIVE},
            {"Annualized Volatility (est.)",
             pct_str(ann_vol / 100.0),
             "--",
             ui::colors::CYAN},
            {"Sharpe Ratio (est.)",
             fmt(sharpe),
             "--",
             sharpe >= 0 ? ui::colors::POSITIVE : ui::colors::NEGATIVE},
            {"Win Rate",
             fmt(win_rate) + "%",
             "--",
             ui::colors::CYAN},
            {"Positions",
             QString::number(summary_.total_positions),
             "--",
             ui::colors::CYAN},
            {"Total Value",
             currency_ + " " + fmt(summary_.total_market_value),
             "--",
             ui::colors::WARNING},
            {"Cost Basis",
             currency_ + " " + fmt(summary_.total_cost_basis),
             "--",
             ui::colors::TEXT_SECONDARY},
            {"FFN Deep Metrics",
             "Click RUN FFN ANALYSIS for full stats",
             "--",
             ui::colors::AMBER},
        };
    }

    overview_table_->setRowCount(rows.size());
    for (int r = 0; r < rows.size(); ++r) {
        const auto& row = rows[r];
        overview_table_->setRowHeight(r, 28);
        overview_table_->setItem(
            r, 0, make_item(row.name,      Qt::AlignLeft  | Qt::AlignVCenter,
                            QColor(ui::colors::TEXT_SECONDARY)));
        overview_table_->setItem(
            r, 1, make_item(row.value,     Qt::AlignRight | Qt::AlignVCenter,
                            QColor(row.color)));
        overview_table_->setItem(
            r, 2, make_item(row.benchmark, Qt::AlignRight | Qt::AlignVCenter,
                            QColor(ui::colors::TEXT_TERTIARY)));
    }
}

// ── update_benchmark ──────────────────────────────────────────────────────────

void PortfolioFFNView::update_benchmark() {
    // Populate from optimization.stats.current which gives real portfolio perf.
    auto opt_obj   = ffn_data_["optimization"].toObject();
    auto stats_obj = opt_obj["stats"].toObject();
    auto cur_stats = stats_obj["current"].toObject();

    // Rows: metric | portfolio value | benchmark placeholder
    struct BRow { QString metric; QString portfolio; };
    QVector<BRow> rows;

    if (!cur_stats.isEmpty()) {
        double total_ret = cur_stats["total_return"].toDouble();
        double cagr      = cur_stats["cagr"].toDouble();
        double vol       = cur_stats["volatility"].toDouble();
        double sharpe    = cur_stats["sharpe"].toDouble();
        double max_dd    = cur_stats["max_drawdown"].toDouble();

        rows = {
            {"Total Return",   pct_str(total_ret)},
            {"CAGR",           pct_str(cagr)},
            {"Volatility",     pct_str(vol)},
            {"Sharpe Ratio",   fmt(sharpe)},
            {"Max Drawdown",   pct_str(max_dd)},
        };
    } else {
        rows = {
            {"Total Return",   "--"},
            {"CAGR",           "--"},
            {"Volatility",     "--"},
            {"Sharpe Ratio",   "--"},
            {"Max Drawdown",   "--"},
        };
    }

    benchmark_table_->setRowCount(rows.size());
    for (int r = 0; r < rows.size(); ++r) {
        const auto& row = rows[r];
        benchmark_table_->setRowHeight(r, 28);
        benchmark_table_->setItem(
            r, 0, make_item(row.metric, Qt::AlignLeft | Qt::AlignVCenter,
                            QColor(ui::colors::TEXT_SECONDARY)));
        benchmark_table_->setItem(
            r, 1, make_item(row.portfolio, Qt::AlignRight | Qt::AlignVCenter,
                            QColor(ui::colors::TEXT_PRIMARY)));
        benchmark_table_->setItem(
            r, 2, make_item("--", Qt::AlignRight | Qt::AlignVCenter,
                            QColor(ui::colors::TEXT_TERTIARY)));
    }
}

// ── update_optimization ───────────────────────────────────────────────────────

void PortfolioFFNView::update_optimization() {
    auto opt_obj = ffn_data_["optimization"].toObject();
    if (opt_obj.isEmpty()) {
        opt_stack_->setCurrentIndex(0);
        return;
    }

    auto erc_obj     = opt_obj["erc"].toObject();
    auto inv_obj     = opt_obj["inv_vol"].toObject();
    auto equal_obj   = opt_obj["equal"].toObject();
    auto current_obj = opt_obj["current"].toObject();
    auto stats_obj   = opt_obj["stats"].toObject();

    // Collect symbols from the current weights object
    QStringList syms = current_obj.keys();
    syms.sort();

    // ── Weights table ─────────────────────────────────────────────────────────
    opt_weights_table_->setRowCount(syms.size());
    for (int r = 0; r < syms.size(); ++r) {
        const QString& sym = syms[r];
        opt_weights_table_->setRowHeight(r, 28);

        auto pct = [](double v) { return fmt(v * 100.0, 1) + "%"; };

        opt_weights_table_->setItem(
            r, 0, make_item(sym, Qt::AlignLeft | Qt::AlignVCenter,
                            QColor(ui::colors::AMBER)));
        opt_weights_table_->setItem(
            r, 1, make_item(pct(current_obj[sym].toDouble()),
                            Qt::AlignRight | Qt::AlignVCenter, QColor(ui::colors::CYAN)));
        opt_weights_table_->setItem(
            r, 2, make_item(pct(erc_obj[sym].toDouble()),
                            Qt::AlignRight | Qt::AlignVCenter, QColor(ui::colors::TEXT_PRIMARY)));
        opt_weights_table_->setItem(
            r, 3, make_item(pct(inv_obj[sym].toDouble()),
                            Qt::AlignRight | Qt::AlignVCenter, QColor(ui::colors::TEXT_PRIMARY)));
        opt_weights_table_->setItem(
            r, 4, make_item(pct(equal_obj[sym].toDouble()),
                            Qt::AlignRight | Qt::AlignVCenter, QColor(ui::colors::TEXT_PRIMARY)));
    }

    // ── Stats table ───────────────────────────────────────────────────────────
    const QStringList strategy_keys = {"current", "erc", "inv_vol", "equal"};
    const QStringList strategy_names = {"CURRENT", "ERC", "INV-VOL", "EQUAL"};
    opt_stats_table_->setRowCount(strategy_keys.size());

    for (int r = 0; r < strategy_keys.size(); ++r) {
        const QString& key  = strategy_keys[r];
        const QString& name = strategy_names[r];
        auto s = stats_obj[key].toObject();
        opt_stats_table_->setRowHeight(r, 28);

        double tr  = s["total_return"].toDouble();
        double vol = s["volatility"].toDouble();
        double sh  = s["sharpe"].toDouble();
        double dd  = s["max_drawdown"].toDouble();

        opt_stats_table_->setItem(
            r, 0, make_item(name, Qt::AlignLeft | Qt::AlignVCenter,
                            QColor(ui::colors::AMBER)));
        opt_stats_table_->setItem(
            r, 1, make_item(pct_str(tr),
                            Qt::AlignRight | Qt::AlignVCenter,
                            QColor(tr >= 0 ? ui::colors::POSITIVE : ui::colors::NEGATIVE)));
        opt_stats_table_->setItem(
            r, 2, make_item(pct_str(vol),
                            Qt::AlignRight | Qt::AlignVCenter, QColor(ui::colors::CYAN)));
        opt_stats_table_->setItem(
            r, 3, make_item(fmt(sh),
                            Qt::AlignRight | Qt::AlignVCenter,
                            QColor(sh >= 0 ? ui::colors::POSITIVE : ui::colors::NEGATIVE)));
        opt_stats_table_->setItem(
            r, 4, make_item(pct_str(dd),
                            Qt::AlignRight | Qt::AlignVCenter, QColor(ui::colors::NEGATIVE)));
    }

    opt_stack_->setCurrentIndex(1);
}

// ── update_rebased ────────────────────────────────────────────────────────────

void PortfolioFFNView::update_rebased() {
    auto rebased_obj = ffn_data_["rebased"].toObject();
    if (rebased_obj.isEmpty()) {
        rebased_stack_->setCurrentIndex(0);
        return;
    }

    QChart* chart = rebased_chart_view_->chart();
    chart->removeAllSeries();
    // Remove old axes
    const auto old_axes = chart->axes();
    for (auto* ax : old_axes)
        chart->removeAxis(ax);

    auto* x_axis = new QDateTimeAxis;
    x_axis->setFormat("MMM yy");
    x_axis->setTitleText("Date");
    x_axis->setLabelsColor(QColor(ui::colors::TEXT_SECONDARY));
    x_axis->setTitleBrush(QBrush(QColor(ui::colors::TEXT_TERTIARY)));
    x_axis->setGridLineColor(QColor(ui::colors::BORDER_DIM));
    x_axis->setLinePenColor(QColor(ui::colors::BORDER_MED));

    auto* y_axis = new QValueAxis;
    y_axis->setTitleText("Value (Base = 100)");
    y_axis->setLabelsColor(QColor(ui::colors::TEXT_SECONDARY));
    y_axis->setTitleBrush(QBrush(QColor(ui::colors::TEXT_TERTIARY)));
    y_axis->setGridLineColor(QColor(ui::colors::BORDER_DIM));
    y_axis->setLinePenColor(QColor(ui::colors::BORDER_MED));

    chart->addAxis(x_axis, Qt::AlignBottom);
    chart->addAxis(y_axis, Qt::AlignLeft);

    int color_idx = 0;
    QStringList syms = rebased_obj.keys();
    syms.sort();

    double y_min = 1e9, y_max = -1e9;
    qint64 x_min = std::numeric_limits<qint64>::max();
    qint64 x_max = std::numeric_limits<qint64>::min();

    for (const QString& sym : syms) {
        auto date_map = rebased_obj[sym].toObject();
        if (date_map.isEmpty())
            continue;

        auto* series = new QLineSeries;
        series->setName(sym);
        QColor col = series_color(color_idx++);
        series->setColor(col);
        series->setPen(QPen(col, 1.5));

        QStringList dates = date_map.keys();
        dates.sort();
        for (const QString& d : dates) {
            QDateTime dt = QDateTime::fromString(d, "yyyy-MM-dd");
            if (!dt.isValid())
                continue;
            double v = date_map[d].toDouble();
            qint64 ms = dt.toMSecsSinceEpoch();
            series->append(ms, v);
            x_min = std::min(x_min, ms);
            x_max = std::max(x_max, ms);
            y_min = std::min(y_min, v);
            y_max = std::max(y_max, v);
        }

        chart->addSeries(series);
        series->attachAxis(x_axis);
        series->attachAxis(y_axis);
    }

    if (x_min < x_max) {
        x_axis->setRange(QDateTime::fromMSecsSinceEpoch(x_min),
                         QDateTime::fromMSecsSinceEpoch(x_max));
    }
    if (y_min < y_max) {
        double pad = (y_max - y_min) * 0.05;
        y_axis->setRange(y_min - pad, y_max + pad);
    }

    rebased_stack_->setCurrentIndex(1);
}

// ── update_drawdowns ──────────────────────────────────────────────────────────

void PortfolioFFNView::update_drawdowns() {
    auto dd_obj = ffn_data_["drawdown_series"].toObject();
    if (dd_obj.isEmpty()) {
        drawdowns_stack_->setCurrentIndex(0);
        return;
    }

    QChart* chart = drawdowns_chart_view_->chart();
    chart->removeAllSeries();
    const auto old_axes = chart->axes();
    for (auto* ax : old_axes)
        chart->removeAxis(ax);

    auto* x_axis = new QDateTimeAxis;
    x_axis->setFormat("MMM yy");
    x_axis->setLabelsColor(QColor(ui::colors::TEXT_SECONDARY));
    x_axis->setTitleBrush(QBrush(QColor(ui::colors::TEXT_TERTIARY)));
    x_axis->setGridLineColor(QColor(ui::colors::BORDER_DIM));
    x_axis->setLinePenColor(QColor(ui::colors::BORDER_MED));

    auto* y_axis = new QValueAxis;
    y_axis->setTitleText("Drawdown");
    y_axis->setLabelsColor(QColor(ui::colors::TEXT_SECONDARY));
    y_axis->setTitleBrush(QBrush(QColor(ui::colors::TEXT_TERTIARY)));
    y_axis->setGridLineColor(QColor(ui::colors::BORDER_DIM));
    y_axis->setLinePenColor(QColor(ui::colors::BORDER_MED));

    auto* label_format_axis = y_axis;
    Q_UNUSED(label_format_axis);

    chart->addAxis(x_axis, Qt::AlignBottom);
    chart->addAxis(y_axis, Qt::AlignLeft);

    int color_idx = 0;
    QStringList syms = dd_obj.keys();
    syms.sort();

    double y_min = 0.0;
    qint64 x_min = std::numeric_limits<qint64>::max();
    qint64 x_max = std::numeric_limits<qint64>::min();

    for (const QString& sym : syms) {
        auto date_map = dd_obj[sym].toObject();
        if (date_map.isEmpty())
            continue;

        // Upper boundary line at 0
        auto* upper = new QLineSeries;
        // Drawdown line (actual values, negative fractions)
        auto* lower = new QLineSeries;
        lower->setName(sym);

        QColor col = series_color(color_idx++);

        QStringList dates = date_map.keys();
        dates.sort();
        for (const QString& d : dates) {
            QDateTime dt = QDateTime::fromString(d, "yyyy-MM-dd");
            if (!dt.isValid())
                continue;
            double v  = date_map[d].toDouble();
            qint64 ms = dt.toMSecsSinceEpoch();
            upper->append(ms, 0.0);
            lower->append(ms, v * 100.0);  // convert fraction to percent
            x_min = std::min(x_min, ms);
            x_max = std::max(x_max, ms);
            y_min = std::min(y_min, v * 100.0);
        }

        auto* area = new QAreaSeries(upper, lower);
        area->setName(sym);
        QColor fill_col = col;
        fill_col.setAlpha(60);
        area->setBrush(fill_col);
        area->setPen(QPen(col, 1.2));

        chart->addSeries(area);
        area->attachAxis(x_axis);
        area->attachAxis(y_axis);
    }

    if (x_min < x_max) {
        x_axis->setRange(QDateTime::fromMSecsSinceEpoch(x_min),
                         QDateTime::fromMSecsSinceEpoch(x_max));
    }
    double pad = std::abs(y_min) * 0.05;
    y_axis->setRange(y_min - pad, 2.0);

    drawdowns_stack_->setCurrentIndex(1);
}

// ── update_rolling ────────────────────────────────────────────────────────────

void PortfolioFFNView::update_rolling() {
    auto rc_obj = ffn_data_["rolling_corr"].toObject();
    if (rc_obj.isEmpty()) {
        rolling_stack_->setCurrentIndex(0);
        return;
    }

    QChart* chart = rolling_chart_view_->chart();
    chart->removeAllSeries();
    const auto old_axes = chart->axes();
    for (auto* ax : old_axes)
        chart->removeAxis(ax);

    auto* x_axis = new QDateTimeAxis;
    x_axis->setFormat("MMM yy");
    x_axis->setLabelsColor(QColor(ui::colors::TEXT_SECONDARY));
    x_axis->setTitleBrush(QBrush(QColor(ui::colors::TEXT_TERTIARY)));
    x_axis->setGridLineColor(QColor(ui::colors::BORDER_DIM));
    x_axis->setLinePenColor(QColor(ui::colors::BORDER_MED));

    auto* y_axis = new QValueAxis;
    y_axis->setTitleText("Correlation");
    y_axis->setRange(-1.0, 1.0);
    y_axis->setLabelsColor(QColor(ui::colors::TEXT_SECONDARY));
    y_axis->setTitleBrush(QBrush(QColor(ui::colors::TEXT_TERTIARY)));
    y_axis->setGridLineColor(QColor(ui::colors::BORDER_DIM));
    y_axis->setLinePenColor(QColor(ui::colors::BORDER_MED));

    chart->addAxis(x_axis, Qt::AlignBottom);
    chart->addAxis(y_axis, Qt::AlignLeft);

    int color_idx = 0;
    QStringList pairs = rc_obj.keys();
    pairs.sort();

    qint64 x_min = std::numeric_limits<qint64>::max();
    qint64 x_max = std::numeric_limits<qint64>::min();

    for (const QString& pair : pairs) {
        auto date_map = rc_obj[pair].toObject();
        if (date_map.isEmpty())
            continue;

        auto* series = new QLineSeries;
        series->setName(pair);
        QColor col = series_color(color_idx++);
        series->setColor(col);
        series->setPen(QPen(col, 1.5));

        QStringList dates = date_map.keys();
        dates.sort();
        for (const QString& d : dates) {
            QDateTime dt = QDateTime::fromString(d, "yyyy-MM-dd");
            if (!dt.isValid())
                continue;
            double v  = date_map[d].toDouble();
            qint64 ms = dt.toMSecsSinceEpoch();
            series->append(ms, v);
            x_min = std::min(x_min, ms);
            x_max = std::max(x_max, ms);
        }

        chart->addSeries(series);
        series->attachAxis(x_axis);
        series->attachAxis(y_axis);
    }

    if (x_min < x_max) {
        x_axis->setRange(QDateTime::fromMSecsSinceEpoch(x_min),
                         QDateTime::fromMSecsSinceEpoch(x_max));
    }

    rolling_stack_->setCurrentIndex(1);
}

// ── run_ffn ───────────────────────────────────────────────────────────────────

void PortfolioFFNView::run_ffn() {
    if (summary_.holdings.isEmpty())
        return;

    run_btn_->setEnabled(false);
    status_label_->setText("Running FFN analysis...");
    status_label_->setStyleSheet(
        QString("color:%1; font-size:9px;").arg(ui::colors::AMBER));

    QStringList symbols;
    QJsonObject weights_obj;
    for (const auto& h : summary_.holdings) {
        symbols.append(h.symbol);
        weights_obj[h.symbol] = h.weight / 100.0;
    }

    QJsonObject args;
    args["symbols"] = QJsonArray::fromStringList(symbols);
    args["weights"] = weights_obj;
    QString args_str = QJsonDocument(args).toJson(QJsonDocument::Compact);

    QPointer<PortfolioFFNView> self = this;
    PythonRunner::instance().run("ffn_analysis", {args_str}, [self](PythonResult result) {
        if (!self)
            return;
        QMetaObject::invokeMethod(self, [self, result]() {
            if (!self)
                return;

            self->run_btn_->setEnabled(true);

            if (!result.success || result.output.trimmed().isEmpty()) {
                self->status_label_->setText("FFN failed — check Python/yfinance");
                self->status_label_->setStyleSheet(
                    QString("color:%1; font-size:9px;").arg(ui::colors::NEGATIVE));
                LOG_ERROR("FFNView", "FFN script failed: " + result.error.left(300));
                return;
            }

            auto doc = QJsonDocument::fromJson(result.output.trimmed().toUtf8());
            if (!doc.isObject()) {
                self->status_label_->setText("FFN returned invalid JSON");
                self->status_label_->setStyleSheet(
                    QString("color:%1; font-size:9px;").arg(ui::colors::NEGATIVE));
                LOG_ERROR("FFNView", "FFN JSON parse failed");
                return;
            }

            self->ffn_data_ = doc.object();

            // Count only per-symbol keys (exclude the section keys)
            static const QStringList k_section_keys = {
                "rebased", "drawdown_series", "rolling_corr", "optimization", "error"
            };
            int sym_count = 0;
            for (const auto& k : self->ffn_data_.keys())
                if (!k_section_keys.contains(k))
                    ++sym_count;

            self->status_label_->setText(
                QString("FFN complete — %1 symbol%2")
                    .arg(sym_count)
                    .arg(sym_count != 1 ? "s" : ""));
            self->status_label_->setStyleSheet(
                QString("color:%1; font-size:9px;").arg(ui::colors::POSITIVE));

            LOG_INFO("FFNView",
                     QString("FFN analysis complete for %1 symbol(s)").arg(sym_count));

            self->update_overview();
            self->update_benchmark();
            self->update_optimization();
            self->update_rebased();
            self->update_drawdowns();
            self->update_rolling();

        }, Qt::QueuedConnection);
    });
}

} // namespace fincept::screens
