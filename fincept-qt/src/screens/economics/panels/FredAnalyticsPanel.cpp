// src/screens/economics/panels/FredAnalyticsPanel.cpp
// FRED Analytics panel — composite dataset views built from multiple FRED series.
// Requires FRED_API_KEY (same key as FredPanel).
//
// Response shapes vary per command (see fred_economic_data.py):
//   money_supply: { observations:[{date,value}], measure, ... }
//   credit:       { observations:[{date,value}], series_id }
//   yield_curve:  { yield_curve:{<tenor>:{series_id, observations:[{date,value}]}}, spreads:{...}, ... }
//   stress:       { stress_indices:{<name>:{series_id, observations:[{date,value}]}}, ... }
//   sentiment:    { sentiment:{<name>:{...}}, ... }
//   pce:          { pce:{<name>:{...}}, ... }
// The last four nest one series per key; flatten_nested_series() turns them into {date, value, series} rows.
#include "screens/economics/panels/FredAnalyticsPanel.h"

#include "core/logging/Logger.h"
#include "services/economics/EconomicsService.h"

#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonObject>
#include <QLabel>

#include <algorithm>
#include <utility>

namespace fincept::screens {
namespace {

static constexpr const char* kFredAnalyticsScript = "fred_economic_data.py";
static constexpr const char* kFredAnalyticsSourceId = "fred_analytics";
static constexpr const char* kFredAnalyticsColor = "#F97316"; // orange (distinct from FredPanel red)
} // namespace

struct FredAnalDataset {
    QString label;
    QString command;
    QStringList args;
};

static const QList<FredAnalDataset> kFredAnalDatasets = {
    {"Yield Curve (Treasury Rates)", "yield_curve", {}},
    {"M1 Money Supply", "money_supply", {"m1"}},
    {"M2 Money Supply", "money_supply", {"m2"}},
    {"M3 Money Supply", "money_supply", {"m3"}},
    {"Fed Funds Rate (Credit)", "credit", {"fed_funds_rate"}},
    {"Prime Rate (Credit)", "credit", {"prime_rate"}},
    {"Financial Stress Index", "stress", {}},
    {"Consumer Sentiment", "sentiment", {}},
    {"PCE Price Index", "pce", {}},
};

FredAnalyticsPanel::FredAnalyticsPanel(QWidget* parent)
    : EconPanelBase(kFredAnalyticsSourceId, kFredAnalyticsColor, parent) {
    build_base_ui(this);
    connect(&services::EconomicsService::instance(), &services::EconomicsService::result_ready, this,
            &FredAnalyticsPanel::on_result);
}

void FredAnalyticsPanel::activate() {
    show_empty(tr("Set FRED_API_KEY environment variable, then select a dataset and click FETCH\n"
                  "Uses the same key as the FRED panel — fred.stlouisfed.org/docs/api/api_key.html"));
}

void FredAnalyticsPanel::build_controls(QHBoxLayout* thl) {
    dataset_lbl_ = new QLabel(tr("DATASET"));
    dataset_lbl_->setStyleSheet(ctrl_label_style());

    dataset_combo_ = new QComboBox;
    for (const auto& d : kFredAnalDatasets)
        dataset_combo_->addItem(d.label, d.command);
    dataset_combo_->setFixedHeight(26);
    dataset_combo_->setMinimumWidth(240);

    thl->addWidget(dataset_lbl_);
    thl->addWidget(dataset_combo_);
}

void FredAnalyticsPanel::on_fetch() {
    const int idx = dataset_combo_->currentIndex();
    const auto& dataset = kFredAnalDatasets[idx];

    show_loading(tr("Fetching FRED Analytics: %1…").arg(dataset.label));

    // EconomicsService::execute() prepends `command` to the argv. Repeating it
    // here shifted the parameters by one: `money_supply money_supply m1` was
    // parsed as measure="money_supply", start_date="m1", so the M1/M2/M3
    // selection never reached FRED and every dataset got a junk start date.
    const QStringList args = dataset.args;

    const QString arg_suffix = dataset.args.isEmpty() ? "" : "_" + dataset.args.join("_");
    services::EconomicsService::instance().execute(kFredAnalyticsSourceId, kFredAnalyticsScript, dataset.command, args,
                                                   "fredan_" + dataset.command + arg_suffix);
}

// Extract rows from various FRED Analytics response shapes.
static QJsonArray extract_fred_analytics_rows(const QJsonObject& data) {
    // Standard observations array
    QJsonArray obs = data["observations"].toArray();
    if (!obs.isEmpty())
        return obs;

    // Yield curve: {curve:[{maturity, yield, date}]}
    obs = data["curve"].toArray();
    if (!obs.isEmpty())
        return obs;

    // Fallback service normalisation
    obs = data["data"].toArray();
    return obs;
}

// Multi-series datasets (yield curve, stress, sentiment, PCE) return one
// {series_id, observations:[{date,value}]} object per series, nested under a group key
// ("yield_curve" + "spreads", "stress_indices", "sentiment", "pce"). Flatten them into
// {date, value, series} rows — the shape BlsPanel feeds display() — newest first, so the
// first page shows the latest readings rather than the 1960s. FRED's "." gaps are left
// for the caller's filter.
static QJsonArray flatten_nested_series(const QJsonObject& data) {
    struct Point {
        QString date;
        QString series;
        QJsonValue value;
    };
    QList<Point> points;
    for (auto group = data.begin(); group != data.end(); ++group) {
        // Scalars and arrays (start_date, available_tenors, ...) convert to an empty object.
        const QJsonObject members = group.value().toObject();
        for (auto member = members.begin(); member != members.end(); ++member) {
            const QString series = member.key();
            const QJsonObject body = member.value().toObject();
            // A series that failed to download is {error, error_code} with no observations.
            // Log the code only: `error` embeds the request URL, api_key included.
            if (!body.contains("observations")) {
                if (body.contains("error_code")) {
                    LOG_WARN("FredAnalyticsPanel",
                             QString("Series %1 unavailable: %2").arg(series, body["error_code"].toString()));
                }
                continue;
            }
            const QJsonArray observations = body["observations"].toArray();
            for (const auto& ov : observations) {
                const QJsonObject o = ov.toObject();
                points.append(Point{o["date"].toString(), series, o["value"]});
            }
        }
    }
    // ISO dates sort lexically; stable, so rows sharing a date keep their series order.
    std::stable_sort(points.begin(), points.end(), [](const Point& a, const Point& b) { return a.date > b.date; });

    QJsonArray rows;
    for (const Point& p : std::as_const(points))
        rows.append(QJsonObject{{"date", p.date}, {"value", p.value}, {"series", p.series}});
    return rows;
}

void FredAnalyticsPanel::on_result(const QString& request_id, const services::EconomicsResult& result) {
    if (result.source_id != kFredAnalyticsSourceId)
        return;
    if (!request_id.startsWith("fredan_"))
        return;

    if (!result.success) {
        const QString msg = result.error;
        if (msg.contains("FRED_API_KEY") || msg.contains("api_key") || msg.contains("API key")) {
            show_error(tr("FRED API key not configured.\n"
                          "Set FRED_API_KEY environment variable.\n"
                          "Free key at: fred.stlouisfed.org/docs/api/api_key.html"));
        } else {
            show_error(msg);
        }
        return;
    }

    // Check inline error
    const QString inline_err = result.data["error"].toString();
    if (!inline_err.isEmpty()) {
        if (inline_err.contains("FRED_API_KEY") || inline_err.contains("not set")) {
            show_error(tr("FRED API key not configured.\n"
                          "Set FRED_API_KEY environment variable.\n"
                          "Free key at: fred.stlouisfed.org/docs/api/api_key.html"));
        } else {
            show_error(inline_err);
        }
        return;
    }

    // Filter string sentinel values (FRED uses "." for missing)
    QJsonArray raw = extract_fred_analytics_rows(result.data);
    // Multi-series datasets have no top-level series array: flatten their nested series instead.
    const bool flattened = raw.isEmpty();
    if (flattened)
        raw = flatten_nested_series(result.data);
    QJsonArray rows;
    for (const auto& rv : raw) {
        const QJsonObject r = rv.toObject();
        const QString val_str = r["value"].toString();
        if (val_str == "." || (val_str.isEmpty() && !r["value"].isDouble()))
            continue;
        rows.append(r);
    }

    if (rows.isEmpty()) {
        show_error(tr("No data returned — check FRED_API_KEY is set"));
        return;
    }

    const int idx = dataset_combo_->currentIndex();
    const QString title =
        "FRED Analytics: " +
        (idx >= 0 && idx < kFredAnalDatasets.size() ? kFredAnalDatasets[idx].label : request_id.mid(7));

    // A flattened table mixes series on different scales, so LATEST/CHANGE/MIN/MAX/AVG would
    // be meaningless (same call as ImfPanel's cross-section view). Single series get them back.
    set_stats_visible(!flattened);
    display(rows, title);
    LOG_INFO("FredAnalyticsPanel", QString("Displayed %1 rows: %2").arg(rows.size()).arg(title));
}

// ── i18n ──────────────────────────────────────────────────────────────────────

void FredAnalyticsPanel::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange)
        retranslateUi();
    EconPanelBase::changeEvent(event);
}

void FredAnalyticsPanel::retranslateUi() {
    if (dataset_lbl_)
        dataset_lbl_->setText(tr("DATASET"));
    EconPanelBase::retranslateUi();
}

} // namespace fincept::screens
