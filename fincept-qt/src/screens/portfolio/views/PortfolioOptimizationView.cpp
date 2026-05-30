// src/screens/portfolio/views/PortfolioOptimizationView.cpp
#include "screens/portfolio/views/PortfolioOptimizationView.h"

#include "core/events/EventBus.h"
#include "core/logging/Logger.h"
#include "services/backtesting/BacktestingService.h"
#include "services/portfolio/PortfolioAnalyticsService.h"
#include "storage/cache/CacheManager.h"
#include "ui/theme/Theme.h"

using fincept::services::AnalyticsResult;
using fincept::services::PortfolioAnalyticsService;

#define QT_CHARTS_USE_NAMESPACE
#include <QChart>
#include <QTabBar>
#include <QDate>
#include <QEvent>
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
                         .arg(ui::colors::BG_BASE(), ui::colors::TEXT_PRIMARY(), ui::colors::BORDER_DIM(),
                              ui::colors::BG_SURFACE(), ui::colors::TEXT_SECONDARY(), ui::colors::AMBER()));
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
    lbl->setStyleSheet(QString("color:%1; font-size:11px; padding:40px;").arg(ui::colors::TEXT_TERTIARY()));
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

    optimize_tab_index_ = tabs_->addTab(build_optimize_tab(), tr("OPTIMIZE"));
    frontier_tab_index_ = tabs_->addTab(build_frontier_tab(), tr("FRONTIER"));
    allocation_tab_index_ = tabs_->addTab(build_allocation_tab(), tr("ALLOCATION"));
    strategies_tab_index_ = tabs_->addTab(build_strategies_tab(), tr("STRATEGIES"));
    compare_tab_index_ = tabs_->addTab(build_compare_tab(), tr("COMPARE"));
    backtest_tab_index_ = tabs_->addTab(build_backtest_tab(), tr("BACKTEST"));
    risk_tab_index_ = tabs_->addTab(build_risk_tab(), tr("RISK"));
    stress_tab_index_ = tabs_->addTab(build_stress_tab(), tr("STRESS"));
    bl_tab_index_ = tabs_->addTab(build_black_litterman_tab(), tr("B-L MODEL"));

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

    auto add_combo = [&](const QString& label, QLabel*& label_slot, const QStringList& items) -> QComboBox* {
        label_slot = new QLabel(label);
        label_slot->setStyleSheet(QString("color:%1; font-size:9px; font-weight:700;").arg(ui::colors::TEXT_TERTIARY()));
        config->addWidget(label_slot);
        auto* cb = new QComboBox;
        cb->addItems(items);
        cb->setFixedHeight(24);
        cb->setStyleSheet(QString("QComboBox { background:%1; color:%2; border:1px solid %3;"
                                  "  padding:0 8px; font-size:10px; }"
                                  "QComboBox::drop-down { border:none; }"
                                  "QComboBox QAbstractItemView { background:%1; color:%2; }")
                              .arg(ui::colors::BG_RAISED(), ui::colors::TEXT_PRIMARY(), ui::colors::BORDER_DIM()));
        config->addWidget(cb);
        return cb;
    };

    // Method/returns/risk-model items are sent to Python \u2014 keep them in English
    // for the cache key and analytics service. Display labels are translated
    // via the combo's currentText only after look-up against the storage key.
    method_cb_ = add_combo(tr("METHOD:"), method_field_label_,
                           {"Max Sharpe", "Min Volatility", "Risk Parity", "Max Return", "Equal Weight", "HRP",
                            "Target Return", "B-L Model"});
    returns_cb_ = add_combo(tr("RETURNS:"), returns_field_label_,
                            {"Mean Historical", "EMA", "CAPM", "James-Stein"});
    risk_model_cb_ = add_combo(tr("RISK MODEL:"), risk_model_field_label_,
                               {"Sample Covariance", "Ledoit-Wolf", "Semicovariance", "Exponential"});

    config->addStretch();

    run_btn_ = new QPushButton(tr("\u25B6 RUN OPTIMIZATION"));
    run_btn_->setFixedHeight(26);
    run_btn_->setCursor(Qt::PointingHandCursor);
    run_btn_->setStyleSheet(QString("QPushButton { background:%1; color:%5; border:none;"
                                    "  padding:0 16px; font-size:10px; font-weight:700; }"
                                    "QPushButton:hover { background:%2; }"
                                    "QPushButton:disabled { background:%3; color:%4; }")
                                .arg(ui::colors::AMBER(), ui::colors::WARNING(), ui::colors::BG_RAISED(),
                                     ui::colors::TEXT_TERTIARY(), ui::colors::BG_BASE()));
    connect(run_btn_, &QPushButton::clicked, this, &PortfolioOptimizationView::run_optimization);
    config->addWidget(run_btn_);

    layout->addLayout(config);

    status_label_ = new QLabel;
    status_label_->setStyleSheet(QString("color:%1; font-size:10px;").arg(ui::colors::TEXT_TERTIARY()));
    layout->addWidget(status_label_);

    result_table_ = new QTableWidget;
    result_table_->setColumnCount(5);
    result_table_->setHorizontalHeaderLabels({tr("SYMBOL"), tr("CURRENT WT%"), tr("OPTIMAL WT%"), tr("CHANGE"), tr("ACTION")});
    style_table(result_table_);
    layout->addWidget(result_table_, 1);

    return w;
}

