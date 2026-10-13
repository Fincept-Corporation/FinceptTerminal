#pragma once
#include "ui/theme/ThemeTokens.h"

#include <QDialog>
#include <QFrame>
#include <QHBoxLayout>
#include <QHideEvent>
#include <QJsonObject>
#include <QLabel>
#include <QPushButton>
#include <QShowEvent>
#include <QTimer>
#include <QVBoxLayout>

#include <functional>

class QTableWidget;

namespace fincept::screens::widgets {

class LoadingOverlay;

/// Reusable widget container — title bar with accent color, close/refresh/config buttons.
///
/// Configurable widgets override `make_config_dialog(parent)` to return a
/// modal dialog; BaseWidget then shows a gear icon in the title bar that
/// opens it. On `accept()`, the subclass should call `apply_config()` and
/// emit `config_changed()`, which DashboardCanvas persists back to the
/// layout repository. Widgets without config return nullptr (default) —
/// the gear icon stays hidden.
class BaseWidget : public QFrame {
    Q_OBJECT
  public:
    explicit BaseWidget(const QString& title, QWidget* parent = nullptr, const QString& accent_color = {});

    QVBoxLayout* content_layout() const { return content_layout_; }
    QWidget* drag_handle() const { return title_bar_; }

    void set_loading(bool loading);

    /// Determinate-progress variant of `set_loading`. Shows an animated
    /// "LOADING X / Y ITEMS" overlay over the content area; widget
    /// subclasses call this from their data-arrival slot with the running
    /// (loaded, expected) counts. When `loaded >= expected` the overlay
    /// fades out automatically. If `expected <= 0`, falls back to the
    /// indeterminate "LOADING…" animation. The title-bar text indicator is
    /// also toggled in lockstep.
    void set_loading_progress(int loaded, int expected);

    /// Show a terminal error over the content area ("what failed / what to
    /// do"). Non-destructive: the content widgets stay alive underneath and
    /// the next set_loading()/set_loading_progress() — e.g. from the title-bar
    /// refresh — clears it. (It used to delete every content widget, leaving
    /// subclasses holding dangling pointers.)
    void set_error(const QString& error);
    /// True while the loading overlay is up with nothing delivered yet (and no
    /// error shown) — i.e. a producer error now would leave a blank tile.
    bool is_waiting_for_first_data() const;
    /// set_error() is currently on the overlay.
    bool is_showing_error() const { return error_shown_; }
    void set_title(const QString& title);

    /// External-trigger entry point — emits `refresh_requested` so the
    /// widget runs the same refresh path as a user clicking the title-bar
    /// refresh button. Used by the dashboard "REFRESH ALL" toolbar button.
    void request_refresh() { emit refresh_requested(); }

    /// Current per-instance config. Default: empty — subclasses override to
    /// return their live state (symbol, broker id, filters, etc.).
    virtual QJsonObject config() const { return {}; }

    /// Apply a persisted config. Called by the factory for fresh tiles and
    /// by the canvas when the user edits a config. Default: no-op.
    virtual void apply_config(const QJsonObject& /*cfg*/) {}

  signals:
    void close_requested();
    void refresh_requested();
    /// Emitted after the user edits config; canvas listens and persists.
    void config_changed(QJsonObject new_config);

  protected:
    /// Override in subclasses to re-apply styles when theme/font changes.
    /// Called automatically — no manual connection needed.
    virtual void on_theme_changed() {}

    /// Override to return a modal config dialog (takes ownership: parent =
    /// `parent`). Return nullptr (default) to disable configuration and
    /// hide the gear icon.
    virtual QDialog* make_config_dialog(QWidget* /*parent*/) { return nullptr; }

    /// Subclasses call this from their ctor once they support configuration
    /// so the gear icon appears. Also hides the icon if set to false later.
    void set_configurable(bool configurable);

    /// Coalesce repeated re-render requests into a single call on the next
    /// event-loop turn.
    ///
    /// Hub deliveries arrive one topic at a time: a widget subscribed to N
    /// symbols gets N callbacks per refresh cycle, and a naive
    /// `rebuild_from_cache()` inside each callback rebuilds the whole table N
    /// times (O(N^2) widget churn + N full CSS reparses per row). Routing the
    /// rebuild through this helper collapses a whole delivery burst into one
    /// render. The last functor wins, so passing a lambda that reads current
    /// state is always correct.
    ///
    /// The timer is bound to `this` as its context object, so a pending render
    /// is dropped automatically if the widget is destroyed first.
    void schedule_render(std::function<void()> fn);

    /// Re-arms the loading watchdog on becoming visible. Many subclasses call
    /// `set_loading(true)` from their constructor, which arms the watchdog
    /// before the widget is mounted and before any subscription has been
    /// dispatched. Without this override the 20 s budget would burn while
    /// the widget is still off-screen.
    void showEvent(QShowEvent* event) override;

