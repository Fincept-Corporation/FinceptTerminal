// src/screens/economics/panels/FredPanel.cpp
#include "screens/economics/panels/FredPanel.h"

#include "core/logging/Logger.h"
#include "services/economics/EconomicsService.h"

#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonObject>
#include <QLabel>
#include <QVBoxLayout>

namespace fincept::screens {
namespace {

static constexpr const char* kFredScript   = "fred_data.py";
static constexpr const char* kFredSourceId = "fred";
static constexpr const char* kFredColor    = "#EF4444";  // red
} // namespace

static const QList<QPair<QString,QString>> kFredPresets = {
    {"-- Custom series ID --",                    ""},
    {"Real GDP (GDPC1)",                          "GDPC1"},
    {"Nominal GDP (GDP)",                         "GDP"},
    {"CPI All Items (CPIAUCSL)",                  "CPIAUCSL"},
    {"Core CPI (CPILFESL)",                       "CPILFESL"},
    {"Unemployment Rate (UNRATE)",                "UNRATE"},
    {"Federal Funds Rate (FEDFUNDS)",             "FEDFUNDS"},
    {"10-Year Treasury Yield (DGS10)",            "DGS10"},
    {"2-Year Treasury Yield (DGS2)",              "DGS2"},
    {"30-Year Mortgage Rate (MORTGAGE30US)",      "MORTGAGE30US"},
    {"M2 Money Supply (M2SL)",                    "M2SL"},
    {"Personal Savings Rate (PSAVERT)",           "PSAVERT"},
    {"Industrial Production (INDPRO)",            "INDPRO"},
    {"Retail Sales (RSXFS)",                      "RSXFS"},
    {"Housing Starts (HOUST)",                    "HOUST"},
    {"Trade Balance (BOPGSTB)",                   "BOPGSTB"},
    {"VIX Volatility Index (VIXCLS)",             "VIXCLS"},
    {"S&P 500 (SP500)",                           "SP500"},
};

FredPanel::FredPanel(QWidget* parent)
    : EconPanelBase(kFredSourceId, kFredColor, parent) {
    build_base_ui(this);
    connect(&services::EconomicsService::instance(),
            &services::EconomicsService::result_ready,
            this, &FredPanel::on_result);
}

void FredPanel::activate() {
    show_empty("Set FRED_API_KEY environment variable, then select a series and click FETCH\n"
               "Get a free key at: fred.stlouisfed.org/docs/api/api_key.html");
}

void FredPanel::build_controls(QHBoxLayout* thl) {
    auto* lbl1 = new QLabel("PRESET");
    lbl1->setStyleSheet(ctrl_label_style());

    preset_combo_ = new QComboBox;
    for (const auto& p : kFredPresets) preset_combo_->addItem(p.first, p.second);
    preset_combo_->setFixedHeight(26);
    preset_combo_->setMinimumWidth(200);
    connect(preset_combo_, &QComboBox::currentIndexChanged, this,
            [this](int idx) {
                const QString code = preset_combo_->itemData(idx).toString();
                if (!code.isEmpty()) series_input_->setText(code);
            });

    auto* lbl2 = new QLabel("SERIES ID");
    lbl2->setStyleSheet(ctrl_label_style());

    series_input_ = new QLineEdit;
    series_input_->setPlaceholderText("e.g. GDPC1");
    series_input_->setFixedHeight(26);
    series_input_->setFixedWidth(100);

    thl->addWidget(lbl1);
    thl->addWidget(preset_combo_);
    thl->addWidget(lbl2);
    thl->addWidget(series_input_);
}

void FredPanel::on_fetch() {
    QString series = series_input_->text().trimmed().toUpper();
    if (series.isEmpty()) {
        const QString code = preset_combo_->currentData().toString();
        if (code.isEmpty()) { show_empty("Enter a FRED series ID"); return; }
        series = code;
    }
    show_loading("Fetching FRED series " + series + "…");
    services::EconomicsService::instance().execute(
        kFredSourceId, kFredScript, "series", {series},
        "fred_" + series);
}

void FredPanel::on_result(const QString& request_id,
                          const services::EconomicsResult& result) {
    if (result.source_id != kFredSourceId) return;
    if (!result.success) {
        // Check for API key error specifically
        if (result.error.contains("API key") || result.error.contains("api_key")) {
            show_error("FRED API key not configured.\n"
                       "Set FRED_API_KEY environment variable.\n"
                       "Free key at: fred.stlouisfed.org/docs/api/api_key.html");
        } else {
            show_error(result.error);
        }
        return;
    }

    if (request_id.startsWith("fred_")) {
        // FRED returns: {observations: [{date, value}]}
        QJsonArray obs = result.data["observations"].toArray();
        if (obs.isEmpty()) obs = result.data["data"].toArray();

        // Convert string values to numbers where possible
        QJsonArray clean;
        for (const auto& v : obs) {
            auto obj = v.toObject();
            const QString val_str = obj["value"].toString();
            if (val_str == "." || val_str.isEmpty()) continue;
            bool ok = false;
            double d = val_str.toDouble(&ok);
            if (ok) obj["value"] = d;
            clean.append(obj);
        }

        const QString series = request_id.mid(5);  // strip "fred_"
        display(clean, "FRED: " + series);
        LOG_INFO("FredPanel", QString("Displayed %1 observations").arg(clean.size()));
    }
}

} // namespace fincept::screens
