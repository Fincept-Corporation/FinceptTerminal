// src/screens/economics/panels/WtoPanel.cpp
// WTO (World Trade Organization) data wrapper
//
// Three sections with different commands and response shapes:
//
// TRADE STATISTICS — requires WTO_API_KEY (Ocp-Apim-Subscription-Key)
//   Command: timeseries_data --i <indicator> --r <reporter> --ps <years>
//   Response: { "success": true, "data": { "Dataset": [...], "dataTotalRecords": N } }
//   Dataset[] rows: { "ReporterCode": "840", "ReporterName": "United States",
//                     "ProductOrSectorCode": "...", "Year": 2023, "Value": 1234567.8,
//                     "ValueFlag": null }
//
// QR MEMBERS — free, no API key
//   Command: qr_members
//   Response: { "success": true, "data": [{"code": "US", "name": "United States", ...}] }
//
// QR NOTIFICATIONS — free, no API key
//   Command: qr_notifications --reporter_member_code <code>
//   Response: { "success": true, "data": [{"notification_year", "reporter_member_code",
//               "product_description", "measure_type", ...}] }
#include "screens/economics/panels/WtoPanel.h"

#include "core/logging/Logger.h"
#include "services/economics/EconomicsService.h"

#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonObject>
#include <QLabel>
#include <QVBoxLayout>

