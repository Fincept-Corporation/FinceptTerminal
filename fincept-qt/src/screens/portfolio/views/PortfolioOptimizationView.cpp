// src/screens/portfolio/views/PortfolioOptimizationView.cpp
#include "screens/portfolio/views/PortfolioOptimizationView.h"

#include "core/events/EventBus.h"
#include "screens/portfolio/PortfolioSectorPanel.h"
#include "screens/portfolio/PortfolioSharedRun.h"
#include "screens/portfolio/PortfolioStress.h"
#include "services/backtesting/BacktestingService.h"
#include "services/portfolio/PortfolioAnalyticsService.h"
#include "services/portfolio/PortfolioService.h"
#include "ui/charts/BarChart.h"
#include "ui/charts/MatrixHeatmap.h"
#include "ui/charts/TimeSeriesChart.h"
#include "ui/formatting/DateFormat.h"
#include "ui/formatting/FormatWidgets.h"
#include "ui/formatting/NumberFormat.h"
#include "ui/theme/Theme.h"
#include "ui/widgets/AutoLoadPane.h"
#include "ui/widgets/AutoLoadPolicy.h"
#include "ui/widgets/EmptyState.h"
#include "ui/widgets/InputMemory.h"
#include "ui/widgets/KpiStrip.h"
#include "ui/widgets/LoadingSkeleton.h"
#include "ui/widgets/Requirements.h"
#include "ui/widgets/StatTile.h"

#include <QCursor>
#include <QLegendMarker>
#include <QSplitter>
#include <QTimeZone>
#include <QToolTip>
#include <functional>
#include <limits>
#include <memory>

using fincept::services::AnalyticsResult;
using fincept::services::PortfolioAnalyticsService;

#define QT_CHARTS_USE_NAMESPACE
#include <QChart>
#include <QDate>
#include <QEvent>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLineSeries>
#include <QPieSeries>
#include <QPointer>
#include <QScatterSeries>
#include <QSet>
#include <QTabBar>
#include <QVBoxLayout>
#include <QValueAxis>

#include <algorithm>
#include <cmath>
#include <iterator>

