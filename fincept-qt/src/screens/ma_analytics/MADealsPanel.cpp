// MADealsPanel.cpp — deal-database query sub-tab builder.
// Method definition split from MAModulePanel.cpp.

#include "screens/ma_analytics/MAModulePanel.h"

#include "services/ma_analytics/MAAnalyticsService.h"
#include "ui/theme/Theme.h"

#include <QComboBox>
#include <QDoubleSpinBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSpinBox>
#include <QTabWidget>
#include <QVBoxLayout>
#include <QWidget>

using namespace fincept::services::ma;

namespace fincept::screens {


// ═══════════════════════════════════════════════════════════════════════════════
// MODULE 3: DEAL DATABASE
// ═══════════════════════════════════════════════════════════════════════════════

QWidget* MAModulePanel::build_deals_panel() {
    auto* w = new QWidget(this);
    auto* vl = new QVBoxLayout(w);
    vl->setContentsMargins(16, 16, 16, 16);
    vl->setSpacing(12);

    // Controls row
    auto* ctl = new QWidget(w);
    auto* ctl_hl = new QHBoxLayout(ctl);
    ctl_hl->setContentsMargins(0, 0, 0, 0);
    ctl_hl->setSpacing(8);

    auto* scan_days = make_int_spin(1, 365, 30, ctl);
    int_inputs_["scan_days"] = scan_days;
    ctl_hl->addWidget(new QLabel("Scan Days:", ctl));
    ctl_hl->addWidget(scan_days);

    auto* scan_btn = make_run_button("SCAN SEC FILINGS", ctl);
    connect(scan_btn, &QPushButton::clicked, this, [this]() {
        status_label_->setText("Scanning SEC EDGAR...");
        MAAnalyticsService::instance().scan_filings(int_inputs_["scan_days"]->value());
    });
    ctl_hl->addWidget(scan_btn);

    auto* load_btn = make_run_button("LOAD ALL DEALS", ctl);
    connect(load_btn, &QPushButton::clicked, this, [this]() {
        status_label_->setText("Loading deals...");
        MAAnalyticsService::instance().get_all_deals();
    });
    ctl_hl->addWidget(load_btn);
    ctl_hl->addStretch();
    vl->addWidget(ctl);

    // Search
    auto* search_box = new QComboBox(w);
    search_box->setEditable(true);
    search_box->setPlaceholderText("Search deals by target, acquirer, or industry...");
    search_box->setStyleSheet(QString("QComboBox { background:%1; color:%2; border:1px solid %3;"
                                      "font-family:%4; font-size:%5px; padding:6px 10px; }")
                                  .arg(ui::colors::BG_RAISED(), ui::colors::TEXT_PRIMARY(), ui::colors::BORDER_MED())
                                  .arg(ui::fonts::DATA_FAMILY)
                                  .arg(ui::fonts::SMALL));
    combo_inputs_["deal_search"] = search_box;
    // Search on enter
    connect(search_box->lineEdit(), &QLineEdit::returnPressed, this, [this]() {
        auto q = combo_inputs_["deal_search"]->currentText();
        if (!q.isEmpty()) {
            status_label_->setText("Searching deals...");
            MAAnalyticsService::instance().search_deals(q);
        }
    });
    vl->addWidget(search_box);

    // Results area
    results_container_ = new QWidget(w);
    results_layout_ = new QVBoxLayout(results_container_);
    results_layout_->setContentsMargins(0, 8, 0, 0);
    results_layout_->setSpacing(8);
    vl->addWidget(results_container_);
    vl->addStretch();

    return w;
}

// ═══════════════════════════════════════════════════════════════════════════════
// MODULE 4: STARTUP VALUATION

} // namespace fincept::screens
