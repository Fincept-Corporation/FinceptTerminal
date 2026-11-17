// src/screens/ma_analytics/MAModulePanel.cpp
#include "screens/ma_analytics/MAModulePanel.h"
#include "screens/ma_analytics/MaDealGrid.h"
#include "screens/ma_analytics/MaFootballField.h"
#include "screens/ma_analytics/MaResultFormat.h"

#include "core/logging/Logger.h"
#include "services/ma_analytics/MAAnalyticsService.h"
#include "storage/cache/CacheManager.h"
#include "ui/charts/BarChart.h"
#include "ui/formatting/FormatWidgets.h"
#include "ui/charts/Gauge.h"
#include "ui/charts/RangeBar.h"
#include "ui/theme/Theme.h"
#include "ui/theme/ThemeManager.h"
#include "ui/widgets/AutoLoadPane.h"
#include "ui/widgets/AutoLoadPolicy.h"
#include "ui/widgets/InputMemory.h"
#include "ui/widgets/EmptyState.h"
#include "ui/widgets/ResultLayout.h"
#include "ui/widgets/ResultView.h"
#include "ui/widgets/SensitivityTable.h"
#include "ui/widgets/symbol/SymbolSearchBox.h"

#include <QCoreApplication>
#include <QFontMetrics>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLineEdit>
#include <QRegularExpression>
#include <QScrollArea>
#include <QTableWidget>

#include <algorithm>
#include <cmath>
#include <initializer_list>
#include <limits>
#include <memory>

