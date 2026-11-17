#pragma once
#include "app/ScreenAliases.h"
#include "core/identity/Uuid.h"
#include "core/symbol/SymbolGroup.h"

#include <QHash>
#include <QList>
#include <QObject>
#include <QPointer>
#include <QSet>

#include <DockManager.h>
#include <DockWidget.h>
#include <functional>

namespace fincept::ui {
class GroupBadge;
}

namespace fincept {

class IGroupLinked;
struct SymbolRef;

namespace dock_router_link_defaults {
/// Record that the user chose link groups themselves (tab badge, panel menu or
/// command palette). Ends the one-time "core screens join group A" default.
void mark_user_configured_links();
} // namespace dock_router_link_defaults

/// ADS-aware screen router. Each screen is wrapped in a CDockWidget that can be
/// docked, tabbed, floated, or auto-hidden. Mirrors ScreenRouter's API so the
/// transition is straightforward.
///
/// Lazy loading: factories are stored and only called when a screen is first
/// navigated to (or when ensure_all_registered() pre-creates placeholders for
/// state restore).
///
/// One instance per dock id, by construction: a screen's factory runs only
/// through materialize_screen(), which returns the existing screen when there is
/// one and joins a build of the same id that is still running (a nested
/// visibilityChanged / navigate during the first build's construction or first
/// show) instead of building a second one. navigate() for an id whose navigate
/// is already running joins it too. Single-instance types (DockInstanceGuard.h;
/// the default) also never get a "#dup" copy, and collapse_duplicates() removes
/// any second instance that is found anyway (old saved layouts included).
class DockScreenRouter : public QObject {
    Q_OBJECT
  public:
    /// Builds a screen as a child of `parent` — the content host that already
    /// sits inside the screen's dock widget. Constructing in place matters for
    /// speed: re-parenting a finished widget tree makes Qt recompute the
    /// stylesheet of every widget in it, and that pass was most of the cost of
    /// opening a screen. A factory may ignore `parent`; the router then
    /// re-parents the result (correct, just slower).
    using ScreenFactory = std::function<QWidget*(QWidget* parent)>;

    /// objectName of that per-dock-widget content host.
    static constexpr const char* kScreenHostName = "dockScreenHost";

    /// Dynamic property every screen the router builds or is given carries: its
    /// dock id. How collapse_duplicates() recognises a second instance.
    static constexpr const char* kScreenIdProperty = "_fincept_dock_id";

    explicit DockScreenRouter(ads::CDockManager* manager, QObject* parent = nullptr);

    /// Phase 4b: explicitly tear down PanelRegistry entries owned by this
    /// router. Without this, closing a WindowFrame would leave dangling
    /// PanelHandle entries pointing at a destroyed CDockManager — the
    /// registry's dock_widget pointers are QPointer-guarded but the type
    /// id / frame id metadata would still pollute find_by_frame() results.
    ~DockScreenRouter() override;

    /// Register a pre-built screen (eager — lightweight screens).
    void register_screen(const QString& id, QWidget* screen);

    /// Register a factory that creates the screen on first navigation (lazy).
    void register_factory(const QString& id, ScreenFactory factory);

    /// Navigate to a screen: ensures the dock widget exists, shows it, and
    /// brings it to the front. Creates from factory on first access.
    /// If exclusive is true, all other screens are closed first (single-screen mode).
    void navigate(const QString& id, bool exclusive = false);

    /// Add screen as an ADS tab inside the currently focused dock area.
    /// If the screen is already open somewhere, just raises it.
    /// If no area is open yet, opens it as the only screen.
    void tab_into(const QString& id);

    /// Open `primary` alone, then split `secondary` alongside it (side-by-side).
    void add_alongside(const QString& primary, const QString& secondary);

    /// Close all panels except `primary`, which fills the full area.
    void remove_screen(const QString& primary);

    /// Close current panel and open `secondary` in its place (full area).
    void replace_screen(const QString& primary, const QString& secondary);

    /// Returns the currently focused/active screen id.
    QString current_screen_id() const { return current_id_; }

    /// Pre-create CDockWidgets (with placeholder content) for ALL registered
    /// factories. Required before calling CDockManager::restoreState() since
    /// ADS needs all dock widgets to exist by objectName.
    void ensure_all_registered();

    /// ensure_all_registered() limited to the panels named in a saved ADS
    /// layout (what restoreState() looks up). Use before restoring `dock_state`.
    void ensure_registered_for_state(const QByteArray& dock_state);

