#pragma once
// WatchlistScreen — multiple watchlists with live quotes and analytics.
//
// Layout: list sidebar | table / market map / correlation view | detail pane.
//
// Implementation is split by concern:
//   WatchlistScreen.cpp          UI construction, theme, lifecycle, state, symbol link
//   WatchlistScreen_Data.cpp     lists / entries, DataHub subscriptions, layout, footer, views
//   WatchlistScreen_Actions.cpp  add / remove / import / export, list management, menus, reorder
//
// The table is WatchlistView over WatchlistModel (+ WatchlistProxyModel);
// columns come from the WatchlistColumns registry and the per-list layout from
// WatchlistColumnPrefs. Alerts are WatchlistAlertService's.

#include "core/events/EventBus.h"
#include "core/symbol/IGroupLinked.h"
#include "screens/common/ISectionLinked.h"
#include "screens/common/IStatefulScreen.h"
#include "screens/watchlist/WatchlistColumnPrefs.h"
#include "screens/watchlist/WatchlistModel.h"
#include "services/markets/MarketDataService.h"
#include "storage/repositories/WatchlistRepository.h"

#include <QHash>
#include <QHideEvent>
#include <QList>
#include <QPointer>
#include <QSet>
#include <QShowEvent>
#include <QStringList>
#include <QVector>
#include <QWidget>

#include <memory>

class QComboBox;
class QCompleter;
class QHBoxLayout;
class QLabel;
class QLineEdit;
class QListWidget;
class QPushButton;
class QSplitter;
class QStackedWidget;
class QStringListModel;
class QTimer;
class QToolButton;

namespace fincept::ui {
class EmptyState;
}

namespace fincept::screens {

class WatchlistView;
class WatchlistFooter;
class WatchlistDetailPane;
class WatchlistMapView;
class WatchlistCorrelationView;
struct WatchlistImportEntry;

/// One ADD / import / drop whose symbols are being looked up (WatchlistScreen_Actions.cpp).
struct WatchlistAddBatch;

class WatchlistScreen : public QWidget, public IStatefulScreen, public IGroupLinked, public ISectionLinked {
    Q_OBJECT
    Q_INTERFACES(fincept::IGroupLinked)
  public:
    explicit WatchlistScreen(QWidget* parent = nullptr);
    ~WatchlistScreen() override;

    /// ISectionLinked — "list:<id>" (selects that list as a click does) | "table" | "map" |
    /// "correlation" (the view buttons) | "new" (the + dialog; nothing is created without its OK) |
    /// "alerts" (the Alerts… dialog of the focus symbol, else the current row; with an empty list the
    /// ADD box takes the focus). focus = a listed symbol: that row is selected.
    bool open_section(const QString& section, const QString& focus) override;

    void restore_state(const QVariantMap& state) override;
    QVariantMap save_state() const override;
    QString state_key() const override { return "watchlist"; }
    int state_version() const override { return 1; }

    // IGroupLinked
    void set_group(SymbolGroup g) override;
    SymbolGroup group() const override { return link_group_; }
    void on_group_symbol_changed(const SymbolRef& ref) override;
    SymbolRef current_symbol() const override;

  protected:
    void showEvent(QShowEvent* event) override;
    void hideEvent(QHideEvent* event) override;
    void changeEvent(QEvent* event) override;

  private slots:
    void on_watchlist_selected(int row);
    void on_add_watchlist();
    void on_delete_watchlist();
    void on_add_stock();
    void on_remove_stock();
    /// Also what F5 / View > Refresh invokes (WindowFrame::refresh_focused_panel).
    void on_refresh();
    void on_export_csv();
    void on_import_csv();
    void refresh_theme();

  private:
    enum class FeedbackTone { Info, Success, Warning, Error };

    // ── UI (WatchlistScreen.cpp) ────────────────────────────────────────────
    void build_ui();
    QWidget* build_sidebar();
    QWidget* build_main_panel();
    QWidget* build_top_bar();
    QWidget* build_add_bar();
    QWidget* build_feedback_bar();
    void install_shortcuts();
    void retranslateUi();
    /// Table, or the ui::EmptyState for no list / an empty list / a filter matching nothing.
    void update_empty_state();
    /// The detail pane is hidden while the list is empty (or none is selected) —
    /// the list's own empty state says what to do; it returns with the first row.
    void sync_detail_visibility();
    /// EmptyState primary on an empty list: the region's large caps (MarketProfile),
    /// added through the ordinary add path. Only ever the user's click.
    void add_region_large_caps();
    void update_list_actions();
    /// Non-modal message under the toolbars. `ms` = 0 keeps it until replaced / closed.
    void show_feedback(const QString& text, FeedbackTone tone, int ms = 7000, const QString& details = {});
    void clear_feedback();
    /// "table" | "map" | "correlation".
    void set_view_mode(const QString& view, bool user = true);
    void set_detail_visible(bool on, bool user = true);
    void style_view_buttons();

