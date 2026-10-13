// src/screens/economics/panels/FredPanel.cpp
#include "screens/economics/panels/FredPanel.h"

#include "core/logging/Logger.h"
#include "screens/economics/panels/EconomicsPresets.h"
#include "services/economics/EconomicsService.h"

#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonObject>
#include <QLabel>
#include <QVBoxLayout>

namespace fincept::screens {
namespace {

static constexpr const char* kFredScript = "fred_data.py";
static constexpr const char* kFredSourceId = "fred";
static constexpr const char* kFredColor = "#EF4444"; // red
} // namespace

FredPanel::FredPanel(QWidget* parent) : EconPanelBase(kFredSourceId, kFredColor, parent) {
    build_base_ui(this);
    connect(&services::EconomicsService::instance(), &services::EconomicsService::result_ready, this,
            &FredPanel::on_result);
}

void FredPanel::activate() {
    show_empty(tr("Set FRED_API_KEY environment variable, then select a series and click FETCH\n"
                  "Get a free key at: fred.stlouisfed.org/docs/api/api_key.html"));
}

void FredPanel::build_controls(QHBoxLayout* thl) {
    preset_lbl_ = new QLabel(tr("PRESET"));
    preset_lbl_->setObjectName(QStringLiteral("econCtrlLabel"));

    preset_combo_ = new QComboBox;
    for (const auto& p : fred_presets())
        preset_combo_->addItem(p.label, p.series_id);
    preset_combo_->setFixedHeight(26);
    preset_combo_->setMinimumWidth(200);
    connect(preset_combo_, &QComboBox::currentIndexChanged, this, [this](int idx) {
        const QString code = preset_combo_->itemData(idx).toString();
        if (!code.isEmpty())
            series_input_->setText(code);
    });

    series_lbl_ = new QLabel(tr("SERIES ID"));
    series_lbl_->setObjectName(QStringLiteral("econCtrlLabel"));

    series_input_ = new QLineEdit;
    series_input_->setPlaceholderText(tr("e.g. GDPC1"));
    series_input_->setFixedHeight(26);
    series_input_->setFixedWidth(100);

    thl->addWidget(preset_lbl_);
    thl->addWidget(preset_combo_);
    thl->addWidget(series_lbl_);
    thl->addWidget(series_input_);
}

void FredPanel::on_fetch() {
    QString series = series_input_->text().trimmed().toUpper();
    if (series.isEmpty()) {
        const QString code = preset_combo_->currentData().toString();
        if (code.isEmpty()) {
            show_empty(tr("Enter a FRED series ID"));
            return;
        }
        series = code;
    }
    show_loading(tr("Fetching FRED series %1…").arg(series));
    services::EconomicsService::instance().execute(kFredSourceId, kFredScript, "series", {series}, "fred_" + series);
}

void FredPanel::on_result(const QString& request_id, const services::EconomicsResult& result) {
    if (result.source_id != kFredSourceId)
        return;
    if (!result.success) {
        // EconomicsService prefixes errors with "[CODE] " when the script
        // returns a structured error_code. Branch on those for friendly UX.
        if (result.error.startsWith("[MISSING_API_KEY]")) {
            show_error(tr("FRED API key not configured.\n"
                          "Set FRED_API_KEY environment variable.\n"
                          "Free key at: fred.stlouisfed.org/docs/api/api_key.html"));
        } else if (result.error.startsWith("[INVALID_API_KEY]")) {
            show_error(tr("FRED rejected your API key.\n"
                          "Check FRED_API_KEY — re-issue one at:\n"
                          "fred.stlouisfed.org/docs/api/api_key.html"));
        } else if (result.error.startsWith("[RATE_LIMITED]")) {
            show_error(tr("FRED rate-limit hit. Try again in a moment.\n") +
                       result.error.mid(QStringLiteral("[RATE_LIMITED] ").size()));
        } else if (result.error.startsWith("[TIMEOUT]")) {
            show_error(tr("FRED request timed out — check your network and retry."));
        } else if (result.error.contains("API key") || result.error.contains("api_key")) {
            // Legacy un-coded message fallback
            show_error(tr("FRED API key not configured.\n"
                          "Set FRED_API_KEY environment variable.\n"
                          "Free key at: fred.stlouisfed.org/docs/api/api_key.html"));
        } else {
            show_error(result.error);
        }
        return;
    }

    if (request_id.startsWith("fred_")) {
        // fred_data.py: {series_id, title, units, frequency, seasonal_adjustment, last_updated,
        //                observations:[{date, value}]} — metadata missing when FRED's /series call failed.
        QJsonArray obs = result.data["observations"].toArray();
        if (obs.isEmpty())
            obs = result.data["data"].toArray();

        // Convert string values to numbers where possible
        QJsonArray clean;
        for (const auto& v : obs) {
            auto obj = v.toObject();
            // fred_data.py already emits `value` as a JSON number (and drops FRED's "."
            // gaps). toString() on a number is empty, so without this check every
            // observation hit the isEmpty() skip below and the panel showed 0 rows.
            if (obj["value"].isDouble()) {
                clean.append(obj);
                continue;
            }
            const QString val_str = obj["value"].toString();
            if (val_str == "." || val_str.isEmpty())
                continue;
            bool ok = false;
            double d = val_str.toDouble(&ok);
            if (ok)
                obj["value"] = d;
            clean.append(obj);
        }

        const QString series = request_id.mid(5); // strip "fred_"
        // FRED states units / frequency / seasonal adjustment / revision date for every series — show
        // them (the CHANGE card switches to percentage points for "Percent" series).
        auto meta_text = [&result](const char* key) {
            const QString v = result.data.value(QLatin1String(key)).toString().trimmed();
            return v == QLatin1String("N/A") ? QString() : v;
        };
        SeriesMeta meta;
        meta.units = meta_text("units");
        meta.frequency = meta_text("frequency");
        meta.seasonal_adj = meta_text("seasonal_adjustment");
        meta.last_updated = meta_text("last_updated");
        set_series_meta(meta);
        const QString title = meta_text("title");
        display(clean, title.isEmpty() ? "FRED: " + series : QStringLiteral("FRED: %1 (%2)").arg(title, series));
        LOG_INFO("FredPanel", QString("Displayed %1 observations").arg(clean.size()));
    }
}

void FredPanel::restore_panel_state(const QVariantMap& s) {
    if (!s.contains(QStringLiteral("series"))) {
        EconPanelBase::restore_panel_state(s);
        return;
    }
    // Pre-v4.5.1 format. The preset must be applied BEFORE the series text: selecting a preset rewrites
    // the series field, which used to replace a custom series id with the preset's on every restart.
    if (preset_combo_ && s.contains(QStringLiteral("preset")))
        preset_combo_->setCurrentIndex(s.value(QStringLiteral("preset")).toInt());
    if (series_input_)
        series_input_->setText(s.value(QStringLiteral("series")).toString());
}

// ── i18n ──────────────────────────────────────────────────────────────────────

void FredPanel::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange)
        retranslateUi();
    EconPanelBase::changeEvent(event);
}

void FredPanel::retranslateUi() {
    if (preset_lbl_)
        preset_lbl_->setText(tr("PRESET"));
    if (series_lbl_)
        series_lbl_->setText(tr("SERIES ID"));
    if (series_input_)
        series_input_->setPlaceholderText(tr("e.g. GDPC1"));
    EconPanelBase::retranslateUi();
}

} // namespace fincept::screens
