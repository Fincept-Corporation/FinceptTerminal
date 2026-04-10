// src/screens/economics/panels/ImfPanel.cpp
// IMF DataMapper panel — nested pivot {values.INDICATOR.COUNTRY.YEAR}
#include "screens/economics/panels/ImfPanel.h"

#include "core/logging/Logger.h"
#include "services/economics/EconomicsService.h"

#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonObject>
#include <QLabel>
#include <QVBoxLayout>

namespace fincept::screens {
namespace {

static constexpr const char* kImfScript = "imf_datamapper_data.py";
static constexpr const char* kImfSourceId = "imf";
static constexpr const char* kImfColor = "#3B82F6"; // blue

// Curated indicator list: display name → IMF DataMapper code
static const QList<QPair<QString, QString>> kImfIndicators = {
    {"Real GDP Growth (%)", "NGDP_RPCH"},
    {"Nominal GDP (USD billions)", "NGDPD"},
    {"GDP per capita (USD)", "NGDPDPC"},
    {"Inflation, End of Period (%)", "PCPIE"},
    {"Inflation, Average (%)", "PCPIPCH"},
    {"Unemployment Rate (%)", "LUR"},
    {"Current Account Balance (% GDP)", "BCA_NGDPD"},
    {"General Gov. Gross Debt (% GDP)", "GGXWDG_NGDP"},
    {"General Gov. Net Lending (% GDP)", "GGXCNL_NGDP"},
    {"General Gov. Revenue (% GDP)", "GGR_NGDP"},
    {"Total Investment (% GDP)", "NID_NGDP"},
    {"Gross National Savings (% GDP)", "NGSD_NGDP"},
    {"Volume of Imports of Goods (% change)", "TM_RPCH"},
    {"Volume of Exports of Goods (% change)", "TX_RPCH"},
    {"Commodity Terms of Trade", "TT"},
    {"Population (millions)", "LP"},
};

// Curated country list
static const QList<QPair<QString, QString>> kImfCountries = {
    {"All Countries", ""}, {"United States", "USA"},  {"China", "CHN"},       {"Japan", "JPN"},
    {"Germany", "DEU"},    {"United Kingdom", "GBR"}, {"France", "FRA"},      {"India", "IND"},
    {"Brazil", "BRA"},     {"Canada", "CAN"},         {"South Korea", "KOR"}, {"Australia", "AUS"},
    {"Russia", "RUS"},     {"Mexico", "MEX"},         {"Indonesia", "IDN"},   {"Saudi Arabia", "SAU"},
    {"Turkey", "TUR"},     {"Netherlands", "NLD"},    {"Switzerland", "CHE"}, {"Argentina", "ARG"},
};

} // namespace

// ── Constructor ───────────────────────────────────────────────────────────────

ImfPanel::ImfPanel(QWidget* parent) : EconPanelBase(kImfSourceId, kImfColor, parent) {

    auto* main = new QHBoxLayout(this);
    main->setContentsMargins(0, 0, 0, 0);
    main->setSpacing(0);

    // Left: indicator list
    auto* left = new QWidget(this);
    left->setFixedWidth(240);
    left->setStyleSheet(sidebar_style());
    auto* lvl = new QVBoxLayout(left);
    lvl->setContentsMargins(0, 0, 0, 0);
    lvl->setSpacing(0);

    auto* hdr = new QLabel("IMF INDICATOR");
    hdr->setStyleSheet(section_lbl_style() + section_hdr_style());
    lvl->addWidget(hdr);

    indicator_search_ = new QLineEdit;
    indicator_search_->setPlaceholderText("Filter indicators…");
    indicator_search_->setStyleSheet(search_input_style());
    connect(indicator_search_, &QLineEdit::textChanged, this, &ImfPanel::on_indicator_filter);
    lvl->addWidget(indicator_search_);

    indicator_list_ = new QListWidget;
    indicator_list_->setStyleSheet(list_style());
    for (const auto& pair : kImfIndicators) {
        auto* item = new QListWidgetItem(pair.first, indicator_list_);
        item->setData(Qt::UserRole, pair.second);
    }
    indicator_list_->setCurrentRow(0);
    lvl->addWidget(indicator_list_, 1);

    main->addWidget(left);

    auto* right = new QWidget(this);
    main->addWidget(right, 1);
    build_base_ui(right);

    connect(&services::EconomicsService::instance(), &services::EconomicsService::result_ready, this,
            &ImfPanel::on_result);
}

// ── Activate ──────────────────────────────────────────────────────────────────

void ImfPanel::activate() {
    show_empty("Select an indicator and country, then click FETCH");
}

// ── Controls ──────────────────────────────────────────────────────────────────

void ImfPanel::build_controls(QHBoxLayout* thl) {
    auto* lbl = new QLabel("COUNTRY");
    lbl->setStyleSheet(ctrl_label_style());
    country_combo_ = new QComboBox;
    for (const auto& pair : kImfCountries)
        country_combo_->addItem(pair.first, pair.second);
    country_combo_->setFixedHeight(26);
    country_combo_->setMinimumWidth(140);
    thl->addWidget(lbl);
    thl->addWidget(country_combo_);
}

// ── Fetch ─────────────────────────────────────────────────────────────────────

void ImfPanel::on_fetch() {
    auto* ind_item = indicator_list_->currentItem();
    if (!ind_item) {
        show_empty("Select an indicator");
        return;
    }

    const QString code = ind_item->data(Qt::UserRole).toString();
    const QString country = country_combo_->currentData().toString();

    show_loading("Fetching IMF DataMapper data…");
    services::EconomicsService::instance().execute(kImfSourceId, kImfScript, "data", {code},
                                                   "imf_data_" + code + "_" + country);
}

// ── Result ────────────────────────────────────────────────────────────────────

void ImfPanel::on_result(const QString& request_id, const services::EconomicsResult& result) {
    if (result.source_id != kImfSourceId)
        return;
    if (!result.success) {
        show_error(result.error);
        return;
    }

    if (request_id.startsWith("imf_data_")) {
        // Response: {values: {CODE: {COUNTRY: {YEAR: value}}}}
        const QJsonObject values = result.data["values"].toObject();
        if (values.isEmpty()) {
            show_error("No data in response");
            return;
        }

        auto* ind_item = indicator_list_->currentItem();
        const QString code = ind_item ? ind_item->data(Qt::UserRole).toString() : values.keys().first();
        const QString country = country_combo_->currentData().toString();

        const QJsonArray rows = flatten_pivot(values, code, country);
        const QString title = (ind_item ? ind_item->text() : code) +
                              (country.isEmpty() ? " — All Countries" : " — " + country_combo_->currentText());
        display(rows, title);
        LOG_INFO("ImfPanel", QString("Displayed %1 rows").arg(rows.size()));
    }
}

QJsonArray ImfPanel::flatten_pivot(const QJsonObject& values, const QString& indicator_code,
                                   const QString& country_filter) const {
    const QJsonObject by_country = values[indicator_code].toObject();
    QJsonArray rows;

    // Collect all years across all countries for sorting
    // If country_filter is set, only include that country
    const QStringList countries = country_filter.isEmpty() ? by_country.keys() : QStringList{country_filter};

    // Collect all years
    QSet<QString> year_set;
    for (const auto& c : countries) {
        for (const auto& y : by_country[c].toObject().keys())
            year_set.insert(y);
    }
    QStringList years = year_set.values();
    std::sort(years.begin(), years.end());

    if (country_filter.isEmpty()) {
        // Wide format: one row per country, columns = years (last 10)
        const QStringList recent_years = years.mid(qMax(0, years.size() - 10));
        for (const auto& c : countries) {
            const QJsonObject y_map = by_country[c].toObject();
            QJsonObject row;
            row["country"] = c;
            for (const auto& y : recent_years) {
                const QJsonValue v = y_map[y];
                row[y] = v.isNull() ? QJsonValue() : v;
            }
            rows.append(row);
        }
    } else {
        // Long format: one row per year for selected country
        const QJsonObject y_map = by_country[country_filter].toObject();
        for (const auto& y : years) {
            const QJsonValue v = y_map[y];
            if (v.isNull() || v.isUndefined())
                continue;
            QJsonObject row;
            row["year"] = y;
            row["value"] = v;
            rows.append(row);
        }
    }
    return rows;
}

void ImfPanel::on_indicator_filter(const QString& text) {
    filter_list(indicator_list_, text);
}

} // namespace fincept::screens
