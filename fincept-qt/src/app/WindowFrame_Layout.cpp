// src/app/WindowFrame_Layout.cpp
//
// Workspace layout serialization: capture_layout() walks the dock manager
// and built-in screens to snapshot a FrameLayout, apply_layout() restores
// one. Together they implement the WorkspaceShell ↔ WindowFrame contract.
//
// Part of the partial-class split of WindowFrame.cpp.

#include "app/WindowFrame.h"

#include "app/DockScreenRouter.h"
#include "core/layout/LayoutCatalog.h"
#include "core/layout/WorkspaceShell.h"
#include "core/logging/Logger.h"
#include "core/panel/PanelMaterialiser.h"
#include "core/symbol/IGroupLinked.h"
#include "screens/common/IStatefulScreen.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QScreen>

#include <DockAreaWidget.h>
#include <DockManager.h>
#include <DockWidget.h>
#include <FloatingDockContainer.h>

#include <algorithm>

namespace fincept {

layout::FrameLayout WindowFrame::capture_layout() const {
    layout::FrameLayout fl;
    fl.window_id = frame_uuid();
    fl.focus_mode = focus_mode_;
    fl.always_on_top = always_on_top_;
    fl.schema_version = layout::kFrameLayoutVersion;

    if (!dock_manager_ || !dock_router_)
        return fl;

    // ADS dock state — opaque blob covering splits, tab order, sizes.
    fl.dock_state = dock_manager_->saveState();

    // Active panel — whichever dock widget currently has focus.
    if (auto* focused = dock_manager_->focusedDockWidget()) {
        const QString id = focused->objectName();
        const auto uuid = dock_router_->panel_uuid_for(id);
        if (!uuid.is_null())
            fl.active_panel = uuid;
    }

    // Per-panel state. Walk every dock widget the manager knows about,
    // but skip the ones currently closed. The dock manager holds two
    // populations:
    //   1. Panels the user has actually opened (visible, or visible then
    //      user-closed).
    //   2. Placeholder shells pre-registered by ensure_all_registered() so
    //      restoreState can match them by objectName — these sit as hidden
    //      tabs in a single shared dock area and were never user-opened.
    //
    // Capturing population (2) silently poisoned the autosave: on the next
    // apply_layout() the placeholders were re-injected as visible tabs in
    // the source window (Profile, About, etc. would all appear next to
    // the user's actual panel). Per-panel state for genuinely user-closed
    // tabs is already persisted via save_screen_state(), so dropping them
    // here is non-destructive — reopen restores the screen state from disk.
    for (const auto& entry : dock_manager_->dockWidgetsMap().toStdMap()) {
        const QString& id = entry.first;
        ads::CDockWidget* dw = entry.second;
        if (!dw || dw->isClosed())
            continue;

        layout::PanelState ps;
        ps.instance_id = dock_router_->panel_uuid_for(id);
        ps.title = dw->windowTitle();

        // Strip "#dupN" suffix from the type id so two watchlists share
        // type_id="watchlist" but have distinct instance_ids.
        QString type_id = id;
        const int dup_idx = type_id.indexOf("#dup");
        if (dup_idx > 0)
            type_id = type_id.left(dup_idx);
        ps.type_id = type_id;

        // IGroupLinked screens carry a link group letter. SymbolGroup is
        // a single-char enum class; we serialise as the letter.
        if (auto* w = dw->widget()) {
            if (auto* linked = dynamic_cast<IGroupLinked*>(w)) {
                const SymbolGroup g = linked->group();
                if (g != SymbolGroup::None)
                    ps.link_group = QString(QChar(symbol_group_letter(g)));
            }
            if (auto* stateful = dynamic_cast<screens::IStatefulScreen*>(w)) {
                stateful->flush_pending_state();
                const QVariantMap state = stateful->save_state();
                QJsonObject obj;
                for (auto it = state.constBegin(); it != state.constEnd(); ++it)
                    obj.insert(it.key(), QJsonValue::fromVariant(it.value()));
                ps.state_blob = QJsonDocument(obj).toJson(QJsonDocument::Compact);
                ps.state_version = stateful->state_version();
            }
        }
        ps.schema_version = layout::kPanelStateVersion;
        fl.panels.append(ps);
    }

    LOG_INFO("WindowFrame", QString("capture_layout: window %1 -> %2 panels, dock_state=%3 bytes")
                                .arg(window_id_).arg(fl.panels.size()).arg(fl.dock_state.size()));
    return fl;
}

bool WindowFrame::apply_layout(const layout::FrameLayout& fl) {
    if (!dock_manager_ || !dock_router_) {
        LOG_WARN("WindowFrame", "apply_layout: dock manager/router not ready");
        return false;
    }

    LOG_INFO("WindowFrame", QString("apply_layout: window %1, %2 panels, dock_state=%3 bytes")
                                .arg(window_id_).arg(fl.panels.size()).arg(fl.dock_state.size()));

    // Phase 8 staged materialisation (decision 9.3): split the per-panel
    // work into a cheap "shell-only" pass that runs synchronously before
    // ADS restoreState (which needs every dock widget to exist by
    // objectName), and a "screen factory" pass that runs deferred via
    // PanelMaterialiser so 50-panel workspaces don't freeze the UI.

    // Track each panel's resolved string id alongside its FrameLayout
    // entry so the deferred pass doesn't have to recompute the
    // dup_index suffix.
    QList<QString> panel_ids;
    panel_ids.reserve(fl.panels.size());

    // Phase 1: pre-create dock widget shells (no screen factory call yet)
    // and adopt the saved instance UUIDs. The shells are what ADS's
    // restoreState matches by objectName; the heavy widget construction
    // is deferred to Phase 5 below.
    for (const auto& ps : fl.panels) {
        QString id = ps.type_id;
        int dup_index = 0;
        for (const auto& other : fl.panels) {
            if (&other == &ps) break;
            if (other.type_id == ps.type_id) ++dup_index;
        }
        if (dup_index > 0)
            id = QString("%1#dup%2").arg(ps.type_id).arg(dup_index + 1);

        panel_ids.append(id);

        dock_router_->prepare_dock_widget(id);
        if (!ps.instance_id.is_null())
            dock_router_->adopt_panel_instance(id, ps.instance_id);
    }

    // Phase 2: restore the ADS dock layout blob. CDockManager::restoreState
    // returns false if the blob is incompatible (version mismatch, missing
    // dock widgets). The caller (WorkspaceMatcher) decides whether to
    // fall back to a default layout.
    bool ok = true;
    if (!fl.dock_state.isEmpty()) {
        ok = dock_manager_->restoreState(fl.dock_state);
        if (!ok) {
            LOG_WARN("WindowFrame", "apply_layout: CDockManager::restoreState rejected dock blob");
        }
    }

    // Phase 3: chrome flags.
    if (focus_mode_ != fl.focus_mode)
        toggle_focus_mode();
    if (always_on_top_ != fl.always_on_top)
        set_always_on_top(fl.always_on_top);

    // Phase 4: materialise the active panel synchronously so the user sees
    // populated content the moment the layout returns. Deferring this would
    // briefly show an empty dock area while PanelMaterialiser drains.
    QString active_id;
    if (!fl.active_panel.is_null()) {
        const auto map = dock_manager_->dockWidgetsMap();
        for (auto it = map.constBegin(); it != map.constEnd(); ++it) {
            if (dock_router_->panel_uuid_for(it.key()) == fl.active_panel) {
                active_id = it.key();
                break;
            }
        }
    }
    if (!active_id.isEmpty()) {
        dock_router_->materialize_now(active_id);
        if (auto* dw = dock_router_->find_dock_widget(active_id)) {
            dw->raise();
            dw->setAsCurrentTab();
        }
    }

    // Phase 5: enqueue the remaining panels with PanelMaterialiser. Priority:
    //   1 — currently-visible panels on a frame the user is looking at
    //       (foreground tab animation will fill in within ~16ms ticks)
    //   2 — closed/hidden panels and anything on inactive frames (drained
    //       at lower urgency; the user can't see them anyway)
    //
    // Each enqueued item is owned by `this` (the WindowFrame). If the
    // frame is destroyed mid-stage, PanelMaterialiser drops the items
    // on next tick instead of waking up to run no-op lambdas. Note the
    // owner check coexists with the QPointer&lt;DockScreenRouter&gt; guard
    // inside the lambda — the guard handles "frame still alive but
    // router torn down" (rare; same teardown order in practice).
    QPointer<DockScreenRouter> router_guard(dock_router_);
    const bool frame_active = is_active_for_work();
    for (const QString& id : panel_ids) {
        if (id == active_id)
            continue; // already materialised synchronously above
        auto* dw = dock_router_->find_dock_widget(id);
        const bool currently_visible = dw && !dw->isClosed();
        const int priority = (frame_active && currently_visible) ? 1 : 2;

        PanelMaterialiser::instance().enqueue(
            id,
            [router_guard, id]() {
                if (!router_guard) return;
                router_guard->materialize_now(id);
            },
            priority,
            /*owner=*/this);
    }

    LOG_INFO("WindowFrame", QString("apply_layout: window %1 staged %2 panels (active='%3')")
                                .arg(window_id_).arg(panel_ids.size()).arg(active_id));

    return ok;
}

} // namespace fincept