QWidget* PortfolioOptimizationView::build_frontier_tab() {
    auto* w = new QWidget(this);
    auto* layout = new QVBoxLayout(w);
    layout->setContentsMargins(12, 8, 12, 8);

    frontier_title_ = new QLabel(tr("EFFICIENT FRONTIER"));
    frontier_title_->setStyleSheet(
        QString("color:%1; font-size:11px; font-weight:700; letter-spacing:1px;").arg(ui::colors::AMBER()));
    layout->addWidget(frontier_title_);

    frontier_stack_ = new QStackedWidget;

    frontier_placeholder_ = make_placeholder(tr("Run optimization on the OPTIMIZE tab to generate the efficient frontier."));
    frontier_stack_->addWidget(frontier_placeholder_);

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
    alloc_table_->setHorizontalHeaderLabels({tr("SYMBOL"), tr("WEIGHT"), tr("VALUE"), tr("VS EQUAL WT")});
    style_table(alloc_table_);
    layout->addWidget(alloc_table_, 1);

    return w;
}

QWidget* PortfolioOptimizationView::build_strategies_tab() {
    auto* w = new QWidget(this);
    auto* layout = new QVBoxLayout(w);
    layout->setContentsMargins(12, 8, 12, 8);
    layout->setSpacing(8);

    strategies_title_ = new QLabel(tr("STRATEGY COMPARISON  (populated after optimization)"));
    strategies_title_->setStyleSheet(
        QString("color:%1; font-size:11px; font-weight:700; letter-spacing:1px;").arg(ui::colors::AMBER()));
    layout->addWidget(strategies_title_);

    strategies_stack_ = new QStackedWidget;
    strategies_placeholder_ = make_placeholder(tr("Run optimization on the OPTIMIZE tab.\n"
                                                  "All 5 strategies will be compared automatically."));
    strategies_stack_->addWidget(strategies_placeholder_);

    strategies_table_ = new QTableWidget;
    strategies_table_->setColumnCount(5);
    strategies_table_->setHorizontalHeaderLabels({tr("STRATEGY"), tr("EXP. RETURN"), tr("VOLATILITY"), tr("SHARPE"), tr("DESCRIPTION")});
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

    compare_title_ = new QLabel(tr("WEIGHT COMPARISON  (all methods, per symbol)"));
    compare_title_->setStyleSheet(
        QString("color:%1; font-size:11px; font-weight:700; letter-spacing:1px;").arg(ui::colors::AMBER()));
    layout->addWidget(compare_title_);

    compare_stack_ = new QStackedWidget;
    compare_placeholder_ = make_placeholder(tr("Run optimization on the OPTIMIZE tab to populate this comparison."));
    compare_stack_->addWidget(compare_placeholder_);

    compare_table_ = new QTableWidget;
    style_table(compare_table_);
    compare_stack_->addWidget(compare_table_);

    layout->addWidget(compare_stack_, 1);
    return w;
}

