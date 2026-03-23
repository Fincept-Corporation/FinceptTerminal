// src/screens/portfolio/views/QuantStatsView.cpp
#include "screens/portfolio/views/QuantStatsView.h"
#include "core/logging/Logger.h"
#include "python/PythonRunner.h"
#include "ui/theme/Theme.h"

using fincept::python::PythonRunner;
using fincept::python::PythonResult;

#include <QGridLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPointer>
#include <QVBoxLayout>

#include <cmath>

namespace fincept::screens {

QuantStatsView::QuantStatsView(QWidget* parent) : QWidget(parent) {
    build_ui();
}

void QuantStatsView::build_ui() {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    tabs_ = new QTabWidget;
    tabs_->setDocumentMode(true);
    tabs_->setStyleSheet(QString(
        "QTabWidget::pane { border:0; background:%1; }"
        "QTabBar::tab { background:%2; color:%3; padding:6px 14px; border:0;"
        "  border-bottom:2px solid transparent; font-size:9px; font-weight:700;"
        "  letter-spacing:0.5px; }"
        "QTabBar::tab:selected { color:%4; border-bottom:2px solid %4; }"
        "QTabBar::tab:hover { color:%5; }")
        .arg(ui::colors::BG_BASE, ui::colors::BG_SURFACE, ui::colors::TEXT_SECONDARY,
             ui::colors::AMBER, ui::colors::TEXT_PRIMARY));

    // ── Metrics tab ──────────────────────────────────────────────────────────
    auto* metrics_w = new QWidget;
    auto* metrics_layout = new QVBoxLayout(metrics_w);
    metrics_layout->setContentsMargins(12, 8, 12, 8);

    auto* metrics_title = new QLabel("KEY PERFORMANCE INDICATORS");
    metrics_title->setStyleSheet(QString("color:%1; font-size:11px; font-weight:700; letter-spacing:1px;")
                                 .arg(ui::colors::AMBER));
    metrics_layout->addWidget(metrics_title);

    metrics_table_ = new QTableWidget;
    metrics_table_->setColumnCount(3);
    metrics_table_->setHorizontalHeaderLabels({"METRIC", "VALUE", "BENCHMARK"});
    metrics_table_->setSelectionMode(QAbstractItemView::NoSelection);
    metrics_table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    metrics_table_->setShowGrid(false);
    metrics_table_->verticalHeader()->setVisible(false);
    metrics_table_->horizontalHeader()->setStretchLastSection(true);
    metrics_table_->setColumnWidth(0, 200);
    metrics_table_->setColumnWidth(1, 120);
    metrics_table_->setStyleSheet(QString(
        "QTableWidget { background:%1; color:%2; border:none; font-size:11px; }"
        "QTableWidget::item { padding:4px 8px; border-bottom:1px solid %3; }"
        "QHeaderView::section { background:%4; color:%5; border:none;"
        "  border-bottom:2px solid %6; padding:4px 8px; font-size:9px;"
        "  font-weight:700; letter-spacing:0.5px; }")
        .arg(ui::colors::BG_BASE, ui::colors::TEXT_PRIMARY, ui::colors::BORDER_DIM,
             ui::colors::BG_SURFACE, ui::colors::TEXT_SECONDARY, ui::colors::AMBER));
    metrics_layout->addWidget(metrics_table_, 1);
    tabs_->addTab(metrics_w, "METRICS");

    // ── Returns tab ──────────────────────────────────────────────────────────
    returns_panel_ = new QWidget;
    returns_panel_->setStyleSheet(QString("background:%1;").arg(ui::colors::BG_BASE));
    tabs_->addTab(returns_panel_, "RETURNS");

    // ── Drawdown tab ─────────────────────────────────────────────────────────
    drawdown_panel_ = new QWidget;
    drawdown_panel_->setStyleSheet(QString("background:%1;").arg(ui::colors::BG_BASE));
    tabs_->addTab(drawdown_panel_, "DRAWDOWN");

    // ── Rolling tab ──────────────────────────────────────────────────────────
    rolling_panel_ = new QWidget;
    rolling_panel_->setStyleSheet(QString("background:%1;").arg(ui::colors::BG_BASE));
    tabs_->addTab(rolling_panel_, "ROLLING");

    // ── Monte Carlo tab ──────────────────────────────────────────────────────
    auto* mc_w = new QWidget;
    auto* mc_layout = new QVBoxLayout(mc_w);
    mc_layout->setContentsMargins(16, 12, 16, 12);
    mc_layout->setSpacing(8);

    auto* mc_title = new QLabel("MONTE CARLO SIMULATION");
    mc_title->setStyleSheet(QString("color:%1; font-size:12px; font-weight:700; letter-spacing:1px;")
                            .arg(ui::colors::AMBER));
    mc_layout->addWidget(mc_title);

    auto* mc_desc = new QLabel("Simulate 1,000+ portfolio return paths to estimate probability distributions\n"
                               "of future returns, drawdowns, and terminal wealth.");
    mc_desc->setWordWrap(true);
    mc_desc->setStyleSheet(QString("color:%1; font-size:11px;").arg(ui::colors::TEXT_TERTIARY));
    mc_layout->addWidget(mc_desc);

    mc_run_btn_ = new QPushButton("\u25B6 RUN MONTE CARLO (1000 paths)");
    mc_run_btn_->setFixedHeight(28);
    mc_run_btn_->setCursor(Qt::PointingHandCursor);
    mc_run_btn_->setStyleSheet(QString(
        "QPushButton { background:%1; color:#000; border:none;"
        "  padding:0 16px; font-size:10px; font-weight:700; }"
        "QPushButton:hover { background:%2; }"
        "QPushButton:disabled { background:%3; color:%4; }")
        .arg(ui::colors::AMBER, ui::colors::WARNING,
             ui::colors::BG_RAISED, ui::colors::TEXT_TERTIARY));
    connect(mc_run_btn_, &QPushButton::clicked, this, &QuantStatsView::run_monte_carlo);
    mc_layout->addWidget(mc_run_btn_);

    mc_status_ = new QLabel;
    mc_status_->setStyleSheet(QString("color:%1; font-size:10px;").arg(ui::colors::TEXT_TERTIARY));
    mc_layout->addWidget(mc_status_);

    mc_results_ = new QWidget;
    mc_results_->setStyleSheet(QString("background:%1;").arg(ui::colors::BG_BASE));
    mc_layout->addWidget(mc_results_, 1);

    tabs_->addTab(mc_w, "MONTE CARLO");

    layout->addWidget(tabs_);
}

void QuantStatsView::set_data(const portfolio::PortfolioSummary& summary,
                                const QString& currency) {
    summary_ = summary;
    currency_ = currency;
    update_metrics();
}

void QuantStatsView::update_metrics() {
    struct MetricDef {
        const char* name;
        double value;
        double benchmark;
        const char* format; // "pct", "ratio", "currency"
    };

    // Compute metrics from current data
    double pnl_pct = summary_.total_unrealized_pnl_percent;
    double day_chg = summary_.total_day_change_percent;

    double avg_vol = 0;
    int vol_n = 0;
    double neg_sq = 0;
    int neg_n = 0;
    for (const auto& h : summary_.holdings) {
        if (std::abs(h.day_change_percent) > 0.001) {
            avg_vol += std::abs(h.day_change_percent);
            ++vol_n;
            if (h.day_change_percent < 0) {
                neg_sq += h.day_change_percent * h.day_change_percent;
                ++neg_n;
            }
        }
    }
    double daily_vol = vol_n > 0 ? avg_vol / vol_n : 0;
    double ann_vol = daily_vol * std::sqrt(252.0);
    double rf = 4.0;
    double sharpe = ann_vol > 0.01 ? (pnl_pct - rf) / ann_vol : 0;
    double sortino = (neg_n > 0 && std::sqrt(neg_sq / neg_n) > 0.01)
        ? (pnl_pct - rf) / (std::sqrt(neg_sq / neg_n) * std::sqrt(252.0)) : 0;
    double max_dd = pnl_pct < 0 ? std::abs(pnl_pct) : 0;
    double calmar = max_dd > 0.1 ? pnl_pct / max_dd : 0;

    QVector<MetricDef> metrics = {
        {"Total Return",           pnl_pct,    10.2,  "pct"},
        {"Annualized Return",      pnl_pct,    10.2,  "pct"},
        {"Annualized Volatility",  ann_vol,    15.5,  "pct"},
        {"Sharpe Ratio",           sharpe,     0.85,  "ratio"},
        {"Sortino Ratio",          sortino,    1.10,  "ratio"},
        {"Calmar Ratio",           calmar,     0.65,  "ratio"},
        {"Max Drawdown",           max_dd,     -20.5, "pct"},
        {"Daily Value at Risk",    summary_.total_market_value * daily_vol * 1.645 / 100.0, 0, "currency"},
        {"Best Day",               day_chg > 0 ? day_chg : 0, 4.2,  "pct"},
        {"Worst Day",              day_chg < 0 ? day_chg : 0, -3.8, "pct"},
        {"Win Rate",               summary_.gainers > 0 ? summary_.gainers * 100.0 / summary_.total_positions : 0, 55.0, "pct"},
        {"Gain/Loss Ratio",        summary_.losers > 0 ? static_cast<double>(summary_.gainers) / summary_.losers : 0, 1.2, "ratio"},
        {"Positive Positions",     static_cast<double>(summary_.gainers), 0, "ratio"},
        {"Negative Positions",     static_cast<double>(summary_.losers),  0, "ratio"},
    };

    metrics_table_->setRowCount(metrics.size());

    for (int r = 0; r < metrics.size(); ++r) {
        const auto& m = metrics[r];
        metrics_table_->setRowHeight(r, 28);

        auto set = [&](int col, const QString& text, const char* color = nullptr) {
            auto* item = new QTableWidgetItem(text);
            item->setTextAlignment(col == 0 ? (Qt::AlignLeft | Qt::AlignVCenter)
                                            : (Qt::AlignRight | Qt::AlignVCenter));
            if (color) item->setForeground(QColor(color));
            metrics_table_->setItem(r, col, item);
        };

        set(0, m.name, ui::colors::TEXT_PRIMARY);

        auto fmt_val = [&](double v) -> QString {
            if (strcmp(m.format, "pct") == 0)
                return QString("%1%2%").arg(v >= 0 ? "+" : "").arg(QString::number(v, 'f', 2));
            if (strcmp(m.format, "currency") == 0)
                return QString("%1 %2").arg(currency_, QString::number(v, 'f', 2));
            return QString::number(v, 'f', 2);
        };

        const char* val_color = (strcmp(m.format, "pct") == 0 || strcmp(m.format, "ratio") == 0)
            ? (m.value >= 0 ? ui::colors::POSITIVE : ui::colors::NEGATIVE)
            : ui::colors::CYAN;

        set(1, fmt_val(m.value), val_color);
        set(2, m.benchmark != 0 ? fmt_val(m.benchmark) : "--", ui::colors::TEXT_TERTIARY);
    }
}

void QuantStatsView::run_quantstats() {
    // Placeholder for Python integration
    LOG_INFO("QuantStats", "QuantStats report requested");
}

void QuantStatsView::run_monte_carlo() {
    if (mc_running_ || summary_.holdings.isEmpty()) return;
    mc_running_ = true;
    mc_run_btn_->setEnabled(false);
    mc_status_->setText("Running 1000 simulation paths...");
    mc_status_->setStyleSheet(QString("color:%1; font-size:10px;").arg(ui::colors::AMBER));

    QStringList symbols;
    QStringList weights;
    for (const auto& h : summary_.holdings) {
        symbols.append(h.symbol);
        weights.append(QString::number(h.weight / 100.0, 'f', 4));
    }

    QJsonObject args;
    args["symbols"] = QJsonArray::fromStringList(symbols);
    args["weights"] = QJsonArray::fromStringList(weights);
    args["num_simulations"] = 1000;

    QString args_str = QJsonDocument(args).toJson(QJsonDocument::Compact);

    QPointer<QuantStatsView> self = this;

    PythonRunner::instance().run("quantstats_monte_carlo", {args_str},
        [self](PythonResult result) {
            if (!self) return;
            QMetaObject::invokeMethod(self, [self, result]() {
                if (!self) return;
                self->mc_running_ = false;
                self->mc_run_btn_->setEnabled(true);

                if (!result.success) {
                    self->mc_status_->setText("Monte Carlo simulation failed.");
                    self->mc_status_->setStyleSheet(
                        QString("color:%1; font-size:10px;").arg(ui::colors::NEGATIVE));
                    return;
                }

                self->mc_status_->setText("Monte Carlo complete — 1000 paths simulated");
                self->mc_status_->setStyleSheet(
                    QString("color:%1; font-size:10px;").arg(ui::colors::POSITIVE));

                // Parse and display results
                if (self->mc_results_->layout())
                    delete self->mc_results_->layout();

                auto* layout = new QVBoxLayout(self->mc_results_);
                layout->setContentsMargins(0, 8, 0, 0);

                auto doc = QJsonDocument::fromJson(result.output.toUtf8());
                if (doc.isObject()) {
                    auto root = doc.object();
                    auto add_result = [&](const QString& label, const QString& key, const char* color) {
                        auto* row = new QHBoxLayout;
                        auto* lbl = new QLabel(label);
                        lbl->setStyleSheet(QString("color:%1; font-size:10px;")
                                           .arg(ui::colors::TEXT_SECONDARY));
                        row->addWidget(lbl);
                        auto* val = new QLabel(root.contains(key)
                            ? QString::number(root[key].toDouble(), 'f', 2) + "%" : "--");
                        val->setAlignment(Qt::AlignRight);
                        val->setStyleSheet(QString("color:%1; font-size:12px; font-weight:700;").arg(color));
                        row->addWidget(val);
                        layout->addLayout(row);
                    };

                    add_result("Median Return (1Y)", "median_return", ui::colors::POSITIVE);
                    add_result("5th Percentile", "percentile_5", ui::colors::NEGATIVE);
                    add_result("95th Percentile", "percentile_95", ui::colors::POSITIVE);
                    add_result("Probability of Loss", "prob_loss", ui::colors::WARNING);
                    add_result("Expected Max Drawdown", "expected_max_dd", ui::colors::NEGATIVE);
                } else {
                    auto* raw = new QLabel(result.output.left(500));
                    raw->setWordWrap(true);
                    raw->setStyleSheet(QString("color:%1; font-size:10px;").arg(ui::colors::TEXT_SECONDARY));
                    layout->addWidget(raw);
                }

                layout->addStretch();
            }, Qt::QueuedConnection);
        });
}

} // namespace fincept::screens
