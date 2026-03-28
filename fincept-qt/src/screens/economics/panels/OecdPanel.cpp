// src/screens/economics/panels/OecdPanel.cpp
#include "screens/economics/panels/OecdPanel.h"

#include "core/logging/Logger.h"
#include "services/economics/EconomicsService.h"

#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonObject>
#include <QLabel>
#include <QVBoxLayout>

namespace fincept::screens {

static constexpr const char* kScript   = "oecd_data.py";
static constexpr const char* kSourceId = "oecd";
static constexpr const char* kColor    = "#F59E0B";  // amber

static const QList<QPair<QString,QString>> kDatasets = {
    {"GDP (Real)",          "gdp_real"},
    {"CPI / Inflation",     "cpi"},
    {"GDP Forecast",        "gdp_forecast"},
    {"Unemployment",        "unemployment"},
    {"Interest Rates",      "interest_rates"},
    {"Trade Balance",       "trade_balance"},
};

static const QList<QPair<QString,QString>> kCountries = {
    {"United States", "US"},
    {"Germany",       "DE"},
    {"Japan",         "JP"},
    {"France",        "FR"},
    {"United Kingdom","GB"},
    {"Canada",        "CA"},
    {"Australia",     "AU"},
    {"South Korea",   "KR"},
    {"Italy",         "IT"},
    {"Spain",         "ES"},
    {"G7",            "G-7"},
    {"OECD Total",    "OECD"},
};

OecdPanel::OecdPanel(QWidget* parent)
    : EconPanelBase(kSourceId, kColor, parent) {
    build_base_ui(this);
    connect(&services::EconomicsService::instance(),
            &services::EconomicsService::result_ready,
            this, &OecdPanel::on_result);
}

void OecdPanel::activate() {
    show_empty("Select a dataset and country, then click FETCH");
}

void OecdPanel::build_controls(QHBoxLayout* thl) {
    auto lbl = [](const QString& t) {
        auto* l = new QLabel(t);
        l->setStyleSheet(
            "color:#525252; font-size:9px; font-weight:700; background:transparent;");
        return l;
    };

    dataset_combo_ = new QComboBox;
    for (const auto& p : kDatasets) dataset_combo_->addItem(p.first, p.second);
    dataset_combo_->setFixedHeight(26);

    country_combo_ = new QComboBox;
    for (const auto& p : kCountries) country_combo_->addItem(p.first, p.second);
    country_combo_->setFixedHeight(26);

    frequency_combo_ = new QComboBox;
    frequency_combo_->addItem("Annual",    "A");
    frequency_combo_->addItem("Quarterly", "Q");
    frequency_combo_->addItem("Monthly",   "M");
    frequency_combo_->setFixedHeight(26);

    thl->addWidget(lbl("DATASET"));
    thl->addWidget(dataset_combo_);
    thl->addWidget(lbl("COUNTRY"));
    thl->addWidget(country_combo_);
    thl->addWidget(lbl("FREQ"));
    thl->addWidget(frequency_combo_);
}

void OecdPanel::on_fetch() {
    const QString cmd     = dataset_combo_->currentData().toString();
    const QString country = country_combo_->currentData().toString();
    const QString freq    = frequency_combo_->currentData().toString();

    show_loading("Fetching OECD data…");
    services::EconomicsService::instance().execute(
        kSourceId, kScript, cmd,
        {country, freq},
        "oecd_" + cmd + "_" + country);
}

void OecdPanel::on_result(const QString& request_id,
                          const services::EconomicsResult& result) {
    if (result.source_id != kSourceId) return;
    if (!result.success) { show_error(result.error); return; }
    if (request_id.startsWith("oecd_")) {
        const QJsonArray arr = result.data["data"].toArray();
        const QString title  = dataset_combo_->currentText() +
                               " — " + country_combo_->currentText();
        display(arr, title);
        LOG_INFO("OecdPanel", QString("Displayed %1 rows").arg(arr.size()));
    }
}

} // namespace fincept::screens