namespace fincept::screens {

// ── Helpers ───────────────────────────────────────────────────────────────────

namespace {

/// The view's one stylesheet: set on the tab widget before any page exists, so the
/// theme manager re-styles every page with it (no per-widget sheets, no run-time
/// recolouring).
QString opt_view_sheet() {
    QString s = QStringLiteral(
        "QTabWidget::pane { border:0; background:{BG_BASE}; }"
        "QTabBar::tab { background:{BG_SURFACE}; color:{TEXT_SECONDARY}; padding:6px 14px; border:0;"
        "  border-bottom:2px solid transparent; font-size:{F5}px; font-weight:700; letter-spacing:0.5px; }"
        "QTabBar::tab:selected { color:{AMBER}; border-bottom:2px solid {AMBER}; }"
        "QTabBar::tab:hover { color:{TEXT_PRIMARY}; }"
        "QChartView { border:none; background:transparent; }"
        "QLabel#poFieldLabel { color:{TEXT_SECONDARY}; font-size:{F5}px; font-weight:700; background:transparent; }"
        "QLabel#poTitle { color:{AMBER}; font-size:{F3}px; font-weight:700; letter-spacing:1px; background:transparent; }"
        "QLabel#poAsOf { color:{TEXT_TERTIARY}; font-size:{F5}px; background:transparent; }"
        "QLabel#poCaption { color:{TEXT_TERTIARY}; font-size:{F4}px; background:transparent; }"
        "QLabel#poNote { color:{TEXT_TERTIARY}; font-size:{F4}px; background:transparent; }"
        "QLabel#poWarn { background:{BG_RAISED}; color:{TEXT_PRIMARY}; border:none; border-left:3px solid {WARNING};"
        "  padding:4px 8px; font-size:{F4}px; }"
        "QComboBox#poCombo { background:{BG_RAISED}; color:{TEXT_PRIMARY}; border:1px solid {BORDER_DIM};"
        "  padding:0 8px; font-size:{F4}px; }"
        "QComboBox#poCombo:focus { border-color:{AMBER}; }"
        "QComboBox#poCombo::drop-down { border:none; }"
        "QComboBox#poCombo QAbstractItemView { background:{BG_RAISED}; color:{TEXT_PRIMARY}; }"
        "QDoubleSpinBox#poSpin { background:{BG_RAISED}; color:{TEXT_PRIMARY}; border:1px solid {BORDER_DIM};"
        "  padding:0 6px; font-size:{F4}px; }"
        "QDoubleSpinBox#poSpin:focus { border-color:{AMBER}; }"
        "QPushButton#poRun { background:{AMBER}; color:{TEXT_ON_ACCENT}; border:none; padding:0 16px;"
        "  font-size:{F4}px; font-weight:700; }"
        "QPushButton#poRun:hover { background:{WARNING}; }"
        "QPushButton#poRun:disabled { background:{BG_RAISED}; color:{TEXT_TERTIARY}; }"
        "QPushButton#poAction { background:{BG_RAISED}; color:{AMBER}; border:1px solid {AMBER_DIM}; padding:4px 14px;"
        "  font-size:{F4}px; font-weight:700; letter-spacing:0.5px; }"
        "QPushButton#poAction:hover { background:{AMBER}; color:{TEXT_ON_ACCENT}; }"
        "QPushButton#poAction:disabled { background:{BG_SURFACE}; color:{TEXT_DIM}; border-color:{BG_SURFACE}; }"
        "QTableWidget#poTable { background:{BG_BASE}; color:{TEXT_PRIMARY}; border:none; font-size:{F3}px; }"
        "QTableWidget#poTable::item { padding:4px 8px; border-bottom:1px solid {BORDER_DIM}; }"
        "QTableWidget#poTable QHeaderView::section { background:{BG_SURFACE}; color:{TEXT_SECONDARY}; border:none;"
        "  border-bottom:2px solid {AMBER}; padding:4px 8px; font-size:{F5}px; font-weight:700; letter-spacing:0.5px; }");
    const QList<QPair<QString, QString>> tokens = {
        {QStringLiteral("{BG_BASE}"), ui::colors::BG_BASE()},
        {QStringLiteral("{BG_SURFACE}"), ui::colors::BG_SURFACE()},
        {QStringLiteral("{BG_RAISED}"), ui::colors::BG_RAISED()},
        {QStringLiteral("{BORDER_DIM}"), ui::colors::BORDER_DIM()},
        {QStringLiteral("{TEXT_PRIMARY}"), ui::colors::TEXT_PRIMARY()},
        {QStringLiteral("{TEXT_SECONDARY}"), ui::colors::TEXT_SECONDARY()},
        {QStringLiteral("{TEXT_TERTIARY}"), ui::colors::TEXT_TERTIARY()},
        {QStringLiteral("{TEXT_DIM}"), ui::colors::TEXT_DIM()},
        {QStringLiteral("{TEXT_ON_ACCENT}"), ui::colors::TEXT_ON_ACCENT()},
        {QStringLiteral("{AMBER_DIM}"), ui::colors::AMBER_DIM()},
        {QStringLiteral("{AMBER}"), ui::colors::AMBER()},
        {QStringLiteral("{WARNING}"), ui::colors::WARNING()},
        {QStringLiteral("{F3}"), ui::fonts::px(-3)},
        {QStringLiteral("{F4}"), ui::fonts::px(-4)},
        {QStringLiteral("{F5}"), ui::fonts::px(-5)},
    };
    for (const auto& t : tokens)
        s.replace(t.first, t.second);
    return s;
}

void opt_style_table(QTableWidget* t) {
    t->setObjectName(QStringLiteral("poTable")); // styled by opt_view_sheet()
    t->setSelectionMode(QAbstractItemView::NoSelection);
    t->setEditTriggers(QAbstractItemView::NoEditTriggers);
    t->setShowGrid(false);
    t->verticalHeader()->setVisible(false);
    t->horizontalHeader()->setStretchLastSection(true);
}

QChartView* make_chart_view(QChart* chart, QWidget* parent) {
    auto* cv = new QChartView(chart, parent);
    cv->setRenderHint(QPainter::Antialiasing); // frame / background: opt_view_sheet()
    return cv;
}

// Table row height: `design_h` at the default font size, growing with it.
int opt_row_height(int design_h) {
    return std::max(design_h, ui::fonts::font_px(-3) + design_h - 11);
}

// The page stack inside `tabs` (QTabWidget's own child), so a tab page built there is not
// moved — and re-styled as a whole — by addTab(). `fallback` when it cannot be found.
QWidget* opt_page_parent(QTabWidget* tabs, QWidget* fallback) {
    if (!tabs)
        return fallback;
    QWidget* stack = tabs->findChild<QStackedWidget*>(QStringLiteral("qt_tabwidget_stackedwidget"),
                                                     Qt::FindDirectChildrenOnly);
    return stack ? stack : fallback;
}

/// Symbols the optimizer actually had price history for (empty = all).
QSet<QString> opt_priced(const QJsonObject& root) {
    QSet<QString> priced;
    for (const auto& v : root.value(QStringLiteral("symbols")).toArray())
        priced.insert(v.toString());
    return priced;
}

/// The five strategies every run compares (keys of the payload's "comparison").
struct OptStrategyRow {
    const char* key;
    const char* label;
    const char* desc;
};
const OptStrategyRow kOptStrategies[] = {
    {"max_sharpe", QT_TRANSLATE_NOOP("fincept::screens::PortfolioOptimizationView", "Max Sharpe"),
     QT_TRANSLATE_NOOP("fincept::screens::PortfolioOptimizationView", "Maximise risk-adjusted return")},
    {"min_volatility", QT_TRANSLATE_NOOP("fincept::screens::PortfolioOptimizationView", "Min Volatility"),
     QT_TRANSLATE_NOOP("fincept::screens::PortfolioOptimizationView", "Minimise portfolio standard deviation")},
    {"risk_parity", QT_TRANSLATE_NOOP("fincept::screens::PortfolioOptimizationView", "Risk Parity"),
     QT_TRANSLATE_NOOP("fincept::screens::PortfolioOptimizationView", "Equal risk contribution per asset")},
    {"hrp", QT_TRANSLATE_NOOP("fincept::screens::PortfolioOptimizationView", "HRP"),
     QT_TRANSLATE_NOOP("fincept::screens::PortfolioOptimizationView",
                       "Hierarchical risk parity (correlation clusters, recursive bisection)")},
    {"equal_weight", QT_TRANSLATE_NOOP("fincept::screens::PortfolioOptimizationView", "Equal Weight"),
     QT_TRANSLATE_NOOP("fincept::screens::PortfolioOptimizationView", "Uniform 1/N allocation")},
};
constexpr int kOptStrategyCount = static_cast<int>(sizeof(kOptStrategies) / sizeof(kOptStrategies[0]));

/// Display name of the payload's "strategy" key.
QString opt_strategy_name(const QString& key) {
    static const QHash<QString, QString> names = {
        {QStringLiteral("max_sharpe"), QStringLiteral("Max Sharpe")},
        {QStringLiteral("min_volatility"), QStringLiteral("Min Volatility")},
        {QStringLiteral("risk_parity"), QStringLiteral("Risk Parity")},
        {QStringLiteral("max_return"), QStringLiteral("Max Return")},
        {QStringLiteral("equal_weight"), QStringLiteral("Equal Weight")},
        {QStringLiteral("hrp"), QStringLiteral("HRP")},
        {QStringLiteral("target_return"), QStringLiteral("Target Return")},
        {QStringLiteral("black_litterman"), QStringLiteral("B-L Model")},
    };
    return names.value(key, key);
}

/// Keeps what the BACKTEST tab draws from a provider result (the full result,
/// with trades and logs, is ~10x larger and would not be remembered).
QJsonObject opt_backtest_payload(const QJsonObject& data, bool optimal, int years, const QString& start,
                                 const QString& end) {
    QJsonObject p;
    p.insert(QStringLiteral("performance"), data.value(QStringLiteral("performance")).toObject());
    QJsonArray eq;
    for (const auto& v : data.value(QStringLiteral("equity")).toArray()) {
        const QJsonObject o = v.toObject();
        QJsonObject row;
        row.insert(QStringLiteral("date"), o.value(QStringLiteral("date")).toString().left(10));
        row.insert(QStringLiteral("equity"), o.value(QStringLiteral("equity")));
        row.insert(QStringLiteral("drawdown"), o.value(QStringLiteral("drawdown")));
        eq.append(row);
    }
    p.insert(QStringLiteral("equity"), eq);
    p.insert(QStringLiteral("synthetic"), data.value(QStringLiteral("usingSyntheticData")).toBool(false));
    p.insert(QStringLiteral("source"), data.value(QStringLiteral("dataSource")).toString());
    p.insert(QStringLiteral("optimal"), optimal);
    p.insert(QStringLiteral("years"), years);
    p.insert(QStringLiteral("start"), start);
    p.insert(QStringLiteral("end"), end);
    return p;
}

} // namespace

// ── Constructor ───────────────────────────────────────────────────────────────

PortfolioOptimizationView::PortfolioOptimizationView(QWidget* parent) : QWidget(parent) {
    shared_ = new PortfolioSharedRun(this);
    shared_->set_launch([this](quint64 run_id) { launch_optimizer(run_id); });
    shared_->set_abort([this](quint64 run_id) { opt_requests_.take(run_id).cancel(); });
    build_ui();
    // Tables and charts carry their colours: re-render them once the new
    // appearance is in place (not inside theme_changed).
    connect(&ui::ThemeManager::instance(), &ui::ThemeManager::appearance_settled, this,
            &PortfolioOptimizationView::refresh_theme);
    // Currency / number format changed: the same re-render from the results on screen
    // (nothing re-runs).
    ui::formatting::on_format_changed(this, [this]() { refresh_theme(); });
}

PortfolioOptimizationView::~PortfolioOptimizationView() {
    // Stop what still runs for this view; the panes die after this body, when the
    // members their callbacks use are already gone — so detach those first.
    for (auto it = opt_requests_.cbegin(); it != opt_requests_.cend(); ++it)
        it.value().cancel();
    opt_requests_.clear();
    auto& bt = services::backtest::BacktestingService::instance();
    for (auto it = backtest_runs_.cbegin(); it != backtest_runs_.cend(); ++it)
        bt.cancel(it.key());
    backtest_runs_.clear();
    if (backtest_pane_)
        backtest_pane_->set_cancel({});
    if (shared_)
        shared_->set_abort({});
}

void PortfolioOptimizationView::refresh_theme() {
    if (frontier_chart_) {
        if (auto* chart = frontier_chart_->chart()) {
            chart->setBackgroundBrush(QBrush(QColor(ui::colors::BG_BASE())));
            chart->legend()->setLabelColor(QColor(ui::colors::TEXT_SECONDARY()));
        }
    }
    if (!has_data_)
        return;
    // Tables and charts carry their colours: rebuild them from the data on screen.
    update_allocation();
    update_stress();
    for (int t = 0; t < kResultTabCount; ++t)
        if (!rendered_[t].isEmpty())
            render_tab(static_cast<ResultTab>(t), rendered_[t]);
    if (!backtest_rendered_.isEmpty())
        render_backtest(backtest_rendered_);
}

// ── UI construction ───────────────────────────────────────────────────────────

void PortfolioOptimizationView::build_ui() {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    // Under its final parent from the start: built parentless, the whole tab widget (all
    // nine pages) was re-styled when layout->addWidget() below moved it into the view.
    tabs_ = new QTabWidget(this);
    tabs_->tabBar()->setElideMode(Qt::ElideNone);
    tabs_->tabBar()->setExpanding(false);
    tabs_->tabBar()->setUsesScrollButtons(true); // nine tabs overflow at 1280 px / large fonts
    tabs_->setDocumentMode(true);
    tabs_->setStyleSheet(opt_view_sheet()); // before any page exists

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

    // The optimiser inputs are remembered (and are what the next default run uses).
    opt_inputs_ = new ui::InputMemory(QStringLiteral("portfolio"), QStringLiteral("optimize"), this);
    opt_inputs_->track(QStringLiteral("method"), method_cb_);
    opt_inputs_->track(QStringLiteral("returns"), returns_cb_);
    opt_inputs_->track(QStringLiteral("risk_model"), risk_model_cb_);
    opt_inputs_->track(QStringLiteral("target"), target_ret_spin_);
    opt_inputs_->track(QStringLiteral("max_wt"), max_w_spin_);
    opt_inputs_->track(QStringLiteral("min_wt"), min_w_spin_);
    opt_inputs_->restore();
    // An edit marks the remembered result as made from other inputs ("Inputs changed — RUN AGAIN").
    connect(opt_inputs_, &ui::InputMemory::changed, this, &PortfolioOptimizationView::apply_view_key);

    backtest_inputs_ = new ui::InputMemory(QStringLiteral("portfolio"), QStringLiteral("opt_backtest"), this);
    backtest_inputs_->track(QStringLiteral("years"), backtest_period_cb_);
    backtest_inputs_->restore();
    connect(backtest_inputs_, &ui::InputMemory::changed, this, &PortfolioOptimizationView::apply_backtest_view);

    apply_state_texts();
}

QHBoxLayout* PortfolioOptimizationView::make_tab_header(QWidget* page, QLabel*& title, const QString& text,
                                                        ResultTab tab) {
    auto* row = new QHBoxLayout;
    row->setSpacing(8);
    title = new QLabel(text, page);
    title->setObjectName(QStringLiteral("poTitle"));
    row->addWidget(title);
    row->addStretch(1);
    auto* as_of = new QLabel(page);
    as_of->setObjectName(QStringLiteral("poAsOf"));
    row->addWidget(as_of);
    as_of_labels_[tab] = as_of;
    return row;
}

ui::AutoLoadPane* PortfolioOptimizationView::make_result_body(ResultTab tab, QWidget* page) {
    auto* body = new QStackedWidget(page);
    auto* pane = new ui::AutoLoadPane(ui::AutoLoadPane::Policy::AutoRun, body);
    body->addWidget(pane);
    auto* empty = new ui::EmptyState(body);
    connect(empty, &ui::EmptyState::primary_clicked, this, &PortfolioOptimizationView::add_position_requested);
    body->addWidget(empty);
    body->setCurrentWidget(pane);

    pane->set_cache_key(QStringLiteral("portfolio/opt"), kPortfolioResultTtlDays);
    pane->set_max_age(kPortfolioScriptMaxAgeS);
    pane->set_timeout(150);
    pane->set_requirements({ui::Requirement::python()});
    if (as_of_labels_[tab])
        pane->set_status_label(as_of_labels_[tab]);
    pane->set_render([this, tab](const QJsonValue& payload, const QDateTime&, bool) {
        render_tab(tab, payload.toObject());
    });
    shared_->attach(pane);

    result_panes_[tab] = pane;
    result_bodies_[tab] = body;
    result_empty_[tab] = empty;
    return pane;
}

QWidget* PortfolioOptimizationView::build_optimize_tab() {
    auto* w = new QWidget(opt_page_parent(tabs_, this)); // built where addTab() keeps it
    auto* layout = new QVBoxLayout(w);
    layout->setContentsMargins(12, 8, 12, 8);
    layout->setSpacing(6);

    // Inputs in two rows — method and models, then bounds and RUN — so the row fits
    // at 1280 px with large fonts (the single row pushed RUN off-screen).
    auto* row1 = new QHBoxLayout;
    row1->setSpacing(8);
    auto* row2 = new QHBoxLayout;
    row2->setSpacing(8);

    auto add_combo = [&](QHBoxLayout* row, const QString& label, QLabel*& label_slot,
                         const QStringList& items) -> QComboBox* {
        label_slot = new QLabel(label, w);
        label_slot->setObjectName(QStringLiteral("poFieldLabel"));
        row->addWidget(label_slot);
        auto* cb = new QComboBox(w);
        cb->setObjectName(QStringLiteral("poCombo"));
        cb->addItems(items);
        cb->setMinimumHeight(24);
        row->addWidget(cb);
        return cb;
    };

    // Method/returns/risk-model items are sent to Python — keep them in English: they
    // are the analytics service contract and part of the remembered inputs.
    method_cb_ = add_combo(row1, tr("METHOD:"), method_field_label_,
                           {"Max Sharpe", "Min Volatility", "Risk Parity", "Max Return", "Equal Weight", "HRP",
                            "Target Return", "B-L Model"});
    returns_cb_ = add_combo(row1, tr("RETURNS:"), returns_field_label_,
                            {"Mean Historical", "EMA", "CAPM", "James-Stein"});
    risk_model_cb_ = add_combo(row1, tr("RISK MODEL:"), risk_model_field_label_,
                               {"Sample Covariance", "Ledoit-Wolf", "Semicovariance", "Exponential"});

    // Target Return needs a target — the script supports one ("target_return").
    // Only visible for that method.
    target_ret_label_ = new QLabel(tr("TARGET RETURN:"), w);
    target_ret_label_->setObjectName(QStringLiteral("poFieldLabel"));
    row1->addWidget(target_ret_label_);
    target_ret_spin_ = new QDoubleSpinBox(w);
    target_ret_spin_->setObjectName(QStringLiteral("poSpin"));
    target_ret_spin_->setRange(0.0, 100.0);
    target_ret_spin_->setDecimals(1);
    target_ret_spin_->setSingleStep(0.5);
    target_ret_spin_->setSuffix(QStringLiteral(" %"));
    target_ret_spin_->setValue(10.0);
    target_ret_spin_->setMinimumHeight(24);
    row1->addWidget(target_ret_spin_);
    row1->addStretch(1);
    target_ret_label_->setVisible(false);
    target_ret_spin_->setVisible(false);
    connect(method_cb_, &QComboBox::currentTextChanged, this, [this](const QString& m) {
        const bool is_target = (m == QLatin1String("Target Return"));
        target_ret_label_->setVisible(is_target);
        target_ret_spin_->setVisible(is_target);
    });

    // Per-position bounds.
    auto add_bound = [&](QLabel*& label_slot, const QString& label, double lo, double hi, double value,
                         const QString& tip) {
        label_slot = new QLabel(label, w);
        label_slot->setObjectName(QStringLiteral("poFieldLabel"));
        row2->addWidget(label_slot);
        auto* s = new QDoubleSpinBox(w);
        s->setObjectName(QStringLiteral("poSpin"));
        s->setRange(lo, hi);
        s->setDecimals(1);
        s->setSingleStep(1.0);
        s->setSuffix(QStringLiteral(" %"));
        s->setValue(value);
        s->setToolTip(tip);
        s->setMinimumHeight(24);
        row2->addWidget(s);
        return s;
    };
    max_w_spin_ = add_bound(max_w_label_, tr("MAX WT:"), 1.0, 100.0, 100.0,
                            tr("Largest weight any one position may get (100 % = no cap)"));
    min_w_spin_ = add_bound(min_w_label_, tr("MIN WT:"), 0.0, 50.0, 0.0,
                            tr("Smallest weight every position must keep (0 % = may be dropped)"));
    row2->addStretch(1);

    auto* as_of = new QLabel(w);
    as_of->setObjectName(QStringLiteral("poAsOf"));
    row2->addWidget(as_of);
    as_of_labels_[kOptimizeTab] = as_of;

    run_btn_ = new QPushButton(tr("▶ RUN OPTIMIZATION"), w);
    run_btn_->setObjectName(QStringLiteral("poRun"));
    run_btn_->setMinimumHeight(26);
    run_btn_->setCursor(Qt::PointingHandCursor);
    connect(run_btn_, &QPushButton::clicked, this, [this]() {
        if (summary_.holdings.isEmpty() || !result_panes_[kOptimizeTab])
            return;
        apply_view_key();
        result_panes_[kOptimizeTab]->run();
    });
    row2->addWidget(run_btn_);

    layout->addLayout(row1);
    layout->addLayout(row2);

    auto* pane = make_result_body(kOptimizeTab, w);
    pane->skeleton()->set_shape(ui::LoadingSkeleton::Shape::Result);
    pane->skeleton()->set_kpi_count(4);

    // Result: headline tiles, the models used, any caveat, then the trades.
    auto* content = new QWidget(pane->content_parent());
    auto* cl = new QVBoxLayout(content);
    cl->setContentsMargins(0, 4, 0, 0);
    cl->setSpacing(6);
    opt_kpis_ = new ui::KpiStrip(content);
    opt_kpis_->add_tile(QStringLiteral("ret"), tr("EXP. RETURN"));
    opt_kpis_->add_tile(QStringLiteral("vol"), tr("VOLATILITY"))->set_delta_inverted(true);
    opt_kpis_->add_tile(QStringLiteral("sharpe"), tr("SHARPE"));
    opt_kpis_->add_tile(QStringLiteral("turnover"), tr("TURNOVER"));
    cl->addWidget(opt_kpis_);
    opt_caption_ = new QLabel(content);
    opt_caption_->setObjectName(QStringLiteral("poCaption"));
    opt_caption_->setWordWrap(true);
    // Long notes must not set the minimum width of the whole view.
    opt_caption_->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    cl->addWidget(opt_caption_);
    opt_warn_ = new QLabel(content);
    opt_warn_->setObjectName(QStringLiteral("poWarn"));
    opt_warn_->setWordWrap(true);
    opt_warn_->setTextFormat(Qt::PlainText);
    opt_warn_->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    opt_warn_->hide();
    cl->addWidget(opt_warn_);

    result_table_ = new QTableWidget(content);
    // Columns 0-2 are the optimizer's view of the book — keep their order.
    result_table_->setColumnCount(7);
    result_table_->setHorizontalHeaderLabels({tr("SYMBOL"), tr("CURRENT WT%"), tr("OPTIMAL WT%"), tr("CHANGE"),
                                              tr("ACTION"), tr("TRADE QTY"), tr("TRADE VALUE")});
    result_table_->setToolTip(tr("TRADE QTY / VALUE: what moves the book from current to optimal weights at today's "
                                 "prices and NAV (before costs and taxes)."));
    opt_style_table(result_table_);
    cl->addWidget(result_table_, 1);
    pane->set_content(content);

    layout->addWidget(result_bodies_[kOptimizeTab], 1);
    return w;
}

QWidget* PortfolioOptimizationView::build_frontier_tab() {
    auto* w = new QWidget(opt_page_parent(tabs_, this)); // built where addTab() keeps it
    auto* layout = new QVBoxLayout(w);
    layout->setContentsMargins(12, 8, 12, 8);
    layout->setSpacing(6);
    layout->addLayout(make_tab_header(w, frontier_title_, tr("EFFICIENT FRONTIER"), kFrontierTab));

    auto* pane = make_result_body(kFrontierTab, w);
    pane->skeleton()->set_shape(ui::LoadingSkeleton::Shape::Chart);

    frontier_stack_ = new QStackedWidget(pane->content_parent());
    auto* chart = new QChart;
    chart->setBackgroundBrush(QBrush(QColor(ui::colors::BG_BASE())));
    chart->setMargins(QMargins(8, 8, 8, 8));
    chart->legend()->setVisible(true);
    chart->legend()->setLabelColor(QColor(ui::colors::TEXT_SECONDARY()));
    chart->setAnimationOptions(QChart::NoAnimation);
    frontier_chart_ = make_chart_view(chart, frontier_stack_);
    frontier_stack_->addWidget(frontier_chart_);
    frontier_empty_ = new ui::EmptyState(frontier_stack_);
    frontier_stack_->addWidget(frontier_empty_);
    frontier_stack_->setCurrentWidget(frontier_chart_);
    pane->set_content(frontier_stack_);

    layout->addWidget(result_bodies_[kFrontierTab], 1);
    return w;
}

QWidget* PortfolioOptimizationView::build_allocation_tab() {
    auto* w = new QWidget(opt_page_parent(tabs_, this)); // built where addTab() keeps it
    auto* outer = new QVBoxLayout(w);
    outer->setContentsMargins(12, 8, 12, 8);
    outer->setSpacing(6);
    alloc_title_ = new QLabel(tr("CURRENT ALLOCATION"), w);
    alloc_title_->setObjectName(QStringLiteral("poTitle"));
    outer->addWidget(alloc_title_);

    alloc_body_ = new QStackedWidget(w);
    auto* content = new QWidget(alloc_body_);
    auto* layout = new QHBoxLayout(content);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(12);

    auto* chart = new QChart;
    chart->setBackgroundBrush(Qt::transparent);
    chart->setMargins(QMargins(0, 0, 0, 0));
    chart->legend()->setVisible(false);
    alloc_chart_ = make_chart_view(chart, content);
    // Grows with the font instead of a fixed 220 px square.
    const int side = std::max(200, ui::fonts::font_px(0) * 15);
    alloc_chart_->setMinimumSize(side, side);
    alloc_chart_->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
    layout->addWidget(alloc_chart_, 2);

    alloc_table_ = new QTableWidget(content);
    alloc_table_->setColumnCount(4);
    alloc_table_->setHorizontalHeaderLabels({tr("SYMBOL"), tr("WEIGHT"), tr("VALUE"), tr("VS EQUAL WT")});
    opt_style_table(alloc_table_);
    layout->addWidget(alloc_table_, 3);
    alloc_body_->addWidget(content);

    alloc_empty_ = new ui::EmptyState(alloc_body_);
    connect(alloc_empty_, &ui::EmptyState::primary_clicked, this, &PortfolioOptimizationView::add_position_requested);
    alloc_body_->addWidget(alloc_empty_);
    alloc_body_->setCurrentIndex(0);
    outer->addWidget(alloc_body_, 1);
    return w;
}

QWidget* PortfolioOptimizationView::build_strategies_tab() {
    auto* w = new QWidget(opt_page_parent(tabs_, this)); // built where addTab() keeps it
    auto* layout = new QVBoxLayout(w);
    layout->setContentsMargins(12, 8, 12, 8);
    layout->setSpacing(6);
    layout->addLayout(make_tab_header(w, strategies_title_, tr("STRATEGY COMPARISON"), kStrategiesTab));

    auto* pane = make_result_body(kStrategiesTab, w);
    pane->skeleton()->set_shape(ui::LoadingSkeleton::Shape::Result);
    pane->skeleton()->set_kpi_count(0);
    pane->skeleton()->set_row_count(5);

    auto* content = new QSplitter(Qt::Vertical, pane->content_parent());
    content->setChildrenCollapsible(false);
    strategies_chart_ = new ui::BarChart(content);
    strategies_chart_->set_show_values(true);
    strategies_chart_->set_show_legend(true);
    strategies_chart_->set_value_formatter([](double v) { return ui::formatting::pct(v, 1, false); });
    strategies_chart_->setMinimumHeight(std::max(160, ui::fonts::font_px(0) * 11));
    content->addWidget(strategies_chart_);

    strategies_table_ = new QTableWidget(content);
    strategies_table_->setColumnCount(5);
    strategies_table_->setHorizontalHeaderLabels(
        {tr("STRATEGY"), tr("EXP. RETURN"), tr("VOLATILITY"), tr("SHARPE"), tr("DESCRIPTION")});
    opt_style_table(strategies_table_);
    content->addWidget(strategies_table_);
    content->setStretchFactor(0, 3);
    content->setStretchFactor(1, 2);
    pane->set_content(content);

    layout->addWidget(result_bodies_[kStrategiesTab], 1);
    return w;
}

QWidget* PortfolioOptimizationView::build_compare_tab() {
    auto* w = new QWidget(opt_page_parent(tabs_, this)); // built where addTab() keeps it
    auto* layout = new QVBoxLayout(w);
    layout->setContentsMargins(12, 8, 12, 8);
    layout->setSpacing(6);
    layout->addLayout(make_tab_header(w, compare_title_, tr("WEIGHT COMPARISON  (all methods, per symbol)"),
                                      kCompareTab));

    auto* pane = make_result_body(kCompareTab, w);
    pane->skeleton()->set_shape(ui::LoadingSkeleton::Shape::Table);

    auto* content = new QSplitter(Qt::Vertical, pane->content_parent());
    content->setChildrenCollapsible(false);
    compare_heat_ = new ui::MatrixHeatmap(content);
    compare_heat_->set_sequential_scale(0.0);
    compare_heat_->set_show_values(true);
    compare_heat_->set_min_cell_size(10);
    compare_heat_->set_value_formatter([](double v) { return ui::formatting::pct(v, 1, false); });
    content->addWidget(compare_heat_);

    compare_table_ = new QTableWidget(content);
    opt_style_table(compare_table_);
    content->addWidget(compare_table_);
    content->setStretchFactor(0, 3);
    content->setStretchFactor(1, 2);
    pane->set_content(content);

    layout->addWidget(result_bodies_[kCompareTab], 1);
    return w;
}

QWidget* PortfolioOptimizationView::build_backtest_tab() {
    auto* w = new QWidget(opt_page_parent(tabs_, this)); // built where addTab() keeps it
    auto* outer = new QVBoxLayout(w);
    outer->setContentsMargins(12, 8, 12, 8);
    outer->setSpacing(6);

    // Toolbar: the run's inputs and the other actions; the result is always below it.
    auto* bar = new QHBoxLayout;
    bar->setSpacing(8);
    backtest_history_label_ = new QLabel(tr("HISTORY:"), w);
    backtest_history_label_->setObjectName(QStringLiteral("poFieldLabel"));
    bar->addWidget(backtest_history_label_);
    backtest_period_cb_ = new QComboBox(w);
    backtest_period_cb_->setObjectName(QStringLiteral("poCombo"));
    backtest_period_cb_->setMinimumHeight(24);
    for (int y : {1, 3, 5, 10})
        backtest_period_cb_->addItem(tr("%n year(s)", "", y), y);
    bar->addWidget(backtest_period_cb_);

    backtest_current_btn_ = new QPushButton(tr("BACKTEST CURRENT WEIGHTS"), w);
    backtest_current_btn_->setObjectName(QStringLiteral("poAction"));
    backtest_current_btn_->setCursor(Qt::PointingHandCursor);
    connect(backtest_current_btn_, &QPushButton::clicked, this, [this]() { run_backtest(false); });
    bar->addWidget(backtest_current_btn_);

    backtest_optimal_btn_ = new QPushButton(tr("BACKTEST OPTIMAL WEIGHTS"), w);
    backtest_optimal_btn_->setObjectName(QStringLiteral("poAction"));
    backtest_optimal_btn_->setCursor(Qt::PointingHandCursor);
    backtest_optimal_btn_->setEnabled(false);
    backtest_optimal_btn_->setToolTip(tr("Available once an optimisation result is on screen"));
    connect(backtest_optimal_btn_, &QPushButton::clicked, this, [this]() { run_backtest(true); });
    bar->addWidget(backtest_optimal_btn_);

    open_terminal_btn_ = new QPushButton(tr("OPEN FULL BACKTESTING TERMINAL"), w);
    open_terminal_btn_->setObjectName(QStringLiteral("poAction"));
    open_terminal_btn_->setCursor(Qt::PointingHandCursor);
    connect(open_terminal_btn_, &QPushButton::clicked, this, [this]() {
        if (summary_.holdings.isEmpty())
            return;
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
    bar->addWidget(open_terminal_btn_);
    bar->addStretch(1);
    backtest_as_of_ = new QLabel(w);
    backtest_as_of_->setObjectName(QStringLiteral("poAsOf"));
    bar->addWidget(backtest_as_of_);
    outer->addLayout(bar);

    backtest_body_ = new QStackedWidget(w);
    backtest_pane_ = new ui::AutoLoadPane(ui::AutoLoadPane::Policy::AutoRun, backtest_body_);
    backtest_body_->addWidget(backtest_pane_);
    backtest_empty_ = new ui::EmptyState(backtest_body_);
    connect(backtest_empty_, &ui::EmptyState::primary_clicked, this,
            &PortfolioOptimizationView::add_position_requested);
    backtest_body_->addWidget(backtest_empty_);
    backtest_body_->setCurrentWidget(backtest_pane_);

    backtest_pane_->skeleton()->set_shape(ui::LoadingSkeleton::Shape::Result);
    backtest_pane_->skeleton()->set_kpi_count(4);
    backtest_pane_->set_cache_key(QStringLiteral("portfolio/opt_backtest"), kPortfolioResultTtlDays);
    backtest_pane_->set_persist_limit(512 * 1024);
    backtest_pane_->set_max_age(kPortfolioScriptMaxAgeS);
    backtest_pane_->set_timeout(180);
    backtest_pane_->set_requirements({ui::Requirement::python()});
    backtest_pane_->set_status_label(backtest_as_of_);
    backtest_pane_->set_start([this](quint64 id) { start_backtest(id); });
    backtest_pane_->set_cancel([this](quint64 id) { cancel_backtest(id); });
    backtest_pane_->set_render([this](const QJsonValue& payload, const QDateTime&, bool) {
        render_backtest(payload.toObject());
    });

    auto* content = new QWidget(backtest_pane_->content_parent());
    auto* rl = new QVBoxLayout(content);
    rl->setContentsMargins(0, 4, 0, 0);
    rl->setSpacing(6);
    backtest_caption_ = new QLabel(content);
    backtest_caption_->setObjectName(QStringLiteral("poCaption"));
    backtest_caption_->setWordWrap(true);
    backtest_caption_->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    rl->addWidget(backtest_caption_);
    backtest_warn_ = new QLabel(content);
    backtest_warn_->setObjectName(QStringLiteral("poWarn"));
    backtest_warn_->setWordWrap(true);
    backtest_warn_->setTextFormat(Qt::PlainText);
    backtest_warn_->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    backtest_warn_->hide();
    rl->addWidget(backtest_warn_);
    backtest_kpis_ = new ui::KpiStrip(content);
    backtest_kpis_->add_tile(QStringLiteral("total"), tr("TOTAL RETURN"));
    backtest_kpis_->add_tile(QStringLiteral("ann"), tr("ANNUALISED"));
    backtest_kpis_->add_tile(QStringLiteral("sharpe"), tr("SHARPE"));
    backtest_kpis_->add_tile(QStringLiteral("maxdd"), tr("MAX DRAWDOWN"));
    backtest_kpis_->add_tile(QStringLiteral("vol"), tr("VOLATILITY"));
    backtest_kpis_->add_tile(QStringLiteral("sortino"), tr("SORTINO"));
    backtest_kpis_->add_tile(QStringLiteral("winners"), tr("WINNERS"));
    backtest_kpis_->add_tile(QStringLiteral("calmar"), tr("CALMAR"));
    rl->addWidget(backtest_kpis_);

    // The equity and drawdown curves of the run.
    backtest_chart_ = new ui::TimeSeriesChart(content);
    backtest_chart_->set_utc(true);
    backtest_chart_->set_show_mode_buttons(false);
    backtest_chart_->set_export_name(QStringLiteral("portfolio_backtest"));
    backtest_chart_->set_time_formatter(
        [](double x) { return ui::formatting::bar_date(static_cast<qint64>(x / 1000.0)); });
    backtest_chart_->setMinimumHeight(std::max(220, ui::fonts::font_px(0) * 16));
    rl->addWidget(backtest_chart_, 1);
    backtest_pane_->set_content(content);

    outer->addWidget(backtest_body_, 1);

    // Results of runs this view started (correlated by run id; another screen's
    // backtest never lands here).
    auto& svc = fincept::services::backtest::BacktestingService::instance();
    connect(&svc, &fincept::services::backtest::BacktestingService::run_completed, this,
            [this](quint64 run_id, const QString& /*provider*/, const QString& /*command*/, const QJsonObject& data) {
                const auto it = backtest_runs_.find(run_id);
                if (it == backtest_runs_.end())
                    return;
                const BacktestRun run = it.value();
                backtest_runs_.erase(it);
                if (backtest_pane_)
                    backtest_pane_->deliver(run.request_id,
                                            opt_backtest_payload(data, run.optimal, run.years, run.start, run.end));
            });
    connect(&svc, &fincept::services::backtest::BacktestingService::run_failed, this,
            [this](quint64 run_id, const QString& /*context*/, const QString& message) {
                const auto it = backtest_runs_.find(run_id);
                if (it == backtest_runs_.end())
                    return;
                const quint64 request_id = it.value().request_id;
                backtest_runs_.erase(it);
                if (backtest_pane_)
                    backtest_pane_->fail(request_id, message.isEmpty() ? tr("The backtest failed.") : message);
            });
    return w;
}

QWidget* PortfolioOptimizationView::build_risk_tab() {
    auto* w = new QWidget(opt_page_parent(tabs_, this)); // built where addTab() keeps it
    auto* layout = new QVBoxLayout(w);
    layout->setContentsMargins(12, 8, 12, 8);
    layout->setSpacing(6);
    layout->addLayout(make_tab_header(w, risk_title_, tr("RISK DECOMPOSITION"), kRiskTab));

    auto* pane = make_result_body(kRiskTab, w);
    pane->skeleton()->set_shape(ui::LoadingSkeleton::Shape::Result);
    pane->skeleton()->set_kpi_count(0);

    auto* content = new QSplitter(Qt::Vertical, pane->content_parent());
    content->setChildrenCollapsible(false);
    risk_chart_ = new ui::BarChart(content);
    risk_chart_->set_orientation(Qt::Horizontal);
    risk_chart_->set_show_legend(true);
    risk_chart_->set_value_formatter([](double v) { return ui::formatting::pct(v, 1, false); });
    risk_chart_->setMinimumHeight(std::max(160, ui::fonts::font_px(0) * 11));
    content->addWidget(risk_chart_);

    risk_table_ = new QTableWidget(content);
    risk_table_->setColumnCount(5);
    risk_table_->setHorizontalHeaderLabels(
        {tr("SYMBOL"), tr("WEIGHT"), tr("ASSET VOL"), tr("MARGINAL RISK"), tr("RISK CONTRIB")});
    opt_style_table(risk_table_);
    content->addWidget(risk_table_);
    content->setStretchFactor(0, 3);
    content->setStretchFactor(1, 2);
    pane->set_content(content);

    layout->addWidget(result_bodies_[kRiskTab], 1);
    return w;
}

QWidget* PortfolioOptimizationView::build_stress_tab() {
    auto* w = new QWidget(opt_page_parent(tabs_, this)); // built where addTab() keeps it
    auto* layout = new QVBoxLayout(w);
    layout->setContentsMargins(12, 8, 12, 8);
    layout->setSpacing(6);

    stress_title_ = new QLabel(tr("STRESS SCENARIOS"), w);
    stress_title_->setObjectName(QStringLiteral("poTitle"));
    layout->addWidget(stress_title_);

    stress_stack_ = new QStackedWidget(w);
    stress_empty_ = new ui::EmptyState(stress_stack_);
    connect(stress_empty_, &ui::EmptyState::primary_clicked, this, &PortfolioOptimizationView::add_position_requested);
    stress_stack_->addWidget(stress_empty_);

    stress_table_ = new QTableWidget(stress_stack_);
    stress_table_->setColumnCount(4);
    stress_table_->setHorizontalHeaderLabels({tr("SCENARIO"), tr("DESCRIPTION"), tr("IMPACT"), tr("EST. LOSS")});
    opt_style_table(stress_table_);
    stress_stack_->addWidget(stress_table_);

    layout->addWidget(stress_stack_, 1);
    return w;
}

QWidget* PortfolioOptimizationView::build_black_litterman_tab() {
    auto* w = new QWidget(opt_page_parent(tabs_, this)); // built where addTab() keeps it
    auto* layout = new QVBoxLayout(w);
    layout->setContentsMargins(12, 8, 12, 8);
    layout->setSpacing(6);
    layout->addLayout(make_tab_header(w, bl_title_, tr("BLACK-LITTERMAN MODEL"), kBlTab));

    bl_body_ = new QLabel(tr("IMPLIED = equilibrium returns at which your current mix would be optimal "
                             "(π = δ·Σ·w). POSTERIOR = π blended with the historical-return views "
                             "(Ω = diag(τΣ), τ = 0.05). Select 'B-L Model' as the METHOD to optimise on the posterior."),
                          w);
    bl_body_->setObjectName(QStringLiteral("poNote"));
    bl_body_->setWordWrap(true);
    layout->addWidget(bl_body_);

    auto* pane = make_result_body(kBlTab, w);
    pane->skeleton()->set_shape(ui::LoadingSkeleton::Shape::Table);

    bl_table_ = new QTableWidget(pane->content_parent());
    bl_table_->setColumnCount(5);
    bl_table_->setHorizontalHeaderLabels(
        {tr("SYMBOL"), tr("IMPLIED RETURN"), tr("POSTERIOR RETURN"), tr("CURRENT WT"), tr("OPTIMIZED WT")});
    opt_style_table(bl_table_);
    pane->set_content(bl_table_);

    layout->addWidget(result_bodies_[kBlTab], 1);
    return w;
}

void PortfolioOptimizationView::apply_state_texts() {
    const QString loading = tr("Downloading 1 year of prices and optimising…");
    struct TabText {
        ResultTab tab;
        QString what;   // what the tab shows (no-positions and preview messages)
        ui::LoadingSkeleton::Shape shape;
    };
    const TabText texts[] = {
        {kOptimizeTab, tr("the optimal weights and the trades that get there"), ui::LoadingSkeleton::Shape::Result},
        {kFrontierTab, tr("the efficient frontier of your holdings"), ui::LoadingSkeleton::Shape::Chart},
        {kStrategiesTab, tr("five allocation strategies side by side"), ui::LoadingSkeleton::Shape::Result},
        {kCompareTab, tr("the weights every method gives each holding"), ui::LoadingSkeleton::Shape::Table},
        {kRiskTab, tr("each holding's share of the portfolio's risk"), ui::LoadingSkeleton::Shape::Result},
        {kBlTab, tr("market-implied and Black-Litterman returns"), ui::LoadingSkeleton::Shape::Table},
    };
    for (const auto& t : texts) {
        if (auto* pane = result_panes_[t.tab]) {
            pane->set_loading_text(loading);
            ui::EmptyState::Spec s;
            s.glyph = QStringLiteral("∑"); // ∑
            s.glyph_tone = ui::EmptyState::Tone::Accent;
            s.title = tr("Optimisation not run");
            s.message = tr("Shows %1, computed from 1 year of daily prices of your holdings. Free; takes a few "
                           "seconds.")
                            .arg(t.what);
            s.primary = tr("RUN OPTIMIZATION");
            s.illustration = true;
            s.illustration_shape = t.shape;
            pane->preview()->apply(s);
        }
        if (auto* empty = result_empty_[t.tab])
            empty->apply(ui::EmptyState::no_data_spec(
                tr("No positions to optimise"),
                tr("This tab shows %1 once the portfolio holds a position.").arg(t.what),
                tr("BUY")));
    }
    if (frontier_empty_)
        frontier_empty_->apply(ui::EmptyState::no_data_spec(
            tr("No frontier for these holdings"),
            tr("The optimiser could not trace an efficient frontier for this book (too few holdings with price "
               "history, or the bounds leave no room). The other tabs still show the run.")));
    if (alloc_empty_)
        alloc_empty_->apply(ui::EmptyState::no_data_spec(
            tr("No positions yet"), tr("The allocation donut appears once the portfolio holds a position."),
            tr("BUY")));
    if (stress_empty_)
        stress_empty_->apply(ui::EmptyState::no_data_spec(
            tr("No positions to stress"),
            tr("Historical crisis shocks are applied to your asset-class mix once the portfolio holds a position."),
            tr("BUY")));
    if (backtest_empty_)
        backtest_empty_->apply(ui::EmptyState::no_data_spec(
            tr("No positions to backtest"), tr("A buy-and-hold backtest of your weights appears once the portfolio "
                                               "holds a position."),
            tr("BUY")));
    if (backtest_pane_) {
        backtest_pane_->set_loading_text(tr("Backtesting your weights on %n year(s) of prices…", "",
                                            backtest_period_cb_ ? backtest_period_cb_->currentData().toInt() : 1));
        ui::EmptyState::Spec s;
        s.glyph = QStringLiteral("▶"); // ▶
        s.glyph_tone = ui::EmptyState::Tone::Accent;
        s.title = tr("Backtest not run");
        s.message = tr("Buy-and-hold of your current weights over the chosen history: equity curve, drawdown, "
                       "return, Sharpe. Free; takes a few seconds.");
        s.primary = tr("BACKTEST CURRENT WEIGHTS");
        s.illustration = true;
        s.illustration_shape = ui::LoadingSkeleton::Shape::Result;
        backtest_pane_->preview()->apply(s);
    }
}

// ── Data ──────────────────────────────────────────────────────────────────────

void PortfolioOptimizationView::set_data(const portfolio::PortfolioSummary& summary, const QString& currency) {
    summary_ = summary;
    currency_ = currency;
    has_data_ = true;

    // The panes remember a result per portfolio + holdings (a trade = a new key and a
    // new default run; a price tick is not part of it).
    const QString key = portfolio_run_key(summary_);
    if (key != run_key_) {
        run_key_ = key;
        // Optimal weights belong to the result of the previous key; the backtest
        // starts again from the current weights.
        last_opt_weights_ = {};
        backtest_optimal_ = false;
        if (backtest_optimal_btn_)
            backtest_optimal_btn_->setEnabled(false);
    }

    // An empty book has nothing to optimise or backtest: the "no positions" state
    // (with BUY) instead of a run. Switched before the panes get their key, so a
    // hidden pane never starts.
    update_empty_states();
    apply_view_key();

    {
        const bool has_holdings = !summary_.holdings.isEmpty();
        const QString why = has_holdings ? QString() : tr("Add a position first");
        for (QPushButton* b : {run_btn_, backtest_current_btn_, open_terminal_btn_}) {
            if (!b)
                continue;
            b->setEnabled(has_holdings);
            b->setToolTip(why);
        }
    }
    update_allocation();
    update_stress(); // stress test runs off current holdings — no optimization needed
}

void PortfolioOptimizationView::update_empty_states() {
    const int page = summary_.holdings.isEmpty() ? 1 : 0;
    for (QStackedWidget* body : result_bodies_)
        if (body)
            body->setCurrentIndex(page);
    if (backtest_body_)
        backtest_body_->setCurrentIndex(page);
    if (alloc_body_)
        alloc_body_->setCurrentIndex(page);
}

void PortfolioOptimizationView::set_analytics(const portfolio::MarketAnalytics& analytics) {
    analytics_ = analytics;
    if (has_data_)
        update_stress();
}

void PortfolioOptimizationView::set_target_return(double annual_return) {
    if (annual_return <= 0.0 || !method_cb_ || !target_ret_spin_)
        return;
    const int idx = method_cb_->findText(QStringLiteral("Target Return"));
    if (idx < 0)
        return;
    method_cb_->setCurrentIndex(idx);
    target_ret_spin_->setValue(std::min(annual_return * 100.0, target_ret_spin_->maximum()));
    tabs_->setCurrentIndex(optimize_tab_index_);
    if (summary_.holdings.isEmpty() || !result_panes_[kOptimizeTab])
        return;
    apply_view_key();
    result_panes_[kOptimizeTab]->run(); // the user asked for it: runs now, bypassing the queue
}

void PortfolioOptimizationView::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange)
        retranslateUi();
    QWidget::changeEvent(event);
}

void PortfolioOptimizationView::retranslateUi() {
    if (tabs_) {
        if (optimize_tab_index_ >= 0)
            tabs_->setTabText(optimize_tab_index_, tr("OPTIMIZE"));
        if (frontier_tab_index_ >= 0)
            tabs_->setTabText(frontier_tab_index_, tr("FRONTIER"));
        if (allocation_tab_index_ >= 0)
            tabs_->setTabText(allocation_tab_index_, tr("ALLOCATION"));
        if (strategies_tab_index_ >= 0)
            tabs_->setTabText(strategies_tab_index_, tr("STRATEGIES"));
        if (compare_tab_index_ >= 0)
            tabs_->setTabText(compare_tab_index_, tr("COMPARE"));
        if (backtest_tab_index_ >= 0)
            tabs_->setTabText(backtest_tab_index_, tr("BACKTEST"));
        if (risk_tab_index_ >= 0)
            tabs_->setTabText(risk_tab_index_, tr("RISK"));
        if (stress_tab_index_ >= 0)
            tabs_->setTabText(stress_tab_index_, tr("STRESS"));
        if (bl_tab_index_ >= 0)
            tabs_->setTabText(bl_tab_index_, tr("B-L MODEL"));
    }
    if (method_field_label_)
        method_field_label_->setText(tr("METHOD:"));
    if (returns_field_label_)
        returns_field_label_->setText(tr("RETURNS:"));
    if (risk_model_field_label_)
        risk_model_field_label_->setText(tr("RISK MODEL:"));
    if (target_ret_label_)
        target_ret_label_->setText(tr("TARGET RETURN:"));
    if (max_w_label_)
        max_w_label_->setText(tr("MAX WT:"));
    if (min_w_label_)
        min_w_label_->setText(tr("MIN WT:"));
    if (run_btn_)
        run_btn_->setText(tr("▶ RUN OPTIMIZATION"));
    if (opt_kpis_) {
        opt_kpis_->tile(QStringLiteral("ret"))->set_label(tr("EXP. RETURN"));
        opt_kpis_->tile(QStringLiteral("vol"))->set_label(tr("VOLATILITY"));
        opt_kpis_->tile(QStringLiteral("sharpe"))->set_label(tr("SHARPE"));
        opt_kpis_->tile(QStringLiteral("turnover"))->set_label(tr("TURNOVER"));
    }
    if (backtest_kpis_) {
        backtest_kpis_->tile(QStringLiteral("total"))->set_label(tr("TOTAL RETURN"));
        backtest_kpis_->tile(QStringLiteral("ann"))->set_label(tr("ANNUALISED"));
        backtest_kpis_->tile(QStringLiteral("sharpe"))->set_label(tr("SHARPE"));
        backtest_kpis_->tile(QStringLiteral("maxdd"))->set_label(tr("MAX DRAWDOWN"));
        backtest_kpis_->tile(QStringLiteral("vol"))->set_label(tr("VOLATILITY"));
        backtest_kpis_->tile(QStringLiteral("sortino"))->set_label(tr("SORTINO"));
        backtest_kpis_->tile(QStringLiteral("winners"))->set_label(tr("WINNERS"));
        backtest_kpis_->tile(QStringLiteral("calmar"))->set_label(tr("CALMAR"));
    }

    if (result_table_)
        result_table_->setHorizontalHeaderLabels({tr("SYMBOL"), tr("CURRENT WT%"), tr("OPTIMAL WT%"), tr("CHANGE"),
                                                  tr("ACTION"), tr("TRADE QTY"), tr("TRADE VALUE")});
    if (alloc_table_)
        alloc_table_->setHorizontalHeaderLabels({tr("SYMBOL"), tr("WEIGHT"), tr("VALUE"), tr("VS EQUAL WT")});
    if (strategies_table_)
        strategies_table_->setHorizontalHeaderLabels(
            {tr("STRATEGY"), tr("EXP. RETURN"), tr("VOLATILITY"), tr("SHARPE"), tr("DESCRIPTION")});
    if (risk_table_)
        risk_table_->setHorizontalHeaderLabels(
            {tr("SYMBOL"), tr("WEIGHT"), tr("ASSET VOL"), tr("MARGINAL RISK"), tr("RISK CONTRIB")});
    if (stress_table_)
        stress_table_->setHorizontalHeaderLabels({tr("SCENARIO"), tr("DESCRIPTION"), tr("IMPACT"), tr("EST. LOSS")});
    if (bl_table_)
        bl_table_->setHorizontalHeaderLabels(
            {tr("SYMBOL"), tr("IMPLIED RETURN"), tr("POSTERIOR RETURN"), tr("CURRENT WT"), tr("OPTIMIZED WT")});

    if (frontier_title_)
        frontier_title_->setText(tr("EFFICIENT FRONTIER"));
    if (alloc_title_)
        alloc_title_->setText(tr("CURRENT ALLOCATION"));
    if (strategies_title_)
        strategies_title_->setText(tr("STRATEGY COMPARISON"));
    if (compare_title_)
        compare_title_->setText(tr("WEIGHT COMPARISON  (all methods, per symbol)"));
    if (risk_title_)
        risk_title_->setText(tr("RISK DECOMPOSITION"));
    if (stress_title_)
        stress_title_->setText(tr("STRESS SCENARIOS"));
    if (bl_title_)
        bl_title_->setText(tr("BLACK-LITTERMAN MODEL"));
    if (bl_body_)
        bl_body_->setText(
            tr("IMPLIED = equilibrium returns at which your current mix would be optimal "
               "(π = δ·Σ·w). POSTERIOR = π blended with the historical-return views "
               "(Ω = diag(τΣ), τ = 0.05). Select 'B-L Model' as the METHOD to optimise on the posterior."));
    if (backtest_history_label_)
        backtest_history_label_->setText(tr("HISTORY:"));
    if (backtest_period_cb_) {
        for (int i = 0; i < backtest_period_cb_->count(); ++i)
            backtest_period_cb_->setItemText(i, tr("%n year(s)", "", backtest_period_cb_->itemData(i).toInt()));
    }
    if (backtest_current_btn_)
        backtest_current_btn_->setText(tr("BACKTEST CURRENT WEIGHTS"));
    if (backtest_optimal_btn_)
        backtest_optimal_btn_->setText(tr("BACKTEST OPTIMAL WEIGHTS"));
    if (open_terminal_btn_)
        open_terminal_btn_->setText(tr("OPEN FULL BACKTESTING TERMINAL"));
    apply_state_texts();

    // Combo items are English keys sent to Python (Max Sharpe, Risk Parity, etc.)
    // and intentionally not translated. Tables and charts are rebuilt from data.
    refresh_theme();
}

// ── Optimization (one run, six tabs) ──────────────────────────────────────────

QString PortfolioOptimizationView::optimizer_signature() const {
    QVariantMap in;
    in.insert(QStringLiteral("method"), method_cb_ ? method_cb_->currentText() : QString());
    in.insert(QStringLiteral("returns"), returns_cb_ ? returns_cb_->currentText() : QString());
    in.insert(QStringLiteral("risk_model"), risk_model_cb_ ? risk_model_cb_->currentText() : QString());
    if (method_cb_ && method_cb_->currentText() == QLatin1String("Target Return") && target_ret_spin_)
        in.insert(QStringLiteral("target"), target_ret_spin_->value());
    in.insert(QStringLiteral("max_wt"), max_w_spin_ ? max_w_spin_->value() : 100.0);
    in.insert(QStringLiteral("min_wt"), min_w_spin_ ? min_w_spin_->value() : 0.0);
    return ui::autoload::signature_of(in);
}

void PortfolioOptimizationView::apply_view_key() {
    if (run_key_.isEmpty() || !shared_)
        return;
    shared_->set_view(run_key_, optimizer_signature());
    apply_backtest_view();
}

QString PortfolioOptimizationView::optimizer_args(QString* why) const {
    QJsonArray symbols_arr;
    QJsonArray weights_arr;
    for (const auto& h : summary_.holdings) {
        if (!h.valued() || !std::isfinite(h.weight))
            continue; // an unvalued position has no current weight to start from
        symbols_arr.append(h.symbol);
        weights_arr.append(h.weight / 100.0);
    }
    if (symbols_arr.isEmpty()) {
        if (why)
            *why = summary_.holdings.isEmpty() ? tr("The portfolio holds no position to optimise.")
                                               : tr("No position can be valued yet — prices are still loading. "
                                                    "Try again in a moment.");
        return {};
    }

    QJsonObject args;
    args["symbols"] = symbols_arr;
    args["weights"] = weights_arr;
    args["method"] = method_cb_->currentText().toLower().replace(" ", "_");
    args["returns_method"] = returns_cb_->currentText().toLower().replace(" ", "_");
    args["risk_model"] = risk_model_cb_->currentText().toLower().replace(" ", "_");
    if (method_cb_->currentText() == QLatin1String("Target Return"))
        args["target_return"] = target_ret_spin_->value() / 100.0; // fraction, as the script expects
    if (max_w_spin_ && max_w_spin_->value() < 100.0)
        args["max_weight"] = max_w_spin_->value() / 100.0;
    if (min_w_spin_ && min_w_spin_->value() > 0.0)
        args["min_weight"] = min_w_spin_->value() / 100.0;
    // The portfolio's own risk-free rate (base currency, override honoured). An unknown
    // rate is left out — the script then uses 0 and says so — instead of inventing one.
    {
        const auto rf = services::PortfolioService::instance().risk_free_for(summary_.portfolio);
        if (rf.known)
            args["risk_free_rate"] = rf.rate;
    }
    return QString::fromUtf8(QJsonDocument(args).toJson(QJsonDocument::Compact));
}

void PortfolioOptimizationView::launch_optimizer(quint64 run_id) {
    QString why;
    const QString args = optimizer_args(&why);
    if (args.isEmpty()) {
        shared_->fail(run_id, why);
        return;
    }
    QPointer<PortfolioOptimizationView> self = this;
    // The reply may in principle arrive before optimize_weights() returns.
    auto answered = std::make_shared<bool>(false);
    const services::AnalyticsRequest req = PortfolioAnalyticsService::instance().optimize_weights(
        args, [self, run_id, answered](const AnalyticsResult& r) {
            *answered = true;
            if (!self)
                return;
            self->opt_requests_.remove(run_id);
            if (r.cancelled)
                return; // nobody waits for it any more
            if (!r.success) {
                self->shared_->fail(run_id, r.error.isEmpty() ? tr("The optimiser failed.") : r.error);
                return;
            }
            self->shared_->finish(run_id, r.data);
        });
    if (!*answered)
        opt_requests_.insert(run_id, req);
}

void PortfolioOptimizationView::render_tab(ResultTab tab, const QJsonObject& root) {
    rendered_[tab] = root;
    // Any result on screen feeds BACKTEST OPTIMAL WEIGHTS (whichever tab showed it).
    const QJsonObject weights = root.value(QStringLiteral("weights")).toObject();
    if (!weights.isEmpty() && weights != last_opt_weights_) {
        last_opt_weights_ = weights;
        if (backtest_optimal_btn_) {
            backtest_optimal_btn_->setEnabled(!summary_.holdings.isEmpty());
            backtest_optimal_btn_->setToolTip(QString());
        }
        if (backtest_optimal_)
            apply_backtest_view(); // the optimal backtest now stands for other weights
    }
    switch (tab) {
        case kOptimizeTab:
            render_optimize(root);
            break;
        case kFrontierTab:
            update_frontier(root);
            break;
        case kStrategiesTab:
            update_strategies(root);
            break;
        case kCompareTab:
            update_compare(root);
            break;
        case kRiskTab:
            update_risk(root);
            break;
        case kBlTab:
            update_black_litterman(root);
            break;
        case kResultTabCount:
            break;
    }
}

void PortfolioOptimizationView::render_optimize(const QJsonObject& root) {
    const auto opt_weights = root["weights"].toObject();
    const double ret = root["expected_annual_return"].toDouble();
    const double vol = root["annual_volatility"].toDouble();
    const double sharpe = root["sharpe_ratio"].toDouble();
    const QJsonObject cur = root["current"].toObject();

    // Symbols the optimizer actually had price history for. The script drops the
    // rest, so their "optimal" weight would silently fall back to the current one
    // and the column would no longer add up to 100%.
    const QSet<QString> priced = opt_priced(root);
    QStringList excluded;
    double turnover = 0.0;

    // ── Trades table ──────────────────────────────────────────────────────────
    result_table_->setUpdatesEnabled(false);
    result_table_->setRowCount(static_cast<int>(summary_.holdings.size()));
    for (int row = 0; row < static_cast<int>(summary_.holdings.size()); ++row) {
        const auto& h = summary_.holdings[row];
        const double cw = h.weight;
        const bool no_data = !priced.isEmpty() && !priced.contains(h.symbol);

        result_table_->setRowHeight(row, opt_row_height(28));

        auto set = [&](int col, const QString& text, const char* color = nullptr) {
            auto* item = new QTableWidgetItem(text);
            item->setTextAlignment(col == 0 ? (Qt::AlignLeft | Qt::AlignVCenter) : (Qt::AlignRight | Qt::AlignVCenter));
            if (color)
                item->setForeground(QColor(color));
            result_table_->setItem(row, col, item);
        };

        set(0, h.symbol, ui::colors::CYAN);
        set(1, ui::formatting::pct(cw, 1, false));

        if (no_data) {
            excluded.append(h.symbol);
            set(2, tr("n/a"), ui::colors::TEXT_TERTIARY);
            set(3, QStringLiteral("--"), ui::colors::TEXT_TERTIARY);
            set(4, tr("NO DATA"), ui::colors::TEXT_TERTIARY);
            set(5, QStringLiteral("--"), ui::colors::TEXT_TERTIARY);
            set(6, QStringLiteral("--"), ui::colors::TEXT_TERTIARY);
            continue;
        }

        const double ow = opt_weights.value(h.symbol).toDouble(cw / 100.0) * 100.0;
        const double ch = ow - cw;
        if (std::isfinite(ch))
            turnover += std::abs(ch) / 2.0;
        set(2, ui::formatting::pct(ow, 1, false), ui::colors::AMBER);
        set(3, ui::formatting::pct(ch, 1), ch >= 0.0 ? ui::colors::POSITIVE : ui::colors::NEGATIVE);

        const QString action = std::abs(ch) < 0.5 ? tr("HOLD") : ch > 0.0 ? tr("INCREASE") : tr("DECREASE");
        const char* ac = std::abs(ch) < 0.5 ? ui::colors::TEXT_TERTIARY
                         : ch > 0.0         ? ui::colors::POSITIVE
                                            : ui::colors::NEGATIVE;
        set(4, action, ac);

        // The trade that gets there: Δweight × NAV, in shares at the last price. Whole
        // shares for whole-share positions; fractional (crypto, funds) otherwise.
        const double trade_value = ch / 100.0 * summary_.total_market_value;
        if (std::abs(ch) >= 0.5 && h.current_price > 0.0) {
            const bool whole = h.quantity == std::floor(h.quantity);
            double qty = trade_value / h.current_price;
            if (whole)
                qty = std::round(qty);
            set(5, ui::formatting::signed_number(qty, whole ? 0 : 4), ac);
            set(6, ui::formatting::signed_money(trade_value, currency_, ui::formatting::Compact::None, 0), ac);
        } else {
            set(5, QStringLiteral("--"), ui::colors::TEXT_TERTIARY);
            set(6, QStringLiteral("--"), ui::colors::TEXT_TERTIARY);
        }
    }
    result_table_->setUpdatesEnabled(true);

    // ── Headline tiles: the result, and how it compares with today's mix ──────
    if (opt_kpis_) {
        const bool have_cur = !cur.isEmpty();
        const double cur_ret = cur["return"].toDouble();
        const double cur_vol = cur["volatility"].toDouble();
        const double cur_sharpe = cur["sharpe"].toDouble();
        auto* t_ret = opt_kpis_->tile(QStringLiteral("ret"));
        t_ret->set_state(ui::StatTile::State::Normal);
        t_ret->set_value(ui::formatting::pct(ret * 100.0, 1));
        auto* t_vol = opt_kpis_->tile(QStringLiteral("vol"));
        t_vol->set_state(ui::StatTile::State::Normal);
        t_vol->set_value(ui::formatting::pct(vol * 100.0, 1, false));
        auto* t_sh = opt_kpis_->tile(QStringLiteral("sharpe"));
        t_sh->set_state(ui::StatTile::State::Normal);
        t_sh->set_value(ui::formatting::number(sharpe, 2));
        if (have_cur) {
            const double d_ret = (ret - cur_ret) * 100.0;
            const double d_vol = (vol - cur_vol) * 100.0;
            const double d_sh = sharpe - cur_sharpe;
            t_ret->set_delta(d_ret, tr("%1 vs current").arg(ui::formatting::pp(d_ret, 1)));
            t_ret->set_sub_label(tr("current %1").arg(ui::formatting::pct(cur_ret * 100.0, 1)));
            t_vol->set_delta(d_vol, tr("%1 vs current").arg(ui::formatting::pp(d_vol, 1)));
            t_vol->set_sub_label(tr("current %1").arg(ui::formatting::pct(cur_vol * 100.0, 1, false)));
            t_sh->set_delta(d_sh, tr("%1 vs current").arg(ui::formatting::signed_number(d_sh, 2)));
            t_sh->set_sub_label(tr("current %1").arg(ui::formatting::number(cur_sharpe, 2)));
        } else {
            for (auto* t : {t_ret, t_vol, t_sh}) {
                t->clear_delta();
                t->set_sub_label(QString());
            }
        }
        auto* t_to = opt_kpis_->tile(QStringLiteral("turnover"));
        t_to->set_state(ui::StatTile::State::Normal);
        t_to->set_value(ui::formatting::pct(turnover, 1, false));
        t_to->set_sub_label(tr("of the book trades to get there"));
    }

    // ── Caption: what produced it ─────────────────────────────────────────────
    if (opt_caption_) {
        QStringList parts;
        parts << opt_strategy_name(root["strategy"].toString());
        const QString models = root["returns_model"].toString();
        if (!models.isEmpty())
            parts << tr("%1 returns").arg(models) << tr("%1 risk").arg(root["risk_model"].toString());
        if (root["risk_free_rate"].isDouble())
            parts << tr("risk-free %1").arg(ui::formatting::fraction_pct(root["risk_free_rate"].toDouble(), 2, false));
        const int obs = root["observations"].toInt();
        if (obs > 0) {
            const QString since = root["history_start"].toString();
            parts << (since.isEmpty() ? tr("%n trading day(s)", "", obs)
                                      : tr("%n trading day(s) since %1", "", obs).arg(since));
        }
        opt_caption_->setText(parts.join(QStringLiteral("  ·  ")));
    }

    // ── Caveats: never let them read as a clean result ────────────────────────
    QStringList notes;
    if (root["strategy"].toString() == QLatin1String("target_return") && target_ret_spin_) {
        // The target is a floor (return >= target): a result above it is the
        // minimum-volatility mix and meets the request. Only a shortfall means
        // the target could not be reached.
        const double target = target_ret_spin_->value() / 100.0;
        if (target - ret > 0.005)
            notes << tr("Target %1 is not reachable with these holdings — the closest result is shown.")
                         .arg(ui::formatting::pct(target * 100.0, 1, false));
    }
    if (!excluded.isEmpty())
        notes << tr("No price history, left out: %1").arg(excluded.join(QStringLiteral(", ")));
    const bool converged = !root.contains("converged") || root["converged"].toBool(true);
    if (!converged)
        notes << tr("The solver did not converge%1 — treat these weights with caution.")
                     .arg(root["solver_message"].toString().isEmpty()
                              ? QString()
                              : QStringLiteral(" (%1)").arg(root["solver_message"].toString()));
    for (const auto& w : root["warnings"].toArray())
        notes << w.toString();
    if (opt_warn_) {
        QStringList lines;
        for (const QString& n : notes)
            lines << QStringLiteral("⚠ ") + n; // ⚠ — the tone is not carried by colour alone
        opt_warn_->setText(lines.join(QLatin1Char('\n')));
        opt_warn_->setVisible(!notes.isEmpty());
    }
}

// ── Allocation donut ──────────────────────────────────────────────────────────

void PortfolioOptimizationView::update_allocation() {
    if (summary_.holdings.isEmpty()) {
        // Sold the last position: don't leave the old donut/table behind.
        alloc_chart_->chart()->removeAllSeries();
        alloc_table_->setRowCount(0);
        return;
    }

    auto* chart = alloc_chart_->chart();
    chart->removeAllSeries();

    auto* series = new QPieSeries;
    series->setHoleSize(0.5);

    // Eight hues of the portfolio category palette, in this donut's order.
    constexpr int kHueOrder[] = {0, 1, 2, 4, 3, 9, 5, 8};

    auto sorted = summary_.holdings;
    std::sort(sorted.begin(), sorted.end(), [](const auto& a, const auto& b) { return a.weight > b.weight; });

    for (int i = 0; i < static_cast<int>(sorted.size()); ++i) {
        auto* slice = series->append(sorted[i].symbol, sorted[i].weight);
        slice->setColor(PortfolioSectorPanel::sector_color(kHueOrder[i % 8]));
        slice->setBorderColor(QColor(ui::colors::BG_BASE()));
        slice->setBorderWidth(1);
    }
    chart->addSeries(series);

    alloc_table_->setUpdatesEnabled(false);
    alloc_table_->setRowCount(static_cast<int>(sorted.size()));
    const double eq_w = 100.0 / static_cast<double>(summary_.holdings.size());

    for (int r = 0; r < static_cast<int>(sorted.size()); ++r) {
        const auto& h = sorted[r];
        alloc_table_->setRowHeight(r, opt_row_height(28));
        const double diff = h.weight - eq_w;

        auto set = [&](int col, const QString& text, const char* color = nullptr) {
            auto* item = new QTableWidgetItem(text);
            item->setTextAlignment(col == 0 ? (Qt::AlignLeft | Qt::AlignVCenter) : (Qt::AlignRight | Qt::AlignVCenter));
            if (color)
                item->setForeground(QColor(color));
            alloc_table_->setItem(r, col, item);
        };

        set(0, h.symbol, ui::colors::CYAN);
        set(1, ui::formatting::pct(h.weight, 1, false), ui::colors::AMBER);
        set(2, ui::formatting::money(h.market_value, currency_));
        set(3, ui::formatting::pct(diff, 1), diff >= 0.0 ? ui::colors::POSITIVE : ui::colors::NEGATIVE);
    }
    alloc_table_->setUpdatesEnabled(true);
}

// ── Efficient frontier chart ──────────────────────────────────────────────────

void PortfolioOptimizationView::update_frontier(const QJsonObject& root) {
    const QJsonArray pts = root["frontier"].toArray();
    if (pts.isEmpty()) {
        // No frontier this run: say so, not the previous run's chart.
        if (frontier_stack_ && frontier_empty_)
            frontier_stack_->setCurrentWidget(frontier_empty_);
        return;
    }
    const QJsonObject frontier_current = root["current"].toObject();
    const QJsonObject frontier_assets = root["assets"].toObject();

    auto* chart = frontier_chart_->chart();
    chart->removeAllSeries();
    const auto old_axes = chart->axes();
    for (auto* ax : old_axes) {
        // removeAxis() only detaches — without the delete every re-run leaked
        // both axes.
        chart->removeAxis(ax);
        delete ax;
    }

    // Frontier line
    auto* frontier_series = new QLineSeries;
    frontier_series->setName(tr("Efficient frontier"));
    frontier_series->setColor(QColor(QString(ui::colors::AMBER())));
    QPen fp{QColor(QString(ui::colors::AMBER()))};
    fp.setWidth(2);
    frontier_series->setPen(fp);

    // Hover read-out for every point.
    auto hover_tip = [this](QXYSeries* s, std::function<QString(const QPointF&)> text) {
        connect(s, &QXYSeries::hovered, this, [text = std::move(text)](const QPointF& p, bool state) {
            if (state)
                QToolTip::showText(QCursor::pos(), text(p));
            else
                QToolTip::hideText();
        });
    };
    auto point_text = [](const QString& title, const QPointF& p) {
        return tr("%1\nVolatility %2 · Expected return %3")
            .arg(title, ui::formatting::pct(p.x(), 1, false), ui::formatting::pct(p.y(), 1));
    };
    frontier_series->setPointsVisible(true);
    hover_tip(frontier_series, [point_text](const QPointF& p) { return point_text(tr("Frontier portfolio"), p); });

    // Sharpe-optimal point (max sharpe on frontier)
    auto* opt_series = new QScatterSeries;
    opt_series->setName(tr("Max Sharpe"));
    opt_series->setPointLabelsVisible(true);
    opt_series->setPointLabelsFormat(tr("Max Sharpe"));
    opt_series->setPointLabelsColor(QColor(ui::colors::TEXT_PRIMARY()));
    hover_tip(opt_series, [point_text](const QPointF& p) { return point_text(tr("Max-Sharpe portfolio"), p); });
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

    // Today's mix and each holding on its own, on the same axes — the distance from the
    // "Current" marker to the curve is what optimizing buys.
    QScatterSeries* cur_series = nullptr;
    if (!frontier_current.isEmpty()) {
        cur_series = new QScatterSeries;
        cur_series->setName(tr("Current"));
        cur_series->setColor(QColor(ui::colors::CYAN()));
        cur_series->setMarkerShape(QScatterSeries::MarkerShapeRectangle);
        cur_series->setMarkerSize(10);
        cur_series->setPointLabelsVisible(true);
        cur_series->setPointLabelsFormat(tr("Current"));
        cur_series->setPointLabelsColor(QColor(ui::colors::TEXT_PRIMARY()));
        hover_tip(cur_series, [point_text](const QPointF& p) { return point_text(tr("Your current mix"), p); });
        const double cx = frontier_current["volatility"].toDouble() * 100.0;
        const double cy = frontier_current["return"].toDouble() * 100.0;
        cur_series->append(cx, cy);
        min_x = std::min(min_x, cx);
        max_x = std::max(max_x, cx);
        min_y = std::min(min_y, cy);
        max_y = std::max(max_y, cy);
        chart->addSeries(cur_series);
    }
    // Each holding on its own, labelled with its ticker (one series per holding so the
    // label can carry the name; only the first shows in the legend).
    QVector<QScatterSeries*> asset_series_list;
    if (!frontier_assets.isEmpty()) {
        for (auto it = frontier_assets.begin(); it != frontier_assets.end(); ++it) {
            const auto o = it.value().toObject();
            const double ax = o["volatility"].toDouble() * 100.0;
            const double ay = o["return"].toDouble() * 100.0;
            if (!std::isfinite(ax) || !std::isfinite(ay) || ax <= 0.0)
                continue;
            auto* s = new QScatterSeries;
            s->setName(tr("Holdings"));
            s->setColor(QColor(ui::colors::TEXT_TERTIARY()));
            s->setMarkerSize(6);
            s->append(ax, ay);
            QString label = it.key();
            label.replace(QLatin1Char('@'), QLatin1Char(' ')); // '@' starts a label token
            s->setPointLabelsFormat(label);
            s->setPointLabelsVisible(true);
            s->setPointLabelsColor(QColor(ui::colors::TEXT_SECONDARY()));
            s->setPointLabelsFont(ui::fonts::font(-4));
            const QString sym = it.key();
            hover_tip(s, [point_text, sym](const QPointF& p) { return point_text(sym, p); });
            chart->addSeries(s);
            asset_series_list.append(s);
            min_x = std::min(min_x, ax);
            max_x = std::max(max_x, ax);
            min_y = std::min(min_y, ay);
            max_y = std::max(max_y, ay);
        }
        for (int i = 1; i < asset_series_list.size(); ++i)
            for (auto* m : chart->legend()->markers(asset_series_list[i]))
                m->setVisible(false);
    }

    auto* x_axis = new QValueAxis;
    x_axis->setTitleText(tr("Volatility (annualised, %)"));
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
    if (cur_series)
        cur_series->attachAxis(x_axis);
    for (auto* s : asset_series_list)
        s->attachAxis(x_axis);

    auto* y_axis = new QValueAxis;
    y_axis->setTitleText(tr("Expected return (annualised, %)"));
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
    if (cur_series)
        cur_series->attachAxis(y_axis);
    for (auto* s : asset_series_list)
        s->attachAxis(y_axis);

    frontier_stack_->setCurrentWidget(frontier_chart_);
}

// ── Strategies comparison ─────────────────────────────────────────────────────

void PortfolioOptimizationView::update_strategies(const QJsonObject& root) {
    const QJsonObject comparison = root["comparison"].toObject();

    QStringList cats;
    QVector<double> rets, vols;
    strategies_table_->setUpdatesEnabled(false);
    strategies_table_->setRowCount(kOptStrategyCount);
    for (int r = 0; r < kOptStrategyCount; ++r) {
        const auto& row = kOptStrategies[r];
        strategies_table_->setRowHeight(r, opt_row_height(30));

        const auto obj = comparison.value(QLatin1String(row.key)).toObject();
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

        const QString label = tr(row.label);
        set(0, label, ui::colors::TEXT_PRIMARY);
        set(1, obj.isEmpty() ? QStringLiteral("--") : ui::formatting::pct(ret, 1, false), ui::colors::TEXT_PRIMARY);
        set(2, obj.isEmpty() ? QStringLiteral("--") : ui::formatting::pct(vol, 1, false), ui::colors::TEXT_PRIMARY);
        set(3, obj.isEmpty() ? QStringLiteral("--") : ui::formatting::number(sharpe, 2), ui::colors::CYAN);
        set(4, tr(row.desc), ui::colors::TEXT_TERTIARY);

        cats << label;
        rets << (obj.isEmpty() ? std::numeric_limits<double>::quiet_NaN() : ret);
        vols << (obj.isEmpty() ? std::numeric_limits<double>::quiet_NaN() : vol);
    }
    strategies_table_->setUpdatesEnabled(true);

    if (strategies_chart_) {
        strategies_chart_->set_categories(cats);
        strategies_chart_->set_series({ui::BarSeries{tr("Expected return"), rets, ui::colors::chart_color(0)},
                                       ui::BarSeries{tr("Volatility"), vols, ui::colors::chart_color(1)}});
        strategies_chart_->set_state(comparison.isEmpty() ? ui::PaintedChart::Empty : ui::PaintedChart::Ready,
                                     tr("This run compared no strategies."));
    }
}

// ── Compare weights ───────────────────────────────────────────────────────────

void PortfolioOptimizationView::update_compare(const QJsonObject& root) {
    const QJsonObject comparison = root["comparison"].toObject();
    const QSet<QString> priced = opt_priced(root);

    static const struct {
        const char* key;
        const char* label;
    } kCols[] = {
        {"max_sharpe", QT_TR_NOOP("MAX SHARPE")}, {"min_volatility", QT_TR_NOOP("MIN VOL")},
        {"risk_parity", QT_TR_NOOP("RISK PARITY")}, {"hrp", QT_TR_NOOP("HRP")},
        {"equal_weight", QT_TR_NOOP("EQUAL WT")},
    };
    const int n_methods = static_cast<int>(sizeof(kCols) / sizeof(kCols[0]));
    const int n_rows = static_cast<int>(summary_.holdings.size());

    QStringList headers;
    QStringList method_labels;
    headers << tr("SYMBOL");
    for (int c = 0; c < n_methods; ++c) {
        headers << tr(kCols[c].label);
        method_labels << tr(kCols[c].label);
    }
    QStringList row_labels;
    QVector<QVector<double>> matrix;

    compare_table_->setUpdatesEnabled(false);
    compare_table_->setRowCount(n_rows);
    compare_table_->setColumnCount(1 + n_methods);
    compare_table_->setHorizontalHeaderLabels(headers);

    for (int r = 0; r < n_rows; ++r) {
        const QString& sym = summary_.holdings[r].symbol;
        compare_table_->setRowHeight(r, opt_row_height(28));

        auto* sym_item = new QTableWidgetItem(sym);
        sym_item->setTextAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        sym_item->setForeground(QColor(ui::colors::CYAN()));
        compare_table_->setItem(r, 0, sym_item);

        // No price history: the optimizer left it out, so a "0.0%" would read as a real
        // zero allocation.
        const bool no_data = !priced.isEmpty() && !priced.contains(sym);
        QVector<double> mrow;
        for (int c = 0; c < n_methods; ++c) {
            const auto obj = comparison.value(QLatin1String(kCols[c].key)).toObject();
            const bool blank = obj.isEmpty() || no_data;
            const double w = blank ? 0.0 : obj["weights"].toObject().value(sym).toDouble() * 100.0;

            auto* item = new QTableWidgetItem(blank ? QStringLiteral("--") : ui::formatting::pct(w, 1, false));
            item->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
            item->setForeground(QColor(blank ? ui::colors::TEXT_TERTIARY() : ui::colors::AMBER()));
            compare_table_->setItem(r, 1 + c, item);
            mrow << (blank ? std::numeric_limits<double>::quiet_NaN() : w);
        }
        row_labels << sym;
        matrix << mrow;
    }
    compare_table_->setUpdatesEnabled(true);

    if (compare_heat_) {
        if (comparison.isEmpty() || n_rows == 0) {
            compare_heat_->clear();
            compare_heat_->set_state(ui::PaintedChart::Empty, tr("This run compared no strategies."));
        } else {
            compare_heat_->set_matrix(row_labels, method_labels, matrix);
            compare_heat_->set_state(ui::PaintedChart::Ready);
        }
    }
}

// ── Risk decomposition ──────────────────────────────────────────────────────────
// Renders the per-asset marginal risk contribution computed by the optimizer
// (risk_contributions / marginal_risk / asset_volatility from the Python output).
void PortfolioOptimizationView::update_risk(const QJsonObject& root) {
    const auto rc = root["risk_contributions"].toObject();
    const auto av = root["asset_volatility"].toObject();
    const auto mr = root["marginal_risk"].toObject();
    const auto weights = root["weights"].toObject();
    if (!risk_table_)
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

    QStringList cats;
    QVector<double> wv, rv;
    risk_table_->setUpdatesEnabled(false);
    risk_table_->setRowCount(static_cast<int>(rows.size()));
    for (int r = 0; r < static_cast<int>(rows.size()); ++r) {
        const auto& row = rows[r];
        risk_table_->setRowHeight(r, opt_row_height(28));
        auto set = [&](int col, const QString& text, const char* color = nullptr) {
            auto* item = new QTableWidgetItem(text);
            item->setTextAlignment(col == 0 ? (Qt::AlignLeft | Qt::AlignVCenter) : (Qt::AlignRight | Qt::AlignVCenter));
            if (color)
                item->setForeground(QColor(color));
            risk_table_->setItem(r, col, item);
        };
        set(0, row.sym, ui::colors::CYAN);
        set(1, ui::formatting::pct(row.w, 1, false));
        set(2, ui::formatting::pct(row.vol, 1, false));
        set(3, ui::formatting::number(row.mrc, 3));
        // Flag assets whose share of risk exceeds their share of capital (risk concentration):
        // a marker as well as the warning colour.
        const bool concentrated = row.rc > row.w + 0.5;
        set(4,
            concentrated ? QStringLiteral("▲ ") + ui::formatting::pct(row.rc, 1, false)
                         : ui::formatting::pct(row.rc, 1, false),
            concentrated ? ui::colors::WARNING : ui::colors::AMBER);
        cats << row.sym;
        wv << row.w;
        rv << row.rc;
    }
    risk_table_->setUpdatesEnabled(true);
    risk_table_->setToolTip(tr("▲ = the holding carries more of the risk than of the capital."));

    if (risk_chart_) {
        risk_chart_->set_categories(cats);
        risk_chart_->set_series({ui::BarSeries{tr("Optimal weight"), wv, ui::colors::chart_color(0)},
                                 ui::BarSeries{tr("Risk share"), rv, ui::colors::chart_color(1)}});
        risk_chart_->set_state(cats.isEmpty() ? ui::PaintedChart::Empty : ui::PaintedChart::Ready,
                               tr("This run returned no risk decomposition."));
    }
}

// ── Stress scenarios ────────────────────────────────────────────────────────────
// Applies historical crisis shocks to the portfolio's current asset-class mix.
// Runs off current holdings — no optimization required.
void PortfolioOptimizationView::update_stress() {
    if (!stress_table_)
        return;
    if (summary_.holdings.isEmpty()) {
        stress_table_->setRowCount(0);
        stress_stack_->setCurrentWidget(stress_empty_);
        return;
    }

    // Same model as the RISK view (screens/portfolio/PortfolioStress.h): equity lines
    // move by their own beta when the analytics run has one.
    namespace ps = portfolio_stress;
    const bool have_betas = !analytics_.holdings.isEmpty() && analytics_.portfolio_id == summary_.portfolio.id;
    const QHash<QString, portfolio::HoldingRisk>* risk = have_betas ? &analytics_.holdings : nullptr;
    const int n = static_cast<int>(std::size(ps::kScenarios));
    stress_table_->setUpdatesEnabled(false);
    stress_table_->setRowCount(n);
    for (int i = 0; i < n; ++i) {
        const auto& sc = ps::kScenarios[i];
        // Base currency and valued total from the summary (FX leg included, cash not shocked).
        const ps::Result res = ps::apply(sc, summary_, risk);
        const double impact_pct = res.impact_pct;
        const double loss = res.pnl;

        stress_table_->setRowHeight(i, opt_row_height(28));
        auto set = [&](int col, const QString& text, const char* color = nullptr) {
            auto* item = new QTableWidgetItem(text);
            item->setTextAlignment(col <= 1 ? (Qt::AlignLeft | Qt::AlignVCenter) : (Qt::AlignRight | Qt::AlignVCenter));
            if (color)
                item->setForeground(QColor(color));
            stress_table_->setItem(i, col, item);
        };
        set(0, ps::scenario_name(sc), ui::colors::TEXT_PRIMARY);
        set(1, ps::scenario_description(sc), ui::colors::TEXT_TERTIARY);
        const char* ic = impact_pct >= 0 ? ui::colors::POSITIVE : ui::colors::NEGATIVE;
        set(2, ui::formatting::pct(impact_pct, 1), ic);
        set(3, ui::formatting::signed_money(loss, currency_, ui::formatting::Compact::None, 0), ic);
    }
    stress_table_->setUpdatesEnabled(true);
    stress_stack_->setCurrentWidget(stress_table_);
}

// ── Black-Litterman ─────────────────────────────────────────────────────────────
// Shows market-implied equilibrium returns plus the optimized weights. When the
// B-L method was the one run, the final column is the Black-Litterman allocation.
void PortfolioOptimizationView::update_black_litterman(const QJsonObject& root) {
    const auto implied = root["implied_returns"].toObject();
    const auto opt_weights = root["weights"].toObject();
    const QSet<QString> priced = opt_priced(root);
    if (!bl_table_)
        return;

    const bool is_bl = root["strategy"].toString() == "black_litterman";
    const auto posterior = root["posterior_returns"].toObject();
    bl_table_->setHorizontalHeaderLabels({tr("SYMBOL"), tr("IMPLIED RETURN"), tr("POSTERIOR RETURN"), tr("CURRENT WT"),
                                          is_bl ? tr("B-L WT") : tr("OPTIMIZED WT")});

    bl_table_->setUpdatesEnabled(false);
    bl_table_->setRowCount(static_cast<int>(summary_.holdings.size()));
    for (int r = 0; r < static_cast<int>(summary_.holdings.size()); ++r) {
        const auto& h = summary_.holdings[r];
        bl_table_->setRowHeight(r, opt_row_height(28));
        auto set = [&](int col, const QString& text, const char* color = nullptr) {
            auto* item = new QTableWidgetItem(text);
            item->setTextAlignment(col == 0 ? (Qt::AlignLeft | Qt::AlignVCenter) : (Qt::AlignRight | Qt::AlignVCenter));
            if (color)
                item->setForeground(QColor(color));
            bl_table_->setItem(r, col, item);
        };
        set(0, h.symbol, ui::colors::CYAN);
        // No price history (or no implied return in this run): no implied return, no
        // optimized weight.
        if ((!priced.isEmpty() && !priced.contains(h.symbol)) || !implied.contains(h.symbol)) {
            set(1, QStringLiteral("--"), ui::colors::TEXT_TERTIARY);
            set(2, QStringLiteral("--"), ui::colors::TEXT_TERTIARY);
            set(3, ui::formatting::pct(h.weight, 1, false));
            set(4, QStringLiteral("--"), ui::colors::TEXT_TERTIARY);
            continue;
        }
        const double imp = implied.value(h.symbol).toDouble() * 100.0;
        const double ow = opt_weights.value(h.symbol).toDouble(h.weight / 100.0) * 100.0;
        set(1, ui::formatting::pct(imp, 1), imp >= 0 ? ui::colors::POSITIVE : ui::colors::NEGATIVE);
        if (posterior.contains(h.symbol)) {
            const double post = posterior.value(h.symbol).toDouble() * 100.0;
            set(2, ui::formatting::pct(post, 1), post >= 0 ? ui::colors::POSITIVE : ui::colors::NEGATIVE);
        } else {
            set(2, QStringLiteral("--"), ui::colors::TEXT_TERTIARY);
        }
        set(3, ui::formatting::pct(h.weight, 1, false));
        set(4, ui::formatting::pct(ow, 1, false), ui::colors::AMBER);
    }
    bl_table_->setUpdatesEnabled(true);
}

// ── BACKTEST ──────────────────────────────────────────────────────────────────

void PortfolioOptimizationView::apply_backtest_view() {
    if (!backtest_pane_ || run_key_.isEmpty())
        return;
    const int years = backtest_period_cb_ ? backtest_period_cb_->currentData().toInt() : 1;
    QVariantMap in;
    in.insert(QStringLiteral("years"), years);
    in.insert(QStringLiteral("optimal"), backtest_optimal_);
    if (backtest_optimal_)
        in.insert(QStringLiteral("weights"),
                  QString::fromUtf8(QJsonDocument(last_opt_weights_).toJson(QJsonDocument::Compact)));
    // One remembered result per kind of weights: switching CURRENT / OPTIMAL shows the
    // other one at once.
    backtest_pane_->set_view(run_key_ + (backtest_optimal_ ? QStringLiteral("|optimal") : QStringLiteral("|current")));
    backtest_pane_->set_signature(ui::autoload::signature_of(in));
    backtest_pane_->set_loading_text(tr("Backtesting your weights on %n year(s) of prices…", "", years));
}

void PortfolioOptimizationView::run_backtest(bool optimal) {
    if (summary_.holdings.isEmpty() || !backtest_pane_)
        return;
    if (optimal && last_opt_weights_.isEmpty())
        return;
    backtest_optimal_ = optimal;
    apply_backtest_view();
    backtest_pane_->run();
}

void PortfolioOptimizationView::start_backtest(quint64 request_id) {
    QJsonArray symbols, weights;
    if (backtest_optimal_) {
        // The optimizer's own numbers (not the table text).
        for (auto it = last_opt_weights_.begin(); it != last_opt_weights_.end(); ++it) {
            const double w = it.value().toDouble(std::numeric_limits<double>::quiet_NaN());
            if (!std::isfinite(w) || w <= 1e-9)
                continue; // a zero / missing weight has nothing to backtest
            symbols.append(it.key());
            weights.append(w);
        }
    } else {
        for (const auto& h : summary_.holdings) {
            if (!h.valued() || !std::isfinite(h.weight))
                continue; // an unvalued position has no weight to replay
            symbols.append(h.symbol);
            weights.append(h.weight / 100.0);
        }
    }
    if (symbols.isEmpty()) {
        backtest_pane_->fail(request_id,
                             backtest_optimal_
                                 ? tr("No optimisation result is on screen yet — run the optimisation first.")
                                 : tr("No position can be valued yet — prices are still loading. Try again in a "
                                      "moment."));
        return;
    }

    const int years = backtest_period_cb_ ? backtest_period_cb_->currentData().toInt() : 1;
    BacktestRun run;
    run.request_id = request_id;
    run.optimal = backtest_optimal_;
    run.years = years;
    run.start = QDate::currentDate().addYears(-years).toString(Qt::ISODate);
    run.end = QDate::currentDate().addDays(-1).toString(Qt::ISODate);

    QJsonObject args;
    args["symbols"] = symbols;
    args["weights"] = weights;
    args["startDate"] = run.start;
    args["endDate"] = run.end;
    args["initialCapital"] = summary_.total_market_value > 0 ? summary_.total_market_value : 100000.0;
    QJsonObject strategy;
    strategy["type"] = "buy_and_hold";
    strategy["params"] = QJsonObject{};
    args["strategy"] = strategy;

    const quint64 run_id =
        fincept::services::backtest::BacktestingService::instance().execute(QStringLiteral("vectorbt"),
                                                                            QStringLiteral("backtest"), args);
    backtest_runs_.insert(run_id, run);
}

void PortfolioOptimizationView::cancel_backtest(quint64 request_id) {
    for (auto it = backtest_runs_.begin(); it != backtest_runs_.end(); ++it) {
        if (it.value().request_id != request_id)
            continue;
        const quint64 run_id = it.key();
        backtest_runs_.erase(it);
        fincept::services::backtest::BacktestingService::instance().cancel(run_id);
        return;
    }
}

void PortfolioOptimizationView::render_backtest(const QJsonObject& payload) {
    backtest_rendered_ = payload;
    const QJsonObject perf = payload.value(QStringLiteral("performance")).toObject();

    if (backtest_caption_) {
        const bool optimal = payload.value(QStringLiteral("optimal")).toBool();
        const int years = payload.value(QStringLiteral("years")).toInt(1);
        QString text = optimal ? tr("Buy-and-hold of the optimal weights") : tr("Buy-and-hold of your current weights");
        text += QStringLiteral("  ·  ") + tr("%n year(s)", "", years);
        const QString start = payload.value(QStringLiteral("start")).toString();
        const QString end = payload.value(QStringLiteral("end")).toString();
        if (!start.isEmpty() && !end.isEmpty())
            text += QStringLiteral("  ·  ") + tr("%1 to %2").arg(start, end);
        const QString source = payload.value(QStringLiteral("source")).toString();
        if (!source.isEmpty())
            text += QStringLiteral("  ·  ") + tr("prices: %1").arg(source);
        backtest_caption_->setText(text);
    }
    if (backtest_warn_) {
        const bool synthetic = payload.value(QStringLiteral("synthetic")).toBool();
        backtest_warn_->setText(synthetic ? QStringLiteral("⚠ ") +
                                                tr("Prices could not be downloaded — this run used synthetic data "
                                                   "and does not describe your portfolio.")
                                          : QString());
        backtest_warn_->setVisible(synthetic);
    }

    if (backtest_kpis_) {
        auto fill = [&](const QString& tile, const QString& key, int kind) {
            auto* t = backtest_kpis_->tile(tile);
            if (!t)
                return;
            t->set_state(ui::StatTile::State::Normal);
            t->set_tone(ui::StatTile::Tone::Neutral);
            const QJsonValue v = perf.value(key);
            if (!v.isDouble() || !std::isfinite(v.toDouble())) {
                t->set_value(ui::formatting::placeholder());
                return;
            }
            const double d = v.toDouble();
            switch (kind) {
                case 0: // signed return: sign shown, coloured by direction
                    t->set_value(ui::formatting::fraction_pct(d, 2, true));
                    t->set_tone(d > 0 ? ui::StatTile::Tone::Positive
                                      : d < 0 ? ui::StatTile::Tone::Negative : ui::StatTile::Tone::Neutral);
                    break;
                case 1: // drawdown magnitude, shown as a loss
                    t->set_value(ui::formatting::fraction_pct(-std::abs(d), 2, true));
                    break;
                case 2: // unsigned fraction
                    t->set_value(ui::formatting::fraction_pct(d, 1, false));
                    break;
                default: // ratio
                    t->set_value(ui::formatting::number(d, 2));
                    break;
            }
        };
        fill(QStringLiteral("total"), QStringLiteral("totalReturn"), 0);
        fill(QStringLiteral("ann"), QStringLiteral("annualizedReturn"), 0);
        fill(QStringLiteral("sharpe"), QStringLiteral("sharpeRatio"), 3);
        fill(QStringLiteral("maxdd"), QStringLiteral("maxDrawdown"), 1);
        fill(QStringLiteral("vol"), QStringLiteral("volatility"), 2);
        fill(QStringLiteral("sortino"), QStringLiteral("sortinoRatio"), 3);
        fill(QStringLiteral("winners"), QStringLiteral("winRate"), 2);
        fill(QStringLiteral("calmar"), QStringLiteral("calmarRatio"), 3);
        if (auto* t = backtest_kpis_->tile(QStringLiteral("winners")))
            t->set_sub_label(tr("of positions in profit"));
    }

    // Equity + drawdown curves.
    backtest_chart_->clear();
    QVector<QPointF> eq, dd;
    for (const auto& v : payload.value(QStringLiteral("equity")).toArray()) {
        const QJsonObject o = v.toObject();
        const QDate d = QDate::fromString(o.value("date").toString().left(10), Qt::ISODate);
        if (!d.isValid() || !o.value("equity").isDouble())
            continue;
        const double x = static_cast<double>(QDateTime(d, QTime(0, 0), QTimeZone::UTC).toMSecsSinceEpoch());
        eq.append({x, o.value("equity").toDouble()});
        dd.append({x, o.value("drawdown").toDouble() * (std::abs(o.value("drawdown").toDouble()) <= 1.0 ? 100.0 : 1.0)});
    }
    if (backtest_chart_->pane_count() < 2)
        backtest_chart_->add_pane(0.35);
    const QString ccy = currency_;
    ui::TimeSeriesChart::SeriesSpec es;
    es.name = tr("Equity");
    es.points = eq;
    es.style = ui::TimeSeriesChart::Style::Area;
    es.color = ui::TimeSeriesChart::ColorRole::Accent;
    es.rebase = false;
    es.formatter = [ccy](double v) { return ui::formatting::money(v, ccy); };
    backtest_chart_->add_series(es);
    backtest_chart_->set_value_formatter(ui::TimeSeriesChart::Axis::Left, [ccy](double v) {
        return ui::formatting::money(v, ccy, ui::formatting::Compact::FromSettings);
    });
    ui::TimeSeriesChart::SeriesSpec ds;
    ds.name = tr("Drawdown");
    ds.points = dd;
    ds.pane = 1;
    ds.style = ui::TimeSeriesChart::Style::Area;
    ds.color = ui::TimeSeriesChart::ColorRole::Negative;
    ds.baseline = 0.0;
    ds.rebase = false;
    ds.formatter = [](double v) { return ui::formatting::pct(v, 2); };
    backtest_chart_->add_series(ds);
    backtest_chart_->set_value_formatter(ui::TimeSeriesChart::Axis::Left,
                                         [](double v) { return ui::formatting::pct(v, 0); }, 1);
    backtest_chart_->set_state(eq.isEmpty() ? ui::TimeSeriesChart::State::Empty : ui::TimeSeriesChart::State::Ready,
                               tr("The run returned no equity curve."));
    backtest_chart_->reset_x_range();
}

} // namespace fincept::screens