namespace fincept::screens {

using namespace fincept::services::ma;

namespace {
bool ma_panel_no_auto(const QString& context); // defined with the result helpers below
} // namespace

// ── Helpers: create styled input widgets ─────────────────────────────────────

QDoubleSpinBox* MAModulePanel::make_double_spin(double min, double max, double val, int decimals, const QString& suffix,
                                                QWidget* parent) {
    auto* spin = new QDoubleSpinBox(parent);
    spin->setRange(min, max);
    spin->setValue(val);
    spin->setDecimals(decimals);
    if (!suffix.isEmpty())
        spin->setSuffix(suffix);
    spin->setStyleSheet(QString("QDoubleSpinBox { background:%1; color:%2; border:1px solid %3;"
                                "font-family:%4; font-size:%5px; padding:4px 6px; }"
                                "QDoubleSpinBox:focus { border-color:%6; }")
                            .arg(ui::colors::BG_RAISED(), ui::colors::TEXT_PRIMARY(), ui::colors::BORDER_MED())
                            .arg(ui::fonts::DATA_FAMILY)
                            .arg(ui::fonts::SMALL.get())
                            .arg(ui::colors::legible(module_.color)));
    return spin;
}

QSpinBox* MAModulePanel::make_int_spin(int min, int max, int val, QWidget* parent) {
    auto* spin = new QSpinBox(parent);
    spin->setRange(min, max);
    spin->setValue(val);
    spin->setStyleSheet(QString("QSpinBox { background:%1; color:%2; border:1px solid %3;"
                                "font-family:%4; font-size:%5px; padding:4px 6px; }"
                                "QSpinBox:focus { border-color:%6; }")
                            .arg(ui::colors::BG_RAISED(), ui::colors::TEXT_PRIMARY(), ui::colors::BORDER_MED())
                            .arg(ui::fonts::DATA_FAMILY)
                            .arg(ui::fonts::SMALL.get())
                            .arg(ui::colors::legible(module_.color)));
    return spin;
}

QPushButton* MAModulePanel::make_run_button(const QString& text, QWidget* parent) {
    auto* btn = new QPushButton(text, parent);
    btn->setCursor(Qt::PointingHandCursor);
    btn->setStyleSheet(QString("QPushButton { background:%1; color:%2; font-family:%3; font-size:%4px;"
                               "font-weight:700; border:none; padding:8px 20px;"
                               "letter-spacing:1px; }"
                               "QPushButton:hover { background:%5; }"
                               "QPushButton:pressed { background:%6; }"
                               "QPushButton:disabled { background:%7; color:%8; }")
                           .arg(ui::colors::legible(module_.color))
                           .arg(ui::colors::TEXT_ON_ACCENT())
                           .arg(ui::fonts::DATA_FAMILY)
                           .arg(ui::fonts::SMALL.get())
                           .arg(ui::colors::legible(module_.color.lighter(115)))
                           .arg(ui::colors::legible(module_.color.darker(115)))
                           .arg(ui::colors::BG_RAISED())
                           .arg(ui::colors::TEXT_TERTIARY()));
    // Connected before the caller's own clicked handler, so the loading state is
    // up before the run starts (a cached result can arrive synchronously).
    connect(btn, &QPushButton::clicked, this, [this, btn]() { begin_run(btn); });
    run_buttons_.append(btn);
    return btn;
}

QWidget* MAModulePanel::build_input_row(const QString& label, QWidget* input, QWidget* parent) {
    auto* row = new QWidget(parent);
    auto* hl = new QHBoxLayout(row);
    hl->setContentsMargins(0, 2, 0, 2);
    hl->setSpacing(8);
    auto* lbl = new QLabel(row);
    lbl->setFixedWidth(std::max(160, QFontMetrics(ui::fonts::font(-1)).horizontalAdvance(QString(28, QLatin1Char('M'))))); // longest label: 27 characters
    // Labels ending in "($)" are a currency-unit hint — bind them so the symbol
    // tracks the preferred currency live; others are plain text.
    if (label.endsWith("($)")) {
        QString base = label;
        base.chop(3); // strip "($)"
        // The display currency's unit, kept in step with the format settings.
        fincept::ui::formatting::bind_currency_label(lbl, base + "(%1)");
    } else {
        lbl->setText(label);
    }
    lbl->setStyleSheet(QString("color:%1; font-size:%2px; font-family:%3;")
                           .arg(ui::colors::TEXT_SECONDARY())
                           .arg(ui::fonts::SMALL.get())
                           .arg(ui::fonts::DATA_FAMILY));
    hl->addWidget(lbl);
    hl->addWidget(input, 1);
    // "Yahoo, 17/11/26" once the field was filled from a loaded company (MaCompanyPrefill.cpp); hidden until then.
    auto* source = new QLabel(row);
    source->setObjectName(QStringLiteral("maFieldSource"));
    source->setStyleSheet(QString("QLabel#maFieldSource { color:%1; font-size:%2px; background:transparent; }")
                              .arg(ui::colors::TEXT_TERTIARY())
                              .arg(ui::fonts::px(-3)));
    source->setVisible(false);
    hl->addWidget(source);
    source_labels_.insert(input, source);
    input_labels_.insert(input, lbl->text());
    return row;
}

// ── Constructor ──────────────────────────────────────────────────────────────

MAModulePanel::MAModulePanel(const ModuleInfo& info, QWidget* parent) : QWidget(parent), module_(info) {
    build_ui();
    connect_service();
    refresh_theme();
    // Amounts follow the display currency / number format: redraw the result on screen after a format change
    // (inputs are amounts in the display currency as typed; nothing stored is converted).
    fincept::ui::formatting::on_format_changed(this, [this]() {
        if (pane_ && pane_->has_result() && pane_->state() == ui::AutoLoadPane::State::Content)
            render_payload(pane_->payload());
    });
}

MAModulePanel::~MAModulePanel() {
    // The pane is destroyed after this object's members: stop its run now, and keep it from
    // calling back into members that no longer exist.
    if (pane_) {
        if (pane_->is_running())
            MAAnalyticsService::instance().cancel(pane_->running_view());
        pane_->set_cancel({});
    }
}

void MAModulePanel::connect_service() {
    auto& svc = MAAnalyticsService::instance();
    connect(&svc, &MAAnalyticsService::result_ready, this, &MAModulePanel::on_result_ready);
    connect(&svc, &MAAnalyticsService::error_occurred, this, &MAModulePanel::on_error);
    connect(&svc, &MAAnalyticsService::input_required, this, &MAModulePanel::on_input_required);
}

// ── Build UI ─────────────────────────────────────────────────────────────────

void MAModulePanel::build_ui() {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // Module header bar (styled through its object name: a bare rule would also draw the border on every child)
    header_bar_ = new QWidget(this);
    header_bar_->setObjectName(QStringLiteral("maPanelHeader"));
    header_bar_->setMinimumHeight(36);
    auto* hhl = new QHBoxLayout(header_bar_);
    hhl->setContentsMargins(16, 0, 16, 0);
    header_title_ = new QLabel(module_.label.toUpper(), header_bar_);
    hhl->addWidget(header_title_);

    auto* div = new QWidget(header_bar_);
    div->setFixedSize(1, 14);
    div->setObjectName("maPanelDivider");
    hhl->addWidget(div);

    header_category_ = new QLabel(module_.category.toUpper(), header_bar_);
    hhl->addWidget(header_category_);
    hhl->addStretch();

    status_label_ = new QLabel(header_bar_);
    hhl->addWidget(status_label_);
    build_case_buttons(hhl); // SAVE CASE / CASES (MaCases.cpp)
    root->addWidget(header_bar_);

    // Scrollable content area
    auto* scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setStyleSheet("QScrollArea { border:none; background:transparent; }");

    QWidget* panel_content = nullptr;
    switch (module_.id) {
        case ModuleId::Valuation:
            panel_content = build_valuation_panel();
            break;
        case ModuleId::Merger:
            panel_content = build_merger_panel();
            break;
        case ModuleId::Deals:
            panel_content = build_deals_panel();
            break;
        case ModuleId::Startup:
            panel_content = build_startup_panel();
            break;
        case ModuleId::Fairness:
            panel_content = build_fairness_panel();
            break;
        case ModuleId::Industry:
            panel_content = build_industry_panel();
            break;
        case ModuleId::Advanced:
            panel_content = build_advanced_panel();
            break;
        case ModuleId::Comparison:
            panel_content = build_comparison_panel();
            break;
    }

    scroll->setWidget(panel_content);
    root->addWidget(scroll, 1);
    ensure_result_view();

    // Pages built already (the first lazy tab, eagerly added tabs) remember their inputs; lazily
    // built ones do so in build_sub_tab.
    for (const auto& pair : std::as_const(sub_tab_sources_))
        if (!pending_sub_tabs_.contains(pair.first))
            attach_memory(pair.first);
    attach_module_memory();

    // The pane follows the visible sub-tab (connected after the lazy build of its form).
    if (pane_ && sub_tabs_) {
        connect(sub_tabs_, &QTabWidget::currentChanged, this, [this](int) { select_view(); });
        select_view();
    }
}

// ═══════════════════════════════════════════════════════════════════════════════
// RESULT DISPLAY
// ═══════════════════════════════════════════════════════════════════════════════
//
// Every result is shown by the shared ui::ResultView (headline KPIs, charts,
// sortable tables, collapsible sections for nested objects, RAW toggle). What
// is M&A-specific lives here: the unit conventions of the corporateFinance
// scripts (rates mostly arrive already in percent, amounts in dollars, a few
// fields as fractions), each context's headline KPIs, and the charts the
// generic layout cannot infer (contribution split, football field, process
// gauge, Monte Carlo range).

namespace {

using fincept::screens::ma_fmt::ma_money_text;
using fincept::screens::ma_fmt::ma_num;
using fincept::screens::ma_fmt::ma_result_options;

// The analysis (service context) each sub-tab runs, by module and untranslated tab label.
QString ma_panel_tab_context(ModuleId module, const QString& source_label) {
    static const QHash<QString, QString> kValuation = {
        {QStringLiteral("DCF"), QStringLiteral("dcf")},
        {QStringLiteral("LBO Returns"), QStringLiteral("lbo_returns")},
        {QStringLiteral("LBO Model"), QStringLiteral("lbo_model")},
        {QStringLiteral("Debt Schedule"), QStringLiteral("lbo_debt_schedule")},
        {QStringLiteral("Sensitivity"), QStringLiteral("lbo_sensitivity")},
        {QStringLiteral("Trading Comps"), QStringLiteral("trading_comps")},
        {QStringLiteral("Precedent Txns"), QStringLiteral("precedent_txns")},
        {QStringLiteral("Football Field"), QStringLiteral("football_field")}};
    static const QHash<QString, QString> kMerger = {
        {QStringLiteral("Accretion/Dilution"), QStringLiteral("accretion_dilution")},
        {QStringLiteral("Synergies"), QStringLiteral("synergy_dcf")},
        {QStringLiteral("Pro Forma"), QStringLiteral("pro_forma")},
        {QStringLiteral("Sources & Uses"), QStringLiteral("sources_uses")},
        {QStringLiteral("Contribution"), QStringLiteral("contribution")},
        {QStringLiteral("Payment"), QStringLiteral("payment_structure")},
        {QStringLiteral("Earnout"), QStringLiteral("earnout")},
        {QStringLiteral("Exchange Ratio"), QStringLiteral("exchange_ratio")},
        {QStringLiteral("Collar"), QStringLiteral("collar")},
        {QStringLiteral("CVR"), QStringLiteral("cvr")}};
    static const QHash<QString, QString> kStartup = {
        {QStringLiteral("Berkus"), QStringLiteral("berkus")},
        {QStringLiteral("Scorecard"), QStringLiteral("scorecard")},
        {QStringLiteral("VC Method"), QStringLiteral("vc_method")},
        {QStringLiteral("First Chicago"), QStringLiteral("first_chicago")},
        {QStringLiteral("Risk Factor"), QStringLiteral("risk_factor")},
        {QStringLiteral("All Methods"), QStringLiteral("startup_comprehensive")}};
    // "Range Check" keeps the context id of the former "Fairness Analysis" (remembered inputs and results).
    static const QHash<QString, QString> kFairness = {
        {QStringLiteral("Range Check"), QStringLiteral("fairness_opinion")},
        {QStringLiteral("Premium Analysis"), QStringLiteral("premium_analysis")},
        {QStringLiteral("Process Quality"), QStringLiteral("process_quality")}};
    static const QHash<QString, QString> kIndustry = {
        {QStringLiteral("Technology"), QStringLiteral("tech_metrics")},
        {QStringLiteral("Healthcare"), QStringLiteral("healthcare_metrics")},
        {QStringLiteral("Financial Services"), QStringLiteral("finserv_metrics")}};
    static const QHash<QString, QString> kAdvanced = {
        {QStringLiteral("Monte Carlo"), QStringLiteral("monte_carlo")},
        {QStringLiteral("Regression"), QStringLiteral("regression")}};
    static const QHash<QString, QString> kComparison = {
        {QStringLiteral("Compare"), QStringLiteral("compare_deals")},
        {QStringLiteral("Rank"), QStringLiteral("rank_deals")},
        {QStringLiteral("Benchmark"), QStringLiteral("benchmark_premium")},
        {QStringLiteral("Payment"), QStringLiteral("payment_structures")},
        {QStringLiteral("Industry"), QStringLiteral("industry_deals")}};
    switch (module) {
        case ModuleId::Valuation:
            return kValuation.value(source_label);
        case ModuleId::Merger:
            return kMerger.value(source_label);
        case ModuleId::Startup:
            return kStartup.value(source_label);
        case ModuleId::Fairness:
            return kFairness.value(source_label);
        case ModuleId::Industry:
            return kIndustry.value(source_label);
        case ModuleId::Advanced:
            return kAdvanced.value(source_label);
        case ModuleId::Comparison:
            return kComparison.value(source_label);
        case ModuleId::Deals: // its own workspace: the deal table loads when the tab is built
            return QString();
    }
    return QString();
}

// Analyses that are not run automatically: each run calls a rate-limited outside data source
// (Trading Comps fetches every ticker's market data). They open on a preview with the RUN action.
bool ma_panel_no_auto(const QString& context) {
    return context == QLatin1String("trading_comps");
}

} // namespace

void MAModulePanel::ensure_result_view() {
    if (pane_ || !results_layout_)
        return;
    // COPY / CSV / PNG / ADD TO REPORT for the result on screen (MaResultActions.cpp).
    build_result_actions(results_container_);
    results_layout_->addWidget(actions_bar_);
    // ONE pane per module; its view is the visible sub-tab's analysis.
    pane_ = new ui::AutoLoadPane(results_container_);
    results_layout_->addWidget(pane_);

    auto* content = new QWidget(pane_->content_parent());
    auto* cl = new QVBoxLayout(content);
    cl->setContentsMargins(0, 0, 0, 0);
    cl->setSpacing(0);
    result_view_ = new ui::ResultView(content);
    result_view_->set_scrollable(false); // the module panel already scrolls
    cl->addWidget(result_view_);
    // A needs_data reply (no comparables yet) is a result too: a designed empty state, with the
    // actions that fill it.
    needs_empty_ = new ui::EmptyState(content);
    needs_empty_->set_alignment(Qt::AlignTop);
    needs_empty_->hide();
    cl->addWidget(needs_empty_);
    connect(needs_empty_, &ui::EmptyState::primary_clicked, this, [this]() {
        // Fill the comparables grid with the labelled sample set, then run.
        const QString context = pane_->view();
        MaDealGrid* grid = deal_grids_.value(context);
        if (!grid)
            return;
        grid->load_sample();
        pane_->run();
    });
    connect(needs_empty_, &ui::EmptyState::secondary_clicked, this,
            [this]() { emit open_module_requested(static_cast<int>(ModuleId::Deals)); });
    pane_->set_content(content);

    pane_->set_cache_key(QStringLiteral("ma_analytics/") + module_.key);
    pane_->set_auto_delay(300); // sub-tabs are flicked through
    // Deterministic local calculations: a remembered result whose inputs match (set_signature, from
    // the sub-tab's InputMemory) needs no re-run; changed inputs show "Inputs changed — RUN AGAIN".
    // (select_view gives the Manual Trading Comps view a day instead: market data ages.)
    pane_->set_max_age(ui::autoload::kNeverRefresh);
    // "Nothing to compare yet" depends on the Deal Database, not on this tab's inputs: a remembered
    // one is checked again once when shown.
    connect(pane_, &ui::AutoLoadPane::result_shown, this, [this](const QDateTime&, bool from_cache) {
        if (from_cache && pane_->payload().toObject().contains(QStringLiteral("needs_data")))
            pane_->invalidate();
    });
    pane_->set_status_label(status_label_);
    pane_->set_start([this](quint64 id) { start_run(id); });
    pane_->set_cancel([this](quint64 id) { cancel_run(id); });
    pane_->set_render([this](const QJsonValue& payload, const QDateTime&, bool) { render_payload(payload); });
    connect(pane_, &ui::AutoLoadPane::state_changed, this, [this](ui::AutoLoadPane::State) { update_result_actions(); });
}

void MAModulePanel::clear_results() {
    if (result_view_)
        result_view_->clear();
}

void MAModulePanel::begin_run(QPushButton* button) {
    // Some runs read inputs from other tabs (the LBO model uses the LBO
    // Returns tab's debt): every input must exist before the run handler,
    // which is connected after this one, reads them.
    build_all_sub_tabs();
    if (!pane_ || pane_clicking_)
        return; // a pane run (default, RETRY, preview RUN): its request id is already recorded
    // The user pressed RUN on the visible sub-tab: a new pane run of its view (it supersedes one in
    // flight, keeps the result on screen dimmed meanwhile); the button's handler, next, starts it.
    if (context_of_widget(button) != pane_->view())
        return;
    user_clicking_ = true;
    pane_->run();
    user_clicking_ = false;
}

void MAModulePanel::display_error(const QString& msg) {
    // An input check of the visible sub-tab's run: that run fails (message + RETRY).
    if (pane_)
        pane_->fail(run_ids_.value(pane_->view()), msg, tr("Check the inputs"));
    status_label_->setText(tr("Error"));
}

void MAModulePanel::display_result(const QJsonObject& envelope, const QString& context) {
    ensure_result_view();
    if (!result_view_)
        return;
    if (needs_empty_)
        needs_empty_->hide();
    result_view_->show();

    // Most scripts answer {"success": true, "data": <result>}: show the result itself, not a "SUCCESS: YES"
    // card next to it. Other top-level scalars (count, filings_found, ...) are kept; RAW still shows the
    // untouched envelope.
    const QJsonObject payload = ma_fmt::ma_flatten_result(envelope);
    shown_grids_.clear();
    shown_charts_.clear();

    // The Deals module shows SEC/EDGAR filing values, which are USD — pin them
    // so they don't get repainted with the user's preferred currency symbol.
    // Every other M&A module is a calculator the user denominates themselves.
    const QString intrinsic =
        (module_.id == fincept::services::ma::ModuleId::Deals) ? QStringLiteral("USD") : QString();

    result_view_->set_options(ma_result_options(context, payload, intrinsic));
    result_view_->show_result(payload, envelope);
    add_context_widgets(context, payload);
    update_result_actions();
}

void MAModulePanel::add_context_widgets(const QString& context, const QJsonObject& payload) {
    const fincept::ui::result::Options plain;
    auto pct_text = [plain](double v) {
        return fincept::ui::result::format_number(v, fincept::ui::result::Unit::Percent, plain);
    };
    const QString intrinsic =
        (module_.id == fincept::services::ma::ModuleId::Deals) ? QStringLiteral("USD") : QString();
    auto money_text = [intrinsic](double v) { return ma_money_text(v, intrinsic); };

    if (context == QLatin1String("accretion_dilution")) {
        add_merger_widgets(payload); // the EPS bridge first, then the grids
        add_sensitivity_widgets(context, payload);
        return;
    }
    if (context == QLatin1String("dcf") || context == QLatin1String("lbo_sensitivity"))
        add_sensitivity_widgets(context, payload);
    if (context == QLatin1String("football_field")) {
        add_football_field_widget(payload);
        return;
    }

    if (context == QLatin1String("contribution")) {
        // Each side's share of the combined company vs the ownership it gets.
        const QList<QPair<const char*, QString>> parts = {{"ownership", tr("Ownership")},
                                                          {"revenue_contribution", tr("Revenue")},
                                                          {"ebitda_contribution", tr("EBITDA")},
                                                          {"net_income_contribution", tr("Net income")},
                                                          {"assets_contribution", tr("Assets")}};
        QStringList cats;
        QVector<double> acq, tgt;
        for (const auto& part : parts) {
            const QJsonObject o = payload.value(QLatin1String(part.first)).toObject();
            if (o.isEmpty())
                continue;
            cats << part.second;
            acq << ma_num(o, "acquirer_pct");
            tgt << ma_num(o, "target_pct");
        }
        if (cats.isEmpty())
            return;
        auto* chart = new ui::BarChart;
        chart->set_categories(cats);
        ui::BarSeries a;
        a.name = tr("Acquirer");
        a.values = acq;
        ui::BarSeries b;
        b.name = tr("Target");
        b.values = tgt;
        chart->set_series({a, b});
        chart->set_mode(ui::BarChart::Mode::Stacked);
        chart->set_show_values(true);
        chart->set_value_formatter(pct_text);
        chart->setMinimumHeight(240);
        shown_charts_.append({tr("Contribution vs ownership"), chart});
        result_view_->add_custom_widget(
            ui::ResultView::make_card(tr("Contribution vs ownership"), chart, tr("% of the combined company")));
        return;
    }

    if (context == QLatin1String("fairness_opinion")) {
        // The range check: each method's valuation range against the offer, on one axis.
        const QJsonArray methods = payload.value(QStringLiteral("methods_used")).toArray();
        const double offer = payload.value(QStringLiteral("offer_price")).toDouble(std::numeric_limits<double>::quiet_NaN());
        QVector<ma_ff::Range> rows;
        for (const QJsonValue& m : methods) {
            const QJsonObject o = m.toObject();
            ma_ff::Range r;
            r.label = o.value(QStringLiteral("method")).toString();
            r.low = ma_num(o, "low");
            r.high = ma_num(o, "high");
            r.mid = ma_num(o, "midpoint");
            if (r.valid())
                rows << r;
        }
        double lo = 0, hi = 0;
        if (rows.isEmpty() || !ma_ff::axis(rows, {offer}, 0.08, &lo, &hi))
            return;
        QVector<ui::RangeMarker> markers;
        if (std::isfinite(offer)) {
            ui::RangeMarker mark;
            mark.value = offer;
            mark.label = tr("Offer");
            mark.role = ui::RangeMarker::Role::Current;
            markers << mark;
        }
        auto* field = new MaFootballField;
        field->set_rows(rows, lo, hi, markers, money_text);
        shown_charts_.append({tr("Methods vs the offer"), field});
        result_view_->add_custom_widget(ui::ResultView::make_card(
            tr("Methods vs the offer"), field, tr("valuation range per method; not a fairness opinion")));
        return;
    }

    if (context == QLatin1String("process_quality")) {
        auto* box = new QWidget;
        auto* hl = new QHBoxLayout(box);
        hl->setContentsMargins(0, 0, 0, 0);
        hl->setSpacing(12);
        const double score = payload.value(QStringLiteral("overall_score")).toDouble(std::numeric_limits<double>::quiet_NaN());
        if (std::isfinite(score)) {
            auto* gauge = new ui::Gauge(box);
            gauge->set_range(1.0, 5.0);
            gauge->set_value(std::clamp(score, 1.0, 5.0));
            ui::GaugeZone weak{1.0, 2.0, ui::GaugeZone::Tone::Bad, tr("Weak")};
            ui::GaugeZone adequate{2.0, 3.5, ui::GaugeZone::Tone::Caution, tr("Adequate")};
            ui::GaugeZone strong{3.5, 5.0, ui::GaugeZone::Tone::Good, tr("Strong")};
            gauge->set_zones({weak, adequate, strong});
            gauge->set_caption(payload.value(QStringLiteral("quality_rating")).toString());
            gauge->setMinimumHeight(130);
            gauge->setMaximumWidth(300);
            hl->addWidget(gauge);
        }
        const QJsonObject factors = payload.value(QStringLiteral("factor_scores")).toObject();
        if (!factors.isEmpty()) {
            QStringList cats;
            QVector<double> vals;
            for (auto it = factors.constBegin(); it != factors.constEnd(); ++it) {
                cats << fincept::ui::result::humanize(it.key());
                vals << (it.value().isDouble() ? it.value().toDouble() : std::numeric_limits<double>::quiet_NaN());
            }
            auto* chart = new ui::BarChart(box);
            chart->set_data(cats, vals, tr("Score"));
            chart->set_orientation(Qt::Horizontal);
            chart->set_show_values(true);
            chart->setMinimumHeight(std::max(200, int(cats.size()) * 26 + 40));
            hl->addWidget(chart, 1);
        }
        shown_charts_.append({tr("Process quality"), box});
        result_view_->add_custom_widget(
            ui::ResultView::make_card(tr("Process quality"), box, tr("overall score 1 – 5 and each factor")));
        return;
    }

    if (context == QLatin1String("monte_carlo")) {
        const double p5 = ma_num(payload, "p5"), p25 = ma_num(payload, "p25"), med = ma_num(payload, "median"),
                     mean = ma_num(payload, "mean"), p75 = ma_num(payload, "p75"), p95 = ma_num(payload, "p95");
        if (!std::isfinite(p5) || !std::isfinite(p95) || p95 <= p5)
            return;
        ui::RangeBarData d;
        const double pad = (p95 - p5) * 0.05;
        d.low = p5 - pad;
        d.high = p95 + pad;
        d.fill_to_current = false;
        if (std::isfinite(p25) && std::isfinite(p75)) {
            ui::RangeBand iqr;
            iqr.from = p25;
            iqr.to = p75;
            iqr.tone = ui::RangeBand::Tone::Accent;
            iqr.label = tr("P25 – P75");
            ui::RangeBand tails;
            tails.from = p5;
            tails.to = p95;
            tails.tone = ui::RangeBand::Tone::Neutral;
            tails.label = tr("P5 – P95");
            d.bands = {tails, iqr};
        }
        auto marker = [](double v, const QString& label, ui::RangeMarker::Role role) {
            ui::RangeMarker m;
            m.value = v;
            m.label = label;
            m.role = role;
            return m;
        };
        if (std::isfinite(med))
            d.markers << marker(med, tr("Median"), ui::RangeMarker::Role::Current);
        if (std::isfinite(mean))
            d.markers << marker(mean, tr("Mean"), ui::RangeMarker::Role::Mean);
        d.markers << marker(p5, QStringLiteral("P5"), ui::RangeMarker::Role::Low)
                  << marker(p95, QStringLiteral("P95"), ui::RangeMarker::Role::High);
        auto* bar = new ui::RangeBar;
        bar->set_data(d);
        bar->set_value_formatter(money_text);
        bar->setMinimumHeight(64);
        shown_charts_.append({tr("Valuation distribution"), bar});
        result_view_->add_custom_widget(
            ui::ResultView::make_card(tr("Valuation distribution"), bar, tr("simulated percentiles")));

        // Histogram of the returned random sample of simulated values (P1 - P99, the far tails are cut).
        QVector<double> sample;
        const QJsonArray samples = payload.value(QStringLiteral("samples")).toArray();
        sample.reserve(samples.size());
        for (const QJsonValue& v : samples)
            if (v.isDouble() && std::isfinite(v.toDouble()))
                sample << v.toDouble();
        if (sample.size() >= 50) {
            std::sort(sample.begin(), sample.end());
            const double lo = sample.at(sample.size() / 100);
            const double hi = sample.at(sample.size() - 1 - sample.size() / 100);
            if (hi > lo) {
                constexpr int kBins = 20;
                const double width = (hi - lo) / kBins;
                QVector<double> share(kBins, 0.0);
                for (double v : sample) {
                    if (v < lo || v > hi)
                        continue;
                    ++share[std::min(kBins - 1, int((v - lo) / width))];
                }
                QStringList cats;
                for (int i = 0; i < kBins; ++i) {
                    share[i] = share[i] * 100.0 / double(sample.size());
                    cats << money_text(lo + (i + 0.5) * width);
                }
                auto* hist = new ui::BarChart;
                hist->set_data(cats, share, tr("Share of simulations"));
                hist->set_value_formatter(pct_text);
                hist->setMinimumHeight(220);
                shown_charts_.append({tr("Simulated values"), hist});
                result_view_->add_custom_widget(ui::ResultView::make_card(
                    tr("Simulated values"),
                    hist, tr("%1 of %2 runs, P1 - P99").arg(sample.size()).arg(ma_num(payload, "num_simulations"), 0, 'f', 0)));
            }
        }
    }
}

// ── Service signal handlers ──────────────────────────────────────────────────

static const QHash<ModuleId, QStringList>& get_context_map() {
    static const QHash<ModuleId, QStringList> map = {
        {ModuleId::Valuation,
         {"dcf", "lbo_returns", "lbo_model", "lbo_debt_schedule", "lbo_sensitivity", "trading_comps",
          "precedent_txns"}},
        {ModuleId::Merger,
         {"accretion_dilution", "pro_forma", "sources_uses", "contribution", "synergy_dcf", "payment_structure",
          "earnout", "exchange_ratio", "collar", "cvr"}},
        // The Deal Database module talks to the service through its own request-id signals (MaDealsWorkspace).
        {ModuleId::Deals, {}},
        {ModuleId::Startup,
         {"berkus", "scorecard", "vc_method", "first_chicago", "risk_factor", "startup_comprehensive"}},
        {ModuleId::Fairness, {"fairness_opinion", "premium_analysis", "process_quality"}},
        {ModuleId::Industry, {"tech_metrics", "healthcare_metrics", "finserv_metrics"}},
        {ModuleId::Advanced, {"monte_carlo", "regression"}},
        {ModuleId::Comparison,
         {"compare_deals", "rank_deals", "benchmark_premium", "payment_structures", "industry_deals"}},
    };
    return map;
}

bool MAModulePanel::owns_context(const QString& context) const {
    auto it = get_context_map().find(module_.id);
    return it != get_context_map().end() && it->contains(context);
}

void MAModulePanel::on_result_ready(const QString& context, const QJsonObject& payload) {
    if (!owns_context(context) || !pane_)
        return;
    // Matched to the pane run that asked for it; a superseded, cancelled or left run is ignored there.
    const quint64 id = run_ids_.value(context);
    // A script that answers {"success": false, ...} (without the "error" key PythonRunner looks for)
    // used to be rendered as a result card reading "SUCCESS: NO".
    const QJsonValue ok = payload.value(QStringLiteral("success"));
    if (ok.isBool() && !ok.toBool()) {
        QString text = payload.value(QStringLiteral("message")).toString();
        if (text.isEmpty())
            text = payload.value(QStringLiteral("error")).toString();
        pane_->fail(id, QString("[%1] %2").arg(context, text.isEmpty() ? tr("The analysis failed") : text));
        return;
    }
    // A needs_data reply (nothing computed, no comparables yet) is delivered too: render_payload
    // shows it as the empty state that says what to add.
    pane_->deliver(id, payload);
}

void MAModulePanel::render_payload(const QJsonValue& value) {
    const QString context = pane_ ? pane_->view() : QString();
    const QJsonObject payload = value.toObject();
    const QJsonObject needs = payload.value(QStringLiteral("needs_data")).toObject();
    if (!needs.isEmpty()) {
        display_needs_data(needs);
        return;
    }
    display_result(payload, context);
    remember_run(context, payload);
    // Figures left out for want of an optional input (shares outstanding, share price): point at the inputs.
    QStringList missing;
    const QJsonArray missing_arr =
        payload.value(QStringLiteral("data")).toObject().value(QStringLiteral("missing_inputs")).toArray();
    for (const QJsonValue& v : missing_arr)
        missing << v.toString();
    if (!missing.isEmpty())
        flag_inputs(context, missing, /*focus=*/false);
}

void MAModulePanel::display_needs_data(const QJsonObject& needs) {
    ensure_result_view();
    if (!needs_empty_ || !result_view_)
        return;
    const QString context = pane_ ? pane_->view() : QString();
    const QString title = needs.value(QStringLiteral("title")).toString();
    QList<QPair<QString, QString>> steps;
    const QJsonArray arr = needs.value(QStringLiteral("steps")).toArray();
    for (int i = 0; i < arr.size(); ++i)
        steps.append({QString::number(i + 1), arr.at(i).toString()});
    needs_empty_->set_glyph(QStringLiteral("◇"), ui::EmptyState::Tone::Info); // ◇
    needs_empty_->set_title(title.isEmpty() ? tr("Nothing to compare with yet") : title);
    needs_empty_->set_message(needs.value(QStringLiteral("message")).toString());
    needs_empty_->set_details(steps, tr("HOW TO ADD THEM"));
    const bool sample = deal_grids_.value(context) != nullptr;
    needs_empty_->set_note(sample ? tr("The samples are illustrative figures, not your data") : QString(),
                           ui::EmptyState::Tone::Info);
    needs_empty_->set_primary_action(sample ? tr("USE SAMPLE COMPARABLES") : QString());
    // Comparables come from the Deal Database; the football field needs runs of this module instead.
    needs_empty_->set_secondary_action(context == QLatin1String("football_field") ? QString() : tr("OPEN DEAL DATABASE"));
    result_view_->clear();
    result_view_->hide();
    needs_empty_->show();
    update_result_actions();
}

void MAModulePanel::on_input_required(const QString& context, const QString& message,
                                      const QStringList& missing_inputs) {
    if (!owns_context(context))
        return;
    const bool shown = pane_ && pane_->view() == context;
    if (pane_)
        pane_->fail(run_ids_.value(context), message, tr("Check the inputs"));
    // Outline the inputs it asked for; take the user there only when it is the sub-tab on screen.
    flag_inputs(context, missing_inputs, /*focus=*/shown);
    if (shown)
        status_label_->setText(tr("Input needed"));
}

int MAModulePanel::flag_inputs(const QString& context, const QStringList& keys, bool focus) {
    // Script parameter -> the panel input that supplies it.
    static const QHash<QString, QHash<QString, QString>> kInputOf = {
        {QStringLiteral("monte_carlo"),
         {{QStringLiteral("base_valuation"), QStringLiteral("mc_base")},
          {QStringLiteral("margin_mean"), QStringLiteral("mc_margin_mean")}}},
        {QStringLiteral("payment_structure"),
         {{QStringLiteral("acquirer_share_price"), QStringLiteral("pay_acq_price")},
          {QStringLiteral("acquirer_shares_outstanding"), QStringLiteral("pay_acq_shares")},
          {QStringLiteral("target_shares_outstanding"), QStringLiteral("pay_tgt_shares")}}},
        {QStringLiteral("premium_analysis"), {{QStringLiteral("shares_outstanding"), QStringLiteral("pa_shares")}}},
        {QStringLiteral("accretion_dilution"),
         {{QStringLiteral("acquirer_share_price"), QStringLiteral("acq_share_price")},
          {QStringLiteral("acquirer_net_income"), QStringLiteral("acq_net_income")},
          {QStringLiteral("acquirer_shares"), QStringLiteral("acq_shares")},
          {QStringLiteral("target_net_income"), QStringLiteral("tgt_net_income")},
          {QStringLiteral("deal_value"), QStringLiteral("deal_value")},
          {QStringLiteral("cash_pct"), QStringLiteral("cash_pct")}}},
        {QStringLiteral("collar"), {{QStringLiteral("target_shares"), QStringLiteral("collar_tgt_shares")}}},
    };
    const QHash<QString, QString> map = kInputOf.value(context);
    int found = 0;
    QWidget* first = nullptr;
    for (const QString& key : keys) {
        const QString input = map.value(key);
        QAbstractSpinBox* w = double_inputs_.value(input, nullptr);
        if (!w)
            w = int_inputs_.value(input, nullptr);
        if (!w)
            continue;
        ++found;
        if (!first)
            first = w;
        if (w->property("maFlagged").toBool())
            continue;
        const QString base = w->styleSheet();
        w->setProperty("maFlagged", true);
        w->setStyleSheet(base + QString("QAbstractSpinBox { border:1px solid %1; }").arg(ui::colors::WARNING()));
        // The outline goes as soon as the user has edited the value.
        auto conn = std::make_shared<QMetaObject::Connection>();
        QPointer<QAbstractSpinBox> guard = w;
        *conn = connect(w, &QAbstractSpinBox::editingFinished, this, [guard, base, conn]() {
            if (guard) {
                guard->setProperty("maFlagged", false);
                guard->setStyleSheet(base);
            }
            QObject::disconnect(*conn);
        });
    }
    if (focus && first && sub_tabs_) {
        for (int i = 0; i < sub_tabs_->count(); ++i) {
            QWidget* page = sub_tabs_->widget(i);
            if (page && page->isAncestorOf(first)) {
                sub_tabs_->setCurrentIndex(i);
                break;
            }
        }
        first->setFocus(Qt::OtherFocusReason);
    }
    return found;
}

void MAModulePanel::on_error(const QString& context, const QString& message) {
    if (!owns_context(context) || !pane_)
        return;
    // A run the pane cancelled (its view was left, or a new RUN superseded it) answers "Cancelled".
    // Replies carry no request id, so it must not fail the run of the same analysis that replaced it.
    if (message == QLatin1String("Cancelled"))
        return;
    pane_->fail(run_ids_.value(context), QString("[%1] %2").arg(context, message));
}

// ── Pane wiring ──────────────────────────────────────────────────────────────

QString MAModulePanel::context_of_tab(int index) const {
    if (!sub_tabs_ || index < 0 || index >= sub_tabs_->count())
        return QString();
    QWidget* page = sub_tabs_->widget(index);
    for (const auto& pair : sub_tab_sources_)
        if (pair.first == page)
            return ma_panel_tab_context(module_.id, pair.second);
    return QString();
}

QString MAModulePanel::context_of_widget(const QWidget* w) const {
    if (!sub_tabs_ || !w)
        return QString();
    for (int i = 0; i < sub_tabs_->count(); ++i) {
        QWidget* page = sub_tabs_->widget(i);
        if (page && page->isAncestorOf(w))
            return context_of_tab(i);
    }
    return QString();
}

QPushButton* MAModulePanel::run_button_of_tab(int index) const {
    if (!sub_tabs_ || index < 0 || index >= sub_tabs_->count())
        return nullptr;
    QWidget* page = sub_tabs_->widget(index);
    for (const auto& b : run_buttons_)
        if (b && page && page->isAncestorOf(b))
            return b.data();
    return nullptr;
}

QString MAModulePanel::tab_title(int index) const {
    if (!sub_tabs_ || index < 0 || index >= sub_tabs_->count())
        return QString();
    QString t = sub_tabs_->tabText(index);
    t.replace(QLatin1String("&&"), QLatin1String("&")); // the tab bar's escaped ampersand
    return t;
}

void MAModulePanel::select_view() {
    if (!pane_ || !sub_tabs_)
        return;
    const int index = sub_tabs_->currentIndex();
    const QString context = context_of_tab(index);
    if (context.isEmpty())
        return;
    const bool manual = ma_panel_no_auto(context);
    if (manual)
        configure_preview(context);
    pane_->set_loading_text(tr("Computing %1…").arg(tab_title(index)));
    // Before the switch (it decides at once whether the view runs): market data ages, a calculation
    // with the same inputs does not.
    // The football field is drawn from the other analyses' latest runs: rebuilt (locally) on every show.
    const bool live = context == QLatin1String("football_field");
    pane_->set_max_age(manual ? ui::autoload::kDefaultMaxAgeSeconds
                              : (live ? ui::autoload::kAlwaysRefresh : ui::autoload::kNeverRefresh));
    pane_->set_view(context, manual ? ui::AutoLoadPane::Policy::Manual : ui::AutoLoadPane::Policy::AutoRun);
    // The inputs this view's remembered result must match.
    if (memories_.contains(context))
        pane_->set_signature(signature_of(context));
    update_result_actions();
}

QString MAModulePanel::signature_of(const QString& context) const {
    QString sig;
    if (ui::InputMemory* mem = memories_.value(context))
        sig = mem->signature();
    // The Comparison deal grid sits above the sub-tabs: every Comparison result depends on it.
    if (module_.id == services::ma::ModuleId::Comparison && module_memory_)
        sig += QLatin1Char('|') + module_memory_->signature();
    return sig;
}

void MAModulePanel::attach_module_memory() {
    if (module_memory_)
        return;
    // The company symbols and the Comparison deal grid belong to the module, not to one sub-tab.
    module_memory_ = new ui::InputMemory(QStringLiteral("ma_analytics"), QStringLiteral("module_") + module_.key, this);
    for (auto it = company_boxes_.cbegin(); it != company_boxes_.cend(); ++it)
        if (it.value() && it.value()->line_edit())
            module_memory_->track(it.value()->objectName(), it.value()->line_edit());
    if (comparison_grid_)
        module_memory_->track(comparison_grid_->memory_name(), comparison_grid_->memory_field());
    module_memory_->restore();
    connect(module_memory_, &ui::InputMemory::changed, this, [this]() {
        if (pane_ && module_.id == services::ma::ModuleId::Comparison && memories_.contains(pane_->view()))
            pane_->set_signature(signature_of(pane_->view()));
    });
}

bool MAModulePanel::grid_deals(MaDealGrid* grid, QJsonArray* out, bool allow_empty) {
    if (!grid)
        return allow_empty;
    *out = grid->deals();
    if (!out->isEmpty() || allow_empty)
        return true;
    display_error(grid->row_count() > 0
                      ? tr("None of the %n row(s) is complete: %1", nullptr, grid->row_count())
                            .arg(grid->invalid_count() > 0 ? tr("fix the rows marked in red (hover a row for the reason).")
                                                           : tr("fill the required columns."))
                      : tr("Add at least one deal, or load them from the Deal Database."));
    return false;
}

void MAModulePanel::remember_run(const QString& context, const QJsonObject& payload) {
    static const QSet<QString> kKept = {QStringLiteral("dcf"), QStringLiteral("trading_comps"),
                                        QStringLiteral("precedent_txns"), QStringLiteral("lbo_sensitivity")};
    if (!kKept.contains(context) || payload.contains(QStringLiteral("needs_data")))
        return;
    QJsonObject keep = ma_fmt::ma_flatten_result(payload);
    keep.insert(QStringLiteral("_as_of"), QDateTime::currentDateTime().toString(Qt::ISODate));
    last_runs_.insert(context, keep);
    // Kept 30 days so the football field still has it after a restart.
    fincept::CacheManager::instance().put(QStringLiteral("ma_analytics:last_run:") + context,
                                          QString::fromUtf8(QJsonDocument(keep).toJson(QJsonDocument::Compact)),
                                          30 * 24 * 3600, QStringLiteral("ma_analytics"));
}

QJsonObject MAModulePanel::last_run(const QString& context) const {
    if (last_runs_.contains(context))
        return last_runs_.value(context);
    const QVariant v = fincept::CacheManager::instance().get(QStringLiteral("ma_analytics:last_run:") + context);
    return v.isNull() ? QJsonObject() : QJsonDocument::fromJson(v.toString().toUtf8()).object();
}

void MAModulePanel::attach_memory(QWidget* page) {
    if (!page)
        return;
    QString context;
    for (const auto& pair : std::as_const(sub_tab_sources_))
        if (pair.first == page)
            context = ma_panel_tab_context(module_.id, pair.second);
    if (context.isEmpty() || memories_.contains(context))
        return; // the Deal Database keeps its own data; a page is attached once
    // Parented to the page: flushed when the page hides (another sub-tab) and when it is destroyed.
    auto* mem = new ui::InputMemory(QStringLiteral("ma_analytics"), context, page);
    // Named inputs by their field name (stable across layout changes)…
    auto track_named = [mem, page](const auto& map) {
        for (auto it = map.cbegin(); it != map.cend(); ++it)
            if (it.value() && page->isAncestorOf(it.value()))
                mem->track(it.key(), it.value());
    };
    track_named(double_inputs_);
    track_named(int_inputs_);
    track_named(combo_inputs_);
    // Shared symbol pickers (Trading Comps target): their text, by the box's object name.
    for (auto* box : page->findChildren<ui::SymbolSearchBox*>())
        if (!box->objectName().isEmpty() && box->line_edit())
            mem->track(box->objectName(), box->line_edit());
    // Deal / comparables grids: their rows (JSON in a hidden mirror), by the grid's name.
    for (auto* grid : page->findChildren<MaDealGrid*>())
        mem->track(grid->memory_name(), grid->memory_field());
    // …and the rest (the deal / comparables JSON boxes) by position.
    mem->track_all(page);
    mem->restore();
    memories_.insert(context, mem);
    connect(mem, &ui::InputMemory::changed, this, [this, context]() {
        if (pane_ && pane_->view() == context)
            pane_->set_signature(signature_of(context));
    });
}

void MAModulePanel::start_run(quint64 id) {
    const QString context = pane_->view();
    run_ids_.insert(context, id);
    if (user_clicking_)
        return; // the user's RUN: the button's own handler starts it right after
    // A default run, RETRY or the preview's RUN: press the sub-tab's RUN button for it — the
    // exact inputs and checks of a click.
    const int index = sub_tabs_ ? sub_tabs_->currentIndex() : -1;
    QPushButton* run = context_of_tab(index) == context ? run_button_of_tab(index) : nullptr;
    if (!run) {
        pane_->fail(id, tr("This analysis has no RUN action here."));
        return;
    }
    pane_clicking_ = true;
    run->click(); // begin_run (builds the forms), then the handler (the service call; a cached reply may
                  // arrive synchronously, the id is already recorded)
    pane_clicking_ = false;
}

void MAModulePanel::cancel_run(quint64 id) {
    // A default run whose sub-tab (or module) was left: stop its script.
    for (auto it = run_ids_.begin(); it != run_ids_.end(); ++it) {
        if (it.value() == id) {
            MAAnalyticsService::instance().cancel(it.key());
            run_ids_.erase(it);
            return;
        }
    }
}

void MAModulePanel::configure_preview(const QString& context) {
    ui::EmptyState* pv = pane_->preview();
    QList<QPair<QString, QString>> inputs;
    if (context == QLatin1String("trading_comps")) {
        if (auto* t = findChild<ui::SymbolSearchBox*>(QStringLiteral("comps_target_symbol")))
            inputs.append({tr("Target"), t->text().trimmed()});
        if (auto* p = combo_inputs_.value(QStringLiteral("comps_peers")))
            inputs.append({tr("Peers"), p->currentText()});
        pv->set_glyph(QStringLiteral("⌁"), ui::EmptyState::Tone::Accent); // ⌁
        pv->set_title(tr("Trading comparables"));
        pv->set_message(tr("Values the target on its peers' trading multiples (EV/Revenue, EV/EBITDA, P/E) from "
                           "the market data of every ticker. Not run automatically: each run fetches live data "
                           "from a rate-limited source. Set the tickers above and press RUN."));
        pv->set_note(tr("Live market data (Yahoo Finance) · no credits"), ui::EmptyState::Tone::Info);
    } else {
        pv->set_glyph(QStringLiteral("⌁"), ui::EmptyState::Tone::Accent);
        pv->set_title(tab_title(sub_tabs_ ? sub_tabs_->currentIndex() : -1));
        pv->set_message(tr("This analysis fetches outside data, so it is not run automatically. Set the inputs "
                           "above and press RUN."));
        pv->set_note(QString());
    }
    pv->set_details(inputs, tr("INPUTS"));
    pv->set_primary_action(tr("RUN %1").arg(tab_title(sub_tabs_ ? sub_tabs_->currentIndex() : -1).toUpper()));
    pv->set_secondary_action(QString());
}

// ── Sub-tab registration (records source label for retranslateUi) ────────────
void MAModulePanel::add_sub_tab(QWidget* page, const char* source_label) {
    if (!sub_tabs_ || !page)
        return;
    // A tab bar reads "&" as a keyboard-shortcut marker ("Sources & Uses" came out as "Sources _Uses").
    sub_tabs_->addTab(page, tr(source_label).replace(QLatin1Char('&'), QLatin1String("&&")));
    sub_tab_sources_.append({page, QString::fromUtf8(source_label)});
}

void MAModulePanel::add_lazy_sub_tab(const char* source_label, std::function<void(QWidget* page)> build) {
    if (!sub_tabs_ || !build)
        return;
    const bool first = sub_tabs_->count() == 0;
    auto* page = new QWidget(sub_tabs_);
    add_sub_tab(page, source_label);
    if (first) {
        build(page); // the tab on show when the module opens
        return;
    }
    pending_sub_tabs_.insert(page, std::move(build));
    if (!lazy_tabs_wired_) {
        lazy_tabs_wired_ = true;
        connect(sub_tabs_, &QTabWidget::currentChanged, this,
                [this](int index) { build_sub_tab(sub_tabs_->widget(index)); });
    }
}

void MAModulePanel::build_sub_tab(QWidget* page) {
    auto it = pending_sub_tabs_.find(page);
    if (it == pending_sub_tabs_.end())
        return;
    const std::function<void(QWidget*)> build = std::move(it.value());
    pending_sub_tabs_.erase(it);
    // Built while hidden, then shown once: children added to a page that is
    // already visible would only appear on the next event-loop pass.
    const bool visible = page->isVisible();
    if (visible)
        page->hide();
    build(page);
    attach_memory(page); // the last session's inputs, before anything on it runs
    if (visible)
        page->show();
}

void MAModulePanel::build_all_sub_tabs() {
    while (!pending_sub_tabs_.isEmpty())
        build_sub_tab(pending_sub_tabs_.constBegin().key());
}

// ── Live language switch ─────────────────────────────────────────────────────
void MAModulePanel::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange)
        retranslateUi();
    QWidget::changeEvent(event);
}

