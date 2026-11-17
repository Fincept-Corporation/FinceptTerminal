// MADealsPanel.cpp — Deal Database module builder for MAModulePanel.
// Method definition split from MAModulePanel.cpp. The module's content (Deals and Filings sub-tabs, their
// tables, scan with progress / cancel, edit / verify dialog, CSV export) lives in MaDealsWorkspace; this file
// only wires its two pages into the module's lazily built sub-tabs.

#include "screens/ma_analytics/MAModulePanel.h"
#include "screens/ma_analytics/MaDealsWorkspace.h"

#include <QLabel>
#include <QTabWidget>
#include <QVBoxLayout>
#include <QWidget>

namespace fincept::screens {

// ═══════════════════════════════════════════════════════════════════════════════
// MODULE 3: DEAL DATABASE
// ═══════════════════════════════════════════════════════════════════════════════

QWidget* MAModulePanel::build_deals_panel() {
    auto* w = new QWidget(this);
    auto* vl = new QVBoxLayout(w);
    vl->setContentsMargins(16, 16, 16, 16);
    vl->setSpacing(12);

    deals_ws_ = new MaDealsWorkspace(module_.color, [this](const QString& text) {
        if (status_label_)
            status_label_->setText(text);
    }, this);

    sub_tabs_ = new QTabWidget(w);
    apply_tab_stylesheet();
    // The deal table is what the module opens on; the filing browser is built when first shown.
    add_lazy_sub_tab(QT_TR_NOOP("Deals"), [this](QWidget* page) { deals_ws_->build_deals_page(page); });
    add_lazy_sub_tab(QT_TR_NOOP("Filings"), [this](QWidget* page) { deals_ws_->build_filings_page(page); });
    vl->addWidget(sub_tabs_, 1);

    // No shared result view: every reply of this module lands in the workspace's own tables and notes
    // (results_layout_ stays null, so ensure_result_view() builds nothing here).
    return w;
}

} // namespace fincept::screens
