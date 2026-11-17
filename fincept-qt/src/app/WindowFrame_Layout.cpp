// src/app/WindowFrame_Layout.cpp
//
// Workspace layout serialization: capture_layout() walks the dock manager
// and built-in screens to snapshot a FrameLayout, apply_layout() restores
// one. Together they implement the WorkspaceShell ↔ WindowFrame contract.
//
// Part of the partial-class split of WindowFrame.cpp.

#include "app/DockScreenRouter.h"
#include "app/DockStateFilter.h"
#include "app/ScreenAliases.h"
#include "app/WindowFrame.h"
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

    // ADS dock state — opaque blob covering splits, tab order, sizes. Closed
    // panels are cut from it so workspace snapshots capture only the visible
    // grid and don't carry phantom areas that reshuffle the layout on restore
    // (from the serialised state only — detaching them from the live tree
    // re-styles every closed screen).
    fl.dock_state = dock_router_->save_state_without_closed();

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
        // The real dock id ("watchlist#dup3") — apply_layout must register
        // exactly this name for restoreState to place the panel.
        ps.dock_id = id;

        // IGroupLinked screens carry a link group letter. SymbolGroup is
        // a single-char enum class; we serialise as the letter.
        // The screen itself (the dock widget's content is the host around it).
        if (auto* w = dock_router_->screen_widget(id) ? dock_router_->screen_widget(id) : dw->widget()) {
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

    LOG_DEBUG("WindowFrame", QString("capture_layout: window %1 -> %2 panels, dock_state=%3 bytes")
                                .arg(window_id_)
                                .arg(fl.panels.size())
                                .arg(fl.dock_state.size()));
    return fl;
}

