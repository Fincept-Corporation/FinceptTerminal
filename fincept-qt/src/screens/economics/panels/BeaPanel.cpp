// src/screens/economics/panels/BeaPanel.cpp
// BEA (Bureau of Economic Analysis) — requires BEA_API_KEY env var.
//
// Uses the frontend integration command:
//   fetch <indicator_id> <start_year>-01-01 <end_year>-12-31
//   Response: { "success": true,
//               "data": [{"date": "2022", "value": 123.4}],
//               "metadata": {"indicator": "gdp_growth", "indicator_name": "Real GDP Growth...",
//                            "country": "United States", "source": "BEA NIPA",
//                            "table": "T10101", "line": "1"} }
//
// 27 indicators mapped across 5 categories:
//   GDP: gdp_growth, nominal_gdp, real_gdp, gdp_deflator, gdp_per_capita, gdi
//   Consumption: pce, pce_goods, pce_services, gross_investment, fixed_investment
//   Trade: net_exports, exports, imports, current_account
//   Income: personal_income, compensation, wages_salaries, disposable_income,
//           personal_saving, saving_rate
//   Government: govt_spending, federal_spending, defense_spending,
//               govt_receipts, personal_taxes, corporate_taxes, gross_saving
#include "screens/economics/panels/BeaPanel.h"

#include "core/logging/Logger.h"
#include "services/economics/EconomicsService.h"

#include <QDate>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonObject>
#include <QLabel>
#include <QSplitter>
#include <QVBoxLayout>

