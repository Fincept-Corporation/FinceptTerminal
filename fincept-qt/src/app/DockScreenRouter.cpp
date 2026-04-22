#include "app/DockScreenRouter.h"

#include "auth/InactivityGuard.h"
#include "core/components/PopularityTracker.h"
#include "core/logging/Logger.h"
#include "core/session/ScreenStateManager.h"
#include "core/session/SessionManager.h"
#include "core/symbol/IGroupLinked.h"
#include "core/symbol/SymbolContext.h"
#include "core/symbol/SymbolRef.h"
#include "screens/IStatefulScreen.h"
#include "ui/widgets/GroupBadge.h"

#include <QApplication>
#include <QClipboard>
#include <QContextMenuEvent>
#include <QEvent>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QVBoxLayout>

#include <DockAreaWidget.h>
#include <DockManager.h>
#include <DockWidget.h>
#include <DockWidgetTab.h>

namespace fincept {

QString DockScreenRouter::title_for_id(const QString& id) {
    static const QHash<QString, QString> titles = {
        {"dashboard", "Dashboard"},
        {"markets", "Markets"},
        {"crypto_trading", "Crypto Trading"},
        {"equity_trading", "Equity Trading"},
        {"algo_trading", "Algo Trading"},
        {"backtesting", "Backtesting"},
        {"portfolio", "Portfolio"},
        {"watchlist", "Watchlist"},
        {"news", "News"},
        {"ai_chat", "AI Chat"},
        {"equity_research", "Equity Research"},
        {"economics", "Economics"},
        {"dbnomics", "DBnomics"},
        {"akshare", "AkShare Data"},
        {"asia_markets", "Asia Markets"},
        {"geopolitics", "Geopolitics"},
        {"gov_data", "Gov Data"},
        {"maritime", "Maritime"},
        {"polymarket", "Prediction Markets"},
        {"relationship_map", "Relationship Map"},
        {"derivatives", "Derivatives"},
        {"alt_investments", "Alt Investments"},
        {"ma_analytics", "M&A Analytics"},
        {"surface_analytics", "Surface Analytics"},
        {"quantlib", "QuantLib"},
        {"ai_quant_lab", "AI Quant Lab"},
        {"alpha_arena", "Alpha Arena"},
        {"agent_config", "Agent Config"},
        {"mcp_servers", "MCP Servers"},
        {"node_editor", "Node Editor"},
        {"code_editor", "Code Editor"},
        {"excel", "Excel"},
        {"report_builder", "Report Builder"},
        {"trade_viz", "Trade Viz"},
        {"data_sources", "Data Sources"},
        {"data_mapping", "Data Mapping"},
        {"file_manager", "File Manager"},
        {"notes", "Notes"},
        {"forum", "Forum"},
        {"docs", "Docs"},
        {"support", "Support"},
        {"about", "About"},
        {"profile", "Profile"},
        {"settings", "Settings"},
        {"contact", "Contact"},
        {"terms", "Terms"},
        {"privacy", "Privacy"},
        {"trademarks", "Trademarks"},
        {"help", "Help"},
    };
    return titles.value(id, id);
}

DockScreenRouter::DockScreenRouter(ads::CDockManager* manager, QObject* parent) : QObject(parent), manager_(manager) {
    // Route SymbolContext broadcasts to the matching IGroupLinked screens.
    // One central listener per router — cheaper and easier to reason about
    // than per-screen connections, and it gives us a single filter point
    // for source-suppression (skip re-delivery to the publisher).
    connect(&SymbolContext::instance(), &SymbolContext::group_symbol_changed, this,
            &DockScreenRouter::on_group_symbol_changed_external);
}

void DockScreenRouter::on_group_symbol_changed_external(SymbolGroup g, const SymbolRef& ref, QObject* source) {
    for (auto it = group_linked_.begin(); it != group_linked_.end(); ++it) {
        IGroupLinked* linked = it.value();
        if (!linked || linked->group() != g)
            continue;
        auto* as_obj = dynamic_cast<QObject*>(linked);
        if (as_obj && as_obj == source)
            continue; // skip feedback to the publisher
        linked->on_group_symbol_changed(ref);
    }
}

QWidget* DockScreenRouter::wrap_with_group_badge(const QString& id, QWidget* screen) {
    if (!screen)
        return screen;
    auto* linked = dynamic_cast<IGroupLinked*>(screen);
    if (!linked)
        return screen; // screen doesn't opt in — no wrapping, no overhead

    // Container with a narrow strip on top holding the badge. The badge is
    // parented to the strip so deleting the screen cascades-deletes it.
    auto* container = new QWidget;
    container->setObjectName("group_linked_container__" + id);
    auto* v = new QVBoxLayout(container);
    v->setContentsMargins(0, 0, 0, 0);
    v->setSpacing(0);

    auto* strip = new QWidget(container);
    strip->setObjectName("group_badge_strip");
    strip->setFixedHeight(22);
    auto* h = new QHBoxLayout(strip);
    h->setContentsMargins(6, 2, 6, 2);
    h->setSpacing(6);

    auto* badge = new ui::GroupBadge(strip);
    badge->set_group_silent(linked->group());
    h->addWidget(badge);

    auto* label = new QLabel(strip);
    label->setObjectName("group_badge_symbol");
    label->setStyleSheet("color:#9ca3af;font-size:11px;");
    const SymbolRef cur = SymbolContext::instance().group_symbol(linked->group());
    if (cur.is_valid())
        label->setText(cur.display());
    h->addWidget(label);
    h->addStretch(1);

    v->addWidget(strip);
    v->addWidget(screen, 1);

    // Keep the strip label in sync with the group's active symbol.
    auto sync_label = [label, linked]() {
        const SymbolRef cur = SymbolContext::instance().group_symbol(linked->group());
        label->setText(cur.is_valid() ? cur.display() : QString());
    };

    connect(badge, &ui::GroupBadge::group_change_requested, this,
            [this, id, linked, badge, sync_label](SymbolGroup g) {
                linked->set_group(g);
                badge->set_group_silent(g);
                sync_label();
                // If the new group already has an active symbol, push it into
                // the newly-linked screen immediately. If the screen itself
                // has a current symbol and the group has none, seed the group
                // from the screen.
                const SymbolRef group_sym = SymbolContext::instance().group_symbol(g);
                if (group_sym.is_valid()) {
                    linked->on_group_symbol_changed(group_sym);
                } else if (g != SymbolGroup::None) {
                    const SymbolRef own = linked->current_symbol();
                    if (own.is_valid())
                        SymbolContext::instance().set_group_symbol(g, own,
                                                                  dynamic_cast<QObject*>(linked));
                }
            });

    // Re-sync the label when someone *else* publishes to the group this panel
    // is linked to.
    connect(&SymbolContext::instance(), &SymbolContext::group_symbol_changed, label,
            [linked, sync_label](SymbolGroup g, const SymbolRef&, QObject*) {
                if (linked->group() == g)
                    sync_label();
            });

    group_linked_[id] = linked;
    group_badges_[id] = badge;
    return container;
}

void DockScreenRouter::register_screen(const QString& id, QWidget* screen) {
    // Store the screen widget — the CDockWidget is only created on first navigate().
    // This prevents all eagerly registered screens from appearing at startup.
    screens_[id] = screen;
    factories_.remove(id);
}

void DockScreenRouter::register_factory(const QString& id, ScreenFactory factory) {
    factories_[id] = std::move(factory);
}

void DockScreenRouter::navigate(const QString& id, bool exclusive) {
    // Hard refuse while the lock screen is showing — signals/timers/keyboard
    // shortcuts that would otherwise mutate panel state behind the PIN gate
    // must be no-ops. MainWindow also disables the dock_manager_ widget tree
    // for keyboard/focus safety; this check catches programmatic callers.
    if (auth::InactivityGuard::instance().is_terminal_locked()) {
        LOG_DEBUG("DockRouter", QString("navigate('%1') suppressed — terminal locked").arg(id));
        return;
    }

    LOG_INFO("DockRouter",
             QString(">>> navigate('%1', exclusive=%2) panel_count=%3").arg(id).arg(exclusive).arg(panel_count_));
    // Bump popularity so the Component Browser surfaces frequently-used
    // panels at the top. Safe for unknown ids — PopularityTracker no-ops
    // on failure and we still fall into the unknown-screen branch below.
    PopularityTracker::instance().increment(id);

    auto* dw = find_dock_widget(id);

    bool needs_add = false;
    if (!dw) {
        // Screen not yet in dock manager — check if we know how to create it
        if (!factories_.contains(id) && !screens_.contains(id)) {
            LOG_WARN("DockRouter", "Unknown screen: " + id);
            return;
        }
        dw = create_dock_widget(id);
        needs_add = true;
    } else if (!dw->dockManager()) {
        // Widget exists but was never added to the dock manager (pre-created
        // by ensure_all_registered for state restore, but restoreState was
        // skipped or didn't place this widget).
        needs_add = true;
    }

    if (screens_.contains(id) && dw->widget() != screens_[id]) {
        QWidget* old = dw->widget();
        dw->setWidget(screens_[id]);
        if (old) {
            old->setParent(nullptr);
            old->deleteLater();
        }
    }

    materialize_screen(id);

    if (exclusive) {
        for (auto* area : manager_->openedDockAreas()) {
            for (auto* other : area->openedDockWidgets()) {
                if (other != dw)
                    other->toggleView(false);
            }
        }
        for (auto* other : manager_->dockWidgetsMap()) {
            if (other != dw && other->isAutoHide())
                other->toggleView(false);
        }
        grid_top_left_ = grid_top_right_ = grid_bottom_left_ = grid_bottom_right_ = nullptr;
        panel_count_ = 0;
    }

    // Determine whether this widget needs to be physically placed into a new
    // split area, or whether it already has its own dedicated area.
    //
    // ensure_all_registered() pre-creates every dock widget and groups them all
    // into a SINGLE shared dock area (all hidden). We detect this by checking
    // whether dw's area contains other dock widgets (shared) vs being solo.
    // We check dockWidgets() (all, not just open) because ensure_all_registered
    // hides them all — openedDockWidgets() would be empty and miss the poisoning.
    auto* existing_area = dw->dockAreaWidget();
    const bool area_is_poisoned = existing_area && existing_area->dockWidgets().size() > 1;
    const bool must_place = needs_add || !existing_area || area_is_poisoned;

    // ── Helper: sync grid tracking from currently open areas ────────────────
    // After restoreState(), exclusive navigate, or user drag, the grid pointers
    // may be stale. This lambda rebuilds them from whatever ADS actually has.
    auto sync_grid_from_reality = [this]() {
        const auto opened = manager_->openedDockAreas();
        panel_count_ = std::min(static_cast<int>(opened.size()), 4);

        // Invalidate pointers that no longer exist
        if (grid_top_left_ && !opened.contains(grid_top_left_))
            grid_top_left_ = nullptr;
        if (grid_top_right_ && !opened.contains(grid_top_right_))
            grid_top_right_ = nullptr;
        if (grid_bottom_left_ && !opened.contains(grid_bottom_left_))
            grid_bottom_left_ = nullptr;
        if (grid_bottom_right_ && !opened.contains(grid_bottom_right_))
            grid_bottom_right_ = nullptr;

        // Fill in missing pointers from opened areas
        int idx = 0;
        if (!grid_top_left_ && idx < opened.size())
            grid_top_left_ = opened.at(idx);
        if (grid_top_left_)
            idx = opened.indexOf(grid_top_left_) + 1;
        if (!grid_top_right_ && idx < opened.size())
            grid_top_right_ = opened.at(idx);
        if (grid_top_right_)
            idx = opened.indexOf(grid_top_right_) + 1;
        if (!grid_bottom_left_ && idx < opened.size())
            grid_bottom_left_ = opened.at(idx);
        if (grid_bottom_left_)
            idx = opened.indexOf(grid_bottom_left_) + 1;
        if (!grid_bottom_right_ && idx < opened.size())
            grid_bottom_right_ = opened.at(idx);

        LOG_INFO("DockRouter", QString("GRID synced: panel_count=%1 opened=%2 TL=%3 TR=%4 BL=%5 BR=%6")
                                   .arg(panel_count_)
                                   .arg(opened.size())
                                   .arg(grid_top_left_ ? "ok" : "-")
                                   .arg(grid_top_right_ ? "ok" : "-")
                                   .arg(grid_bottom_left_ ? "ok" : "-")
                                   .arg(grid_bottom_right_ ? "ok" : "-"));
    };

    if (exclusive) {
        // Exclusive: place into full container (always, regardless of must_place)
        auto* area = manager_->addDockWidget(ads::CenterDockWidgetArea, dw);
        grid_top_left_ = area;
        grid_top_right_ = grid_bottom_left_ = grid_bottom_right_ = nullptr;
        panel_count_ = 1;

    } else if (must_place) {
        // Sync grid from reality before placing
        sync_grid_from_reality();

        LOG_INFO("DockRouter", QString("GRID [%1] must_place panel_count=%2").arg(id).arg(panel_count_));

        if (panel_count_ == 0) {
            LOG_INFO("DockRouter", QString("GRID [%1] -> PANEL 1 (full)").arg(id));
            auto* area = manager_->addDockWidget(ads::CenterDockWidgetArea, dw);
            grid_top_left_ = area;
            panel_count_ = 1;

        } else if (panel_count_ == 1 && grid_top_left_) {
            LOG_INFO("DockRouter", QString("GRID [%1] -> PANEL 2 (right of TL)").arg(id));
            auto* area = manager_->addDockWidget(ads::RightDockWidgetArea, dw, grid_top_left_);
            grid_top_right_ = area;
            panel_count_ = 2;

        } else if (panel_count_ == 2 && grid_top_left_) {
            LOG_INFO("DockRouter", QString("GRID [%1] -> PANEL 3 (below TL)").arg(id));
            auto* area = manager_->addDockWidget(ads::BottomDockWidgetArea, dw, grid_top_left_);
            grid_bottom_left_ = area;
            panel_count_ = 3;

        } else if (panel_count_ == 3 && grid_bottom_left_) {
            LOG_INFO("DockRouter", QString("GRID [%1] -> PANEL 4 (right of BL)").arg(id));
            auto* area = manager_->addDockWidget(ads::RightDockWidgetArea, dw, grid_bottom_left_);
            grid_bottom_right_ = area;
            panel_count_ = 4;

        } else {
            // 5th+ panel or missing grid pointer: tab into last area
            LOG_INFO("DockRouter", QString("GRID [%1] -> tab into last (panel_count=%2)").arg(id).arg(panel_count_));
            ads::CDockAreaWidget* target =
                grid_bottom_right_
                    ? grid_bottom_right_
                    : (manager_->openedDockAreas().isEmpty() ? nullptr : manager_->openedDockAreas().last());
            if (target)
                manager_->addDockWidget(ads::CenterDockWidgetArea, dw, target);
            else
                manager_->addDockWidget(ads::CenterDockWidgetArea, dw);
            panel_count_++;
        }

    } else {
        // Widget already has a dedicated area — just show it.
        // But sync grid tracking so it knows about this area for future placements.
        LOG_INFO("DockRouter", QString("GRID [%1] already placed, syncing grid").arg(id));
        sync_grid_from_reality();
    }

    // Always call toggleView(true) — addDockWidget() marks the widget as
    // not-closed but does NOT make the tab visible. Without this unconditional
    // call, the tab stays hidden because isClosed() returns false and the
    // previous conditional `if (isClosed()) toggleView(true)` was skipped.
    dw->toggleView(true);
    // ADS does not restore CDockWidgetTab visibility when toggleView(true)
    // re-opens a previously hidden widget — the tab stays hidden from the
    // ensure_all_registered() phase where all widgets were toggleView(false)'d.
    // Force the tab visible so the label text appears in the title bar.
    if (auto* tab = dw->tabWidget()) {
        if (tab->isHidden())
            tab->setVisible(true);
    }
    dw->raise();
    dw->setAsCurrentTab();

    apply_ads_theme();

    current_id_ = id;
    SessionManager::instance().set_last_screen(id);
    LOG_INFO("DockRouter", "Navigated to: " + id);
    emit screen_changed(id);
}

void DockScreenRouter::tab_into(const QString& id) {
    if (!factories_.contains(id) && !screens_.contains(id)) {
        LOG_WARN("DockRouter", "Unknown screen: " + id);
        return;
    }

    auto* dw = find_dock_widget(id);
    bool needs_add = !dw || !dw->dockManager();

    if (!dw) {
        dw = create_dock_widget(id);
        needs_add = true;
    }

    materialize_screen(id);

    // If already visible somewhere just raise it — no need to re-tab.
    if (!dw->isClosed() && dw->dockAreaWidget()) {
        dw->raise();
        dw->setAsCurrentTab();
        current_id_ = id;
        emit screen_changed(id);
        return;
    }

    // Find the focused/active dock area to tab into.
    // Prefer the focused widget's area; fall back to the first open area.
    ads::CDockAreaWidget* target_area = nullptr;
    if (auto* focused = manager_->focusedDockWidget())
        target_area = focused->dockAreaWidget();
    if (!target_area) {
        const auto opened = manager_->openedDockAreas();
        if (!opened.isEmpty())
            target_area = opened.first();
    }

    if (needs_add || !dw->dockAreaWidget()) {
        if (target_area)
            // Tab into the existing area — CenterDockWidgetArea = same area, new tab
            manager_->addDockWidget(ads::CenterDockWidgetArea, dw, target_area);
        else
            // No area open yet — open as the sole screen
            manager_->addDockWidget(ads::CenterDockWidgetArea, dw);
    }

    dw->toggleView(true);
    if (auto* tab = dw->tabWidget()) {
        if (tab->isHidden())
            tab->setVisible(true);
    }
    dw->raise();
    dw->setAsCurrentTab();

    current_id_ = id;
    SessionManager::instance().set_last_screen(id);
    LOG_INFO("DockRouter", "Tabbed into: " + id);
    emit screen_changed(id);
}

void DockScreenRouter::add_alongside(const QString& primary, const QString& secondary) {
    // If primary is already open, just add secondary into the next grid slot
    // without resetting the layout. This allows building up to a 2x2 grid
    // (4 panels) incrementally via repeated "add" commands.
    auto* primary_dw = find_dock_widget(primary);
    bool primary_visible = primary_dw && !primary_dw->isClosed() && primary_dw->dockAreaWidget();

    LOG_INFO("DockRouter", QString("add_alongside: primary=%1 visible=%2 secondary=%3 panel_count=%4")
                               .arg(primary)
                               .arg(primary_visible)
                               .arg(secondary)
                               .arg(panel_count_));

    if (primary_visible) {
        // Primary already showing — add secondary non-exclusively
        navigate(secondary, false);
    } else {
        // Primary not showing — open it exclusively, then split secondary alongside
        navigate(primary, true);
        navigate(secondary, false);
    }

    // Re-focus the primary so it stays the "main" panel — the user typed
    // "primary add secondary", meaning primary is what they're working in.
    primary_dw = find_dock_widget(primary);
    if (primary_dw && !primary_dw->isClosed()) {
        primary_dw->raise();
        primary_dw->setAsCurrentTab();
        current_id_ = primary;
        emit screen_changed(primary);
    }
}

void DockScreenRouter::remove_screen(const QString& primary) {
    // Close all panels except primary, which expands to fill the area.
    auto* keep = find_dock_widget(primary);
    for (auto* dw : manager_->dockWidgetsMap()) {
        if (dw != keep && !dw->isClosed())
            dw->toggleView(false);
    }
    if (keep) {
        if (keep->isClosed())
            keep->toggleView(true);
        keep->raise();
        keep->setAsCurrentTab();
        current_id_ = primary;
        emit screen_changed(primary);
    }
}

void DockScreenRouter::replace_screen(const QString& primary, const QString& secondary) {
    // Navigate to primary (ensures it exists), close everything else, then
    // navigate to secondary in exclusive mode so it fills the full area.
    Q_UNUSED(primary)
    navigate(secondary, true);
}

void DockScreenRouter::ensure_all_registered() {
    // Pre-create CDockWidgets for ALL known screens and register them
    // with the dock manager so restoreState() can find them by objectName
    // in the DockWidgetsMap. All widgets are added as tabs into a single
    // dock area (not separate splits!) and immediately closed. restoreState()
    // will reopen and reposition only those that were visible in the saved layout.
    QStringList all_ids;
    for (auto it = factories_.cbegin(); it != factories_.cend(); ++it)
        all_ids.append(it.key());
    for (auto it = screens_.cbegin(); it != screens_.cend(); ++it) {
        if (!all_ids.contains(it.key()))
            all_ids.append(it.key());
    }

    ads::CDockAreaWidget* first_area = nullptr;
    for (const QString& id : all_ids) {
        if (!dock_widgets_.contains(id)) {
            auto* dw = create_dock_widget(id);
            // Add all widgets as tabs in the SAME dock area to avoid
            // creating 47 vertical splits. The first widget creates the area,
            // all subsequent ones tab into it.
            if (first_area)
                manager_->addDockWidget(ads::CenterDockWidgetArea, dw, first_area);
            else
                first_area = manager_->addDockWidget(ads::CenterDockWidgetArea, dw);
            dw->toggleView(false); // start hidden
        }
    }
    // Apply theme directly to all title bars and tabs created above.
    apply_ads_theme();
}

ads::CDockWidget* DockScreenRouter::find_dock_widget(const QString& id) const {
    return dock_widgets_.value(id, nullptr);
}

void DockScreenRouter::cycle_focused_panel(bool forward) {
    if (!manager_)
        return;
    // Build an ordered list of currently-visible dock widgets — we cycle
    // through those, not the entire registration catalogue.
    QList<ads::CDockWidget*> visible;
    visible.reserve(dock_widgets_.size());
    for (auto it = dock_widgets_.cbegin(); it != dock_widgets_.cend(); ++it) {
        ads::CDockWidget* dw = it.value();
        if (dw && !dw->isClosed())
            visible.append(dw);
    }
    if (visible.size() < 2)
        return;

    // Find the currently-focused dock widget. ADS exposes focusedDockWidget()
    // but it may be null if the focused QWidget isn't inside any dock —
    // fall back to the dock containing current_id_.
    ads::CDockWidget* current = manager_->focusedDockWidget();
    if (!current)
        current = dock_widgets_.value(current_id_, nullptr);

    int idx = visible.indexOf(current);
    if (idx < 0)
        idx = 0;
    else
        idx = (idx + (forward ? 1 : -1) + visible.size()) % visible.size();

    ads::CDockWidget* next = visible.at(idx);
    next->raise();
    next->setAsCurrentTab();
    if (auto* w = next->widget())
        w->setFocus(Qt::ShortcutFocusReason);
    current_id_ = next->objectName();
    emit screen_changed(current_id_);
}

ads::CDockWidget* DockScreenRouter::duplicate_panel(const QString& id) {
    if (!manager_)
        return nullptr;

    // Strip any existing #dup<N> suffix so duplicates-of-duplicates all derive
    // from the same original factory — prevents unbounded id drift.
    QString base = id;
    const int hash = id.indexOf('#');
    if (hash > 0)
        base = id.left(hash);

    // We need a factory to produce a fresh widget — the eagerly-registered
    // path gives one instance that we can't clone safely.
    auto fit = factories_.find(base);
    // Special case: the original factory was consumed on first materialise,
    // so factories_ may no longer contain `base`. In that case reconstruct
    // from a registered rebuild helper (we don't have one yet) — fail with a
    // warning so the user sees a concrete reason rather than a silent no-op.
    if (fit == factories_.end()) {
        // Only eager (one-instance) screens reach here. Duplication is not
        // supported for them in this phase.
        LOG_WARN("DockRouter",
                 QString("duplicate_panel('%1'): factory not available — duplication requires "
                         "register_factory() and the original factory is consumed on first materialise. "
                         "Re-register a factory before duplication to support this path.").arg(base));
        return nullptr;
    }

    // Allocate a unique suffixed id. Scan existing ids to avoid collisions.
    int n = 2;
    QString dup_id;
    do {
        dup_id = QString("%1#dup%2").arg(base).arg(n++);
    } while (dock_widgets_.contains(dup_id) || factories_.contains(dup_id));

    // Build the duplicate via a fresh factory copy so navigate() can
    // materialize it on-demand through the existing path. Re-install the
    // base factory too so further duplicates remain possible.
    ScreenFactory factory_copy = fit.value();
    factories_[dup_id] = factory_copy;
    factories_[base]   = factory_copy;

    // Title: "<Original Title> 2", "<Original Title> 3", ...
    const int dup_index = n - 1;
    // Populate title for the synthetic id so the tab label reads cleanly.
    // title_for_id() is static — we can't extend it at runtime, so we store
    // the custom title via the existing user-rename path.
    save_tab_title(dup_id, QString("%1 %2").arg(title_for_id(base)).arg(dup_index));

    // Create the dock widget. This path also wraps the widget in a
    // GroupBadge container if the source screen implements IGroupLinked,
    // materialises via the factory, and adds the dock widget to the manager.
    auto* dw = create_dock_widget(dup_id);
    if (!dw) {
        LOG_WARN("DockRouter", QString("duplicate_panel: create_dock_widget failed for %1").arg(dup_id));
        return nullptr;
    }
    materialize_screen(dup_id);

    // Copy IStatefulScreen state from source → duplicate so the user doesn't
    // re-type the ticker etc. Source is the inner screen widget, not the
    // group-badge container — screens_ always stores the inner widget.
    auto* src_screen = screens_.value(base, nullptr);
    auto* dup_screen = screens_.value(dup_id, nullptr);
    if (src_screen && dup_screen) {
        auto* src_stateful = dynamic_cast<screens::IStatefulScreen*>(src_screen);
        auto* dup_stateful = dynamic_cast<screens::IStatefulScreen*>(dup_screen);
        if (src_stateful && dup_stateful)
            dup_stateful->restore_state(src_stateful->save_state());
    }

    // Place the duplicate next to the original as a tabbed sibling so the
    // user sees it immediately without manually docking.
    auto* src_dw = dock_widgets_.value(base, nullptr);
    if (src_dw && src_dw->dockAreaWidget()) {
        manager_->addDockWidgetTabToArea(dw, src_dw->dockAreaWidget());
    } else {
        manager_->addDockWidget(ads::CenterDockWidgetArea, dw);
    }
    dw->setAsCurrentTab();
    dw->raise();

    LOG_INFO("DockRouter", QString("Duplicated '%1' → '%2'").arg(base, dup_id));
    return dw;
}

// ── Phase 12 — tab right-click context menu ──────────────────────────────────

void DockScreenRouter::show_tab_context_menu(const QString& id, const QPoint& global_pos) {
    auto* dw = dock_widgets_.value(id, nullptr);
    if (!dw)
        return;

    QMenu menu;
    menu.setStyleSheet(
        "QMenu{background:#111827;color:#e5e7eb;border:1px solid #374151;padding:4px 0;}"
        "QMenu::item{padding:5px 24px 5px 12px;}"
        "QMenu::item:selected{background:#1f2937;color:#d97706;}"
        "QMenu::item:disabled{color:#6b7280;}"
        "QMenu::separator{background:#374151;height:1px;margin:4px 8px;}");

    // Rename — reuses the inline editor code path. Simpler to pop a modal
    // prompt here; the inline editor requires the label's geometry which is
    // awkward to invoke from a menu click.
    auto* act_rename = menu.addAction("Rename Tab…");
    connect(act_rename, &QAction::triggered, this, [this, id, dw]() {
        bool ok = false;
        const QString name = QInputDialog::getText(nullptr, "Rename Tab", "New name:",
                                                   QLineEdit::Normal, dw->windowTitle(), &ok);
        if (ok && !name.trimmed().isEmpty()) {
            dw->setWindowTitle(name.trimmed());
            save_tab_title(id, name.trimmed());
        }
    });

    // Duplicate — only enabled for factory-registered screens. We can't tell
    // from here reliably without re-running the factory lookup, so we flag the
    // item as enabled and let duplicate_panel no-op with a log warning on
    // unsupported screens.
    auto* act_dup = menu.addAction("Duplicate Panel");
    connect(act_dup, &QAction::triggered, this, [this, id]() { duplicate_panel(id); });

    menu.addSeparator();

    // Float or re-dock. ADS's setFloating() has no matching setDocked(); to
    // re-dock a floating widget we close its view and re-open it, which
    // causes ADS to re-attach it to the main container.
    const bool floating = dw->isFloating();
    auto* act_float = menu.addAction(floating ? "Re-dock Panel" : "Float Panel");
    connect(act_float, &QAction::triggered, this, [this, dw, floating]() {
        if (floating) {
            dw->toggleView(false);
            dw->toggleView(true);
            if (manager_ && manager_->dockAreaCount() == 0)
                manager_->addDockWidget(ads::CenterDockWidgetArea, dw);
        } else {
            dw->setFloating();
        }
    });

    // Always-on-top — only meaningful when the dock is a top-level floating
    // window. For docked tabs, the flag lives on the MainWindow; we don't
    // expose it here to avoid confusion.
    if (floating) {
        if (auto* fc = dw->floatingDockContainer()) {
            const bool on = fc->windowFlags().testFlag(Qt::WindowStaysOnTopHint);
            auto* act_top = menu.addAction("Always on Top");
            act_top->setCheckable(true);
            act_top->setChecked(on);
            connect(act_top, &QAction::triggered, this, [fc, on]() {
                Qt::WindowFlags f = fc->windowFlags();
                if (on)
                    f &= ~Qt::WindowStaysOnTopHint;
                else
                    f |= Qt::WindowStaysOnTopHint;
                fc->setWindowFlags(f);
                fc->show();
            });
        }
    }

    menu.addSeparator();

    // Link to Group (Phase 1) — only for screens implementing IGroupLinked.
    // The inner screen is in screens_[id]; the container wrapping happens at
    // dw->widget(), so cast from the stored screen to test the interface.
    if (auto* screen = screens_.value(id, nullptr)) {
        if (auto* linked = dynamic_cast<IGroupLinked*>(screen)) {
            auto* group_menu = menu.addMenu("Link to Group");
            group_menu->setStyleSheet(menu.styleSheet());
            const SymbolGroup current = linked->group();

            auto* unlink = group_menu->addAction("(None)");
            unlink->setCheckable(true);
            unlink->setChecked(current == SymbolGroup::None);
            connect(unlink, &QAction::triggered, this, [this, id]() {
                if (auto* s = screens_.value(id, nullptr)) {
                    if (auto* l = dynamic_cast<IGroupLinked*>(s))
                        l->set_group(SymbolGroup::None);
                }
                QPointer<ui::GroupBadge> badge = group_badges_.value(id);
                if (badge)
                    badge->set_group_silent(SymbolGroup::None);
            });
            group_menu->addSeparator();

            for (SymbolGroup g : all_symbol_groups()) {
                auto* a = group_menu->addAction(QString("Group %1").arg(symbol_group_letter(g)));
                a->setCheckable(true);
                a->setChecked(g == current);
                connect(a, &QAction::triggered, this, [this, id, g]() {
                    auto* s = screens_.value(id, nullptr);
                    auto* l = s ? dynamic_cast<IGroupLinked*>(s) : nullptr;
                    if (!l) return;
                    l->set_group(g);
                    QPointer<ui::GroupBadge> badge = group_badges_.value(id);
                    if (badge)
                        badge->set_group_silent(g);
                    // Seed the newly-linked group with the screen's current
                    // symbol (if any) so the other group panels follow it.
                    const SymbolRef own = l->current_symbol();
                    if (own.is_valid())
                        SymbolContext::instance().set_group_symbol(
                            g, own, dynamic_cast<QObject*>(l));
                });
            }
        }
    }

    menu.addSeparator();

    // Copy tab title to clipboard — small but handy for sharing a panel id
    // over chat / for scripting.
    auto* act_copy = menu.addAction("Copy Tab Title");
    connect(act_copy, &QAction::triggered, this, [dw]() {
        QApplication::clipboard()->setText(dw->windowTitle());
    });

    // Close tab.
    auto* act_close = menu.addAction("Close Tab");
    connect(act_close, &QAction::triggered, this, [dw]() { dw->toggleView(false); });

    menu.exec(global_pos);
}

QStringList DockScreenRouter::all_screen_ids() const {
    QStringList ids;
    ids.reserve(dock_widgets_.size() + factories_.size());
    for (auto it = dock_widgets_.cbegin(); it != dock_widgets_.cend(); ++it)
        ids.append(it.key());
    for (auto it = factories_.cbegin(); it != factories_.cend(); ++it) {
        if (!ids.contains(it.key()))
            ids.append(it.key());
    }
    return ids;
}

bool DockScreenRouter::eventFilter(QObject* obj, QEvent* event) {
    // Phase 12 — right-click context menu on any installed tab/label. Handled
    // first so we don't gate on QLabel-only below (the tab widget itself is
    // also a watched object now).
    if (event->type() == QEvent::ContextMenu) {
        const QString id = obj->property("dock_id").toString();
        if (!id.isEmpty() && dock_widgets_.contains(id)) {
            auto* cm = static_cast<QContextMenuEvent*>(event);
            show_tab_context_menu(id, cm->globalPos());
            return true;
        }
    }

    if (event->type() != QEvent::MouseButtonDblClick)
        return QObject::eventFilter(obj, event);

    auto* lbl = qobject_cast<QLabel*>(obj);
    if (!lbl)
        return QObject::eventFilter(obj, event);

    const QString id = lbl->property("dock_id").toString();
    auto* dw = dock_widgets_.value(id, nullptr);
    if (!dw)
        return QObject::eventFilter(obj, event);

    // Show a QLineEdit overlaid on the label for inline rename.
    // Parent it to the label's parent (the tab frame) so geometry is correct.
    QWidget* tab_frame = lbl->parentWidget();
    auto* editor = new QLineEdit(tab_frame);
    editor->setText(dw->windowTitle());
    editor->selectAll();
    editor->setFrame(false);
    editor->setStyleSheet("QLineEdit{"
                          "  background:#1a1a1a;"
                          "  color:#ffffff;"
                          "  border:1px solid #d97706;"
                          "  padding:0 4px;"
                          "  font-size:11px;"
                          "  font-family:'Consolas',monospace;"
                          "}");
    // Map label rect into the tab frame's coordinate space
    editor->setGeometry(tab_frame->mapFromGlobal(lbl->mapToGlobal(QPoint(0, 0))).x(), 2, lbl->width(),
                        tab_frame->height() - 4);
    editor->show();
    editor->raise();
    editor->setFocus();

    auto commit = [dw, editor]() {
        const QString name = editor->text().trimmed();
        if (!name.isEmpty()) {
            // setWindowTitle on CDockWidget propagates to the tab label via
            // Qt's QEvent::WindowTitleChange handling inside CDockWidget.
            dw->setWindowTitle(name);
        }
        editor->deleteLater();
    };

    connect(editor, &QLineEdit::returnPressed, editor, commit);
    connect(editor, &QLineEdit::editingFinished, editor, commit);

    // Cancel on Escape via a local event filter
    struct EscFilter : QObject {
        QLineEdit* ed;
        explicit EscFilter(QLineEdit* e) : QObject(e), ed(e) {}
        bool eventFilter(QObject*, QEvent* e) override {
            if (e->type() == QEvent::KeyPress && static_cast<QKeyEvent*>(e)->key() == Qt::Key_Escape) {
                ed->deleteLater();
                return true;
            }
            return false;
        }
    };
    editor->installEventFilter(new EscFilter(editor));

    return true; // consume double-click so ADS doesn't float the widget
}

ads::CDockWidget* DockScreenRouter::create_dock_widget(const QString& id) {
    if (dock_widgets_.contains(id))
        return dock_widgets_[id];

    auto* dw = new ads::CDockWidget(manager_, title_for_id(id));
    dw->setObjectName(id);

    // Pinnable is intentionally excluded: the auto-hide pin button converts panels
    // to collapsible sidebars, which causes them to disappear when another panel
    // is opened alongside them. Movable/Floatable/Closable/Focusable are kept.
    dw->setFeatures(ads::CDockWidget::DockWidgetMovable | ads::CDockWidget::DockWidgetFloatable |
                    ads::CDockWidget::DockWidgetClosable | ads::CDockWidget::DockWidgetFocusable);

    // Use dock-widget minimum size (not content size) so screens with large
    // internal canvases (like DashboardCanvas) don't prevent side-by-side splits.
    dw->setMinimumSizeHintMode(ads::CDockWidget::MinimumSizeHintFromDockWidget);
    dw->setMinimumWidth(200);

    // Screen widgets must allow shrinking so ADS can create side-by-side splits.
    // Factory screens get a lightweight placeholder here; the real widget is swapped
    // in on first navigation via materialize_screen().
    if (screens_.contains(id)) {
        screens_[id]->setMinimumWidth(0);
        dw->setWidget(wrap_with_group_badge(id, screens_[id]));
    } else {
        auto* placeholder = new QWidget;
        auto* lbl = new QLabel(title_for_id(id));
        lbl->setAlignment(Qt::AlignCenter);
        lbl->setStyleSheet("color:#555;font-size:16px;");
        auto* vl = new QVBoxLayout(placeholder);
        vl->addWidget(lbl);
        dw->setWidget(placeholder);
    }

    // Intercept double-click on the title label inside CDockWidgetTab to allow
    // inline rename. We use the label (objectName "dockWidgetTabLabel"), NOT the
    // tab frame — ADS already uses tab frame double-click to undock/float.
    //
    // Phase 12 — also install the filter on the tab widget itself so right-click
    // anywhere on the tab (including the icon / padding) opens the context
    // menu. The event filter distinguishes by event type, not watched object.
    if (auto* tab = dw->tabWidget()) {
        tab->installEventFilter(this);
        tab->setProperty("dock_id", id);
        tab->setContextMenuPolicy(Qt::DefaultContextMenu);
        if (auto* lbl = tab->findChild<QLabel*>("dockWidgetTabLabel")) {
            lbl->installEventFilter(this);
            lbl->setProperty("dock_id", id);
        }
    }

    // Restore a previously saved custom title (user may have renamed this tab).
    const QString saved_title = load_tab_title(id);
    if (!saved_title.isEmpty())
        dw->setWindowTitle(saved_title);

    // Persist the title whenever the user renames it via the inline editor.
    connect(dw, &ads::CDockWidget::titleChanged, this, [this, id](const QString& title) {
        if (title != title_for_id(id)) // only save if user actually changed it
            save_tab_title(id, title);
    });

    // Materialize on first show; save state on hide.
    // Grid tracking (panel_count_, grid_*) is NOT updated here — hide signals
    // fire during exclusive resets and would corrupt the counter before the
    // new panel is placed. Grid is updated only inside navigate() itself.
    connect(dw, &ads::CDockWidget::visibilityChanged, this, [this, id](bool visible) {
        if (visible) {
            materialize_screen(id);
        } else {
            save_screen_state(id);
        }
    });

    dock_widgets_[id] = dw;
    return dw;
}

void DockScreenRouter::materialize_screen(const QString& id) {
    if (screens_.contains(id))
        return;

    auto fit = factories_.find(id);
    if (fit == factories_.end())
        return;

    LOG_INFO("DockRouter", "Lazy-constructing screen: " + id);
    QWidget* screen = nullptr;
    try {
        screen = fit.value()();
    } catch (const std::exception& e) {
        LOG_ERROR("DockRouter", QString("Factory threw for '%1': %2").arg(id, e.what()));
    } catch (...) {
        LOG_ERROR("DockRouter", QString("Factory threw unknown exception for '%1'").arg(id));
    }
    if (!screen) {
        LOG_ERROR("DockRouter", "Factory returned null for: " + id);
        return;
    }
    screen->setMinimumWidth(0);
    factories_.erase(fit);

    auto* dw = dock_widgets_.value(id, nullptr);
    if (!dw)
        return;

    QWidget* old = dw->widget();
    dw->setWidget(wrap_with_group_badge(id, screen));
    if (old) {
        old->setParent(nullptr);
        old->deleteLater();
    }
    screens_[id] = screen;
    restore_screen_state(id);
}

// ── Tab title persistence ─────────────────────────────────────────────────────

void DockScreenRouter::save_tab_title(const QString& id, const QString& title) {
    SessionManager::instance().save_tab_state("title/" + id, {{"title", title}});
}

QString DockScreenRouter::load_tab_title(const QString& id) const {
    const auto state = SessionManager::instance().load_tab_state("title/" + id);
    return state.value("title").toString();
}

// ── Screen state persistence (IStatefulScreen opt-in) ────────────────────────

void DockScreenRouter::save_screen_state(const QString& id) {
    auto* screen = screens_.value(id, nullptr);
    if (!screen)
        return;
    auto* stateful = dynamic_cast<screens::IStatefulScreen*>(screen);
    if (!stateful)
        return;
    ScreenStateManager::instance().save_now(stateful);
}

void DockScreenRouter::restore_screen_state(const QString& id) {
    auto* screen = screens_.value(id, nullptr);
    if (!screen)
        return;
    auto* stateful = dynamic_cast<screens::IStatefulScreen*>(screen);
    if (!stateful)
        return;
    ScreenStateManager::instance().restore(stateful);
}

void DockScreenRouter::apply_ads_theme() {
    // ADS's CSS (build_ads_qss on CDockManager) handles all ADS widget styling.
    // This function ensures tab widgets and labels don't have stale widget-level
    // QSS that could interfere with ADS's CSS cascade, and forces hidden tabs
    // visible (they can get stuck hidden after ensure_all_registered()).
    for (auto* dw : manager_->dockWidgetsMap()) {
        if (auto* tab = dw->tabWidget()) {
            if (!tab->styleSheet().isEmpty())
                tab->setStyleSheet(QString());
            if (tab->isHidden() && !dw->isClosed())
                tab->setVisible(true);
            for (auto* lbl : tab->findChildren<QLabel*>()) {
                if (!lbl->styleSheet().isEmpty())
                    lbl->setStyleSheet(QString());
            }
        }
    }
}

} // namespace fincept