bool WindowFrame::apply_layout(const layout::FrameLayout& fl_in) {
    if (!dock_manager_ || !dock_router_) {
        LOG_WARN("WindowFrame", "Layout not applied, the dock manager is not ready");
        return false;
    }
    // Never restore a duplicate of a single-instance trading screen (an old
    // layout saved with "equity_trading#dup2"): drop it from the blob and the
    // panel list, keep everything else (DockStateFilter.h).
    layout::FrameLayout fl = fl_in;
    {
        // A panel of a merged screen's old id ("agent_workbench") is the merged
        // screen now (ScreenAliases.h): renamed in the blob and the panel list
        // first, so a layout holding both collapses to one just below.
        QStringList renamed;
        fl.dock_state = screen_alias::with_aliases(fl.dock_state, &renamed);
        for (auto& ps : fl.panels) {
            if (screen_alias::is_alias(ps.type_id) || screen_alias::is_alias(ps.dock_id)) {
                renamed << (ps.dock_id.isEmpty() ? ps.type_id : ps.dock_id);
                ps.type_id = screen_alias::canonical(ps.type_id);
                if (!ps.dock_id.isEmpty())
                    ps.dock_id = screen_alias::canonical(ps.dock_id);
            }
        }
        if (!renamed.isEmpty())
            LOG_INFO("WindowFrame", QString("Layout panels renamed to their merged screen window=%1 panels=\"%2\"")
                                        .arg(window_id_)
                                        .arg(renamed.join(QLatin1Char(','))));
        QStringList dropped;
        fl.dock_state = dock_state::without_single_instance_duplicates(fl.dock_state, &dropped);
        // The panel list too: a "#dup" copy of a single-instance screen, and every
        // further panel of a single-instance type after its first (an older layout
        // that recorded no dock ids lists two "equity_trading" panels).
        QStringList single_seen;
        for (qsizetype i = 0; i < fl.panels.size();) {
            const auto& ps = fl.panels.at(i);
            const QString id = ps.dock_id.isEmpty() ? ps.type_id : ps.dock_id;
            const bool single = dock_instance::is_single_instance_type(ps.type_id);
            if (dock_state::is_single_instance_duplicate(id) || (single && single_seen.contains(ps.type_id))) {
                if (!dropped.contains(id))
                    dropped << id;
                fl.panels.removeAt(i);
                continue;
            }
            if (single)
                single_seen << ps.type_id;
            ++i;
        }
        if (!dropped.isEmpty())
            LOG_INFO("WindowFrame", QString("Layout cleaned, single-instance duplicates dropped window=%1 panels=\"%2\"")
                                        .arg(window_id_)
                                        .arg(dropped.join(QLatin1Char(','))));
    }

    LOG_DEBUG("WindowFrame", QString("apply_layout: window %1, %2 panels, dock_state=%3 bytes")
                                .arg(window_id_)
                                .arg(fl.panels.size())
                                .arg(fl.dock_state.size()));

    // Phase 8 staged materialisation (decision 9.3): split the per-panel
    // work into a cheap "shell-only" pass that runs synchronously before
    // ADS restoreState (which needs every dock widget to exist by
    // objectName), and a "screen factory" pass that runs deferred via
    // PanelMaterialiser so 50-panel workspaces don't freeze the UI.

    // Phase 0: pre-create EVERY factory-registered dock widget into a single
    // shared dock area. ADS's restoreState walks the blob and calls
    // findDockWidget(name); when a referenced widget is not registered, ADS
    // *silently skips it* (DockAreaWidget.cpp: `if (!DockWidget) continue;`)
    // and deletes any dock area that ends up empty as a result. That is the
    // recovery-bug path: closed-at-save tabs are in the dock_state blob but
    // absent from fl.panels, so they were never pre-created — restore drops
    // them and any tab group that contained only such widgets vanishes.
    //
    // ensure_all_registered is idempotent (skips widgets already in
    // dock_widgets_), groups everything as hidden tabs in one CenterDockArea
    // (vs. prepare_dock_widget's one-area-per-call), and is also what the
    // constructor calls on the QSettings-restore path — same protection here.
    // Suppress the debounced layout save during the bulk pre-create +
    // restoreState pass. These operations fire dockWidgetAdded and
    // viewToggled dozens of times; saving mid-restore would persist an
    // incomplete intermediate state.
    suppress_layout_save_ = true;

    // Only the panels the blob names (what restoreState looks up) — registering
    // every screen made each layout restore take seconds.
    dock_router_->ensure_registered_for_state(fl.dock_state);

    // Track each panel's resolved string id alongside its FrameLayout
    // entry so the deferred pass doesn't have to recompute the
    // dup_index suffix.
    QList<QString> panel_ids;
    panel_ids.reserve(fl.panels.size());

    // Phase 1: pre-create dock widget shells (no screen factory call yet)
    // and adopt the saved instance UUIDs. The shells are what ADS's
    // restoreState matches by objectName; the heavy widget construction
    // is deferred to Phase 5 below. Idempotent after Phase 0 for non-dup
    // ids; still needed for "<type>#dup<n>" panels not in factories_.
    for (const auto& ps : fl.panels) {
        QString id = ps.type_id;
        if (!ps.dock_id.isEmpty() &&
            (ps.dock_id == ps.type_id || ps.dock_id.startsWith(ps.type_id + QLatin1String("#dup")))) {
            // Saved by a build that records the real dock id.
            id = ps.dock_id;
        } else {
            // Older layouts: rebuild the "#dup<n>" suffix from panel order.
            int dup_index = 0;
            for (const auto& other : fl.panels) {
                if (&other == &ps)
                    break;
                if (other.type_id == ps.type_id)
                    ++dup_index;
            }
            if (dup_index > 0)
                id = QString("%1#dup%2").arg(ps.type_id).arg(dup_index + 1);
        }

        panel_ids.append(id);

        dock_router_->prepare_dock_widget(id);
        if (!ps.instance_id.is_null())
            dock_router_->adopt_panel_instance(id, ps.instance_id);
        // Re-apply the panel's saved colour link when its screen is built.
        if (!ps.link_group.isEmpty())
            dock_router_->set_pending_link_group(id, symbol_group_from_char(ps.link_group.at(0)));
    }
    // Any other "#dup<n>" widget the blob names (e.g. an older layout whose
    // order-based ids above don't match) still needs a shell, or restoreState
    // drops it together with its dock area.
    dock_router_->prepare_duplicate_panels(fl.dock_state);

    // Phase 2: restore the ADS dock layout blob. CDockManager::restoreState
    // returns false if the blob is incompatible (version mismatch, missing
    // dock widgets). The caller (WorkspaceMatcher) decides whether to
    // fall back to a default layout.
    bool ok = true;
    if (!fl.dock_state.isEmpty()) {
        // As in the constructor's restore: restoreState() shows every restored
        // panel, and building each screen inline there froze the window for
        // seconds. Screens are built below — the active one now, the others
        // one per event-loop turn or when first shown.
        dock_router_->set_suppress_materialize(true);
        ok = dock_manager_->restoreState(fl.dock_state);
        dock_router_->set_suppress_materialize(false);
        // A second panel of a single-instance screen that survived the filter
        // above (any path an older build found) is removed before anything is built.
        if (ok)
            dock_router_->collapse_duplicates(QStringLiteral("layout_apply"));
        if (!ok) {
            LOG_WARN("WindowFrame", QString("Layout not applied, the saved dock state was rejected window=%1 bytes=%2")
                                        .arg(window_id_)
                                        .arg(fl.dock_state.size()));
            suppress_layout_save_ = false;
            return false;
        }
    }

    suppress_layout_save_ = false;

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
        // Only panels that will be on screen: a closed panel or a background
        // tab is built when it is first shown (visibilityChanged →
        // materialize_screen), not at start-up.
        const bool on_screen = dw && !dw->isClosed() && dw->dockAreaWidget() &&
                               dw->dockAreaWidget()->currentDockWidget() == dw;
        if (!on_screen)
            continue;
        const int priority = frame_active ? 1 : 2;

        PanelMaterialiser::instance().enqueue(
            id,
            [router_guard, id]() {
                if (!router_guard)
                    return;
                router_guard->materialize_now(id);
            },
            priority,
            /*owner=*/this);
    }

    LOG_DEBUG("WindowFrame", QString("apply_layout: window %1 staged %2 panels (active='%3')")
                                .arg(window_id_)
                                .arg(panel_ids.size())
                                .arg(active_id));

    return ok;
}

} // namespace fincept