void MAModulePanel::retranslateUi() {
    // Header label/category come from module_ (data) — not translated here.
    // Re-apply sub-tab labels from their recorded source strings.
    if (sub_tabs_) {
        for (const auto& pair : sub_tab_sources_) {
            int idx = sub_tabs_->indexOf(pair.first);
            if (idx >= 0)
                sub_tabs_->setTabText(
                    idx, tr(pair.second.toUtf8().constData()).replace(QLatin1Char('&'), QLatin1String("&&")));
        }
    }
}

// ── Tab stylesheet helper ───────────────────────────────────────────────────
void MAModulePanel::apply_tab_stylesheet() {
    if (!sub_tabs_)
        return;
    // Never truncate tab labels to "Accreti…"; show full text and scroll on overflow.
    sub_tabs_->setElideMode(Qt::ElideNone);
    sub_tabs_->setUsesScrollButtons(true);
    sub_tabs_->setStyleSheet(QString("QTabWidget::pane { border:1px solid %1; background:%2; }"
                                     "QTabBar::tab { background:%3; color:%4; padding:6px 16px;"
                                     "font-family:%5; font-size:%6px; border:1px solid %1; border-bottom:none; }"
                                     "QTabBar::tab:selected { background:%2; color:%7; font-weight:700;"
                                     "border-bottom:2px solid %7; }")
                                 .arg(ui::colors::BORDER_DIM(), ui::colors::BG_SURFACE(), ui::colors::BG_RAISED())
                                 .arg(ui::colors::TEXT_SECONDARY())
                                 .arg(ui::fonts::DATA_FAMILY())
                                 .arg(ui::fonts::SMALL.get())
                                 .arg(ui::colors::legible(module_.color)));
}