QWidget* PortfolioOptimizationView::build_backtest_tab() {
    auto* w = new QWidget(this);
    auto* outer = new QVBoxLayout(w);
    outer->setContentsMargins(0, 0, 0, 0);

    backtest_stack_ = new QStackedWidget(w);

    // ── Page 0: Buttons ──
    auto* buttons_page = new QWidget(backtest_stack_);
    auto* bl = new QVBoxLayout(buttons_page);
    bl->setContentsMargins(16, 12, 16, 12);
    bl->setSpacing(12);

    backtest_title_ = new QLabel(tr("BACKTEST PORTFOLIO"));
    backtest_title_->setStyleSheet(
        QString("color:%1; font-size:12px; font-weight:700; letter-spacing:1px;").arg(ui::colors::AMBER()));
    backtest_title_->setAlignment(Qt::AlignCenter);
    bl->addWidget(backtest_title_);

    backtest_body_ = new QLabel(tr("Run a buy-and-hold backtest on your portfolio to see\n"
                                   "historical performance, or open the full Backtesting terminal."));
    backtest_body_->setAlignment(Qt::AlignCenter);
    backtest_body_->setWordWrap(true);
    backtest_body_->setStyleSheet(QString("color:%1; font-size:11px; padding:8px 20px;").arg(ui::colors::TEXT_TERTIARY()));
    bl->addWidget(backtest_body_);

    auto btn_style = QString("QPushButton { background:%1; color:%2; border:1px solid %3; padding:8px 20px;"
                             "font-size:11px; font-weight:700; font-family:%4; letter-spacing:0.5px; }"
                             "QPushButton:hover { background:%5; color:%6; }"
                             "QPushButton:disabled { background:%7; color:%8; border-color:%7; }")
                         .arg(ui::colors::BG_RAISED(), ui::colors::AMBER(), ui::colors::AMBER_DIM())
                         .arg(ui::fonts::DATA_FAMILY)
                         .arg(ui::colors::AMBER(), ui::colors::BG_BASE())
                         .arg(ui::colors::BG_SURFACE(), ui::colors::TEXT_DIM());

    auto run_inline_backtest = [this](const QJsonArray& symbols, const QJsonArray& weights) {
        if (symbols.isEmpty()) return;
        backtest_status_label_->setText("RUNNING...");
        backtest_status_label_->setStyleSheet(
            QString("color:%1; font-size:11px; font-weight:700; padding:4px;").arg(ui::colors::WARNING()));
        backtest_metrics_table_->setRowCount(0);
        backtest_stack_->setCurrentIndex(1);

        QJsonObject args;
        args["symbols"] = symbols;
        args["weights"] = weights;
        args["startDate"] = QDate::currentDate().addYears(-1).toString("yyyy-MM-dd");
        args["endDate"] = QDate::currentDate().addDays(-1).toString("yyyy-MM-dd");
        args["initialCapital"] = summary_.total_market_value > 0 ? summary_.total_market_value : 100000.0;
        QJsonObject strategy;
        strategy["type"] = "buy_and_hold";
        strategy["params"] = QJsonObject{};
        args["strategy"] = strategy;

        auto& svc = fincept::services::backtest::BacktestingService::instance();
        svc.execute("vectorbt", "backtest", args);
    };

    backtest_current_btn_ = new QPushButton(tr("BACKTEST CURRENT WEIGHTS"), buttons_page);
    backtest_current_btn_->setCursor(Qt::PointingHandCursor);
    backtest_current_btn_->setStyleSheet(btn_style);
    bl->addWidget(backtest_current_btn_, 0, Qt::AlignCenter);

    connect(backtest_current_btn_, &QPushButton::clicked, this, [this, run_inline_backtest]() {
        if (summary_.holdings.isEmpty()) return;
        QJsonArray symbols, weights;
        for (const auto& h : summary_.holdings) {
            symbols.append(h.symbol);
            weights.append(h.weight / 100.0);
        }
        run_inline_backtest(symbols, weights);
    });

    backtest_optimal_btn_ = new QPushButton(tr("BACKTEST OPTIMAL WEIGHTS"), buttons_page);
    backtest_optimal_btn_->setCursor(Qt::PointingHandCursor);
    backtest_optimal_btn_->setStyleSheet(btn_style);
    backtest_optimal_btn_->setEnabled(false);
    backtest_optimal_btn_->setToolTip(tr("Run optimization first to enable"));
    bl->addWidget(backtest_optimal_btn_, 0, Qt::AlignCenter);

    connect(backtest_optimal_btn_, &QPushButton::clicked, this, [this, run_inline_backtest]() {
        if (!result_table_ || result_table_->rowCount() == 0) return;
        QJsonArray symbols, weights;
        for (int r = 0; r < result_table_->rowCount(); ++r) {
            auto* sym_item = result_table_->item(r, 0);
            auto* wt_item = result_table_->item(r, 2);
            if (!sym_item || !wt_item) continue;
            symbols.append(sym_item->text());
            QString wt_str = wt_item->text();
            wt_str.remove('%').remove(' ');
            weights.append(wt_str.toDouble() / 100.0);
        }
        run_inline_backtest(symbols, weights);
    });

    auto* open_terminal_btn = new QPushButton(tr("OPEN FULL BACKTESTING TERMINAL"), buttons_page);
    open_terminal_btn->setCursor(Qt::PointingHandCursor);
    open_terminal_btn->setStyleSheet(btn_style);
    bl->addWidget(open_terminal_btn, 0, Qt::AlignCenter);
    connect(open_terminal_btn, &QPushButton::clicked, this, [this]() {
        if (summary_.holdings.isEmpty()) return;
        QJsonArray symbols, weights;
        for (const auto& h : summary_.holdings) {
            symbols.append(h.symbol);
            weights.append(h.weight / 100.0);
        }
        QJsonObject config;
        config["symbols"] = symbols;
        config["weights"] = weights;
        config["initialCapital"] = summary_.total_market_value;
        fincept::services::backtest::BacktestingService::instance().set_pending_portfolio_config(config);
        fincept::EventBus::instance().publish("nav.switch_screen", {{"screen_id", QString("backtesting")}});
    });

    bl->addStretch();
    backtest_stack_->addWidget(buttons_page);

    // ── Page 1: Inline results ──
    auto* results_page = new QWidget(backtest_stack_);
    auto* rl = new QVBoxLayout(results_page);
    rl->setContentsMargins(16, 12, 16, 12);
    rl->setSpacing(8);

    backtest_status_label_ = new QLabel(tr("READY"), results_page);
    backtest_status_label_->setAlignment(Qt::AlignCenter);
    backtest_status_label_->setStyleSheet(
        QString("color:%1; font-size:12px; font-weight:700; letter-spacing:1px;").arg(ui::colors::AMBER()));
    rl->addWidget(backtest_status_label_);

    backtest_metrics_table_ = new QTableWidget(0, 2, results_page);
    backtest_metrics_table_->setHorizontalHeaderLabels({"Metric", "Value"});
    backtest_metrics_table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    backtest_metrics_table_->setAlternatingRowColors(true);
    backtest_metrics_table_->horizontalHeader()->setStretchLastSection(true);
    backtest_metrics_table_->verticalHeader()->setVisible(false);
    backtest_metrics_table_->setStyleSheet(
        QString("QTableWidget { background:%1; color:%2; gridline-color:%3; font-family:%4; font-size:%5px; border:none; }"
                "QTableWidget::item { padding:3px 8px; }"
                "QHeaderView::section { background:%6; color:%7; font-weight:700; padding:4px 8px;"
                "  border:1px solid %3; font-family:%4; font-size:%5px; }"
                "QTableWidget::item:alternate { background:%8; }")
            .arg(ui::colors::BG_SURFACE(), ui::colors::TEXT_PRIMARY(), ui::colors::BORDER_DIM())
            .arg(ui::fonts::DATA_FAMILY)
            .arg(ui::fonts::SMALL)
            .arg(ui::colors::BG_RAISED(), ui::colors::TEXT_SECONDARY(), ui::colors::ROW_ALT()));
    rl->addWidget(backtest_metrics_table_, 1);

    auto* back_btn = new QPushButton(tr("BACK"), results_page);
    back_btn->setCursor(Qt::PointingHandCursor);
    back_btn->setStyleSheet(btn_style);
    rl->addWidget(back_btn, 0, Qt::AlignCenter);
    connect(back_btn, &QPushButton::clicked, this, [this]() {
        backtest_stack_->setCurrentIndex(0);
    });

    backtest_stack_->addWidget(results_page);
    backtest_stack_->setCurrentIndex(0);

    // Connect BacktestingService results for inline display
    auto& svc = fincept::services::backtest::BacktestingService::instance();
    connect(&svc, &fincept::services::backtest::BacktestingService::result_ready, this,
            [this](const QString& /*provider*/, const QString& command, const QJsonObject& data) {
                if (command != "backtest" || backtest_stack_->currentIndex() != 1)
                    return;
                backtest_status_label_->setText("BUY & HOLD BACKTEST RESULTS");
                backtest_status_label_->setStyleSheet(
                    QString("color:%1; font-size:12px; font-weight:700; letter-spacing:1px;").arg(ui::colors::POSITIVE()));

                auto perf = data.value("performance").toObject();
                QStringList keys = {"totalReturn", "annualizedReturn", "sharpeRatio", "sortinoRatio",
                                    "maxDrawdown", "winRate", "profitFactor", "calmarRatio",
                                    "volatility", "totalTrades"};
                backtest_metrics_table_->setRowCount(keys.size());
                for (int r = 0; r < keys.size(); ++r) {
                    auto key = keys[r];
                    auto* name_item = new QTableWidgetItem(key.toUpper());
                    backtest_metrics_table_->setItem(r, 0, name_item);
                    auto val = perf.value(key);
                    QString display;
                    if (val.isDouble()) {
                        double v = val.toDouble();
                        if (key.contains("Return") || key.contains("Drawdown") || key.contains("Rate") || key.contains("Volatility"))
                            display = QString("%1%").arg(v * 100.0, 0, 'f', 2);
                        else
                            display = QString::number(v, 'f', 4);
                    } else {
                        display = val.toVariant().toString();
                    }
                    auto* val_item = new QTableWidgetItem(display);
                    val_item->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
                    backtest_metrics_table_->setItem(r, 1, val_item);
                }
                backtest_metrics_table_->resizeColumnsToContents();
            });

    connect(&svc, &fincept::services::backtest::BacktestingService::error_occurred, this,
            [this](const QString& /*ctx*/, const QString& message) {
                if (backtest_stack_->currentIndex() != 1) return;
                backtest_status_label_->setText("ERROR: " + message);
                backtest_status_label_->setStyleSheet(
                    QString("color:%1; font-size:11px; font-weight:700; padding:4px;").arg(ui::colors::NEGATIVE()));
            });

    outer->addWidget(backtest_stack_);
    return w;
}

