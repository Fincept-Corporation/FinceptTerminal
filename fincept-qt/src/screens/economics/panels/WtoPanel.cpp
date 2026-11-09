// src/screens/economics/panels/WtoPanel.cpp
// WTO (World Trade Organization) data wrapper
//
// Three sections with different commands and response shapes:
//
// Every WTO API (Timeseries, QR, ...) requires WTO_API_KEY (Ocp-Apim-Subscription-Key).
//
// TRADE STATISTICS (Timeseries API)
//   Command: timeseries_data --i=<indicator> --r=<reporter> --ps=<years> --head=M [--pc=<product> --p=000]
//            (wto_data.py turns ISO reporter codes into WTO codes, e.g. US -> 840, and 2015-2023 into
//            the comma-separated year list the API takes)
//   Response: { "success": true, "data": { "Dataset": [...] } }
//   Dataset[] rows (head=M): { "IndicatorCode", "ReportingEconomyCode": "840",
//                     "ReportingEconomy": "United States of America", "PartnerEconomyCode",
//                     "ProductOrSectorCode", "ProductOrSector", "Year": 2023, "Unit", "Value": 2018059 }
//
// QR MEMBERS
//   Command: qr_members
//   Response: { "success": true, "data": [{"code": "US", "name": "United States", ...}] }
//
// QR NOTIFICATIONS
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

static constexpr const char* kWtoScript = "wto_data.py";
static constexpr const char* kWtoSourceId = "wto";
static constexpr const char* kWtoColor = "#E91E63"; // pink-red
// Trade Statistics indicators: annual series from the WTO Timeseries catalogue
// (GET /timeseries/v1/indicators). Product/sector indicators are asked for their total (TO = total
// merchandise, SOX = commercial services) against the World partner (000), so the answer is one
// series per reporter.
struct WtoIndicator {
    const char* code;
    const char* label;
    const char* product; // pc filter; nullptr: the indicator has no product/partner dimension
};
static const WtoIndicator kWtoIndicators[] = {
    {"ITS_MTV_AX", "Merchandise exports, total (ITS_MTV_AX)", "TO"},
    {"ITS_MTV_AM", "Merchandise imports, total (ITS_MTV_AM)", "TO"},
    {"ITS_CS_AX6", "Commercial services exports (ITS_CS_AX6)", "SOX"},
    {"ITS_CS_AM6", "Commercial services imports (ITS_CS_AM6)", "SOX"},
    {"TP_A_0010", "Simple average MFN applied tariff, all products (TP_A_0010)", nullptr},
    {"TP_A_0160", "Simple average MFN applied tariff, agricultural (TP_A_0160)", nullptr},
    {"TP_A_0430", "Simple average MFN applied tariff, non-agricultural (TP_A_0430)", nullptr},
    {"TP_A_0030", "Trade-weighted MFN applied tariff average, all products (TP_A_0030)", nullptr},
};

const WtoIndicator* wto_find_indicator(const QString& code) {
    for (const auto& ind : kWtoIndicators)
        if (code == QLatin1String(ind.code))
            return &ind;
    return nullptr;
}

} // namespace

// ── Constructor ───────────────────────────────────────────────────────────────

WtoPanel::WtoPanel(QWidget* parent) : EconPanelBase(kWtoSourceId, kWtoColor, parent) {
    build_base_ui(this);
    connect(&services::EconomicsService::instance(), &services::EconomicsService::result_ready, this,
            &WtoPanel::on_result);
}

void WtoPanel::activate() {
    show_empty(tr("Select a section and click FETCH\n"
                  "Every WTO API used here requires WTO_API_KEY\n"
                  "(Ocp-Apim-Subscription-Key from apiportal.wto.org)"));
}

// ── Controls ──────────────────────────────────────────────────────────────────