// ── Theme refresh ───────────────────────────────────────────────────────────
void MAModulePanel::refresh_theme() {
    // Header bar
    if (header_bar_)
        header_bar_->setStyleSheet(QString("QWidget#maPanelHeader { background:%1; border-bottom:1px solid %2; }")
                                       .arg(ui::colors::BG_RAISED(), ui::colors::BORDER_DIM()));

    // Header title
    if (header_title_)
        header_title_->setStyleSheet(
            QString("color:%1; font-size:%2px; font-weight:700; font-family:%3; letter-spacing:1px;")
                .arg(ui::colors::legible(module_.color))
                .arg(ui::fonts::TINY.get())
                .arg(ui::fonts::DATA_FAMILY()));

    // Header divider
    if (header_bar_) {
        auto* div = header_bar_->findChild<QWidget*>("maPanelDivider");
        if (div)
            div->setStyleSheet(QString("background:%1;").arg(ui::colors::BORDER_DIM()));
    }

    // Header category
    if (header_category_)
        header_category_->setStyleSheet(QString("color:%1; font-size:%2px; font-family:%3;")
                                            .arg(ui::colors::TEXT_SECONDARY())
                                            .arg(ui::fonts::TINY.get())
                                            .arg(ui::fonts::DATA_FAMILY()));

    // Status label
    if (status_label_)
        status_label_->setStyleSheet(QString("color:%1; font-size:%2px; font-family:%3;")
                                         .arg(ui::colors::TEXT_SECONDARY())
                                         .arg(ui::fonts::TINY.get())
                                         .arg(ui::fonts::DATA_FAMILY()));

    // Tab widget
    apply_tab_stylesheet();
}

} // namespace fincept::screens
