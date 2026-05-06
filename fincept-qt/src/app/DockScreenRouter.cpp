#include "app/DockScreenRouter.h"

#include "app/WindowFrame.h"
#include "auth/InactivityGuard.h"
#include "core/components/PopularityTracker.h"
#include "core/keys/WindowCycler.h"
#include "core/logging/Logger.h"
#include "core/panel/PanelHandle.h"
#include "core/panel/PanelRegistry.h"
#include "core/session/ScreenStateManager.h"
#include "core/session/SessionManager.h"
#include "core/window/WindowRegistry.h"
#include "core/symbol/IGroupLinked.h"
#include "core/symbol/SymbolContext.h"
#include "core/symbol/SymbolGroupRegistry.h"
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
    // The badge now lives inside the ADS tab title (next to close/float),
    // not inside the screen content area. Wrapping happens in
    // attach_group_badge_to_tab() after the CDockWidget is constructed —
    // this function is now a pass-through that just records the linked
    // pointer if the screen opts in. Existing call sites are unchanged.
    if (!screen)
        return screen;
    if (auto* linked = dynamic_cast<IGroupLinked*>(screen))
        group_linked_[id] = linked;
    attach_group_badge_to_tab(id, screen);
    return screen;
}

void DockScreenRouter::attach_group_badge_to_tab(const QString& id, QWidget* screen) {
    auto* dw = dock_widgets_.value(id, nullptr);
    if (!dw)
        return;
    auto* tab = dw->tabWidget();
    if (!tab)
        return;
    auto* layout = qobject_cast<QBoxLayout*>(tab->layout());
    if (!layout)
        return; // ADS layout not built yet — caller will retry on materialise

    // If a badge already exists (placeholder phase, or duplicate call), tear
    // it down so we can re-wire with the real screen's linked pointer. The
    // re-create is cheaper than juggling state in the existing widget.
    if (auto* existing = tab->findChild<ui::GroupBadge*>("dockTabBadge")) {
        layout->removeWidget(existing);
        existing->deleteLater();
    }

    auto* linked = dynamic_cast<IGroupLinked*>(screen);

    auto* badge = new ui::GroupBadge(tab);
    badge->setObjectName("dockTabBadge");
    badge->set_group_silent(linked ? linked->group() : SymbolGroup::None);
    if (!linked) {
        // Non-linked panel: still show the badge so the control is uniform,
        // but disable interaction — there is no IGroupLinked target.
        badge->setEnabled(false);
        badge->setToolTip(QStringLiteral("This panel doesn't support symbol groups"));
    }
    // Insert at index 0 — before the icon/title label.
    layout->insertWidget(0, badge);
    layout->insertSpacing(1, 6);

    if (!linked) {
        // No further wiring; the tab badge is decorative for this panel.
        group_badges_[id] = badge;
        return;
    }

    connect(badge, &ui::GroupBadge::group_change_requested, this,
            [linked, badge](SymbolGroup g) {
                linked->set_group(g);
                badge->set_group_silent(g);
                // If the new group already has an active symbol, push it
                // into the newly-linked screen immediately. Otherwise seed
                // the group from the screen's current symbol.
                const SymbolRef group_sym = SymbolContext::instance().group_symbol(g);
                if (group_sym.is_valid()) {
                    linked->on_group_symbol_changed(group_sym);
                } else if (g != SymbolGroup::None) {
                    const SymbolRef own = linked->current_symbol();
                    if (own.is_valid())
                        SymbolContext::instance().set_group_symbol(
                            g, own, dynamic_cast<QObject*>(linked));
                }
            });

    group_badges_[id] = badge;
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
    // must be no-ops. WindowFrame also disables the dock_manager_ widget tree
    // for keyboard/focus safety; this check catches programmatic callers.
    if (auth::InactivityGuard::instance().is_terminal_locked()) {
        LOG_DEBUG("DockRouter", QString("navigate('%1') suppressed — terminal locked").arg(id));
        return;
    }

    LOG_INFO("DockRouter",
             QString(">>> navigate('%1', exclusive=%2) opened_areas=%3").arg(id).arg(exclusive).arg(manager_ ? manager_->openedDockAreas().size() : 0));
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
    }

    // Determine whether this widget needs to be physically placed into a new
    // split area, or whether it already has its own dedicated area.
    //
    // ensure_all_registered() pre-creates every dock widget and groups them all
    // into a SINGLE shared dock area (all hidden). We detect this by checking
    // whether dw's area contains other dock widgets (shared) vs being solo.
    auto* existing_area = dw->dockAreaWidget();
    const bool area_is_poisoned = existing_area && existing_area->dockWidgets().size() > 1;
    const bool must_place = needs_add || !existing_area || area_is_poisoned;

    if (exclusive) {
        // Exclusive: replace whatever's open with this single panel in the
        // central area.
        manager_->addDockWidget(ads::CenterDockWidgetArea, dw);

    } else if (must_place) {
        // Phase 4 grid removal: ADS handles arbitrary splits natively. We
        // tab new panels into the *focused* dock area when one exists, and
        // create a fresh central area otherwise. The user can split panels
        // into a side-by-side layout via ADS drag-drop on the tab bar (the
        // existing right-click "Float Panel" / "Tear off into new window"
        // entries also remain). The pre-Phase-4 hard-coded
        // "right-of-TL → below-TL → right-of-BL" cycle was a v1
        // simplification; this is the cleaner Bloomberg-grade behaviour
        // (everything tabs into centre by default; user splits explicitly).
        const auto opened = manager_->openedDockAreas();
        ads::CDockAreaWidget* target = nullptr;
        if (auto* focused = manager_->focusedDockWidget()) {
            target = focused->dockAreaWidget();
        }
        if (!target && !opened.isEmpty()) {
            target = opened.last();
        }
        if (target)
            manager_->addDockWidget(ads::CenterDockWidgetArea, dw, target);
        else
            manager_->addDockWidget(ads::CenterDockWidgetArea, dw);

    } else {
        // Widget already has a dedicated area — just bring it to the front.
        LOG_DEBUG("DockRouter", QString("[%1] already placed, raising").arg(id));
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

    LOG_INFO("DockRouter", QString("add_alongside: primary=%1 visible=%2 secondary=%3 opened=%4")
                               .arg(primary)
                               .arg(primary_visible)
                               .arg(secondary)
                               .arg(manager_ ? manager_->openedDockAreas().size() : 0));

    if (!primary_visible) {
        navigate(primary, true);
        primary_dw = find_dock_widget(primary);
    }

    // Phase 4 (grid removal): add_alongside is the EXPLICIT "I want a
    // split" API. Pre-Phase-4 navigate() handled the split via the
    // hard-coded grid; post-removal navigate() tabs by default. So we
    // do the split here ourselves: ensure secondary's CDockWidget exists,
    // then addDockWidget(RightDockWidgetArea, secondary, primary's area).
    if (primary_dw) {
        auto* sec_dw = find_dock_widget(secondary);
        bool needs_add = !sec_dw;
        if (!sec_dw) {
            if (!factories_.contains(secondary) && !screens_.contains(secondary)) {
                LOG_WARN("DockRouter", "add_alongside: unknown secondary screen: " + secondary);
                return;
            }
            sec_dw = create_dock_widget(secondary);
        } else if (!sec_dw->dockManager()) {
            needs_add = true;
        }
        materialize_screen(secondary);

        auto* primary_area = primary_dw->dockAreaWidget();
        if (needs_add || !sec_dw->dockAreaWidget() ||
            sec_dw->dockAreaWidget()->dockWidgets().size() > 1) {
            if (primary_area)
                manager_->addDockWidget(ads::RightDockWidgetArea, sec_dw, primary_area);
            else
                manager_->addDockWidget(ads::CenterDockWidgetArea, sec_dw);
        }
        sec_dw->toggleView(true);
        if (auto* tab = sec_dw->tabWidget(); tab && tab->isHidden())
            tab->setVisible(true);
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
    // window. For docked tabs, the flag lives on the WindowFrame; we don't
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
            auto& registry = SymbolGroupRegistry::instance();

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

            // Only enabled slots are selectable; disabled ones are surfaced
            // as greyed-out entries so the full inventory stays visible.
            for (SymbolGroup g : all_symbol_groups()) {
                const bool en = registry.enabled(g);
                QString label = QStringLiteral("%1  (%2)")
                                    .arg(registry.name(g))
                                    .arg(symbol_group_letter(g));
                if (!en)
                    label += QStringLiteral("  — disabled");
                auto* a = group_menu->addAction(label);
                a->setCheckable(true);
                a->setChecked(g == current);
                a->setEnabled(en);
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

    // Phase 5: tear off into a new WindowFrame. Always offered; the call
    // itself logs + no-ops if the panel can't be torn off (eager screen
    // without a factory).
    auto* act_tear_off = menu.addAction("Tear off into new window");
    connect(act_tear_off, &QAction::triggered, this, [this, id]() { tear_off_to_new_frame(id); });

    // Phase 5: move to another open WindowFrame. Submenu lists every
    // other frame by display number; current frame is excluded. Hidden
    // entirely if there's only one frame (nothing to move to).
    {
        const auto frames = WindowRegistry::instance().frames();
        WindowFrame* this_frame = qobject_cast<WindowFrame*>(parent());
        // Count peers other than `this` to decide if the submenu is worth showing.
        int peers = 0;
        for (auto* f : frames)
            if (f && f != this_frame)
                ++peers;
        if (peers > 0) {
            auto* move_menu = menu.addMenu("Move to window");
            move_menu->setStyleSheet(menu.styleSheet());
            int display_num = 1;
            for (auto* f : frames) {
                if (!f) continue;
                if (f == this_frame) {
                    ++display_num;
                    continue;
                }
                const QString label = QString("Window %1").arg(display_num);
                auto* a = move_menu->addAction(label);
                connect(a, &QAction::triggered, this, [this, id, f]() {
                    if (f && f->dock_router())
                        move_panel_to_frame(id, f->dock_router());
                });
                ++display_num;
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

DockScreenRouter::~DockScreenRouter() {
    // Phase 4b cleanup: unregister every panel this router owns so the
    // PanelRegistry doesn't keep stale entries pointing at this frame
    // after WindowFrame destruction.
    for (auto it = instance_ids_.constBegin(); it != instance_ids_.constEnd(); ++it)
        PanelRegistry::instance().unregister_panel(it.value());
    instance_ids_.clear();
}

PanelInstanceId DockScreenRouter::panel_uuid_for(const QString& id) const {
    return instance_ids_.value(id);
}

void DockScreenRouter::tile_2x2() {
    // Phase 6 final / decision 5.5. Detach every open dock widget then
    // re-add with explicit ADS area hints to force the 2x2 grid.
    if (!manager_)
        return;
    QList<ads::CDockWidget*> open;
    for (auto* dw : manager_->dockWidgetsMap()) {
        if (dw && !dw->isClosed())
            open.append(dw);
    }
    if (open.isEmpty()) {
        LOG_DEBUG("DockRouter", "tile_2x2: no open panels");
        return;
    }
    LOG_INFO("DockRouter", QString("tile_2x2: rearranging %1 panel(s)").arg(open.size()));

    for (auto* dw : open)
        manager_->removeDockWidget(dw);

    ads::CDockAreaWidget* tl = nullptr;
    ads::CDockAreaWidget* tr = nullptr;
    ads::CDockAreaWidget* bl = nullptr;
    ads::CDockAreaWidget* br = nullptr;

    for (int i = 0; i < open.size(); ++i) {
        ads::CDockWidget* dw = open.at(i);
        if (i == 0) {
            tl = manager_->addDockWidget(ads::CenterDockWidgetArea, dw);
        } else if (i == 1) {
            tr = manager_->addDockWidget(ads::RightDockWidgetArea, dw, tl);
        } else if (i == 2) {
            bl = manager_->addDockWidget(ads::BottomDockWidgetArea, dw, tl);
        } else if (i == 3) {
            br = manager_->addDockWidget(ads::RightDockWidgetArea, dw, bl);
        } else {
            ads::CDockAreaWidget* target = br ? br : (bl ? bl : (tr ? tr : tl));
            if (target)
                manager_->addDockWidget(ads::CenterDockWidgetArea, dw, target);
            else
                manager_->addDockWidget(ads::CenterDockWidgetArea, dw);
        }
        dw->toggleView(true);
    }
    (void) tr; // suppress unused-variable warnings if compiler is strict
}

// ── Phase 5: tear-off + cross-frame move ────────────────────────────────────

void DockScreenRouter::adopt_panel_instance(const QString& id, PanelInstanceId instance_id) {
    if (id.isEmpty() || instance_id.is_null())
        return;

    instance_ids_.insert(id, instance_id);

    // Update the existing PanelHandle so PanelRegistry::find_by_frame
    // returns the right results for this frame's owning WindowFrame.
    if (auto* handle = PanelRegistry::instance().find(instance_id)) {
        WindowId frame_id;
        if (auto* frame = qobject_cast<WindowFrame*>(parent()))
            frame_id = frame->frame_uuid();
        handle->set_frame_id(frame_id);
        // dock_widget pointer will be repointed when create_dock_widget
        // runs in this router and assigns the freshly-created widget.
        handle->set_dock_widget(nullptr);
        handle->set_state(PanelHandle::State::Spawning);
    }
}

bool DockScreenRouter::move_panel_to_frame(const QString& id, DockScreenRouter* target) {
    if (!target || target == this) {
        LOG_DEBUG("DockRouter", QString("move_panel_to_frame('%1'): target invalid (null=%2 same=%3)")
                                    .arg(id).arg(target == nullptr).arg(target == this));
        return false;
    }
    auto* dw = dock_widgets_.value(id, nullptr);
    if (!dw) {
        LOG_WARN("DockRouter", QString("move_panel_to_frame('%1'): no dock widget").arg(id));
        return false;
    }
    if (target->dock_widgets_.contains(id)) {
        // Collision: the target already has a panel under this string id.
        // We could synthesise <id>#movedN but for v1 we refuse so the user
        // gets clear feedback. Caller (context menu / action) should
        // surface a toast / status message.
        LOG_WARN("DockRouter",
                 QString("move_panel_to_frame('%1'): target already has this panel id — refusing").arg(id));
        return false;
    }
    if (!factories_.contains(id) && !screens_.contains(id)) {
        // No factory and no eager screen — can't recreate on the target.
        LOG_WARN("DockRouter",
                 QString("move_panel_to_frame('%1'): no factory available, can't recreate on target").arg(id));
        return false;
    }

    LOG_INFO("DockRouter", QString("Moving panel '%1' to another frame").arg(id));

    // 1. Flush + sync-save state under the panel's UUID. The save itself
    //    is async (QtConcurrent), so there's a tight race where the target's
    //    restore could read a slightly stale row. SQLite WAL writes are
    //    sub-ms and widget materialise is 10-100ms, so practically the
    //    race almost never fires. Phase 8 formalises sync flush.
    auto* screen = screens_.value(id, nullptr);
    if (auto* stateful = dynamic_cast<screens::IStatefulScreen*>(screen)) {
        stateful->flush_pending_state();
        const PanelInstanceId uuid = instance_ids_.value(id);
        if (!uuid.is_null())
            ScreenStateManager::instance().save_now_by_uuid(stateful, uuid.to_string());
        else
            ScreenStateManager::instance().save_now(stateful);
    }

    // 2. Capture the UUID + factory so we can transfer them to the target.
    const PanelInstanceId panel_uuid = instance_ids_.value(id);
    ScreenFactory factory_copy;
    if (factories_.contains(id))
        factory_copy = factories_.value(id);

    // 3. Tear down the panel on this router. removeDockWidget unregisters
    //    from the manager; deleteLater destroys the widget. We purposely
    //    skip our own save_screen_state() because we already did the
    //    UUID-keyed save above — the visibility-changed handler would
    //    otherwise try a second save against a half-torn-down widget.
    manager_->removeDockWidget(dw);
    dw->deleteLater();
    dock_widgets_.remove(id);
    if (auto* sw = screens_.take(id))
        sw->deleteLater();
    factories_.remove(id);
    instance_ids_.remove(id);

    // 4. Set the target up. Transfer the factory + UUID before navigate()
    //    so target's create_dock_widget() reuses the existing UUID instead
    //    of minting a new one (and PanelRegistry's existing handle is
    //    re-used, frame_id repointed by adopt_panel_instance).
    if (factory_copy)
        target->factories_.insert(id, factory_copy);
    if (!panel_uuid.is_null())
        target->adopt_panel_instance(id, panel_uuid);

    // 5. Materialise on target. navigate() will create the dock widget,
    //    materialise the screen, and restore_screen_state will read the
    //    UUID-keyed row written in step 1.
    target->navigate(id);

    LOG_INFO("DockRouter", QString("Moved panel '%1' (uuid=%2) to target frame")
                               .arg(id, panel_uuid.to_string()));
    return true;
}

bool DockScreenRouter::tear_off_to_new_frame(const QString& id) {
    if (!dock_widgets_.contains(id)) {
        LOG_WARN("DockRouter", QString("tear_off_to_new_frame('%1'): no such panel").arg(id));
        return false;
    }
    if (!factories_.contains(id) && !screens_.contains(id)) {
        LOG_WARN("DockRouter",
                 QString("tear_off_to_new_frame('%1'): no factory available, can't tear off").arg(id));
        return false;
    }

    // Spawn a new frame on the next monitor. Reuses WindowCycler's smart
    // placement so tear-offs land somewhere sensible (decision 5.6).
    WindowCycler::instance().new_window_on_next_monitor();

    // Find the freshly-created frame. WindowCycler::new_window_on_next_monitor
    // doesn't return a pointer, but the new frame is the most-recently-added
    // entry in WindowRegistry.
    auto frames = WindowRegistry::instance().frames();
    if (frames.isEmpty()) {
        LOG_ERROR("DockRouter", "tear_off_to_new_frame: spawned frame not found in registry");
        return false;
    }
    // The newly-created frame has the highest window_id, so it's last in
    // the (ascending) sorted list.
    WindowFrame* new_frame = frames.last();
    if (!new_frame || !new_frame->dock_router()) {
        LOG_ERROR("DockRouter", "tear_off_to_new_frame: spawned frame has no router");
        return false;
    }

    return move_panel_to_frame(id, new_frame->dock_router());
}

ads::CDockWidget* DockScreenRouter::create_dock_widget(const QString& id) {
    if (dock_widgets_.contains(id))
        return dock_widgets_[id];

    auto* dw = new ads::CDockWidget(manager_, title_for_id(id));
    dw->setObjectName(id);

    // Phase 5 polish: DockWidgetFloatable removed alongside the global
    // DoubleClickUndocksWidget flag (see WindowFrame::setup_docking_mode).
    // Tear-off goes through the right-click menu's "Tear off into new
    // window" entry, which uses the close-and-recreate path that
    // preserves the panel UUID + state.
    //
    // Pinnable is intentionally excluded too: the auto-hide pin button
    // converts panels to collapsible sidebars, which causes them to
    // disappear when another panel is opened alongside them.
    dw->setFeatures(ads::CDockWidget::DockWidgetMovable |
                    ads::CDockWidget::DockWidgetClosable |
                    ads::CDockWidget::DockWidgetFocusable);

    // Use dock-widget minimum size (not content size) so screens with large
    // internal canvases (like DashboardCanvas) don't prevent side-by-side splits.
    dw->setMinimumSizeHintMode(ads::CDockWidget::MinimumSizeHintFromDockWidget);
    dw->setMinimumWidth(200);

    // Register the dock widget early so attach_group_badge_to_tab() (called
    // from wrap_with_group_badge below) can resolve the tab via dock_widgets_.
    dock_widgets_[id] = dw;

    // Phase 4b: mint a PanelInstanceId for this string id and register a
    // PanelHandle with PanelRegistry. The handle's frame_id comes from
    // the parent WindowFrame (the router's QObject parent). Two open
    // panels of the same screen type get distinct UUIDs because they go
    // through duplicate_panel() which calls create_dock_widget with a
    // synthetic `<base>#dup<N>` id — each gets its own UUID.
    if (!instance_ids_.contains(id)) {
        const PanelInstanceId panel_uuid = PanelInstanceId::generate();
        instance_ids_.insert(id, panel_uuid);

        WindowId frame_id; // null if the router's parent isn't a WindowFrame
        if (auto* frame = qobject_cast<WindowFrame*>(parent()))
            frame_id = frame->frame_uuid();

        // Strip any "#dup<N>" suffix from the type id so two watchlists
        // share a type_id of "watchlist" while having distinct instance_ids.
        QString type_id = id;
        const int dup_idx = type_id.indexOf("#dup");
        if (dup_idx > 0)
            type_id = type_id.left(dup_idx);

        auto* handle = new PanelHandle(panel_uuid, type_id, title_for_id(type_id), frame_id);
        handle->set_dock_widget(dw);
        handle->set_state(PanelHandle::State::Active); // dock widget is created
        PanelRegistry::instance().register_panel(handle);
    }

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
        // Attach a decorative `[·]` badge for factory screens too — once the
        // real screen materialises, attach_group_badge_to_tab is idempotent
        // and won't add a second badge, so the existing one stays in place.
        attach_group_badge_to_tab(id, placeholder);
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
    connect(dw, &ads::CDockWidget::visibilityChanged, this, [this, id](bool visible) {
        if (visible) {
            materialize_screen(id);
        } else {
            save_screen_state(id);
        }
    });

    // dock_widgets_[id] was registered earlier (before wrap_with_group_badge)
    // so attach_group_badge_to_tab could resolve the tab during construction.
    return dw;
}

void DockScreenRouter::prepare_dock_widget(const QString& id) {
    // Phase 8: pre-create the dock widget shell for restoreState matching.
    // Idempotent — a no-op if the widget already exists (likely registered
    // earlier by ensure_all_registered() at WindowFrame construction).
    if (dock_widgets_.contains(id))
        return;
    if (!factories_.contains(id) && !screens_.contains(id)) {
        LOG_WARN("DockRouter", QString("prepare_dock_widget: unknown id '%1'").arg(id));
        return;
    }
    auto* dw = create_dock_widget(id);
    if (!dw)
        return;

    // CRITICAL: register with the dock manager so restoreState can find
    // the widget by objectName via CDockManager::dockWidgetsMap. Without
    // this, the saved layout silently fails to restore. Mirrors
    // ensure_all_registered's "tab into shared area, hide" pattern so
    // we don't generate N vertical splits during the pre-restore pass.
    if (manager_) {
        manager_->addDockWidget(ads::CenterDockWidgetArea, dw);
        dw->toggleView(false); // hidden until restoreState repositions it
    }
}

void DockScreenRouter::materialize_now(const QString& id) {
    materialize_screen(id);
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
    // Phase 5: keep the factory in factories_ even after first materialise.
    // Pre-Phase-5 we erased it as a memory tidy-up, but tear-off / move-to-
    // window need to re-create the widget on a different router, which
    // requires the factory to still be available. The factory is a small
    // std::function — no meaningful memory cost. duplicate_panel's
    // factory-reinstall workaround (line ~626) becomes unnecessary too.

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

    // Phase 4c: prefer the UUID-keyed save path so two open panels of the
    // same screen type (e.g. two watchlists) get distinct rows in
    // screen_state. Fall back to the legacy screen-key path only if no
    // PanelInstanceId has been minted for this id — that would indicate
    // a screen registered before create_dock_widget() ran (shouldn't
    // happen in practice; defensive).
    const PanelInstanceId uuid = panel_uuid_for(id);
    if (!uuid.is_null()) {
        ScreenStateManager::instance().save_now_by_uuid(stateful, uuid.to_string());
    } else {
        LOG_DEBUG("DockRouter",
                  QString("save_screen_state('%1'): no PanelInstanceId, falling back to screen-key path").arg(id));
        ScreenStateManager::instance().save_now(stateful);
    }
}

void DockScreenRouter::restore_screen_state(const QString& id) {
    auto* screen = screens_.value(id, nullptr);
    if (!screen)
        return;
    auto* stateful = dynamic_cast<screens::IStatefulScreen*>(screen);
    if (!stateful)
        return;

    // Phase 4c: load via the UUID-keyed path; restore_by_uuid falls back to
    // the legacy screen_key path internally on first run for users
    // upgrading, so no state is lost across the migration.
    const PanelInstanceId uuid = panel_uuid_for(id);
    if (!uuid.is_null()) {
        ScreenStateManager::instance().restore_by_uuid(
            stateful, uuid.to_string(), /*fallback_to_screen_key=*/true);
    } else {
        LOG_DEBUG("DockRouter",
                  QString("restore_screen_state('%1'): no PanelInstanceId, falling back to screen-key path").arg(id));
        ScreenStateManager::instance().restore(stateful);
    }
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
