// src/screens/economics/panels/FinceptMacroPanel.cpp
// Fincept Macro — Fincept's macro API (see FinceptMacroPanel.h). Endpoint shapes (FinceptAPI macro.py):
//   wgb/central-bank-rates  {central_bank_rates:[{country, rate_pct, change_bp, period, scraped_at}]}
//   wgb/sovereign-cds       {sovereign_cds:[{country_code, country_name, cds_value, scraped_at}]}
//   wgb/bond-spreads        {bond_spreads:[{country, yield_10y_pct, spread_vs_ger|us|cn|jp, scraped_at}]}
//   wgb/credit-ratings      {credit_ratings:[{country, sp_rating, moodys_rating, fitch_rating, dbrs_rating, …}]}
//   wgb/inverted-yields     {inverted_yields:[{country, details, scraped_at}]}
//   wgb/yield-forecast      {yield_forecast:[{country, maturity, current_yield, forecast_yield, scraped_at}]}
//   wgb/country-detail      {country_detail:[{country_slug, bond_10y_yield, cb_rate, cb_rate_date, credit_rating,
//                                             cds_spread, default_probability, main_spread_value, scraped_at}]}
//   upcoming-events         {events:[{date, time, country, event, category, reference_period, actual, forecast,
//                                     consensus, previous, revised_from, importance, …}], total_count}
//   ceic/series             {series:[{id, series_id, country, period, year, value}], total_count, returned_count}
//   ceic/series/countries   {countries:[{country, row_count}]}
//   ceic/series/indicators  {indicators:[{series_id, indicator, country, data_points}]}
//   ceic/indicators         {indicators:[{id, country, series_id, slug, name, unit, category, frequency, date_range}]}
// Every list endpoint is wrapped in {success, message, data}; 401 = not signed in, 402 = out of credits.
#include "screens/economics/panels/FinceptMacroPanel.h"

#include "core/agent/AgentRiskMarkers.h"
#include "core/logging/Logger.h"
#include "services/billing/PlanAccess.h"
#include "ui/widgets/EnterprisePromo.h"
#include "ui/widgets/PlanHint.h"

#include <QCoreApplication>
#include <QDate>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonObject>
#include <QLabel>
#include <QLocale>
#include <QRegularExpression>
#include <QSignalBlocker>

#include <algorithm>
#include <cmath>
#include <initializer_list>

