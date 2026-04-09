// src/screens/portfolio/views/PortfolioOptimizationView.cpp
#include "screens/portfolio/views/PortfolioOptimizationView.h"

#include "core/logging/Logger.h"
#include "python/PythonRunner.h"
#include "storage/cache/CacheManager.h"
#include "ui/theme/Theme.h"

using fincept::python::PythonResult;
using fincept::python::PythonRunner;

#define QT_CHARTS_USE_NAMESPACE
#include <QChart>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLineSeries>
#include <QPieSeries>
#include <QPointer>
#include <QScatterSeries>
#include <QVBoxLayout>
#include <QValueAxis>

#include <algorithm>
#include <cmath>

namespace fincept::screens {

// ── Helpers ───────────────────────────────────────────────────────────────────

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
                         .arg(ui::colors::BG_BASE, ui::colors::TEXT_PRIMARY, ui::colors::BORDER_DIM,
                              ui::colors::BG_SURFACE, ui::colors::TEXT_SECONDARY, ui::colors::AMBER));
}

static QChartView* make_chart_view(QChart* chart) {
    auto* cv = new QChartView(chart);
    cv->setRenderHint(QPainter::Antialiasing);
    cv->setStyleSheet("border:none; background:transparent;");
    return cv;
}

static QLabel* make_placeholder(const QString& text) {
    auto* lbl = new QLabel(text);
    lbl->setAlignment(Qt::AlignCenter);
    lbl->setWordWrap(true);
    lbl->setStyleSheet(QString("color:%1; font-size:11px; padding:40px;").arg(ui::colors::TEXT_TERTIARY));
    return lbl;
}

// ── Constructor ───────────────────────────────────────────────────────────────

PortfolioOptimizationView::PortfolioOptimizationView(QWidget* parent) : QWidget(parent) {
    build_ui();
}

// ── UI construction ───────────────────────────────────────────────────────────

void PortfolioOptimizationView::build_ui() {
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

    tabs_->addTab(build_optimize_tab(), "OPTIMIZE");
    tabs_->addTab(build_frontier_tab(), "FRONTIER");
    tabs_->addTab(build_allocation_tab(), "ALLOCATION");
    tabs_->addTab(build_strategies_tab(), "STRATEGIES");
    tabs_->addTab(build_compare_tab(), "COMPARE");
    tabs_->addTab(build_backtest_tab(), "BACKTEST");
    tabs_->addTab(build_risk_tab(), "RISK");
    tabs_->addTab(build_stress_tab(), "STRESS");
    tabs_->addTab(build_black_litterman_tab(), "B-L MODEL");

    layout->addWidget(tabs_);
}

