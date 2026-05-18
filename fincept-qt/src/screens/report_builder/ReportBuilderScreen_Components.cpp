// src/screens/report_builder/ReportBuilderScreen_Components.cpp
//
// Side-panel toggles, component CRUD slots (add / select / remove / duplicate /
// move), service-event observers, and canvas/structure refresh helpers.
//
// Part of the partial-class split of ReportBuilderScreen.cpp.

#include "screens/report_builder/ReportBuilderScreen.h"

#include "core/session/ScreenStateManager.h"
#include "datahub/DataHub.h"
#include "datahub/DataHubMetaTypes.h"
#include "services/file_manager/FileManagerService.h"
#include "services/markets/MarketDataService.h"
#include "services/report_builder/ReportBuilderService.h"
#include "ui/theme/Theme.h"

#include <QComboBox>
#include <QDateTime>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QListWidgetItem>
#include <QMessageBox>
#include <QPageLayout>
#include <QPageSize>
#include <QPainter>
#include <QPointer>
#include <QPrintPreviewDialog>
#include <QPrinter>
#include <QPropertyAnimation>
#include <QPushButton>
#include <QShowEvent>
#include <QHideEvent>
#include <QTextDocument>
#include <QTextFrame>
#include <QVBoxLayout>

namespace fincept::screens {

namespace rep = ::fincept::report;
using Service = ::fincept::services::ReportBuilderService;

void ReportBuilderScreen::on_toggle_left() { apply_left_collapsed(!left_collapsed_, /*animate=*/true); }
void ReportBuilderScreen::on_toggle_right() { apply_right_collapsed(!right_collapsed_, /*animate=*/true); }

void ReportBuilderScreen::apply_left_collapsed(bool collapsed, bool animate) {
    left_collapsed_ = collapsed;
    if (!comp_toolbar_)
        return;
    const int target = collapsed ? 0 : kLeftPanelWidth;
    if (!left_anim_) {
        left_anim_ = new QPropertyAnimation(comp_toolbar_, "maximumWidth", this);
        left_anim_->setDuration(180);
        left_anim_->setEasingCurve(QEasingCurve::OutCubic);
    }
    left_anim_->stop();
    if (animate) {
        left_anim_->setStartValue(comp_toolbar_->maximumWidth());
        left_anim_->setEndValue(target);
        left_anim_->start();
    } else {
        comp_toolbar_->setMaximumWidth(target);
    }
    if (left_toggle_btn_) {
        left_toggle_btn_->setText(collapsed ? "›" : "‹");
        left_toggle_btn_->setToolTip(collapsed ? "Expand components panel  (Ctrl+B)"
                                               : "Collapse components panel  (Ctrl+B)");
    }
}

void ReportBuilderScreen::apply_right_collapsed(bool collapsed, bool animate) {
    right_collapsed_ = collapsed;
    if (!properties_)
        return;
    const int target = collapsed ? 0 : kRightPanelWidth;
    if (!right_anim_) {
        right_anim_ = new QPropertyAnimation(properties_, "maximumWidth", this);
        right_anim_->setDuration(180);
        right_anim_->setEasingCurve(QEasingCurve::OutCubic);
    }
    right_anim_->stop();
    if (animate) {
        right_anim_->setStartValue(properties_->maximumWidth());
        right_anim_->setEndValue(target);
        right_anim_->start();
    } else {
        properties_->setMaximumWidth(target);
    }
    if (right_toggle_btn_) {
        right_toggle_btn_->setText(collapsed ? "‹" : "›");
        right_toggle_btn_->setToolTip(collapsed ? "Expand properties panel  (Ctrl+Shift+B)"
                                                : "Collapse properties panel  (Ctrl+Shift+B)");
    }
}

// ── User-driven mutations (delegate to service) ──────────────────────────────

void ReportBuilderScreen::add_component(const QString& type) {
    rep::ReportComponent comp;
    comp.type = type;
    if (type == "table") {
        comp.config["rows"] = "3";
        comp.config["cols"] = "3";
    }
    auto& svc = Service::instance();
    int sel_idx = (selected_id_ > 0) ? svc.index_of(selected_id_) : -1;
    int at = (sel_idx >= 0) ? sel_idx + 1 : -1;
    int new_id = svc.add_component(comp, at);
    selected_id_ = new_id;
    select_component(svc.index_of(new_id));
}

void ReportBuilderScreen::select_component(int index) {
    auto comps = Service::instance().components();
    if (index >= 0 && index < comps.size()) {
        selected_id_ = comps[index].id;
        properties_->show_properties(&comps[index], index);
    } else {
        selected_id_ = 0;
        properties_->show_empty();
    }
    refresh_canvas();
    refresh_structure();
}

void ReportBuilderScreen::remove_component_at(int index) {
    auto comps = Service::instance().components();
    if (index < 0 || index >= comps.size())
        return;
    Service::instance().remove_component(comps[index].id);
}

void ReportBuilderScreen::duplicate_at(int index) {
    auto comps = Service::instance().components();
    if (index < 0 || index >= comps.size())
        return;
    rep::ReportComponent copy = comps[index];
    copy.id = 0; // service allocates a fresh id
    int new_id = Service::instance().add_component(copy, index + 1);
    selected_id_ = new_id;
}

void ReportBuilderScreen::move_up_at(int index) {
    auto comps = Service::instance().components();
    if (index <= 0 || index >= comps.size())
        return;
    Service::instance().move_component(comps[index].id, index - 1);
}

void ReportBuilderScreen::move_down_at(int index) {
    auto comps = Service::instance().components();
    if (index < 0 || index >= comps.size() - 1)
        return;
    Service::instance().move_component(comps[index].id, index + 1);
}

// ── Service signal handlers ──────────────────────────────────────────────────

void ReportBuilderScreen::rebind_from_service() {
    auto& svc = Service::instance();
    auto comps = svc.components();
    if (selected_id_ > 0 && svc.index_of(selected_id_) < 0)
        selected_id_ = 0;
    if (selected_id_ == 0 && !comps.isEmpty())
        selected_id_ = comps.first().id;
    refresh_canvas();
    refresh_structure();
    int sel_idx = (selected_id_ > 0) ? svc.index_of(selected_id_) : -1;
    if (sel_idx >= 0)
        properties_->show_properties(&svc.components()[sel_idx], sel_idx);
    else
        properties_->show_empty();
}

void ReportBuilderScreen::on_component_added(int id, int /*index*/) {
    selected_id_ = id;
    auto& svc = Service::instance();
    int sel_idx = svc.index_of(id);
    refresh_canvas();
    refresh_structure();
    if (sel_idx >= 0)
        properties_->show_properties(&svc.components()[sel_idx], sel_idx);
    ScreenStateManager::instance().notify_changed(this);
}

void ReportBuilderScreen::on_component_updated(int id) {
    refresh_canvas();
    refresh_structure();
    if (id == selected_id_) {
        auto& svc = Service::instance();
        int sel_idx = svc.index_of(id);
        if (sel_idx >= 0)
            properties_->show_properties(&svc.components()[sel_idx], sel_idx);
    }
    ScreenStateManager::instance().notify_changed(this);
}

void ReportBuilderScreen::on_component_removed(int id, int /*prior_index*/) {
    if (id == selected_id_) {
        // Pick a sensible neighbour as the new selection.
        auto comps = Service::instance().components();
        selected_id_ = comps.isEmpty() ? 0 : comps.first().id;
    }
    refresh_canvas();
    refresh_structure();
    int sel_idx = (selected_id_ > 0) ? Service::instance().index_of(selected_id_) : -1;
    if (sel_idx >= 0)
        properties_->show_properties(&Service::instance().components()[sel_idx], sel_idx);
    else
        properties_->show_empty();
    ScreenStateManager::instance().notify_changed(this);
}

void ReportBuilderScreen::on_component_moved(int /*id*/, int /*from*/, int /*to*/) {
    refresh_canvas();
    refresh_structure();
    ScreenStateManager::instance().notify_changed(this);
}

void ReportBuilderScreen::on_metadata_changed() {
    refresh_canvas();
    ScreenStateManager::instance().notify_changed(this);
}

void ReportBuilderScreen::on_theme_changed() {
    refresh_canvas();
    ScreenStateManager::instance().notify_changed(this);
}

void ReportBuilderScreen::on_document_changed() {
    // Coarse signal — only refresh structure (canvas is repainted by the
    // fine-grained handlers that fire alongside).
    refresh_structure();
}

void ReportBuilderScreen::on_document_loaded(const QString& /*path*/) {
    selected_id_ = 0;
    rebind_from_service();
}

// ── Refresh ──────────────────────────────────────────────────────────────────

void ReportBuilderScreen::refresh_canvas() {
    auto& svc = Service::instance();
    auto comps = svc.components();
    int sel_idx = (selected_id_ > 0) ? svc.index_of(selected_id_) : -1;
    canvas_->render(comps, svc.metadata(), svc.theme(), sel_idx);
}

void ReportBuilderScreen::refresh_structure() {
    auto comps = Service::instance().components();
    QStringList items;
    for (const auto& c : comps) {
        QString label = c.type.toUpper();
        if (!c.content.isEmpty())
            label += ": " + c.content.left(28);
        else if (c.type == "market_data")
            label += ": " + c.config.value("symbol", "?");
        else if (c.type == "chart")
            label += ": " + c.config.value("title", "Chart");
        else if (c.type == "stats_block")
            label += ": " + c.config.value("title", "Stats");
        else if (c.type == "sparkline")
            label += ": " + c.config.value("title", "");
        else if (c.type == "callout")
            label += " [" + c.config.value("style", "info") + "]";
        items << label;
    }
    int sel_idx = (selected_id_ > 0) ? Service::instance().index_of(selected_id_) : -1;
    comp_toolbar_->update_structure(items, sel_idx);
}

// ── Dialogs ──────────────────────────────────────────────────────────────────

} // namespace fincept::screens