    /// Find an existing dock widget by screen id, or nullptr.
    ads::CDockWidget* find_dock_widget(const QString& id) const;

    /// Return the materialized screen widget for a route id, or nullptr if the
    /// screen has not been created yet. This is the raw screen (not the ADS
    /// dock-widget wrapper), so callers can qobject_cast it to the concrete type
    /// to hand it a payload after navigate() (e.g. a notebook path to open).
    QWidget* screen_widget(const QString& id) const { return screens_.value(screen_alias::canonical(id)); }

    /// Run `fn` with the screen of `id` once it is fully materialised (built in
    /// place, shown, its saved state restored): at once when it already is and
    /// nothing is in flight for it; otherwise right after the build / navigate
    /// in flight finishes (queued, on this router's thread) — `fn` then gets
    /// nullptr if that build failed. Returns false, and drops `fn`, when the
    /// screen is neither built nor being built. Use this, not screen_widget(),
    /// to hand a screen a payload after navigate(): a navigate() that joined a
    /// build already in flight returns before the screen exists.
    bool when_materialised(const QString& id, std::function<void(QWidget*)> fn);

    /// True while a build or a navigate() of `id` is running.
    bool is_busy(const QString& id_in) const {
        const QString id = screen_alias::canonical(id_in);
        return building_.contains(id) || navigating_.contains(id);
    }

    /// Live screens of `id`'s type in this window (all its dock ids, every
    /// instance inside each content host). 1 for a single-instance type once it
    /// is built; 0 before.
    int instance_count(const QString& id) const;

    /// Self-heal: remove every second screen instance inside one content host,
    /// and every surplus panel of a single-instance type (an old layout's
    /// "equity_trading#dup2", or one restored next to its original), keeping the
    /// one this router records. Logs one warning per removal. Returns how many
    /// were removed. Runs after every build and after every layout restore.
    int collapse_duplicates(const QString& where);

    /// Returns all registered screen IDs (both eager and factory).
    QStringList all_screen_ids() const;

    /// Cycle focus between all currently-open dock widgets in this manager.
    /// Wraps at either end. No-op if fewer than 2 panels are open.
    /// `forward = true` moves to the next panel in open order; false moves back.
    void cycle_focused_panel(bool forward);

    /// Duplicate an open dock widget into a new tab. Only works for screens
    /// that were registered via register_factory() — eagerly-registered
    /// screens use a single widget instance and cannot be duplicated. The
    /// new tab gets a synthetic id `<base>#dup<N>` so it doesn't collide in
    /// QSettings. If the source screen implements IStatefulScreen, its
    /// save_state() is applied to the duplicate via restore_state() so the
    /// duplicate starts in the same view (e.g. same ticker loaded in
    /// EquityResearch, same watchlist selected in WatchlistScreen).
    /// Returns the new dock widget or nullptr on failure.
    ads::CDockWidget* duplicate_panel(const QString& id);

    /// Phase 6 final / decision 5.5: rearrange the currently-open panels
    /// into a 2x2 grid across the central area. Up to 4 panels:
    ///   panel 1 → top-left full
    ///   panel 2 → right-of-1
    ///   panel 3 → below-1
    ///   panel 4 → right-of-3 (= bottom-right)
    /// Panels 5+ tab into the bottom-right area.
    void tile_2x2();

    /// Drop every closed/hidden dock widget out of the dock manager so it is
    /// not serialised into the saved layout. ADS keeps a toggled-off widget in
    /// its own area in the tree (Closed="1"); CDockManager::saveState() writes
    /// all of them and restoreState() faithfully rebuilds them, so closed
    /// panels accumulate across sessions into dozens of zero-size split areas
    /// that corrupt the restored grid (a clean left|right comes back as a
    /// reshuffled top/bottom split). Call this immediately before saveState().
    ///
    /// The CDockWidget objects survive — the router owns them in dock_widgets_,
    /// so navigate()/tab_into()/add_alongside() re-add them on demand via their
    /// needs_add path (triggered by dockManager()==nullptr). Returns the number
    /// of panels pruned. No-op (returns 0) when nothing is hidden.
    ///
    /// Expensive: every detached panel is re-parented, which re-computes the
    /// stylesheet of its whole screen (hundreds of ms per screen) — and it is
    /// re-parented again when re-opened. For saving, use
    /// save_state_without_closed(), which leaves the live tree alone.
    int prune_hidden_panels();

