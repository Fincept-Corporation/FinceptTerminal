// src/screens/portfolio/PortfolioFFNView.cpp
#include "screens/portfolio/PortfolioFFNView.h"

#include "core/logging/Logger.h"
#include "python/PythonRunner.h"
#include "ui/theme/Theme.h"

#include <QHBoxLayout>
#include <QHeaderView>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>

using fincept::python::PythonResult;
using fincept::python::PythonRunner;

namespace fincept::screens {

PortfolioFFNView::PortfolioFFNView(QWidget* parent) : QWidget(parent) {
    build_ui();
}

void PortfolioFFNView::build_ui() {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    // Header with back button
    auto* header = new QWidget;
    header->setFixedHeight(36);
    header->setStyleSheet(
        QString("background:%1; border-bottom:1px solid %2;").arg(ui::colors::BG_SURFACE, ui::colors::AMBER));

    auto* h_layout = new QHBoxLayout(header);
    h_layout->setContentsMargins(8, 0, 8, 0);
    h_layout->setSpacing(8);

    back_btn_ = new QPushButton("\u2190 BACK");
    back_btn_->setFixedHeight(24);
    back_btn_->setCursor(Qt::PointingHandCursor);
    back_btn_->setStyleSheet(QString("QPushButton { background:transparent; color:%1; border:1px solid %1;"
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
        QString("color:%1; font-size:11px; font-weight:700; letter-spacing:1px;").arg(ui::colors::AMBER));
    h_layout->addWidget(title);

    h_layout->addStretch();

    status_label_ = new QLabel;
    status_label_->setStyleSheet(QString("color:%1; font-size:9px;").arg(ui::colors::TEXT_TERTIARY));
    h_layout->addWidget(status_label_);

    run_btn_ = new QPushButton("RUN FFN ANALYSIS");
    run_btn_->setFixedHeight(24);
    run_btn_->setCursor(Qt::PointingHandCursor);
    run_btn_->setStyleSheet(QString("QPushButton { background:%1; color:#000; border:none;"
                                    "  padding:0 12px; font-size:9px; font-weight:700; letter-spacing:0.5px; }"
                                    "QPushButton:hover { background:%2; }"
                                    "QPushButton:disabled { background:%3; color:%4; }")
                                .arg(ui::colors::POSITIVE, "#22c55e", ui::colors::BG_SURFACE,
                                     ui::colors::TEXT_TERTIARY));
    connect(run_btn_, &QPushButton::clicked, this, &PortfolioFFNView::run_ffn);
    h_layout->addWidget(run_btn_);

    layout->addWidget(header);

    // Tabs
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

    // ── Overview tab ─────────────────────────────────────────────────────────
    auto* overview_w = new QWidget;
    auto* overview_layout = new QVBoxLayout(overview_w);
    overview_layout->setContentsMargins(12, 8, 12, 8);

    auto* ov_title = new QLabel("PORTFOLIO METRICS OVERVIEW");
    ov_title->setStyleSheet(
        QString("color:%1; font-size:11px; font-weight:700; letter-spacing:1px;").arg(ui::colors::AMBER));
    overview_layout->addWidget(ov_title);

    overview_table_ = new QTableWidget;
    overview_table_->setColumnCount(3);
    overview_table_->setHorizontalHeaderLabels({"METRIC", "PORTFOLIO", "BENCHMARK (SPY)"});
    overview_table_->setSelectionMode(QAbstractItemView::NoSelection);
    overview_table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    overview_table_->setShowGrid(false);
    overview_table_->verticalHeader()->setVisible(false);
    overview_table_->horizontalHeader()->setStretchLastSection(true);
    overview_table_->setColumnWidth(0, 220);
    overview_table_->setColumnWidth(1, 140);
    overview_table_->setStyleSheet(
        QString("QTableWidget { background:%1; color:%2; border:none; font-size:11px; }"
                "QTableWidget::item { padding:3px 8px; border-bottom:1px solid %3; }"
                "QHeaderView::section { background:%4; color:%5; border:none;"
                "  border-bottom:2px solid %6; padding:3px 8px; font-size:9px; font-weight:700; }")
            .arg(ui::colors::BG_BASE, ui::colors::TEXT_PRIMARY, ui::colors::BORDER_DIM, ui::colors::BG_SURFACE,
                 ui::colors::TEXT_SECONDARY, ui::colors::AMBER));
    overview_layout->addWidget(overview_table_, 1);
    tabs_->addTab(overview_w, "OVERVIEW");

    // ── Benchmark tab ────────────────────────────────────────────────────────
    benchmark_panel_ = new QWidget;
    benchmark_panel_->setStyleSheet(QString("background:%1;").arg(ui::colors::BG_BASE));
    auto* bench_layout = new QVBoxLayout(benchmark_panel_);
    bench_layout->setContentsMargins(16, 12, 16, 12);
    bench_layout->setAlignment(Qt::AlignCenter);
    auto* bench_desc = new QLabel("BENCHMARK COMPARISON\n\nCompare portfolio performance against SPY benchmark.\n"
                                  "Requires Python FFN library for full analysis.");
    bench_desc->setAlignment(Qt::AlignCenter);
    bench_desc->setStyleSheet(QString("color:%1; font-size:11px;").arg(ui::colors::TEXT_TERTIARY));
    bench_layout->addWidget(bench_desc);
    tabs_->addTab(benchmark_panel_, "BENCHMARK");

    // ── Remaining tabs (placeholders) ────────────────────────────────────────
    auto make_placeholder = [this](const QString& tab_name, const QString& desc) {
        auto* w = new QWidget;
        auto* l = new QVBoxLayout(w);
        l->setAlignment(Qt::AlignCenter);
        auto* d = new QLabel(desc);
        d->setAlignment(Qt::AlignCenter);
        d->setWordWrap(true);
        d->setStyleSheet(QString("color:%1; font-size:11px;").arg(ui::colors::TEXT_TERTIARY));
        l->addWidget(d);
        tabs_->addTab(w, tab_name);
        return w;
    };

    optimization_panel_ = make_placeholder(
        "OPTIMIZATION",
        "EFFICIENT FRONTIER\n\nRun portfolio optimization using FFN to find\nthe optimal risk-return tradeoff.");
    rebased_panel_ = make_placeholder(
        "REBASED",
        "REBASED PRICE CHARTS\n\nCompare holdings on a common base (100)\nto visualize relative performance.");
    drawdowns_panel_ = make_placeholder(
        "DRAWDOWNS",
        "DRAWDOWN ANALYSIS\n\nVisualize historical drawdowns for each holding\nand the overall portfolio.");
    rolling_panel_ = make_placeholder(
        "ROLLING", "ROLLING CORRELATIONS\n\nTrack how correlations between holdings\nchange over rolling windows.");

    layout->addWidget(tabs_);
}

void PortfolioFFNView::set_data(const portfolio::PortfolioSummary& summary, const QString& currency) {
    summary_ = summary;
    currency_ = currency;
    update_overview();
}

void PortfolioFFNView::update_overview() {
    // Aggregate FFN data across all symbols (weighted by portfolio weight)
    // Fall back to live summary data if FFN hasn't been run yet
    bool has_ffn = !ffn_data_.isEmpty();

    double total_ann_ret = 0, total_ann_vol = 0, total_sharpe = 0, total_max_dd = 0;
    double total_best_day = 0, total_worst_day = 0;
    int pos_days = 0, neg_days = 0;
    int sym_count = 0;
    QString best_sym, worst_sym;
    double best_ret = -1e9, worst_ret = 1e9;

    if (has_ffn) {
        for (const auto& h : summary_.holdings) {
            if (!ffn_data_.contains(h.symbol))
                continue;
            auto s = ffn_data_[h.symbol].toObject();
            double w = h.weight / 100.0;
            total_ann_ret  += s["annualized_return"].toDouble()     * w;
            total_ann_vol  += s["annualized_volatility"].toDouble() * w;
            total_sharpe   += s["sharpe_ratio"].toDouble()          * w;
            total_max_dd   += s["max_drawdown"].toDouble()          * w;
            total_best_day  = std::max(total_best_day, s["best_day"].toDouble());
            total_worst_day = std::min(total_worst_day, s["worst_day"].toDouble());
            pos_days       += s["positive_days"].toInt();
            neg_days       += s["negative_days"].toInt();
            double sym_ret  = s["total_return"].toDouble();
            if (sym_ret > best_ret)  { best_ret  = sym_ret; best_sym  = h.symbol; }
            if (sym_ret < worst_ret) { worst_ret = sym_ret; worst_sym = h.symbol; }
            ++sym_count;
        }
    }

    double pnl_pct = summary_.total_unrealized_pnl_percent;
    double win_rate = summary_.total_positions > 0
                          ? summary_.gainers * 100.0 / summary_.total_positions : 0;

    struct Row { QString name; QString value; QString benchmark; const char* color; };
    QVector<Row> rows;

    auto pct_str = [](double v, int dp = 2) {
        return QString("%1%2%").arg(v >= 0 ? "+" : "").arg(QString::number(v * 100.0, 'f', dp));
    };
    auto fmt = [](double v, int dp = 2) { return QString::number(v, 'f', dp); };

    if (has_ffn) {
        rows = {
            {"Annualized Return",     pct_str(total_ann_ret),      "+10.50%", total_ann_ret  >= 0 ? ui::colors::POSITIVE : ui::colors::NEGATIVE},
            {"Annualized Volatility", pct_str(total_ann_vol),      "15.20%",  ui::colors::CYAN},
            {"Sharpe Ratio",          fmt(total_sharpe),           "0.82",    total_sharpe   >= 0 ? ui::colors::POSITIVE : ui::colors::NEGATIVE},
            {"Max Drawdown",          pct_str(total_max_dd),       "-20.50%", ui::colors::NEGATIVE},
            {"Best Day (any)",        pct_str(total_best_day),     "--",      ui::colors::POSITIVE},
            {"Worst Day (any)",       pct_str(total_worst_day),    "--",      ui::colors::NEGATIVE},
            {"Positive Days",         QString::number(pos_days),   "--",      ui::colors::POSITIVE},
            {"Negative Days",         QString::number(neg_days),   "--",      ui::colors::NEGATIVE},
            {"Best Holding",          best_sym.isEmpty()  ? "--" : best_sym  + " (" + pct_str(best_ret)  + ")", "--", ui::colors::POSITIVE},
            {"Worst Holding",         worst_sym.isEmpty() ? "--" : worst_sym + " (" + pct_str(worst_ret) + ")", "--", ui::colors::NEGATIVE},
            {"Total Return (cost)",   pct_str(pnl_pct / 100.0),   "--",      pnl_pct >= 0 ? ui::colors::POSITIVE : ui::colors::NEGATIVE},
            {"Win Rate",              fmt(win_rate) + "%",         "55.00%",  ui::colors::CYAN},
            {"Positions",             QString::number(summary_.total_positions), "--", ui::colors::CYAN},
            {"Total Value",           currency_ + " " + fmt(summary_.total_market_value), "--", ui::colors::WARNING},
            {"Cost Basis",            currency_ + " " + fmt(summary_.total_cost_basis),   "--", ui::colors::TEXT_SECONDARY},
        };
    } else {
        // Pre-run placeholder — show live data and prompt to run
        double vol = 0; int vn = 0;
        for (const auto& h : summary_.holdings)
            if (std::abs(h.day_change_percent) > 0.001) { vol += std::abs(h.day_change_percent); ++vn; }
        double daily_vol = vn > 0 ? vol / vn : 0;
        double ann_vol   = daily_vol * std::sqrt(252.0);
        double sharpe    = ann_vol > 0.01 ? (pnl_pct - 4.0) / ann_vol : 0;

        rows = {
            {"Total Return (unrealized)", pct_str(pnl_pct / 100.0), "+10.50%", pnl_pct >= 0 ? ui::colors::POSITIVE : ui::colors::NEGATIVE},
            {"Annualized Volatility (est.)", pct_str(ann_vol / 100.0), "15.20%", ui::colors::CYAN},
            {"Sharpe Ratio (est.)",          fmt(sharpe),              "0.82",   sharpe >= 0 ? ui::colors::POSITIVE : ui::colors::NEGATIVE},
            {"Win Rate",                     fmt(win_rate) + "%",      "55.00%", ui::colors::CYAN},
            {"Positions",                    QString::number(summary_.total_positions), "--", ui::colors::CYAN},
            {"Total Value",                  currency_ + " " + fmt(summary_.total_market_value), "--", ui::colors::WARNING},
            {"Cost Basis",                   currency_ + " " + fmt(summary_.total_cost_basis),   "--", ui::colors::TEXT_SECONDARY},
            {"FFN Deep Metrics",             "Click RUN FFN ANALYSIS for full stats", "--", ui::colors::AMBER},
        };
    }

    overview_table_->setRowCount(rows.size());
    for (int r = 0; r < rows.size(); ++r) {
        const auto& row = rows[r];
        overview_table_->setRowHeight(r, 28);

        auto set = [&](int col, const QString& text, const char* color) {
            auto* item = new QTableWidgetItem(text);
            item->setTextAlignment(col == 0 ? (Qt::AlignLeft | Qt::AlignVCenter)
                                            : (Qt::AlignRight | Qt::AlignVCenter));
            item->setForeground(QColor(color));
            overview_table_->setItem(r, col, item);
        };

        set(0, row.name,      ui::colors::TEXT_SECONDARY);
        set(1, row.value,     row.color);
        set(2, row.benchmark, ui::colors::TEXT_TERTIARY);
    }
}

void PortfolioFFNView::run_ffn() {
    if (summary_.holdings.isEmpty())
        return;

    run_btn_->setEnabled(false);
    status_label_->setText("Running FFN analysis...");
    status_label_->setStyleSheet(QString("color:%1; font-size:9px;").arg(ui::colors::AMBER));

    QStringList symbols;
    for (const auto& h : summary_.holdings)
        symbols.append(h.symbol);

    QJsonObject args;
    args["symbols"] = QJsonArray::fromStringList(symbols);
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
                return;
            }

            self->ffn_data_ = doc.object();
            self->status_label_->setText(
                QString("FFN complete — %1 symbols").arg(self->ffn_data_.keys().size()));
            self->status_label_->setStyleSheet(
                QString("color:%1; font-size:9px;").arg(ui::colors::POSITIVE));
            self->update_overview();
        }, Qt::QueuedConnection);
    });
}

} // namespace fincept::screens
