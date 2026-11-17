// src/app/DockScreenRouter_Navigation.cpp
//
// User-driven panel navigation: navigate (open/exclusive), tab_into,
// add_alongside, remove_screen, replace_screen, ensure_all_registered.
// These are the entry points the rest of the app calls when the user picks
// a screen from the command palette / dock toolbar / etc.
//
// Part of the partial-class split of DockScreenRouter.cpp.

#include "core/debug/UiStallMonitor.h"
#include "app/DockInstanceGuard.h"
#include "app/DockScreenRouter.h"
#include "app/ScreenAliases.h"
#include "app/WindowFrame.h"
#include "auth/InactivityGuard.h"
#include "core/components/PopularityTracker.h"
#include "core/keys/WindowCycler.h"
#include "core/logging/Logger.h"
#include "core/panel/PanelHandle.h"
#include "core/panel/PanelRegistry.h"
#include "core/session/ScreenStateManager.h"
#include "core/session/SessionManager.h"
#include "core/symbol/IGroupLinked.h"
#include "core/symbol/SymbolContext.h"
#include "core/symbol/SymbolGroupRegistry.h"
#include "core/symbol/SymbolRef.h"
#include "core/window/WindowRegistry.h"
#include "screens/common/ISectionLinked.h"
#include "screens/common/IStatefulScreen.h"
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
#include <QRegularExpression>
#include <QVBoxLayout>

#include <DockAreaWidget.h>
#include <DockManager.h>
#include <DockWidget.h>
#include <DockWidgetTab.h>

