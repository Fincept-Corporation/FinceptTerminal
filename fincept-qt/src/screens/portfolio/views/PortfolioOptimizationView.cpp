// src/screens/portfolio/views/PortfolioOptimizationView.cpp
#include "screens/portfolio/views/PortfolioOptimizationView.h"

#include "core/logging/Logger.h"
#include "python/PythonRunner.h"
#include "ui/theme/Theme.h"

using fincept::python::PythonResult;
using fincept::python::PythonRunner;

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

PortfolioOptimizationView::PortfolioOptimizationView(QWidget* parent) : QWidget(parent) {
    build_ui();
}

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
    tabs_->addTab(build_backtest_tab(), "BACKTEST");
    tabs_->addTab(build_allocation_tab(), "ALLOCATION");
    tabs_->addTab(build_risk_tab(), "RISK");
    tabs_->addTab(build_strategies_tab(), "STRATEGIES");
    tabs_->addTab(build_stress_tab(), "STRESS");
    tabs_->addTab(build_compare_tab(), "COMPARE");
    tabs_->addTab(build_black_litterman_tab(), "B-L MODEL");

    layout->addWidget(tabs_);
}

// ── Helper: standard table style ─────────────────────────────────────────────

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

// ── Tab builders ─────────────────────────────────────────────────────────────

