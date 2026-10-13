// src/screens/report_builder/ReportBuilderScreen_Components.cpp
//
// Side-panel toggles, component CRUD slots (add / select / remove / duplicate /
// move), service-event observers, and canvas/structure refresh helpers.
//
// Part of the partial-class split of ReportBuilderScreen.cpp.

#include "core/session/ScreenStateManager.h"
#include "datahub/DataHub.h"
#include "datahub/DataHubMetaTypes.h"
#include "screens/report_builder/ReportBuilderScreen.h"
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
#include <QHideEvent>
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
#include <QTextDocument>
#include <QTextFrame>
#include <QTimer>
#include <QVBoxLayout>

#include <cmath>

namespace fincept::screens {

namespace rep = ::fincept::report;
using Service = ::fincept::services::ReportBuilderService;

void ReportBuilderScreen::on_toggle_left() {
    apply_left_collapsed(!left_collapsed_, /*animate=*/true);
}
void ReportBuilderScreen::on_toggle_right() {
    apply_right_collapsed(!right_collapsed_, /*animate=*/true);
}

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
        left_toggle_btn_->setToolTip(collapsed ? tr("Expand components panel  (Ctrl+B)")
                                               : tr("Collapse components panel  (Ctrl+B)"));
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
        right_toggle_btn_->setToolTip(collapsed ? tr("Expand properties panel  (Ctrl+Shift+B)")
                                                : tr("Collapse properties panel  (Ctrl+Shift+B)"));
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
    // props_editing_: the update came from a keystroke in the properties panel itself;
    // rebuilding it here would delete the editor the user is typing in.
    if (id == selected_id_ && !props_editing_) {
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
    // The properties panel addresses its component by INDEX, and a move changes the index
    // of the selected component (or of the one it swapped with). It was never re-pointed,
    // so after Move Up/Down — or an undo of one — edits typed into the panel landed in the
    // neighbouring component.
    const int sel_idx = (selected_id_ > 0) ? Service::instance().index_of(selected_id_) : -1;
    if (sel_idx >= 0 && !props_editing_)
        properties_->show_properties(&Service::instance().components()[sel_idx], sel_idx);
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

// ── Live data blocks ─────────────────────────────────────────────────────────

namespace {

// Two decimals for normal prices; small prices (FX crosses, crypto) keep 6 significant
// digits instead of collapsing to "0.00".
QString rb_price_text(double v) {
    if (std::abs(v) >= 1.0 || v == 0.0)
        return QString::number(v, 'f', 2);
    return QString::number(v, 'g', 6);
}

} // namespace

void ReportBuilderScreen::fetch_quote(const QString& symbol) {
    const QString sym = symbol.trimmed().toUpper();
    if (sym.isEmpty())
        return;

    // Mark every block showing this symbol as loading (one hub request serves them all).
    auto& svc = Service::instance();
    for (const auto& c : svc.components()) {
        if (c.type == QLatin1String("market_data") && c.config.value("symbol").compare(sym, Qt::CaseInsensitive) == 0 &&
            c.config.value("status") != QLatin1String("loading")) {
            auto cfg = c.config;
            cfg["status"] = "loading";
            cfg.remove("error");
            svc.update_component(c.id, c.content, cfg);
        }
    }
    if (pending_quotes_.contains(sym))
        return; // already in flight
    pending_quotes_.insert(sym);

    const QString topic = QStringLiteral("market:quote:") + sym;
    QPointer<ReportBuilderScreen> self = this;

    // Write the result (or the failure) into every block of this symbol still loading.
    auto settle = [self, sym](const fincept::services::QuoteData* q, const QString& error) {
        if (!self)
            return;
        self->pending_quotes_.remove(sym);
        fincept::datahub::DataHub::instance().unsubscribe(self, QStringLiteral("market:quote:") + sym);
        fincept::datahub::DataHub::instance().unsubscribe_errors(self, QStringLiteral("market:quote:") + sym);
        auto& s = Service::instance();
        const QString as_of = QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm t");
        for (const auto& c : s.components()) {
            if (c.type != QLatin1String("market_data") ||
                c.config.value("symbol").compare(sym, Qt::CaseInsensitive) != 0 ||
                c.config.value("status") != QLatin1String("loading"))
                continue;
            auto cfg = c.config;
            if (q && q->price > 0) {
                cfg["price"] = rb_price_text(q->price);
                cfg["change"] = rb_price_text(q->change);
                cfg["change_pct"] = QString::number(q->change_pct, 'f', 2);
                cfg["name"] = q->name;
                cfg["high"] = q->high > 0 ? rb_price_text(q->high) : "";
                cfg["low"] = q->low > 0 ? rb_price_text(q->low) : "";
                cfg["volume"] = q->volume > 0 ? QString::number(q->volume, 'f', 0) : "";
                cfg["as_of"] = as_of;
                cfg["status"] = "ok";
                cfg.remove("error");
            } else {
                // Keep the last good values (and their as-of) visible, flag the failure.
                cfg["status"] = cfg.value("price").isEmpty() ? "error" : "ok";
                cfg["error"] = error;
            }
            s.update_component(c.id, c.content, cfg);
        }
    };

    auto& hub = fincept::datahub::DataHub::instance();
    hub.subscribe<fincept::services::QuoteData>(this, topic, [settle](const fincept::services::QuoteData& q) {
        settle(&q, q.price > 0 ? QString() : QStringLiteral("no price in the quote"));
    });
    // A producer failure (unknown symbol, provider down) settles at once with its reason.
    hub.subscribe_errors(this, topic, [settle](const QString& err) { settle(nullptr, err); });
    // Always a fresh request: a user who presses Fetch / Refresh Data wants a quote taken
    // now, not whatever the hub cached earlier (which carried no timestamp at all).
    hub.request(topic, /*force=*/true);
    // A bad symbol or a dead feed must not leave the block "Fetching…" forever (and
    // printed that way into an exported PDF).
    QTimer::singleShot(20000, this, [self, sym, settle]() {
        if (self && self->pending_quotes_.contains(sym))
            settle(nullptr, ReportBuilderScreen::tr("no quote received within 20 s"));
    });
}

void ReportBuilderScreen::refresh_live_data() {
    QSet<QString> symbols;
    for (const auto& c : Service::instance().components()) {
        if (c.type == QLatin1String("market_data")) {
            const QString sym = c.config.value("symbol").trimmed().toUpper();
            if (!sym.isEmpty())
                symbols.insert(sym);
        }
    }
    for (const QString& sym : symbols)
        fetch_quote(sym);
}

// ── Dialogs ──────────────────────────────────────────────────────────────────

} // namespace fincept::screens