    // ── Data (WatchlistScreen_Data.cpp) ─────────────────────────────────────
    void load_watchlists();
    void rebuild_sidebar();
    void load_stocks();
    void apply_layout_for_current_list();
    void layout_edited();
    void save_layout_now();
    /// Subscribe the current list's quote topics. `force` (the user's refresh, a changed
    /// symbol set) also refetches them now; a plain re-show only subscribes, so cached
    /// quotes are handed over and only stale ones are fetched.
    void hub_resubscribe(bool force);
    void hub_unsubscribe_all();
    /// Subscribe info / sparkline / history topics the visible columns and views need.
    void update_data_needs();
    void ensure_info(const QStringList& symbols);
    void ensure_spark(const QStringList& symbols);
    void ensure_history(const QStringList& symbols);
    void request_visible_history();
    void on_quote(const QString& sym, const services::QuoteData& q);
    void on_history(const QString& sym, const QVector<services::HistoryPoint>& bars);
    bool needs_info_for_all() const;
    bool needs_history_for_all() const;
    bool history_columns_visible() const;
    void update_quote_status();
    void update_last_updated();
    void schedule_footer();
    void update_footer();
    void refresh_alert_summaries();
    void schedule_views();
    void refresh_map_view();
    void refresh_correlation_view();
    void sync_fx();
    void set_display_currency(const QString& ccy);
    void on_current_row_changed();
    /// List filled and no row is current: select the first displayed row and show it
    /// in the detail pane. Not published to the link group (an automatic selection
    /// must not retarget linked screens; only the user's selections publish).
    void select_first_row_if_none();
    void publish_selection_to_group();
    void save_selection();
    void restore_selection();

    // ── Actions (WatchlistScreen_Actions.cpp) ───────────────────────────────
    void start_add_batch(const std::shared_ptr<WatchlistAddBatch>& batch);
    void launch_lookups(const std::shared_ptr<WatchlistAddBatch>& batch);
    void finish_add_batch(const std::shared_ptr<WatchlistAddBatch>& batch);
    void show_next_suggestion(const QString& prefix = {});
    void add_symbols_direct(const QList<SymbolRef>& refs);
    /// Select, scroll to and show in the detail pane rows just added — on the next event-loop
    /// turn, after the reload's model-reset restores the previous selection.
    void reveal_added(const QStringList& symbols);
    void import_entries(const QVector<WatchlistImportEntry>& entries, const QStringList& invalid, int file_duplicates);
    void remove_symbols(const QStringList& symbols);
    void rename_watchlist(int row);
    void edit_list_description(int row);
    void set_list_color(int row, const QString& color);
    void set_default_list(int row);
    void duplicate_list(int row);
    void merge_list(int row, const QString& target_id, bool delete_source);
    void move_list(int row, int delta);
    void persist_sidebar_order();
    void show_watchlist_menu(const QPoint& pos);
    void show_stock_menu(const QPoint& pos);
    void show_list_actions_menu();
    void open_symbol_in(const QString& screen_id, const QString& symbol);
    void edit_notes(const QString& symbol);
    void set_group_for(const QStringList& symbols);
    void open_alerts(const QString& symbol);
    void copy_symbols_to(const QStringList& symbols, const QString& target_id, bool move);
    void move_rows(const QStringList& moved, const QString& anchor, bool after, const QString& group_key,
                   bool has_group);
    /// Move the selected rows `delta` places (+1 down / -1 up) in the manual order.
    void move_selection(int delta);
    void move_selection_to_edge(bool top);
    QStringList base_order_for_move() const;
    void export_json();
    void backtest_symbols(const QStringList& symbols);
    void on_add_text_edited(const QString& text);
    void run_autocomplete();
    void on_completion_chosen(const QString& text);

    QString current_list_name() const;
    int row_of_list(const QString& id) const;

    // ── Data ────────────────────────────────────────────────────────────────
    QVector<fincept::Watchlist> watchlists_;
    QHash<QString, int> counts_;
    QVector<fincept::WatchlistStock> stocks_;
    QString current_wl_id_;
    WatchlistLayout layout_;
    bool applying_layout_ = false;
    /// A model reset happened and the group expansion is restored on the next turn:
    /// until then every group reads as collapsed, so the layout must not capture it.
    bool expansion_restore_pending_ = false;
    /// on_current_row_changed() queued for the end of the current click / key press.
    bool current_row_pending_ = false;