    /// CDockManager::saveState() with every closed panel — and every area,
    /// splitter and floating container left empty by that — removed from the
    /// serialised layout, as if prune_hidden_panels() had run first, but
    /// without touching a single widget. This is what layout saves use. (Falls
    /// back to prune_hidden_panels() + saveState() if the state cannot be read.)
    QByteArray save_state_without_closed();

    /// Synchronously persist the UI state of EVERY materialized stateful screen
    /// (IStatefulScreen) right now, on the calling thread. Panels that are
    /// visible at app-quit never receive visibilityChanged(false), so their
    /// per-screen save never fires through the normal hide path; and an async
    /// save dispatched from closeEvent would race process exit. WindowFrame
    /// calls this at the top of closeEvent so each open tab's latest config
    /// (FNO underlying/expiry/broker, equity symbol/watchlist, active sub-tab,
    /// …) is on disk before shutdown. Returns the number of screens flushed.
    int flush_all_screen_states();

    /// Phase 5: tear off a panel into a brand-new WindowFrame. Spawns the
    /// new frame on the next available monitor (mirrors the
    /// new-window-on-next-monitor placement policy). Then performs a
    /// close-and-recreate move into the new frame's router.
    ///
    /// Returns true on success. Common failure: the panel id is unknown
    /// or has no attached factory (eagerly-registered single-instance
    /// screens can't be torn off — the original frame would lose its
    /// only widget for that screen and the screen has no factory to
    /// re-create on the target).
    ///
    /// Edge case: if this is the only panel in the source frame, the
    /// source frame is left empty rather than destroyed. The user can
    /// close it manually if they want.
    ///
    /// Public because Phase 9 command handlers (`panel.tear_off`) call this
    /// alongside the right-click menu path. Originally protected when only
    /// the menu invoked it.
    ///
    /// `error` (optional) receives a user-facing reason when the call fails;
    /// it stays empty when the user cancelled the monitor picker.
    bool tear_off_to_new_frame(const QString& id, QString* error = nullptr);

    /// Phase 5: move a panel from this router to another router. Used by
    /// the right-click "Move to window > Window N" context menu and by
    /// the panel.move_to_frame action (Phase 9 command handler).
    ///
    /// Mechanics: a close-and-recreate, NOT live cross-manager reparenting
    /// — the latter is feasible in ADS but introduces orphaned
    /// CFloatingDockContainer cleanup and gesture-interception complexity
    /// (see .grill-me/phase-5-complete.md for the feasibility report).
    /// Close-and-recreate is the proven path used by duplicate_panel.
    ///
    /// State is preserved: source flushes pending state, saves under the
    /// panel's instance UUID; the same UUID is transferred to the target
    /// router's instance_ids_ map; target navigate() materialises the
    /// screen and restore_screen_state reads the same UUID-keyed row.
    /// `panel_id`, `type_id`, and the `PanelInstanceId` are unchanged
    /// across the move — link group memberships and other shell-side
    /// metadata survive too.
    ///
    /// Returns true on success. No-op if `target` == this router, if
    /// `target` is null, or if the panel id is unknown.
    ///
    /// If the target already holds a CLOSED shell for `id` (pre-registered
    /// for layout restore, or opened and closed there earlier) the move
    /// reuses that shell and the target's own instance id; the panel state
    /// is written under the target's id. Refused only when the panel is
    /// currently open in the target. `error` (optional) receives a
    /// user-facing reason on failure.
    bool move_panel_to_frame(const QString& id, DockScreenRouter* target, QString* error = nullptr);

    /// Phase 5 helper: adopt a (string id, PanelInstanceId) pair from
    /// another router. Updates the existing PanelHandle's frame_id to
    /// point at this router's owning frame so PanelRegistry::find_by_frame
    /// returns correct results. Called by move_panel_to_frame on the
    /// target side; not generally useful elsewhere.
    void adopt_panel_instance(const QString& id, PanelInstanceId instance_id);

    /// Link group saved with a layout for a panel that is not materialised yet.
    /// Applied when the screen is built (before the default-group rule), so a
    /// restored layout keeps its colour links across restarts.
    void set_pending_link_group(const QString& id, SymbolGroup group);

    /// Phase 4b: PanelInstanceId for the panel registered under string id.
    /// Returns null UUID if the id isn't known. The router maintains a
    /// 1:1 mapping (one PanelInstanceId per string id, including the
    /// duplicate-panel synthetic ids).
    ///
    /// Public because Phase 6 layout capture (`WindowFrame::capture_layout`)
    /// needs to read the saved-side UUID for each open panel.
    PanelInstanceId panel_uuid_for(const QString& id) const;

