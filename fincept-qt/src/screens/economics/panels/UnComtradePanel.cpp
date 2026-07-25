// src/screens/economics/panels/UnComtradePanel.cpp
#include "screens/economics/panels/UnComtradePanel.h"

#include "core/logging/Logger.h"
#include "services/economics/EconomicsService.h"

#include <QDate>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonObject>
#include <QLabel>
#include <QVBoxLayout>

namespace fincept::screens {
namespace {

static constexpr const char* kUnComtradeScript = "un_comtrade_data.py";
static constexpr const char* kUnComtradeSourceId = "un_comtrade";
static constexpr const char* kUnComtradeColor = "#0891B2"; // cyan
} // namespace

// ISO numeric reporter codes for major economies
static const QList<QPair<QString, QString>> kReporters = {
    {"United States", "842"}, {"China", "156"},  {"Germany", "276"}, {"Japan", "392"},  {"United Kingdom", "826"},
    {"France", "251"},        {"India", "699"},  {"Brazil", "76"},   {"Canada", "124"}, {"South Korea", "410"},
    {"Australia", "36"},      {"Russia", "643"}, {"Mexico", "484"},  {"World", "0"},
};

UnComtradePanel::UnComtradePanel(QWidget* parent) : EconPanelBase(kUnComtradeSourceId, kUnComtradeColor, parent) {
    build_base_ui(this);
    connect(&services::EconomicsService::instance(), &services::EconomicsService::result_ready, this,
            &UnComtradePanel::on_result);
}

void UnComtradePanel::activate() {
    show_empty(tr("Select reporter, flow, year and commodity level, then click FETCH\n"
                  "Source: UN Comtrade — annual (A) merchandise trade, HS classification\n"
                  "Values are current US dollars. Free tier: up to 500 records per request;\n"
                  "set UN_COMTRADE_API_KEY for 100,000 records/call (register at comtradedeveloper.un.org)"));
}

void UnComtradePanel::build_controls(QHBoxLayout* thl) {
    auto lbl = [](const QString& t) {
        auto* l = new QLabel(t);
        l->setStyleSheet(ctrl_label_style());
        return l;
    };

    reporter_combo_ = new QComboBox;
    for (const auto& p : kReporters)
        reporter_combo_->addItem(p.first, p.second);
    reporter_combo_->setFixedHeight(26);

    flow_combo_ = new QComboBox;
    flow_combo_->addItem(tr("Exports"), "X");
    flow_combo_->addItem(tr("Imports"), "M");
    flow_combo_->addItem(tr("Re-exports"), "RX");
    flow_combo_->addItem(tr("Re-imports"), "RM");
    flow_combo_->setFixedHeight(26);

    // Last 5 years
    period_combo_ = new QComboBox;
    int cur_year = QDate::currentDate().year();
    for (int y = cur_year; y >= cur_year - 5; --y)
        period_combo_->addItem(QString::number(y), QString::number(y));
    period_combo_->setFixedHeight(26);

    cmd_combo_ = new QComboBox;
    cmd_combo_->addItem(tr("Total (all commodities)"), "TOTAL");
    cmd_combo_->addItem(tr("HS 2-digit chapters"), "AG2");
    cmd_combo_->setFixedHeight(26);

    thl->addWidget(reporter_lbl_ = lbl(tr("REPORTER")));
    thl->addWidget(reporter_combo_);
    thl->addWidget(flow_lbl_ = lbl(tr("FLOW")));
    thl->addWidget(flow_combo_);
    thl->addWidget(year_lbl_ = lbl(tr("YEAR")));
    thl->addWidget(period_combo_);
    // The commodity selector used to be built but never added to the toolbar —
    // an unreachable, un-parented widget that also leaked.
    thl->addWidget(cmd_lbl_ = lbl(tr("COMMODITY")));
    thl->addWidget(cmd_combo_);
}

void UnComtradePanel::on_fetch() {
    const QString reporter = reporter_combo_->currentData().toString();
    const QString period = period_combo_->currentData().toString();
    const QString flow = flow_combo_->currentData().toString();
    const QString cmd_code = cmd_combo_->currentData().toString();

    // Was: `trade_balance <reporter> <period>` — that endpoint takes NO flow
    // argument (so the FLOW selector only changed the title, never the data)
    // and it requires a UN_COMTRADE_API_KEY, so the key-free default path
    // always errored. `trade_data` honours flow + commodity and works on the
    // free preview tier.
    // CLI: trade_data <reporter_code> <period> [flow_code] [cmd_code]
    show_loading(tr("Fetching UN Comtrade: %1 %2 %3…")
                     .arg(reporter_combo_->currentText(), flow_combo_->currentText(), period));
    services::EconomicsService::instance().execute(kUnComtradeSourceId, kUnComtradeScript, "trade_data",
                                                   {reporter, period, flow, cmd_code},
                                                   "comtrade_" + reporter + "_" + period + "_" + flow + "_" + cmd_code);
}

void UnComtradePanel::on_result(const QString& request_id, const services::EconomicsResult& result) {
    if (result.source_id != kUnComtradeSourceId)
        return;
    if (!result.success) {
        show_error(result.error);
        return;
    }

    if (request_id.startsWith("comtrade_")) {
        const QJsonArray arr = result.data["data"].toArray();
        if (arr.isEmpty()) {
            show_empty(tr("No trade records returned for this reporter/flow/year.\n"
                          "Free tier is capped at 500 records — set UN_COMTRADE_API_KEY for full access."));
            return;
        }
        // One row per commodity line for a single year is a cross-section, not a
        // time series — the aggregate stat cards would be meaningless.
        set_stats_visible(cmd_combo_->currentData().toString() == QLatin1String("TOTAL"));
        const QString title = "UN Comtrade: " + reporter_combo_->currentText() + " — " +
                              flow_combo_->currentText() + " " + period_combo_->currentText() + " (" +
                              cmd_combo_->currentText() + ")";
        display(arr, title);
        LOG_INFO("UnComtradePanel", QString("Displayed %1 rows").arg(arr.size()));
    }
}

// ── i18n ──────────────────────────────────────────────────────────────────────

void UnComtradePanel::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange)
        retranslateUi();
    EconPanelBase::changeEvent(event);
}

void UnComtradePanel::retranslateUi() {
    if (reporter_lbl_)
        reporter_lbl_->setText(tr("REPORTER"));
    if (flow_lbl_)
        flow_lbl_->setText(tr("FLOW"));
    if (year_lbl_)
        year_lbl_->setText(tr("YEAR"));
    if (cmd_lbl_)
        cmd_lbl_->setText(tr("COMMODITY"));
    if (flow_combo_ && flow_combo_->count() >= 4) {
        flow_combo_->setItemText(0, tr("Exports"));
        flow_combo_->setItemText(1, tr("Imports"));
        flow_combo_->setItemText(2, tr("Re-exports"));
        flow_combo_->setItemText(3, tr("Re-imports"));
    }
    if (cmd_combo_ && cmd_combo_->count() >= 2) {
        cmd_combo_->setItemText(0, tr("Total (all commodities)"));
        cmd_combo_->setItemText(1, tr("HS 2-digit chapters"));
    }
    EconPanelBase::retranslateUi();
}

} // namespace fincept::screens
