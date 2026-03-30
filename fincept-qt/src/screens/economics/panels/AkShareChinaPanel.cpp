// src/screens/economics/panels/AkShareChinaPanel.cpp
// AkShare China macroeconomic data panel. No API key required.
//
// Response shape: { success:true, data:[{...}], count, timestamp, source }
// Column names are in Chinese — displayed as-is in the table.
// Working commands: cpi, ppi, gdp, pmi
#include "screens/economics/panels/AkShareChinaPanel.h"

#include "core/logging/Logger.h"
#include "services/economics/EconomicsService.h"

#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonObject>
#include <QLabel>

namespace fincept::screens {
namespace {

static constexpr const char* kAkShareChinaScript   = "akshare_economics_china.py";
static constexpr const char* kAkShareChinaSourceId = "akshare_cn";
static constexpr const char* kAkShareChinaColor    = "#DC2626";  // China red
} // namespace

struct AkCnSeries {
    QString label;
    QString command;
    QString description;
};

static const QList<AkCnSeries> kAkShareSeries = {
    { "CPI (Consumer Price Index)",      "cpi", "Monthly CPI year-on-year & month-on-month"  },
    { "PPI (Producer Price Index)",      "ppi", "Monthly PPI change rates"                   },
    { "GDP (Gross Domestic Product)",    "gdp", "Quarterly GDP by expenditure approach"       },
    { "PMI (Manufacturing & Services)",  "pmi", "Monthly PMI — official NBS data"            },
};

AkShareChinaPanel::AkShareChinaPanel(QWidget* parent)
    : EconPanelBase(kAkShareChinaSourceId, kAkShareChinaColor, parent) {
    build_base_ui(this);
    connect(&services::EconomicsService::instance(),
            &services::EconomicsService::result_ready,
            this, &AkShareChinaPanel::on_result);
}

void AkShareChinaPanel::activate() {
    show_empty("Select a series and click FETCH\n"
               "Source: AkShare — China NBS macroeconomic data (no API key required)\n"
               "Note: column headers are in Chinese as provided by the source");
}

void AkShareChinaPanel::build_controls(QHBoxLayout* thl) {
    auto* lbl = new QLabel("SERIES");
    lbl->setStyleSheet(
        "color:#525252; font-size:9px; font-weight:700; background:transparent;");

    series_combo_ = new QComboBox;
    for (const auto& s : kAkShareSeries)
        series_combo_->addItem(s.label, s.command);
    series_combo_->setFixedHeight(26);
    series_combo_->setMinimumWidth(260);
    series_combo_->setToolTip("Data from China National Bureau of Statistics via AkShare");

    thl->addWidget(lbl);
    thl->addWidget(series_combo_);
}

void AkShareChinaPanel::on_fetch() {
    const int   idx    = series_combo_->currentIndex();
    const auto& series = kAkShareSeries[idx];

    show_loading("Fetching AkShare China: " + series.label + "…");
    services::EconomicsService::instance().execute(
        kAkShareChinaSourceId, kAkShareChinaScript, series.command, {},
        "akcn_" + series.command);
}

void AkShareChinaPanel::on_result(const QString& request_id,
                                   const services::EconomicsResult& result) {
    if (result.source_id != kAkShareChinaSourceId) return;
    if (!request_id.startsWith("akcn_")) return;
    if (!result.success) { show_error(result.error); return; }

    // Response: {success, data:[{...}], count}
    QJsonArray rows = result.data["data"].toArray();

    if (rows.isEmpty()) {
        const QString err = result.data["error"].toString();
        show_error(err.isEmpty() ? "No data returned" : err);
        return;
    }

    const int idx = series_combo_->currentIndex();
    const QString title = "AkShare China: "
        + (idx >= 0 && idx < kAkShareSeries.size() ? kAkShareSeries[idx].label : request_id.mid(6));

    display(rows, title);
    LOG_INFO("AkShareChinaPanel", QString("Displayed %1 rows: %2")
                                      .arg(rows.size()).arg(title));
}

} // namespace fincept::screens