    /// Phase 8 staged materialisation: pre-create the ADS dock widget shell
    /// for `id` without invoking the screen factory. Used by
    /// `WindowFrame::apply_layout` so `CDockManager::restoreState` can match
    /// dock widgets by objectName before each panel's heavy screen widget
    /// is constructed. The instance UUID is minted normally; callers that
    /// want to adopt a saved UUID should call `adopt_panel_instance` after
    /// this returns. Idempotent: a no-op if `id` is already registered.
    void prepare_dock_widget(const QString& id);

    /// Pre-create shells (prepare_dock_widget) for every duplicated panel
    /// ("<base>#dup<N>") named in an ADS saveState() blob. Duplicate ids have
    /// no factory of their own after a restart, so without this restoreState
    /// silently drops them. Call before CDockManager::restoreState().
    void prepare_duplicate_panels(const QByteArray& dock_state);

    /// Phase 8 staged materialisation: invoke the screen factory and
    /// install the constructed widget into the dock widget. Idempotent.
    /// Called by `PanelMaterialiser` during the staged drain triggered
    /// from `WindowFrame::apply_layout`. Restores per-panel state via
    /// the same path navigate() uses. A screen that has no dock widget yet
    /// (never opened in this window) gets a hidden shell first, so it can be
    /// built without being shown (the Research BUY / SELL order ticket).
    void materialize_now(const QString& id);

    /// Suppress/resume the visibilityChanged→materialize_screen autowire.
    /// Used by WindowFrame to prevent synchronous screen construction
    /// during CDockManager::restoreState() — the restore toggles
    /// dozens of widgets visible, and constructing each screen factory
    /// inline blocks the first paint for seconds. The caller sets true
    /// before restoreState, false after, then triggers deferred
    /// materialization via PanelMaterialiser.
    void set_suppress_materialize(bool suppress) { suppress_visibility_materialize_ = suppress; }

  signals:
    void screen_changed(const QString& id);

  protected:
    bool eventFilter(QObject* obj, QEvent* event) override;

  private:
    ads::CDockManager* manager_ = nullptr;
    QHash<QString, ads::CDockWidget*> dock_widgets_;
    QHash<QString, QWidget*> screens_;        // materialized screen widgets
    QHash<QString, ScreenFactory> factories_; // pending lazy factories
    QString current_id_;

    /// Phase 4b: per-string-id PanelInstanceId. Populated lazily on first
    /// `create_dock_widget(id)` call. Lifetime is the router's; cleaned up
    /// by `~DockScreenRouter` indirectly via PanelRegistry::unregister_panel.
    QHash<QString, PanelInstanceId> instance_ids_;

    /// When true, the visibilityChanged-→-materialize_screen autowire on
    /// every CDockWidget short-circuits. Set during bulk widget registration
    /// (ensure_all_registered / prepare_dock_widget) so adding a freshly-
    /// created dock widget — which transiently emits visibilityChanged(true)
    /// before toggleView(false) hides it — doesn't cascade into eagerly
    /// instantiating every screen factory. Without this guard a multi-window
    /// session's startup constructs all ~46 screens up front (DataHub
    /// subscriptions, OpenGL surfaces, etc.), defeating the lazy intent.
    bool suppress_visibility_materialize_ = false;

    // Phase 4 grid removal: the four `grid_*_` CDockAreaWidget pointers and
    // `panel_count_` int are gone. ADS handles arbitrary splits natively;
    // navigate() now tabs new panels into the focused dock area or creates
    // a fresh central area. `add_alongside()` is the explicit "I want a
    // split" API and does its own RightDockWidgetArea split. Users wanting
    // the pre-Phase-4 hard-coded right-of-TL → below-TL → right-of-BL
    // cycle should split panels manually via ADS drag-drop.

    // Phase 4 grid removal: previous `grid_top_left_/_top_right_/_bottom_left_/_bottom_right_`
    // and `panel_count_` members deleted. See note above near instance_ids_.