void WtoPanel::build_controls(QHBoxLayout* thl) {
    auto make_lbl = [](const QString& text) {
        auto* l = new QLabel(text);
        l->setObjectName(QStringLiteral("econCtrlLabel"));
        return l;
    };

    section_combo_ = new QComboBox;
    section_combo_->addItem(tr("Trade Statistics"), "timeseries");
    section_combo_->addItem(tr("QR Members"), "qr_members");
    section_combo_->addItem(tr("QR Notifications"), "qr_notifications");
    section_combo_->setFixedHeight(26);
    section_combo_->setMinimumWidth(200);
    connect(section_combo_, &QComboBox::currentIndexChanged, this, &WtoPanel::on_section_changed);

    indicator_combo_ = new QComboBox;
    for (const auto& ind : kWtoIndicators)
        indicator_combo_->addItem(QString::fromLatin1(ind.label), QString::fromLatin1(ind.code));
    indicator_combo_->setFixedHeight(26);
    indicator_combo_->setMinimumWidth(270);

    reporter_input_ = new QLineEdit;
    reporter_input_->setPlaceholderText(tr("Reporter (e.g. US, CN, DE)"));
    reporter_input_->setText("US");
    reporter_input_->setFixedHeight(26);
    reporter_input_->setFixedWidth(130);

    years_input_ = new QLineEdit;
    years_input_->setPlaceholderText(tr("Years (e.g. 2015-2023)"));
    years_input_->setText("2015-2023");
    years_input_->setFixedHeight(26);
    years_input_->setFixedWidth(110);

    apikey_notice_ = new QLabel(tr("Requires WTO_API_KEY"));
    apikey_notice_->setStyleSheet(notice_style());

    thl->addWidget(section_lbl_ = make_lbl(tr("SECTION")));
    thl->addWidget(section_combo_);
    thl->addWidget(indicator_lbl_ = make_lbl(tr("INDICATOR")));
    thl->addWidget(indicator_combo_);
    thl->addWidget(reporter_lbl_ = make_lbl(tr("REPORTER")));
    thl->addWidget(reporter_input_);
    thl->addWidget(years_lbl_ = make_lbl(tr("YEARS")));
    thl->addWidget(years_input_);
    thl->addWidget(apikey_notice_);
}

void WtoPanel::on_section_changed(int index) {
    const bool is_timeseries = (index == 0);
    indicator_combo_->setEnabled(is_timeseries);
    years_input_->setEnabled(is_timeseries);

    // The QR API needs the same subscription key as the Timeseries API (apikey_notice_ says so).
    reporter_input_->setPlaceholderText(is_timeseries ? tr("Reporter (e.g. US, CN, DE)")
                                                      : tr("Member code (e.g. US, CN)"));
}

// ── Fetch ─────────────────────────────────────────────────────────────────────

void WtoPanel::on_fetch() {
    const QString section = section_combo_->currentData().toString();
    const QString reporter = reporter_input_->text().trimmed().toUpper();

    if (section == "timeseries") {
        const QString indicator = indicator_combo_->currentData().toString();
        const QString years = years_input_->text().trimmed();

        if (indicator.isEmpty()) {
            show_empty(tr("Select a trade indicator"));
            return;
        }
        if (reporter.isEmpty()) {
            show_empty(tr("Enter a reporter code (e.g. US)"));
            return;
        }

        // wto_data.py takes --flag=value args named like the API parameters. head=M asks for the
        // machine-readable row keys on_result() reads (ReportingEconomy, Year, Value, ...).
        QStringList args = {"--i=" + indicator, "--r=" + reporter,
                            "--ps=" + (years.isEmpty() ? QStringLiteral("default") : years),
                            QStringLiteral("--head=M")};
        const WtoIndicator* ind = wto_find_indicator(indicator);
        if (ind && ind->product)
            args << QStringLiteral("--pc=") + QLatin1String(ind->product) << QStringLiteral("--p=000");
        show_loading(tr("Fetching WTO Trade Statistics: %1 for %2…").arg(indicator_combo_->currentText(), reporter));
        services::EconomicsService::instance().execute(kWtoSourceId, kWtoScript, "timeseries_data", args,
                                                       "wto_ts_" + indicator + "_" + reporter);

    } else if (section == "qr_members") {
        QStringList args;
        if (!reporter.isEmpty())
            args << "--member_code=" + reporter;
        show_loading(tr("Fetching WTO QR Members…"));
        services::EconomicsService::instance().execute(kWtoSourceId, kWtoScript, "qr_members", args,
                                                       "wto_qrm_" + reporter);

    } else {
        // qr_notifications
        QStringList args;
        if (!reporter.isEmpty())
            args << "--reporter_member_code=" + reporter;
        show_loading(tr("Fetching WTO QR Notifications for %1…").arg(reporter));
        services::EconomicsService::instance().execute(kWtoSourceId, kWtoScript, "qr_notifications", args,
                                                       "wto_qrn_" + reporter);
    }
}

// ── Result ────────────────────────────────────────────────────────────────────