QWidget* PortfolioOptimizationView::build_optimize_tab() {
    auto* w = new QWidget(this);
    auto* layout = new QVBoxLayout(w);
    layout->setContentsMargins(12, 8, 12, 8);
    layout->setSpacing(8);

    // Config row
    auto* config = new QHBoxLayout;
    config->setSpacing(8);

    auto add_combo = [&](const QString& label, const QStringList& items) -> QComboBox* {
        auto* lbl = new QLabel(label);
        lbl->setStyleSheet(QString("color:%1; font-size:9px; font-weight:700;").arg(ui::colors::TEXT_TERTIARY));
        config->addWidget(lbl);
        auto* cb = new QComboBox;
        cb->addItems(items);
        cb->setFixedHeight(24);
        cb->setStyleSheet(QString("QComboBox { background:%1; color:%2; border:1px solid %3;"
                                  "  padding:0 8px; font-size:10px; }"
                                  "QComboBox::drop-down { border:none; }"
                                  "QComboBox QAbstractItemView { background:%1; color:%2; }")
                              .arg(ui::colors::BG_RAISED, ui::colors::TEXT_PRIMARY, ui::colors::BORDER_DIM));
        config->addWidget(cb);
        return cb;
    };

    method_cb_ = add_combo("METHOD:", {"Max Sharpe", "Min Volatility", "Risk Parity", "Max Return", "Equal Weight",
                                       "HRP", "Target Return"});
    returns_cb_ = add_combo("RETURNS:", {"Mean Historical", "EMA", "CAPM", "James-Stein"});
    risk_model_cb_ = add_combo("RISK MODEL:", {"Sample Covariance", "Ledoit-Wolf", "Semicovariance", "Exponential"});

    config->addStretch();

    run_btn_ = new QPushButton("\u25B6 RUN OPTIMIZATION");
    run_btn_->setFixedHeight(26);
    run_btn_->setCursor(Qt::PointingHandCursor);
    run_btn_->setStyleSheet(QString("QPushButton { background:%1; color:%5; border:none;"
                                    "  padding:0 16px; font-size:10px; font-weight:700; }"
                                    "QPushButton:hover { background:%2; }"
                                    "QPushButton:disabled { background:%3; color:%4; }")
                                .arg(ui::colors::AMBER, ui::colors::WARNING, ui::colors::BG_RAISED,
                                     ui::colors::TEXT_TERTIARY, ui::colors::BG_BASE));
    connect(run_btn_, &QPushButton::clicked, this, &PortfolioOptimizationView::run_optimization);
    config->addWidget(run_btn_);

    layout->addLayout(config);

    status_label_ = new QLabel;
    status_label_->setStyleSheet(QString("color:%1; font-size:10px;").arg(ui::colors::TEXT_TERTIARY));
    layout->addWidget(status_label_);

    result_table_ = new QTableWidget;
    result_table_->setColumnCount(5);
    result_table_->setHorizontalHeaderLabels({"SYMBOL", "CURRENT WT%", "OPTIMAL WT%", "CHANGE", "ACTION"});
    style_table(result_table_);
    layout->addWidget(result_table_, 1);

    return w;
}

QWidget* PortfolioOptimizationView::build_frontier_tab() {
    auto* w = new QWidget(this);
    auto* layout = new QVBoxLayout(w);
    layout->setContentsMargins(12, 8, 12, 8);

    auto* title = new QLabel("EFFICIENT FRONTIER");
    title->setStyleSheet(
        QString("color:%1; font-size:11px; font-weight:700; letter-spacing:1px;").arg(ui::colors::AMBER));
    layout->addWidget(title);

    frontier_stack_ = new QStackedWidget;

    frontier_stack_->addWidget(
        make_placeholder("Run optimization on the OPTIMIZE tab to generate the efficient frontier."));

    // Chart page
    auto* chart_w = new QWidget(this);
    auto* chart_layout = new QVBoxLayout(chart_w);
    chart_layout->setContentsMargins(0, 0, 0, 0);

    auto* chart = new QChart;
    chart->setBackgroundBrush(QBrush(QColor(ui::colors::BG_BASE())));
    chart->setMargins(QMargins(8, 8, 8, 8));
    chart->legend()->setVisible(true);
    chart->legend()->setLabelColor(QColor(ui::colors::TEXT_SECONDARY()));
    chart->setAnimationOptions(QChart::NoAnimation);

    frontier_chart_ = make_chart_view(chart);
    chart_layout->addWidget(frontier_chart_);
    frontier_stack_->addWidget(chart_w);

    layout->addWidget(frontier_stack_, 1);
    return w;
}

QWidget* PortfolioOptimizationView::build_allocation_tab() {
    auto* w = new QWidget(this);
    auto* layout = new QHBoxLayout(w);
    layout->setContentsMargins(12, 8, 12, 8);
    layout->setSpacing(12);

    auto* chart = new QChart;
    chart->setBackgroundBrush(Qt::transparent);
    chart->setMargins(QMargins(0, 0, 0, 0));
    chart->legend()->setVisible(false);

    alloc_chart_ = new QChartView(chart);
    alloc_chart_->setRenderHint(QPainter::Antialiasing);
    alloc_chart_->setFixedSize(220, 220);
    alloc_chart_->setStyleSheet("border:none; background:transparent;");
    layout->addWidget(alloc_chart_);

    alloc_table_ = new QTableWidget;
    alloc_table_->setColumnCount(4);
    alloc_table_->setHorizontalHeaderLabels({"SYMBOL", "WEIGHT", "VALUE", "VS EQUAL WT"});
    style_table(alloc_table_);
    layout->addWidget(alloc_table_, 1);

    return w;
}

