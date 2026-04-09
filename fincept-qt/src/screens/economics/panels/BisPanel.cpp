// src/screens/economics/panels/BisPanel.cpp
// BIS SDMX API — 13 statistical domains, no API key required.
// Commands: get_central_bank_policy_rates, get_effective_exchange_rates,
//           get_exchange_rates, get_long_term_interest_rates,
//           get_short_term_interest_rates, get_credit_to_non_financial_sector,
//           get_house_prices, get_economic_overview
// Response: { "success": true, "data": [...], "metadata": {...} }
// data[] rows: { "date": "YYYY-MM", "value": 1.23, "country": "US", "series_key": "..." }
#include "screens/economics/panels/BisPanel.h"

#include "core/logging/Logger.h"
#include "services/economics/EconomicsService.h"

#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonObject>
#include <QLabel>
#include <QVBoxLayout>

namespace fincept::screens {
namespace {

static constexpr const char* kBisScript   = "bis_data.py";
static constexpr const char* kBisSourceId = "bis";
static constexpr const char* kBisColor    = "#9D4EDD";  // purple
} // namespace

// Dataset combo entries: { display label, CLI command, placeholder country hint }
struct BisDataset {
    QString label;
    QString command;
    QString country_hint;
    QString default_country;
};

static const QList<BisDataset> kBisDatasets = {
    {"Central Bank Policy Rates",        "get_central_bank_policy_rates", "e.g. US, GB, JP, DE", "US"},
    {"Effective Exchange Rates",         "get_effective_exchange_rates",  "e.g. US, GB, JP",     "US"},
    {"Exchange Rates vs USD",            "get_exchange_rates",            "e.g. GB, JP, DE, AU", "GB"},
    {"Long-term Interest Rates",         "get_long_term_interest_rates",  "e.g. US, DE, JP, GB", "US"},
    {"Short-term Interest Rates",        "get_short_term_interest_rates", "e.g. US, DE, JP, GB", "US"},
    {"Credit to Non-Financial Sector",   "get_credit_to_non_financial_sector", "e.g. US, CN, JP", "US"},
    {"House Prices",                     "get_house_prices",              "e.g. US, GB, DE, AU", "US"},
    {"Economic Overview (multi-series)", "get_economic_overview",         "e.g. US",             "US"},
};

// ── Constructor ───────────────────────────────────────────────────────────────

BisPanel::BisPanel(QWidget* parent)
    : EconPanelBase(kBisSourceId, kBisColor, parent) {
    build_base_ui(this);
    connect(&services::EconomicsService::instance(),
            &services::EconomicsService::result_ready,
            this, &BisPanel::on_result);
}

void BisPanel::activate() {
    show_empty("Select a dataset and country code, then click FETCH\n"
               "BIS data is free — no API key required");
}

// ── Controls ──────────────────────────────────────────────────────────────────

void BisPanel::build_controls(QHBoxLayout* thl) {
    auto make_lbl = [this](const QString& text) {
        auto* l = new QLabel(text);
        l->setStyleSheet(ctrl_label_style());
        return l;
    };

    dataset_combo_ = new QComboBox;
    for (const auto& d : kBisDatasets)
        dataset_combo_->addItem(d.label);
    dataset_combo_->setFixedHeight(26);
    dataset_combo_->setMinimumWidth(230);
    connect(dataset_combo_, &QComboBox::currentIndexChanged,
            this, &BisPanel::on_dataset_changed);

    country_input_ = new QLineEdit;
    country_input_->setPlaceholderText("Country code");
    country_input_->setText("US");
    country_input_->setFixedHeight(26);
    country_input_->setFixedWidth(70);

    start_input_ = new QLineEdit;
    start_input_->setPlaceholderText("Start year");
    start_input_->setFixedHeight(26);
    start_input_->setFixedWidth(70);

    end_input_ = new QLineEdit;
    end_input_->setPlaceholderText("End year");
    end_input_->setFixedHeight(26);
    end_input_->setFixedWidth(70);

    thl->addWidget(make_lbl("DATASET"));
    thl->addWidget(dataset_combo_);
    thl->addWidget(make_lbl("COUNTRY"));
    thl->addWidget(country_input_);
    thl->addWidget(make_lbl("FROM"));
    thl->addWidget(start_input_);
    thl->addWidget(make_lbl("TO"));
    thl->addWidget(end_input_);
}

void BisPanel::on_dataset_changed(int index) {
    if (index < 0 || index >= kBisDatasets.size()) return;
    const auto& ds = kBisDatasets[index];
    country_input_->setPlaceholderText(ds.country_hint);
    // If user hasn't typed anything custom, fill in default
    if (country_input_->text().isEmpty() ||
        country_input_->text() == kBisDatasets[qMax(0, index - 1)].default_country) {
        country_input_->setText(ds.default_country);
    }
    // Economic overview doesn't use a country filter
    country_input_->setEnabled(ds.command != "get_economic_overview");
}

// ── Fetch ─────────────────────────────────────────────────────────────────────

void BisPanel::on_fetch() {
    const int idx = dataset_combo_->currentIndex();
    if (idx < 0 || idx >= kBisDatasets.size()) return;
    const auto& ds = kBisDatasets[idx];

    QStringList args;
    const QString country = country_input_->text().trimmed().toUpper();
    const QString start   = start_input_->text().trimmed();
    const QString end     = end_input_->text().trimmed();

    // economic_overview doesn't take country/date args
    if (ds.command != "get_economic_overview") {
        if (country.isEmpty()) {
            show_empty("Enter a country code (e.g. US, GB, JP)");
            return;
        }
        args << country;
        if (!start.isEmpty()) args << start;
        if (!start.isEmpty() && !end.isEmpty()) args << end;
    }

    show_loading("Fetching BIS " + ds.label + "…");
    services::EconomicsService::instance().execute(
        kBisSourceId, kBisScript, ds.command, args,
        "bis_" + ds.command + "_" + country);
}

// ── Result ────────────────────────────────────────────────────────────────────

void BisPanel::on_result(const QString& request_id,
                         const services::EconomicsResult& result) {
    if (result.source_id != kBisSourceId) return;

    if (!result.success) {
        show_error(result.error);
        return;
    }

    // BIS response: { "success": true, "data": [...], "metadata": {...} }
    // data[] rows: { "date": "YYYY-MM", "value": 1.23, "country": "US", "series_key": "..." }
    // economic_overview may return nested: { "data": { "exchange_rates": [...], ... } }
    QJsonArray rows = result.data["data"].toArray();

    if (rows.isEmpty()) {
        // Try nested overview structure — flatten all sub-arrays into one
        const QJsonObject data_obj = result.data["data"].toObject();
        if (!data_obj.isEmpty()) {
            for (const auto& key : data_obj.keys()) {
                const QJsonArray sub = data_obj[key].toArray();
                for (const auto& v : sub) rows.append(v);
            }
        }
    }

    if (rows.isEmpty()) {
        show_empty("No data returned — try a different country or date range");
        return;
    }

    // Build title from metadata
    const QString title = "BIS: " + dataset_combo_->currentText()
                        + " — " + country_input_->text().trimmed().toUpper();

    display(rows, title);
    LOG_INFO("BisPanel", QString("Displayed %1 rows for %2")
             .arg(rows.size()).arg(request_id));
}

} // namespace fincept::screens
