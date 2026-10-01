// src/screens/economics/panels/BisPanel.cpp
// BIS SDMX API — statistical domains (dataflows), no API key required.
// Command: fetch_series <dataflow> <country> [start_year] [end_year]
//          (WS_CBPOL, WS_EER, WS_XRU, WS_TC, WS_DSR, WS_CREDIT_GAP, WS_SPP, WS_LONG_CPI)
// Response: { "success": true, "data": [...], "metadata": {...} }
// data[] rows: { "date": "YYYY-MM", "value": 1.23, "series": "" } — `series` is empty when the
// dataflow holds a single series, otherwise the series key (several series are never averaged).
// The older get_* commands return raw SDMX-JSON (no flat rows) and some target dataflows BIS has
// retired (WS_LTINT / WS_STINT / WS_CRD / WS_HP), so they are not used here.
#include "screens/economics/panels/BisPanel.h"

#include "core/logging/Logger.h"
#include "services/economics/EconomicsService.h"

#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonObject>
#include <QLabel>
#include <QSet>
#include <QVBoxLayout>

namespace fincept::screens {
namespace {

static constexpr const char* kBisScript = "bis_data.py";
static constexpr const char* kBisSourceId = "bis";
static constexpr const char* kBisColor = "#9D4EDD"; // purple
} // namespace

// Dataset combo entries: { display label, BIS dataflow, placeholder country hint, default country }
struct BisDataset {
    QString label;
    QString flow;
    QString country_hint;
    QString default_country;
};

static const QList<BisDataset> kBisDatasets = {
    {"Central Bank Policy Rates", "WS_CBPOL", "e.g. US, GB, JP, DE", "US"},
    {"Effective Exchange Rates (nominal, broad)", "WS_EER", "e.g. US, GB, JP", "US"},
    {"Exchange Rates vs USD", "WS_XRU", "e.g. GB, JP, DE, AU", "GB"},
    {"Total Credit to Non-Financial Sector", "WS_TC", "e.g. US, CN, JP", "US"},
    {"Debt Service Ratios", "WS_DSR", "e.g. US, GB, DE", "US"},
    {"Credit-to-GDP Gaps", "WS_CREDIT_GAP", "e.g. US, CN, JP", "US"},
    {"Selected Property Prices", "WS_SPP", "e.g. US, GB, DE, AU", "US"},
    {"Long Consumer Price Series", "WS_LONG_CPI", "e.g. US, GB, DE", "US"},
};

// ── Constructor ───────────────────────────────────────────────────────────────

BisPanel::BisPanel(QWidget* parent) : EconPanelBase(kBisSourceId, kBisColor, parent) {
    build_base_ui(this);
    connect(&services::EconomicsService::instance(), &services::EconomicsService::result_ready, this,
            &BisPanel::on_result);
}

void BisPanel::activate() {
    show_empty(tr("Select a dataset and country code, then click FETCH\n"
                  "BIS data is free — no API key required"));
}

// ── Controls ──────────────────────────────────────────────────────────────────

void BisPanel::build_controls(QHBoxLayout* thl) {
    auto make_lbl = [](const QString& text) {
        auto* l = new QLabel(text);
        l->setStyleSheet(ctrl_label_style());
        return l;
    };

    dataset_combo_ = new QComboBox;
    for (const auto& d : kBisDatasets)
        dataset_combo_->addItem(d.label);
    dataset_combo_->setFixedHeight(26);
    dataset_combo_->setMinimumWidth(230);
    connect(dataset_combo_, &QComboBox::currentIndexChanged, this, &BisPanel::on_dataset_changed);

    country_input_ = new QLineEdit;
    country_input_->setPlaceholderText(tr("Country code"));
    country_input_->setText("US");
    country_input_->setFixedHeight(26);
    country_input_->setFixedWidth(70);

    start_input_ = new QLineEdit;
    start_input_->setPlaceholderText(tr("Start year"));
    start_input_->setFixedHeight(26);
    start_input_->setFixedWidth(70);

    end_input_ = new QLineEdit;
    end_input_->setPlaceholderText(tr("End year"));
    end_input_->setFixedHeight(26);
    end_input_->setFixedWidth(70);

    thl->addWidget(dataset_lbl_ = make_lbl(tr("DATASET")));
    thl->addWidget(dataset_combo_);
    thl->addWidget(country_lbl_ = make_lbl(tr("COUNTRY")));
    thl->addWidget(country_input_);
    thl->addWidget(from_lbl_ = make_lbl(tr("FROM")));
    thl->addWidget(start_input_);
    thl->addWidget(to_lbl_ = make_lbl(tr("TO")));
    thl->addWidget(end_input_);
}

void BisPanel::on_dataset_changed(int index) {
    if (index < 0 || index >= kBisDatasets.size())
        return;
    const auto& ds = kBisDatasets[index];
    country_input_->setPlaceholderText(ds.country_hint);
    // If user hasn't typed anything custom, fill in default
    if (country_input_->text().isEmpty() ||
        country_input_->text() == kBisDatasets[qMax(0, index - 1)].default_country) {
        country_input_->setText(ds.default_country);
    }
}

// ── Fetch ─────────────────────────────────────────────────────────────────────

void BisPanel::on_fetch() {
    const int idx = dataset_combo_->currentIndex();
    if (idx < 0 || idx >= kBisDatasets.size())
        return;
    const auto& ds = kBisDatasets[idx];

    const QString country = country_input_->text().trimmed().toUpper();
    const QString start = start_input_->text().trimmed();
    const QString end = end_input_->text().trimmed();

    if (country.isEmpty()) {
        show_empty(tr("Enter a country code (e.g. US, GB, JP)"));
        return;
    }

    // CLI: fetch_series <dataflow> <country> [start_year] [end_year]
    QStringList args{ds.flow, country};
    if (!start.isEmpty())
        args << start;
    if (!start.isEmpty() && !end.isEmpty())
        args << end;

    show_loading(tr("Fetching BIS %1…").arg(ds.label));
    services::EconomicsService::instance().execute(kBisSourceId, kBisScript, "fetch_series", args,
                                                   "bis_" + ds.flow + "_" + country);
}

// ── Result ────────────────────────────────────────────────────────────────────

void BisPanel::on_result(const QString& request_id, const services::EconomicsResult& result) {
    if (result.source_id != kBisSourceId)
        return;

    if (!result.success) {
        show_error(result.error);
        return;
    }

    // BIS response: { "success": true, "data": [...], "metadata": {...} }
    // data[] rows: { "date": "YYYY-MM", "value": 1.23, "series": "" }
    const QJsonArray raw = result.data["data"].toArray();

    // `series` is empty for single-series dataflows — drop the blank column. When a
    // dataflow carries several series (different units/sectors), LATEST/CHANGE/MIN/MAX/AVG
    // would mix unrelated series, so the stat cards are hidden in that case.
    QJsonArray rows;
    QSet<QString> distinct_series;
    for (const auto& rv : raw) {
        QJsonObject row = rv.toObject();
        const QString series = row.value("series").toString();
        if (series.isEmpty())
            row.remove("series");
        else
            distinct_series.insert(series);
        rows.append(row);
    }

    if (rows.isEmpty()) {
        show_empty(tr("No data returned — try a different country or date range"));
        return;
    }

    // Build title from metadata
    const QString title = "BIS: " + dataset_combo_->currentText() + " — " + country_input_->text().trimmed().toUpper();

    set_stats_visible(distinct_series.size() <= 1);
    display(rows, title);
    LOG_INFO("BisPanel", QString("Displayed %1 rows for %2").arg(rows.size()).arg(request_id));
}

// ── i18n ──────────────────────────────────────────────────────────────────────

void BisPanel::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange)
        retranslateUi();
    EconPanelBase::changeEvent(event);
}

void BisPanel::retranslateUi() {
    if (dataset_lbl_)
        dataset_lbl_->setText(tr("DATASET"));
    if (country_lbl_)
        country_lbl_->setText(tr("COUNTRY"));
    if (from_lbl_)
        from_lbl_->setText(tr("FROM"));
    if (to_lbl_)
        to_lbl_->setText(tr("TO"));
    // country_input_ placeholder is replaced with a per-dataset hint (data) on
    // selection, so it is not re-applied here.
    if (start_input_)
        start_input_->setPlaceholderText(tr("Start year"));
    if (end_input_)
        end_input_->setPlaceholderText(tr("End year"));
    EconPanelBase::retranslateUi();
}

} // namespace fincept::screens
