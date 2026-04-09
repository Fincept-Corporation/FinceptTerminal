// src/screens/economics/panels/WorldBankPanel.cpp
#include "screens/economics/panels/WorldBankPanel.h"

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
namespace {

static constexpr const char* kWorldBankScript = "worldbank_data.py";
static constexpr const char* kWorldBankSourceId = "worldbank";
static constexpr const char* kWorldBankColor = "#22C55E";
} // namespace

static const QList<QPair<QString, QString>> kWbIndicators = {
    {"GDP (current US$)", "NY.GDP.MKTP.CD"},
    {"GDP per capita (current US$)", "NY.GDP.PCAP.CD"},
    {"GDP growth (annual %)", "NY.GDP.MKTP.KD.ZG"},
    {"GNI per capita (current US$)", "NY.GNP.PCAP.CD"},
    {"Inflation, CPI (annual %)", "FP.CPI.TOTL.ZG"},
    {"Unemployment (% labor force)", "SL.UEM.TOTL.ZS"},
    {"Population, total", "SP.POP.TOTL"},
    {"Population growth (annual %)", "SP.POP.GROW"},
    {"Life expectancy at birth", "SP.DYN.LE00.IN"},
    {"Exports of goods & services (% GDP)", "NE.EXP.GNFS.ZS"},
    {"Imports of goods & services (% GDP)", "NE.IMP.GNFS.ZS"},
    {"Current account balance (% GDP)", "BN.CAB.XOKA.GD.ZS"},
    {"Government debt (% GDP)", "GC.DOD.TOTL.GD.ZS"},
    {"FDI net inflows (% GDP)", "BX.KLT.DINV.WD.GD.ZS"},
    {"Electric power consumption (kWh/cap)", "EG.USE.ELEC.KH.PC"},
    {"CO2 emissions (metric tons per cap)", "EN.ATM.CO2E.PC"},
    {"Internet users (% population)", "IT.NET.USER.ZS"},
    {"Mobile subscriptions (per 100)", "IT.CEL.SETS.P2"},
    {"Forest area (% land area)", "AG.LND.FRST.ZS"},
    {"Access to electricity (% pop)", "EG.ELC.ACCS.ZS"},
};

// ── Constructor ───────────────────────────────────────────────────────────────

WorldBankPanel::WorldBankPanel(QWidget* parent) : EconPanelBase(kWorldBankSourceId, kWorldBankColor, parent) {

    // Outer layout: left selector | right content (base UI)
    auto* main = new QHBoxLayout(this);
    main->setContentsMargins(0, 0, 0, 0);
    main->setSpacing(0);

    // ── Left: country + indicator selectors ──────────────────────────────────
    auto* left = new QWidget(this);
    left->setFixedWidth(220);
    left->setStyleSheet(sidebar_style());
    auto* lvl = new QVBoxLayout(left);
    lvl->setContentsMargins(0, 0, 0, 0);
    lvl->setSpacing(0);

    auto section_header = [this](const QString& text) {
        auto* lbl = new QLabel(text);
        lbl->setStyleSheet(section_lbl_style() + section_hdr_style());
        return lbl;
    };

    // Country
    lvl->addWidget(section_header("COUNTRY"));
    country_search_ = new QLineEdit;
    country_search_->setPlaceholderText("Filter countries…");
    country_search_->setStyleSheet(search_input_style());
    connect(country_search_, &QLineEdit::textChanged, this, &WorldBankPanel::on_country_filter);
    lvl->addWidget(country_search_);

    country_list_ = new QListWidget;
    country_list_->setStyleSheet(list_style());
    connect(country_list_, &QListWidget::currentItemChanged, this, [this](QListWidgetItem* item) {
        if (item)
            selected_country_ = item->data(Qt::UserRole).toString();
    });
    lvl->addWidget(country_list_, 2);

    // Indicator
    lvl->addWidget(section_header("INDICATOR"));
    indicator_search_ = new QLineEdit;
    indicator_search_->setPlaceholderText("Filter indicators…");
    indicator_search_->setStyleSheet(search_input_style());
    connect(indicator_search_, &QLineEdit::textChanged, this, &WorldBankPanel::on_indicator_filter);
    lvl->addWidget(indicator_search_);

    indicator_list_ = new QListWidget;
    indicator_list_->setStyleSheet(list_style());
    for (const auto& pair : kWbIndicators) {
        auto* item = new QListWidgetItem(pair.first, indicator_list_);
        item->setData(Qt::UserRole, pair.second);
    }
    indicator_list_->setCurrentRow(0);
    selected_indicator_ = kWbIndicators[0].second;
    lvl->addWidget(indicator_list_, 3);

    main->addWidget(left);

    // ── Right: base UI (toolbar + stat cards + table) ─────────────────────────
    auto* right = new QWidget(this);
    main->addWidget(right, 1);
    build_base_ui(right);

    connect(&services::EconomicsService::instance(), &services::EconomicsService::result_ready, this,
            &WorldBankPanel::on_result);
}

