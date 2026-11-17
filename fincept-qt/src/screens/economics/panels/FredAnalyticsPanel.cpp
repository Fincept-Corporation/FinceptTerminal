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
    // Single-series datasets: what the numbers are (FRED series metadata). Empty for the multi-series
    // groups, whose rows mix units.
    QString units;
    QString frequency;
    QString seasonal;
};

// "M3 Money Supply" was removed: the Fed discontinued M3 in March 2006, FRED has no current M3
// series, and the request ("m3") always failed.
static const QList<FredAnalDataset> kFredAnalDatasets = {
    {"Yield Curve (Treasury Rates)", "yield_curve", {}, "", "", ""},
    {"M1 Money Supply (M1SL)", "money_supply", {"m1"}, "Billions of Dollars", "Monthly", "Seasonally Adjusted"},
    {"M2 Money Supply (M2SL)", "money_supply", {"m2"}, "Billions of Dollars", "Monthly", "Seasonally Adjusted"},
    {"Monetary Base (BOGMBASE)", "money_supply", {"monetary_base"}, "Millions of Dollars", "Monthly",
     "Not Seasonally Adjusted"},
    {"M2 Velocity (M2V)", "money_supply", {"m2_velocity"}, "Ratio", "Quarterly", "Seasonally Adjusted"},
    {"Fed Funds Rate, effective (FEDFUNDS)", "credit", {"fed_funds_rate"}, "Percent", "Monthly",
     "Not Seasonally Adjusted"},
    {"Bank Prime Loan Rate (DPRIME)", "credit", {"prime_rate"}, "Percent", "Daily", "Not Seasonally Adjusted"},
    {"Financial Stress Index", "stress", {}, "", "", ""},
    {"Consumer Sentiment", "sentiment", {}, "", "", ""},
    {"PCE Price Index", "pce", {}, "", "", ""},
};

FredAnalyticsPanel::FredAnalyticsPanel(QWidget* parent)
    : EconPanelBase(kFredAnalyticsSourceId, kFredAnalyticsColor, parent) {
    build_base_ui(this);
    connect(&services::EconomicsService::instance(), &services::EconomicsService::result_ready, this,
            &FredAnalyticsPanel::on_result);
}

void FredAnalyticsPanel::activate() {
    // Nothing to prepare: the results pane runs the query on show — or, without a FRED key, shows the
    // page that names FRED_API_KEY and opens Settings > Credentials.
}

QList<ui::Requirement> FredAnalyticsPanel::load_requirements() const {
    return {ui::Requirement::api_key(QStringLiteral("FRED_API_KEY"), QStringLiteral("FRED"))};
}

void FredAnalyticsPanel::build_controls(QHBoxLayout* thl) {
    dataset_lbl_ = new QLabel(tr("DATASET"));
    dataset_lbl_->setObjectName(QStringLiteral("econCtrlLabel"));

    dataset_combo_ = new QComboBox;
    for (const auto& d : kFredAnalDatasets)
        dataset_combo_->addItem(d.label, d.command);
    dataset_combo_->setMinimumHeight(26);
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
    execute_query(kFredAnalyticsSourceId, kFredAnalyticsScript, dataset.command, args,
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
// `skipped` (optional) receives the members that carried no observations.
static QJsonArray flatten_nested_series(const QJsonObject& data, QStringList* skipped = nullptr) {
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
                // Only series-shaped members count as "unavailable" (not metadata objects).
                if (skipped && (body.contains("error") || body.contains("error_code") || body.contains("series_id")))
                    skipped->append(series);
                continue;
            }
            const QJsonArray observations = body["observations"].toArray();
            for (const auto& ov : observations) {
                const QJsonObject o = ov.toObject();
                points.append(Point{o["date"].toVariant().toString(), series, o["value"]});
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
                          "Add it in Settings > Credentials (FRED_API_KEY).\n"
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
                          "Add it in Settings > Credentials (FRED_API_KEY).\n"
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
    QStringList skipped_series;
    if (flattened)
        raw = flatten_nested_series(result.data, &skipped_series);
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

    // Identify the dataset from the request id ("fredan_<command>[_<args>]"), not the combo.
    const FredAnalDataset* ds = nullptr;
    for (const auto& d : kFredAnalDatasets) {
        if (request_id == "fredan_" + d.command + (d.args.isEmpty() ? QString() : "_" + d.args.join("_"))) {
            ds = &d;
            break;
        }
    }
    const QString title = "FRED Analytics: " + (ds ? ds->label : request_id.mid(7));

    // A flattened table mixes series on different scales, so LATEST/CHANGE/MIN/MAX/AVG would
    // be meaningless (same call as ImfPanel's cross-section view). Single series get them back.
    set_stats_visible(!flattened);
    if (ds && !flattened) {
        SeriesMeta meta;
        meta.units = ds->units;
        meta.frequency = ds->frequency;
        meta.seasonal_adj = ds->seasonal;
        set_series_meta(meta);
    }
    display(rows, title);
    // Say which advertised series are missing instead of dropping them silently (after display(),
    // which clears the banner).
    if (!skipped_series.isEmpty())
        set_banner(tr("%n series unavailable: %1", "", static_cast<int>(skipped_series.size()))
                       .arg(skipped_series.join(QStringLiteral(", "))));
    LOG_DEBUG("FredAnalyticsPanel", QString("Displayed %1 rows: %2").arg(rows.size()).arg(title));
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
