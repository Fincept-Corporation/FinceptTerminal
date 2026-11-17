#pragma once
#include "core/symbol/SymbolGroup.h"
#include "core/symbol/SymbolRef.h"
#include "ui/theme/ThemeTokens.h"
#include "ui/widgets/EmptyState.h"

#include <QDialog>
#include <QFrame>
#include <QHBoxLayout>
#include <QHash>
#include <QHideEvent>
#include <QJsonObject>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QSet>
#include <QShowEvent>
#include <QStringList>
#include <QTimer>
#include <QVBoxLayout>

#include <functional>

class QTableWidget;

namespace fincept::ui {
class GroupBadge;
}

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
    ///
    /// Once something has been delivered (`loaded > 0`) and the overlay is
    /// down — e.g. the watchdog dismissed it because one item never arrived —
    /// a partial count no longer re-raises it: every later delivery used to put
    /// the near-opaque overlay back over good rows for another 20 s. A repeat
    /// of the same count also no longer restarts the watchdog.
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
    /// The designed empty state (show_empty_state()) covers the content.
    bool is_empty_state_shown() const;
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

    // ── Symbol link group (SymbolContext) ────────────────────────────────────
    // A linked tile shares its symbol with every panel in the same group —
    // dashboard tiles and IGroupLinked screens alike. Publishers (rows/cells
    // registered with link_symbol() / install_symbol_menu()) push the clicked
    // symbol; followers (enable_symbol_link(true)) switch to the group's symbol.
    // The group is stored in the tile's persisted config under kLinkGroupKey,
    // which DashboardCanvas merges into every config it saves.

    static constexpr const char* kLinkGroupKey = "link_group";

    SymbolGroup link_group() const { return link_group_; }
    bool is_symbol_link_enabled() const { return link_enabled_; }
    /// Change the group (updates the badge, emits link_group_changed). Ignored
    /// unless linking is enabled.
    void set_link_group(SymbolGroup g);
    /// Restore the group from a persisted config. A config without the key
    /// gets the widget's default group. Safe to call before linking is enabled.
    void restore_link_group(const QJsonObject& cfg);
    /// Write the group into `cfg` (no-op for tiles that never linked).
    void store_link_group(QJsonObject& cfg) const;

  signals:
    void close_requested();
    void refresh_requested();
    /// Emitted after the user edits config; canvas listens and persists.
    void config_changed(QJsonObject new_config);
    /// The user (or set_link_group) moved the tile to another link group.
    void link_group_changed(fincept::SymbolGroup group);

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

    /// Push-only tiles with nothing to re-request call set_refreshable(false):
    /// the title-bar refresh button is hidden instead of sitting there dead.
    void set_refreshable(bool refreshable);

    /// Agent-approval marker for a tile whose load commits something — billed
    /// tiles (Fincept credits) call set_agent_risk("spend") from their ctor. The
    /// "fincept_agent_risk" property of that kind goes on the tile's content area with
    /// "fincept_agent_risk_inherit" = true (covering every unmarked control inside
    /// it — not the title bar's close / configure buttons), on the title-bar refresh
    /// button and on the empty state's primary
    /// button ("LOAD"), also when the
    /// empty state is first built later (EmptyState::set_primary_agent_risk_default,
    /// so a Spec naming its own kind still wins). nullptr / "" removes it.
    void set_agent_risk(const char* kind);

    /// Open the tile's config dialog — what the gear does. For an empty
    /// state's "SET …" action. No-op for a tile without a config dialog.
    void open_config();

    // ── Designed empty state over the content ────────────────────────────────
    // A tile with nothing to draw for a known reason (no broker account, an
    // empty list, nothing configured yet, a default that could not be fetched)
    // covers its content area with a compact ui::EmptyState: what the tile is,
    // why it is empty and the one action that fixes it. The cover is built on
    // first use (never for tiles that do not need it), paints the tile surface
    // from the live theme and follows the content area's size; the tile's own
    // widgets stay alive underneath. Showing it drops the loading spinner and
    // its watchdog; a later set_loading(true) (a retry in flight) draws the
    // spinner on top of it. Call hide_empty_state() as soon as there is
    // something to show. Repeated calls re-apply the spec (retranslation).

    /// `on_primary` / `on_secondary` run on the EmptyState's buttons (the spec's
    /// primary / secondary texts); they replace the previous call's handlers.
    void show_empty_state(const ui::EmptyState::Spec& spec, std::function<void()> on_primary = {},
                          std::function<void()> on_secondary = {});
    void hide_empty_state();
    /// Broker tiles without an account: "No broker account connected", the
    /// sentence `what_it_shows` (what the tile would show), and OPEN EQUITY
    /// TRADING. Never connects anything by itself. While it is up, an account
    /// becoming active (added / activated in Equity Trading) runs
    /// `on_account_available` once, so the tile can resolve the account again
    /// (and hide the state). hide_empty_state() also cancels that wait.
    void show_no_broker_account(const QString& what_it_shows, std::function<void()> on_account_available);

    // ── Non-blocking message boxes ───────────────────────────────────────────
    // The QMessageBox statics run exec(): a nested event loop around a STACK
    // box parented to this tile. A tile destroyed while that box is open (layout
    // reload, MCP remove, another window) deletes the stack object through its
    // parent, and the code after exec() then runs on a dead `this`. These open a
    // heap box (window-modal, deleted on close) and return immediately.

    /// Yes / No question, default No. `on_yes` runs only when the user clicks
    /// Yes while this tile is still alive (this tile is the connection context).
    void confirm_async(QMessageBox::Icon icon, const QString& title, const QString& text,
                       std::function<void()> on_yes);
    /// Informational box with a single OK button.
    void notify_async(QMessageBox::Icon icon, const QString& title, const QString& text);

    /// Broker tiles: header text for `account_id` — its display name, plus
    /// "TOKEN EXPIRED / CONNECTION ERROR — reconnect in Equity Trading" while a
    /// live account's session is unusable. In that case the reason is also
    /// recorded as an error of `topic` (note_topic_error; ignored unless `topic`
    /// is tracked), so a tile with no data shows why instead of "No data yet".
    QString broker_account_hint(const QString& account_id, const QString& topic);

    // ── Producer errors (DataHub error channel) ──────────────────────────────
    // Producers report a failed fetch with DataHub::publish_error() and a
    // user-facing reason ("Python is not set up yet…", "Yahoo is rate
    // limiting…"). A tile that only subscribes to values never hears it and
    // spins until the 20 s watchdog's generic "No data yet". A tile declares
    // the topics its content comes from with track_topics(); BaseWidget then
    // listens to their error channel and:
    //   * while the tile has no data and EVERY tracked topic has failed, puts
    //     the reason on the overlay (with "click refresh to retry" — the
    //     title-bar refresh button), replacing the spinner;
    //   * once the tile has data, never covers it: a failure only shows the
    //     title-bar "STALE" / "n/N FAILED" marker, with the reasons in its
    //     tooltip;
    //   * on watchdog expiry with no data, shows the recorded reasons instead
    //     of the generic "No data yet";
    //   * forgets a topic's error when a value arrives (note_topic_value()).
    // Errors are kept until a value arrives, also across hide/show.

    /// Declare the hub topics this tile's content comes from and subscribe their
    /// error channel (owner = this, so hideEvent()'s DataHub::unsubscribe(this)
    /// and destruction drop it together with the value subscriptions). Replaces
    /// the previous set: topics no longer listed are unsubscribed and forgotten,
    /// topics still listed keep their recorded error / has-data state. Safe to
    /// call repeatedly — it removes this tile's error slot for every listed
    /// topic before adding it, so re-subscribing never stacks duplicates. Call
    /// it wherever the tile (re)subscribes its values, before subscribing them.
    ///
    /// `drive_loading_overlay`: BaseWidget also owns the loading overlay — a
    /// topic counts as resolved once it delivered a value OR an error; the
    /// overlay ("LOADING r / N") covers the content only while no topic has
    /// delivered data, and goes when the first value lands. Multi-symbol tiles
    /// pass true and stop calling set_loading_progress() themselves. With false
    /// the tile keeps driving set_loading() itself (single-topic tiles).
    void track_topics(const QStringList& topics, bool drive_loading_overlay = false);
    /// Drop every tracked topic and its recorded state. Call when the tile
    /// discards the data it was showing (symbol list replaced and rows rebuilt).
    void untrack_topics();
    /// A value for `topic` was rendered: clears its error and marks the tile as
    /// having data. Call from every value slot of a tracked topic.
    void note_topic_value(const QString& topic);
    /// Record a failure that did not come through the hub — a delivered but
    /// unusable payload (empty series). Handled exactly like a hub error.
    void note_topic_error(const QString& topic, const QString& error);
    /// Error recorded for `topic` since its last value; empty if none.
    QString topic_error(const QString& topic) const { return topic_errors_.value(topic); }
    /// Tooltip text for a row / cell fed by `topic`: the reason, prefixed by
    /// "No data" or "Last refresh failed — showing the last data received".
    /// Empty when the topic has no error.
    QString topic_error_tooltip(const QString& topic) const;
    /// Tracked topics currently in error.
    int failed_topic_count() const;
    /// Some tracked topic has delivered a value.
    bool has_topic_data() const { return !valued_topics_.isEmpty(); }
    /// `topic` has delivered a value (since it was first tracked).
    bool topic_has_data(const QString& topic) const { return valued_topics_.contains(topic); }
    /// Placeholder text for a cell whose topic failed with no data ("—").
    static QString failed_value_text() { return QString(QChar(0x2014)); }
    /// Hook: a tracked topic got a new (or different) error. Multi-symbol tiles
    /// re-render here to mark the failed rows; the overlay / title marker are
    /// already handled. Default: nothing.
    virtual void on_topic_errors_changed() {}

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

    /// Row height for an item view whose text is `font_offset` px from the
    /// user's base size: `design_px` at the default size, taller when a larger
    /// base font needs it. Re-apply it from on_theme_changed().
    static int text_row_height(int design_px, int font_offset);

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

    /// Opt into symbol linking and show the group badge in the title bar.
    /// `follows`: the tile switches to the group's symbol (override
    /// on_linked_symbol / linked_symbol). `joins_default_group`: a tile with no
    /// saved group starts in group A; pass false for tiles that must stay
    /// unlinked until the user links them (order entry). link_symbol() and
    /// install_symbol_menu() call this with (false, true) on their own.
    void enable_symbol_link(bool follows, bool joins_default_group = true);
    /// Follower hook: the group's symbol changed. Never called for this tile's
    /// own publishes.
    virtual void on_linked_symbol(const SymbolRef& /*ref*/) {}
    /// Follower hook: this tile's symbol, used to seed a group that has none
    /// yet when the user links the tile.
    virtual SymbolRef linked_symbol() const { return {}; }
    /// Push `symbol` to this tile's link group (no-op when unlinked).
    void publish_symbol(const QString& symbol, const QString& exchange = {});
    /// Publish the symbol in column `col` (`role`) of a clicked row.
    void install_symbol_publish(QTableWidget* table, int col = 0, int role = Qt::DisplayRole);
    /// Index levels, FX/futures and Yahoo crypto pairs are not broker-tradable equities.
    static bool is_tradable_equity(const QString& symbol);

  private:
    void refresh_base_theme();
    void on_config_clicked();
    void arm_watchdog();
    void disarm_watchdog();
    void on_watchdog_fired();
    void on_badge_group_requested(SymbolGroup g);
    SymbolGroup default_link_group() const;
    /// show_no_broker_account(): an account was added / changed — run the wait
    /// once an active account exists.
    void on_broker_accounts_changed();

    // Producer-error tracking — see track_topics().
    void on_tracked_topic_error(const QString& topic, const QString& error);
    /// Re-derive overlay + title marker from the tracked-topic state.
    void refresh_topic_state();
    /// drive_loading_overlay mode: overlay from (resolved, expected).
    void update_topic_loading();
    /// Put the recorded reasons on the overlay (no data yet).
    void show_topic_failure();
    void update_topic_status_label();
    /// Recorded errors grouped by reason, one line per reason ("AAPL, MSFT: …"),
    /// at most `max_groups` lines. A single tracked topic gives the bare reason.
    QString topic_error_lines(int max_groups) const;

  private:
    QWidget* title_bar_ = nullptr;
    QLabel* title_label_ = nullptr;
    QLabel* accent_bar_ = nullptr;
    QLabel* loading_label_ = nullptr;
    QLabel* topic_status_label_ = nullptr; ///< "STALE" / "n/N FAILED" marker (tracked topics)
    QWidget* content_ = nullptr;
    QVBoxLayout* content_layout_ = nullptr;
    QPushButton* refresh_btn_ = nullptr;
    QPushButton* config_btn_ = nullptr;
    QPushButton* close_btn_ = nullptr;
    LoadingOverlay* loading_overlay_ = nullptr;
    QTimer* loading_watchdog_ = nullptr;
    int last_progress_loaded_ = 0;
    int last_progress_expected_ = 0;
    bool error_shown_ = false; ///< set_error() is on the overlay (cleared by the next set_loading*)
    QString accent_color_;
    /// The theme token the accent was given as (null for a non-token colour):
    /// the accent is re-read from it on every theme change.
    const char* ui::ThemeTokens::* accent_field_ = nullptr;
    QString accent_source_; ///< the accent as passed, for a non-token colour

    // Producer-error tracking — see track_topics().
    QStringList tracked_topics_;            ///< declared order (used for messages)
    QSet<QString> tracked_set_;             ///< same, for lookups
    QHash<QString, QString> topic_errors_;  ///< topic -> reason, until its next value
    QSet<QString> valued_topics_;           ///< topics that delivered a value
    bool topic_drive_loading_ = false;      ///< track_topics(…, true)
    bool topic_error_shown_ = false;        ///< the overlay shows show_topic_failure()
    bool topic_tracking_quiet_ = false;     ///< inside track_topics(): defer refresh_topic_state()
    bool topic_retry_pending_ = false;      ///< refresh clicked on a failure: spin until a new outcome
    int topic_failure_resolved_ = -1;       ///< (resolved, expected) when the failure went up —
    int topic_failure_expected_ = -1;       ///< an unchanged count keeps it up instead of re-spinning

    // Render coalescing — see schedule_render().
    std::function<void()> pending_render_;
    bool render_scheduled_ = false;

    // Designed empty state — see show_empty_state(). Null until first needed.
    QWidget* empty_cover_ = nullptr;          ///< opaque host over content_
    ui::EmptyState* empty_state_ = nullptr;   ///< inside empty_cover_
    std::function<void()> empty_primary_;     ///< current primary action
    std::function<void()> empty_secondary_;   ///< current secondary action
    std::function<void()> broker_wait_;       ///< show_no_broker_account(): run once an account is active
    bool broker_watch_connected_ = false;     ///< AccountManager signals connected (once)
    QString agent_risk_;                      ///< set_agent_risk(): refresh + empty-state primary

    // Symbol linking — see enable_symbol_link().
    ui::GroupBadge* group_badge_ = nullptr;
    SymbolGroup link_group_ = SymbolGroup::None;
    bool link_enabled_ = false;
    bool link_follows_ = false;
    bool link_joins_default_ = true;
    bool link_has_saved_ = false; ///< the restored config carried a group
    SymbolGroup link_saved_ = SymbolGroup::None;
};

} // namespace fincept::screens::widgets