// ── Activate ──────────────────────────────────────────────────────────────────

void WorldBankPanel::activate() {
    if (!countries_loaded_)
        load_countries();
}

void WorldBankPanel::load_countries() {
    countries_loaded_ = true;
    show_loading("Loading countries…");
    services::EconomicsService::instance().execute(kWorldBankSourceId, kWorldBankScript, "countries", {},
                                                   "wb_countries");
}

// ── Controls ──────────────────────────────────────────────────────────────────

void WorldBankPanel::build_controls(QHBoxLayout* thl) {
    auto* lbl = new QLabel("YEARS");
    lbl->setStyleSheet(ctrl_label_style());
    date_preset_ = new QComboBox;
    date_preset_->addItem("5 Years", 5);
    date_preset_->addItem("10 Years", 10);
    date_preset_->addItem("20 Years", 20);
    date_preset_->addItem("50 Years", 50);
    date_preset_->setCurrentIndex(1);
    date_preset_->setFixedHeight(26);
    thl->addWidget(lbl);
    thl->addWidget(date_preset_);
}

// ── Fetch ─────────────────────────────────────────────────────────────────────

void WorldBankPanel::on_fetch() {
    if (selected_country_.isEmpty()) {
        show_empty("Select a country from the left panel");
        return;
    }
    auto* ind_item = indicator_list_->currentItem();
    if (!ind_item) {
        show_empty("Select an indicator from the left panel");
        return;
    }
    selected_indicator_ = ind_item->data(Qt::UserRole).toString();
    int years = date_preset_->currentData().toInt();
    int end_year = QDate::currentDate().year();
    QString range = QString::number(end_year - years) + ":" + QString::number(end_year);

    show_loading("Fetching World Bank data…");
    services::EconomicsService::instance().execute(kWorldBankSourceId, kWorldBankScript, "indicators",
                                                   {selected_country_, selected_indicator_, range},
                                                   "wb_data_" + selected_country_ + "_" + selected_indicator_);
}

// ── Result ────────────────────────────────────────────────────────────────────

void WorldBankPanel::on_result(const QString& request_id, const services::EconomicsResult& result) {
    if (result.source_id != kWorldBankSourceId)
        return;
    if (!result.success) {
        show_error(result.error);
        return;
    }

    if (request_id == "wb_countries") {
        const QJsonArray arr = result.data["data"].toArray();
        country_list_->clear();
        for (const auto& v : arr) {
            const auto obj = v.toObject();
            const QString n = obj["name"].toString();
            const QString id = obj["iso2Code"].toString();
            if (n.isEmpty() || id.isEmpty())
                continue;
            if (obj["region"].toString().contains("Aggregates"))
                continue;
            auto* item = new QListWidgetItem(n, country_list_);
            item->setData(Qt::UserRole, id);
        }
        // Default select United States
        const auto hits = country_list_->findItems("United States", Qt::MatchExactly);
        if (!hits.isEmpty()) {
            country_list_->setCurrentItem(hits.first());
            selected_country_ = "US";
        }
        show_empty("Select a country and indicator, then click FETCH");
        LOG_INFO("WorldBankPanel", QString("Loaded %1 countries").arg(arr.size()));
        return;
    }

    if (request_id.startsWith("wb_data_")) {
        // Reverse to chronological order, skip null values
        QJsonArray clean;
        const QJsonArray raw = result.data["data"].toArray();
        for (int i = raw.size() - 1; i >= 0; --i) {
            const auto obj = raw[i].toObject();
            if (!obj["value"].isNull())
                clean.append(obj);
        }
        auto* ind = indicator_list_->currentItem();
        display(clean, (ind ? ind->text() : selected_indicator_) + " — " + selected_country_);
        LOG_INFO("WorldBankPanel", QString("Displayed %1 data points").arg(clean.size()));
    }
}

// ── Filters ───────────────────────────────────────────────────────────────────

void WorldBankPanel::on_country_filter(const QString& text) {
    for (int i = 0; i < country_list_->count(); ++i) {
        auto* item = country_list_->item(i);
        item->setHidden(!item->text().contains(text, Qt::CaseInsensitive));
    }
}

void WorldBankPanel::on_indicator_filter(const QString& text) {
    for (int i = 0; i < indicator_list_->count(); ++i) {
        auto* item = indicator_list_->item(i);
        item->setHidden(!item->text().contains(text, Qt::CaseInsensitive));
    }
}

} // namespace fincept::screens
