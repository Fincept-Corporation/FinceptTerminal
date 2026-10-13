// src/screens/economics/panels/OnsPanel.cpp
// UK Office for National Statistics — GDP, CPI, unemployment, trade, housing.
// Script: ons_data.py  |  No API key required.
//
// Response shape: { success, series, label, unit, count, data:[{date, value}] }
#include "screens/economics/panels/OnsPanel.h"

#include "core/logging/Logger.h"
#include "services/economics/EconomicsService.h"

#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonObject>
#include <QLabel>

namespace fincept::screens {
namespace {

static constexpr const char* kOnsScript = "ons_data.py";
static constexpr const char* kOnsSourceId = "ons";
static constexpr const char* kOnsColor = "#005EB8"; // ONS blue
} // namespace

struct OnsSeries {
    QString label;
    QString command;
};

// D7G7 / L55O / CZBH are the 12-month inflation RATES (%), not the index levels — the labels used to
// say "CPI All Items", inviting a reader to take 3.1 for an index value.
static const QList<OnsSeries> kOnsSeries = {
    {"GDP (Chained Volume, SA)", "gdp"},
    {"CPI Inflation (12-month rate, %)", "cpi"},
    {"CPIH Inflation (12-month rate, %)", "cpih"},
    {"RPI Inflation (12-month rate, %)", "rpi"},
    {"Unemployment Rate (%)", "unemployment"},
    {"Employment Rate (%)", "employment"},
    {"Trade Balance (BoP)", "trade_balance"},
    {"House Prices (UK HPI average price)", "house_prices"},
    {"Average Weekly Earnings", "avg_earnings"},
    {"Public Sector Net Debt (% of GDP)", "public_debt"},
};

OnsPanel::OnsPanel(QWidget* parent) : EconPanelBase(kOnsSourceId, kOnsColor, parent) {
    build_base_ui(this);
    connect(&services::EconomicsService::instance(), &services::EconomicsService::result_ready, this,
            &OnsPanel::on_result);
}

void OnsPanel::activate() {
    show_empty(tr("Select a series and click FETCH\n"
                  "Source: UK Office for National Statistics (no API key required)\n"
                  "Data via api.beta.ons.gov.uk — GDP, CPI, labour market, housing"));
}

void OnsPanel::build_controls(QHBoxLayout* thl) {
    series_lbl_ = new QLabel(tr("SERIES"));
    series_lbl_->setObjectName(QStringLiteral("econCtrlLabel"));

    series_combo_ = new QComboBox;
    for (const auto& s : kOnsSeries)
        series_combo_->addItem(s.label, s.command);
    series_combo_->setFixedHeight(26);
    series_combo_->setMinimumWidth(240);

    thl->addWidget(series_lbl_);
    thl->addWidget(series_combo_);
}

void OnsPanel::on_fetch() {
    const int idx = series_combo_->currentIndex();
    const auto& series = kOnsSeries[idx];

    show_loading(tr("Fetching ONS: %1…").arg(series.label));
    services::EconomicsService::instance().execute(kOnsSourceId, kOnsScript, series.command, {},
                                                   "ons_" + series.command);
}

void OnsPanel::on_result(const QString& request_id, const services::EconomicsResult& result) {
    if (result.source_id != kOnsSourceId)
        return;
    if (!request_id.startsWith("ons_"))
        return;
    if (!result.success) {
        show_error(result.error);
        return;
    }

    // Check inline error
    const QString inline_err = result.data["error"].toString();
    if (!inline_err.isEmpty()) {
        show_error(inline_err);
        return;
    }

    // Response: { success, series, label, unit, count, data:[{date, value}] }
    QJsonArray rows = result.data["data"].toArray();

    if (rows.isEmpty()) {
        show_error(tr("No data returned"));
        return;
    }

    // Label from the request id, not the combo (the user may have switched series mid-request).
    const QString cmd = request_id.mid(4);
    QString fallback = cmd;
    for (const auto& s : kOnsSeries)
        if (s.command == cmd)
            fallback = s.label;
    const QString label = result.data["label"].toString();
    const QString unit = result.data["unit"].toString();
    const QString title = "ONS: " + (label.isEmpty() ? fallback : label);

    SeriesMeta meta;
    meta.units = unit;
    meta.frequency = result.data["frequency"].toString();
    meta.source = result.data["source"].toString();
    meta.last_updated = result.data["release_date"].toString().left(10);
    if (label.contains(QLatin1String("Seasonally adjusted"), Qt::CaseInsensitive))
        meta.seasonal_adj = tr("Seasonally adjusted");
    set_series_meta(meta);
    display(rows, title);
    LOG_INFO("OnsPanel", QString("Displayed %1 rows: %2").arg(rows.size()).arg(title));
}

// ── i18n ──────────────────────────────────────────────────────────────────────

void OnsPanel::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange)
        retranslateUi();
    EconPanelBase::changeEvent(event);
}

void OnsPanel::retranslateUi() {
    if (series_lbl_)
        series_lbl_->setText(tr("SERIES"));
    EconPanelBase::retranslateUi();
}

} // namespace fincept::screens