namespace fincept::screens {
namespace {

static constexpr const char* kWtoScript   = "wto_data.py";
static constexpr const char* kWtoSourceId = "wto";
static constexpr const char* kWtoColor    = "#E91E63";  // pink-red
// Trade Statistics indicators (WTO Timeseries API)
static const QList<QPair<QString,QString>> kWtoIndicators = {
    {"Total Merchandise Trade (TP_A_0010)",     "TP_A_0010"},
    {"Merchandise Exports (TX_A_0010)",         "TX_A_0010"},
    {"Merchandise Imports (TM_A_0010)",         "TM_A_0010"},
    {"Commercial Services Trade (TS_A_0010)",   "TS_A_0010"},
    {"Services Exports (SX_A_0010)",            "SX_A_0010"},
    {"Services Imports (SM_A_0010)",            "SM_A_0010"},
    {"Tariff Rate, All Products (TRF_A_0010)",  "TRF_A_0010"},
};

} // namespace

// ── Constructor ───────────────────────────────────────────────────────────────

WtoPanel::WtoPanel(QWidget* parent)
    : EconPanelBase(kWtoSourceId, kWtoColor, parent) {
    build_base_ui(this);
    connect(&services::EconomicsService::instance(),
            &services::EconomicsService::result_ready,
            this, &WtoPanel::on_result);
}

void WtoPanel::activate() {
    show_empty("Select a section and click FETCH\n"
               "QR Members & Notifications are free — no API key needed\n"
               "Trade Statistics requires WTO_API_KEY (Ocp-Apim-Subscription-Key)");
}

// ── Controls ──────────────────────────────────────────────────────────────────

void WtoPanel::build_controls(QHBoxLayout* thl) {
    auto make_lbl = [](const QString& text) {
        auto* l = new QLabel(text);
        l->setStyleSheet(
            "color:#525252; font-size:9px; font-weight:700; background:transparent;");
        return l;
    };

    section_combo_ = new QComboBox;
    section_combo_->addItem("Trade Statistics (API key)",  "timeseries");
    section_combo_->addItem("QR Members (free)",           "qr_members");
    section_combo_->addItem("QR Notifications (free)",     "qr_notifications");
    section_combo_->setFixedHeight(26);
    section_combo_->setMinimumWidth(200);
    connect(section_combo_, &QComboBox::currentIndexChanged,
            this, &WtoPanel::on_section_changed);

    indicator_combo_ = new QComboBox;
    for (const auto& ind : kWtoIndicators)
        indicator_combo_->addItem(ind.first, ind.second);
    indicator_combo_->setFixedHeight(26);
    indicator_combo_->setMinimumWidth(270);

    reporter_input_ = new QLineEdit;
    reporter_input_->setPlaceholderText("Reporter (e.g. US, CN, DE)");
    reporter_input_->setText("US");
    reporter_input_->setFixedHeight(26);
    reporter_input_->setFixedWidth(130);

    years_input_ = new QLineEdit;
    years_input_->setPlaceholderText("Years (e.g. 2015-2023)");
    years_input_->setText("2015-2023");
    years_input_->setFixedHeight(26);
    years_input_->setFixedWidth(110);

    apikey_notice_ = new QLabel("Requires WTO_API_KEY");
    apikey_notice_->setStyleSheet(
        "color:#f59e0b; font-size:9px; background:transparent;");

    thl->addWidget(make_lbl("SECTION"));
    thl->addWidget(section_combo_);
    thl->addWidget(make_lbl("INDICATOR"));
    thl->addWidget(indicator_combo_);
    thl->addWidget(make_lbl("REPORTER"));
    thl->addWidget(reporter_input_);
    thl->addWidget(make_lbl("YEARS"));
    thl->addWidget(years_input_);
    thl->addWidget(apikey_notice_);
}

void WtoPanel::on_section_changed(int index) {
    const bool is_timeseries = (index == 0);
    indicator_combo_->setEnabled(is_timeseries);
    years_input_->setEnabled(is_timeseries);

    if (is_timeseries) {
        apikey_notice_->setText("Requires WTO_API_KEY");
        apikey_notice_->setStyleSheet(
            "color:#f59e0b; font-size:9px; background:transparent;");
        reporter_input_->setPlaceholderText("Reporter (e.g. US, CN, DE)");
    } else {
        apikey_notice_->setText("Free — no API key");
        apikey_notice_->setStyleSheet(
            "color:#16a34a; font-size:9px; background:transparent;");
        reporter_input_->setPlaceholderText("Member code (e.g. US, CN)");
    }
}

// ── Fetch ─────────────────────────────────────────────────────────────────────

void WtoPanel::on_fetch() {
    const QString section  = section_combo_->currentData().toString();
    const QString reporter = reporter_input_->text().trimmed().toUpper();

    if (section == "timeseries") {
        const QString indicator = indicator_combo_->currentData().toString();
        const QString years     = years_input_->text().trimmed();

        if (indicator.isEmpty()) { show_empty("Select a trade indicator"); return; }
        if (reporter.isEmpty())  { show_empty("Enter a reporter code (e.g. US)"); return; }

        // wto_data.py uses --flag=value style args
        // CLI: timeseries_data --i TP_A_0010 --r US --ps 2015-2023
        show_loading("Fetching WTO Trade Statistics: " +
                     indicator_combo_->currentText() + " for " + reporter + "…");
        services::EconomicsService::instance().execute(
            kWtoSourceId, kWtoScript, "timeseries_data",
            {"--i=" + indicator,
             "--r=" + reporter,
             "--ps=" + (years.isEmpty() ? "default" : years)},
            "wto_ts_" + indicator + "_" + reporter);

    } else if (section == "qr_members") {
        QStringList args;
        if (!reporter.isEmpty()) args << "--member_code=" + reporter;
        show_loading("Fetching WTO QR Members…");
        services::EconomicsService::instance().execute(
            kWtoSourceId, kWtoScript, "qr_members",
            args,
            "wto_qrm_" + reporter);

    } else {
        // qr_notifications
        QStringList args;
        if (!reporter.isEmpty()) args << "--reporter_member_code=" + reporter;
        show_loading("Fetching WTO QR Notifications for " + reporter + "…");
        services::EconomicsService::instance().execute(
            kWtoSourceId, kWtoScript, "qr_notifications",
            args,
            "wto_qrn_" + reporter);
    }
}

// ── Result ────────────────────────────────────────────────────────────────────

void WtoPanel::on_result(const QString& request_id,
                         const services::EconomicsResult& result) {
    if (result.source_id != kWtoSourceId) return;

    if (!result.success) {
        if (result.error.contains("subscription") ||
            result.error.contains("API key") ||
            result.error.contains("401") ||
            result.error.contains("403")) {
            show_error("WTO API key not configured.\n"
                       "Set WTO_API_KEY environment variable\n"
                       "(Ocp-Apim-Subscription-Key from developer.wto.org)");
        } else {
            show_error(result.error);
        }
        return;
    }

    QJsonArray rows;

    if (request_id.startsWith("wto_ts_")) {
        // Trade Statistics: data is { "Dataset": [...], "dataTotalRecords": N }
        // or data is the Dataset array directly
        const QJsonObject data_obj = result.data["data"].toObject();
        QJsonArray dataset = data_obj["Dataset"].toArray();

        if (dataset.isEmpty()) {
            // Try data directly as array
            dataset = result.data["data"].toArray();
        }

        // Flatten to clean display rows: Reporter | Year | Value | Product
        for (const auto& v : dataset) {
            const auto obj = v.toObject();
            QJsonObject row;
            row["reporter"]  = obj["ReporterName"].isUndefined()
                             ? obj["ReporterCode"] : obj["ReporterName"];
            row["year"]      = obj["Year"];
            row["value"]     = obj["Value"];
            row["product"]   = obj["ProductOrSectorCode"];
            row["indicator"] = indicator_combo_->currentData().toString();
            rows.append(row);
        }

        if (rows.isEmpty()) {
            show_empty("No data returned — check reporter code or years range\n"
                       "Also verify WTO_API_KEY is set correctly");
            return;
        }
        display(rows, "WTO Trade Statistics: " + indicator_combo_->currentText());

    } else if (request_id.startsWith("wto_qrm_")) {
        // QR Members: data is array of { code, name, ... }
        rows = result.data["data"].toArray();
        if (rows.isEmpty()) { show_empty("No members data returned"); return; }
        display(rows, "WTO Quantitative Restrictions — Members");

    } else {
        // QR Notifications: data is array of notification objects
        rows = result.data["data"].toArray();
        if (rows.isEmpty()) {
            show_empty("No notifications found for this member code");
            return;
        }
        display(rows, "WTO QR Notifications — " + reporter_input_->text().toUpper());
    }

    LOG_INFO("WtoPanel", QString("Displayed %1 rows for %2")
             .arg(rows.size()).arg(request_id));
}

} // namespace fincept::screens