namespace fincept {

void DockScreenRouter::open_alias_section(const QString& alias, const QString& canonical) {
    const QString section = screen_alias::section_for(alias);
    if (section.isEmpty())
        return;
    when_materialised(canonical, [section](QWidget* screen) {
        if (auto* linked = dynamic_cast<screens::ISectionLinked*>(screen))
            linked->open_section(section, {});
    });
}

void DockScreenRouter::navigate(const QString& id, bool exclusive) {
    // An old screen id that lives on as an alias (ScreenAliases.h): its screen, on its section.
    if (const QString canonical = screen_alias::canonical(id); canonical != id) {
        LOG_DEBUG("DockRouter", QString("Screen alias resolved alias=%1 screen=%2").arg(id, canonical));
        navigate(canonical, exclusive);
        open_alias_section(id, canonical);
        return;
    }
    // Label any GUI stall from here on with the screen being opened.
    debug::UiStallMonitor::set_context(QStringLiteral("screen=") + id);
    // Hard refuse while the lock screen is showing — signals/timers/keyboard
    // shortcuts that would otherwise mutate panel state behind the PIN gate
    // must be no-ops. WindowFrame also disables the dock_manager_ widget tree
    // for keyboard/focus safety; this check catches programmatic callers.
    if (auth::InactivityGuard::instance().is_terminal_locked()) {
        LOG_DEBUG("DockRouter", QString("navigate('%1') suppressed — terminal locked").arg(id));
        return;
    }

    // One navigate per id at a time (DockInstanceGuard.h). A navigate for this id
    // that arrives while one is already running — from a nested event loop or a
    // re-show of the panel during its first build — joins it: the running one
    // places, shows and builds the panel and announces it. One that arrives while
    // only a build is running (a hidden build, a layout restore) is replayed when
    // that build has finished.
    switch (dock_instance::navigate_decision(navigating_.contains(id), building_.contains(id))) {
        case dock_instance::Nav::Join:
            LOG_DEBUG("DockRouter", QString("Screen open joined, already being opened screen=%1").arg(id));
            // A replacing open joined to an adding one keeps its meaning: once the
            // panel is up, the other panels are closed (no second build — it exists).
            if (exclusive)
                defer_while_busy(id, [this, id](QWidget*) { navigate(id, true); });
            return;
        case dock_instance::Nav::Defer:
            LOG_DEBUG("DockRouter", QString("Screen open deferred until its build ends screen=%1").arg(id));
            defer_while_busy(id, [this, id, exclusive](QWidget*) { navigate(id, exclusive); });
            return;
        case dock_instance::Nav::Proceed:
            break;
    }
    navigating_.insert(id);
    struct NavigatingScope {
        DockScreenRouter* router;
        QSet<QString>* set;
        QString id;
        ~NavigatingScope() {
            set->remove(id);
            router->settle(id);
        }
    } const navigating_scope{this, &navigating_, id};

    LOG_DEBUG("DockRouter", QString("Screen open requested screen=%1 exclusive=%2 opened_areas=%3")
                               .arg(id)
                               .arg(exclusive)
                               .arg(manager_ ? manager_->openedDockAreas().size() : 0));
    // Bump popularity so the Component Browser surfaces frequently-used
    // panels at the top. Safe for unknown ids — PopularityTracker no-ops
    // on failure and we still fall into the unknown-screen branch below.
    PopularityTracker::instance().increment(id);

    auto* dw = find_dock_widget(id);

    bool needs_add = false;
    if (!dw) {
        // Screen not yet in dock manager — check if we know how to create it.
        // A "<base>#dup<N>" id from a previous session borrows the base factory.
        ensure_dup_factory(id);
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

    // A screen that exists but is not inside this dock widget (an eagerly
    // registered one) is installed as its content. Factory screens live in
    // the dock widget's content host, so the test is ancestry, not equality.
    if (screens_.contains(id) && !dw->isAncestorOf(screens_[id])) {
        QWidget* old = dw->widget();
        dw->setWidget(screens_[id]);
        if (old) {
            old->setParent(nullptr);
            old->deleteLater();
        }
    }

    // NOT materialised here: the screen is built after the dock widget has
    // been placed (below), inside its final parent, so the finished widget
    // tree is never moved — moving it re-computes every widget's stylesheet.

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
    //
    // "Shared" means the HIDDEN placeholder area of ensure_all_registered(), not
    // any area with several tabs: a tab group the user built (Equity Trading +
    // Watchlist, or a panel and its duplicate) is an OPEN area with several
    // widgets. Treating that as "poisoned" re-placed the panel and re-tiled the
    // whole group into stacked splits on a plain "open in Equity Trading" from
    // another screen. An open area is the panel's real home: activate it there.
    auto* existing_area = dw->dockAreaWidget();
    const bool area_is_open = existing_area && manager_->openedDockAreas().contains(existing_area);
    const bool area_is_poisoned = existing_area && existing_area->dockWidgets().size() > 1 && !area_is_open;
    const bool must_place = needs_add || !existing_area || area_is_poisoned;

    // A panel that still sits alone in its own area of the main container
    // (closed by an earlier exclusive switch) is re-opened where it is: with
    // every other panel closed it fills the container all the same, and not
    // moving it spares re-styling its whole screen (ADS re-parents a dock
    // widget on every move).
    const bool reopen_in_place = exclusive && !must_place && existing_area &&
                                 existing_area->dockContainer() == manager_ && !existing_area->isAutoHide();

    // Placement (and the re-grid) shows the dock widget, which would build the
    // screen at once — and then retile_grid() would move it. Build it only
    // after the panel has reached its final place (toggleView(true) below).
    const bool prev_suppress = suppress_visibility_materialize_;
    suppress_visibility_materialize_ = true;
    if (reopen_in_place) {
        LOG_DEBUG("DockRouter", QString("[%1] re-opened in its own area").arg(id));
    } else if (exclusive) {
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
        // simplification; this is the cleaner Fincept-grade behaviour
        // (everything tabs into centre by default; user splits explicitly).
        // Product behaviour: a newly-added panel auto-tiles into the next grid
        // slot rather than stacking as a tab. Place it anywhere first so it's in
        // a layout, then re-grid all open panels (newest lands in the next slot:
        // 2→left|right, 3→full-width bottom, 4→2x2, 5+→tab into bottom-right).
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
        retile_grid(dw);

    } else {
        // Widget already has a dedicated area — just bring it to the front.
        LOG_DEBUG("DockRouter", QString("[%1] already placed, raising").arg(id));
    }
    suppress_visibility_materialize_ = prev_suppress;

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
    // Normally already done by the visibilityChanged(true) the placement above
    // emitted; this covers a dock widget that was already showing.
    materialize_screen(id);

    apply_ads_theme();

    current_id_ = id;
    if (auto* frame = qobject_cast<WindowFrame*>(parent()))
        SessionManager::instance().set_last_screen(frame->window_id(), id);
    LOG_DEBUG("DockRouter", "Screen opened screen=" + id);
    emit screen_changed(id);
}

void DockScreenRouter::tab_into(const QString& id) {
    if (const QString canonical = screen_alias::canonical(id); canonical != id) {
        tab_into(canonical);
        open_alias_section(id, canonical);
        return;
    }
    if (!factories_.contains(id) && !screens_.contains(id)) {
        LOG_WARN("DockRouter", "Unknown screen: " + id);
        return;
    }
    // Never re-place a panel while it is being opened or built: run once that ends.
    if (defer_while_busy(id, [this, id](QWidget*) { tab_into(id); })) {
        LOG_DEBUG("DockRouter", QString("Screen tab-in deferred until its open ends screen=%1").arg(id));
        return;
    }

    auto* dw = find_dock_widget(id);
    bool needs_add = !dw || !dw->dockManager();

    if (!dw) {
        dw = create_dock_widget(id);
        needs_add = true;
    }

    // If already visible somewhere just raise it — no need to re-tab.
    if (!dw->isClosed() && dw->dockAreaWidget()) {
        dw->raise();
        dw->setAsCurrentTab();
        materialize_screen(id);
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
        // Build the screen only once it is in place (see navigate()).
        const bool prev_suppress = suppress_visibility_materialize_;
        suppress_visibility_materialize_ = true;
        if (target_area)
            // Tab into the existing area — CenterDockWidgetArea = same area, new tab
            manager_->addDockWidget(ads::CenterDockWidgetArea, dw, target_area);
        else
            // No area open yet — open as the sole screen
            manager_->addDockWidget(ads::CenterDockWidgetArea, dw);
        suppress_visibility_materialize_ = prev_suppress;
    }

    dw->toggleView(true);
    if (auto* tab = dw->tabWidget()) {
        if (tab->isHidden())
            tab->setVisible(true);
    }
    dw->raise();
    dw->setAsCurrentTab();
    materialize_screen(id);

    current_id_ = id;
    if (auto* frame = qobject_cast<WindowFrame*>(parent()))
        SessionManager::instance().set_last_screen(frame->window_id(), id);
    LOG_DEBUG("DockRouter", "Screen opened screen=" + id + " mode=tab");
    emit screen_changed(id);
}

void DockScreenRouter::add_alongside(const QString& primary, const QString& secondary) {
    if (screen_alias::is_alias(primary) || screen_alias::is_alias(secondary)) {
        const QString p = screen_alias::canonical(primary);
        const QString s = screen_alias::canonical(secondary);
        if (p == s)
            navigate(primary); // both name the same screen: one panel (and the alias's section)
        else
            add_alongside(p, s);
        if (screen_alias::is_alias(secondary))
            open_alias_section(secondary, s);
        return;
    }
    // Never re-place a panel while it is being opened or built: run once that ends.
    if (defer_while_busy(secondary, [this, primary, secondary](QWidget*) { add_alongside(primary, secondary); })) {
        LOG_DEBUG("DockRouter", QString("Screen add deferred until its open ends screen=%1").arg(secondary));
        return;
    }
    // If primary is already open, just add secondary into the next grid slot
    // without resetting the layout. This allows building up to a 2x2 grid
    // (4 panels) incrementally via repeated "add" commands.
    auto* primary_dw = find_dock_widget(primary);
    bool primary_visible = primary_dw && !primary_dw->isClosed() && primary_dw->dockAreaWidget();

    LOG_DEBUG("DockRouter", QString("Screen opened alongside primary=%1 visible=%2 screen=%3 opened_areas=%4")
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
                LOG_WARN("DockRouter", "Screen could not be opened alongside, unknown screen screen=" + secondary);
                return;
            }
            sec_dw = create_dock_widget(secondary);
        } else if (!sec_dw->dockManager()) {
            needs_add = true;
        }
        auto* primary_area = primary_dw->dockAreaWidget();
        if (needs_add || !sec_dw->dockAreaWidget() || sec_dw->dockAreaWidget()->dockWidgets().size() > 1) {
            // Build the screen only once it is in place (see navigate()).
            const bool prev_suppress = suppress_visibility_materialize_;
            suppress_visibility_materialize_ = true;
            if (primary_area)
                manager_->addDockWidget(ads::RightDockWidgetArea, sec_dw, primary_area);
            else
                manager_->addDockWidget(ads::CenterDockWidgetArea, sec_dw);
            suppress_visibility_materialize_ = prev_suppress;
        }
        sec_dw->toggleView(true);
        if (auto* tab = sec_dw->tabWidget(); tab && tab->isHidden())
            tab->setVisible(true);
        materialize_screen(secondary);
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
    if (screen_alias::is_alias(primary)) {
        remove_screen(screen_alias::canonical(primary));
        open_alias_section(primary, screen_alias::canonical(primary));
        return;
    }
    // Close all panels except primary, which expands to fill the area.
    auto* keep = find_dock_widget(primary);
    if (!keep || !keep->dockManager()) {
        // Never built, or closed earlier and detached by prune_hidden_panels():
        // it has no dock area to expand into, so the loop below would close
        // every other panel and toggleView(true) would then show nothing — an
        // empty frame. navigate(exclusive) creates / re-attaches it and closes
        // the rest itself (and refuses an unknown id without closing anything).
        navigate(primary, /*exclusive=*/true);
        return;
    }
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
    register_hidden(all_ids);
}

void DockScreenRouter::ensure_registered_for_state(const QByteArray& dock_state) {
    // restoreState() looks up only the panels the saved layout names, so only
    // those need a registered shell. Registering every screen (~50 dock
    // widgets, each with its tab and content host, moved through the dock
    // manager) made restoring a layout take seconds at start-up and on every
    // layout switch.
    const QByteArray xml = dock_state.startsWith("<?xml") ? dock_state : qUncompress(dock_state);
    if (xml.isEmpty()) {
        ensure_all_registered(); // unreadable blob: the old, complete registration
        return;
    }
    static const QRegularExpression name_re(QStringLiteral("<Widget\\b[^>]*\\bName=\"([^\"]+)\""));
    QStringList ids;
    auto it = name_re.globalMatch(QString::fromUtf8(xml));
    while (it.hasNext()) {
        const QString id = it.next().captured(1);
        ensure_dup_factory(id); // "<base>#dup<N>" borrows its base screen's factory
        if ((factories_.contains(id) || screens_.contains(id)) && !ids.contains(id))
            ids.append(id);
    }
    register_hidden(ids);
}

void DockScreenRouter::register_hidden(const QStringList& all_ids) {
    // Bulk register all factories. The transient visibilityChanged(true)
    // each addDockWidget emits would otherwise eagerly materialise every
    // screen — see suppress_visibility_materialize_ docs.
    suppress_visibility_materialize_ = true;
    ads::CDockAreaWidget* first_area = nullptr;
    for (const QString& id : all_ids) {
        ads::CDockWidget* dw = dock_widgets_.value(id, nullptr);
        // "Registered" means the MANAGER can find it by objectName — that is
        // the lookup restoreState() does. Owning it in dock_widgets_ is not
        // enough: prune_hidden_panels() removeDockWidget()s every closed panel
        // but keeps it alive here, so after the user navigated away from a
        // panel, applying a saved layout that contains it found nothing to
        // place and restored an empty frame (#412).
        if (dw && manager_->findDockWidget(id))
            continue;
        if (!dw)
            dw = create_dock_widget(id);
        // Add all widgets as tabs in the SAME dock area to avoid
        // creating 47 vertical splits. The first widget creates the area,
        // all subsequent ones tab into it.
        if (first_area)
            manager_->addDockWidget(ads::CenterDockWidgetArea, dw, first_area);
        else
            first_area = manager_->addDockWidget(ads::CenterDockWidgetArea, dw);
        dw->toggleView(false); // start hidden
    }
    suppress_visibility_materialize_ = false;
    // Apply theme directly to all title bars and tabs created above.
    apply_ads_theme();
}

} // namespace fincept
