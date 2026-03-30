// src/screens/economics/panels/FederalReservePanel.cpp
// US Federal Reserve data — Fed Funds Rate, SOFR, Treasury Rates, Yield Curve, Money Supply.
// Script: federal_reserve_data.py  |  No API key required.
//
// Response shape: { success, endpoint, data:[{date, rate|...}] }
#include "screens/economics/panels/FederalReservePanel.h"

#include "core/logging/Logger.h"
#include "services/economics/EconomicsService.h"

#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonObject>
#include <QLabel>

namespace fincept::screens {
namespace {

static constexpr const char* kFederalReserveScript   = "federal_reserve_data.py";
static constexpr const char* kFederalReserveSourceId = "federal_reserve";
static constexpr const char* kFederalReserveColor    = "#DC2626";  // Fed red
} // namespace

struct FedSeries {
    QString label;
    QString command;
    QStringList args;
};

static const QList<FedSeries> kFedReserveSeries = {
    { "Federal Funds Rate",          "federal_funds_rate", {} },
    { "SOFR Overnight Rate",         "sofr_rate",          {} },
    { "Treasury Rates (Yield Curve)","treasury_rates",     {} },
    { "Yield Curve (single date)",   "yield_curve",        {} },
    { "Money Supply (M1/M2)",        "money_measures",     {} },
    { "Central Bank Holdings",       "central_bank_holdings", {} },
};

FederalReservePanel::FederalReservePanel(QWidget* parent)
    : EconPanelBase(kFederalReserveSourceId, kFederalReserveColor, parent) {
    build_base_ui(this);
    connect(&services::EconomicsService::instance(),
            &services::EconomicsService::result_ready,
            this, &FederalReservePanel::on_result);
}

void FederalReservePanel::activate() {
    show_empty("Select a series and click FETCH\n"
               "Source: Federal Reserve Economic Data — federal_reserve_data.py\n"
               "No API key required");
}

void FederalReservePanel::build_controls(QHBoxLayout* thl) {
    auto* lbl = new QLabel("SERIES");
    lbl->setStyleSheet(
        "color:#525252; font-size:9px; font-weight:700; background:transparent;");

    series_combo_ = new QComboBox;
    for (const auto& s : kFedReserveSeries)
        series_combo_->addItem(s.label, s.command);
    series_combo_->setFixedHeight(26);
    series_combo_->setMinimumWidth(240);

    thl->addWidget(lbl);
    thl->addWidget(series_combo_);
}

void FederalReservePanel::on_fetch() {
    const int   idx    = series_combo_->currentIndex();
    const auto& series = kFedReserveSeries[idx];

    show_loading("Fetching Federal Reserve: " + series.label + "…");
    services::EconomicsService::instance().execute(
        kFederalReserveSourceId, kFederalReserveScript, series.command, series.args,
        "fed_" + series.command);
}

void FederalReservePanel::on_result(const QString& request_id,
                                     const services::EconomicsResult& result) {
    if (result.source_id != kFederalReserveSourceId) return;
    if (!request_id.startsWith("fed_")) return;
    if (!result.success) { show_error(result.error); return; }

    const QString inline_err = result.data["error"].toString();
    if (!inline_err.isEmpty()) { show_error(inline_err); return; }

    // Response: { success, endpoint, data:[{date, <value cols>}] }
    QJsonArray rows = result.data["data"].toArray();

    // Some endpoints nest differently — handle yield_curve which may use "rates" key
    if (rows.isEmpty())
        rows = result.data["rates"].toArray();

    // Filter: keep rows that have at least one non-empty numeric-ish value beyond "date"
    QJsonArray clean;
    for (const auto& rv : rows) {
        const QJsonObject r = rv.toObject();
        bool has_val = false;
        for (auto it = r.begin(); it != r.end(); ++it) {
            if (it.key() == "date") continue;
            if (it.value().isDouble() || !it.value().toString().isEmpty()) {
                has_val = true;
                break;
            }
        }
        if (has_val) clean.append(r);
    }

    if (clean.isEmpty()) {
        show_error("No data returned");
        return;
    }

    const int idx = series_combo_->currentIndex();
    const QString title = "Federal Reserve: "
        + (idx >= 0 && idx < kFedReserveSeries.size() ? kFedReserveSeries[idx].label
                                              : request_id.mid(4));

    display(clean, title);
    LOG_INFO("FederalReservePanel",
             QString("Displayed %1 rows: %2").arg(clean.size()).arg(title));
}

} // namespace fincept::screens
