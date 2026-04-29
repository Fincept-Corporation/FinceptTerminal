#pragma once
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

    // Grid layout tracking — stores the dock area for each grid position.
    // Populated as panels are opened (up to 4), used to place the next panel
    // correctly (2x2 grid default). Cleared when exclusive navigation resets.
    ads::CDockAreaWidget* grid_top_left_ = nullptr;
    ads::CDockAreaWidget* grid_top_right_ = nullptr;
    ads::CDockAreaWidget* grid_bottom_left_ = nullptr;
    ads::CDockAreaWidget* grid_bottom_right_ = nullptr;
    int panel_count_ = 0; // number of currently open panels

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
