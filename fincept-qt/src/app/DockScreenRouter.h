#pragma once
#include "core/identity/Uuid.h"
#include "core/symbol/SymbolGroup.h"

#include <QHash>
#include <QObject>
#include <QPointer>

#include <DockManager.h>
#include <DockWidget.h>
#include <functional>

namespace fincept::ui {
class GroupBadge;
}

namespace fincept {

class IGroupLinked;
struct SymbolRef;

/// ADS-aware screen router. Each screen is wrapped in a CDockWidget that can be
/// docked, tabbed, floated, or auto-hidden. Mirrors ScreenRouter's API so the
/// transition is straightforward.
///
/// Lazy loading: factories are stored and only called when a screen is first
/// navigated to (or when ensure_all_registered() pre-creates placeholders for
/// state restore).
class DockScreenRouter : public QObject {
    Q_OBJECT
  public:
    using ScreenFactory = std::function<QWidget*()>;

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

    /// Find an existing dock widget by screen id, or nullptr.
    ads::CDockWidget* find_dock_widget(const QString& id) const;

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
    bool tear_off_to_new_frame(const QString& id);

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
    bool move_panel_to_frame(const QString& id, DockScreenRouter* target);

    /// Phase 5 helper: adopt a (string id, PanelInstanceId) pair from
    /// another router. Updates the existing PanelHandle's frame_id to
    /// point at this router's owning frame so PanelRegistry::find_by_frame
    /// returns correct results. Called by move_panel_to_frame on the
    /// target side; not generally useful elsewhere.
    void adopt_panel_instance(const QString& id, PanelInstanceId instance_id);

    /// Phase 4b: PanelInstanceId for the panel registered under string id.
    /// Returns null UUID if the id isn't known. The router maintains a
    /// 1:1 mapping (one PanelInstanceId per string id, including the
    /// duplicate-panel synthetic ids).
    ///
    /// Public because Phase 6 layout capture (`WindowFrame::capture_layout`)
    /// needs to read the saved-side UUID for each open panel.
    PanelInstanceId panel_uuid_for(const QString& id) const;

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

    // Phase 4 grid removal: the four `grid_*_` CDockAreaWidget pointers and
    // `panel_count_` int are gone. ADS handles arbitrary splits natively;
    // navigate() now tabs new panels into the focused dock area or creates
    // a fresh central area. `add_alongside()` is the explicit "I want a
    // split" API and does its own RightDockWidgetArea split. Users wanting
    // the pre-Phase-4 hard-coded right-of-TL → below-TL → right-of-BL
    // cycle should split panels manually via ADS drag-drop.

    // Phase 4 grid removal: previous `grid_top_left_/_top_right_/_bottom_left_/_bottom_right_`
    // and `panel_count_` members deleted. See note above near instance_ids_.

    ads::CDockWidget* create_dock_widget(const QString& id);
    void materialize_screen(const QString& id);

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
    QHash<QString, QPointer<ui::GroupBadge>> group_badges_;

    void on_group_symbol_changed_external(fincept::SymbolGroup g, const fincept::SymbolRef& ref,
                                          QObject* source);
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