namespace fincept::screens {
namespace fincept_macro_panel_detail {

constexpr const char* kFinceptMacroSourceId = "fincept";
constexpr const char* kFinceptMacroColor = "#d97706"; // amber
constexpr const char* kFmCountriesRequest = "fm_ceic_countries";
constexpr const char* kFmIndicatorsRequestPrefix = "fm_ceic_ind_";
constexpr int kFmCeicLimit = 1000; // backend maximum per call

struct FmView {
    const char* id;
    const char* label;
    const char* endpoint;
    const char* tip;
};

// Labels / tips are translated at use (tr() in the panel); the NOOP marks them for lupdate.
const FmView kFmViews[] = {
    {"cb_rates", QT_TRANSLATE_NOOP("fincept::screens::FinceptMacroPanel", "Central Bank Rates"),
     "wgb/central-bank-rates",
     QT_TRANSLATE_NOOP("fincept::screens::FinceptMacroPanel",
                       "Policy rate (%) and the size of the last move (bp) for every central bank")},
    {"cds", QT_TRANSLATE_NOOP("fincept::screens::FinceptMacroPanel", "Sovereign CDS"), "wgb/sovereign-cds",
     QT_TRANSLATE_NOOP("fincept::screens::FinceptMacroPanel", "Sovereign credit-default-swap spreads (bp)")},
    {"spreads", QT_TRANSLATE_NOOP("fincept::screens::FinceptMacroPanel", "10Y Yields & Spreads"),
     "wgb/bond-spreads",
     QT_TRANSLATE_NOOP("fincept::screens::FinceptMacroPanel",
                       "10-year government bond yield (%) and its spread (bp) to Germany, the US, China and Japan")},
    {"ratings", QT_TRANSLATE_NOOP("fincept::screens::FinceptMacroPanel", "Sovereign Credit Ratings"),
     "wgb/credit-ratings",
     QT_TRANSLATE_NOOP("fincept::screens::FinceptMacroPanel", "S&P, Moody's, Fitch and DBRS sovereign ratings")},
    {"inverted", QT_TRANSLATE_NOOP("fincept::screens::FinceptMacroPanel", "Inverted Yield Curves"),
     "wgb/inverted-yields",
     QT_TRANSLATE_NOOP("fincept::screens::FinceptMacroPanel",
                       "Countries whose government yield curve is inverted, grouped by rating")},
    {"forecast", QT_TRANSLATE_NOOP("fincept::screens::FinceptMacroPanel", "Yield Forecasts"), "wgb/yield-forecast",
     QT_TRANSLATE_NOOP("fincept::screens::FinceptMacroPanel",
                       "Current vs forecast government bond yields (%) by maturity")},
    {"country", QT_TRANSLATE_NOOP("fincept::screens::FinceptMacroPanel", "Country Bond Summary"),
     "wgb/country-detail",
     QT_TRANSLATE_NOOP("fincept::screens::FinceptMacroPanel",
                       "Per-country 10Y yield, policy rate, rating, CDS and implied default probability")},
    {"events", QT_TRANSLATE_NOOP("fincept::screens::FinceptMacroPanel", "Upcoming Macro Events"), "upcoming-events",
     QT_TRANSLATE_NOOP("fincept::screens::FinceptMacroPanel",
                       "Economic calendar: releases with actual, consensus, forecast and previous values")},
    {"ceic", QT_TRANSLATE_NOOP("fincept::screens::FinceptMacroPanel", "CEIC Series Explorer"), "ceic/series",
     QT_TRANSLATE_NOOP("fincept::screens::FinceptMacroPanel",
                       "CEIC macro time series by country and indicator (or the latest value of every indicator)")},
    {"ceic_catalog", QT_TRANSLATE_NOOP("fincept::screens::FinceptMacroPanel", "CEIC Indicator Catalogue"),
     "ceic/indicators",
     QT_TRANSLATE_NOOP("fincept::screens::FinceptMacroPanel",
                       "CEIC indicator names, units, frequency and coverage for a country")},
};

const FmView* find_view(const QString& id) {
    for (const auto& v : kFmViews)
        if (id == QLatin1String(v.id))
            return &v;
    return nullptr;
}

bool is_ceic_view(const QString& id) {
    return id == QLatin1String("ceic") || id == QLatin1String("ceic_catalog");
}

QString humanize_slug(QString slug) {
    slug.replace(QLatin1Char('_'), QLatin1Char('-'));
    QStringList words = slug.split(QLatin1Char('-'), Qt::SkipEmptyParts);
    for (auto& w : words)
        w[0] = w[0].toUpper();
    return words.join(QLatin1Char(' '));
}

int fm_month_from_name(const QString& name) {
    static const char* const kNames[] = {"jan", "feb", "mar", "apr", "may", "jun",
                                         "jul", "aug", "sep", "oct", "nov", "dec"};
    const QString l = name.left(3).toLower();
    for (int i = 0; i < 12; ++i)
        if (l == QLatin1String(kNames[i]))
            return i + 1;
    return 0;
}

QString json_text(const QJsonValue& v) {
    if (v.isDouble()) {
        const double d = v.toDouble();
        return d == std::floor(d) ? QString::number(static_cast<qint64>(d)) : QString::number(d, 'g', 10);
    }
    return v.toString().trimmed();
}

/// CEIC rows carry `year` plus a free-form `period`; build one sortable period label from them.
QString ceic_period_label(const QJsonObject& r) {
    const QString year = json_text(r.value(QStringLiteral("year")));
    const QString period = json_text(r.value(QStringLiteral("period")));
    if (period.isEmpty())
        return year;
    static const QRegularExpression kHasYear(QStringLiteral("\\d{4}"));
    if (kHasYear.match(period).hasMatch() || year.isEmpty())
        return period; // already a full period ("2024-03", "2024-03-31", "2024Q1", "Mar 2024")
    static const QRegularExpression kQuarter(QStringLiteral("^Q([1-4])$"), QRegularExpression::CaseInsensitiveOption);
    if (kQuarter.match(period).hasMatch())
        return year + QStringLiteral("-") + period.toUpper();
    bool ok = false;
    const int m = period.toInt(&ok);
    if (ok && m >= 1 && m <= 12)
        return year + QLatin1Char('-') + QString::number(m).rightJustified(2, QLatin1Char('0'));
    const int mn = fm_month_from_name(period);
    if (mn > 0)
        return year + QLatin1Char('-') + QString::number(mn).rightJustified(2, QLatin1Char('0'));
    return year + QLatin1Char(' ') + period;
}

/// Most recent scrape timestamp in a WorldGovernmentBonds snapshot ("2026-10-12 06:00").
QString latest_scrape(const QJsonArray& rows) {
    QString best;
    for (const auto& v : rows) {
        const QString s = v.toObject().value(QStringLiteral("scraped_at")).toString();
        if (s > best)
            best = s;
    }
    best.replace(QLatin1Char('T'), QLatin1Char(' '));
    return best.left(16);
}

QString credits_text(double c) {
    // Up to three significant digits: a plan's share of a list price can be 0.25.
    return c == std::floor(c) ? QString::number(static_cast<int>(c)) : QString::number(c, 'g', 3);
}

/// What a request to `endpoint` costs this account (the plan's price; the list price without a plan
/// table). A plan that claims "included" yet is refused names the list price instead of 0.
double fm_charged_cost(const QString& endpoint) {
    const double c = services::FinceptMacroService::effective_credit_cost(endpoint);
    return c > 0.0 ? c : services::FinceptMacroService::credit_cost(endpoint);
}

/// The CEIC country / indicator lists are metadata calls (their own, lower list price).
const QString& fm_ceic_list_endpoint() {
    static const QString kEndpoint = QStringLiteral("ceic/series/countries");
    return kEndpoint;
}

QString duration_text(int seconds) {
    if (seconds >= 3600)
        return QCoreApplication::translate("fincept::screens::FinceptMacroPanel", "%1 h").arg(seconds / 3600);
    return QCoreApplication::translate("fincept::screens::FinceptMacroPanel", "%1 min").arg(seconds / 60);
}

services::FinceptMacroService::Params catalog_params(const QString& country) {
    return {{QStringLiteral("country"), country}, {QStringLiteral("limit"), QString::number(kFmCeicLimit)}};
}

} // namespace fincept_macro_panel_detail

namespace fmpd = fincept_macro_panel_detail;

// ── Construction ──────────────────────────────────────────────────────────────

FinceptMacroPanel::FinceptMacroPanel(QWidget* parent)
    : EconPanelBase(fmpd::kFinceptMacroSourceId, fmpd::kFinceptMacroColor, parent) {
    build_base_ui(this);
    fetch_btn_->setProperty("fincept_agent_risk", "spend");                               // billed in Fincept credits
    load_pane()->preview()->primary_button()->setProperty("fincept_agent_risk", "spend"); // the preview's FETCH
    connect(&services::FinceptMacroService::instance(), &services::FinceptMacroService::result_ready, this,
            &FinceptMacroPanel::on_macro_result);
    update_controls_for_view();
    update_credit_hint();
    sync_load_view();
    // The server's plan table arrived or changed: the prices shown, the "included" wording and whether
    // the views load by themselves (a plan that includes macro data loads them like a free source's;
    // otherwise, and without a table, nothing runs by itself) follow it. Before the pane is configured
    // (first show) load_policy() is read instead, which says the same.
    connect(&billing::PlanAccess::instance(), &billing::PlanAccess::changed, this, [this]() {
        update_credit_hint();
        sync_load_view(); // the view's preview text and price first, then the policy presents it
        if (load_pane())
            load_pane()->set_policy(services::FinceptMacroService::included_in_plan()
                                        ? ui::AutoLoadPane::Policy::AutoRun
                                        : ui::AutoLoadPane::Policy::Manual);
    });
}

void FinceptMacroPanel::activate() {
    // Nothing billed is fetched by itself unless the account's plan includes macro data (then the
    // views load like a free source's — load_policy()). The results pane shows the VIEW's remembered
    // result with its "as of" time, or a preview naming this account's price. The CEIC country list
    // (a metadata call, FinceptMacroService::credit_cost) loads on the CEIC view's FETCH (its
    // automatic run on an included plan) — never on picking the view.
}

EconPanelBase::LoadPolicy FinceptMacroPanel::load_policy() const {
    LoadPolicy p;
    // Billed: manual (FETCH) — unless the plan includes macro data at no credit cost (no plan table:
    // manual, as before). Re-applied on PlanAccess::changed (constructor).
    p.manual = !services::FinceptMacroService::included_in_plan();
    p.shape = ui::LoadingSkeleton::Shape::Table;
    p.max_age_s = 24 * 3600; // older than a day: "may be out of date · REFRESH"
    p.preview_title = tr("Fincept Macro");
    p.preview_message = tr("Central-bank rates, sovereign CDS, bond spreads, ratings, forecasts, the macro "
                           "calendar and CEIC series from the Fincept API.");
    p.preview_run = tr("FETCH");
    p.preview_note = tr("Each request uses Fincept credits; repeats inside the reuse window are free");
    return p;
}

QList<ui::Requirement> FinceptMacroPanel::load_requirements() const {
    return {ui::Requirement::signed_in()};
}

void FinceptMacroPanel::on_query_cancelled() {
    // FinceptMacroService has no cancel: the answer is ignored when it arrives.
    pending_request_id_.clear();
}

void FinceptMacroPanel::sync_load_view() {
    const QString view = current_view();
    const fmpd::FmView* def = fmpd::find_view(view);
    if (!def || !load_pane())
        return;
    // The views share the result table: an answer still on its way for the view being left is not
    // drawn (the request was sent — the service keeps its answer for the reuse window).
    if (query_running() && load_pane()->running_view() != view)
        load_pane()->cancel();
    const QString endpoint = QString::fromLatin1(def->endpoint);
    // The pane checks a known balance against what this account's plan pays for the view's request
    // (list price in, PlanAccess decides) and marks its re-run buttons "spend" unless it is included.
    load_pane()->set_cost_credits(services::FinceptMacroService::credit_cost(endpoint),
                                  services::FinceptMacroService::plan_category());
    // This account's price (the plan's; the list price without a plan table).
    const QString cost = fmpd::credits_text(services::FinceptMacroService::effective_credit_cost(endpoint));
    // Macro data is included with the plan: no price on the button for such an account.
    if (services::FinceptMacroService::included_in_plan())
        load_pane()->set_view_preview(
            view, ui::EmptyState::preview_spec(tr(def->label), tr(def->tip), tr("FETCH"),
                                               tr("Included with your Pro plan — no credits used"),
                                               ui::EmptyState::Tone::Info, ui::LoadingSkeleton::Shape::Table));
    else
        load_pane()->set_view_preview(
            view, ui::EmptyState::preview_spec(tr(def->label), tr(def->tip), tr("FETCH · ≈%1 CREDIT(S)").arg(cost),
                                               tr("About %1 credit(s) per request · repeats inside the reuse window "
                                                  "are free")
                                                   .arg(cost),
                                               ui::EmptyState::Tone::Warning, ui::LoadingSkeleton::Shape::Table));
    // The macro calendar is a list of events; every other view is a table. Upcoming events change
    // within the day (a release moves, an actual comes in): "may be out of date" after 6 h, not 24.
    const bool events = view == QLatin1String("events");
    set_view_load_policy(view, events ? ui::LoadingSkeleton::Shape::List : ui::LoadingSkeleton::Shape::Table,
                         events ? 6 * 3600 : load_policy().max_age_s);
    set_load_view(view);
}

void FinceptMacroPanel::build_controls(QHBoxLayout* thl) {
    auto make_lbl = [](const QString& text) {
        auto* l = new QLabel(text);
        l->setObjectName(QStringLiteral("econCtrlLabel"));
        return l;
    };

    view_lbl_ = make_lbl(tr("VIEW"));
    view_combo_ = new QComboBox;
    for (const auto& v : fmpd::kFmViews) {
        view_combo_->addItem(tr(v.label), QString::fromLatin1(v.id));
        view_combo_->setItemData(view_combo_->count() - 1, tr(v.tip), Qt::ToolTipRole);
    }
    view_combo_->setMinimumHeight(26);
    view_combo_->setMinimumWidth(190);
    view_combo_->setAccessibleName(tr("Fincept Macro view"));
    connect(view_combo_, &QComboBox::currentIndexChanged, this, [this]() {
        update_controls_for_view();
        update_credit_hint();
        sync_load_view();
        // A CEIC view's country list is a billed metadata call (list price:
        // FinceptMacroService::credit_cost, the plan's price applies): it loads on
        // the view's FETCH (run_query), never on picking the view.
    });

    country_lbl_ = make_lbl(tr("COUNTRY"));
    country_edit_ = new QLineEdit;
    country_edit_->setMinimumHeight(26);
    set_text_width(country_edit_, 130);
    country_edit_->setClearButtonEnabled(true);
    country_edit_->setAccessibleName(tr("Country filter"));
    // Enter in a toolbar field runs the query (EconPanelBase): a billed request.
    country_edit_->setProperty(fincept::core::agent::kAgentRiskProperty, "spend");

    maturity_lbl_ = make_lbl(tr("MATURITY"));
    maturity_combo_ = new QComboBox;
    maturity_combo_->addItem(tr("All"), QString());
    for (const char* m : {"1Y", "2Y", "5Y", "10Y", "30Y"})
        maturity_combo_->addItem(QString::fromLatin1(m), QString::fromLatin1(m));
    maturity_combo_->setMinimumHeight(26);

    importance_lbl_ = make_lbl(tr("IMPORTANCE"));
    importance_combo_ = new QComboBox;
    importance_combo_->addItem(tr("All"), QString());
    importance_combo_->addItem(tr("High"), QStringLiteral("3"));
    importance_combo_->addItem(tr("Medium"), QStringLiteral("2"));
    importance_combo_->addItem(tr("Low"), QStringLiteral("1"));
    importance_combo_->setMinimumHeight(26);

    days_lbl_ = make_lbl(tr("DAYS"));
    days_spin_ = new QSpinBox;
    days_spin_->setRange(1, 60);
    days_spin_->setValue(7);
    days_spin_->setMinimumHeight(26);
    days_spin_->setToolTip(tr("Look-ahead window from today"));

    ceic_country_lbl_ = make_lbl(tr("COUNTRY"));
    ceic_country_combo_ = new QComboBox;
    ceic_country_combo_->setMinimumHeight(26);
    ceic_country_combo_->setMinimumWidth(160);
    ceic_country_combo_->setPlaceholderText(tr("Press FETCH to load the list"));
    // Choosing a country loads its indicator list (a billed metadata call, the plan's price).
    ceic_country_combo_->setProperty(fincept::core::agent::kAgentRiskProperty, "spend");
    connect(ceic_country_combo_, &QComboBox::currentIndexChanged, this, [this]() {
        if (current_view() == QLatin1String("ceic"))
            load_ceic_indicators();
    });

    ceic_indicator_lbl_ = make_lbl(tr("INDICATOR"));
    ceic_indicator_combo_ = new QComboBox;
    ceic_indicator_combo_->setMinimumHeight(26);
    ceic_indicator_combo_->setMinimumWidth(240);
    ceic_indicator_combo_->setMaxVisibleItems(25);
    ceic_indicator_combo_->setPlaceholderText(tr("Pick a country first"));

    const int this_year = QDate::currentDate().year();
    years_lbl_ = make_lbl(tr("YEARS"));
    year_from_spin_ = new QSpinBox;
    year_from_spin_->setRange(1900, this_year + 5);
    year_from_spin_->setValue(this_year - 15);
    year_from_spin_->setMinimumHeight(26);
    year_from_spin_->setToolTip(tr("First year to fetch"));
    year_to_spin_ = new QSpinBox;
    year_to_spin_->setRange(1900, this_year + 5);
    year_to_spin_->setValue(this_year);
    year_to_spin_->setMinimumHeight(26);
    year_to_spin_->setToolTip(tr("Last year to fetch"));

    category_lbl_ = make_lbl(tr("CATEGORY"));
    category_edit_ = new QLineEdit;
    category_edit_->setMinimumHeight(26);
    set_text_width(category_edit_, 130);
    category_edit_->setClearButtonEnabled(true);
    category_edit_->setPlaceholderText(tr("e.g. prices"));
    category_edit_->setProperty(fincept::core::agent::kAgentRiskProperty, "spend"); // Enter runs the billed query

    refresh_btn_ = new QPushButton(tr("REFRESH"));
    refresh_btn_->setObjectName(QStringLiteral("econCsvBtn"));
    refresh_btn_->setProperty("fincept_agent_risk", "spend"); // bypasses the cache: billed
    refresh_btn_->setCursor(Qt::PointingHandCursor);
    connect(refresh_btn_, &QPushButton::clicked, this, [this]() {
        emit query_started();
        run_query(/*force=*/true); // supersedes a request in flight
    });

    credit_lbl_ = new QLabel;
    credit_lbl_->setObjectName(QStringLiteral("econCreditHint"));

    thl->addWidget(view_lbl_);
    thl->addWidget(view_combo_);
    thl->addSpacing(6);
    for (QWidget* w : std::initializer_list<QWidget*>{country_lbl_, country_edit_, maturity_lbl_, maturity_combo_,
                                                     importance_lbl_, importance_combo_, days_lbl_, days_spin_,
                                                     ceic_country_lbl_, ceic_country_combo_, ceic_indicator_lbl_,
                                                     ceic_indicator_combo_, years_lbl_, year_from_spin_,
                                                     year_to_spin_, category_lbl_, category_edit_})
        thl->addWidget(w);
    thl->addSpacing(6);
    thl->addWidget(refresh_btn_);
    thl->addWidget(credit_lbl_);
    // What the plan changes for macro data (included with Pro: no credits used)
    // — hidden until the server has sent its plan table.
    auto* plan_hint = new ui::PlanHintLabel(QStringLiteral("macro"), this);
    plan_hint->set_compact(true);
    thl->addWidget(plan_hint);
}

QString FinceptMacroPanel::current_view() const {
    return view_combo_ ? view_combo_->currentData().toString() : QString();
}

void FinceptMacroPanel::update_controls_for_view() {
    const QString v = current_view();
    const bool country_text = v == QLatin1String("cb_rates") || v == QLatin1String("cds") ||
                              v == QLatin1String("spreads") || v == QLatin1String("ratings") ||
                              v == QLatin1String("forecast") || v == QLatin1String("country") ||
                              v == QLatin1String("events");
    country_lbl_->setVisible(country_text);
    country_edit_->setVisible(country_text);
    if (v == QLatin1String("country")) {
        country_edit_->setPlaceholderText(tr("e.g. united-states"));
        country_edit_->setToolTip(tr("Country slug (lower-case, hyphenated). Empty = all countries"));
    } else if (v == QLatin1String("events")) {
        country_edit_->setPlaceholderText(tr("e.g. US"));
        country_edit_->setToolTip(tr("ISO country code (US, DE, JP, GB, …). Empty = all countries"));
    } else {
        country_edit_->setPlaceholderText(tr("e.g. Germany"));
        country_edit_->setToolTip(tr("Country name or part of it. Empty = all countries"));
    }
    maturity_lbl_->setVisible(v == QLatin1String("forecast"));
    maturity_combo_->setVisible(v == QLatin1String("forecast"));
    const bool events = v == QLatin1String("events");
    importance_lbl_->setVisible(events);
    importance_combo_->setVisible(events);
    days_lbl_->setVisible(events);
    days_spin_->setVisible(events);
    const bool ceic = fmpd::is_ceic_view(v);
    ceic_country_lbl_->setVisible(ceic);
    ceic_country_combo_->setVisible(ceic);
    const bool series = v == QLatin1String("ceic");
    ceic_indicator_lbl_->setVisible(series);
    ceic_indicator_combo_->setVisible(series);
    years_lbl_->setVisible(series);
    year_from_spin_->setVisible(series);
    year_to_spin_->setVisible(series);
    category_lbl_->setVisible(v == QLatin1String("ceic_catalog"));
    category_edit_->setVisible(v == QLatin1String("ceic_catalog"));
}

void FinceptMacroPanel::update_credit_hint() {
    const fmpd::FmView* view = fmpd::find_view(current_view());
    if (!view || !credit_lbl_)
        return;
    const QString endpoint = QString::fromLatin1(view->endpoint);
    // This account's prices (the plan's; the list prices without a plan table).
    const double cost = services::FinceptMacroService::effective_credit_cost(endpoint);
    const QString reuse = fmpd::duration_text(services::FinceptMacroService::fresh_seconds(endpoint));
    QString text = tr("≈%1 credit/request · reused %2").arg(fmpd::credits_text(cost), reuse);
    if (current_view() == QLatin1String("ceic"))
        text = tr("≈%1 credit/request (+%2 per country/indicator list, once a day) · reused %3")
                   .arg(fmpd::credits_text(cost),
                        fmpd::credits_text(
                            services::FinceptMacroService::effective_credit_cost(fmpd::fm_ceic_list_endpoint())),
                        reuse);
    const bool included = services::FinceptMacroService::included_in_plan();
    if (included)
        text = tr("no credits · reused %1").arg(reuse);
    credit_lbl_->setText(text);
    credit_lbl_->setToolTip(tr("Fincept bills each live request in credits. Repeating the same query within %1 is "
                               "answered from the local cache and costs nothing; REFRESH always asks the server.")
                                .arg(reuse));
    refresh_btn_->setToolTip(included ? tr("Ask the server again, bypassing the local cache (included with Pro)")
                                      : tr("Ask the server again, bypassing the local cache (≈%1 credit)")
                                            .arg(fmpd::credits_text(cost)));
}

// ── CEIC lists ───────────────────────────────────────────────────────────────

void FinceptMacroPanel::load_ceic_countries() {
    if (ceic_countries_requested_)
        return;
    ceic_countries_requested_ = true;
    ceic_country_combo_->setPlaceholderText(tr("Loading countries…"));
    services::FinceptMacroService::instance().fetch(QString::fromLatin1(fmpd::kFmCountriesRequest),
                                                    QStringLiteral("ceic/series/countries"), {});
}

void FinceptMacroPanel::load_ceic_indicators() {
    const QString country = ceic_country_combo_->currentData().toString();
    if (country.isEmpty() || (country == ceic_indicators_country_ && ceic_indicator_combo_->count() > 0))
        return;
    ceic_indicators_country_ = country;
    {
        const QSignalBlocker block(ceic_indicator_combo_);
        ceic_indicator_combo_->clear();
    }
    ceic_indicator_combo_->setPlaceholderText(tr("Loading indicators…"));
    services::FinceptMacroService::instance().fetch(QString::fromLatin1(fmpd::kFmIndicatorsRequestPrefix) + country,
                                                    QStringLiteral("ceic/series/indicators"),
                                                    {{QStringLiteral("country"), country}});
}

// ── Fetch ─────────────────────────────────────────────────────────────────────

void FinceptMacroPanel::on_fetch() {
    run_query(/*force=*/false);
}

void FinceptMacroPanel::run_query(bool force) {
    const QString view = current_view();
    const fmpd::FmView* def = fmpd::find_view(view);
    if (!def)
        return;
    services::FinceptMacroService::Params params;
    const QString country = country_edit_->text().trimmed();

    if (view == QLatin1String("cb_rates") || view == QLatin1String("cds") || view == QLatin1String("spreads") ||
        view == QLatin1String("ratings")) {
        params << qMakePair(QStringLiteral("country"), country);
    } else if (view == QLatin1String("forecast")) {
        params << qMakePair(QStringLiteral("country"), country)
               << qMakePair(QStringLiteral("maturity"), maturity_combo_->currentData().toString());
    } else if (view == QLatin1String("country")) {
        QString slug = country.toLower();
        slug.replace(QRegularExpression(QStringLiteral("\\s+")), QStringLiteral("-"));
        params << qMakePair(QStringLiteral("country"), slug);
    } else if (view == QLatin1String("events")) {
        const QDate today = QDate::currentDate();
        params << qMakePair(QStringLiteral("country"), country.toUpper())
               << qMakePair(QStringLiteral("importance"), importance_combo_->currentData().toString())
               << qMakePair(QStringLiteral("start_date"), today.toString(Qt::ISODate))
               << qMakePair(QStringLiteral("end_date"), today.addDays(days_spin_->value()).toString(Qt::ISODate))
               << qMakePair(QStringLiteral("limit"), QStringLiteral("500"));
    } else if (fmpd::is_ceic_view(view)) {
        if (ceic_country_combo_->count() == 0) {
            // The view's first FETCH loads its country list (a metadata call) — never picking the view.
            ceic_countries_requested_ = false; // a failed earlier load must be retryable
            load_ceic_countries();
            const double list_cost =
                services::FinceptMacroService::effective_credit_cost(fmpd::fm_ceic_list_endpoint());
            show_empty(list_cost > 0.0
                           ? tr("Loading the CEIC country list (≈%1 credit) — pick a country, then press FETCH.")
                                 .arg(fmpd::credits_text(list_cost))
                           : tr("Loading the CEIC country list — pick a country, then press FETCH."));
            return;
        }
        const QString ceic_country = ceic_country_combo_->currentData().toString();
        if (ceic_country.isEmpty()) {
            show_empty(tr("Pick a CEIC country first."));
            return;
        }
        if (view == QLatin1String("ceic_catalog")) {
            params = fmpd::catalog_params(ceic_country);
            params << qMakePair(QStringLiteral("category"), category_edit_->text().trimmed());
        } else {
            if (ceic_indicator_combo_->count() == 0) {
                load_ceic_indicators();
                show_empty(tr("Loading the indicator list for this country — pick one, then press FETCH."));
                return;
            }
            const QString indicator = ceic_indicator_combo_->currentData().toString();
            params << qMakePair(QStringLiteral("country"), ceic_country)
                   << qMakePair(QStringLiteral("limit"), QString::number(fmpd::kFmCeicLimit));
            if (indicator.isEmpty()) {
                params << qMakePair(QStringLiteral("latest_only"), QStringLiteral("true"));
            } else {
                const int from = std::min(year_from_spin_->value(), year_to_spin_->value());
                const int to = std::max(year_from_spin_->value(), year_to_spin_->value());
                params << qMakePair(QStringLiteral("indicator"), indicator)
                       << qMakePair(QStringLiteral("year_from"), QString::number(from))
                       << qMakePair(QStringLiteral("year_to"), QString::number(to));
            }
        }
    }

    pending_view_ = view;
    pending_request_id_ = QStringLiteral("fm_%1_%2").arg(view).arg(++request_seq_);
    mark_query_pending(); // answered through FinceptMacroService, not EconomicsService
    show_loading(force ? tr("Refreshing %1 from Fincept…").arg(tr(def->label))
                       : tr("Fetching %1 from Fincept…").arg(tr(def->label)));
    services::FinceptMacroService::instance().fetch(pending_request_id_, QString::fromLatin1(def->endpoint), params,
                                                    force);
}

void FinceptMacroPanel::on_result(const QString& /*request_id*/, const services::EconomicsResult& /*result*/) {
    // Data for this panel arrives through FinceptMacroService::result_ready → on_macro_result().
}

// ── Results ───────────────────────────────────────────────────────────────────

void FinceptMacroPanel::on_macro_result(const services::FinceptMacroResult& r) {
    using Status = services::FinceptMacroResult::Status;
    const bool usable = r.status == Status::Ok || r.stale;

    if (r.request_id == QLatin1String(fmpd::kFmCountriesRequest)) {
        if (!usable || r.rows.isEmpty()) {
            ceic_countries_requested_ = false; // let FETCH / reopening retry
            ceic_country_combo_->setPlaceholderText(tr("Unavailable — press FETCH to retry"));
            if (fmpd::is_ceic_view(current_view()) && pending_request_id_.isEmpty())
                show_failure(r);
            return;
        }
        QList<QJsonObject> sorted;
        for (const auto& v : r.rows)
            sorted << v.toObject();
        std::sort(sorted.begin(), sorted.end(), [](const QJsonObject& a, const QJsonObject& b) {
            return a.value(QStringLiteral("country")).toString() < b.value(QStringLiteral("country")).toString();
        });
        static const QLocale kLoc(QLocale::English);
        {
            const QSignalBlocker block(ceic_country_combo_);
            ceic_country_combo_->clear();
            for (const auto& o : std::as_const(sorted)) {
                const QString slug = o.value(QStringLiteral("country")).toString();
                if (slug.isEmpty())
                    continue;
                const qint64 n = static_cast<qint64>(o.value(QStringLiteral("row_count")).toDouble());
                ceic_country_combo_->addItem(tr("%1  (%2 obs)").arg(fmpd::humanize_slug(slug), kLoc.toString(n)), slug);
            }
        }
        reapply_pending_inputs(); // the saved country (if any) now exists
        if (current_view() == QLatin1String("ceic"))
            load_ceic_indicators();
        return;
    }

    if (r.request_id.startsWith(QLatin1String(fmpd::kFmIndicatorsRequestPrefix))) {
        const QString country = r.request_id.mid(static_cast<int>(qstrlen(fmpd::kFmIndicatorsRequestPrefix)));
        if (country != ceic_indicators_country_)
            return; // superseded by a newer country selection
        if (!usable || r.rows.isEmpty()) {
            ceic_indicators_country_.clear();
            ceic_indicator_combo_->setPlaceholderText(tr("Unavailable — press FETCH to retry"));
            if (current_view() == QLatin1String("ceic") && pending_request_id_.isEmpty())
                show_failure(r);
            return;
        }
        QList<QPair<QString, QString>> items; // (label, slug)
        for (const auto& v : r.rows) {
            const QJsonObject o = v.toObject();
            QString slug = o.value(QStringLiteral("indicator")).toString();
            if (slug.isEmpty())
                slug = o.value(QStringLiteral("series_id")).toString();
            if (slug.isEmpty())
                continue;
            const qint64 pts = static_cast<qint64>(o.value(QStringLiteral("data_points")).toDouble());
            items.append({tr("%1  (%2 pts)").arg(fmpd::humanize_slug(slug)).arg(pts), slug});
        }
        std::sort(items.begin(), items.end(),
                  [](const QPair<QString, QString>& a, const QPair<QString, QString>& b) {
                      return QString::compare(a.first, b.first, Qt::CaseInsensitive) < 0;
                  });
        {
            const QSignalBlocker block(ceic_indicator_combo_);
            ceic_indicator_combo_->clear();
            ceic_indicator_combo_->addItem(tr("All indicators — latest value of each"), QString());
            for (const auto& it : std::as_const(items))
                ceic_indicator_combo_->addItem(it.first, it.second);
        }
        reapply_pending_inputs();
        return;
    }

    if (r.request_id != pending_request_id_)
        return; // an older request the user has since replaced
    pending_request_id_.clear();
    if (!usable) {
        show_failure(r);
        return;
    }
    show_view_result(r);
}

void FinceptMacroPanel::show_failure(const services::FinceptMacroResult& r) {
    using Status = services::FinceptMacroResult::Status;
    switch (r.status) {
        case Status::Unauthorized:
            show_error(tr("Sign-in required: the Fincept API rejected this session (HTTP %1).\n"
                          "Sign in again (your session may have expired), then retry.")
                           .arg(r.http_status));
            return;
        case Status::NoCredits: {
            QString text = tr("Out of Fincept credits — this request costs about %1 credit(s).\n"
                              "Recharge or upgrade your plan, then retry.")
                               .arg(fmpd::credits_text(fmpd::fm_charged_cost(r.endpoint)));
            // The server's own sentence (its cost / balance figures) when it sent
            // one — the service falls back to a bare "HTTP 402" otherwise.
            if (!r.error.isEmpty() && r.error != QStringLiteral("HTTP %1").arg(r.http_status))
                text += QLatin1Char('\n') + tr("Server: %1").arg(r.error);
            // On Free, say what Pro changes for this data (empty on Pro / older servers).
            const QString pro_note = ui::plan::out_of_credits_note(QStringLiteral("macro"));
            if (!pro_note.isEmpty())
                text += QLatin1Char('\n') + pro_note;
            show_error_with_action(text, tr("VIEW PLANS"), [this]() { ui::billing::open_plans(this); });
            return;
        }
        case Status::Offline:
            show_error(tr("Cannot reach the Fincept API — %1\nCheck your connection, then retry.").arg(r.error));
            return;
        case Status::Error:
        case Status::Ok:
            break;
    }
    show_error(tr("Fincept API error: %1").arg(r.error.isEmpty() ? tr("unknown failure") : r.error));
}

void FinceptMacroPanel::show_view_result(const services::FinceptMacroResult& r) {
    using Status = services::FinceptMacroResult::Status;
    const QString view = pending_view_;
    const fmpd::FmView* def = fmpd::find_view(view);
    if (!def)
        return;
    if (r.rows.isEmpty()) {
        show_empty(view == QLatin1String("events")
                       ? tr("No events in this window — widen DAYS or clear the filters.")
                       : tr("Nothing matches this filter — clear it or try another spelling."));
        return;
    }

    QJsonArray rows;
    QStringList cols;
    SeriesMeta meta;
    bool keep_order = true;
    bool stats = false;
    QString title = tr(def->label);
    QStringList banners;

    const QString wgb_source = tr("WorldGovernmentBonds via Fincept");
    auto num = [](const QJsonObject& o, const char* key) { return o.value(QLatin1String(key)); };

    if (view == QLatin1String("cb_rates")) {
        cols = {tr("Country"), tr("Policy Rate (%)"), tr("Last Move (bp)"), tr("Period"), tr("Scraped")};
        for (const auto& v : r.rows) {
            const QJsonObject o = v.toObject();
            rows.append(QJsonObject{{cols[0], o.value(QStringLiteral("country"))},
                                    {cols[1], num(o, "rate_pct")},
                                    {cols[2], num(o, "change_bp")},
                                    {cols[3], o.value(QStringLiteral("period"))},
                                    {cols[4], o.value(QStringLiteral("scraped_at"))}});
        }
        meta.source = wgb_source;
        meta.last_updated = fmpd::latest_scrape(r.rows);
        meta.note = tr("Rates in % · moves in basis points");
    } else if (view == QLatin1String("cds")) {
        cols = {tr("Country"), tr("Code"), tr("CDS (bp)"), tr("Scraped")};
        for (const auto& v : r.rows) {
            const QJsonObject o = v.toObject();
            rows.append(QJsonObject{{cols[0], o.value(QStringLiteral("country_name"))},
                                    {cols[1], o.value(QStringLiteral("country_code"))},
                                    {cols[2], num(o, "cds_value")},
                                    {cols[3], o.value(QStringLiteral("scraped_at"))}});
        }
        meta.source = wgb_source;
        meta.last_updated = fmpd::latest_scrape(r.rows);
    } else if (view == QLatin1String("spreads")) {
        cols = {tr("Country"),          tr("10Y Yield (%)"),     tr("Spread vs Germany (bp)"), tr("Spread vs US (bp)"),
                tr("Spread vs China (bp)"), tr("Spread vs Japan (bp)"), tr("Scraped")};
        for (const auto& v : r.rows) {
            const QJsonObject o = v.toObject();
            rows.append(QJsonObject{{cols[0], o.value(QStringLiteral("country"))},
                                    {cols[1], num(o, "yield_10y_pct")},
                                    {cols[2], num(o, "spread_vs_ger")},
                                    {cols[3], num(o, "spread_vs_us")},
                                    {cols[4], num(o, "spread_vs_cn")},
                                    {cols[5], num(o, "spread_vs_jp")},
                                    {cols[6], o.value(QStringLiteral("scraped_at"))}});
        }
        meta.source = wgb_source;
        meta.last_updated = fmpd::latest_scrape(r.rows);
    } else if (view == QLatin1String("ratings")) {
        cols = {tr("Country"), tr("S&P"), tr("Moody's"), tr("Fitch"), tr("DBRS"), tr("Scraped")};
        for (const auto& v : r.rows) {
            const QJsonObject o = v.toObject();
            rows.append(QJsonObject{{cols[0], o.value(QStringLiteral("country"))},
                                    {cols[1], o.value(QStringLiteral("sp_rating"))},
                                    {cols[2], o.value(QStringLiteral("moodys_rating"))},
                                    {cols[3], o.value(QStringLiteral("fitch_rating"))},
                                    {cols[4], o.value(QStringLiteral("dbrs_rating"))},
                                    {cols[5], o.value(QStringLiteral("scraped_at"))}});
        }
        meta.source = wgb_source;
        meta.last_updated = fmpd::latest_scrape(r.rows);
    } else if (view == QLatin1String("inverted")) {
        cols = {tr("Group / Country"), tr("Details"), tr("Scraped")};
        for (const auto& v : r.rows) {
            const QJsonObject o = v.toObject();
            rows.append(QJsonObject{{cols[0], o.value(QStringLiteral("country"))},
                                    {cols[1], o.value(QStringLiteral("details"))},
                                    {cols[2], o.value(QStringLiteral("scraped_at"))}});
        }
        meta.source = wgb_source;
        meta.last_updated = fmpd::latest_scrape(r.rows);
    } else if (view == QLatin1String("forecast")) {
        cols = {tr("Country"),           tr("Maturity"),          tr("Current Yield (%)"),
                tr("Forecast Yield (%)"), tr("Implied Move (bp)"), tr("Scraped")};
        for (const auto& v : r.rows) {
            const QJsonObject o = v.toObject();
            QJsonObject row{{cols[0], o.value(QStringLiteral("country"))},
                            {cols[1], o.value(QStringLiteral("maturity"))},
                            {cols[2], num(o, "current_yield")},
                            {cols[3], num(o, "forecast_yield")},
                            {cols[5], o.value(QStringLiteral("scraped_at"))}};
            const QJsonValue cur = num(o, "current_yield");
            const QJsonValue fc = num(o, "forecast_yield");
            if (cur.isDouble() && fc.isDouble())
                row.insert(cols[4], std::round((fc.toDouble() - cur.toDouble()) * 1000.0) / 10.0); // % → bp, 0.1
            rows.append(row);
        }
        meta.source = wgb_source;
        meta.last_updated = fmpd::latest_scrape(r.rows);
        meta.note = tr("Implied move = forecast − current, in basis points");
    } else if (view == QLatin1String("country")) {
        cols = {tr("Country"),  tr("10Y Yield (%)"), tr("Policy Rate (%)"),      tr("Rate Since"),
                tr("Rating"),   tr("CDS (bp)"),      tr("Default Prob. (%)"),    tr("Main Spread (bp)"),
                tr("Scraped")};
        for (const auto& v : r.rows) {
            const QJsonObject o = v.toObject();
            rows.append(QJsonObject{{cols[0], fmpd::humanize_slug(o.value(QStringLiteral("country_slug")).toString())},
                                    {cols[1], num(o, "bond_10y_yield")},
                                    {cols[2], num(o, "cb_rate")},
                                    {cols[3], o.value(QStringLiteral("cb_rate_date"))},
                                    {cols[4], o.value(QStringLiteral("credit_rating"))},
                                    {cols[5], num(o, "cds_spread")},
                                    {cols[6], num(o, "default_probability")},
                                    {cols[7], num(o, "main_spread_value")},
                                    {cols[8], o.value(QStringLiteral("scraped_at"))}});
        }
        meta.source = wgb_source;
        meta.last_updated = fmpd::latest_scrape(r.rows);
    } else if (view == QLatin1String("events")) {
        cols = {tr("Date"),     tr("Time"),      tr("Country"),  tr("Event"),        tr("Ref. Period"), tr("Actual"),
                tr("Consensus"), tr("Forecast"), tr("Previous"), tr("Revised From"), tr("Importance"),  tr("Category")};
        for (const auto& v : r.rows) {
            const QJsonObject o = v.toObject();
            const int imp = o.value(QStringLiteral("importance")).toInt(0);
            const QString imp_text = imp >= 3 ? tr("High") : imp == 2 ? tr("Medium") : imp == 1 ? tr("Low") : QString();
            rows.append(QJsonObject{{cols[0], o.value(QStringLiteral("date"))},
                                    {cols[1], o.value(QStringLiteral("time"))},
                                    {cols[2], o.value(QStringLiteral("country"))},
                                    {cols[3], o.value(QStringLiteral("event"))},
                                    {cols[4], o.value(QStringLiteral("reference_period"))},
                                    {cols[5], o.value(QStringLiteral("actual"))},
                                    {cols[6], o.value(QStringLiteral("consensus"))},
                                    {cols[7], o.value(QStringLiteral("forecast"))},
                                    {cols[8], o.value(QStringLiteral("previous"))},
                                    {cols[9], o.value(QStringLiteral("revised_from"))},
                                    {cols[10], imp_text},
                                    {cols[11], o.value(QStringLiteral("category"))}});
        }
        meta.source = tr("Trading Economics calendar via Fincept");
        meta.note = tr("Times as published by the source calendar");
        const int total = r.payload.value(QStringLiteral("total_count")).toInt(static_cast<int>(r.rows.size()));
        if (total > r.rows.size())
            banners << tr("Showing the first %1 of %2 events — shorten DAYS or add filters.").arg(r.rows.size()).arg(total);
    } else if (view == QLatin1String("ceic_catalog")) {
        cols = {tr("Indicator"), tr("Unit"), tr("Frequency"), tr("Category"), tr("Coverage"), tr("Slug"),
                tr("Series ID")};
        for (const auto& v : r.rows) {
            const QJsonObject o = v.toObject();
            rows.append(QJsonObject{{cols[0], o.value(QStringLiteral("name"))},
                                    {cols[1], o.value(QStringLiteral("unit"))},
                                    {cols[2], o.value(QStringLiteral("frequency"))},
                                    {cols[3], o.value(QStringLiteral("category"))},
                                    {cols[4], o.value(QStringLiteral("date_range"))},
                                    {cols[5], o.value(QStringLiteral("slug"))},
                                    {cols[6], o.value(QStringLiteral("series_id"))}});
        }
        meta.source = tr("CEIC via Fincept");
        title = tr("CEIC catalogue — %1").arg(ceic_country_combo_->currentText());
        const int total = r.payload.value(QStringLiteral("total_count")).toInt(static_cast<int>(r.rows.size()));
        if (total > r.rows.size())
            banners << tr("Showing %1 of %2 indicators — add a CATEGORY filter to narrow the list.")
                           .arg(r.rows.size())
                           .arg(total);
    } else if (view == QLatin1String("ceic")) {
        const QString country = ceic_country_combo_->currentData().toString();
        const QString indicator = ceic_indicator_combo_->currentData().toString();
        meta.source = tr("CEIC via Fincept");
        if (indicator.isEmpty()) {
            // latest_only: one row per series — a cross-section of the country's latest readings.
            cols = {tr("Indicator"), tr("Period"), tr("Value"), tr("Series ID")};
            for (const auto& v : r.rows) {
                const QJsonObject o = v.toObject();
                const QString sid = o.value(QStringLiteral("series_id")).toString();
                static const QRegularExpression kSlug(QStringLiteral("^ipc_[^_]+_(.+)$"));
                const auto m = kSlug.match(sid);
                rows.append(QJsonObject{{cols[0], fmpd::humanize_slug(m.hasMatch() ? m.captured(1) : sid)},
                                        {cols[1], fmpd::ceic_period_label(o)},
                                        {cols[2], o.value(QStringLiteral("value"))},
                                        {cols[3], sid}});
            }
            title = tr("CEIC — latest values, %1").arg(fmpd::humanize_slug(country));
            meta.note = tr("Each row is a different indicator — units differ; see the CEIC Indicator Catalogue");
        } else {
            // One series: untranslated date / value keys so the base recognises the time series.
            cols = {QStringLiteral("date"), QStringLiteral("value"), QStringLiteral("series_id")};
            QString series_id;
            for (const auto& v : r.rows) {
                const QJsonObject o = v.toObject();
                series_id = o.value(QStringLiteral("series_id")).toString();
                rows.append(QJsonObject{{cols[0], fmpd::ceic_period_label(o)},
                                        {cols[1], o.value(QStringLiteral("value"))},
                                        {cols[2], series_id}});
            }
            keep_order = false;
            stats = true;
            title = tr("CEIC — %1, %2").arg(fmpd::humanize_slug(indicator), fmpd::humanize_slug(country));
            // Units / frequency are not part of the series rows. Label them from a cached copy of the
            // country's CEIC catalogue when one exists (no extra request, no credits).
            bool labelled = false;
            if (const auto cat = services::FinceptMacroService::instance().cached_payload(
                    QStringLiteral("ceic/indicators"), fmpd::catalog_params(country))) {
                const QJsonArray entries = cat->value(QStringLiteral("indicators")).toArray();
                for (const auto& ev : entries) {
                    const QJsonObject e = ev.toObject();
                    if (e.value(QStringLiteral("series_id")).toString() == series_id ||
                        e.value(QStringLiteral("slug")).toString() == indicator) {
                        meta.units = e.value(QStringLiteral("unit")).toString();
                        meta.frequency = e.value(QStringLiteral("frequency")).toString();
                        const QString name = e.value(QStringLiteral("name")).toString();
                        if (!name.isEmpty())
                            title = tr("CEIC — %1, %2").arg(name, fmpd::humanize_slug(country));
                        labelled = true;
                        break;
                    }
                }
            }
            if (!labelled)
                meta.note = tr("Units not included in CEIC series rows — open the CEIC Indicator Catalogue view for "
                               "this country to label them");
            const int total = r.payload.value(QStringLiteral("total_count")).toInt(static_cast<int>(r.rows.size()));
            if (total > r.rows.size())
                banners << tr("Showing the OLDEST %1 of %2 observations (the API returns oldest first) — raise the "
                              "first YEAR to include the latest.")
                               .arg(r.rows.size())
                               .arg(total);
        }
    }

    set_retrieval_info(r.fetched_at, r.from_cache);
    set_stats_visible(stats);
    set_series_meta(meta);
    display(rows, title, cols, keep_order);

    // display() clears the banner; set it afterwards.
    if (r.stale) {
        const QString when = r.fetched_at.toLocalTime().toString(QStringLiteral("yyyy-MM-dd HH:mm"));
        banners.prepend(r.status == Status::Offline
                            ? tr("OFFLINE — the live request failed (%1). Showing the copy retrieved %2.")
                                  .arg(r.error, when)
                            : tr("FINCEPT API ERROR — %1. Showing the copy retrieved %2.").arg(r.error, when));
    }
    set_banner(banners.join(QStringLiteral("   ·   ")));
    LOG_DEBUG("FinceptMacroPanel", QString("Displayed %1 row(s) for %2%3")
                                      .arg(rows.size())
                                      .arg(view, r.stale ? QStringLiteral(" (stale)") : QString()));
}

// ── i18n ──────────────────────────────────────────────────────────────────────

void FinceptMacroPanel::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange)
        retranslateUi();
    EconPanelBase::changeEvent(event);
}

