#pragma once
#include <DockManager.h>
#include <DockWidget.h>

#include <QHash>
#include <QObject>
#include <functional>

namespace fincept {

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

  signals:
    void screen_changed(const QString& id);

  protected:
    bool eventFilter(QObject* obj, QEvent* event) override;


  private:
    ads::CDockManager* manager_ = nullptr;
    QHash<QString, ads::CDockWidget*> dock_widgets_;
    QHash<QString, QWidget*> screens_;           // materialized screen widgets
    QHash<QString, ScreenFactory> factories_;    // pending lazy factories
    QString current_id_;

    // Grid layout tracking — stores the dock area for each grid position.
    // Populated as panels are opened (up to 4), used to place the next panel
    // correctly (2x2 grid default). Cleared when exclusive navigation resets.
    ads::CDockAreaWidget* grid_top_left_     = nullptr;
    ads::CDockAreaWidget* grid_top_right_    = nullptr;
    ads::CDockAreaWidget* grid_bottom_left_  = nullptr;
    ads::CDockAreaWidget* grid_bottom_right_ = nullptr;
    int panel_count_ = 0; // number of currently open panels

    ads::CDockWidget* create_dock_widget(const QString& id);
    void materialize_screen(const QString& id);
    static QString title_for_id(const QString& id);

    /// Persist and restore a user-edited tab title (separate from screen state).
    void save_tab_title(const QString& id, const QString& title);
    QString load_tab_title(const QString& id) const;

    /// Save/restore IStatefulScreen state for a materialized screen widget.
    void save_screen_state(const QString& id);
    void restore_screen_state(const QString& id);
};

} // namespace fincept