QWidget* PortfolioOptimizationView::build_strategies_tab() {
    auto* w = new QWidget(this);
    auto* layout = new QVBoxLayout(w);
    layout->setContentsMargins(12, 8, 12, 8);
    layout->setSpacing(8);

    auto* title = new QLabel("STRATEGY COMPARISON  (populated after optimization)");
    title->setStyleSheet(
        QString("color:%1; font-size:11px; font-weight:700; letter-spacing:1px;").arg(ui::colors::AMBER));
    layout->addWidget(title);

    strategies_stack_ = new QStackedWidget;
    strategies_stack_->addWidget(make_placeholder("Run optimization on the OPTIMIZE tab.\n"
                                                  "All 5 strategies will be compared automatically."));

    strategies_table_ = new QTableWidget;
    strategies_table_->setColumnCount(5);
    strategies_table_->setHorizontalHeaderLabels({"STRATEGY", "EXP. RETURN", "VOLATILITY", "SHARPE", "DESCRIPTION"});
    style_table(strategies_table_);
    strategies_stack_->addWidget(strategies_table_);

    layout->addWidget(strategies_stack_, 1);
    return w;
}

QWidget* PortfolioOptimizationView::build_compare_tab() {
    auto* w = new QWidget(this);
    auto* layout = new QVBoxLayout(w);
    layout->setContentsMargins(12, 8, 12, 8);
    layout->setSpacing(8);

    auto* title = new QLabel("WEIGHT COMPARISON  (all methods, per symbol)");
    title->setStyleSheet(
        QString("color:%1; font-size:11px; font-weight:700; letter-spacing:1px;").arg(ui::colors::AMBER));
    layout->addWidget(title);

    compare_stack_ = new QStackedWidget;
    compare_stack_->addWidget(make_placeholder("Run optimization on the OPTIMIZE tab to populate this comparison."));

    compare_table_ = new QTableWidget;
    style_table(compare_table_);
    compare_stack_->addWidget(compare_table_);

    layout->addWidget(compare_stack_, 1);
    return w;
}

QWidget* PortfolioOptimizationView::build_backtest_tab() {
    auto* w = new QWidget(this);
    auto* layout = new QVBoxLayout(w);
    layout->setContentsMargins(16, 12, 16, 12);
    layout->setAlignment(Qt::AlignCenter);

    auto* title = new QLabel("BACKTEST RESULTS");
    title->setStyleSheet(
        QString("color:%1; font-size:12px; font-weight:700; letter-spacing:1px;").arg(ui::colors::AMBER));
    title->setAlignment(Qt::AlignCenter);
    layout->addWidget(title);

    layout->addWidget(make_placeholder("Run an optimization first, then backtest the optimal weights\n"
                                       "against historical data to evaluate out-of-sample performance."));
    return w;
}

QWidget* PortfolioOptimizationView::build_risk_tab() {
    auto* w = new QWidget(this);
    auto* layout = new QVBoxLayout(w);
    layout->setContentsMargins(16, 12, 16, 12);

    auto* title = new QLabel("RISK DECOMPOSITION");
    title->setStyleSheet(
        QString("color:%1; font-size:12px; font-weight:700; letter-spacing:1px;").arg(ui::colors::AMBER));
    layout->addWidget(title);

    layout->addWidget(
        make_placeholder("Risk decomposition shows how each holding contributes to overall portfolio risk.\n"
                         "Run optimization to compute marginal risk contributions."));
    return w;
}

QWidget* PortfolioOptimizationView::build_stress_tab() {
    auto* w = new QWidget(this);
    auto* layout = new QVBoxLayout(w);
    layout->setContentsMargins(16, 12, 16, 12);

    auto* title = new QLabel("OPTIMIZATION STRESS SCENARIOS");
    title->setStyleSheet(
        QString("color:%1; font-size:12px; font-weight:700; letter-spacing:1px;").arg(ui::colors::AMBER));
    layout->addWidget(title);

    layout->addWidget(make_placeholder("Test how different optimization methods perform under stress conditions.\n"
                                       "Compares optimal weights across historical crisis scenarios."));
    return w;
}

