// src/screens/economics/panels/EcbPanel.cpp
// ECB SDMX data panel.
// Response shape: {<key>: [{dimensions:{...}, observations:[{period,value}], obs_count}], ...}
// Working commands: exchange_rates, inflation, money_supply
#include "screens/economics/panels/EcbPanel.h"

#include "core/logging/Logger.h"
#include "services/economics/EconomicsService.h"

#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonObject>
#include <QLabel>

namespace fincept::screens {

static constexpr const char* kScript   = "ecb_sdmx_data.py";
static constexpr const char* kSourceId = "ecb";
static constexpr const char* kColor    = "#2563EB";  // ECB blue

// Series: display name -> {command, arg (optional currency)}
struct EcbSeries {
    QString label;
    QString command;
    QString arg;       // currency for exchange_rates, empty otherwise
    QString result_key; // top-level JSON key in response
};

static const QList<EcbSeries> kSeries = {
    { "EUR/USD Exchange Rate",   "exchange_rates", "USD",  "exchange_rates" },
    { "EUR/GBP Exchange Rate",   "exchange_rates", "GBP",  "exchange_rates" },
    { "EUR/JPY Exchange Rate",   "exchange_rates", "JPY",  "exchange_rates" },
    { "EUR/CHF Exchange Rate",   "exchange_rates", "CHF",  "exchange_rates" },
    { "EUR/CNY Exchange Rate",   "exchange_rates", "CNY",  "exchange_rates" },
    { "Eurozone HICP Inflation", "inflation",      "",     "inflation"      },
    { "M3 Money Supply",         "money_supply",   "",     "money_supply"   },
};

EcbPanel::EcbPanel(QWidget* parent)
    : EconPanelBase(kSourceId, kColor, parent) {
    build_base_ui(this);
    connect(&services::EconomicsService::instance(),
            &services::EconomicsService::result_ready,
            this, &EcbPanel::on_result);
}

void EcbPanel::activate() {
    show_empty("Select a series and click FETCH\n"
               "Source: European Central Bank SDMX 2.1 API");
}

void EcbPanel::build_controls(QHBoxLayout* thl) {
    auto lbl = [](const QString& t) {
        auto* l = new QLabel(t);
        l->setStyleSheet(
            "color:#525252; font-size:9px; font-weight:700; background:transparent;");
        return l;
    };

    series_combo_ = new QComboBox;
    for (const auto& s : kSeries)
        series_combo_->addItem(s.label, s.command + "|" + s.arg);
    series_combo_->setFixedHeight(26);
    series_combo_->setMinimumWidth(220);

    thl->addWidget(lbl("SERIES"));
    thl->addWidget(series_combo_);
}

void EcbPanel::on_fetch() {
    const QString data   = series_combo_->currentData().toString();
    const int     idx    = series_combo_->currentIndex();
    const auto&   series = kSeries[idx];

    show_loading("Fetching ECB data: " + series.label + "…");

    QStringList args = { series.command };
    if (!series.arg.isEmpty()) args << series.arg;

    const QString req_id = "ecb_" + series.command
                         + (series.arg.isEmpty() ? "" : "_" + series.arg);

    services::EconomicsService::instance().execute(
        kSourceId, kScript, series.command, args, req_id);
}

// ECB response: {<result_key>: [{dimensions:{}, observations:[{period,value}], obs_count}], ...}
// Flatten the first series' observations into a plain [{period, value}] array for display().
void EcbPanel::on_result(const QString& request_id,
                         const services::EconomicsResult& result) {
    if (result.source_id != kSourceId) return;
    if (!result.success) { show_error(result.error); return; }
    if (!request_id.startsWith("ecb_")) return;

    // Find which series was fetched
    const int idx = series_combo_->currentIndex();
    if (idx < 0 || idx >= kSeries.size()) return;
    const auto& series = kSeries[idx];

    // Extract observations from first element of the result array
    const QJsonValue top = result.data[series.result_key];
    QJsonArray obs;

    if (top.isArray()) {
        const QJsonArray arr = top.toArray();
        if (!arr.isEmpty()) {
            obs = arr[0].toObject()["observations"].toArray();
        }
    } else if (top.isObject()) {
        // Fallback: result wrapped as object
        obs = top.toObject()["observations"].toArray();
    }

    // Also accept flat data array from service normalisation
    if (obs.isEmpty()) {
        obs = result.data["data"].toArray();
    }

    if (obs.isEmpty()) {
        show_error("No observations returned for " + series.label);
        return;
    }

    display(obs, "ECB: " + series.label);
    LOG_INFO("EcbPanel", QString("Displayed %1 observations for %2")
                             .arg(obs.size()).arg(series.label));
}

} // namespace fincept::screens
