// src/screens/economics/panels/UnComtradePanel.cpp
#include "screens/economics/panels/UnComtradePanel.h"

#include "core/logging/Logger.h"
#include "services/economics/EconomicsService.h"

#include <QDate>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonObject>
#include <QLabel>
#include <QVBoxLayout>

namespace fincept::screens {
namespace {

static constexpr const char* kUnComtradeScript   = "un_comtrade_data.py";
static constexpr const char* kUnComtradeSourceId = "un_comtrade";
static constexpr const char* kUnComtradeColor    = "#0891B2";  // cyan
} // namespace

// ISO numeric reporter codes for major economies
static const QList<QPair<QString,QString>> kReporters = {
    {"United States",  "842"},
    {"China",          "156"},
    {"Germany",        "276"},
    {"Japan",          "392"},
    {"United Kingdom", "826"},
    {"France",         "251"},
    {"India",          "699"},
    {"Brazil",         "76"},
    {"Canada",         "124"},
    {"South Korea",    "410"},
    {"Australia",      "36"},
    {"Russia",         "643"},
    {"Mexico",         "484"},
    {"World",          "0"},
};

UnComtradePanel::UnComtradePanel(QWidget* parent)
    : EconPanelBase(kUnComtradeSourceId, kUnComtradeColor, parent) {
    build_base_ui(this);
    connect(&services::EconomicsService::instance(),
            &services::EconomicsService::result_ready,
            this, &UnComtradePanel::on_result);
}

void UnComtradePanel::activate() {
    show_empty("Select reporter, flow, and period, then click FETCH\n"
               "(Free tier: up to 250 records per request)");
}

void UnComtradePanel::build_controls(QHBoxLayout* thl) {
    auto lbl = [](const QString& t) {
        auto* l = new QLabel(t);
        l->setStyleSheet(
            "color:#525252; font-size:9px; font-weight:700; background:transparent;");
        return l;
    };

    reporter_combo_ = new QComboBox;
    for (const auto& p : kReporters) reporter_combo_->addItem(p.first, p.second);
    reporter_combo_->setFixedHeight(26);

    flow_combo_ = new QComboBox;
    flow_combo_->addItem("Exports", "X");
    flow_combo_->addItem("Imports", "M");
    flow_combo_->addItem("Re-exports", "RX");
    flow_combo_->addItem("Re-imports", "RM");
    flow_combo_->setFixedHeight(26);

    // Last 5 years
    period_combo_ = new QComboBox;
    int cur_year = QDate::currentDate().year();
    for (int y = cur_year; y >= cur_year - 5; --y)
        period_combo_->addItem(QString::number(y), QString::number(y));
    period_combo_->setFixedHeight(26);

    cmd_combo_ = new QComboBox;
    cmd_combo_->addItem("All Commodities", "AG2");
    cmd_combo_->addItem("Total (all)",     "TOTAL");
    cmd_combo_->setFixedHeight(26);

    thl->addWidget(lbl("REPORTER"));
    thl->addWidget(reporter_combo_);
    thl->addWidget(lbl("FLOW"));
    thl->addWidget(flow_combo_);
    thl->addWidget(lbl("YEAR"));
    thl->addWidget(period_combo_);
}

void UnComtradePanel::on_fetch() {
    const QString reporter = reporter_combo_->currentData().toString();
    const QString period   = period_combo_->currentData().toString();
    const QString flow     = flow_combo_->currentData().toString();

    show_loading("Fetching UN Comtrade data…");
    services::EconomicsService::instance().execute(
        kUnComtradeSourceId, kUnComtradeScript, "trade_balance",
        {reporter, period},
        "comtrade_" + reporter + "_" + period + "_" + flow);
}

void UnComtradePanel::on_result(const QString& request_id,
                                const services::EconomicsResult& result) {
    if (result.source_id != kUnComtradeSourceId) return;
    if (!result.success) { show_error(result.error); return; }

    if (request_id.startsWith("comtrade_")) {
        const QJsonArray arr = result.data["data"].toArray();
        const QString title  = reporter_combo_->currentText() + " — " +
                               flow_combo_->currentText() + " " +
                               period_combo_->currentText();
        display(arr, title);
        LOG_INFO("UnComtradePanel", QString("Displayed %1 rows").arg(arr.size()));
    }
}

} // namespace fincept::screens
