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
namespace {

static constexpr const char* kOecdScript = "oecd_data.py";
static constexpr const char* kOecdSourceId = "oecd";
static constexpr const char* kOecdColor = "#F59E0B"; // amber

static const QList<QPair<QString, QString>> kOecdDatasets = {
    {"GDP (Real)", "gdp_real"},       {"CPI / Inflation", "cpi"},           {"GDP Forecast", "gdp_forecast"},
    {"Unemployment", "unemployment"}, {"Interest Rates", "interest_rates"}, {"Trade Balance", "trade_balance"},
};

static const QList<QPair<QString, QString>> kOecdCountries = {
    {"United States", "US"},  {"Germany", "DE"}, {"Japan", "JP"},     {"France", "FR"},
    {"United Kingdom", "GB"}, {"Canada", "CA"},  {"Australia", "AU"}, {"South Korea", "KR"},
    {"Italy", "IT"},          {"Spain", "ES"},   {"G7", "G-7"},       {"OECD Total", "OECD"},
};

} // namespace

OecdPanel::OecdPanel(QWidget* parent) : EconPanelBase(kOecdSourceId, kOecdColor, parent) {
    build_base_ui(this);
    connect(&services::EconomicsService::instance(), &services::EconomicsService::result_ready, this,
            &OecdPanel::on_result);
}

void OecdPanel::activate() {
    show_empty(tr("Select a dataset and country, then click FETCH"));
}

void OecdPanel::build_controls(QHBoxLayout* thl) {
    auto lbl = [](const QString& t) {
        auto* l = new QLabel(t);
        l->setStyleSheet(ctrl_label_style());
        return l;
    };

    dataset_combo_ = new QComboBox;
    for (const auto& p : kOecdDatasets)
        dataset_combo_->addItem(p.first, p.second);
    dataset_combo_->setFixedHeight(26);

    country_combo_ = new QComboBox;
    for (const auto& p : kOecdCountries)
        country_combo_->addItem(p.first, p.second);
    country_combo_->setFixedHeight(26);

    frequency_combo_ = new QComboBox;
    frequency_combo_->addItem(tr("Annual"), "A");
    frequency_combo_->addItem(tr("Quarterly"), "Q");
    frequency_combo_->addItem(tr("Monthly"), "M");
    frequency_combo_->setFixedHeight(26);

    thl->addWidget(dataset_lbl_ = lbl(tr("DATASET")));
    thl->addWidget(dataset_combo_);
    thl->addWidget(country_lbl_ = lbl(tr("COUNTRY")));
    thl->addWidget(country_combo_);
    thl->addWidget(freq_lbl_ = lbl(tr("FREQ")));
    thl->addWidget(frequency_combo_);
}

void OecdPanel::on_fetch() {
    const QString cmd = dataset_combo_->currentData().toString();
    const QString country = country_combo_->currentData().toString();
    const QString freq = frequency_combo_->currentData().toString();

    show_loading(tr("Fetching OECD data…"));
    services::EconomicsService::instance().execute(kOecdSourceId, kOecdScript, cmd, {country, freq},
                                                   "oecd_" + cmd + "_" + country);
}

void OecdPanel::on_result(const QString& request_id, const services::EconomicsResult& result) {
    if (result.source_id != kOecdSourceId)
        return;
    if (!result.success) {
        show_error(result.error);
        return;
    }
    if (request_id.startsWith("oecd_")) {
        const QJsonArray arr = result.data["data"].toArray();
        const QString title = dataset_combo_->currentText() + " — " + country_combo_->currentText();
        display(arr, title);
        LOG_INFO("OecdPanel", QString("Displayed %1 rows").arg(arr.size()));
    }
}

// ── i18n ──────────────────────────────────────────────────────────────────────

void OecdPanel::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange)
        retranslateUi();
    EconPanelBase::changeEvent(event);
}

void OecdPanel::retranslateUi() {
    if (dataset_lbl_)
        dataset_lbl_->setText(tr("DATASET"));
    if (country_lbl_)
        country_lbl_->setText(tr("COUNTRY"));
    if (freq_lbl_)
        freq_lbl_->setText(tr("FREQ"));
    if (frequency_combo_) {
        frequency_combo_->setItemText(0, tr("Annual"));
        frequency_combo_->setItemText(1, tr("Quarterly"));
        frequency_combo_->setItemText(2, tr("Monthly"));
    }
    EconPanelBase::retranslateUi();
}

} // namespace fincept::screens
