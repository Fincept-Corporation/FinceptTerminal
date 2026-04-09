// src/screens/economics/panels/AdbPanel.cpp
// ADB (Asian Development Bank) — Key Indicators Database (KIDB), SDMX API.
// No API key required. 14 Asia-Pacific economies, 7 dataflow categories.
// Commands: get_gdp, get_population, get_financial, get_trade,
//           get_multiple_indicators
// Response: { "data": [...], "metadata": {...}, "error": null }
// data[] rows (SDMX parsed): { "date": "YYYY", "value": 1.23, "time_period": "YYYY", "series_key": "..." }
#include "screens/economics/panels/AdbPanel.h"

#include "core/logging/Logger.h"
#include "services/economics/EconomicsService.h"

#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonObject>
#include <QLabel>
#include <QVBoxLayout>

namespace fincept::screens {
namespace {

static constexpr const char* kAdbScript = "adb_data.py";
static constexpr const char* kAdbSourceId = "adb";
static constexpr const char* kAdbColor = "#0072BC"; // ADB blue
} // namespace

struct AdbCategory {
    QString label;
    QString command;
    QString default_indicator;
    QString indicator_hint;
    bool uses_indicator;
};

static const QList<AdbCategory> kAdbCategories = {
    {"GDP / National Accounts", "get_gdp", "NGDP_XDC", "e.g. NGDP_XDC, NGDPPC_XDC", true},
    {"Population", "get_population", "LP_PE_NUM_MOP", "e.g. LP_PE_NUM_MOP", true},
    {"Financial Indicators", "get_financial", "", "(all indicators)", false},
    {"Trade Data", "get_trade", "", "(all indicators)", false},
};

static const QList<QPair<QString, QString>> kEconomies = {
    {"Philippines (PHI)", "PHI"}, {"Singapore (SGP)", "SGP"},   {"Japan (JPN)", "JPN"},     {"China (CHN)", "CHN"},
    {"India (IND)", "IND"},       {"Korea (KOR)", "KOR"},       {"Thailand (THA)", "THA"},  {"Malaysia (MYS)", "MYS"},
    {"Indonesia (IDN)", "IDN"},   {"Vietnam (VNM)", "VNM"},     {"Hong Kong (HKG)", "HKG"}, {"Taiwan (TWN)", "TWN"},
    {"Australia (AUS)", "AUS"},   {"New Zealand (NZL)", "NZL"}, {"All Economies", "all"},
};

// Common GDP/National Accounts indicators
static const QList<QPair<QString, QString>> kGdpIndicators = {
    {"GDP, current prices (NGDP_XDC)", "NGDP_XDC"},           {"GDP per capita (NGDPPC_XDC)", "NGDPPC_XDC"},
    {"GDP growth rate (NGDP_R_YOY_PT)", "NGDP_R_YOY_PT"},     {"Gross national income (GNI_XDC)", "GNI_XDC"},
    {"Gross fixed capital formation (GFCF_XDC)", "GFCF_XDC"}, {"Exports of goods & services (EXG_XDC)", "EXG_XDC"},
    {"Imports of goods & services (IMG_XDC)", "IMG_XDC"},     {"Government final consumption (GGFC_XDC)", "GGFC_XDC"},
};

// ── Constructor ───────────────────────────────────────────────────────────────

AdbPanel::AdbPanel(QWidget* parent) : EconPanelBase(kAdbSourceId, kAdbColor, parent) {
    build_base_ui(this);
    connect(&services::EconomicsService::instance(), &services::EconomicsService::result_ready, this,
            &AdbPanel::on_result);
}

void AdbPanel::activate() {
    show_empty("Select an economy and data category, then click FETCH\n"
               "ADB data is free — no API key required");
}

// ── Controls ──────────────────────────────────────────────────────────────────

void AdbPanel::build_controls(QHBoxLayout* thl) {
    auto make_lbl = [this](const QString& text) {
        auto* l = new QLabel(text);
        l->setStyleSheet(ctrl_label_style());
        return l;
    };

    economy_combo_ = new QComboBox;
    for (const auto& e : kEconomies)
        economy_combo_->addItem(e.first, e.second);
    economy_combo_->setFixedHeight(26);
    economy_combo_->setMinimumWidth(165);

    category_combo_ = new QComboBox;
    for (const auto& c : kAdbCategories)
        category_combo_->addItem(c.label);
    category_combo_->setFixedHeight(26);
    category_combo_->setMinimumWidth(185);
    connect(category_combo_, &QComboBox::currentIndexChanged, this, &AdbPanel::on_category_changed);

    indicator_input_ = new QLineEdit;
    indicator_input_->setPlaceholderText(kAdbCategories[0].indicator_hint);
    indicator_input_->setText(kAdbCategories[0].default_indicator);
    indicator_input_->setFixedHeight(26);
    indicator_input_->setFixedWidth(130);

    start_input_ = new QLineEdit;
    start_input_->setPlaceholderText("Start year");
    start_input_->setFixedHeight(26);
    start_input_->setFixedWidth(70);

    end_input_ = new QLineEdit;
    end_input_->setPlaceholderText("End year");
    end_input_->setFixedHeight(26);
    end_input_->setFixedWidth(70);

    thl->addWidget(make_lbl("ECONOMY"));
    thl->addWidget(economy_combo_);
    thl->addWidget(make_lbl("CATEGORY"));
    thl->addWidget(category_combo_);
    thl->addWidget(make_lbl("INDICATOR"));
    thl->addWidget(indicator_input_);
    thl->addWidget(make_lbl("FROM"));
    thl->addWidget(start_input_);
    thl->addWidget(make_lbl("TO"));
    thl->addWidget(end_input_);
}

void AdbPanel::on_category_changed(int index) {
    if (index < 0 || index >= kAdbCategories.size())
        return;
    const auto& cat = kAdbCategories[index];
    indicator_input_->setPlaceholderText(cat.indicator_hint);
    indicator_input_->setText(cat.default_indicator);
    indicator_input_->setEnabled(cat.uses_indicator);
}

// ── Fetch ─────────────────────────────────────────────────────────────────────

void AdbPanel::on_fetch() {
    const int cat_idx = category_combo_->currentIndex();
    if (cat_idx < 0 || cat_idx >= kAdbCategories.size())
        return;
    const auto& cat = kAdbCategories[cat_idx];

    const QString economy = economy_combo_->currentData().toString();
    const QString indicator = indicator_input_->text().trimmed().toUpper();
    const QString start = start_input_->text().trimmed();
    const QString end = end_input_->text().trimmed();

    if (economy.isEmpty()) {
        show_empty("Select an economy");
        return;
    }
    if (cat.uses_indicator && indicator.isEmpty()) {
        show_empty("Enter an indicator code (e.g. " + cat.default_indicator + ")");
        return;
    }

    QStringList args;
    args << economy;
    if (cat.uses_indicator)
        args << indicator;
    if (!start.isEmpty())
        args << start;
    if (!start.isEmpty() && !end.isEmpty())
        args << end;

    show_loading("Fetching ADB " + cat.label + " for " + economy + "…");
    services::EconomicsService::instance().execute(kAdbSourceId, kAdbScript, cat.command, args,
                                                   "adb_" + cat.command + "_" + economy + "_" + indicator);
}

// ── Result ────────────────────────────────────────────────────────────────────

void AdbPanel::on_result(const QString& request_id, const services::EconomicsResult& result) {
    if (result.source_id != kAdbSourceId)
        return;

    if (!result.success) {
        // ADB uses "error" key at top level
        const QString err = result.error.isEmpty() ? result.data["error"].toString() : result.error;
        show_error(err.isEmpty() ? "Unknown error from ADB API" : err);
        return;
    }

    // ADB response: { "data": [...], "metadata": {...}, "error": null }
    // rows: { "date"/"time_period": "YYYY", "value": 1.23, "series_key": "..." }
    QJsonArray rows = result.data["data"].toArray();

    if (rows.isEmpty()) {
        // Check for error embedded in data field
        const QString emb_err = result.data["error"].toString();
        if (!emb_err.isEmpty()) {
            show_error(emb_err);
            return;
        }
        show_empty("No data returned — try a different economy, indicator or date range");
        return;
    }

    const QString economy_label = economy_combo_->currentText();
    const QString cat_label = category_combo_->currentText();
    const QString title = "ADB: " + cat_label + " — " + economy_label;

    display(rows, title);
    LOG_INFO("AdbPanel", QString("Displayed %1 rows for %2").arg(rows.size()).arg(request_id));
}

} // namespace fincept::screens