    WatchlistModel* model_ = nullptr;
    WatchlistProxyModel* proxy_ = nullptr;

    // Hub bookkeeping (topics this screen holds, by family).
    bool hub_active_ = false;
    QSet<QString> info_subs_;
    QSet<QString> spark_subs_;
    QSet<QString> hist_subs_;
    QHash<QString, QVector<services::HistoryPoint>> history_;
    QSet<QString> history_failed_;
    qint64 last_quote_ms_ = 0;

    // Selection kept across model resets (regroup, list reload).
    QStringList saved_selection_;
    QString saved_current_;
    int saved_scroll_ = 0;

    // ADD lookups: "<watchlist id>|<SYMBOL>" still running (a second ADD of it is ignored).
    QSet<QString> add_lookups_in_flight_;
    struct Suggestion {
        QString wl_id;
        QString typed;
        QStringList symbols; ///< candidate symbols
        QStringList labels;  ///< "SYM — Name (EXCH)"
        QStringList names;
        QStringList exchanges;
        QString notes;
        QString group;
    };
    QVector<Suggestion> suggestions_;
    QString autocomplete_request_;

    // ── Widgets ─────────────────────────────────────────────────────────────
    QSplitter* splitter_ = nullptr;

    QWidget* sidebar_ = nullptr;
    QWidget* sidebar_header_ = nullptr;
    QLabel* sidebar_title_ = nullptr;
    QListWidget* wl_list_ = nullptr;
    QLabel* wl_count_ = nullptr;
    QPushButton* add_wl_btn_ = nullptr;

    QWidget* main_panel_ = nullptr;
    QWidget* top_bar_ = nullptr;
    QLabel* panel_title_ = nullptr;
    QLabel* stock_count_ = nullptr;
    QLabel* updated_label_ = nullptr;
    QPushButton* view_table_btn_ = nullptr;
    QPushButton* view_map_btn_ = nullptr;
    QPushButton* view_corr_btn_ = nullptr;
    QPushButton* detail_btn_ = nullptr;
    QPushButton* refresh_btn_ = nullptr;
    QPushButton* list_menu_btn_ = nullptr;

    QWidget* add_bar_ = nullptr;
    QLabel* add_label_ = nullptr;
    QLineEdit* add_input_ = nullptr;
    QPushButton* add_btn_ = nullptr;
    QPushButton* remove_btn_ = nullptr;
    QLineEdit* filter_input_ = nullptr;
    QLabel* group_label_ = nullptr;
    QComboBox* group_box_ = nullptr;
    QLabel* ccy_label_ = nullptr;
    QComboBox* ccy_box_ = nullptr;
    QCompleter* completer_ = nullptr;
    QStringListModel* completion_model_ = nullptr;

    QWidget* feedback_bar_ = nullptr;
    QLabel* feedback_label_ = nullptr;
    QHBoxLayout* feedback_actions_ = nullptr;
    QPushButton* feedback_close_ = nullptr;
    QLabel* status_banner_ = nullptr;
    QString status_banner_css_;

    QStackedWidget* view_stack_ = nullptr;
    QWidget* table_page_ = nullptr;
    WatchlistView* view_ = nullptr;
    WatchlistFooter* footer_ = nullptr;
    /// What the empty state shows (its buttons act on it). Built on first need.
    enum class EmptyKind { None, NoList, EmptyList, FilteredOut };
    EmptyKind empty_kind_ = EmptyKind::None;
    fincept::ui::EmptyState* empty_state_ = nullptr;
    WatchlistMapView* map_view_ = nullptr;
    WatchlistCorrelationView* corr_view_ = nullptr;
    WatchlistDetailPane* detail_ = nullptr;

    // ── Timers ──────────────────────────────────────────────────────────────
    QTimer* layout_save_timer_ = nullptr;
    QTimer* footer_timer_ = nullptr;
    QTimer* clock_timer_ = nullptr;
    QTimer* history_timer_ = nullptr;
    QTimer* views_timer_ = nullptr;
    QTimer* feedback_timer_ = nullptr;
    QTimer* autocomplete_timer_ = nullptr;

    // Symbol group link — SymbolGroup::None when unlinked.
    SymbolGroup link_group_ = SymbolGroup::None;

    // EventBus subscriptions — registered in showEvent, released in hideEvent.
    // MCP watchlist tools publish watchlist.created / .deleted / .updated when
    // the LLM mutates watchlists via Finagent or AI Chat.
    QList<EventBus::HandlerId> mcp_event_subs_;
    void subscribe_mcp_events();
    void unsubscribe_mcp_events();
};

} // namespace fincept::screens