QWidget* PortfolioOptimizationView::build_risk_tab() {
    auto* w = new QWidget(this);
    auto* layout = new QVBoxLayout(w);
    layout->setContentsMargins(16, 12, 16, 12);

    risk_title_ = new QLabel(tr("RISK DECOMPOSITION"));
    risk_title_->setStyleSheet(
        QString("color:%1; font-size:12px; font-weight:700; letter-spacing:1px;").arg(ui::colors::AMBER()));
    layout->addWidget(risk_title_);

    risk_stack_ = new QStackedWidget;
    risk_body_ = make_placeholder(tr("Marginal risk contribution decomposes total portfolio volatility across holdings.\n"
                                     "Run optimization on the OPTIMIZE tab to compute it from the covariance matrix."));
    risk_stack_->addWidget(risk_body_);

    risk_table_ = new QTableWidget;
    risk_table_->setColumnCount(5);
    risk_table_->setHorizontalHeaderLabels(
        {tr("SYMBOL"), tr("WEIGHT"), tr("ASSET VOL"), tr("MARGINAL RISK"), tr("RISK CONTRIB")});
    style_table(risk_table_);
    risk_stack_->addWidget(risk_table_);

    layout->addWidget(risk_stack_, 1);
    return w;
}

QWidget* PortfolioOptimizationView::build_stress_tab() {
    auto* w = new QWidget(this);
    auto* layout = new QVBoxLayout(w);
    layout->setContentsMargins(16, 12, 16, 12);

    stress_title_ = new QLabel(tr("STRESS SCENARIOS"));
    stress_title_->setStyleSheet(
        QString("color:%1; font-size:12px; font-weight:700; letter-spacing:1px;").arg(ui::colors::AMBER()));
    layout->addWidget(stress_title_);

    stress_stack_ = new QStackedWidget;
    stress_body_ = make_placeholder(tr("Applies historical crisis shocks to your current asset-class exposure.\n"
                                       "Select a portfolio to see estimated impact per scenario."));
    stress_stack_->addWidget(stress_body_);

    stress_table_ = new QTableWidget;
    stress_table_->setColumnCount(4);
    stress_table_->setHorizontalHeaderLabels(
        {tr("SCENARIO"), tr("DESCRIPTION"), tr("IMPACT"), tr("EST. LOSS")});
    style_table(stress_table_);
    stress_stack_->addWidget(stress_table_);

    layout->addWidget(stress_stack_, 1);
    return w;
}

