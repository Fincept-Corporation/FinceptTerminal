// src/screens/economics/panels/BlsPanel.cpp
// BLS data panel. Requires BLS_API_KEY environment variable.
//
// Response shape (bls_data.py):
//   get_series:  { success, data: { series_data: [row...], metadata, messages: [string...] } }
//   overviews:   { success, data: { <group>: [row...], ..., summary | description } }
//   row:         { symbol, year, period, date, value, title?, footnotes?, latest, annual_average? }
//
// Error shape (PythonRunner turns it into a failed result):
//   { error: true, message: "BLS API key required...", endpoint, status_code, timestamp }
#include "screens/economics/panels/BlsPanel.h"

#include "core/logging/Logger.h"
#include "services/economics/EconomicsService.h"

#include <QCoreApplication>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonObject>
#include <QLabel>

namespace fincept::screens {
namespace {

static constexpr const char* kBlsScript = "bls_data.py";
static constexpr const char* kBlsSourceId = "bls";
static constexpr const char* kBlsColor = "#DC2626"; // BLS red

struct BlsPreset {
    QString label;
    QString command;
    QString series_id; // non-empty -> use get_series command
    QString units;     // what the values are (BLS publishes it per series, not in the API rows)
};

// The CPI / PPI presets are the NOT seasonally adjusted indexes (CUUR…, WPU…): read them year over
// year (the YoY card) — their month-over-month change includes seasonality.
static const QList<BlsPreset> kBlsPresets = {
    {"-- Select a series --", "", "", ""},
    {"Total Nonfarm Employment (SA)", "get_series", "CES0000000001", "Thousands of persons"},
    {"Unemployment Rate U-3 (SA)", "get_series", "LNS14000000", "Percent"},
    {"CPI-U All Items (NSA index)", "get_series", "CUUR0000SA0", "Index 1982-84=100"},
    {"CPI-U Less Food & Energy (NSA index)", "get_series", "CUUR0000SA0L1E", "Index 1982-84=100"},
    {"PPI Final Demand (NSA index)", "get_series", "WPUFD4", "Index Nov 2009=100"},
    {"Average Hourly Earnings, Private (SA)", "get_series", "CES0500000003", "Dollars per hour"},
    {"Labor Force Participation Rate (SA)", "get_series", "LNS11300000", "Percent"},
    {"Job Openings, JOLTS (SA)", "get_series", "JTS000000000000000JOL", "Thousands"},
    {"Labor Market Overview", "get_labor_overview", "", ""},
    {"Inflation Overview", "get_inflation_overview", "", ""},
    {"Employment Cost Index", "get_employment_cost_index", "", ""},
    {"Productivity & Costs", "get_productivity_costs", "", ""},
};

/// BLS series ids carry the seasonal-adjustment code as their third character for the main surveys
/// (CES/CEU, LNS/LNU, CUSR/CUUR, WPS/WPU, JTS/JTU, CIS/CIU, PRS/PRU, SMS/SMU, LAS/LAU).
QString bls_seasonal_label(const QString& series_id) {
    static const QStringList kSurveys = {"CE", "LN", "CU", "WP", "JT", "CI", "PR", "SM", "LA"};
    if (series_id.size() < 3 || !kSurveys.contains(series_id.left(2)))
        return {};
    const QChar code = series_id.at(2);
    if (code == QLatin1Char('S'))
        return QCoreApplication::translate("fincept::screens::BlsPanel", "Seasonally adjusted");
    if (code == QLatin1Char('U'))
        return QCoreApplication::translate("fincept::screens::BlsPanel", "Not seasonally adjusted");
    return {};
}

} // namespace

BlsPanel::BlsPanel(QWidget* parent) : EconPanelBase(kBlsSourceId, kBlsColor, parent) {
    build_base_ui(this);
    connect(&services::EconomicsService::instance(), &services::EconomicsService::result_ready, this,
            &BlsPanel::on_result);
}

void BlsPanel::activate() {
    show_empty(tr("Set BLS_API_KEY environment variable, then select a series and click FETCH\n"
                  "Get a free key at: data.bls.gov/registrationEngine/"));
}

void BlsPanel::build_controls(QHBoxLayout* thl) {
    auto lbl = [](const QString& t) {
        auto* l = new QLabel(t);
        l->setObjectName(QStringLiteral("econCtrlLabel"));
        return l;
    };

    preset_combo_ = new QComboBox;
    for (const auto& p : kBlsPresets)
        preset_combo_->addItem(p.label, p.command + "|" + p.series_id);
    preset_combo_->setFixedHeight(26);
    preset_combo_->setMinimumWidth(240);

    connect(preset_combo_, &QComboBox::currentIndexChanged, this, [this](int idx) {
        if (idx > 0 && idx < kBlsPresets.size()) {
            const auto& p = kBlsPresets[idx];
            if (!p.series_id.isEmpty())
                series_input_->setText(p.series_id);
        }
    });

    series_lbl_ = new QLabel(tr("SERIES ID"));
    series_lbl_->setObjectName(QStringLiteral("econCtrlLabel"));

    series_input_ = new QLineEdit;
    series_input_->setPlaceholderText(tr("e.g. CES0000000001"));
    series_input_->setFixedHeight(26);
    series_input_->setFixedWidth(140);

    thl->addWidget(preset_lbl_ = lbl(tr("PRESET")));
    thl->addWidget(preset_combo_);
    thl->addWidget(series_lbl_);
    thl->addWidget(series_input_);
}

void BlsPanel::on_fetch() {
    const int idx = preset_combo_->currentIndex();
    if (idx <= 0) {
        // Check manual series ID
        const QString sid = series_input_->text().trimmed().toUpper();
        if (sid.isEmpty()) {
            show_empty(tr("Select a preset or enter a series ID"));
            return;
        }
        show_loading(tr("Fetching BLS series %1…").arg(sid));
        services::EconomicsService::instance().execute(kBlsSourceId, kBlsScript, "get_series", {sid},
                                                       "bls_series_" + sid);
        return;
    }

    const auto& preset = kBlsPresets[idx];
    show_loading(tr("Fetching BLS: %1…").arg(preset.label));

    if (preset.command == "get_series") {
        const QString sid = series_input_->text().trimmed().toUpper();
        const QString use_sid = sid.isEmpty() ? preset.series_id : sid;
        services::EconomicsService::instance().execute(kBlsSourceId, kBlsScript, "get_series", {use_sid},
                                                       "bls_series_" + use_sid);
    } else {
        services::EconomicsService::instance().execute(kBlsSourceId, kBlsScript, preset.command, {},
                                                       "bls_" + preset.command);
    }
}

// Flatten bls_data.py's answer (see the shape at the top of this file) into display rows
// {date, value, series, series_id[, footnotes]}.
static QJsonArray extract_bls_rows(const QJsonObject& result) {
    const QJsonObject payload = result.value(QStringLiteral("data")).toObject();
    QJsonArray source = payload.value(QStringLiteral("series_data")).toArray();
    if (source.isEmpty()) {
        // Overview commands: one array of rows per group (unemployment_rate, core_cpi, ...).
        for (auto it = payload.constBegin(); it != payload.constEnd(); ++it)
            if (it.value().isArray())
                for (const auto& v : it.value().toArray())
                    source.append(v);
    }

    QJsonArray rows;
    for (const auto& v : std::as_const(source)) {
        if (!v.isObject())
            continue; // e.g. the "messages" strings
        const QJsonObject r = v.toObject();
        // BLS periods: M01–M12 monthly, M13 = ANNUAL AVERAGE, Q01–Q04 quarterly (Q05 = annual
        // avg), S01–S02 semi-annual (S03 = annual avg), A01 annual.
        const QString year = r.value(QStringLiteral("year")).toString();
        const QString period = r.value(QStringLiteral("period")).toString();
        // Drop the synthetic annual-average rows: mixing them into a monthly/quarterly series
        // inflates the point count and skews MIN/MAX/AVG and CHANGE's "previous" value.
        if (r.value(QStringLiteral("annual_average")).toBool() || period == QLatin1String("M13") ||
            period == QLatin1String("Q05") || period == QLatin1String("S03"))
            continue;
        QString date;
        if (!year.isEmpty() && period.size() >= 2) {
            const QChar freq = period.at(0);
            const int num = period.mid(1).toInt();
            if (freq == QLatin1Char('M'))
                date = year + "-" + QString::number(num).rightJustified(2, '0'); // 2024-03
            else if (freq == QLatin1Char('Q'))
                date = year + "-Q" + QString::number(num); // 2024-Q1 (sorts correctly)
            else if (freq == QLatin1Char('S'))
                date = year + "-S" + QString::number(num);
            else
                date = year; // annual
        } else {
            date = r.value(QStringLiteral("date")).toString(); // period-start date (older answers)
        }
        if (date.isEmpty())
            continue;

        const QString series_id = r.value(QStringLiteral("symbol")).toString();
        QString series = r.value(QStringLiteral("title")).toString();
        if (series.isEmpty())
            series = series_id; // BLS has no catalog entry for some surveys (e.g. JOLTS)
        QJsonObject row;
        row[QStringLiteral("date")] = date;
        row[QStringLiteral("value")] = r.value(QStringLiteral("value"));
        row[QStringLiteral("series")] = series;
        row[QStringLiteral("series_id")] = series_id;
        const QString footnotes = r.value(QStringLiteral("footnotes")).toString();
        if (!footnotes.isEmpty())
            row[QStringLiteral("footnotes")] = footnotes;
        rows.append(row);
    }
    return rows;
}

void BlsPanel::on_result(const QString& request_id, const services::EconomicsResult& result) {
    if (result.source_id != kBlsSourceId)
        return;
    if (!request_id.startsWith("bls_"))
        return;

    if (!result.success) {
        const QString msg = result.error;
        if (msg.contains("API key") || msg.contains("api_key") || msg.contains("BLS_API_KEY")) {
            show_error(tr("BLS API key not configured.\n"
                          "Set BLS_API_KEY environment variable.\n"
                          "Free key at: data.bls.gov/registrationEngine/"));
        } else {
            show_error(msg);
        }
        return;
    }

    // Also check for error object inside successful result
    if (result.data["error"].toBool()) {
        const QString msg = result.data["message"].toString(result.data["error"].toString());
        if (msg.contains("API key") || msg.contains("BLS_API_KEY")) {
            show_error(tr("BLS API key not configured.\n"
                          "Set BLS_API_KEY environment variable.\n"
                          "Free key at: data.bls.gov/registrationEngine/"));
        } else {
            show_error(msg);
        }
        return;
    }

    const QJsonArray rows = extract_bls_rows(result.data);
    if (rows.isEmpty()) {
        show_error(tr("No data returned"));
        return;
    }

    // Describe the series this result belongs to (request id "bls_series_<ID>"), not whatever the
    // combo shows now.
    QString title = "BLS: " + request_id.mid(4);
    SeriesMeta meta;
    if (request_id.startsWith(QLatin1String("bls_series_"))) {
        const QString sid = request_id.mid(QStringLiteral("bls_series_").size());
        title = "BLS: " + sid;
        for (const auto& p : kBlsPresets) {
            if (p.series_id == sid) {
                title = "BLS: " + p.label + " — " + sid;
                meta.units = p.units;
                break;
            }
        }
        if (meta.units.isEmpty() && !rows.isEmpty()) {
            // Custom id: BLS's own series title is the best description available.
            const QString series_title = rows.first().toObject().value(QStringLiteral("series")).toString();
            if (!series_title.isEmpty() && series_title != sid)
                title = "BLS: " + series_title + " — " + sid;
        }
        meta.seasonal_adj = bls_seasonal_label(sid);
    } else {
        for (const auto& p : kBlsPresets)
            if (!p.command.isEmpty() && request_id == "bls_" + p.command)
                title = "BLS: " + p.label;
    }
    set_series_meta(meta);
    display(rows, title);
    LOG_INFO("BlsPanel", QString("Displayed %1 rows").arg(rows.size()));
}

// ── i18n ──────────────────────────────────────────────────────────────────────

void BlsPanel::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange)
        retranslateUi();
    EconPanelBase::changeEvent(event);
}

void BlsPanel::retranslateUi() {
    if (preset_lbl_)
        preset_lbl_->setText(tr("PRESET"));
    if (series_lbl_)
        series_lbl_->setText(tr("SERIES ID"));
    if (series_input_)
        series_input_->setPlaceholderText(tr("e.g. CES0000000001"));
    EconPanelBase::retranslateUi();
}

} // namespace fincept::screens
