// src/screens/economics/panels/BlsPanel.cpp
// BLS data panel. Requires BLS_API_KEY environment variable.
//
// Response shape (series overview commands):
//   { series: [{series_id, title, data:[{year,period,value,...}]}] }
// or for overview commands:
//   { series_data: [...] } / { data: [...] }
//
// Error shape when no key:
//   { error: true, message: "BLS API key required...", endpoint, status_code, timestamp }
#include "screens/economics/panels/BlsPanel.h"

#include "core/logging/Logger.h"
#include "services/economics/EconomicsService.h"

#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonObject>
#include <QLabel>

namespace fincept::screens {
namespace {

static constexpr const char* kBlsScript   = "bls_data.py";
static constexpr const char* kBlsSourceId = "bls";
static constexpr const char* kBlsColor    = "#DC2626";  // BLS red

struct BlsPreset {
    QString label;
    QString command;
    QString series_id;  // non-empty -> use get_series command
};

static const QList<BlsPreset> kBlsPresets = {
    { "-- Select a series --",             "",                      ""             },
    { "Total Nonfarm Employment",          "get_series",            "CES0000000001"},
    { "Unemployment Rate (U-3)",           "get_series",            "LNS14000000"  },
    { "CPI All Urban Consumers",           "get_series",            "CUUR0000SA0"  },
    { "CPI Less Food & Energy (Core)",     "get_series",            "CUUR0000SA0L1E"},
    { "PPI Final Demand",                  "get_series",            "WPUFD4"       },
    { "Average Hourly Earnings",           "get_series",            "CES0500000003"},
    { "Labor Force Participation Rate",    "get_series",            "LNS11300000"  },
    { "Job Openings (JOLTS)",              "get_series",            "JTS000000000000000JOL"},
    { "Labor Market Overview",             "get_labor_overview",    ""             },
    { "Inflation Overview",                "get_inflation_overview",""},
    { "Employment Cost Index",             "get_employment_cost_index",""},
    { "Productivity & Costs",              "get_productivity_costs",""},
};

} // namespace

BlsPanel::BlsPanel(QWidget* parent)
    : EconPanelBase(kBlsSourceId, kBlsColor, parent) {
    build_base_ui(this);
    connect(&services::EconomicsService::instance(),
            &services::EconomicsService::result_ready,
            this, &BlsPanel::on_result);
}

void BlsPanel::activate() {
    show_empty("Set BLS_API_KEY environment variable, then select a series and click FETCH\n"
               "Get a free key at: data.bls.gov/registrationEngine/");
}

void BlsPanel::build_controls(QHBoxLayout* thl) {
    auto lbl = [](const QString& t) {
        auto* l = new QLabel(t);
        l->setStyleSheet(
            "color:#525252; font-size:9px; font-weight:700; background:transparent;");
        return l;
    };

    preset_combo_ = new QComboBox;
    for (const auto& p : kBlsPresets)
        preset_combo_->addItem(p.label, p.command + "|" + p.series_id);
    preset_combo_->setFixedHeight(26);
    preset_combo_->setMinimumWidth(240);

    connect(preset_combo_, &QComboBox::currentIndexChanged, this,
            [this](int idx) {
                if (idx > 0 && idx < kBlsPresets.size()) {
                    const auto& p = kBlsPresets[idx];
                    if (!p.series_id.isEmpty())
                        series_input_->setText(p.series_id);
                }
            });

    auto* lbl2 = new QLabel("SERIES ID");
    lbl2->setStyleSheet(
        "color:#525252; font-size:9px; font-weight:700; background:transparent;");

    series_input_ = new QLineEdit;
    series_input_->setPlaceholderText("e.g. CES0000000001");
    series_input_->setFixedHeight(26);
    series_input_->setFixedWidth(140);

    thl->addWidget(lbl("PRESET"));
    thl->addWidget(preset_combo_);
    thl->addWidget(lbl2);
    thl->addWidget(series_input_);
}

void BlsPanel::on_fetch() {
    const int idx = preset_combo_->currentIndex();
    if (idx <= 0) {
        // Check manual series ID
        const QString sid = series_input_->text().trimmed().toUpper();
        if (sid.isEmpty()) { show_empty("Select a preset or enter a series ID"); return; }
        show_loading("Fetching BLS series " + sid + "…");
        services::EconomicsService::instance().execute(
            kBlsSourceId, kBlsScript, "get_series", {sid}, "bls_series_" + sid);
        return;
    }

    const auto& preset = kBlsPresets[idx];
    show_loading("Fetching BLS: " + preset.label + "…");

    if (preset.command == "get_series") {
        const QString sid = series_input_->text().trimmed().toUpper();
        const QString use_sid = sid.isEmpty() ? preset.series_id : sid;
        services::EconomicsService::instance().execute(
            kBlsSourceId, kBlsScript, "get_series", {use_sid},
            "bls_series_" + use_sid);
    } else {
        services::EconomicsService::instance().execute(
            kBlsSourceId, kBlsScript, preset.command, {},
            "bls_" + preset.command);
    }
}

// Normalize various BLS response shapes into a flat QJsonArray for display().
static QJsonArray extract_bls_rows(const QJsonObject& data) {
    // Shape 1: {series:[{series_id, title, data:[{year,period,value}]}]}
    const QJsonArray series_arr = data["series"].toArray();
    if (!series_arr.isEmpty()) {
        QJsonArray rows;
        for (const auto& sv : series_arr) {
            const QJsonObject s = sv.toObject();
            const QString title = s["title"].toString(s["series_id"].toString());
            for (const auto& dv : s["data"].toArray()) {
                QJsonObject row = dv.toObject();
                row["series"] = title;
                // Combine year+period into a date string
                const QString year   = row["year"].toString();
                const QString period = row["period"].toString();  // e.g. "M01"
                if (!year.isEmpty() && !period.isEmpty() && period != "M13") {
                    const QString month = period.mid(1).rightJustified(2, '0');
                    row["date"] = year + "-" + month;
                }
                rows.append(row);
            }
        }
        return rows;
    }

    // Shape 2: {series_data:[...]} or {data:[...]}
    QJsonArray fallback = data["series_data"].toArray();
    if (fallback.isEmpty()) fallback = data["data"].toArray();
    return fallback;
}

void BlsPanel::on_result(const QString& request_id,
                         const services::EconomicsResult& result) {
    if (result.source_id != kBlsSourceId) return;
    if (!request_id.startsWith("bls_")) return;

    if (!result.success) {
        const QString msg = result.error;
        if (msg.contains("API key") || msg.contains("api_key") ||
            msg.contains("BLS_API_KEY")) {
            show_error("BLS API key not configured.\n"
                       "Set BLS_API_KEY environment variable.\n"
                       "Free key at: data.bls.gov/registrationEngine/");
        } else {
            show_error(msg);
        }
        return;
    }

    // Also check for error object inside successful result
    if (result.data["error"].toBool()) {
        const QString msg = result.data["message"].toString(result.data["error"].toString());
        if (msg.contains("API key") || msg.contains("BLS_API_KEY")) {
            show_error("BLS API key not configured.\n"
                       "Set BLS_API_KEY environment variable.\n"
                       "Free key at: data.bls.gov/registrationEngine/");
        } else {
            show_error(msg);
        }
        return;
    }

    const QJsonArray rows = extract_bls_rows(result.data);
    if (rows.isEmpty()) {
        show_error("No data returned");
        return;
    }

    const int idx = preset_combo_->currentIndex();
    const QString title = (idx > 0 && idx < kBlsPresets.size())
                              ? "BLS: " + kBlsPresets[idx].label
                              : "BLS: " + series_input_->text().trimmed().toUpper();
    display(rows, title);
    LOG_INFO("BlsPanel", QString("Displayed %1 rows").arg(rows.size()));
}

} // namespace fincept::screens
