#pragma once

#include "core/symbol/IGroupLinked.h"
#include "screens/common/ISectionLinked.h"
#include "screens/common/IStatefulScreen.h"

#include <QComboBox>
#include <QEvent>
#include <QHash>
#include <QJsonArray>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QStackedWidget>
#include <QWidget>

#include <atomic>
#include <memory>

class QPlainTextEdit;
class QScrollArea;
class QTableView;

namespace fincept::ui {
class AutoLoadPane;
class EmptyState;
class LoadingSkeleton;
} // namespace fincept::ui

namespace fincept::screens::asia_links {
class ResultModel;
}

namespace fincept::services::asia_markets {
struct QueryResult;
}

namespace fincept::screens {

/// Stock category descriptor for the Asia Markets explorer. The endpoint list (and its
/// count) comes from the script's own `get_all_endpoints` at runtime; a hard-coded count
/// here drifted from the scripts and was never read.
struct StockCategory {
    QString id;
    QString name;
    QString script;
};

/// Asia Markets Terminal — ~390 AKShare stock endpoints for
/// Chinese A/B shares, Hong Kong, and US markets.
/// 8 categories: Realtime, Historical, Financial, Holdings,
/// Fund Flow, Boards, Margin/HSGT, Hot & News.
///
/// Never blank: the first show opens the remembered (or first) category on its remembered (or
/// default, asia_links::default_endpoint) endpoint and runs it. The result area is a
/// ui::AutoLoadPane with one view per "<category>/<endpoint>" (remembered result with its "as of"
/// time, quiet refresh after 15 minutes, skeleton while loading, designed failure pages with RETRY).
///
/// Symbol-group linked whenever a result carries a security-code column: selecting a row publishes
/// its yfinance symbol (600519 -> 600519.SS), a linked A-share/HK symbol fills the SYMBOL parameter
/// and is highlighted when listed, and rows offer "Open in…" (Research, Trading, News, Backtest,
/// Watchlist).
class AsiaMarketsScreen : public QWidget, public IStatefulScreen, public fincept::IGroupLinked, public ISectionLinked {
    Q_OBJECT
    Q_INTERFACES(fincept::IGroupLinked)
  public:
    explicit AsiaMarketsScreen(QWidget* parent = nullptr);

    void restore_state(const QVariantMap& state) override;
    QVariantMap save_state() const override;
    QString state_key() const override { return "asia_markets"; }
    int state_version() const override { return 1; }

    /// ISectionLinked — a category id (realtime historical financial holders fund_flow boards margin
    /// hot_news; also "category:<id>"): the category button's click path (before the first show: the
    /// category the first show opens); "region:<cn_a|cn_b|hk|us|all>": the region filter buttons.
    /// focus = endpoint filter text (the "Search endpoints" box), never a run.
    bool open_section(const QString& section, const QString& focus) override;

    // IGroupLinked
    void set_group(fincept::SymbolGroup g) override { link_group_ = g; }
    fincept::SymbolGroup group() const override { return link_group_; }
    void on_group_symbol_changed(const fincept::SymbolRef& ref) override;
    fincept::SymbolRef current_symbol() const override;

  protected:
    void showEvent(QShowEvent* e) override;
    void hideEvent(QHideEvent* e) override;
    void changeEvent(QEvent* event) override;

  private slots:
    void on_category_changed(int index);
    void on_region_changed(int index);
    void on_endpoint_clicked(QListWidgetItem* item);
    void on_search_changed(const QString& text);
    void on_execute();
    void on_view_toggle();
    void on_refresh();

  private:
    void setup_ui();
    QWidget* create_header();
    QWidget* create_category_bar();
    QWidget* create_left_panel();
    QWidget* create_data_panel();
    QWidget* create_status_bar();

    /// Re-apply tr() lookups to every widget whose text we keep a handle to.
    /// Called from changeEvent() on QEvent::LanguageChange.
    void retranslateUi();
    /// Item colours / fonts of the category headers, the table row height and the category bar
    /// height — what stylesheets do not reach; run once the new appearance has settled.
    void apply_appearance();
    void fit_category_bar();

    /// First show (one event-loop turn later, after restore_state()): open the remembered or first category.
    void first_show();
    /// Switch to category `index` and open `endpoint` (empty = its default endpoint).
    void select_category(int index, const QString& endpoint);
    /// Point the result pane at `endpoint` of the active category.
    void select_endpoint(const QString& endpoint);
    void load_endpoints(int cat_index);
    void populate_endpoint_list(const QJsonObject& result);
    void highlight_endpoint(const QString& endpoint);
    /// The endpoint names of a get_all_endpoints answer, in list order (categories, then OTHER).
    static QStringList endpoint_names(const QJsonObject& result);
    /// Header subtitle: the real endpoint count of what is loaded (all categories once all are known).
    void update_header_count();