void WtoPanel::on_result(const QString& request_id, const services::EconomicsResult& result) {
    if (result.source_id != kWtoSourceId)
        return;

    if (!result.success) {
        if (result.error.contains("subscription") || result.error.contains("API key") || result.error.contains("401") ||
            result.error.contains("403")) {
            show_error(tr("WTO API key not configured.\n"
                          "Set WTO_API_KEY environment variable\n"
                          "(Ocp-Apim-Subscription-Key from developer.wto.org)"));
        } else {
            show_error(result.error);
        }
        return;
    }

    QJsonArray rows;

    if (request_id.startsWith("wto_ts_")) {
        // Trade Statistics: data is { "Dataset": [...] } (rows described at the top of this file)
        // or data is the Dataset array directly
        const QJsonObject data_obj = result.data["data"].toObject();
        QJsonArray dataset = data_obj["Dataset"].toArray();

        if (dataset.isEmpty()) {
            // Try data directly as array
            dataset = result.data["data"].toArray();
        }

        // Describe the request this answer belongs to ("wto_ts_<indicator>_<reporter>"; indicator
        // codes contain '_'), not whatever the combo shows now.
        const QString req_tail = request_id.mid(QStringLiteral("wto_ts_").size());
        const QString req_indicator = req_tail.left(req_tail.lastIndexOf(QLatin1Char('_')));

        // Flatten to clean display rows: Reporter | Year | Value | Product
        QString unit;
        for (const auto& v : dataset) {
            const auto obj = v.toObject();
            QJsonObject row;
            const QJsonValue reporter_name = obj.value(QStringLiteral("ReportingEconomy"));
            row["reporter"] =
                reporter_name.isString() ? reporter_name : obj.value(QStringLiteral("ReportingEconomyCode"));
            row["year"] = obj.value(QStringLiteral("Year"));
            row["value"] = obj.value(QStringLiteral("Value"));
            const QJsonValue product = obj.value(QStringLiteral("ProductOrSector"));
            if (product.isString())
                row["product"] = product;
            row["indicator"] = obj.value(QStringLiteral("IndicatorCode")).toString(req_indicator);
            if (unit.isEmpty())
                unit = obj.value(QStringLiteral("Unit")).toString();
            rows.append(row);
        }

        if (rows.isEmpty()) {
            show_empty(tr("No data returned — check reporter code or years range\n"
                          "Also verify WTO_API_KEY is set correctly"));
            return;
        }
        SeriesMeta meta;
        meta.units = unit; // "Million US dollar", "Percent"
        set_series_meta(meta);
        const WtoIndicator* ind = wto_find_indicator(req_indicator);
        display(rows, "WTO Trade Statistics: " + (ind ? QString::fromLatin1(ind->label) : req_indicator));

    } else if (request_id.startsWith("wto_qrm_")) {
        // QR Members: data is array of { code, name, ... }
        rows = result.data["data"].toArray();
        if (rows.isEmpty()) {
            show_empty(tr("No members data returned"));
            return;
        }
        display(rows, tr("WTO Quantitative Restrictions — Members"));

    } else {
        // QR Notifications: data is array of notification objects
        rows = result.data["data"].toArray();
        if (rows.isEmpty()) {
            show_empty(tr("No notifications found for this member code"));
            return;
        }
        display(rows, tr("WTO QR Notifications — %1").arg(reporter_input_->text().toUpper()));
    }

    LOG_INFO("WtoPanel", QString("Displayed %1 rows for %2").arg(rows.size()).arg(request_id));
}

// ── i18n ──────────────────────────────────────────────────────────────────────

void WtoPanel::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange)
        retranslateUi();
    EconPanelBase::changeEvent(event);
}

void WtoPanel::retranslateUi() {
    if (section_lbl_)
        section_lbl_->setText(tr("SECTION"));
    if (indicator_lbl_)
        indicator_lbl_->setText(tr("INDICATOR"));
    if (reporter_lbl_)
        reporter_lbl_->setText(tr("REPORTER"));
    if (years_lbl_)
        years_lbl_->setText(tr("YEARS"));
    if (section_combo_ && section_combo_->count() >= 3) {
        section_combo_->setItemText(0, tr("Trade Statistics"));
        section_combo_->setItemText(1, tr("QR Members"));
        section_combo_->setItemText(2, tr("QR Notifications"));
    }
    if (years_input_)
        years_input_->setPlaceholderText(tr("Years (e.g. 2015-2023)"));
    // Reporter placeholder + API-key notice depend on the active section.
    const bool is_timeseries = section_combo_ && section_combo_->currentIndex() == 0;
    if (reporter_input_)
        reporter_input_->setPlaceholderText(is_timeseries ? tr("Reporter (e.g. US, CN, DE)")
                                                          : tr("Member code (e.g. US, CN)"));
    if (apikey_notice_)
        apikey_notice_->setText(tr("Requires WTO_API_KEY"));
    EconPanelBase::retranslateUi();
}

} // namespace fincept::screens