QWidget* PortfolioOptimizationView::build_black_litterman_tab() {
    auto* w = new QWidget(this);
    auto* layout = new QVBoxLayout(w);
    layout->setContentsMargins(16, 12, 16, 12);

    auto* title = new QLabel("BLACK-LITTERMAN MODEL");
    title->setStyleSheet(
        QString("color:%1; font-size:12px; font-weight:700; letter-spacing:1px;").arg(ui::colors::AMBER));
    layout->addWidget(title);

    layout->addWidget(
        make_placeholder("The Black-Litterman model combines market equilibrium returns with investor views\n"
                         "to produce more stable and intuitive portfolio allocations.\n\n"
                         "Select 'B-L Model' from the METHOD dropdown on the OPTIMIZE tab to run it."));
    return w;
}

// ── Data ──────────────────────────────────────────────────────────────────────

void PortfolioOptimizationView::set_data(const portfolio::PortfolioSummary& summary, const QString& currency) {
    summary_ = summary;
    currency_ = currency;
    update_allocation();
}

// ── Optimization ──────────────────────────────────────────────────────────────

void PortfolioOptimizationView::run_optimization() {
    if (running_ || summary_.holdings.isEmpty())
        return;

    running_ = true;
    run_btn_->setEnabled(false);
    status_label_->setText("Running optimization…");
    status_label_->setStyleSheet(QString("color:%1; font-size:10px;").arg(ui::colors::AMBER));

    QJsonArray symbols_arr;
    QJsonArray weights_arr;
    for (const auto& h : summary_.holdings) {
        symbols_arr.append(h.symbol);
        weights_arr.append(h.weight / 100.0);
    }

    QJsonObject args;
    args["symbols"] = symbols_arr;
    args["weights"] = weights_arr;
    args["method"] = method_cb_->currentText().toLower().replace(" ", "_");
    args["returns_method"] = returns_cb_->currentText().toLower().replace(" ", "_");
    args["risk_model"] = risk_model_cb_->currentText().toLower().replace(" ", "_");

    const QString args_str = QJsonDocument(args).toJson(QJsonDocument::Compact);

    // Cache optimization results — same inputs always produce same output
    const QString cache_key = "portopt:" + args_str;
    const QVariant cached = fincept::CacheManager::instance().get(cache_key);
    if (!cached.isNull()) {
        auto cached_doc = QJsonDocument::fromJson(cached.toString().toUtf8());
        if (!cached_doc.isNull() && cached_doc.isObject()) {
            // Replay display logic with cached JSON as if it came from Python
            PythonResult fake_result;
            fake_result.success = true;
            fake_result.output = QString::fromUtf8(cached_doc.toJson(QJsonDocument::Compact));
            QPointer<PortfolioOptimizationView> self = this;
            QMetaObject::invokeMethod(
                this,
                [self, fake_result, cache_key]() {
                    if (!self)
                        return;
                    self->running_ = false;
                    self->run_btn_->setEnabled(true);
                    const auto doc = QJsonDocument::fromJson(fake_result.output.trimmed().toUtf8());
                    if (!doc.isObject())
                        return;
                    const auto root = doc.object();
                    const auto opt_weights = root["weights"].toObject();
                    const double ret = root["expected_annual_return"].toDouble();
                    const double vol = root["annual_volatility"].toDouble();
                    const double sharpe = root["sharpe_ratio"].toDouble();
                    self->status_label_->setText(QString("Done — %1 | Exp. Return: %2%  Vol: %3%  Sharpe: %4 (cached)")
                                                     .arg(self->method_cb_->currentText())
                                                     .arg(ret * 100.0, 0, 'f', 1)
                                                     .arg(vol * 100.0, 0, 'f', 1)
                                                     .arg(sharpe, 0, 'f', 2));
                    self->status_label_->setStyleSheet(QString("color:%1; font-size:10px;").arg(ui::colors::POSITIVE));
                    self->result_table_->setRowCount(static_cast<int>(self->summary_.holdings.size()));
                    for (int r = 0; r < static_cast<int>(self->summary_.holdings.size()); ++r) {
                        const auto& h = self->summary_.holdings[r];
                        const double cw = h.weight;
                        const double ow = opt_weights.value(h.symbol).toDouble(cw / 100.0) * 100.0;
                        const double ch = ow - cw;
                        self->result_table_->setRowHeight(r, 28);
                        auto set = [&](int col, const QString& text, const char* color = nullptr) {
                            auto* item = new QTableWidgetItem(text);
                            item->setTextAlignment(col == 0 ? (Qt::AlignLeft | Qt::AlignVCenter)
                                                            : (Qt::AlignRight | Qt::AlignVCenter));
                            if (color)
                                item->setForeground(QColor(color));
                            self->result_table_->setItem(r, col, item);
                        };
                        set(0, h.symbol, ui::colors::CYAN);
                        set(1, QString("%1%").arg(cw, 0, 'f', 1));
                        set(2, QString("%1%").arg(ow, 0, 'f', 1), ui::colors::AMBER);
                        set(3, QString("%1%2%").arg(ch >= 0.0 ? "+" : "").arg(ch, 0, 'f', 1),
                            ch >= 0.0 ? ui::colors::POSITIVE : ui::colors::NEGATIVE);
                        const QString action = std::abs(ch) < 0.5 ? "HOLD" : ch > 0.0 ? "INCREASE" : "DECREASE";
                        const char* ac = action == "HOLD"       ? ui::colors::TEXT_TERTIARY
                                         : action == "INCREASE" ? ui::colors::POSITIVE
                                                                : ui::colors::NEGATIVE;
                        set(4, action, ac);
                    }
                    self->update_frontier(root["frontier"].toArray());
                    self->update_strategies(root["comparison"].toObject());
                    self->update_compare(root["comparison"].toObject());
                },
                Qt::QueuedConnection);
            return;
        }
    }

    QPointer<PortfolioOptimizationView> self = this;

    PythonRunner::instance().run("optimize_portfolio_weights", {args_str}, [self, cache_key](PythonResult result) {
        if (!self)
            return;
        QMetaObject::invokeMethod(
            self,
            [self, result, cache_key]() {
                if (!self)
                    return;
                self->running_ = false;
                self->run_btn_->setEnabled(true);

                if (!result.success || result.output.trimmed().isEmpty()) {
                    self->status_label_->setText("Optimization failed — check Python environment.");
                    self->status_label_->setStyleSheet(QString("color:%1; font-size:10px;").arg(ui::colors::NEGATIVE));
                    LOG_ERROR("PortfolioOpt", "Optimization failed: " + result.error.left(300));
                    return;
                }

                const auto doc = QJsonDocument::fromJson(result.output.trimmed().toUtf8());
                if (!doc.isObject()) {
                    self->status_label_->setText("Invalid response from optimizer.");
                    self->status_label_->setStyleSheet(QString("color:%1; font-size:10px;").arg(ui::colors::NEGATIVE));
                    return;
                }

                const auto root = doc.object();
                const auto opt_weights = root["weights"].toObject();
                const double ret = root["expected_annual_return"].toDouble();
                const double vol = root["annual_volatility"].toDouble();
                const double sharpe = root["sharpe_ratio"].toDouble();

                // ── Status label ──────────────────────────────────────────────
                self->status_label_->setText(QString("Done — %1 | Exp. Return: %2%  Vol: %3%  Sharpe: %4")
                                                 .arg(self->method_cb_->currentText())
                                                 .arg(ret * 100.0, 0, 'f', 1)
                                                 .arg(vol * 100.0, 0, 'f', 1)
                                                 .arg(sharpe, 0, 'f', 2));
                self->status_label_->setStyleSheet(QString("color:%1; font-size:10px;").arg(ui::colors::POSITIVE));

                // ── OPTIMIZE results table ────────────────────────────────────
                self->result_table_->setRowCount(static_cast<int>(self->summary_.holdings.size()));
                for (int r = 0; r < static_cast<int>(self->summary_.holdings.size()); ++r) {
                    const auto& h = self->summary_.holdings[r];
                    const double cw = h.weight;
                    const double ow = opt_weights.value(h.symbol).toDouble(cw / 100.0) * 100.0;
                    const double ch = ow - cw;

                    self->result_table_->setRowHeight(r, 28);

                    auto set = [&](int col, const QString& text, const char* color = nullptr) {
                        auto* item = new QTableWidgetItem(text);
                        item->setTextAlignment(col == 0 ? (Qt::AlignLeft | Qt::AlignVCenter)
                                                        : (Qt::AlignRight | Qt::AlignVCenter));
                        if (color)
                            item->setForeground(QColor(color));
                        self->result_table_->setItem(r, col, item);
                    };

                    set(0, h.symbol, ui::colors::CYAN);
                    set(1, QString("%1%").arg(cw, 0, 'f', 1));
                    set(2, QString("%1%").arg(ow, 0, 'f', 1), ui::colors::AMBER);
                    set(3, QString("%1%2%").arg(ch >= 0.0 ? "+" : "").arg(ch, 0, 'f', 1),
                        ch >= 0.0 ? ui::colors::POSITIVE : ui::colors::NEGATIVE);

                    const QString action = std::abs(ch) < 0.5 ? "HOLD" : ch > 0.0 ? "INCREASE" : "DECREASE";
                    const char* ac = action == "HOLD"       ? ui::colors::TEXT_TERTIARY
                                     : action == "INCREASE" ? ui::colors::POSITIVE
                                                            : ui::colors::NEGATIVE;
                    set(4, action, ac);
                }

                // ── Frontier, Strategies, Compare ─────────────────────────────
                self->update_frontier(root["frontier"].toArray());
                self->update_strategies(root["comparison"].toObject());
                self->update_compare(root["comparison"].toObject());

                fincept::CacheManager::instance().put(
                    cache_key, QVariant(QString::fromUtf8(QJsonDocument(root).toJson(QJsonDocument::Compact))), 10 * 60,
                    "portfolio_opt");
            },
            Qt::QueuedConnection);
    });
}