    // Result pane
    QString view_key() const;
    QStringList current_args() const;
    QString args_signature() const;
    void update_signature();
    void start_query(quint64 request_id);
    void cancel_query(quint64 request_id);
    void fail_query(quint64 request_id, const QString& endpoint, const services::asia_markets::QueryResult& r);
    void render_result(const QJsonValue& payload);
    void fill_json_view();

    // Endpoint list states (loading / failed / nothing listed / search or region hides everything)
    enum class ListState { List, Loading, Failed, Empty, Filtered };
    void show_list_state(ListState state, const QString& detail = QString());
    /// The page shown when the region filter hides every endpoint of the category.
    void fail_region_empty(quint64 request_id);

    // Symbol linking (see class comment)
    QString yf_symbol_at_row(int row) const; // "" when the row has no recognisable security code
    QString name_at_row(int row) const;
    void publish_selection_to_group();
    void show_row_menu(const QPoint& pos);
    int symbol_col_ = -1; // security-code column of the current result, -1 if none
    int name_col_ = -1;
    bool suppress_publish_ = false; // a programmatic selection (group follower) must not re-publish
    fincept::SymbolGroup link_group_ = fincept::SymbolGroup::None;

    // Categories
    QList<StockCategory> categories_;
    int active_category_ = -1;
    QHash<QString, QString> last_endpoint_by_category_; // category id -> endpoint last opened there

    // Market regions — a client-side filter of the endpoint list (by endpoint name)
    QStringList regions_ = {"CN_A", "CN_B", "HK", "US"};
    QStringList region_labels_ = {"CN A", "CN B", "HK", "US"};
    int active_region_ = -1; // -1 = all regions
    /// True when `endpoint` belongs to region index `region` (0 CN A, 1 CN B, 2 HK, 3 US; -1 = any).
    static bool endpoint_in_region(const QString& endpoint, int region);
    /// Hide endpoints that miss the search text or the active region; hide headers left empty.
    /// Returns the number of endpoints left visible.
    int apply_list_filter();

    // Endpoint cache per script
    QHash<QString, QJsonObject> endpoint_cache_;
    QString active_endpoint_;

    // UI — category bar
    QScrollArea* cat_scroll_ = nullptr;
    QList<QPushButton*> cat_btns_;
    QList<QPushButton*> region_btns_;

    // UI — header (cached for retranslateUi)
    QLabel* header_title_ = nullptr;
    QLabel* header_sub_ = nullptr;

    // UI — left panel
    QListWidget* endpoint_list_ = nullptr;
    QLineEdit* search_input_ = nullptr;
    QLineEdit* symbol_input_ = nullptr;
    QLabel* endpoint_count_label_ = nullptr;
    QLabel* sym_label_ = nullptr; // (cached for retranslateUi)
    QStackedWidget* list_stack_ = nullptr;
    ui::LoadingSkeleton* list_skeleton_ = nullptr;
    ui::EmptyState* list_state_ = nullptr;
    ListState list_state_kind_ = ListState::Loading;
    bool list_filtered_by_region_ = false; // the Filtered state comes from the region filter

    // UI — data panel
    ui::AutoLoadPane* pane_ = nullptr;
    QStackedWidget* view_stack_ = nullptr;
    QTableView* data_table_ = nullptr;
    asia_links::ResultModel* model_ = nullptr;
    QPlainTextEdit* json_view_ = nullptr;
    QJsonArray json_rows_;
    bool json_dirty_ = false;
    quint64 json_seq_ = 0; // the JSON document build whose answer is still wanted (built off the GUI thread)
    QPushButton* view_toggle_btn_ = nullptr;
    QPushButton* exec_btn_ = nullptr;
    QPushButton* refresh_btn_ = nullptr; // (cached for retranslateUi)
    QLabel* data_status_ = nullptr;
    QLabel* record_count_ = nullptr;
    QLabel* as_of_label_ = nullptr;

    // UI — status bar
    QLabel* status_left_ = nullptr; // (cached for retranslateUi)
    QLabel* status_category_ = nullptr;
    QLabel* status_region_ = nullptr;

    bool is_table_view_ = true;
    bool first_show_done_ = false;
    bool endpoints_loading_ = false;

    // Bumped whenever the category changes; an endpoint list that finds a different value is stale
    // (category A's list arriving after the user moved on to B) and must not touch the UI.
    int request_seq_ = 0;
    std::shared_ptr<std::atomic<bool>> list_cancel_;
    QHash<quint64, std::shared_ptr<std::atomic<bool>>> query_tokens_;
    quint64 waiting_id_ = 0; // pane run waiting for the endpoint list

    // restore_state(): applied on the first show (nothing loads while the screen is hidden)
    int restored_category_ = -1;
    QString pending_endpoint_;
};

} // namespace fincept::screens