    // ── One instance per dock id ──────────────────────────────────────────────
    /// Ids whose factory / first show is running (materialize_screen).
    QSet<QString> building_;
    /// Ids whose navigate() is running.
    QSet<QString> navigating_;
    /// Work that arrived for an id while it was busy, run (queued) once it is
    /// not: when_materialised() callbacks and deferred navigate / tab_into calls.
    QHash<QString, QList<std::function<void(QWidget*)>>> after_busy_;
    /// Called when a build or a navigate of `id` ends; flushes after_busy_[id]
    /// once nothing is in flight for the id any more.
    void settle(const QString& id);
    /// An alias id (ScreenAliases.h) was opened as `canonical`: show its section
    /// once the screen is materialised (screens::ISectionLinked).
    void open_alias_section(const QString& alias, const QString& canonical);
    /// Queue `fn` until `id` is not busy. Returns false when it is not busy.
    bool defer_while_busy(const QString& id, std::function<void(QWidget*)> fn);
    /// Tear one panel down completely (dock widget, screen, ids, registry entry).
    void discard_panel(const QString& id);

    ads::CDockWidget* create_dock_widget(const QString& id);
    /// Shells for `ids`, tabbed hidden into one parking area (see ensure_all_registered).
    void register_hidden(const QStringList& ids);
    /// Build the screen of `id` (or return the one already built). nullptr when
    /// it cannot be built, or when a build of `id` is already running further up
    /// the stack — that build finishes on its own (see DockInstanceGuard.h).
    QWidget* materialize_screen(const QString& id);

    /// For a duplicate id ("<base>#dup<N>") with no factory of its own,
    /// install a copy of the base screen's factory. Returns true when `id`
    /// has a factory afterwards. Never touches ids without "#dup".
    bool ensure_dup_factory(const QString& id);

    /// Re-tile all open panels into the auto-grid the product wants when a new
    /// panel is added: 1=full, 2=left|right, 3=top-row + full-width bottom,
    /// 4=2x2 (3rd→bottom-left, 4th→bottom-right), 5+=tab into bottom-right.
    /// Existing panels keep their reading order (top→bottom, left→right); if
    /// `newest` is given it is placed into the last/next slot. Shared by the
    /// `layout.tile_2x2` command (newest == nullptr) and navigate()'s
    /// new-panel path.
    void retile_grid(ads::CDockWidget* newest = nullptr);

    // ── Symbol group linking ──────────────────────────────────────────────────
    // If `screen` (or one of its descendants) implements IGroupLinked, wraps
    // it in a thin container with a top-bar GroupBadge and wires the badge to
    // SymbolContext. Returns the container (or `screen` itself if no linking
    // is supported). Safe to call once per materialised screen.
    QWidget* wrap_with_group_badge(const QString& id, QWidget* screen);

    // Inserts a GroupBadge at index 0 of the CDockWidgetTab's layout (next to
    // the title label / close button). Idempotent — safe to call multiple
    // times for the same id. Called from create_dock_widget once the tab is
    // built, and from materialize_screen if the linked-pointer was discovered
    // after the tab existed.
    void attach_group_badge_to_tab(const QString& id, QWidget* screen);

    // The IGroupLinked* for each screen id, so SymbolContext signals can be
    // routed back. Held as raw pointer since lifetime matches the wrapped
    // QWidget in screens_, which we already manage.
    QHash<QString, IGroupLinked*> group_linked_;
    QHash<QString, SymbolGroup> pending_link_groups_; // from a restored layout
    QHash<QString, QPointer<ui::GroupBadge>> group_badges_;

    void on_group_symbol_changed_external(fincept::SymbolGroup g, const fincept::SymbolRef& ref, QObject* source);
    /// Apply theme colors directly to all CDockAreaTitleBar and CDockWidgetTab
    /// widgets. Called after any addDockWidget() call since those create new
    /// QFrame-derived widgets that the app-level QFrame rule overrides.
    void apply_ads_theme();

  public:
    /// Map a screen id to its human-readable title.
    static QString title_for_id(const QString& id);

  private:
    /// Persist and restore a user-edited tab title (separate from screen state).
    void save_tab_title(const QString& id, const QString& title);
    QString load_tab_title(const QString& id) const;

    /// Save/restore IStatefulScreen state for a materialized screen widget.
    void save_screen_state(const QString& id);
    void restore_screen_state(const QString& id);

    /// Phase 12 — build + show the right-click tab context menu at the given
    /// global position for the given dock widget id. Public so callers outside
    /// the router can trigger it (e.g. a "menu" key or toolbar action), but
    /// primarily invoked from the event filter on the tab widget.
  public:
    void show_tab_context_menu(const QString& id, const QPoint& global_pos);
};

} // namespace fincept
