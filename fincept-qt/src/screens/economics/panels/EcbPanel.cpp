// src/screens/economics/panels/EcbPanel.cpp
// ECB SDMX data panel.
// Response shape: {<key>: [{dimensions:{...}, observations:[{period,value}], obs_count}], ...}
// Working commands: exchange_rates, inflation, money_supply
#include "screens/economics/panels/EcbPanel.h"

#include "core/logging/Logger.h"
#include "services/economics/EconomicsService.h"

#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonObject>
#include <QLabel>

namespace fincept::screens {
namespace {

static constexpr const char* kEcbScript = "ecb_sdmx_data.py";
static constexpr const char* kEcbSourceId = "ecb";
static constexpr const char* kEcbColor = "#2563EB"; // ECB blue

// Series: display name -> {command, arg, result key} + what the numbers ARE (the ECB series key
// decides it: EXR …SP00.A = daily reference rate, ICP …N…ANR = annual rate of change, not seasonally
// adjusted; BSI …Y…M10/M20/M30 = outstanding stocks, EUR millions, working-day & seasonally adjusted).
struct EcbSeries {
    QString label;
    QString command;
    QString arg;        // currency for exchange_rates / aggregate for money_supply
    QString result_key; // top-level JSON key in response
    QString units;
    QString frequency;
    QString seasonal;
    int percent; // 1 = rate (changes in pp), 0 = level
};

static const QList<EcbSeries> kEcbSeries = {
    {"EUR/USD Exchange Rate", "exchange_rates", "USD", "exchange_rates", "USD per EUR", "Daily",
     "ECB euro reference rate", 0},
    {"EUR/GBP Exchange Rate", "exchange_rates", "GBP", "exchange_rates", "GBP per EUR", "Daily",
     "ECB euro reference rate", 0},
    {"EUR/JPY Exchange Rate", "exchange_rates", "JPY", "exchange_rates", "JPY per EUR", "Daily",
     "ECB euro reference rate", 0},
    {"EUR/CHF Exchange Rate", "exchange_rates", "CHF", "exchange_rates", "CHF per EUR", "Daily",
     "ECB euro reference rate", 0},
    {"EUR/CNY Exchange Rate", "exchange_rates", "CNY", "exchange_rates", "CNY per EUR", "Daily",
     "ECB euro reference rate", 0},
    {"Euro Area HICP Inflation (annual rate, %)", "inflation", "", "inflation", "Annual rate of change, %", "Monthly",
     "Not seasonally adjusted", 1},
    {"Euro Area M1 (stock)", "money_supply", "M1", "money_supply", "EUR millions", "Monthly",
     "Working-day and seasonally adjusted", 0},
    {"Euro Area M2 (stock)", "money_supply", "M2", "money_supply", "EUR millions", "Monthly",
     "Working-day and seasonally adjusted", 0},
    {"Euro Area M3 (stock)", "money_supply", "M3", "money_supply", "EUR millions", "Monthly",
     "Working-day and seasonally adjusted", 0},
};

} // namespace

EcbPanel::EcbPanel(QWidget* parent) : EconPanelBase(kEcbSourceId, kEcbColor, parent) {
    build_base_ui(this);
    connect(&services::EconomicsService::instance(), &services::EconomicsService::result_ready, this,
            &EcbPanel::on_result);
}

void EcbPanel::activate() {
    show_empty(tr("Select a series and click FETCH\n"
                  "Source: European Central Bank SDMX 2.1 API"));
}

void EcbPanel::build_controls(QHBoxLayout* thl) {
    auto lbl = [](const QString& t) {
        auto* l = new QLabel(t);
        l->setObjectName(QStringLiteral("econCtrlLabel"));
        return l;
    };

    series_combo_ = new QComboBox;
    for (const auto& s : kEcbSeries)
        series_combo_->addItem(s.label, s.command + "|" + s.arg);
    series_combo_->setFixedHeight(26);
    series_combo_->setMinimumWidth(220);

    thl->addWidget(series_lbl_ = lbl(tr("SERIES")));
    thl->addWidget(series_combo_);
}

void EcbPanel::on_fetch() {
    const int idx = series_combo_->currentIndex();
    const auto& series = kEcbSeries[idx];

    show_loading(tr("Fetching ECB data: %1…").arg(series.label));

    // EconomicsService::execute() already prepends `command` to the argv, so
    // args must carry ONLY the extra positional arguments. Repeating the
    // command here shifted every parameter by one — the CLI read
    // `exchange_rates exchange_rates USD` as currency="exchange_rates",
    // freq="USD", so every ECB fetch silently queried a bogus series.
    QStringList args;
    if (!series.arg.isEmpty())
        args << series.arg;

    const QString req_id = "ecb_" + series.command + (series.arg.isEmpty() ? "" : "_" + series.arg);

    services::EconomicsService::instance().execute(kEcbSourceId, kEcbScript, series.command, args, req_id);
}

// ECB response: {<result_key>: [{dimensions:{}, observations:[{period,value}], obs_count}], ...}
// Flatten the first series' observations into a plain [{period, value}] array for display().
void EcbPanel::on_result(const QString& request_id, const services::EconomicsResult& result) {
    if (result.source_id != kEcbSourceId)
        return;
    if (!result.success) {
        show_error(result.error);
        return;
    }
    if (!request_id.startsWith("ecb_"))
        return;

    // Which series was fetched comes from the request id ("ecb_<command>[_<arg>]"), not the combo —
    // the user may have picked another series while this one was loading, which used to read the
    // wrong result key ("No observations") or put this data under the other series' name.
    const EcbSeries* found = nullptr;
    for (const auto& s : kEcbSeries) {
        if (request_id == "ecb_" + s.command + (s.arg.isEmpty() ? QString() : "_" + s.arg)) {
            found = &s;
            break;
        }
    }
    if (!found)
        return;
    const auto& series = *found;

    // Extract observations from first element of the result array
    const QJsonValue top = result.data[series.result_key];
    QJsonArray obs;

    if (top.isArray()) {
        const QJsonArray arr = top.toArray();
        if (!arr.isEmpty()) {
            obs = arr[0].toObject()["observations"].toArray();
        }
    } else if (top.isObject()) {
        // Fallback: result wrapped as object
        obs = top.toObject()["observations"].toArray();
    }

    // Also accept flat data array from service normalisation
    if (obs.isEmpty()) {
        obs = result.data["data"].toArray();
    }

    if (obs.isEmpty()) {
        show_error(tr("No observations returned for %1").arg(series.label));
        return;
    }

    SeriesMeta meta;
    meta.units = series.units;
    meta.frequency = series.frequency;
    meta.seasonal_adj = series.seasonal;
    meta.percent = series.percent;
    set_series_meta(meta);
    display(obs, "ECB: " + series.label);
    LOG_INFO("EcbPanel", QString("Displayed %1 observations for %2").arg(obs.size()).arg(series.label));
}

// ── i18n ──────────────────────────────────────────────────────────────────────

void EcbPanel::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange)
        retranslateUi();
    EconPanelBase::changeEvent(event);
}

void EcbPanel::retranslateUi() {
    if (series_lbl_)
        series_lbl_->setText(tr("SERIES"));
    EconPanelBase::retranslateUi();
}

} // namespace fincept::screens