// ── Allocation donut ──────────────────────────────────────────────────────────

void PortfolioOptimizationView::update_allocation() {
    if (summary_.holdings.isEmpty())
        return;

    auto* chart = alloc_chart_->chart();
    chart->removeAllSeries();

    auto* series = new QPieSeries;
    series->setHoleSize(0.5);

    static const QColor kPalette[] = {
        QColor("#0891b2"), QColor("#16a34a"), QColor("#d97706"), QColor("#9333ea"),
        QColor("#ca8a04"), QColor("#f43f5e"), QColor("#84cc16"), QColor("#60a5fa"),
    };

    auto sorted = summary_.holdings;
    std::sort(sorted.begin(), sorted.end(), [](const auto& a, const auto& b) { return a.weight > b.weight; });

    for (int i = 0; i < static_cast<int>(sorted.size()); ++i) {
        auto* slice = series->append(sorted[i].symbol, sorted[i].weight);
        slice->setColor(kPalette[i % 8]);
        slice->setBorderColor(QColor(ui::colors::BG_BASE()));
        slice->setBorderWidth(1);
    }
    chart->addSeries(series);

    alloc_table_->setRowCount(static_cast<int>(sorted.size()));
    const double eq_w = 100.0 / static_cast<double>(summary_.holdings.size());

    for (int r = 0; r < static_cast<int>(sorted.size()); ++r) {
        const auto& h = sorted[r];
        alloc_table_->setRowHeight(r, 28);
        const double diff = h.weight - eq_w;

        auto set = [&](int col, const QString& text, const char* color = nullptr) {
            auto* item = new QTableWidgetItem(text);
            item->setTextAlignment(col == 0 ? (Qt::AlignLeft | Qt::AlignVCenter) : (Qt::AlignRight | Qt::AlignVCenter));
            if (color)
                item->setForeground(QColor(color));
            alloc_table_->setItem(r, col, item);
        };

        set(0, h.symbol, ui::colors::CYAN);
        set(1, QString("%1%").arg(h.weight, 0, 'f', 1), ui::colors::AMBER);
        set(2, QString("%1 %2").arg(currency_, QString::number(h.market_value, 'f', 2)));
        set(3, QString("%1%2%").arg(diff >= 0.0 ? "+" : "").arg(diff, 0, 'f', 1),
            diff >= 0.0 ? ui::colors::POSITIVE : ui::colors::NEGATIVE);
    }
}

