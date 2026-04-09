// src/screens/economics/panels/WorldBankHealthPanel.cpp
// World Bank health & development indicators panel.
//
// Response shape:
//   { indicator, country, total, records: [
//       { date:"2023", value:78.3, indicator:{id,value}, country:{id,value},
//         countryiso3code, unit, obs_status, decimal }
//   ]}
// Flatten: filter null values, extract date+value, sort oldest→newest.
#include "screens/economics/panels/WorldBankHealthPanel.h"

#include "core/logging/Logger.h"
#include "services/economics/EconomicsService.h"

#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonObject>
#include <QLabel>

namespace fincept::screens {
namespace {

static constexpr const char* kWorldBankHealthScript   = "worldbank_health_data.py";
static constexpr const char* kWorldBankHealthSourceId = "wb_health";
static constexpr const char* kWorldBankHealthColor    = "#16A34A";  // green
} // namespace

struct WbHealthIndicator {
    QString label;
    QString command;
    QString unit;
};

static const QList<WbHealthIndicator> kWbHealthIndicators = {
    { "Life Expectancy",                "life_expectancy",  "years"     },
    { "Infant Mortality Rate",          "infant_mortality", "per 1,000" },
    { "Literacy Rate (Adult)",          "literacy",         "%"         },
    { "Gini Index (Inequality)",        "gini",             "index"     },
    { "Human Development Index",        "hdi",              "index"     },
    { "Poverty Rate ($2.15/day)",       "poverty",          "%"         },
};

static const QList<QPair<QString,QString>> kWbHealthCountries = {
    { "United States",    "US" }, { "China",          "CN" }, { "Germany",       "DE" },
    { "Japan",            "JP" }, { "United Kingdom", "GB" }, { "France",        "FR" },
    { "India",            "IN" }, { "Brazil",         "BR" }, { "Canada",        "CA" },
    { "South Korea",      "KR" }, { "Australia",      "AU" }, { "Russia",        "RU" },
    { "Mexico",           "MX" }, { "Indonesia",      "ID" }, { "Spain",         "ES" },
    { "Italy",            "IT" }, { "Netherlands",    "NL" }, { "Turkey",        "TR" },
    { "Saudi Arabia",     "SA" }, { "South Africa",   "ZA" }, { "Nigeria",       "NG" },
    { "World",            "WLD"},
};

QJsonArray WorldBankHealthPanel::flatten_wb(const QJsonObject& response) {
    const QJsonArray records = response["records"].toArray();
    if (records.isEmpty()) return {};

    QJsonArray rows;
    for (const auto& rv : records) {
        const QJsonObject r = rv.toObject();
        if (r["value"].isNull() || r["value"].isUndefined()) continue;
        const double val = r["value"].toDouble();
        // Skip placeholder zeros for HDI/similar where 0 is not meaningful
        QJsonObject row;
        row["date"]  = r["date"].toString();
        row["value"] = val;
        rows.append(row);
    }

    // Sort oldest→newest — copy to QList to avoid QJsonValueRef swap issue on MSVC
    QList<QJsonValue> sorted(rows.begin(), rows.end());
    std::sort(sorted.begin(), sorted.end(), [](const QJsonValue& a, const QJsonValue& b) {
        return a.toObject()["date"].toString() < b.toObject()["date"].toString();
    });
    rows = QJsonArray();
    for (const auto& v : sorted) rows.append(v);

    return rows;
}

WorldBankHealthPanel::WorldBankHealthPanel(QWidget* parent)
    : EconPanelBase(kWorldBankHealthSourceId, kWorldBankHealthColor, parent) {
    build_base_ui(this);
    connect(&services::EconomicsService::instance(),
            &services::EconomicsService::result_ready,
            this, &WorldBankHealthPanel::on_result);
}

void WorldBankHealthPanel::activate() {
    show_empty("Select an indicator and country, then click FETCH\n"
               "Source: World Bank — Health & Development indicators");
}

void WorldBankHealthPanel::build_controls(QHBoxLayout* thl) {
    auto lbl = [this](const QString& t) {
        auto* l = new QLabel(t);
        l->setStyleSheet(ctrl_label_style());
        return l;
    };

    indicator_combo_ = new QComboBox;
    for (const auto& ind : kWbHealthIndicators)
        indicator_combo_->addItem(ind.label, ind.command);
    indicator_combo_->setFixedHeight(26);
    indicator_combo_->setMinimumWidth(200);

    country_combo_ = new QComboBox;
    for (const auto& c : kWbHealthCountries)
        country_combo_->addItem(c.first, c.second);
    country_combo_->setFixedHeight(26);
    country_combo_->setMinimumWidth(130);

    thl->addWidget(lbl("INDICATOR"));
    thl->addWidget(indicator_combo_);
    thl->addWidget(lbl("COUNTRY"));
    thl->addWidget(country_combo_);
}

void WorldBankHealthPanel::on_fetch() {
    const QString command = indicator_combo_->currentData().toString();
    const QString country = country_combo_->currentData().toString();

    show_loading("Fetching WB Health: " + indicator_combo_->currentText()
                 + " — " + country_combo_->currentText() + "…");

    services::EconomicsService::instance().execute(
        kWorldBankHealthSourceId, kWorldBankHealthScript, command,
        { country },
        "wbhealth_" + command + "_" + country);
}

void WorldBankHealthPanel::on_result(const QString& request_id,
                                      const services::EconomicsResult& result) {
    if (result.source_id != kWorldBankHealthSourceId) return;
    if (!request_id.startsWith("wbhealth_")) return;
    if (!result.success) { show_error(result.error); return; }

    QJsonArray rows = flatten_wb(result.data);

    if (rows.isEmpty())
        rows = result.data["data"].toArray();

    if (rows.isEmpty()) {
        show_error("No data available for this selection");
        return;
    }

    const int idx = indicator_combo_->currentIndex();
    const QString unit = (idx >= 0 && idx < kWbHealthIndicators.size())
                             ? " (" + kWbHealthIndicators[idx].unit + ")"
                             : "";
    const QString title = "WB Health: " + indicator_combo_->currentText()
                        + unit + " — " + country_combo_->currentText();

    display(rows, title);
    LOG_INFO("WorldBankHealthPanel", QString("Displayed %1 records: %2")
                                         .arg(rows.size()).arg(title));
}

} // namespace fincept::screens