QWidget* PortfolioOptimizationView::build_optimize_tab() {
    auto* w = new QWidget;
    auto* layout = new QVBoxLayout(w);
    layout->setContentsMargins(12, 8, 12, 8);
    layout->setSpacing(8);

    // Config row
    auto* config = new QHBoxLayout;
    config->setSpacing(8);

    auto add_combo = [&](const QString& label, const QStringList& items) {
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
    run_btn_->setStyleSheet(
        QString("QPushButton { background:%1; color:#000; border:none;"
                "  padding:0 16px; font-size:10px; font-weight:700; }"
                "QPushButton:hover { background:%2; }"
                "QPushButton:disabled { background:%3; color:%4; }")
            .arg(ui::colors::AMBER, ui::colors::WARNING, ui::colors::BG_RAISED, ui::colors::TEXT_TERTIARY));
    connect(run_btn_, &QPushButton::clicked, this, &PortfolioOptimizationView::run_optimization);
    config->addWidget(run_btn_);

    layout->addLayout(config);

    status_label_ = new QLabel;
    status_label_->setStyleSheet(QString("color:%1; font-size:10px;").arg(ui::colors::TEXT_TERTIARY));
    layout->addWidget(status_label_);

    // Results table
    result_table_ = new QTableWidget;
    result_table_->setColumnCount(5);
    result_table_->setHorizontalHeaderLabels({"SYMBOL", "CURRENT WT%", "OPTIMAL WT%", "CHANGE", "ACTION"});
    style_table(result_table_);
    layout->addWidget(result_table_, 1);

    return w;
}

QWidget* PortfolioOptimizationView::build_frontier_tab() {
    auto* w = new QWidget;
    auto* layout = new QVBoxLayout(w);
    layout->setContentsMargins(12, 8, 12, 8);

    auto* title = new QLabel("EFFICIENT FRONTIER");
    title->setStyleSheet(
        QString("color:%1; font-size:11px; font-weight:700; letter-spacing:1px;").arg(ui::colors::AMBER));
    layout->addWidget(title);

    auto* chart = new QChart;
    chart->setBackgroundBrush(QColor(ui::colors::BG_BASE));
    chart->setMargins(QMargins(8, 8, 8, 8));
    chart->legend()->setVisible(false);
    chart->setTitle("");

    frontier_chart_ = new QChartView(chart);
    frontier_chart_->setRenderHint(QPainter::Antialiasing);
    frontier_chart_->setStyleSheet("border:none; background:transparent;");
    layout->addWidget(frontier_chart_, 1);

    auto* note = new QLabel("Run optimization on the OPTIMIZE tab to generate the efficient frontier curve.");
    note->setAlignment(Qt::AlignCenter);
    note->setStyleSheet(QString("color:%1; font-size:10px;").arg(ui::colors::TEXT_TERTIARY));
    layout->addWidget(note);

    return w;
}

QWidget* PortfolioOptimizationView::build_backtest_tab() {
    auto* w = new QWidget;
    auto* layout = new QVBoxLayout(w);
    layout->setContentsMargins(16, 12, 16, 12);
    layout->setAlignment(Qt::AlignCenter);

    auto* title = new QLabel("BACKTEST RESULTS");
    title->setStyleSheet(
        QString("color:%1; font-size:12px; font-weight:700; letter-spacing:1px;").arg(ui::colors::AMBER));
    title->setAlignment(Qt::AlignCenter);
    layout->addWidget(title);

    auto* desc = new QLabel("Run an optimization first, then backtest the optimal weights\n"
                            "against historical data to evaluate performance.");
    desc->setAlignment(Qt::AlignCenter);
    desc->setWordWrap(true);
    desc->setStyleSheet(QString("color:%1; font-size:11px;").arg(ui::colors::TEXT_TERTIARY));
    layout->addWidget(desc);

    return w;
}

QWidget* PortfolioOptimizationView::build_allocation_tab() {
    auto* w = new QWidget;
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
    alloc_table_->setHorizontalHeaderLabels({"SYMBOL", "WEIGHT", "VALUE", "CHANGE"});
    style_table(alloc_table_);
    layout->addWidget(alloc_table_, 1);

    return w;
}

QWidget* PortfolioOptimizationView::build_risk_tab() {
    auto* w = new QWidget;
    auto* layout = new QVBoxLayout(w);
    layout->setContentsMargins(16, 12, 16, 12);

    auto* title = new QLabel("RISK DECOMPOSITION");
    title->setStyleSheet(
        QString("color:%1; font-size:12px; font-weight:700; letter-spacing:1px;").arg(ui::colors::AMBER));
    layout->addWidget(title);

    auto* desc = new QLabel("Risk decomposition shows how each holding contributes to overall portfolio risk.\n"
                            "Run optimization to compute marginal risk contributions.");
    desc->setWordWrap(true);
    desc->setStyleSheet(QString("color:%1; font-size:11px;").arg(ui::colors::TEXT_TERTIARY));
    layout->addWidget(desc);
    layout->addStretch();

    return w;
}

QWidget* PortfolioOptimizationView::build_strategies_tab() {
    auto* w = new QWidget;
    auto* layout = new QVBoxLayout(w);
    layout->setContentsMargins(12, 8, 12, 8);
    layout->setSpacing(8);

    auto* title = new QLabel("PRE-BUILT STRATEGIES");
    title->setStyleSheet(
        QString("color:%1; font-size:11px; font-weight:700; letter-spacing:1px;").arg(ui::colors::AMBER));
    layout->addWidget(title);

    strategies_table_ = new QTableWidget;
    strategies_table_->setColumnCount(5);
    strategies_table_->setHorizontalHeaderLabels({"STRATEGY", "DESCRIPTION", "EXP. RETURN", "EXP. RISK", "SHARPE"});
    style_table(strategies_table_);
    layout->addWidget(strategies_table_, 1);

    return w;
}

QWidget* PortfolioOptimizationView::build_stress_tab() {
    auto* w = new QWidget;
    auto* layout = new QVBoxLayout(w);
    layout->setContentsMargins(16, 12, 16, 12);

    auto* title = new QLabel("OPTIMIZATION STRESS SCENARIOS");
    title->setStyleSheet(
        QString("color:%1; font-size:12px; font-weight:700; letter-spacing:1px;").arg(ui::colors::AMBER));
    layout->addWidget(title);

    auto* desc = new QLabel("Test how different optimization methods perform under stress conditions.\n"
                            "Compares optimal weights across crisis scenarios.");
    desc->setWordWrap(true);
    desc->setStyleSheet(QString("color:%1; font-size:11px;").arg(ui::colors::TEXT_TERTIARY));
    layout->addWidget(desc);
    layout->addStretch();

    return w;
}

QWidget* PortfolioOptimizationView::build_compare_tab() {
    auto* w = new QWidget;
    auto* layout = new QVBoxLayout(w);
    layout->setContentsMargins(16, 12, 16, 12);

    auto* title = new QLabel("METHOD COMPARISON");
    title->setStyleSheet(
        QString("color:%1; font-size:12px; font-weight:700; letter-spacing:1px;").arg(ui::colors::AMBER));
    layout->addWidget(title);

    auto* desc = new QLabel("Compare results across optimization methods:\n"
                            "Max Sharpe, Min Vol, Risk Parity, HRP, Equal Weight.\n"
                            "Run individual optimizations first to populate comparison data.");
    desc->setWordWrap(true);
    desc->setStyleSheet(QString("color:%1; font-size:11px;").arg(ui::colors::TEXT_TERTIARY));
    layout->addWidget(desc);
    layout->addStretch();

    return w;
}

QWidget* PortfolioOptimizationView::build_black_litterman_tab() {
    auto* w = new QWidget;
    auto* layout = new QVBoxLayout(w);
    layout->setContentsMargins(16, 12, 16, 12);

    auto* title = new QLabel("BLACK-LITTERMAN MODEL");
    title->setStyleSheet(
        QString("color:%1; font-size:12px; font-weight:700; letter-spacing:1px;").arg(ui::colors::AMBER));
    layout->addWidget(title);

    auto* desc = new QLabel("The Black-Litterman model combines market equilibrium with investor views\n"
                            "to produce more stable and intuitive portfolio allocations.\n\n"
                            "Configure your views on expected returns below, then run the model.");
    desc->setWordWrap(true);
    desc->setStyleSheet(QString("color:%1; font-size:11px;").arg(ui::colors::TEXT_TERTIARY));
    layout->addWidget(desc);
    layout->addStretch();

    return w;
}

// ── Data + optimization logic ────────────────────────────────────────────────

void PortfolioOptimizationView::set_data(const portfolio::PortfolioSummary& summary, const QString& currency) {
    summary_ = summary;
    currency_ = currency;
    update_allocation();
    update_strategies();
}

void PortfolioOptimizationView::run_optimization() {
    if (running_ || summary_.holdings.isEmpty())
        return;
    running_ = true;
    run_btn_->setEnabled(false);
    status_label_->setText("Running optimization...");
    status_label_->setStyleSheet(QString("color:%1; font-size:10px;").arg(ui::colors::AMBER));

    // Build symbols and weights for Python
    QStringList symbols;
    QStringList weights;
    for (const auto& h : summary_.holdings) {
        symbols.append(h.symbol);
        weights.append(QString::number(h.weight / 100.0, 'f', 4));
    }

    QString method = method_cb_->currentText().toLower().replace(" ", "_");

    QJsonObject args;
    args["symbols"] = QJsonArray::fromStringList(symbols);
    args["weights"] = QJsonArray::fromStringList(weights);
    args["method"] = method;
    args["returns_method"] = returns_cb_->currentText().toLower().replace(" ", "_");
    args["risk_model"] = risk_model_cb_->currentText().toLower().replace(" ", "_");

    QString script = "optimize_portfolio_weights";
    QString args_str = QJsonDocument(args).toJson(QJsonDocument::Compact);

    QPointer<PortfolioOptimizationView> self = this;

    PythonRunner::instance().run(script, {args_str}, [self](PythonResult result) {
        if (!self)
            return;
        QMetaObject::invokeMethod(
            self,
            [self, result]() {
                if (!self)
                    return;
                self->running_ = false;
                self->run_btn_->setEnabled(true);

                if (!result.success || result.output.trimmed().isEmpty()) {
                    self->status_label_->setText("Optimization failed. Check Python environment.");
                    self->status_label_->setStyleSheet(QString("color:%1; font-size:10px;").arg(ui::colors::NEGATIVE));
                    LOG_ERROR("PortfolioOpt", "Optimization failed: " + result.error.left(200));
                    return;
                }

                // Parse JSON result
                auto doc = QJsonDocument::fromJson(result.output.toUtf8());
                if (!doc.isObject()) {
                    self->status_label_->setText("Invalid response from optimizer.");
                    return;
                }

                auto root = doc.object();
                auto opt_weights = root["weights"].toObject();

                self->status_label_->setText(
                    QString("Optimization complete — %1 method").arg(self->method_cb_->currentText()));
                self->status_label_->setStyleSheet(QString("color:%1; font-size:10px;").arg(ui::colors::POSITIVE));

                // Populate result table
                self->result_table_->setRowCount(self->summary_.holdings.size());
                for (int r = 0; r < self->summary_.holdings.size(); ++r) {
                    const auto& h = self->summary_.holdings[r];
                    double current_w = h.weight;
                    double optimal_w = opt_weights.value(h.symbol).toDouble(current_w) * 100.0;
                    double change = optimal_w - current_w;

                    auto set = [&](int col, const QString& text, const char* color = nullptr) {
                        auto* item = new QTableWidgetItem(text);
                        item->setTextAlignment(col == 0 ? (Qt::AlignLeft | Qt::AlignVCenter)
                                                        : (Qt::AlignRight | Qt::AlignVCenter));
                        if (color)
                            item->setForeground(QColor(color));
                        self->result_table_->setItem(r, col, item);
                    };

                    set(0, h.symbol, ui::colors::CYAN);
                    set(1, QString("%1%").arg(QString::number(current_w, 'f', 1)));
                    set(2, QString("%1%").arg(QString::number(optimal_w, 'f', 1)), ui::colors::AMBER);
                    set(3, QString("%1%2%").arg(change >= 0 ? "+" : "").arg(QString::number(change, 'f', 1)),
                        change >= 0 ? ui::colors::POSITIVE : ui::colors::NEGATIVE);

                    QString action = std::abs(change) < 0.5 ? "HOLD" : change > 0 ? "INCREASE" : "DECREASE";
                    const char* action_color = action == "HOLD"       ? ui::colors::TEXT_TERTIARY
                                               : action == "INCREASE" ? ui::colors::POSITIVE
                                                                      : ui::colors::NEGATIVE;
                    set(4, action, action_color);
                }
            },
            Qt::QueuedConnection);
    });
}

void PortfolioOptimizationView::update_allocation() {
    if (summary_.holdings.isEmpty())
        return;

    // Update allocation donut
    auto* chart = alloc_chart_->chart();
    chart->removeAllSeries();

    auto* series = new QPieSeries;
    series->setHoleSize(0.5);

    static const QColor palette[] = {
        QColor("#0891b2"), QColor("#16a34a"), QColor("#d97706"), QColor("#9333ea"),
        QColor("#ca8a04"), QColor("#f43f5e"), QColor("#84cc16"), QColor("#60a5fa"),
    };

    auto sorted = summary_.holdings;
    std::sort(sorted.begin(), sorted.end(), [](const auto& a, const auto& b) { return a.weight > b.weight; });

    for (int i = 0; i < sorted.size(); ++i) {
        auto* slice = series->append(sorted[i].symbol, sorted[i].weight);
        slice->setColor(palette[i % 8]);
        slice->setBorderColor(QColor(ui::colors::BG_BASE));
        slice->setBorderWidth(1);
    }
    chart->addSeries(series);

    // Update table
    alloc_table_->setRowCount(sorted.size());
    for (int r = 0; r < sorted.size(); ++r) {
        const auto& h = sorted[r];
        alloc_table_->setRowHeight(r, 28);

        auto set = [&](int col, const QString& text, const char* color = nullptr) {
            auto* item = new QTableWidgetItem(text);
            item->setTextAlignment(col == 0 ? (Qt::AlignLeft | Qt::AlignVCenter) : (Qt::AlignRight | Qt::AlignVCenter));
            if (color)
                item->setForeground(QColor(color));
            alloc_table_->setItem(r, col, item);
        };

        set(0, h.symbol, ui::colors::CYAN);
        set(1, QString("%1%").arg(QString::number(h.weight, 'f', 1)), ui::colors::AMBER);
        set(2, QString("%1 %2").arg(currency_, QString::number(h.market_value, 'f', 2)));
        double eq_weight = 100.0 / summary_.holdings.size();
        double diff = h.weight - eq_weight;
        set(3, QString("%1%2%").arg(diff >= 0 ? "+" : "").arg(QString::number(diff, 'f', 1)),
            diff >= 0 ? ui::colors::POSITIVE : ui::colors::NEGATIVE);
    }
}

void PortfolioOptimizationView::update_strategies() {
    struct Strategy {
        const char* name;
        const char* desc;
        double exp_return;
        double exp_risk;
    };
    static const Strategy strategies[] = {
        {"Equal Weight", "Equal allocation across all holdings", 8.5, 15.2},
        {"Risk Parity", "Allocate inversely to volatility", 7.8, 11.5},
        {"Max Sharpe", "Maximize risk-adjusted returns", 12.3, 18.0},
        {"Min Volatility", "Minimize portfolio standard deviation", 6.2, 9.8},
        {"Max Diversification", "Maximize diversification ratio", 9.1, 13.5},
        {"Inverse Volatility", "Weight inversely to volatility", 8.0, 12.0},
        {"Max Decorrelation", "Minimize average correlation", 7.5, 11.8},
    };

    int n = sizeof(strategies) / sizeof(strategies[0]);
    strategies_table_->setRowCount(n);

    for (int r = 0; r < n; ++r) {
        const auto& s = strategies[r];
        strategies_table_->setRowHeight(r, 30);

        auto set = [&](int col, const QString& text, const char* color = nullptr) {
            auto* item = new QTableWidgetItem(text);
            item->setTextAlignment(col <= 1 ? (Qt::AlignLeft | Qt::AlignVCenter) : (Qt::AlignRight | Qt::AlignVCenter));
            if (color)
                item->setForeground(QColor(color));
            strategies_table_->setItem(r, col, item);
        };

        double sharpe = (s.exp_return - 4.0) / s.exp_risk; // risk-free = 4%

        set(0, s.name, ui::colors::TEXT_PRIMARY);
        set(1, s.desc, ui::colors::TEXT_SECONDARY);
        set(2, QString("%1%").arg(QString::number(s.exp_return, 'f', 1)), ui::colors::POSITIVE);
        set(3, QString("%1%").arg(QString::number(s.exp_risk, 'f', 1)), ui::colors::WARNING);
        set(4, QString::number(sharpe, 'f', 2), ui::colors::CYAN);
    }
}

} // namespace fincept::screens