// ── Efficient frontier chart ──────────────────────────────────────────────────

void PortfolioOptimizationView::update_frontier(const QJsonArray& pts) {
    if (pts.isEmpty())
        return;

    auto* chart = frontier_chart_->chart();
    chart->removeAllSeries();
    const auto old_axes = chart->axes();
    for (auto* ax : old_axes)
        chart->removeAxis(ax);

    // Frontier line
    auto* frontier_series = new QLineSeries;
    frontier_series->setName("Efficient Frontier");
    frontier_series->setColor(QColor(QString(ui::colors::AMBER())));
    QPen fp{QColor(QString(ui::colors::AMBER()))};
    fp.setWidth(2);
    frontier_series->setPen(fp);

    // Sharpe-optimal point (max sharpe on frontier)
    auto* opt_series = new QScatterSeries;
    opt_series->setName("Optimal");
    opt_series->setColor(QColor(ui::colors::POSITIVE()));
    opt_series->setMarkerSize(10);

    double max_sharpe = -1e9;
    double opt_x = 0.0, opt_y = 0.0;

    double min_x = 1e9, max_x = 0.0, min_y = 1e9, max_y = 0.0;

    for (const auto& v : pts) {
        const auto obj = v.toObject();
        const double x = obj["volatility"].toDouble() * 100.0;
        const double y = obj["return"].toDouble() * 100.0;
        const double s = obj["sharpe"].toDouble();
        frontier_series->append(x, y);
        if (s > max_sharpe) {
            max_sharpe = s;
            opt_x = x;
            opt_y = y;
        }
        min_x = std::min(min_x, x);
        max_x = std::max(max_x, x);
        min_y = std::min(min_y, y);
        max_y = std::max(max_y, y);
    }

    opt_series->append(opt_x, opt_y);

    chart->addSeries(frontier_series);
    chart->addSeries(opt_series);

    auto* x_axis = new QValueAxis;
    x_axis->setTitleText("Volatility (%)");
    x_axis->setTitleBrush(QColor(ui::colors::TEXT_TERTIARY()));
    x_axis->setLabelsColor(QColor(ui::colors::TEXT_TERTIARY()));
    x_axis->setGridLineColor(QColor(ui::colors::BORDER_DIM()));
    x_axis->setLinePen(QPen(QColor(ui::colors::BORDER_DIM())));
    x_axis->setRange(std::max(0.0, min_x - 2.0), max_x + 2.0);
    x_axis->setTickCount(6);
    x_axis->setLabelFormat("%.0f%%");
    chart->addAxis(x_axis, Qt::AlignBottom);
    frontier_series->attachAxis(x_axis);
    opt_series->attachAxis(x_axis);

    auto* y_axis = new QValueAxis;
    y_axis->setTitleText("Expected Return (%)");
    y_axis->setTitleBrush(QColor(ui::colors::TEXT_TERTIARY()));
    y_axis->setLabelsColor(QColor(ui::colors::TEXT_TERTIARY()));
    y_axis->setGridLineColor(QColor(ui::colors::BORDER_DIM()));
    y_axis->setLinePen(QPen(QColor(ui::colors::BORDER_DIM())));
    y_axis->setRange(min_y - 2.0, max_y + 2.0);
    y_axis->setTickCount(6);
    y_axis->setLabelFormat("%.0f%%");
    chart->addAxis(y_axis, Qt::AlignLeft);
    frontier_series->attachAxis(y_axis);
    opt_series->attachAxis(y_axis);

    frontier_stack_->setCurrentIndex(1);
}

