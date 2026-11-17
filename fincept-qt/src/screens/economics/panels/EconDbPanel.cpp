// src/screens/economics/panels/EconDbPanel.cpp
// EconDB macroeconomic indicators panel. econdb.com currently blocks anonymous API access
// (HTTP 403 behind a browser challenge); the panel says so instead of failing per query.
//
// Response shape:
//   { indicator, country, transform, observation_count,
//     metadata: { description, frequency, country, ticker, dataset },
//     observations: [ {date:"YYYY-MM-DD", value:float}, ... ] }
#include "screens/economics/panels/EconDbPanel.h"

#include "core/logging/Logger.h"
#include "services/economics/EconomicsService.h"

#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonObject>
#include <QLabel>

namespace fincept::screens {
namespace {

static constexpr const char* kEconDbScript = "econdb_data.py";
static constexpr const char* kEconDbSourceId = "econdb";
static constexpr const char* kEconDbColor = "#059669"; // green
} // namespace

static const QList<QPair<QString, QString>> kEconDbIndicators = {
    {"GDP (Nominal)", "gdp"},
    {"Real GDP", "rgdp"},
    {"CPI Inflation", "cpi"},
    {"PPI Inflation", "ppi"},
    {"Core Inflation", "core"},
    {"Unemployment Rate", "urate"},
    {"Employment", "emp"},
    {"Industrial Production", "ip"},
    {"Retail Sales", "reta"},
    {"Consumer Confidence", "conf"},
    {"Policy Interest Rate", "polir"},
    {"10-Year Yield", "y10yd"},
    {"3-Month Yield", "m3yd"},
    {"Government Debt (% GDP)", "gdebt"},
    {"Current Account (% GDP)", "ca"},
    {"Trade Balance", "tb"},
    {"Population", "pop"},
    {"Consumer Prices", "prc"},
    {"Gross Fixed Capital Fmn", "gfcf"},
    {"Exports", "exp"},
    {"Imports", "imp"},
};

// 49 countries available in EconDB (ISO2 codes)
static const QList<QPair<QString, QString>> kEconDbCountries = {
    {"United States", "US"},  {"China", "CN"},        {"Germany", "DE"},        {"Japan", "JP"},
    {"United Kingdom", "GB"}, {"France", "FR"},       {"India", "IN"},          {"Brazil", "BR"},
    {"Canada", "CA"},         {"South Korea", "KR"},  {"Australia", "AU"},      {"Russia", "RU"},
    {"Mexico", "MX"},         {"Spain", "ES"},        {"Italy", "IT"},          {"Netherlands", "NL"},
    {"Switzerland", "CH"},    {"Turkey", "TR"},       {"Poland", "PL"},         {"Sweden", "SE"},
    {"Belgium", "BE"},        {"Argentina", "AR"},    {"Austria", "AT"},        {"Bangladesh", "BD"},
    {"Chile", "CL"},          {"Colombia", "CO"},     {"Czech Republic", "CZ"}, {"Denmark", "DK"},
    {"Egypt", "EG"},          {"Finland", "FI"},      {"Greece", "GR"},         {"Hong Kong", "HK"},
    {"Hungary", "HU"},        {"Indonesia", "ID"},    {"Israel", "IL"},         {"Malaysia", "MY"},
    {"Nigeria", "NG"},        {"Norway", "NO"},       {"Pakistan", "PK"},       {"Peru", "PE"},
    {"Philippines", "PH"},    {"Portugal", "PT"},     {"Romania", "RO"},        {"Saudi Arabia", "SA"},
    {"Singapore", "SG"},      {"South Africa", "ZA"}, {"Taiwan", "TW"},         {"Thailand", "TH"},
    {"Ukraine", "UA"},        {"Vietnam", "VN"},
};

EconDbPanel::EconDbPanel(QWidget* parent) : EconPanelBase(kEconDbSourceId, kEconDbColor, parent) {
    build_base_ui(this);
    connect(&services::EconomicsService::instance(), &services::EconomicsService::result_ready, this,
            &EconDbPanel::on_result);
    // Second action of the preview: the same indicators, key-free, on the DBnomics screen.
    if (auto* pv = load_pane() ? load_pane()->preview() : nullptr) {
        connect(pv, &ui::EmptyState::secondary_clicked, this, []() {
            ui::open_setup_link(ui::SetupLink{QStringLiteral("dbnomics"), QString(), QString()});
        });
    }
}

void EconDbPanel::activate() {
    // econdb.com answers every anonymous API call with HTTP 403 behind a browser (Cloudflare)
    // challenge. The panel stays (FETCH tries the source again) but never runs by itself: its
    // designed preview says why and offers the same indicators on DBnomics.
}

EconPanelBase::LoadPolicy EconDbPanel::load_policy() const {
    LoadPolicy p;
    p.manual = true;
    p.preview_title = tr("EconDB is refusing anonymous requests");
    p.preview_message = tr("econdb.com now blocks API calls without a browser session (HTTP 403). The same "
                           "GDP, inflation, unemployment and trade indicators are available key-free on the "
                           "DBnomics screen.");
    p.preview_run = tr("TRY ECONDB AGAIN");
    p.preview_note = tr("Source currently unavailable");
    p.preview_secondary = tr("OPEN DBNOMICS");
    return p;
}

void EconDbPanel::build_controls(QHBoxLayout* thl) {
    auto lbl = [](const QString& t) {
        auto* l = new QLabel(t);
        l->setObjectName(QStringLiteral("econCtrlLabel"));
        return l;
    };

    indicator_combo_ = new QComboBox;
    for (const auto& p : kEconDbIndicators)
        indicator_combo_->addItem(p.first, p.second);
    indicator_combo_->setMinimumHeight(26);
    indicator_combo_->setMinimumWidth(180);

    country_combo_ = new QComboBox;
    for (const auto& c : kEconDbCountries)
        country_combo_->addItem(c.first, c.second);
    country_combo_->setMinimumHeight(26);
    country_combo_->setMinimumWidth(130);

    thl->addWidget(indicator_lbl_ = lbl(tr("INDICATOR")));
    thl->addWidget(indicator_combo_);
    thl->addWidget(country_lbl_ = lbl(tr("COUNTRY")));
    thl->addWidget(country_combo_);
}

void EconDbPanel::on_fetch() {
    const QString indicator = indicator_combo_->currentData().toString();
    const QString country = country_combo_->currentData().toString();

    show_loading(tr("Fetching EconDB: %1 — %2…").arg(indicator_combo_->currentText(), country_combo_->currentText()));

    execute_query(kEconDbSourceId, kEconDbScript, "indicator", {indicator, country},
                  "econdb_" + indicator + "_" + country);
}

void EconDbPanel::on_result(const QString& request_id, const services::EconomicsResult& result) {
    if (result.source_id != kEconDbSourceId)
        return;
    if (!request_id.startsWith("econdb_"))
        return;
    if (!result.success) {
        // The script reports econdb.com's HTTP 403 as code not_supported: the "not available from this
        // source" page (not a fault to retry at once, not a missing key).
        if (result.error_code == QLatin1String("not_supported") ||
            result.error.contains(QLatin1String("source unavailable")) || result.error.contains(QLatin1String("403")))
            show_error(QStringLiteral("[not_supported] ") +
                       tr("econdb.com blocks anonymous API access (HTTP 403). Use the DBnomics screen for the same "
                          "indicators."));
        else
            show_error(result.error);
        return;
    }

    // Primary shape: {observations:[{date,value}], metadata:{description,...}}
    QJsonArray obs = result.data["observations"].toArray();

    // Fallback: service may wrap as {data:[...]}
    if (obs.isEmpty())
        obs = result.data["data"].toArray();

    if (obs.isEmpty()) {
        show_error(tr("No observations returned"));
        return;
    }

    // Build descriptive title from metadata
    const QJsonObject md = result.data["metadata"].toObject();
    const QString desc = md["description"].toString();
    const QString freq = md["frequency"].toString();
    const QString title = desc.isEmpty() ? "EconDB: " + request_id.mid(7)
                                         : "EconDB: " + desc + (freq.isEmpty() ? "" : " (" + freq + ")");

    SeriesMeta series_meta;
    series_meta.frequency = freq;
    series_meta.note = md["ticker"].toString();
    const QString transform = result.data["transform"].toString();
    if (!transform.isEmpty())
        series_meta.note += (series_meta.note.isEmpty() ? QString() : QStringLiteral(" · ")) +
                            tr("transform: %1").arg(transform);
    set_series_meta(series_meta);
    display(obs, title);
    LOG_DEBUG("EconDbPanel", QString("Displayed %1 observations: %2").arg(obs.size()).arg(title));
}

// ── i18n ──────────────────────────────────────────────────────────────────────

void EconDbPanel::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange)
        retranslateUi();
    EconPanelBase::changeEvent(event);
}

void EconDbPanel::retranslateUi() {
    if (indicator_lbl_)
        indicator_lbl_->setText(tr("INDICATOR"));
    if (country_lbl_)
        country_lbl_->setText(tr("COUNTRY"));
    EconPanelBase::retranslateUi();
}

} // namespace fincept::screens
