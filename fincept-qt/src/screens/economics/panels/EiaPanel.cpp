// src/screens/economics/panels/EiaPanel.cpp
// EIA U.S. Energy Information Administration
//
// Two data sources with different commands and response shapes:
//
// WPSR (Weekly Petroleum Status Report) — NO API KEY NEEDED
//   Command: get_petroleum <category> "" "" "" "" <latest_weeks>   (default view: latest 52 weeks)
//            get_petroleum <category>                              (FULL HISTORY: five years)
//   Response: { "success": true, "category": "...", "data": [...], "count": N }
//   data[] rows: { "date": "YYYY-MM-DD", "category": "...", "table": "...",
//                  "symbol": "...", "value": 1.23, "source": "petroleum_status_report" }
//
// STEO (Short-Term Energy Outlook) — REQUIRES EIA_API_KEY
//   Command: get_steo <table>
//   Response: { "success": true, "table": "...", "data": [...] }
//   data[] rows: { "date": "YYYY-MM", "symbol": "WTIPUUS", "title": "WTI Crude...",
//                  "value": 78.5, "units": "$/barrel", "table": "STEO-01: ...",
//                  "source": "short_term_energy_outlook" }
#include "screens/economics/panels/EiaPanel.h"

#include "core/logging/Logger.h"
#include "services/economics/EconomicsService.h"
#include "ui/theme/Theme.h"

#include <QDate>
#include <QHBoxLayout>
#include <QHash>
#include <QJsonArray>
#include <QJsonObject>
#include <QLabel>
#include <QSignalBlocker>
#include <QVBoxLayout>

