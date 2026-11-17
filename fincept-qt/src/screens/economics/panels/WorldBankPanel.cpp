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
    // EN.ATM.CO2E.PC was archived by the World Bank ("indicator not found"); its successor:
    {"CO2 emissions per capita (t CO2e, excl. LULUCF)", "EN.GHG.CO2.PC.CE.AR5"},
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

    auto section_header = [](const QString& text, QLabel*& out) {
        auto* lbl = new QLabel(text);
        lbl->setStyleSheet(section_lbl_style() + section_hdr_style());
        out = lbl;
        return lbl;
    };

    // Country
    lvl->addWidget(section_header(tr("COUNTRY"), country_hdr_));
    country_search_ = new QLineEdit;
    country_search_->setPlaceholderText(tr("Filter countries…"));
    country_search_->setStyleSheet(search_input_style());
    mark_local_input(country_search_); // filters the list: not part of the query
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
    lvl->addWidget(section_header(tr("INDICATOR"), indicator_hdr_));
    indicator_search_ = new QLineEdit;
    indicator_search_->setPlaceholderText(tr("Filter indicators…"));
    indicator_search_->setStyleSheet(search_input_style());
    mark_local_input(indicator_search_); // filters the list: not part of the query
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
    // The selector list only: it loads beside the query, never in the results pane.
    countries_loaded_ = true;
    services::EconomicsService::instance().execute(kWorldBankSourceId, kWorldBankScript, "countries", {},
                                                   "wb_countries");
}

// ── Controls ──────────────────────────────────────────────────────────────────

void WorldBankPanel::build_controls(QHBoxLayout* thl) {
    years_lbl_ = new QLabel(tr("YEARS"));
    years_lbl_->setObjectName(QStringLiteral("econCtrlLabel"));
    date_preset_ = new QComboBox;
    date_preset_->addItem(tr("5 Years"), 5);
    date_preset_->addItem(tr("10 Years"), 10);
    date_preset_->addItem(tr("20 Years"), 20);
    date_preset_->addItem(tr("50 Years"), 50);
    date_preset_->setCurrentIndex(1);
    date_preset_->setMinimumHeight(26);
    thl->addWidget(years_lbl_);
    thl->addWidget(date_preset_);
}

// ── Fetch ─────────────────────────────────────────────────────────────────────

void WorldBankPanel::on_fetch() {
    if (country_list_->count() == 0) {
        // The country list is still loading: query the remembered country (else the United States)
        // instead of waiting for it.
        const QString remembered = pending_input(country_list_).toMap().value(QStringLiteral("d")).toString();
        if (!remembered.isEmpty())
            selected_country_ = remembered;
        if (!countries_loaded_)
            load_countries();
    }
    if (selected_country_.isEmpty())
        selected_country_ = QStringLiteral("US");
    auto* ind_item = indicator_list_->currentItem();
    if (!ind_item) {
        show_empty(tr("Select an indicator from the left panel"));
        return;
    }
    selected_indicator_ = ind_item->data(Qt::UserRole).toString();
    int years = date_preset_->currentData().toInt();
    int end_year = QDate::currentDate().year();
    QString range = QString::number(end_year - years) + ":" + QString::number(end_year);

    show_loading(tr("Fetching World Bank data…"));
    execute_query(kWorldBankSourceId, kWorldBankScript, "indicators",
                  {selected_country_, selected_indicator_, range},
                  "wb_data_" + selected_country_ + "_" + selected_indicator_);
}

// ── Result ────────────────────────────────────────────────────────────────────

void WorldBankPanel::on_result(const QString& request_id, const services::EconomicsResult& result) {
    if (result.source_id != kWorldBankSourceId)
        return;
    if (!result.success) {
        // A failed country list must stay retryable: activate() only loads it while
        // countries_loaded_ is false. It is a selector list — the results pane is not told.
        if (request_id == "wb_countries") {
            countries_loaded_ = false;
            LOG_WARN("WorldBankPanel", logf("Country list failed", {{"error", result.error.left(200)}}));
            return;
        }
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
        // Select the country the query used (the remembered one, else the United States) — the
        // remembered selection is re-applied by reapply_pending_inputs().
        const QString want = selected_country_.isEmpty() ? QStringLiteral("US") : selected_country_;
        for (int i = 0; i < country_list_->count(); ++i) {
            if (country_list_->item(i)->data(Qt::UserRole).toString() == want) {
                country_list_->setCurrentRow(i);
                break;
            }
        }
        reapply_pending_inputs();
        LOG_DEBUG("WorldBankPanel", QString("Loaded %1 countries").arg(arr.size()));
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
        // Label from the request ("wb_data_<ISO2>_<indicator code>"), not the widgets: the user may have
        // picked another country / indicator while this request was in flight.
        const QString tail = request_id.mid(QStringLiteral("wb_data_").size());
        const int cut = static_cast<int>(tail.indexOf(QLatin1Char('_')));
        const QString country = cut > 0 ? tail.left(cut) : selected_country_;
        const QString code = cut > 0 ? tail.mid(cut + 1) : selected_indicator_;
        QString label = code;
        for (const auto& pair : kWbIndicators) {
            if (pair.second == code) {
                label = pair.first;
                break;
            }
        }
        QString country_name = country;
        for (int i = 0; i < country_list_->count(); ++i) {
            if (country_list_->item(i)->data(Qt::UserRole).toString() == country) {
                country_name = country_list_->item(i)->text();
                break;
            }
        }
        SeriesMeta meta;
        meta.frequency = tr("Annual");
        meta.last_updated = result.data["metadata"].toObject()["last_updated"].toString();
        meta.note = code; // the WDI indicator code, for reproducibility
        set_series_meta(meta);
        display(clean, label + " — " + country_name);
        LOG_DEBUG("WorldBankPanel", QString("Displayed %1 data points").arg(clean.size()));
    }
}

// ── Filters ───────────────────────────────────────────────────────────────────

void WorldBankPanel::on_country_filter(const QString& text) {
    filter_list(country_list_, text);
}

void WorldBankPanel::on_indicator_filter(const QString& text) {
    filter_list(indicator_list_, text);
}

// ── i18n ──────────────────────────────────────────────────────────────────────

void WorldBankPanel::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange)
        retranslateUi();
    EconPanelBase::changeEvent(event);
}

void WorldBankPanel::retranslateUi() {
    if (country_hdr_)
        country_hdr_->setText(tr("COUNTRY"));
    if (indicator_hdr_)
        indicator_hdr_->setText(tr("INDICATOR"));
    if (country_search_)
        country_search_->setPlaceholderText(tr("Filter countries…"));
    if (indicator_search_)
        indicator_search_->setPlaceholderText(tr("Filter indicators…"));
    if (years_lbl_)
        years_lbl_->setText(tr("YEARS"));
    if (date_preset_) {
        date_preset_->setItemText(0, tr("5 Years"));
        date_preset_->setItemText(1, tr("10 Years"));
        date_preset_->setItemText(2, tr("20 Years"));
        date_preset_->setItemText(3, tr("50 Years"));
    }
    EconPanelBase::retranslateUi();
}

} // namespace fincept::screens