namespace fincept::screens {

static constexpr const char* kScript   = "bea_data.py";
static constexpr const char* kSourceId = "bea";
static constexpr const char* kColor    = "#E65100";  // deep orange

struct BeaIndicatorDef { QString name; QString id; QString unit; };

// ── GDP & Output ──────────────────────────────────────────────────────────────
static const QList<BeaIndicatorDef> kGdpIndicators = {
    {"Real GDP Growth (% Change)",          "gdp_growth",     "% change"},
    {"Nominal GDP (Billions $)",            "nominal_gdp",    "Billions $"},
    {"Real GDP (Chained 2017 $)",           "real_gdp",       "Billions $"},
    {"GDP Price Index",                     "gdp_deflator",   "Index"},
    {"GDP Price Change (%)",                "gdp_price_change","% change"},
    {"GDP per Capita (Current $)",          "gdp_per_capita", "$ per capita"},
    {"Gross Domestic Income (Billions $)",  "gdi",            "Billions $"},
};

// ── Consumption & Investment ──────────────────────────────────────────────────
static const QList<BeaIndicatorDef> kConsumptionIndicators = {
    {"Personal Consumption Expenditures",   "pce",              "Billions $"},
    {"PCE — Goods",                         "pce_goods",        "Billions $"},
    {"PCE — Services",                      "pce_services",     "Billions $"},
    {"Gross Private Domestic Investment",   "gross_investment", "Billions $"},
    {"Fixed Investment",                    "fixed_investment", "Billions $"},
};

// ── Trade & External ─────────────────────────────────────────────────────────
static const QList<BeaIndicatorDef> kTradeIndicators = {
    {"Net Exports (Billions $)",            "net_exports",     "Billions $"},
    {"Exports of Goods & Services",         "exports",         "Billions $"},
    {"Imports of Goods & Services",         "imports",         "Billions $"},
    {"Current Account Balance",             "current_account", "Billions $"},
};

// ── Income & Saving ───────────────────────────────────────────────────────────
static const QList<BeaIndicatorDef> kIncomeIndicators = {
    {"Personal Income (Billions $)",        "personal_income",    "Billions $"},
    {"Compensation of Employees",           "compensation",       "Billions $"},
    {"Wages and Salaries",                  "wages_salaries",     "Billions $"},
    {"Disposable Personal Income",          "disposable_income",  "Billions $"},
    {"Personal Saving (Billions $)",        "personal_saving",    "Billions $"},
    {"Personal Saving Rate (%)",            "saving_rate",        "%"},
    {"PCE Inflation (% Change)",            "pce_inflation",      "% change"},
    {"Core PCE Inflation (ex F&E)",         "core_pce_inflation", "% change"},
};

// ── Government ────────────────────────────────────────────────────────────────
static const QList<BeaIndicatorDef> kGovtIndicators = {
    {"Government Spending (Billions $)",    "govt_spending",    "Billions $"},
    {"Federal Government Spending",         "federal_spending", "Billions $"},
    {"National Defense Spending",           "defense_spending", "Billions $"},
    {"Government Current Receipts",         "govt_receipts",    "Billions $"},
    {"Personal Current Taxes",              "personal_taxes",   "Billions $"},
    {"Taxes on Corporate Income",           "corporate_taxes",  "Billions $"},
    {"Gross Saving (Billions $)",           "gross_saving",     "Billions $"},
};

struct BeaCategoryDef {
    QString label;
    const QList<BeaIndicatorDef>* indicators;
};

static const QList<BeaCategoryDef> kCategories = {
    {"GDP & Output",          &kGdpIndicators},
    {"Consumption & Investment", &kConsumptionIndicators},
    {"Trade & External",      &kTradeIndicators},
    {"Income & Saving",       &kIncomeIndicators},
    {"Government",            &kGovtIndicators},
};

// ── Constructor ───────────────────────────────────────────────────────────────

BeaPanel::BeaPanel(QWidget* parent)
    : EconPanelBase(kSourceId, kColor, parent) {

    // Outer layout: left selector | right base UI
    auto* main = new QHBoxLayout(this);
    main->setContentsMargins(0, 0, 0, 0);
    main->setSpacing(0);

    // ── Left: category + indicator selector ──────────────────────────────────
    auto* left = new QWidget;
    left->setFixedWidth(210);
    left->setStyleSheet("background:#0a0a0a; border-right:1px solid #1a1a1a;");
    auto* lvl = new QVBoxLayout(left);
    lvl->setContentsMargins(0, 0, 0, 0);
    lvl->setSpacing(0);

    // Category header
    auto* cat_hdr = new QWidget;
    cat_hdr->setStyleSheet("background:#111111; border-bottom:1px solid #1a1a1a;");
    cat_hdr->setFixedHeight(32);
    auto* chl = new QHBoxLayout(cat_hdr);
    chl->setContentsMargins(8, 0, 8, 0);
    auto* cat_lbl = new QLabel("CATEGORY");
    cat_lbl->setStyleSheet(
        "color:#808080; font-size:9px; font-weight:700; background:transparent;");
    category_combo_ = new QComboBox;
    for (const auto& c : kCategories)
        category_combo_->addItem(c.label);
    category_combo_->setFixedHeight(22);
    connect(category_combo_, &QComboBox::currentIndexChanged,
            this, &BeaPanel::on_category_changed);
    chl->addWidget(cat_lbl);
    chl->addWidget(category_combo_, 1);
    lvl->addWidget(cat_hdr);

    // Indicator search
    indicator_search_ = new QLineEdit;
    indicator_search_->setPlaceholderText("Filter indicators…");
    indicator_search_->setStyleSheet(
        "background:#080808; color:#e5e5e5; border:none;"
        "border-bottom:1px solid #1a1a1a; padding:4px 8px; font-size:10px;");
    indicator_search_->setFixedHeight(28);
    connect(indicator_search_, &QLineEdit::textChanged,
            this, &BeaPanel::on_indicator_filter);
    lvl->addWidget(indicator_search_);

    // Indicator list
    indicator_list_ = new QListWidget;
    indicator_list_->setStyleSheet(
        "QListWidget { background:#080808; border:none; outline:none; font-size:10px; }"
        "QListWidget::item { color:#808080; padding:4px 10px;"
        "  border-bottom:1px solid #111111; }"
        "QListWidget::item:hover { color:#e5e5e5; background:#0f0f0f; }"
        QString("QListWidget::item:selected { color:%1;"
        "  background:rgba(230,81,0,0.08);"
        "  border-left:2px solid %1; padding-left:8px; }").arg(kColor));
    indicator_list_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    lvl->addWidget(indicator_list_, 1);

    main->addWidget(left);

    // ── Right: base UI ────────────────────────────────────────────────────────
    auto* right = new QWidget;
    right->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    build_base_ui(right);
    main->addWidget(right, 1);

    // Populate initial indicator list (GDP category)
    on_category_changed(0);

    connect(&services::EconomicsService::instance(),
            &services::EconomicsService::result_ready,
            this, &BeaPanel::on_result);
}

void BeaPanel::activate() {
    show_empty("Select a category and indicator, then click FETCH\n"
               "Requires BEA_API_KEY — free at: www.bea.gov/data/api/register\n"
               "All data is US national accounts (NIPA)");
}

// ── Controls ──────────────────────────────────────────────────────────────────

void BeaPanel::build_controls(QHBoxLayout* thl) {
    auto make_lbl = [](const QString& text) {
        auto* l = new QLabel(text);
        l->setStyleSheet(
            "color:#525252; font-size:9px; font-weight:700; background:transparent;");
        return l;
    };

    start_input_ = new QLineEdit;
    start_input_->setPlaceholderText("Start year");
    start_input_->setText("2000");
    start_input_->setFixedHeight(26);
    start_input_->setFixedWidth(75);

    end_input_ = new QLineEdit;
    end_input_->setPlaceholderText("End year");
    end_input_->setText(QString::number(QDate::currentDate().year()));
    end_input_->setFixedHeight(26);
    end_input_->setFixedWidth(75);

    thl->addWidget(make_lbl("FROM"));
    thl->addWidget(start_input_);
    thl->addWidget(make_lbl("TO"));
    thl->addWidget(end_input_);

    // API key notice
    auto* notice = new QLabel("Requires BEA_API_KEY");
    notice->setStyleSheet(
        "color:#f59e0b; font-size:9px; background:transparent;");
    thl->addWidget(notice);
}

void BeaPanel::on_category_changed(int index) {
    if (index < 0 || index >= kCategories.size()) return;
    indicator_list_->clear();
    indicator_search_->clear();
    current_indicators_.clear();

    const auto& inds = *kCategories[index].indicators;
    for (const auto& ind : inds) {
        current_indicators_.append({ind.name, ind.id, ind.unit});
        auto* item = new QListWidgetItem(
            ind.name + "  [" + ind.unit + "]");
        item->setData(Qt::UserRole, ind.id);
        item->setToolTip(ind.id + " — " + ind.unit);
        indicator_list_->addItem(item);
    }
    if (indicator_list_->count() > 0)
        indicator_list_->setCurrentRow(0);
}

void BeaPanel::on_indicator_filter(const QString& text) {
    for (int i = 0; i < indicator_list_->count(); ++i) {
        auto* item = indicator_list_->item(i);
        item->setHidden(!text.isEmpty() &&
                        !item->text().contains(text, Qt::CaseInsensitive) &&
                        !item->data(Qt::UserRole).toString()
                             .contains(text, Qt::CaseInsensitive));
    }
}

// ── Fetch ─────────────────────────────────────────────────────────────────────

void BeaPanel::on_fetch() {
    const auto* sel = indicator_list_->currentItem();
    if (!sel) {
        show_empty("Select an indicator from the list");
        return;
    }
    const QString indicator_id = sel->data(Qt::UserRole).toString();
    const QString start = start_input_->text().trimmed();
    const QString end   = end_input_->text().trimmed();

    if (start.isEmpty() || end.isEmpty()) {
        show_empty("Enter start and end years");
        return;
    }

    // CLI: fetch <indicator_id> <start_year>-01-01 <end_year>-12-31
    show_loading("Fetching BEA: " + sel->text().split("  [").first() + "…");
    services::EconomicsService::instance().execute(
        kSourceId, kScript, "fetch",
        {indicator_id, start + "-01-01", end + "-12-31"},
        "bea_fetch_" + indicator_id);
}

// ── Result ────────────────────────────────────────────────────────────────────

void BeaPanel::on_result(const QString& request_id,
                         const services::EconomicsResult& result) {
    if (result.source_id != kSourceId) return;
    if (!request_id.startsWith("bea_fetch_")) return;

    if (!result.success) {
        if (result.error.contains("API key") ||
            result.error.contains("UserID") ||
            result.error.contains("register")) {
            show_error("BEA API key not configured.\n"
                       "Set BEA_API_KEY environment variable.\n"
                       "Free registration at: www.bea.gov/data/api/register");
        } else {
            show_error(result.error);
        }
        return;
    }

    // Response: { "success": true, "data": [{"date": "2022", "value": 123.4}],
    //             "metadata": {"indicator_name": "...", "source": "BEA NIPA"} }
    QJsonArray rows = result.data["data"].toArray();

    if (rows.isEmpty()) {
        show_empty("No data returned — check API key and year range");
        return;
    }

    const QJsonObject meta    = result.data["metadata"].toObject();
    const QString ind_name    = meta["indicator_name"].toString(
        indicator_list_->currentItem()
            ? indicator_list_->currentItem()->text().split("  [").first()
            : "Indicator");

    display(rows, "BEA: " + ind_name);
    LOG_INFO("BeaPanel", QString("Displayed %1 data points for %2")
             .arg(rows.size()).arg(request_id));
}

} // namespace fincept::screens
