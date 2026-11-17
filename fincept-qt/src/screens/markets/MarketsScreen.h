#pragma once
#include "core/symbol/IGroupLinked.h"
#include "screens/common/ISectionLinked.h"
#include "screens/markets/MarketPanelConfig.h"

#include <QHash>
#include <QList>
#include <QPointer>
#include <QStringList>
#include <QVector>
#include <QWidget>

class QLabel;
class QScrollArea;
class QSplitter;
class QStackedWidget;
class QTimer;
class QEvent;

namespace fincept::ui {
class ScreenStatusBar;
}

namespace fincept::screens {

class MarketPanel;
class MarketsQuoteStore;
class MarketsCommandBar;
class MarketsPulseRibbon;
class MarketsDetailPane;
class MarketsHeatmapView;
class MarketsMoversView;
struct MarketsSymbolPick;

/// Markets — global market overview (spec: markets_redesign_spec).
///
///   command bar   symbol search · PANELS | HEATMAP | MOVERS · LAYOUT · DETAIL · refresh · interval · more
///   pulse ribbon  ten live tiles (S&P 500 ... BITCOIN)
///   body          panel grid (3 / 2 / 1 columns by width, maximise) or HEATMAP / MOVERS  |  detail pane
///   status bar    feed state · coverage · breadth · freshness · source · notice ... sessions · clocks
///
/// One data owner (MarketsQuoteStore) holds every symbol's state and is the only
/// DataHub client for quotes / sparklines; subscriptions exist only while the screen
/// is visible. Panels, ribbon, detail pane and the overview views read it.
///
/// Symbol-group linked: selecting an equity / ETF row publishes it to the screen's
/// link group (indices, FX, futures and crypto are not published); a symbol arriving
/// from the group is marked in every panel that lists it.
///
/// MarketsScreen.cpp: construction, wiring, IGroupLinked, show / hide, theme,
/// retranslation, settings. MarketsScreen_Layout.cpp: the panel grid (build / re-flow,
/// move, maximise, splitter state), panel add / edit / remove with undo.
class MarketsScreen : public QWidget, public fincept::IGroupLinked, public ISectionLinked {
    Q_OBJECT
    Q_INTERFACES(fincept::IGroupLinked)
  public:
    explicit MarketsScreen(QWidget* parent = nullptr);
    ~MarketsScreen() override;

    // IGroupLinked
    void set_group(fincept::SymbolGroup g) override;
    fincept::SymbolGroup group() const override { return link_group_; }
    void on_group_symbol_changed(const fincept::SymbolRef& ref) override;
    fincept::SymbolRef current_symbol() const override;

    // ISectionLinked — "panels" (also "overview") | "heatmap" | "movers" (the view switch), or
    // "panel:<panel id or title>" (that panel on the grid, focused). focus with section "" = a symbol:
    // selected in the first panel that lists it. Nothing is added, removed or fetched by a link.
    bool open_section(const QString& section, const QString& focus) override;

  public slots:
    /// F5 / View > Refresh (WindowFrame::refresh_focused_panel invokes this name).
    void on_refresh();