namespace fincept::screens {
namespace {

static constexpr const char* kEiaScript = "eia_data.py";
static constexpr const char* kEiaSourceId = "eia";
static constexpr const char* kEiaColor = "#4CAF50"; // green
/// WPSR default view: the latest report weeks only (eia_data.py's 6th positional, latest_weeks). The
/// five-year balance sheet is ~23,500 rows / 7.6 MB; a year of it ~4,900 rows. FULL HISTORY asks for
/// the whole window.
static constexpr int kEiaDefaultWeeks = 52;
} // namespace

// ── WPSR categories (no API key needed, downloads public XLS)
static const QList<QPair<QString, QString>> kWpsrCategories = {
    {"Balance Sheet", "balance_sheet"},
    {"Inputs & Production", "inputs_and_production"},
    {"Crude Petroleum Stocks", "crude_petroleum_stocks"},
    {"Gasoline & Fuel Stocks", "gasoline_fuel_stocks"},
    {"Gasoline by Sub-PADD", "total_gasoline_by_sub_padd"},
    {"Distillate Fuel Oil Stocks", "distillate_fuel_oil_stocks"},
    {"Imports", "imports"},
    {"Imports by Country", "imports_by_country"},
    {"Weekly Estimates", "weekly_estimates"},
    {"Spot Prices (Crude/Gas/Heating Oil)", "spot_prices_crude_gas_heating"},
    {"Spot Prices (Diesel/Jet/Propane)", "spot_prices_diesel_jet_fuel_propane"},
    {"Retail Prices", "retail_prices"},
    {"Refiner/Blender Net Production", "refiner_blender_net_production"},
};

// ── STEO tables (requires EIA_API_KEY)
static const QList<QPair<QString, QString>> kSteoTables = {
    {"01 — US Energy Markets Summary", "01"},          {"02 — Nominal Energy Prices", "02"},
    {"04a — Petroleum & Liquid Fuels", "04a"},         {"05a — Natural Gas Supply & Consumption", "05a"},
    {"07a — US Electricity Industry Overview", "07a"}, {"09a — Macroeconomic Indicators & CO2", "09a"},
};

// ── Constructor ───────────────────────────────────────────────────────────────

EiaPanel::EiaPanel(QWidget* parent) : EconPanelBase(kEiaSourceId, kEiaColor, parent) {
    build_base_ui(this);
    connect(&services::EconomicsService::instance(), &services::EconomicsService::result_ready, this,
            &EiaPanel::on_result);
}

void EiaPanel::activate() {
    // Nothing to prepare: the results pane runs the query on show. The weekly petroleum report needs
    // no key; the Short-Term Energy Outlook names EIA_API_KEY when it is missing.
}

QList<ui::Requirement> EiaPanel::load_requirements() const {
    if (source_combo_ && source_combo_->currentData().toString() == QLatin1String("steo"))
        return {ui::Requirement::api_key(QStringLiteral("EIA_API_KEY"), QStringLiteral("EIA"))};
    return {};
}

// ── Controls ──────────────────────────────────────────────────────────────────

void EiaPanel::build_controls(QHBoxLayout* thl) {
    auto make_lbl = [](const QString& text) {
        auto* l = new QLabel(text);
        l->setObjectName(QStringLiteral("econCtrlLabel"));
        return l;
    };

    source_combo_ = new QComboBox;
    source_combo_->addItem(tr("Weekly Petroleum (WPSR)"), "wpsr");
    source_combo_->addItem(tr("Short-Term Outlook (STEO)"), "steo");
    source_combo_->setMinimumHeight(26);
    source_combo_->setMinimumWidth(200);
    connect(source_combo_, &QComboBox::currentIndexChanged, this, &EiaPanel::on_source_changed);

    category_combo_ = new QComboBox;
    category_combo_->setMinimumHeight(26);
    category_combo_->setMinimumWidth(260);

    // Populate with WPSR categories initially
    for (const auto& c : kWpsrCategories)
        category_combo_->addItem(c.first, c.second);

    apikey_notice_ = new QLabel(tr("No API key needed"));
    set_label_tone(apikey_notice_, "ok");

    // Filled from the series present in the last result; picking one re-filters locally (no refetch).
    series_combo_ = new QComboBox;
    series_combo_->setMinimumHeight(26);
    series_combo_->setMinimumWidth(260);
    series_combo_->setMaximumWidth(420);
    series_combo_->setToolTip(tr("Series of the fetched category — shown one at a time"));
    series_combo_->setEnabled(false);
    connect(series_combo_, &QComboBox::currentIndexChanged, this, [this](int) { show_selected_series(); });
    mark_local_input(series_combo_); // picks within the result: not part of the query

    // The weekly report opens on its latest 52 weeks; the five-year history is loaded on demand.
    full_history_ = new QCheckBox(tr("FULL HISTORY"));
    full_history_->setToolTip(tr("Load every report week of the last five years instead of the latest %1 weeks "
                                 "(the balance sheet is then about 23,500 rows)")
                                  .arg(kEiaDefaultWeeks));
    full_history_->setAccessibleName(tr("Load the full five-year history"));
    connect(full_history_, &QCheckBox::clicked, this, [this]() { run_query(); });

    thl->addWidget(source_lbl_ = make_lbl(tr("SOURCE")));
    thl->addWidget(source_combo_);
    thl->addWidget(category_lbl_ = make_lbl(tr("CATEGORY")));
    thl->addWidget(category_combo_);
    thl->addWidget(series_lbl_ = make_lbl(tr("SERIES")));
    thl->addWidget(series_combo_);
    thl->addWidget(full_history_);
    thl->addWidget(apikey_notice_);
}

void EiaPanel::on_source_changed(int index) {
    category_combo_->clear();
    if (index == 0) {
        // WPSR
        for (const auto& c : kWpsrCategories)
            category_combo_->addItem(c.first, c.second);
        apikey_notice_->setText(tr("No API key needed"));
        set_label_tone(apikey_notice_, "ok");
    } else {
        // STEO
        for (const auto& t : kSteoTables)
            category_combo_->addItem(t.first, t.second);
        apikey_notice_->setText(tr("Requires EIA_API_KEY"));
        set_label_tone(apikey_notice_, "warn");
    }
    if (full_history_)
        full_history_->setEnabled(index == 0); // the weekly report only
    recheck_load_requirements(); // STEO needs EIA_API_KEY, WPSR nothing
}

// ── Fetch ─────────────────────────────────────────────────────────────────────

void EiaPanel::on_fetch() {
    const QString source = source_combo_->currentData().toString();
    const QString category = category_combo_->currentData().toString();

    if (category.isEmpty()) {
        show_empty(tr("Select a category"));
        return;
    }

    if (source == "wpsr") {
        show_loading(tr("Fetching EIA Petroleum Report: %1…\n"
                        "(Downloads public XLS file — may take a few seconds)")
                         .arg(category_combo_->currentText()));
        // get_petroleum <category> [tables_csv] [start_date] [end_date] [symbol] [latest_weeks]: blank
        // positionals are "not set"; the default view asks for the latest weeks only.
        const bool full = full_history_ && full_history_->isChecked();
        QStringList args{category};
        if (!full)
            args << QString() << QString() << QString() << QString() << QString::number(kEiaDefaultWeeks);
        execute_query(kEiaSourceId, kEiaScript, "get_petroleum", args,
                      (full ? QStringLiteral("eia_wpsrall_") : QStringLiteral("eia_wpsr_")) + category);
    } else {
        show_loading(tr("Fetching EIA STEO Table %1…").arg(category_combo_->currentText()));
        execute_query(kEiaSourceId, kEiaScript, "get_steo", {category},
                      "eia_steo_" + category);
    }
}

// ── Result ────────────────────────────────────────────────────────────────────

void EiaPanel::on_result(const QString& request_id, const services::EconomicsResult& result) {
    if (result.source_id != kEiaSourceId)
        return;

    if (!result.success) {
        // Detect API key error for STEO
        if (!result.error_code.isEmpty()) {
            show_error(result.error); // named by the script ("[missing_key] …", "[bad_args] …"): its own page
        } else if (result.error.contains("API key") || result.error.contains("api_key")) {
            show_error(tr("EIA API key not configured.\n"
                          "Add it in Settings > Credentials (EIA_API_KEY).\n"
                          "Free key at: www.eia.gov/opendata/register.php"));
        } else {
            show_error(result.error);
        }
        return;
    }

    // Both WPSR and STEO return { "success": true, "data": [...] }
    const QJsonArray rows = result.data["data"].toArray();

    if (rows.isEmpty()) {
        show_empty(tr("No data returned for this selection"));
        return;
    }

    // WPSR rows: { date, category, table, symbol, title, value, source }
    // STEO rows: { date, symbol, title, value, units, table, source }
    // A category / table holds dozens of series; they are listed in the SERIES combo and shown one
    // at a time (show_selected_series), so the table is one dated series and the stat cards apply.
    const bool steo = request_id.startsWith(QLatin1String("eia_steo_"));
    const bool full = request_id.startsWith(QLatin1String("eia_wpsrall_"));
    // Title from the request ("eia_<wpsr|wpsrall|steo>_<category>"), not the combo the user may have changed.
    const QString category = request_id.section(QLatin1Char('_'), 2);
    QString label = category;
    for (const auto& c : kWpsrCategories)
        if (c.second == category)
            label = c.first;
    for (const auto& t : kSteoTables)
        if (t.second == category)
            label = t.first;

    // Splitting the rows per series walks every row (~23,500 for five years of the balance sheet):
    // on a worker. The Short-Term Energy Outlook is a forecast — months after the current one are
    // flagged there, so a projected price is never read as a reported one.
    const QString this_month = QDate::currentDate().toString(QStringLiteral("yyyy-MM"));
    const QString forecast_text = tr("EIA forecast");
    const QString history_text = tr("History");
    offload([rows, steo, this_month, forecast_text, history_text]() {
                return split_rows(rows, steo, this_month, forecast_text, history_text);
            },
            [this, label, steo, full, request_id, count = rows.size()](std::shared_ptr<const SeriesSplit> split) {
                split_ = std::move(split);
                result_label_ = label;
                result_is_steo_ = steo;
                result_is_full_ = full;
                {
                    const QSignalBlocker block(series_combo_);
                    const QString previous = series_combo_->currentData().toString();
                    series_combo_->clear();
                    for (const auto& sym : std::as_const(split_->symbols)) {
                        const QString title = split_->titles.value(sym);
                        series_combo_->addItem(title, sym);
                        series_combo_->setItemData(series_combo_->count() - 1, sym + QStringLiteral(" — ") + title,
                                                   Qt::ToolTipRole);
                    }
                    const int keep = series_combo_->findData(previous);
                    series_combo_->setCurrentIndex(keep >= 0 ? keep : 0);
                    reapply_pending_inputs(); // a series remembered from the last session
                    series_combo_->setEnabled(series_combo_->count() > 1);
                }
                show_selected_series();
                LOG_DEBUG("EiaPanel", QString("Loaded %1 rows / %2 series — %3")
                                          .arg(count)
                                          .arg(split_->symbols.size())
                                          .arg(request_id));
            });
}

std::shared_ptr<const EiaPanel::SeriesSplit> EiaPanel::split_rows(const QJsonArray& rows, bool steo,
                                                                  const QString& this_month,
                                                                  const QString& forecast_text,
                                                                  const QString& history_text) {
    auto split = std::make_shared<SeriesSplit>();
    for (const auto& rv : rows) {
        QJsonObject r = rv.toObject();
        if (steo)
            r.insert(QStringLiteral("status"),
                     r.value(QStringLiteral("date")).toString().left(7) > this_month ? forecast_text : history_text);
        const QString sym = r.value(QStringLiteral("symbol")).toString();
        if (sym.isEmpty()) {
            split->unkeyed.append(r);
            continue;
        }
        auto it = split->rows.find(sym);
        if (it == split->rows.end()) {
            // Distinct series in order of first appearance (symbol → its title and units).
            split->symbols << sym;
            const QString t = r.value(QStringLiteral("title")).toString().simplified();
            split->titles.insert(sym, t.isEmpty() ? sym : t);
            it = split->rows.insert(sym, QJsonArray());
        }
        if (!split->units.contains(sym) || split->units.value(sym).isEmpty())
            split->units.insert(sym, r.value(QStringLiteral("units")).toString());
        it->append(r);
    }
    if (!split->symbols.isEmpty())
        split->unkeyed = QJsonArray(); // shown only when no row names a series
    return split;
}

void EiaPanel::show_selected_series() {
    if (!split_)
        return;
    const QString sym = series_combo_ ? series_combo_->currentData().toString() : QString();
    // Rows without symbols are shown as they came; otherwise the picked series (an O(1) lookup).
    const QJsonArray rows = sym.isEmpty() ? split_->unkeyed : split_->rows.value(sym);
    const QString units = sym.isEmpty() ? QString() : split_->units.value(sym);

    SeriesMeta meta;
    if (result_is_steo_) {
        const QString this_month = QDate::currentDate().toString(QStringLiteral("yyyy-MM"));
        meta.note = tr("STEO: months after %1 are EIA forecasts").arg(this_month);
        meta.source = tr("U.S. EIA — Short-Term Energy Outlook");
        // Forecast months stay in the table but never become LATEST.
        meta.stats_cutoff = QDate(QDate::currentDate().year(), QDate::currentDate().month(), 1).addMonths(1).addDays(-1);
    } else {
        meta.source = tr("U.S. EIA — Weekly Petroleum Status Report");
        meta.frequency = tr("Weekly");
        meta.note = result_is_full_ ? tr("Last five years") : tr("Latest %n report week(s)", "", kEiaDefaultWeeks);
    }
    meta.units = units;
    meta.value_column = QStringLiteral("value");
    set_series_meta(meta);
    const QString series_title = series_combo_ && !sym.isEmpty() ? series_combo_->currentText() : QString();
    display(rows, "EIA: " + result_label_ + (series_title.isEmpty() ? QString() : QStringLiteral(" — ") + series_title),
            {QStringLiteral("date"), QStringLiteral("value"), QStringLiteral("status"), QStringLiteral("symbol")});
}

// ── i18n ──────────────────────────────────────────────────────────────────────

void EiaPanel::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange)
        retranslateUi();
    EconPanelBase::changeEvent(event);
}