QWidget* PortfolioOptimizationView::build_black_litterman_tab() {
    auto* w = new QWidget(this);
    auto* layout = new QVBoxLayout(w);
    layout->setContentsMargins(16, 12, 16, 12);

    bl_title_ = new QLabel(tr("BLACK-LITTERMAN MODEL"));
    bl_title_->setStyleSheet(
        QString("color:%1; font-size:12px; font-weight:700; letter-spacing:1px;").arg(ui::colors::AMBER()));
    layout->addWidget(bl_title_);

    bl_body_ = new QLabel(tr("Market-implied equilibrium returns (π = δ·Σ·w_market) shown below. "
                             "Select 'B-L Model' as the METHOD on the OPTIMIZE tab to also compute B-L weights."));
    bl_body_->setWordWrap(true);
    bl_body_->setStyleSheet(QString("color:%1; font-size:10px;").arg(ui::colors::TEXT_TERTIARY()));
    layout->addWidget(bl_body_);

    bl_stack_ = new QStackedWidget;
    auto* bl_placeholder = make_placeholder(tr("Run optimization on the OPTIMIZE tab to compute "
                                               "market-implied returns and Black-Litterman weights."));
    bl_stack_->addWidget(bl_placeholder);

    bl_table_ = new QTableWidget;
    bl_table_->setColumnCount(4);
    bl_table_->setHorizontalHeaderLabels(
        {tr("SYMBOL"), tr("IMPLIED RETURN"), tr("CURRENT WT"), tr("OPTIMIZED WT")});
    style_table(bl_table_);
    bl_stack_->addWidget(bl_table_);

    layout->addWidget(bl_stack_, 1);
    return w;
}

// ── Data ──────────────────────────────────────────────────────────────────────

void PortfolioOptimizationView::set_data(const portfolio::PortfolioSummary& summary, const QString& currency) {
    summary_ = summary;
    currency_ = currency;
    has_data_ = true;
    update_allocation();
    update_stress(); // stress test runs off current holdings — no optimization needed
}

void PortfolioOptimizationView::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange)
        retranslateUi();
    QWidget::changeEvent(event);
}

void PortfolioOptimizationView::retranslateUi() {
    if (tabs_) {
        if (optimize_tab_index_ >= 0)   tabs_->setTabText(optimize_tab_index_, tr("OPTIMIZE"));
        if (frontier_tab_index_ >= 0)   tabs_->setTabText(frontier_tab_index_, tr("FRONTIER"));
        if (allocation_tab_index_ >= 0) tabs_->setTabText(allocation_tab_index_, tr("ALLOCATION"));
        if (strategies_tab_index_ >= 0) tabs_->setTabText(strategies_tab_index_, tr("STRATEGIES"));
        if (compare_tab_index_ >= 0)    tabs_->setTabText(compare_tab_index_, tr("COMPARE"));
        if (backtest_tab_index_ >= 0)   tabs_->setTabText(backtest_tab_index_, tr("BACKTEST"));
        if (risk_tab_index_ >= 0)       tabs_->setTabText(risk_tab_index_, tr("RISK"));
        if (stress_tab_index_ >= 0)     tabs_->setTabText(stress_tab_index_, tr("STRESS"));
        if (bl_tab_index_ >= 0)         tabs_->setTabText(bl_tab_index_, tr("B-L MODEL"));
    }
    if (method_field_label_)     method_field_label_->setText(tr("METHOD:"));
    if (returns_field_label_)    returns_field_label_->setText(tr("RETURNS:"));
    if (risk_model_field_label_) risk_model_field_label_->setText(tr("RISK MODEL:"));
    if (run_btn_)                run_btn_->setText(tr("▶ RUN OPTIMIZATION"));

    if (result_table_)
        result_table_->setHorizontalHeaderLabels(
            {tr("SYMBOL"), tr("CURRENT WT%"), tr("OPTIMAL WT%"), tr("CHANGE"), tr("ACTION")});
    if (alloc_table_)
        alloc_table_->setHorizontalHeaderLabels({tr("SYMBOL"), tr("WEIGHT"), tr("VALUE"), tr("VS EQUAL WT")});
    if (strategies_table_)
        strategies_table_->setHorizontalHeaderLabels(
            {tr("STRATEGY"), tr("EXP. RETURN"), tr("VOLATILITY"), tr("SHARPE"), tr("DESCRIPTION")});

    if (frontier_title_)        frontier_title_->setText(tr("EFFICIENT FRONTIER"));
    if (frontier_placeholder_)
        frontier_placeholder_->setText(tr("Run optimization on the OPTIMIZE tab to generate the efficient frontier."));
    if (strategies_title_)      strategies_title_->setText(tr("STRATEGY COMPARISON  (populated after optimization)"));
    if (strategies_placeholder_)
        strategies_placeholder_->setText(tr("Run optimization on the OPTIMIZE tab.\n"
                                            "All 5 strategies will be compared automatically."));
    if (compare_title_)         compare_title_->setText(tr("WEIGHT COMPARISON  (all methods, per symbol)"));
    if (compare_placeholder_)
        compare_placeholder_->setText(tr("Run optimization on the OPTIMIZE tab to populate this comparison."));
    if (backtest_title_)        backtest_title_->setText(tr("BACKTEST RESULTS"));
    if (backtest_body_)
        backtest_body_->setText(tr("Run an optimization first, then backtest the optimal weights\n"
                                   "against historical data to evaluate out-of-sample performance."));
    if (risk_title_)            risk_title_->setText(tr("RISK DECOMPOSITION"));
    if (risk_body_)
        risk_body_->setText(tr("Marginal risk contribution decomposes total portfolio volatility across holdings.\n"
                               "Run optimization on the OPTIMIZE tab to compute it from the covariance matrix."));
    if (risk_table_)
        risk_table_->setHorizontalHeaderLabels(
            {tr("SYMBOL"), tr("WEIGHT"), tr("ASSET VOL"), tr("MARGINAL RISK"), tr("RISK CONTRIB")});
    if (stress_title_)          stress_title_->setText(tr("STRESS SCENARIOS"));
    if (stress_body_)
        stress_body_->setText(tr("Applies historical crisis shocks to your current asset-class exposure.\n"
                                 "Select a portfolio to see estimated impact per scenario."));
    if (stress_table_)
        stress_table_->setHorizontalHeaderLabels(
            {tr("SCENARIO"), tr("DESCRIPTION"), tr("IMPACT"), tr("EST. LOSS")});
    if (bl_title_)              bl_title_->setText(tr("BLACK-LITTERMAN MODEL"));
    if (bl_body_)
        bl_body_->setText(tr("Market-implied equilibrium returns (π = δ·Σ·w_market) shown below. "
                             "Select 'B-L Model' as the METHOD on the OPTIMIZE tab to also compute B-L weights."));
    if (bl_table_)
        bl_table_->setHorizontalHeaderLabels(
            {tr("SYMBOL"), tr("IMPLIED RETURN"), tr("CURRENT WT"), tr("OPTIMIZED WT")});

    // Combo items are English keys sent to Python (Max Sharpe, Risk Parity, etc.)
    // and intentionally not translated — they double as cache keys + the analytics
    // service contract. Combos remain untranslated. update_allocation() and
    // update_strategies/compare rebuild table content from data, no tr() literals
    // in row cells, so a re-render isn't needed for table contents.
    if (has_data_) {
        update_allocation();
        update_stress();
    }
}