// ── Strategies comparison ─────────────────────────────────────────────────────

void PortfolioOptimizationView::update_strategies(const QJsonObject& comparison) {
    if (comparison.isEmpty())
        return;

    static const struct {
        const char* key;
        const char* label;
        const char* desc;
    } kRows[] = {
        {"max_sharpe", "Max Sharpe", "Maximise risk-adjusted return"},
        {"min_volatility", "Min Volatility", "Minimise portfolio standard deviation"},
        {"risk_parity", "Risk Parity", "Equal risk contribution per asset"},
        {"hrp", "HRP", "Hierarchical risk parity (inv-var)"},
        {"equal_weight", "Equal Weight", "Uniform 1/N allocation"},
    };

    const int n = static_cast<int>(sizeof(kRows) / sizeof(kRows[0]));
    strategies_table_->setRowCount(n);

    for (int r = 0; r < n; ++r) {
        const auto& row = kRows[r];
        strategies_table_->setRowHeight(r, 30);

        const auto obj = comparison.value(row.key).toObject();
        const double ret = obj["return"].toDouble() * 100.0;
        const double vol = obj["volatility"].toDouble() * 100.0;
        const double sharpe = obj["sharpe"].toDouble();

        auto set = [&](int col, const QString& text, const char* color = nullptr) {
            auto* item = new QTableWidgetItem(text);
            item->setTextAlignment(col == 0 || col == 4 ? (Qt::AlignLeft | Qt::AlignVCenter)
                                                        : (Qt::AlignRight | Qt::AlignVCenter));
            if (color)
                item->setForeground(QColor(color));
            strategies_table_->setItem(r, col, item);
        };

        set(0, row.label, ui::colors::TEXT_PRIMARY);
        set(1, obj.isEmpty() ? "--" : QString("%1%").arg(ret, 0, 'f', 1), ui::colors::POSITIVE);
        set(2, obj.isEmpty() ? "--" : QString("%1%").arg(vol, 0, 'f', 1), ui::colors::WARNING);
        set(3, obj.isEmpty() ? "--" : QString::number(sharpe, 'f', 2), ui::colors::CYAN);
        set(4, row.desc, ui::colors::TEXT_TERTIARY);
    }

    strategies_stack_->setCurrentIndex(1);
}

