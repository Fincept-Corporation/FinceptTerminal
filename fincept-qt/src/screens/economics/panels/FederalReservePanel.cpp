// src/screens/economics/panels/FederalReservePanel.cpp
// US Federal Reserve data — Fed Funds Rate, SOFR, Treasury Rates, Yield Curve, Money Supply.
// Script: federal_reserve_data.py  |  No API key required.
//
// Response shape: { success, endpoint, data:[{date, rate|...}] }
#include "screens/economics/panels/FederalReservePanel.h"

#include "core/logging/Logger.h"
#include "services/economics/EconomicsService.h"

#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonObject>
#include <QLabel>

#include <cmath>

namespace fincept::screens {
namespace {

static constexpr const char* kFederalReserveScript = "federal_reserve_data.py";
static constexpr const char* kFederalReserveSourceId = "federal_reserve";
static constexpr const char* kFederalReserveColor = "#DC2626"; // Fed red
} // namespace

struct FedSeries {
    QString label;
    QString command;
    QStringList args;
    bool decimal_rates; // the script returns rates as fractions (0.0388) — shown as percent (3.88)
    QString units;
    QString frequency;
    QString source;
};

static const QList<FedSeries> kFedReserveSeries = {
    {"Effective Federal Funds Rate (EFFR)", "federal_funds_rate", {}, true,
     "Percent (volume: USD billions)", "Daily", "Federal Reserve Bank of New York"},
    {"SOFR Overnight Rate", "sofr_rate", {}, true, "Percent (volume: USD billions)", "Daily",
     "Federal Reserve Bank of New York"},
    {"Treasury Constant-Maturity Yields", "treasury_rates", {}, true, "Percent", "Daily",
     "Federal Reserve H.15 Selected Interest Rates"},
    {"Yield Curve (latest date)", "yield_curve", {}, true, "Percent", "Daily",
     "Federal Reserve H.15 Selected Interest Rates"},
    {"Money Stock M1 / M2 (H.6)", "money_measures", {}, false, "Billions of dollars", "Monthly",
     "Federal Reserve H.6 Money Stock Measures"},
    {"Fed Balance Sheet: SOMA Holdings", "central_bank_holdings", {}, false, "Billions of dollars", "Weekly",
     "Federal Reserve Bank of New York — SOMA"},
};

// Fields of the rate payloads that are NOT rates (left as they are).
static bool fed_is_non_rate_field(const QString& key) {
    return key == QLatin1String("date") || key == QLatin1String("maturity") || key == QLatin1String("volume") ||
           key == QLatin1String("revision_indicator");
}

FederalReservePanel::FederalReservePanel(QWidget* parent)
    : EconPanelBase(kFederalReserveSourceId, kFederalReserveColor, parent) {
    build_base_ui(this);
    connect(&services::EconomicsService::instance(), &services::EconomicsService::result_ready, this,
            &FederalReservePanel::on_result);
}

void FederalReservePanel::activate() {
    show_empty(tr("Select a series and click FETCH\n"
                  "Sources: New York Fed (EFFR, SOFR, SOMA holdings) and Federal Reserve Board releases\n"
                  "(H.15 Treasury yields, H.6 money stock). Rates are shown in percent. No API key required"));
}

void FederalReservePanel::build_controls(QHBoxLayout* thl) {
    series_lbl_ = new QLabel(tr("SERIES"));
    series_lbl_->setObjectName(QStringLiteral("econCtrlLabel"));

    series_combo_ = new QComboBox;
    for (const auto& s : kFedReserveSeries)
        series_combo_->addItem(s.label, s.command);
    series_combo_->setFixedHeight(26);
    series_combo_->setMinimumWidth(240);

    thl->addWidget(series_lbl_);
    thl->addWidget(series_combo_);
}

void FederalReservePanel::on_fetch() {
    const int idx = series_combo_->currentIndex();
    const auto& series = kFedReserveSeries[idx];

    show_loading(tr("Fetching Federal Reserve: %1…").arg(series.label));
    services::EconomicsService::instance().execute(kFederalReserveSourceId, kFederalReserveScript, series.command,
                                                   series.args, "fed_" + series.command);
}

void FederalReservePanel::on_result(const QString& request_id, const services::EconomicsResult& result) {
    if (result.source_id != kFederalReserveSourceId)
        return;
    if (!request_id.startsWith("fed_"))
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

    // Response: { success, endpoint, data:[{date, <value cols>}] }
    QJsonArray rows = result.data["data"].toArray();

    // Some endpoints nest differently — handle yield_curve which may use "rates" key
    if (rows.isEmpty())
        rows = result.data["rates"].toArray();

    // Filter: keep rows that have at least one non-empty numeric-ish value beyond "date"
    QJsonArray clean;
    for (const auto& rv : rows) {
        const QJsonObject r = rv.toObject();
        bool has_val = false;
        for (auto it = r.begin(); it != r.end(); ++it) {
            if (it.key() == "date")
                continue;
            if (it.value().isDouble() || !it.value().toString().isEmpty()) {
                has_val = true;
                break;
            }
        }
        if (has_val)
            clean.append(r);
    }

    if (clean.isEmpty()) {
        show_error(tr("No data returned"));
        return;
    }

    // The series this result belongs to (request id "fed_<command>"), not the combo's current pick.
    const FedSeries* series = nullptr;
    for (const auto& fs : kFedReserveSeries)
        if (request_id == "fed_" + fs.command)
            series = &fs;
    const QString title = "Federal Reserve: " + (series ? series->label : request_id.mid(4));

    SeriesMeta meta;
    if (series) {
        meta.units = series->units;
        meta.frequency = series->frequency;
        meta.source = series->source;
        if (series->decimal_rates) {
            // NY Fed / H.15 payloads carry rates as decimal fractions: 0.0388 is 3.88 %. Shown raw,
            // the table and the LATEST card read like a 0.04 % funds rate.
            meta.percent = 1;
            meta.value_column = QStringLiteral("rate");
            QJsonArray scaled;
            for (const auto& rv : std::as_const(clean)) {
                QJsonObject r = rv.toObject();
                for (auto it = r.begin(); it != r.end(); ++it)
                    if (!fed_is_non_rate_field(it.key()) && it.value().isDouble())
                        it.value() = std::round(it.value().toDouble() * 100.0 * 1e6) / 1e6; // to %, no float dust
                scaled.append(r);
            }
            clean = scaled;
        }
        if (series->command == QLatin1String("money_measures")) {
            meta.value_column = QStringLiteral("M2");
            meta.seasonal_adj = tr("Not seasonally adjusted");
        }
        if (series->command == QLatin1String("central_bank_holdings"))
            meta.value_column = QStringLiteral("total");
        if (series->command == QLatin1String("treasury_rates"))
            meta.value_column = QStringLiteral("year_10"); // cards follow the 10-year tenor
    }
    set_series_meta(meta);
    display(clean, title);
    LOG_INFO("FederalReservePanel", QString("Displayed %1 rows: %2").arg(clean.size()).arg(title));
}

// ── i18n ──────────────────────────────────────────────────────────────────────

void FederalReservePanel::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange)
        retranslateUi();
    EconPanelBase::changeEvent(event);
}

void FederalReservePanel::retranslateUi() {
    if (series_lbl_)
        series_lbl_->setText(tr("SERIES"));
    EconPanelBase::retranslateUi();
}

} // namespace fincept::screens