void FinceptMacroPanel::retranslateUi() {
    if (view_lbl_)
        view_lbl_->setText(tr("VIEW"));
    if (view_combo_) {
        int i = 0;
        for (const auto& v : fmpd::kFmViews) {
            if (i >= view_combo_->count())
                break;
            view_combo_->setItemText(i, tr(v.label));
            view_combo_->setItemData(i, tr(v.tip), Qt::ToolTipRole);
            ++i;
        }
    }
    if (country_lbl_)
        country_lbl_->setText(tr("COUNTRY"));
    if (maturity_lbl_)
        maturity_lbl_->setText(tr("MATURITY"));
    if (importance_lbl_)
        importance_lbl_->setText(tr("IMPORTANCE"));
    if (days_lbl_)
        days_lbl_->setText(tr("DAYS"));
    if (ceic_country_lbl_)
        ceic_country_lbl_->setText(tr("COUNTRY"));
    if (ceic_indicator_lbl_)
        ceic_indicator_lbl_->setText(tr("INDICATOR"));
    if (years_lbl_)
        years_lbl_->setText(tr("YEARS"));
    if (category_lbl_)
        category_lbl_->setText(tr("CATEGORY"));
    if (refresh_btn_)
        refresh_btn_->setText(tr("REFRESH"));
    if (maturity_combo_ && maturity_combo_->count() > 0)
        maturity_combo_->setItemText(0, tr("All"));
    if (importance_combo_ && importance_combo_->count() >= 4) {
        importance_combo_->setItemText(0, tr("All"));
        importance_combo_->setItemText(1, tr("High"));
        importance_combo_->setItemText(2, tr("Medium"));
        importance_combo_->setItemText(3, tr("Low"));
    }
    if (days_spin_)
        days_spin_->setToolTip(tr("Look-ahead window from today"));
    if (year_from_spin_)
        year_from_spin_->setToolTip(tr("First year to fetch"));
    if (year_to_spin_)
        year_to_spin_->setToolTip(tr("Last year to fetch"));
    if (category_edit_)
        category_edit_->setPlaceholderText(tr("e.g. prices"));
    if (country_edit_)
        update_controls_for_view(); // placeholder / tooltip depend on the view
    update_credit_hint();
    EconPanelBase::retranslateUi();
}

} // namespace fincept::screens