  protected:
    void showEvent(QShowEvent* event) override;
    void hideEvent(QHideEvent* event) override;
    void changeEvent(QEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

  private:
    // ── MarketsScreen.cpp ──
    void load_prefs();
    void build_ui();
    void wire_shortcuts();
    void retranslateUi();
    void refresh_theme();
    void on_appearance_settled();
    void on_coverage_changed();
    /// Status bar (the shared ui::ScreenStatusBar): items, sessions / clocks / freshness tick.
    void build_status_bar();
    void tick_status_bar();
    void update_banner();
    void on_symbol_selected(const QString& panel_id, const QString& symbol);
    void on_symbol_activated(const QString& symbol);
    void on_detail_requested(const QString& symbol, bool toggle);
    void show_symbol(const QString& symbol);
    void publish_row_symbol(const QString& symbol);
    void set_view(int view, bool save);
    void set_detail_visible(bool on, bool save);
    void apply_detail_visibility();
    void ensure_detail();
    void ensure_heatmap();
    void ensure_movers();
    void set_pulse_visible(bool on, bool save);
    void on_interval_changed(int ms);
    void toggle_auto();
    void schedule_sparklines();
    void push_panel_symbols();
    void schedule_config_save();
    void flush_config_save();
    void on_search_picked(const QVector<MarketsSymbolPick>& picks);
    void on_search_unresolved(const QStringList& texts, const QString& message);
    void focus_search();
    void export_focused_panel();
    void export_all();
    void apply_columns_for_all(const QStringList& ids);
    void reset_all_widths();
    bool select_in_panels(const QString& symbol, bool focus);
    void show_add_to_panel_menu(const QString& symbol);
    void run_resolve_stored();
    void save_pref(const QString& key, const QString& value);

    // ── MarketsScreen_Layout.cpp ──
    void create_panels();
    MarketPanel* create_panel(const MarketPanelConfig& cfg, QWidget* parent);
    void wire_panel(MarketPanel* p);
    void build_grid();
    int effective_columns() const;
    void schedule_reflow();
    void apply_reflow();
    QVector<QStringList> saved_columns() const;
    void renumber_configs();
    void save_splitter_state();
    void restore_splitter_state();
    void save_vsplit_state();
    bool restore_vsplit_state();
    void equalise_panels();
    void apply_compact_heights();
    MarketPanel* panel_by_id(const QString& id) const;
    MarketPanelConfig* config_by_id(const QString& id);
    QVector<MarketPanel*> panels_in_reading_order() const;
    QStringList all_panel_symbols(const QString& except_panel = {}) const;
    void open_editor(const QString& panel_id);
    void add_panel(const QStringList& symbols = {});
    void on_panel_delete(const QString& panel_id);
    void reset_panels();
    void on_panel_config_changed(const MarketPanelConfig& cfg);
    void move_panel(const QString& panel_id, Qt::ArrowType direction);
    /// The saved 3-column layout `cols` into the configs (column / splitter index), saved,
    /// the grid rebuilt and `focus_id`'s table focused — the Move menu's and the drag's path.
    void apply_saved_columns(const QVector<QStringList>& cols, const QString& focus_id);
    /// Drag-to-reorder by a panel header: the drop bar follows the pointer over the grid …
    void on_panel_drag_moved(const QString& panel_id, const QPoint& global_pos);
    /// … and a drop on the grid moves the panel there (Esc / a release outside cancels).
    void on_panel_drag_finished(const QString& panel_id, const QPoint& global_pos, bool drop);
    /// The insertion bar / column frame for dragging `panel_id` to `global_pos`; hidden
    /// when the point is off the grid or the drop would not move the panel. True (with
    /// the saved slot in `column` / `index`) when a drop there moves it.
    bool update_drop_bar(const QString& panel_id, const QPoint& global_pos, int* column, int* index);
    void toggle_maximise(const QString& panel_id);
    void restore_maximised();
    void remove_symbol(const QString& panel_id, const QString& symbol);
    void replace_symbol(const QString& panel_id, const QString& from, const QString& to);
    bool add_symbol(const QString& panel_id, const QString& symbol, bool select, int at = -1);
    void focus_panel(const QString& panel_id, int delta);

    // Data
    MarketsQuoteStore* store_ = nullptr;
    QVector<MarketPanelConfig> configs_;
    QHash<QString, MarketPanel*> panels_; ///< id -> panel (panels persist across grid rebuilds)
    fincept::SymbolGroup link_group_ = fincept::SymbolGroup::None;
    QString selected_symbol_;
    QString pending_publish_;
    QHash<QString, QString> resolver_notes_;      ///< stored symbol -> resolver note
    QHash<QString, QString> resolver_successors_; ///< stored symbol -> current successor (offered)
    QPointer<QWidget> last_table_;

    // Chrome
    MarketsCommandBar* command_bar_ = nullptr;
    MarketsPulseRibbon* ribbon_ = nullptr;
    QLabel* banner_ = nullptr;
    QSplitter* body_split_ = nullptr;
    QStackedWidget* view_stack_ = nullptr;
    QStackedWidget* grid_stack_ = nullptr; ///< page 0: the grid; page 1: one maximised panel
    QScrollArea* grid_scroll_ = nullptr;
    QWidget* grid_host_ = nullptr;
    QWidget* max_page_ = nullptr;
    QLabel* no_panels_ = nullptr;
    QSplitter* h_splitter_ = nullptr;
    QVector<QSplitter*> col_splitters_;
    /// Drag-to-reorder indicator over the grid (decorative, mouse-transparent; created on
    /// the first drag, a child of grid_host_ so it scrolls with the panels).
    QPointer<QWidget> drop_bar_;
    fincept::ui::ScreenStatusBar* status_bar_ = nullptr;
    QTimer* status_tick_ = nullptr;      ///< 20 s: sessions, clocks, freshness (only while shown)
    qint64 status_newest_ms_ = 0;        ///< newest quote time of the last coverage
    bool status_have_coverage_ = false;
    MarketsDetailPane* detail_ = nullptr;
    MarketsHeatmapView* heatmap_ = nullptr;
    MarketsMoversView* movers_ = nullptr;
    QWidget* search_ = nullptr;

    // Grid state
    int grid_columns_ = 0;          ///< columns the grid is built with (3 / 2 / 1)
    bool h_restored_ = false;
    QString maximised_id_;
    QPointer<QSplitter> max_split_;
    int max_index_ = -1;
    QByteArray max_split_state_;

    // Undo (one level, in memory)
    struct Undo {
        enum class Kind { None, Symbol, Panel } kind = Kind::None;
        QString panel_id;
        QString symbol;
        int index = -1;
        MarketPanelConfig panel;
        int panel_pos = -1;
    } undo_;

    // Preferences
    bool auto_update_ = true;
    int interval_ms_ = 60000;
    int view_ = 0;
    int detail_pref_ = -1; ///< -1 = by width, 0 / 1 = the user's choice
    bool pulse_visible_ = true;
    bool resolved_stored_ = false;

    // Timers
    QTimer* save_timer_ = nullptr;
    QTimer* spark_timer_ = nullptr;
    QTimer* reflow_timer_ = nullptr;
    QTimer* vsplit_save_timer_ = nullptr;
};

} // namespace fincept::screens