    /// Hub-subscription teardown guarantee. Every tile subscribes in
    /// showEvent() with itself as the owner; this drops ALL of them
    /// (`DataHub::unsubscribe(this)`) whenever the tile is hidden, so a
    /// subclass that forgets its own hideEvent() unsubscribe — or subscribed
    /// from a config change — can never keep a producer polling for a tile
    /// nobody can see (P3 / D3). Destruction is covered by the hub itself
    /// (owner `destroyed()`). Subclasses chain to this first, then reset
    /// their own `hub_active_`-style flags.
    void hideEvent(QHideEvent* event) override;
    void changeEvent(QEvent* event) override;

    /// setStyleSheet() only when `css` differs from what this helper last
    /// applied to `w`. For data-driven colours set on every hub delivery:
    /// a stylesheet is a full CSS re-parse, and the colour bucket rarely
    /// changes between two ticks.
    static void set_css_cached(QWidget* w, const QString& css);
    /// Forget what set_css_cached() applied to `w` (call after restyling `w`
    /// directly, e.g. on a theme change), so the next call re-applies.
    static void reset_css_cache(QWidget* w);

    /// Right-click menu for a symbol: open in Equity Research / Equity (or
    /// Crypto) Trading / News, backtest it, add it to a saved watchlist, or
    /// copy it. Navigation goes through `nav.open_symbol`.
    void show_symbol_menu(const QString& symbol, const QPoint& global_pos,
                          const QString& asset_class = QStringLiteral("equity"), const QString& exchange = {});

    /// Give `table` the symbol right-click menu. The symbol is read from
    /// column `col` of the clicked row, from `role` (DisplayRole by default;
    /// pass Qt::UserRole when the cell shows a display label instead).
    void install_symbol_menu(QTableWidget* table, int col = 0, int role = Qt::DisplayRole);

    /// Re-apply tr() to the title bar buttons and loading text. Subclasses
    /// SHOULD override and call BaseWidget::retranslateUi() first, then
    /// retranslate their own widgets.
    virtual void retranslateUi();

    /// Open `symbol` in another screen through the `nav.open_symbol` event
    /// (navigates, constructing the screen if needed, then hands it the
    /// symbol via IGroupLinked). Default target is Equity Research; pass
    /// "crypto_trading" + asset_class "crypto" for crypto pairs.
    void open_symbol(const QString& symbol, const QString& screen_id = QStringLiteral("equity_research"),
                     const QString& asset_class = QStringLiteral("equity"), const QString& exchange = {});

    /// Make `w` open `symbol` in Equity Research when double-clicked, and show
    /// the symbol menu (show_symbol_menu) on right-click. Use for rows/cells
    /// that are plain QWidgets (table widgets should connect to
    /// cellDoubleClicked + install_symbol_menu instead). Safe to call
    /// repeatedly on the same widget (e.g. when a recycled row is refilled) —
    /// it only refreshes the symbol.
    void link_symbol(QWidget* w, const QString& symbol);

    /// Make `w` switch to the full `screen_id` screen (nav.switch_screen) when
    /// double-clicked. For rows that describe an event/dataset rather than a
    /// ticker (economic calendar, geopolitics, vessels, prediction markets).
    void link_screen(QWidget* w, const QString& screen_id, const QString& tooltip);

    /// Handles the double-clicks installed by link_symbol() / link_screen(),
    /// and the right-click symbol menu for link_symbol() widgets.
    /// Subclasses that override this must chain to BaseWidget::eventFilter().
    bool eventFilter(QObject* obj, QEvent* event) override;

  private:
    void refresh_base_theme();
    void on_config_clicked();
    void arm_watchdog();
    void disarm_watchdog();
    void on_watchdog_fired();

  private:
    QWidget* title_bar_ = nullptr;
    QLabel* title_label_ = nullptr;
    QLabel* accent_bar_ = nullptr;
    QLabel* loading_label_ = nullptr;
    QWidget* content_ = nullptr;
    QVBoxLayout* content_layout_ = nullptr;
    QPushButton* refresh_btn_ = nullptr;
    QPushButton* config_btn_ = nullptr;
    QPushButton* close_btn_ = nullptr;
    LoadingOverlay* loading_overlay_ = nullptr;
    QTimer* loading_watchdog_ = nullptr;
    int last_progress_loaded_ = 0;
    bool error_shown_ = false; ///< set_error() is on the overlay (cleared by the next set_loading*)
    QString accent_color_;

    // Render coalescing — see schedule_render().
    std::function<void()> pending_render_;
    bool render_scheduled_ = false;
};

} // namespace fincept::screens::widgets