// ── Compare weights table ─────────────────────────────────────────────────────

void PortfolioOptimizationView::update_compare(const QJsonObject& comparison) {
    if (comparison.isEmpty() || summary_.holdings.isEmpty())
        return;

    static const struct {
        const char* key;
        const char* label;
    } kCols[] = {
        {"max_sharpe", "MAX SHARPE"}, {"min_volatility", "MIN VOL"}, {"risk_parity", "RISK PARITY"}, {"hrp", "HRP"},
        {"equal_weight", "EQUAL WT"},
    };
    const int n_methods = static_cast<int>(sizeof(kCols) / sizeof(kCols[0]));

    compare_table_->setRowCount(static_cast<int>(summary_.holdings.size()));
    compare_table_->setColumnCount(1 + n_methods);

    QStringList headers;
    headers << "SYMBOL";
    for (int c = 0; c < n_methods; ++c)
        headers << kCols[c].label;
    compare_table_->setHorizontalHeaderLabels(headers);

    for (int r = 0; r < static_cast<int>(summary_.holdings.size()); ++r) {
        const QString& sym = summary_.holdings[r].symbol;
        compare_table_->setRowHeight(r, 28);

        auto* sym_item = new QTableWidgetItem(sym);
        sym_item->setTextAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        sym_item->setForeground(QColor(ui::colors::CYAN()));
        compare_table_->setItem(r, 0, sym_item);

        for (int c = 0; c < n_methods; ++c) {
            const auto obj = comparison.value(kCols[c].key).toObject();
            const double w = obj.isEmpty() ? 0.0 : obj["weights"].toObject().value(sym).toDouble() * 100.0;

            auto* item = new QTableWidgetItem(obj.isEmpty() ? "--" : QString("%1%").arg(w, 0, 'f', 1));
            item->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
            if (!obj.isEmpty())
                item->setForeground(QColor(ui::colors::AMBER()));
            compare_table_->setItem(r, 1 + c, item);
        }
    }

    compare_stack_->setCurrentIndex(1);
}

} // namespace fincept::screens
