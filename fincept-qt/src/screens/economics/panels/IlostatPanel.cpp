// src/screens/economics/panels/IlostatPanel.cpp
// ILO ILOSTAT — unemployment rate, labour force participation, employment-to-population.
// Script: ilostat_data.py  |  No API key required.
//
// Response shape: { success, indicator, dataflow, key, data:[{TIME_PERIOD, OBS_VALUE, REF_AREA, ...}] }
#include "screens/economics/panels/IlostatPanel.h"

#include "core/logging/Logger.h"
#include "services/economics/EconomicsService.h"

#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonObject>
#include <QLabel>

namespace fincept::screens {
namespace {

static constexpr const char* kIlostatScript = "ilostat_data.py";
static constexpr const char* kIlostatSourceId = "ilostat";
static constexpr const char* kIlostatColor = "#3B82F6"; // ILO blue
} // namespace

struct IloSeries {
    QString label;
    QString command;
    QString description;
};

static const QList<IloSeries> kIlostatSeries = {
    {"Unemployment Rate", "unemployment", "UNE_DEAP_SEX_AGE_RT — annual, total, 15+"},
    {"Labour Force Participation Rate", "lfpr", "EAP_DWAP_SEX_AGE_RT — annual, total, 15+"},
    {"Employment-to-Population Ratio", "emp_to_pop", "EMP_DWAP_SEX_AGE_RT — annual, total, 15+"},
};

IlostatPanel::IlostatPanel(QWidget* parent) : EconPanelBase(kIlostatSourceId, kIlostatColor, parent) {
    build_base_ui(this);
    connect(&services::EconomicsService::instance(), &services::EconomicsService::result_ready, this,
            &IlostatPanel::on_result);
}

void IlostatPanel::activate() {
    show_empty("Select a series, enter a country code, then click FETCH\n"
               "Source: ILO ILOSTAT SDMX REST API (sdmx.ilo.org) — no API key required\n"
               "Country codes: USA, GBR, DEU, FRA, IND, CHN, JPN, BRA, ZAF, G7=CAN+USA+GBR+DEU+FRA+ITA+JPN");
}

void IlostatPanel::build_controls(QHBoxLayout* thl) {
    auto mk_lbl = [](const QString& t) {
        auto* l = new QLabel(t);
        l->setStyleSheet(ctrl_label_style());
        return l;
    };

    series_combo_ = new QComboBox;
    for (const auto& s : kIlostatSeries)
        series_combo_->addItem(s.label, s.command);
    series_combo_->setFixedHeight(26);
    series_combo_->setMinimumWidth(220);

    for (int i = 0; i < kIlostatSeries.size(); ++i)
        series_combo_->setItemData(i, kIlostatSeries[i].description, Qt::ToolTipRole);

    country_edit_ = new QLineEdit("USA");
    country_edit_->setFixedHeight(26);
    country_edit_->setFixedWidth(120);
    country_edit_->setPlaceholderText("ISO-2 code…");
    country_edit_->setToolTip("ISO-2 country code, e.g. USA, GBR, DEU\n"
                              "Multiple: CAN+USA+GBR\n"
                              "All countries: ALL");

    start_edit_ = new QLineEdit("2010");
    start_edit_->setFixedHeight(26);
    start_edit_->setFixedWidth(52);

    end_edit_ = new QLineEdit("2023");
    end_edit_->setFixedHeight(26);
    end_edit_->setFixedWidth(52);

    thl->addWidget(mk_lbl("SERIES"));
    thl->addWidget(series_combo_);
    thl->addSpacing(6);
    thl->addWidget(mk_lbl("COUNTRY"));
    thl->addWidget(country_edit_);
    thl->addSpacing(6);
    thl->addWidget(mk_lbl("FROM"));
    thl->addWidget(start_edit_);
    thl->addWidget(mk_lbl("TO"));
    thl->addWidget(end_edit_);
}

void IlostatPanel::on_fetch() {
    const int idx = series_combo_->currentIndex();
    const auto& series = kIlostatSeries[idx];

    const QString country = country_edit_->text().trimmed().toUpper();
    const QString start = start_edit_->text().trimmed();
    const QString end = end_edit_->text().trimmed();

    if (country.isEmpty()) {
        show_error("Please enter a country code (e.g. USA)");
        return;
    }

    show_loading("Fetching ILO: " + series.label + " — " + country + "…");

    services::EconomicsService::instance().execute(kIlostatSourceId, kIlostatScript, series.command,
                                                   {country, "A", "SEX_T", "AGE_YTHADULT_YGE15", start, end},
                                                   "ilostat_" + series.command);
}

void IlostatPanel::on_result(const QString& request_id, const services::EconomicsResult& result) {
    if (result.source_id != kIlostatSourceId)
        return;
    if (!request_id.startsWith("ilostat_"))
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

    // Response: { success, indicator, dataflow, key, data:[{TIME_PERIOD, OBS_VALUE, REF_AREA, ...}] }
    QJsonArray rows = result.data["data"].toArray();

    if (rows.isEmpty()) {
        show_error("No data returned — try a different country code or year range");
        return;
    }

    // Build title
    const QString cmd = request_id.mid(8); // strip "ilostat_"
    QString label;
    for (const auto& s : kIlostatSeries) {
        if (s.command == cmd) {
            label = s.label;
            break;
        }
    }
    const QString country = country_edit_->text().trimmed().toUpper();
    const QString title = "ILO: " + (label.isEmpty() ? cmd : label) + (country.isEmpty() ? "" : " — " + country);

    display(rows, title);
    LOG_INFO("IlostatPanel", QString("Displayed %1 rows: %2").arg(rows.size()).arg(title));
}

} // namespace fincept::screens
