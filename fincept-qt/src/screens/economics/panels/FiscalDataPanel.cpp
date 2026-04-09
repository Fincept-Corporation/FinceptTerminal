// src/screens/economics/panels/FiscalDataPanel.cpp
// US Treasury FiscalData — debt to penny, interest rates, interest expense, exchange rates.
// Script: fiscal_data.py  |  No API key required.
//
// Response shape: { data:[{record_date, <value cols>}] }
#include "screens/economics/panels/FiscalDataPanel.h"

#include "core/logging/Logger.h"
#include "services/economics/EconomicsService.h"

#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonObject>
#include <QLabel>

namespace fincept::screens {
namespace {

static constexpr const char* kFiscalDataScript = "fiscal_data.py";
static constexpr const char* kFiscalDataSourceId = "fiscal_data";
static constexpr const char* kFiscalDataColor = "#0EA5E9"; // Treasury blue
} // namespace

struct FiscalSeries {
    QString label;
    QString command;
    QStringList args;
};

static const QList<FiscalSeries> kFiscalDataSeries = {
    {"Debt to the Penny", "debt-to-penny", {"--all"}},   {"Avg Interest Rates", "avg-interest-rates", {"--all"}},
    {"Interest Expense", "interest-expense", {"--all"}}, {"US Treasury Exchange Rates", "exchange-rates", {}},
    {"Record-Setting Auction Debt", "record-debt", {}},
};

FiscalDataPanel::FiscalDataPanel(QWidget* parent) : EconPanelBase(kFiscalDataSourceId, kFiscalDataColor, parent) {
    build_base_ui(this);
    connect(&services::EconomicsService::instance(), &services::EconomicsService::result_ready, this,
            &FiscalDataPanel::on_result);
}

void FiscalDataPanel::activate() {
    show_empty("Select a dataset and click FETCH\n"
               "Source: US Treasury FiscalData API (fiscaldata.treasury.gov)\n"
               "No API key required");
}

void FiscalDataPanel::build_controls(QHBoxLayout* thl) {
    auto* lbl = new QLabel("DATASET");
    lbl->setStyleSheet(ctrl_label_style());

    series_combo_ = new QComboBox;
    for (const auto& s : kFiscalDataSeries)
        series_combo_->addItem(s.label, s.command);
    series_combo_->setFixedHeight(26);
    series_combo_->setMinimumWidth(240);

    thl->addWidget(lbl);
    thl->addWidget(series_combo_);
}

void FiscalDataPanel::on_fetch() {
    const int idx = series_combo_->currentIndex();
    const auto& series = kFiscalDataSeries[idx];

    show_loading("Fetching FiscalData: " + series.label + "…");

    QStringList args = {series.command};
    args << series.args;

    services::EconomicsService::instance().execute(kFiscalDataSourceId, kFiscalDataScript, series.command, series.args,
                                                   "fiscal_" + QString(series.command).replace('-', '_'));
}

void FiscalDataPanel::on_result(const QString& request_id, const services::EconomicsResult& result) {
    if (result.source_id != kFiscalDataSourceId)
        return;
    if (!request_id.startsWith("fiscal_"))
        return;
    if (!result.success) {
        show_error(result.error);
        return;
    }

    const QString inline_err = result.data["error"].toString();
    if (!inline_err.isEmpty()) {
        show_error(inline_err);
        return;
    }

    // Response: { data:[{record_date, ...}] }
    QJsonArray rows = result.data["data"].toArray();

    if (rows.isEmpty()) {
        show_error("No data returned");
        return;
    }

    const int idx = series_combo_->currentIndex();
    const QString title = "FiscalData: " + (idx >= 0 && idx < kFiscalDataSeries.size() ? kFiscalDataSeries[idx].label
                                                                                       : request_id.mid(7));

    display(rows, title);
    LOG_INFO("FiscalDataPanel", QString("Displayed %1 rows: %2").arg(rows.size()).arg(title));
}

} // namespace fincept::screens
