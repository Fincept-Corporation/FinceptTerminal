// src/screens/economics/panels/OwIdPanel.cpp
// Our World in Data — CO2, energy, life expectancy, poverty, GDP per capita.
// Script: owid_data.py  |  No API key required.
//
// Response shape: { success, title, country, count, data:[{country, year, <metric cols>}] }
#include "screens/economics/panels/OwIdPanel.h"

#include "core/logging/Logger.h"
#include "services/economics/EconomicsService.h"

#include <QDate>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonObject>
#include <QLabel>

namespace fincept::screens {
namespace {

static constexpr const char* kOwIdScript = "owid_data.py";
static constexpr const char* kOwIdSourceId = "owid";
static constexpr const char* kOwIdColor = "#10B981"; // OWID green

struct OwIdSeries {
    QString label;
    QString command;
    QString default_country;
};

static const QList<OwIdSeries> kOwIdSeries = {
    {"CO2 Emissions", "co2", "United States"}, {"CO2 Per Capita", "co2_per_capita", "United States"},
    {"Energy Consumption", "energy", "China"}, {"Life Expectancy", "life_expectancy", "Japan"},
    {"Poverty Headcount", "poverty", "World"}, {"GDP Per Capita", "gdp_per_capita", "Germany"},
};

} // namespace

OwIdPanel::OwIdPanel(QWidget* parent) : EconPanelBase(kOwIdSourceId, kOwIdColor, parent) {
    build_base_ui(this);
    connect(&services::EconomicsService::instance(), &services::EconomicsService::result_ready, this,
            &OwIdPanel::on_result);
}

void OwIdPanel::activate() {
    // The results pane shows the remembered result or loads the selection on show.
}

void OwIdPanel::build_controls(QHBoxLayout* thl) {
    auto mk_lbl = [](const QString& t) {
        auto* l = new QLabel(t);
        l->setObjectName(QStringLiteral("econCtrlLabel"));
        return l;
    };

    series_combo_ = new QComboBox;
    for (const auto& s : kOwIdSeries)
        series_combo_->addItem(s.label, s.command);
    series_combo_->setMinimumHeight(26);
    series_combo_->setMinimumWidth(180);

    country_edit_ = new QLineEdit("United States");
    country_edit_->setMinimumHeight(26);
    set_text_width(country_edit_, 130);
    country_edit_->setPlaceholderText(tr("Country…"));

    start_edit_ = new QLineEdit("2000");
    start_edit_->setMinimumHeight(26);
    set_text_width(start_edit_, 52);

    // Current year: a fixed "2023" hid every newer observation from the default query.
    end_edit_ = new QLineEdit(QString::number(QDate::currentDate().year()));
    end_edit_->setMinimumHeight(26);
    set_text_width(end_edit_, 52);

    // Update default country when series changes
    connect(series_combo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int idx) {
        if (idx >= 0 && idx < kOwIdSeries.size())
            country_edit_->setText(kOwIdSeries[idx].default_country);
    });

    thl->addWidget(series_lbl_ = mk_lbl(tr("SERIES")));
    thl->addWidget(series_combo_);
    thl->addSpacing(6);
    thl->addWidget(country_lbl_ = mk_lbl(tr("COUNTRY")));
    thl->addWidget(country_edit_);
    thl->addSpacing(6);
    thl->addWidget(from_lbl_ = mk_lbl(tr("FROM")));
    thl->addWidget(start_edit_);
    thl->addWidget(to_lbl_ = mk_lbl(tr("TO")));
    thl->addWidget(end_edit_);
}

void OwIdPanel::on_fetch() {
    const int idx = series_combo_->currentIndex();
    const auto& series = kOwIdSeries[idx];

    const QString country = country_edit_->text().trimmed();
    const QString start = start_edit_->text().trimmed();
    const QString end = end_edit_->text().trimmed();

    if (country.isEmpty()) {
        show_empty(tr("Please enter a country name"));
        return;
    }

    show_loading(tr("Fetching OWID: %1 — %2…").arg(series.label, country));

    execute_query(kOwIdSourceId, kOwIdScript, series.command, {country, start, end},
                  "owid_" + series.command);
}

void OwIdPanel::on_result(const QString& request_id, const services::EconomicsResult& result) {
    if (result.source_id != kOwIdSourceId)
        return;
    if (!request_id.startsWith("owid_"))
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

    // Response: { success, title, country, count, data:[{country, year, <cols>}] }
    QJsonArray rows = result.data["data"].toArray();

    if (rows.isEmpty()) {
        show_error(tr("No data returned — try a different country or year range"));
        return;
    }

    const QString title = result.data["title"].toString();
    const QString country = result.data["country"].toString();
    const QString display_title =
        title.isEmpty()
            ? ("OWID: " + (series_combo_->currentIndex() >= 0 ? kOwIdSeries[series_combo_->currentIndex()].label
                                                              : request_id.mid(5)))
            : (title + (country.isEmpty() || country == "All" ? "" : " — " + country));

    display(rows, display_title);
    LOG_DEBUG("OwIdPanel", QString("Displayed %1 rows: %2").arg(rows.size()).arg(display_title));
}

// ── i18n ──────────────────────────────────────────────────────────────────────

void OwIdPanel::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange)
        retranslateUi();
    EconPanelBase::changeEvent(event);
}

void OwIdPanel::retranslateUi() {
    if (series_lbl_)
        series_lbl_->setText(tr("SERIES"));
    if (country_lbl_)
        country_lbl_->setText(tr("COUNTRY"));
    if (from_lbl_)
        from_lbl_->setText(tr("FROM"));
    if (to_lbl_)
        to_lbl_->setText(tr("TO"));
    if (country_edit_)
        country_edit_->setPlaceholderText(tr("Country…"));
    EconPanelBase::retranslateUi();
}

} // namespace fincept::screens
