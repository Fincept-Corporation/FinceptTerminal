// src/screens/economics/panels/NberPanel.cpp
// NBER Business Cycle Dating Committee — US recession peaks/troughs.
// Script: nber_data.py | No API key required.
//
// Response shapes (VERIFIED by running the script, 2026-07-25 — the payload key
// differs per command, which is why extract_rows() below is keyed on it):
//   cycles    → { source, url, total_cycles, latest_trough, current_status,
//                 statistics{…}, cycles:[{peak,trough,expansion_months,
//                                          contraction_months,recession}] }   ← 34 rows
//   recession → { start_date, end_date, indicator, source,
//                 data:[{date,recession}] }                                    ← 300 rows
//
// Only these two commands are exposed. nber_data.py also offers macro / cps /
// unemployment / datasets, but those hit the network (unreachable from the
// machine this was written on) or return a catalogue rather than a series, so
// wiring them would have been guesswork. A control that looks wired and renders
// nothing is worse than one that isn't there.
#include "screens/economics/panels/NberPanel.h"

#include "core/logging/Logger.h"
#include "services/economics/EconomicsService.h"

#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonObject>
#include <QLabel>

namespace fincept::screens {
namespace {

static constexpr const char* kNberScript = "nber_data.py";
static constexpr const char* kNberSourceId = "nber";
static constexpr const char* kNberColor = "#7C3AED"; // violet

struct NberDataset {
    QString label;
    QString command;
    QString payload_key; // the array key in the response — differs per command
};

} // namespace

static const QList<NberDataset> kNberDatasets = {
    {"Business Cycles (Peaks & Troughs)", "cycles", "cycles"},
    {"Recession Indicator (Monthly)", "recession", "data"},
};

NberPanel::NberPanel(QWidget* parent) : EconPanelBase(kNberSourceId, kNberColor, parent) {
    build_base_ui(this);
    connect(&services::EconomicsService::instance(), &services::EconomicsService::result_ready, this,
            &NberPanel::on_result);
}

void NberPanel::activate() {
    show_empty(tr("Select a dataset and click FETCH\n"
                  "Source: NBER Business Cycle Dating Committee (nber.org)\n"
                  "No API key and no network required — the committee's dates ship with the app"));
}

void NberPanel::build_controls(QHBoxLayout* thl) {
    dataset_lbl_ = new QLabel(tr("DATASET"));
    // objectName instead of setStyleSheet(ctrl_label_style()): the rule lives in
    // the container stylesheet EconPanelBase already applies, so this label
    // costs no extra CSS parse. The other 30 panels still use the inline form;
    // migrate them as they're touched.
    dataset_lbl_->setObjectName(QStringLiteral("econCtrlLabel"));

    dataset_combo_ = new QComboBox;
    for (const auto& d : kNberDatasets)
        dataset_combo_->addItem(d.label, d.command);
    dataset_combo_->setFixedHeight(26);
    dataset_combo_->setMinimumWidth(260);
    dataset_combo_->setAccessibleName(tr("NBER dataset"));

    thl->addWidget(dataset_lbl_);
    thl->addWidget(dataset_combo_);
}

void NberPanel::on_fetch() {
    const int idx = dataset_combo_->currentIndex();
    if (idx < 0 || idx >= kNberDatasets.size())
        return;
    const auto& ds = kNberDatasets[idx];

    show_loading(tr("Fetching NBER: %1…").arg(ds.label));

    // EconomicsService::execute() prepends `command` to the argv — pass only
    // extra positional args here (these two commands take none). Getting this
    // wrong is not hypothetical: four economics panels shipped with the command
    // duplicated, which shifted every subsequent argument and made the script
    // answer a completely different query.
    services::EconomicsService::instance().execute(kNberSourceId, kNberScript, ds.command, {}, "nber_" + ds.command);
}

void NberPanel::on_result(const QString& request_id, const services::EconomicsResult& result) {
    if (result.source_id != kNberSourceId)
        return;
    if (!request_id.startsWith("nber_"))
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

    const int idx = dataset_combo_->currentIndex();
    if (idx < 0 || idx >= kNberDatasets.size())
        return;
    const auto& ds = kNberDatasets[idx];

    // nber_data.py emits no "success" key and no uniform "data" key — `cycles`
    // puts its rows under "cycles", `recession` under "data". Read the key this
    // dataset declares, then fall back so a future command still renders
    // something rather than silently showing "no data".
    QJsonArray rows = result.data[ds.payload_key].toArray();
    if (rows.isEmpty())
        rows = result.data["data"].toArray();

    if (rows.isEmpty()) {
        show_error(tr("No data returned"));
        return;
    }

    QString title = QStringLiteral("NBER: ") + ds.label;
    // The cycles response carries useful context the table itself cannot show.
    const QString status = result.data["current_status"].toString();
    const QString latest_trough = result.data["latest_trough"].toString();
    if (!status.isEmpty())
        title += QStringLiteral(" — ") + status;
    if (!latest_trough.isEmpty())
        title += tr(" (last trough %1)").arg(latest_trough);

    display(rows, title);
    LOG_INFO("NberPanel", QString("Displayed %1 rows: %2").arg(rows.size()).arg(ds.label));
}

// ── i18n ──────────────────────────────────────────────────────────────────────

void NberPanel::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange)
        retranslateUi();
    EconPanelBase::changeEvent(event);
}

void NberPanel::retranslateUi() {
    if (dataset_lbl_)
        dataset_lbl_->setText(tr("DATASET"));
    if (dataset_combo_) {
        const int cur = dataset_combo_->currentIndex();
        for (int i = 0; i < kNberDatasets.size() && i < dataset_combo_->count(); ++i)
            dataset_combo_->setItemText(i, kNberDatasets[i].label);
        dataset_combo_->setCurrentIndex(cur);
        dataset_combo_->setAccessibleName(tr("NBER dataset"));
    }
    EconPanelBase::retranslateUi();
}

} // namespace fincept::screens