void EiaPanel::retranslateUi() {
    if (source_lbl_)
        source_lbl_->setText(tr("SOURCE"));
    if (category_lbl_)
        category_lbl_->setText(tr("CATEGORY"));
    if (series_lbl_)
        series_lbl_->setText(tr("SERIES"));
    if (series_combo_)
        series_combo_->setToolTip(tr("Series of the fetched category — shown one at a time"));
    if (full_history_) {
        full_history_->setText(tr("FULL HISTORY"));
        full_history_->setToolTip(tr("Load every report week of the last five years instead of the latest %1 weeks "
                                     "(the balance sheet is then about 23,500 rows)")
                                      .arg(kEiaDefaultWeeks));
        full_history_->setAccessibleName(tr("Load the full five-year history"));
    }
    if (source_combo_ && source_combo_->count() >= 2) {
        source_combo_->setItemText(0, tr("Weekly Petroleum (WPSR)"));
        source_combo_->setItemText(1, tr("Short-Term Outlook (STEO)"));
    }
    // API-key notice depends on the active source.
    if (apikey_notice_ && source_combo_)
        apikey_notice_->setText(source_combo_->currentIndex() == 0 ? tr("No API key needed")
                                                                   : tr("Requires EIA_API_KEY"));
    EconPanelBase::retranslateUi();
}

} // namespace fincept::screens
