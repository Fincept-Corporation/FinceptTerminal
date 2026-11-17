// src/app/DockScreenRouter_Materialize.cpp
//
// Dock-widget construction + lazy materialisation: create_dock_widget,
// prepare_dock_widget, materialize_now, materialize_screen.
//
// Part of the partial-class split of DockScreenRouter.cpp.

#include "app/DockScreenRouter.h"
#include "app/DockStateFilter.h"
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
#include "core/telemetry/UsageReporter.h"
#include "core/window/WindowRegistry.h"
#include "screens/common/IStatefulScreen.h"
#include "ui/theme/Theme.h"
#include "ui/widgets/GroupBadge.h"

#include <QApplication>
#include <QClipboard>
#include <QContextMenuEvent>
#include <QElapsedTimer>
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
    dw->setFeatures(ads::CDockWidget::DockWidgetMovable | ads::CDockWidget::DockWidgetClosable |
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
        WindowId frame_id; // null if the router's parent isn't a WindowFrame
        int win_id = 0;
        if (auto* frame = qobject_cast<WindowFrame*>(parent())) {
            frame_id = frame->frame_uuid();
            win_id = frame->window_id();
        }

        // Derive the instance id DETERMINISTICALLY from window+dock id rather
        // than a fresh random uuid. The dock id is stable across restarts (the
        // ADS layout restores the same ids), so the same panel reuses the same
        // PanelInstanceId every launch — which is what lets ScreenStateManager
        // find the panel's saved UI state again (restore_by_uuid keys on this).
        // A random generate() minted a new id each launch, so per-screen state
        // (FNO underlying/expiry, equity symbol/watchlist, …) was written under
        // an id that never matched on the next run and was effectively lost.
        // window id is included so the same dock id in two windows stays unique.
        const PanelInstanceId panel_uuid = PanelInstanceId::from_name(QStringLiteral("w%1:%2").arg(win_id).arg(id));
        instance_ids_.insert(id, panel_uuid);

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
        // The dock widget's permanent content: a host the screen is later
        // BUILT INSIDE (materialize_screen passes it to the factory as the
        // parent), so the finished screen is never re-parented. Until then it
        // shows the panel title.
        auto* host = new QWidget;
        host->setObjectName(QLatin1String(kScreenHostName));
        // A stylesheet that styles nothing (one empty rule, matched only by
        // this host's own name), set before anything is built inside it. It
        // gives the host its own stylesheet style, which every widget created
        // inside it then shares. That is what makes the many setStyleSheet()
        // calls a screen makes while it is being built cheap: on a widget
        // already under a stylesheet style Qt just stores the text until the
        // widget is first shown, whereas on a widget without one it re-styles
        // the widget's whole subtree on the spot — and screens style their
        // containers last (refresh_theme() at the end of the constructor), so
        // that re-styled almost every widget of the screen once more.
        host->setStyleSheet(QStringLiteral("QWidget#dockScreenHost { }"));
        // Fill with the application palette's Window colour (the theme's base
        // background, kept current by ThemeManager) so placeholders don't flash
        // white/system-gray during the brief window between show() and
        // deferred screen materialization.
        host->setAutoFillBackground(true);
        auto* vl = new QVBoxLayout(host);
        vl->setContentsMargins(0, 0, 0, 0);
        vl->setSpacing(0);
        auto* lbl = new QLabel(title_for_id(id), host);
        lbl->setObjectName(QStringLiteral("dockScreenPlaceholder"));
        lbl->setAlignment(Qt::AlignCenter);
        lbl->setStyleSheet(QString("color:%1;font-size:" + fincept::ui::fonts::px(0) + "px;font-weight:500;").arg(QString(ui::colors::TEXT_DIM())));
        vl->addWidget(lbl);
        dw->setWidget(host);
        // Attach a decorative `[·]` badge for factory screens too — once the
        // real screen materialises, attach_group_badge_to_tab is idempotent
        // and won't add a second badge, so the existing one stays in place.
        attach_group_badge_to_tab(id, host);
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
    // A rename back to the default title clears the override (an empty saved
    // title means "none" in load_tab_title) — otherwise the older custom name
    // would come back on the next start.
    connect(dw, &ads::CDockWidget::titleChanged, this, [this, id](const QString& title) {
        save_tab_title(id, title == title_for_id(id) ? QString() : title);
    });

    // Materialize on first show; save state on hide.
    connect(dw, &ads::CDockWidget::visibilityChanged, this, [this, id](bool visible) {
        telemetry::UsageReporter::screen_visibility(this, id, visible, suppress_visibility_materialize_);
        if (visible) {
            // Bulk-register paths (ensure_all_registered, prepare_dock_widget)
            // transiently make the widget visible just to register it with the
            // manager before hiding it again with toggleView(false). Skip the
            // factory call in that window — the user hasn't asked to see this
            // panel yet, and materialising every registered screen at startup
            // is the cold-boot bug we're guarding against.
            if (suppress_visibility_materialize_)
                return;
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
    // Idempotent — a no-op if the manager already knows the widget (likely
    // registered earlier by ensure_all_registered()). A widget we own but that
    // prune_hidden_panels() detached from the manager is re-attached instead:
    // restoreState() matches through the manager's map only (#412).
    ads::CDockWidget* dw = dock_widgets_.value(id, nullptr);
    if (dw && manager_ && manager_->findDockWidget(id))
        return;
    if (!dw) {
        // "<base>#dup<N>" panels restored from a saved layout have no factory
        // of their own in a fresh session — borrow the base screen's.
        ensure_dup_factory(id);
        if (!factories_.contains(id) && !screens_.contains(id)) {
            if (!dock_state::is_single_instance_duplicate(id)) // that one was logged by ensure_dup_factory
                LOG_WARN("DockRouter", QString("Panel could not be prepared, unknown screen screen=%1").arg(id));
            return;
        }
        dw = create_dock_widget(id);
    }
    if (!dw)
        return;

    // CRITICAL: register with the dock manager so restoreState can find
    // the widget by objectName via CDockManager::dockWidgetsMap. Without
    // this, the saved layout silently fails to restore. Mirrors
    // ensure_all_registered's "tab into shared area, hide" pattern so
    // we don't generate N vertical splits during the pre-restore pass.
    if (manager_) {
        // Same rationale as ensure_all_registered: the transient
        // visibilityChanged(true) the addDockWidget emits must not
        // materialise the screen factory before the layout is restored.
        suppress_visibility_materialize_ = true;
        manager_->addDockWidget(ads::CenterDockWidgetArea, dw);
        dw->toggleView(false); // hidden until restoreState repositions it
        suppress_visibility_materialize_ = false;
    }
}

bool DockScreenRouter::ensure_dup_factory(const QString& id) {
    if (factories_.contains(id))
        return true;
    const int dup = id.indexOf(QLatin1String("#dup"));
    if (dup <= 0)
        return false;
    // A duplicate of a single-instance trading screen (saved before duplication
    // of those was refused) is never re-created: two order tickets for one
    // account. Without a factory it is not prepared, and restoreState() skips it.
    if (dock_state::is_single_instance_duplicate(id)) {
        LOG_INFO("DockRouter", QString("Saved duplicate of a single-instance screen dropped screen=%1").arg(id));
        return false;
    }
    const auto base = factories_.constFind(id.left(dup));
    if (base == factories_.constEnd())
        return false;
    const ScreenFactory factory = base.value(); // copy before inserting into the same hash
    factories_.insert(id, factory);
    return true;
}

void DockScreenRouter::prepare_duplicate_panels(const QByteArray& dock_state) {
    if (dock_state.isEmpty())
        return;
    // saveState() output is qCompress()ed XML unless compression is disabled;
    // mirror restoreState's own check (see also DockLayoutSelftest::analyze).
    const QByteArray xml = dock_state.startsWith("<?xml") ? dock_state : qUncompress(dock_state);
    if (xml.isEmpty())
        return;
    static const QRegularExpression dup_re(QStringLiteral("<Widget\\b[^>]*\\bName=\"([^\"]+#dup\\d+)\""));
    auto it = dup_re.globalMatch(QString::fromUtf8(xml));
    while (it.hasNext())
        prepare_dock_widget(it.next().captured(1)); // idempotent
}

void DockScreenRouter::materialize_now(const QString& id) {
    // A screen never opened in this window has no dock widget: give it a hidden
    // shell (not added to the dock manager — navigate() places it when the user
    // opens it) so it can be built without being shown.
    if (!dock_widgets_.contains(id)) {
        ensure_dup_factory(id);
        if (!factories_.contains(id) && !screens_.contains(id))
            return;
        create_dock_widget(id);
    }
    materialize_screen(id);
}

namespace {
/// Runs a callable when the scope ends, on every path out of it.
class DrmScopeExit {
  public:
    explicit DrmScopeExit(std::function<void()> f) : f_(std::move(f)) {}
    ~DrmScopeExit() {
        if (f_)
            f_();
    }
    DrmScopeExit(const DrmScopeExit&) = delete;
    DrmScopeExit& operator=(const DrmScopeExit&) = delete;

  private:
    std::function<void()> f_;
};
} // namespace

QWidget* DockScreenRouter::materialize_screen(const QString& id) {
    // One build per id: the existing screen, or nothing while a build of this id
    // is still running further up the stack. That build used to be invisible
    // here — the screen was recorded only after its factory AND its first show
    // had run — so a visibilityChanged(true) or navigate() for the same id that
    // arrived in between (the first show of a screen runs arbitrary code; a
    // chart's QWebEngineView can make Qt re-create and re-show the whole window)
    // built a second screen into the same content host: two Equity Trading
    // screens stacked in one panel.
    switch (dock_instance::materialise_decision(screens_.contains(id), building_.contains(id))) {
        case dock_instance::Build::Existing:
            return screens_.value(id);
        case dock_instance::Build::Join:
            LOG_DEBUG("DockRouter", QString("Screen build joined, already in progress screen=%1").arg(id));
            return nullptr;
        case dock_instance::Build::Create:
            break;
    }

    const auto fit = factories_.constFind(id);
    if (fit == factories_.constEnd())
        return nullptr;
    auto* dw = dock_widgets_.value(id, nullptr);
    if (!dw)
        return nullptr; // nowhere to put it (callers create the dock widget first)
    // A copy: the factory may run code that inserts into factories_ (a screen
    // opening another one), which would move the stored function mid-call.
    const ScreenFactory factory = fit.value();

    building_.insert(id);
    const DrmScopeExit done([this, id]() {
        building_.remove(id);
        settle(id);
    });

    // Build the screen inside the dock widget's content host, so the finished
    // tree is never re-parented (see ScreenFactory). Dock widgets made before
    // the host existed (none today) fall back to swapping the content.
    QWidget* current = dw->widget();
    QWidget* host = current && current->objectName() == QLatin1String(kScreenHostName) ? current : nullptr;

    QWidget* screen = nullptr;
    QElapsedTimer build_timer;
    build_timer.start();
    try {
        screen = factory(host);
    } catch (const std::exception& e) {
        LOG_ERROR("DockRouter", QString("Screen construction failed screen=%1 error=\"%2\"").arg(id, e.what()));
    } catch (...) {
        LOG_ERROR("DockRouter", QString("Screen construction failed screen=%1 error=\"unknown exception\"").arg(id));
    }
    if (!screen) {
        LOG_ERROR("DockRouter", "Screen construction failed screen=" + id + " error=\"factory returned null\"");
        return nullptr;
    }
    screen->setProperty(kScreenIdProperty, id);
    // Constructor cost of the screen (what the user waits for on first open,
    // before it is polished and painted). Kept on the widget for the smoke
    // walk's timing report (FINCEPT_SMOKE_TIMING).
    const qint64 factory_ms = build_timer.elapsed();
    screen->setProperty("_fincept_factory_ms", factory_ms);
    LOG_DEBUG("DockRouter", QString("Screen constructed screen=%1 duration_ms=%2").arg(id).arg(factory_ms));
    screen->setMinimumWidth(0);
    // Phase 5: keep the factory in factories_ even after first materialise.
    // Pre-Phase-5 we erased it as a memory tidy-up, but tear-off / move-to-
    // window need to re-create the widget on a different router, which
    // requires the factory to still be available. The factory is a small
    // std::function — no meaningful memory cost. duplicate_panel's
    // factory-reinstall workaround (line ~626) becomes unnecessary too.

    if (host && screen->parentWidget() == host) {
        // Built in place: drop the title placeholder and lay the screen out.
        if (auto* placeholder = host->findChild<QLabel*>(QStringLiteral("dockScreenPlaceholder"),
                                                          Qt::FindDirectChildrenOnly)) {
            placeholder->hide();
            placeholder->deleteLater();
        }
        if (auto* box = qobject_cast<QBoxLayout*>(host->layout()))
            box->addWidget(screen, 1);
        wrap_with_group_badge(id, screen); // records the IGroupLinked pointer, builds the tab badge
        screen->show();                    // a child created under a visible parent starts hidden
    } else {
        QWidget* old = dw->widget();
        dw->setWidget(wrap_with_group_badge(id, screen));
        if (old) {
            old->setParent(nullptr);
            old->deleteLater();
        }
    }
    screens_[id] = screen;
    restore_screen_state(id);

    // Final check: exactly one instance of this screen in its panel, and one panel
    // of a single-instance type in this window. Anything else is removed here.
    if (instance_count(id) != 1 || dock_instance::is_single_instance_type(id))
        collapse_duplicates(QStringLiteral("build"));
    LOG_DEBUG("DockRouter", QString("materialised %1 instances=%2").arg(id).arg(instance_count(id)));
    return screen;
}

// ── One instance per dock id ──────────────────────────────────────────────────

bool DockScreenRouter::defer_while_busy(const QString& id, std::function<void(QWidget*)> fn) {
    if (!is_busy(id))
        return false;
    after_busy_[id].append(std::move(fn));
    return true;
}

void DockScreenRouter::settle(const QString& id) {
    if (is_busy(id))
        return; // the outer build / navigate of this id settles it
    const auto it = after_busy_.find(id);
    if (it == after_busy_.end())
        return;
    const QList<std::function<void(QWidget*)>> work = std::move(it.value());
    after_busy_.erase(it);
    // Queued: this runs at the tail of a build or a navigate, possibly deep inside
    // a nested event loop; the work (a navigate, a symbol hand-over) runs from the
    // event loop instead, on a settled router.
    for (const auto& fn : work) {
        QMetaObject::invokeMethod(
            this, [this, id, fn]() { fn(screens_.value(id)); }, Qt::QueuedConnection);
    }
}

bool DockScreenRouter::when_materialised(const QString& id_in, std::function<void(QWidget*)> fn) {
    if (!fn)
        return false;
    const QString id = screen_alias::canonical(id_in); // an alias waits for its screen
    if (defer_while_busy(id, fn))
        return true;
    QWidget* screen = screens_.value(id);
    if (!screen)
        return false;
    fn(screen);
    return true;
}

int DockScreenRouter::instance_count(const QString& id) const {
    const QString type = dock_instance::base_type(screen_alias::canonical(id));
    int n = 0;
    for (auto it = dock_widgets_.cbegin(); it != dock_widgets_.cend(); ++it) {
        if (dock_instance::base_type(it.key()) != type || !it.value())
            continue;
        QWidget* content = it.value()->widget();
        if (!content)
            continue;
        if (!content->property(kScreenIdProperty).toString().isEmpty()) {
            ++n; // an eagerly registered screen is the dock widget's content itself
            continue;
        }
        const auto children = content->findChildren<QWidget*>(QString(), Qt::FindDirectChildrenOnly);
        for (QWidget* w : children)
            if (!w->property(kScreenIdProperty).toString().isEmpty())
                ++n;
    }
    return n;
}

int DockScreenRouter::collapse_duplicates(const QString& where) {
    int removed = 0;

    // 1. A second screen inside one panel's content host.
    for (auto it = dock_widgets_.cbegin(); it != dock_widgets_.cend(); ++it) {
        const QString& id = it.key();
        if (building_.contains(id) && !screens_.contains(id))
            continue; // still under construction: its build checks the panel when it ends
        QWidget* content = it.value() ? it.value()->widget() : nullptr;
        if (!content || content->objectName() != QLatin1String(kScreenHostName))
            continue;
        QList<QWidget*> found;
        for (QWidget* w : content->findChildren<QWidget*>(QString(), Qt::FindDirectChildrenOnly))
            if (w->property(kScreenIdProperty).toString() == id)
                found << w;
        if (found.size() < 2)
            continue;
        QList<dock_instance::Instance> instances;
        for (QWidget* w : std::as_const(found))
            instances << dock_instance::Instance{id, w == screens_.value(id), w->isVisible()};
        const int keep = dock_instance::keeper(instances);
        for (int i = 0; i < found.size(); ++i) {
            if (i == keep)
                continue;
            QWidget* extra = found.at(i);
            LOG_WARN("DockRouter", QString("Second instance of a screen removed screen=%1 instances=%2 where=%3")
                                       .arg(id)
                                       .arg(found.size())
                                       .arg(where));
            extra->hide();
            if (auto* box = content->layout())
                box->removeWidget(extra);
            extra->deleteLater();
            ++removed;
        }
        if (screens_.value(id) != found.at(keep)) {
            screens_[id] = found.at(keep);
            wrap_with_group_badge(id, found.at(keep));
        }
    }

    // 2. A surplus panel of a single-instance type ("equity_trading#dup2" next to
    //    "equity_trading", from an older layout or any path not seen yet).
    QList<dock_instance::Instance> panels;
    for (auto it = dock_widgets_.cbegin(); it != dock_widgets_.cend(); ++it) {
        if (!dock_instance::is_single_instance_type(it.key()))
            continue;
        panels << dock_instance::Instance{it.key(), false, it.value() && !it.value()->isClosed()};
    }
    for (const QString& id : dock_instance::surplus_panels(panels)) {
        if (is_busy(id))
            continue; // never under a build / navigate of that very panel
        LOG_WARN("DockRouter", QString("Surplus panel of a single-instance screen closed screen=%1 type=%2 where=%3")
                                   .arg(id, dock_instance::base_type(id), where));
        discard_panel(id);
        ++removed;
    }
    return removed;
}

void DockScreenRouter::discard_panel(const QString& id) {
    ads::CDockWidget* dw = dock_widgets_.take(id);
    group_linked_.remove(id);
    group_badges_.remove(id);
    pending_link_groups_.remove(id);
    QWidget* screen = screens_.take(id);
    if (id.contains(QLatin1String("#dup")))
        factories_.remove(id);
    const PanelInstanceId uuid = instance_ids_.take(id);
    if (!uuid.is_null())
        PanelRegistry::instance().unregister_panel(uuid);
    if (dw) {
        // Closed before it is removed, so its screen saves nothing more and the
        // layout save that follows no longer names it.
        const bool prev = suppress_visibility_materialize_;
        suppress_visibility_materialize_ = true;
        if (!dw->isClosed())
            dw->toggleView(false);
        if (manager_ && dw->dockManager() == manager_ && manager_->findDockWidget(id) == dw)
            manager_->removeDockWidget(dw);
        suppress_visibility_materialize_ = prev;
        dw->deleteLater(); // takes its content host and the screen in it along
    } else if (screen) {
        screen->deleteLater();
    }
    if (current_id_ == id)
        current_id_.clear();
}

// ── Tab title persistence ─────────────────────────────────────────────────────

} // namespace fincept