// ── Optimization ──────────────────────────────────────────────────────────────

void PortfolioOptimizationView::run_optimization() {
    if (running_ || summary_.holdings.isEmpty())
        return;

    running_ = true;
    run_btn_->setEnabled(false);
    status_label_->setText(tr("Running optimization…"));
    status_label_->setStyleSheet(QString("color:%1; font-size:10px;").arg(ui::colors::AMBER()));

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
            const QJsonObject cached_root = cached_doc.object();
            QPointer<PortfolioOptimizationView> self = this;
            QMetaObject::invokeMethod(
                this,
                [self, cached_root, cache_key]() {
                    if (!self)
                        return;
                    self->running_ = false;
                    self->run_btn_->setEnabled(true);
                    const auto& root = cached_root;
                    const auto opt_weights = root["weights"].toObject();
                    const double ret = root["expected_annual_return"].toDouble();
                    const double vol = root["annual_volatility"].toDouble();
                    const double sharpe = root["sharpe_ratio"].toDouble();
                    self->status_label_->setText(QString("Done — %1 | Exp. Return: %2%  Vol: %3%  Sharpe: %4 (cached)")
                                                     .arg(self->method_cb_->currentText())
                                                     .arg(ret * 100.0, 0, 'f', 1)
                                                     .arg(vol * 100.0, 0, 'f', 1)
                                                     .arg(sharpe, 0, 'f', 2));
                    self->status_label_->setStyleSheet(QString("color:%1; font-size:10px;").arg(ui::colors::POSITIVE()));
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
                    self->update_risk(root);
                    self->update_black_litterman(root);
                    if (self->backtest_optimal_btn_)
                        self->backtest_optimal_btn_->setEnabled(true);
                },
                Qt::QueuedConnection);
            return;
        }
    }

    QPointer<PortfolioOptimizationView> self = this;

    PortfolioAnalyticsService::instance().optimize_weights(
        args_str, [self, cache_key](const AnalyticsResult& r) {
            if (!self)
                return;
            QMetaObject::invokeMethod(
                self,
                [self, r, cache_key]() {
                    if (!self)
                        return;
                    self->running_ = false;
                    self->run_btn_->setEnabled(true);

                    if (!r.success) {
                        self->status_label_->setText(
                            QString("Optimization: %1").arg(r.error));
                        self->status_label_->setStyleSheet(
                            QString("color:%1; font-size:10px;").arg(ui::colors::NEGATIVE()));
                        return;
                    }

                    const auto root = r.data;
                    const auto opt_weights = root["weights"].toObject();
                    const double ret = root["expected_annual_return"].toDouble();
                    const double vol = root["annual_volatility"].toDouble();
                    const double sharpe = root["sharpe_ratio"].toDouble();

                    // ── Status label ──────────────────────────────────────────
                    self->status_label_->setText(
                        QString("Done — %1 | Exp. Return: %2%  Vol: %3%  Sharpe: %4")
                            .arg(self->method_cb_->currentText())
                            .arg(ret * 100.0, 0, 'f', 1)
                            .arg(vol * 100.0, 0, 'f', 1)
                            .arg(sharpe, 0, 'f', 2));
                    self->status_label_->setStyleSheet(
                        QString("color:%1; font-size:10px;").arg(ui::colors::POSITIVE()));

                    // ── OPTIMIZE results table ────────────────────────────────
                    self->result_table_->setRowCount(
                        static_cast<int>(self->summary_.holdings.size()));
                    for (int row = 0; row < static_cast<int>(self->summary_.holdings.size()); ++row) {
                        const auto& h = self->summary_.holdings[row];
                        const double cw = h.weight;
                        const double ow = opt_weights.value(h.symbol).toDouble(cw / 100.0) * 100.0;
                        const double ch = ow - cw;

                        self->result_table_->setRowHeight(row, 28);

                        auto set = [&](int col, const QString& text,
                                       const char* color = nullptr) {
                            auto* item = new QTableWidgetItem(text);
                            item->setTextAlignment(col == 0 ? (Qt::AlignLeft | Qt::AlignVCenter)
                                                            : (Qt::AlignRight | Qt::AlignVCenter));
                            if (color)
                                item->setForeground(QColor(color));
                            self->result_table_->setItem(row, col, item);
                        };

                        set(0, h.symbol, ui::colors::CYAN);
                        set(1, QString("%1%").arg(cw, 0, 'f', 1));
                        set(2, QString("%1%").arg(ow, 0, 'f', 1), ui::colors::AMBER);
                        set(3, QString("%1%2%").arg(ch >= 0.0 ? "+" : "").arg(ch, 0, 'f', 1),
                            ch >= 0.0 ? ui::colors::POSITIVE : ui::colors::NEGATIVE);

                        const QString action = std::abs(ch) < 0.5 ? "HOLD"
                                               : ch > 0.0        ? "INCREASE"
                                                                 : "DECREASE";
                        const char* ac = action == "HOLD"       ? ui::colors::TEXT_TERTIARY
                                         : action == "INCREASE" ? ui::colors::POSITIVE
                                                                : ui::colors::NEGATIVE;
                        set(4, action, ac);
                    }

                    // ── Frontier, Strategies, Compare, Risk, B-L ──────────────
                    self->update_frontier(root["frontier"].toArray());
                    self->update_strategies(root["comparison"].toObject());
                    self->update_compare(root["comparison"].toObject());
                    self->update_risk(root);
                    self->update_black_litterman(root);
                    if (self->backtest_optimal_btn_)
                        self->backtest_optimal_btn_->setEnabled(true);

                    fincept::CacheManager::instance().put(
                        cache_key,
                        QVariant(QString::fromUtf8(
                            QJsonDocument(root).toJson(QJsonDocument::Compact))),
                        10 * 60, "portfolio_opt");
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

// ── Risk decomposition ──────────────────────────────────────────────────────────
// Renders the per-asset marginal risk contribution computed by the optimizer
// (risk_contributions / marginal_risk / asset_volatility from the Python output).
void PortfolioOptimizationView::update_risk(const QJsonObject& root) {
    const auto rc = root["risk_contributions"].toObject();
    const auto av = root["asset_volatility"].toObject();
    const auto mr = root["marginal_risk"].toObject();
    const auto weights = root["weights"].toObject();
    if (rc.isEmpty() || !risk_table_)
        return;

    struct Row {
        QString sym;
        double w, vol, mrc, rc;
    };
    QVector<Row> rows;
    for (auto it = rc.begin(); it != rc.end(); ++it) {
        const QString sym = it.key();
        rows.append({sym, weights.value(sym).toDouble() * 100.0, av.value(sym).toDouble() * 100.0,
                     mr.value(sym).toDouble(), it.value().toDouble()});
    }
    std::sort(rows.begin(), rows.end(), [](const Row& a, const Row& b) { return a.rc > b.rc; });

    risk_table_->setRowCount(rows.size());
    for (int r = 0; r < rows.size(); ++r) {
        const auto& row = rows[r];
        risk_table_->setRowHeight(r, 28);
        auto set = [&](int col, const QString& text, const char* color = nullptr) {
            auto* item = new QTableWidgetItem(text);
            item->setTextAlignment(col == 0 ? (Qt::AlignLeft | Qt::AlignVCenter) : (Qt::AlignRight | Qt::AlignVCenter));
            if (color)
                item->setForeground(QColor(color));
            risk_table_->setItem(r, col, item);
        };
        set(0, row.sym, ui::colors::CYAN);
        set(1, QString("%1%").arg(row.w, 0, 'f', 1));
        set(2, QString("%1%").arg(row.vol, 0, 'f', 1));
        set(3, QString::number(row.mrc, 'f', 3));
        // Flag assets whose share of risk exceeds their share of capital (risk concentration).
        const char* rc_col = row.rc > row.w + 0.5 ? ui::colors::NEGATIVE : ui::colors::AMBER;
        set(4, QString("%1%").arg(row.rc, 0, 'f', 1), rc_col);
    }
    risk_stack_->setCurrentIndex(1);
}

// ── Stress scenarios ────────────────────────────────────────────────────────────
// Applies historical crisis shocks to the portfolio's current asset-class mix.
// Runs off current holdings — no optimization required.
void PortfolioOptimizationView::update_stress() {
    if (summary_.holdings.isEmpty() || !stress_table_)
        return;

    struct StressScenario {
        const char* name;
        const char* desc;
        double equity_shock;    // %
        double bond_shock;      // %
        double commodity_shock; // %
    };
    static const StressScenario kScenarios[] = {
        {"2008 Financial Crisis", "Lehman collapse, credit freeze", -38.5, 5.2, -33.0},
        {"2020 COVID Crash", "Pandemic selloff (Feb–Mar 2020)", -33.9, 1.2, -20.0},
        {"Dot-com Bust", "Tech bubble burst (2000–2002)", -49.1, 8.5, -10.0},
        {"Interest Rate Shock +3%", "Aggressive Fed tightening", -15.0, -12.0, -5.0},
        {"Black Monday 1987", "Single-day market crash", -22.6, 2.0, -8.0},
        {"Inflation Surge", "1970s-style stagflation", -10.0, -15.0, 25.0},
        {"Geopolitical Crisis", "Major conflict escalation", -12.0, 3.0, 15.0},
        {"Currency Crisis", "Emerging-market contagion", -18.0, 1.0, 5.0},
    };

    // Classify current holdings into asset classes by symbol heuristics.
    double equity_wt = 0, bond_wt = 0, commodity_wt = 0, crypto_wt = 0;
    for (const auto& h : summary_.holdings) {
        const QString s = h.symbol.toUpper();
        const double wt = h.weight / 100.0;
        const bool is_crypto = s.endsWith("-USD") || s.endsWith("-USDT") || s.endsWith("USDT");
        const bool is_bond = s == "TLT" || s == "AGG" || s == "BND" || s == "IEF" || s == "SHY" || s == "LQD" ||
                             s == "HYG" || s.contains("TREAS") || s.contains("BOND");
        const bool is_comm = s == "GLD" || s == "IAU" || s == "SLV" || s == "USO" || s == "DBC" ||
                             s.contains("GOLD") || s.contains("OIL");
        if (is_crypto)
            crypto_wt += wt;
        else if (is_bond)
            bond_wt += wt;
        else if (is_comm)
            commodity_wt += wt;
        else
            equity_wt += wt;
    }

    const double total_mv = summary_.total_market_value;
    const int n = static_cast<int>(sizeof(kScenarios) / sizeof(kScenarios[0]));
    stress_table_->setRowCount(n);
    for (int i = 0; i < n; ++i) {
        const auto& sc = kScenarios[i];
        // Crypto treated as 1.5× equity beta to the shock.
        const double impact_pct = sc.equity_shock * equity_wt + sc.bond_shock * bond_wt +
                                  sc.commodity_shock * commodity_wt + sc.equity_shock * 1.5 * crypto_wt;
        const double loss = total_mv * impact_pct / 100.0;

        stress_table_->setRowHeight(i, 28);
        auto set = [&](int col, const QString& text, const char* color = nullptr) {
            auto* item = new QTableWidgetItem(text);
            item->setTextAlignment(col <= 1 ? (Qt::AlignLeft | Qt::AlignVCenter) : (Qt::AlignRight | Qt::AlignVCenter));
            if (color)
                item->setForeground(QColor(color));
            stress_table_->setItem(i, col, item);
        };
        set(0, tr(sc.name), ui::colors::TEXT_PRIMARY);
        set(1, tr(sc.desc), ui::colors::TEXT_TERTIARY);
        const char* ic = impact_pct >= 0 ? ui::colors::POSITIVE : ui::colors::NEGATIVE;
        set(2, QString("%1%2%").arg(impact_pct >= 0 ? "+" : "").arg(impact_pct, 0, 'f', 1), ic);
        set(3, QString("%1%2 %3").arg(loss >= 0 ? "+" : "-", currency_, QString::number(std::abs(loss), 'f', 0)), ic);
    }
    stress_stack_->setCurrentIndex(1);
}

// ── Black-Litterman ─────────────────────────────────────────────────────────────
// Shows market-implied equilibrium returns plus the optimized weights. When the
// B-L method was the one run, the final column is the Black-Litterman allocation.
void PortfolioOptimizationView::update_black_litterman(const QJsonObject& root) {
    const auto implied = root["implied_returns"].toObject();
    const auto opt_weights = root["weights"].toObject();
    if (implied.isEmpty() || !bl_table_)
        return;

    const bool is_bl = root["strategy"].toString() == "black_litterman";
    bl_table_->setHorizontalHeaderLabels(
        {tr("SYMBOL"), tr("IMPLIED RETURN"), tr("CURRENT WT"), is_bl ? tr("B-L WT") : tr("OPTIMIZED WT")});

    bl_table_->setRowCount(static_cast<int>(summary_.holdings.size()));
    for (int r = 0; r < static_cast<int>(summary_.holdings.size()); ++r) {
        const auto& h = summary_.holdings[r];
        bl_table_->setRowHeight(r, 28);
        auto set = [&](int col, const QString& text, const char* color = nullptr) {
            auto* item = new QTableWidgetItem(text);
            item->setTextAlignment(col == 0 ? (Qt::AlignLeft | Qt::AlignVCenter) : (Qt::AlignRight | Qt::AlignVCenter));
            if (color)
                item->setForeground(QColor(color));
            bl_table_->setItem(r, col, item);
        };
        const double imp = implied.value(h.symbol).toDouble() * 100.0;
        const double ow = opt_weights.value(h.symbol).toDouble(h.weight / 100.0) * 100.0;
        set(0, h.symbol, ui::colors::CYAN);
        set(1, QString("%1%2%").arg(imp >= 0 ? "+" : "").arg(imp, 0, 'f', 1),
            imp >= 0 ? ui::colors::POSITIVE : ui::colors::NEGATIVE);
        set(2, QString("%1%").arg(h.weight, 0, 'f', 1));
        set(3, QString("%1%").arg(ow, 0, 'f', 1), ui::colors::AMBER);
    }
    bl_stack_->setCurrentIndex(1);
}

} // namespace fincept::screens
