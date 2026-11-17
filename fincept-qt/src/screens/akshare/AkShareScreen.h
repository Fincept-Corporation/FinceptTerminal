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
#include <functional>
#include <memory>

class QGridLayout;
class QPlainTextEdit;
class QTableView;

namespace fincept::ui {
class AutoLoadPane;
class EmptyState;
class LoadingSkeleton;
} // namespace fincept::ui

namespace fincept::screens::asia_links {
class ResultModel;
}

namespace fincept::services::akshare {
struct QueryResult;
}

namespace fincept::screens {

/// Data source descriptor for AkShare scripts.
struct AkShareSource {
    QString id;
    QString name;
    QString script; // e.g. "akshare_stocks_realtime.py"
    QString icon;   // emoji
};

/// AkShare Data Explorer screen.
/// Provides access to 25 AkShare data sources with 1000+ endpoints.
/// Users select a source, pick an endpoint, optionally set parameters,
/// and view results in a table or raw JSON.
///
/// Never blank: the first show opens the remembered (or first) source on its remembered (or default,
/// asia_links::default_endpoint) endpoint and runs it. The result area is a ui::AutoLoadPane with one
/// view per "<source>/<endpoint>": the remembered result shows at once with its "as of" time and is
/// refreshed quietly when older than 15 minutes; a newer pick supersedes (and cancels) the run in
/// flight; leaving the screen cancels an automatic run.
///
/// Symbol-group linked whenever a result carries a security-code column (see AsiaSymbolLinks.h):
/// selecting a row publishes its yfinance symbol, a linked A-share/HK symbol fills the SYMBOL
/// parameter and is highlighted when listed, and rows offer "Open in…".
class AkShareScreen : public QWidget, public IStatefulScreen, public fincept::IGroupLinked, public ISectionLinked {
    Q_OBJECT
    Q_INTERFACES(fincept::IGroupLinked)
  public:
    explicit AkShareScreen(QWidget* parent = nullptr);

    /// ISectionLinked — "source:<id>" or the bare source id ("stocks_realtime", "bonds", …
    /// build_sources()): the source button's click path (before the first show: the source the first
    /// show opens). focus = endpoint filter text (the "Search endpoints" box), never a run.
    bool open_section(const QString& section, const QString& focus) override;

    // IStatefulScreen — remembers active source, last endpoint, search text,
    // and whether the user prefers table vs JSON view.
    QVariantMap save_state() const override;
    void restore_state(const QVariantMap& state) override;
    QString state_key() const override { return "akshare"; }

    // IGroupLinked
    void set_group(fincept::SymbolGroup g) override { link_group_ = g; }
    fincept::SymbolGroup group() const override { return link_group_; }
    void on_group_symbol_changed(const fincept::SymbolRef& ref) override;
    fincept::SymbolRef current_symbol() const override;

  protected:
    void changeEvent(QEvent* event) override;
    void showEvent(QShowEvent* event) override;
    void hideEvent(QHideEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

  private slots:
    void on_source_clicked(int index);
    void on_endpoint_clicked(QListWidgetItem* item);
    void on_search_changed(const QString& text);
    void on_execute();
    void on_view_toggle();
    void on_refresh();

  private:
    void setup_ui();
    QWidget* create_header();
    QWidget* create_source_grid();
    QWidget* create_endpoint_panel();
    QWidget* create_params_panel();
    QWidget* create_data_panel();
    QWidget* create_status_bar();

    /// Re-apply tr() lookups to every widget whose text we keep a handle to.
    /// Called from changeEvent() on QEvent::LanguageChange.
    void retranslateUi();
    /// Item colours / fonts of the category headers, the table row height and the source grid —
    /// what stylesheets do not reach; run once the new appearance has settled.
    void apply_appearance();
    /// Lays the source buttons out in as many columns as the width allows.
    void reflow_sources();

    /// First show (one event-loop turn later, after restore_state()): open the remembered or first source.
    void first_show();
    /// Switch to source `index` and open `endpoint` (empty = its default endpoint).
    void select_source(int index, const QString& endpoint);
    /// Point the result pane at `endpoint` of the active source (shows its remembered result, runs
    /// it when there is none or it is old).
    void select_endpoint(const QString& endpoint);
    void load_endpoints(const AkShareSource& source);
    void populate_endpoint_list(const QJsonObject& result);
    /// Highlight `endpoint` in the list (no signal, no run).
    void highlight_endpoint(const QString& endpoint);

    // Result pane
    QString view_key() const;
    QStringList current_args() const;
    QString args_signature() const;
    void update_signature();
    void start_query(quint64 request_id);
    void cancel_query(quint64 request_id);
    void fail_query(quint64 request_id, const QString& endpoint, const services::akshare::QueryResult& r);
    void render_result(const QJsonValue& payload);
    void fill_json_view();

    // Endpoint list states (loading / failed / nothing listed / filter hides everything)
    enum class ListState { List, Loading, Failed, Empty, Filtered };
    void show_list_state(ListState state, const QString& detail = QString());

    /// Status text after a query: the endpoint, plus the parameter boxes it does not use (the
    /// script reports them as "ignored_params").
    static QString query_status_text(const QString& endpoint, const QStringList& ignored);

    // Symbol linking (see class comment)
    QString yf_symbol_at_row(int row) const; // "" when the row has no recognisable security code
    QString name_at_row(int row) const;
    void publish_selection_to_group();
    void show_row_menu(const QPoint& pos);
    int symbol_col_ = -1; // security-code column of the current result, -1 if none
    int name_col_ = -1;
    bool suppress_publish_ = false; // a programmatic selection (group follower) must not re-publish
    fincept::SymbolGroup link_group_ = fincept::SymbolGroup::None;

    // Data sources
    QList<AkShareSource> sources_;
    int active_source_ = -1;
    QString active_endpoint_;
    QHash<QString, QString> last_endpoint_by_source_; // source id -> endpoint last opened there

    // Endpoint cache: script -> { endpoints json }
    QHash<QString, QJsonObject> endpoint_cache_;

    // UI
    QList<QPushButton*> source_btns_;
    QWidget* source_grid_host_ = nullptr;
    QGridLayout* source_grid_ = nullptr;
    int source_cols_ = 0;
    QListWidget* endpoint_list_ = nullptr;
    QLineEdit* search_input_ = nullptr;
    QLabel* endpoint_count_ = nullptr;
    QStackedWidget* list_stack_ = nullptr;
    ui::LoadingSkeleton* list_skeleton_ = nullptr;
    ui::EmptyState* list_state_ = nullptr;
    ListState list_state_kind_ = ListState::List;

    // Header (cached for retranslateUi)
    QLabel* header_title_ = nullptr;
    QLabel* header_sub_ = nullptr;
    QLabel* header_badge_ = nullptr;

    // Parameters
    QLineEdit* param_symbol_ = nullptr;
    QLineEdit* param_start_ = nullptr;
    QLineEdit* param_end_ = nullptr;
    QComboBox* param_period_ = nullptr;

    // Parameter labels (cached for retranslateUi)
    QLabel* sym_label_ = nullptr;
    QLabel* start_label_ = nullptr;
    QLabel* end_label_ = nullptr;
    QLabel* period_label_ = nullptr;

    // Data display
    ui::AutoLoadPane* pane_ = nullptr;
    QStackedWidget* view_stack_ = nullptr;
    QTableView* data_table_ = nullptr;
    asia_links::ResultModel* model_ = nullptr;
    QPlainTextEdit* json_view_ = nullptr;
    QJsonArray json_rows_; // rows of the result on screen, for the JSON view (filled when shown)
    bool json_dirty_ = false;
    quint64 json_seq_ = 0; // the JSON document build whose answer is still wanted (built off the GUI thread)
    QPushButton* view_toggle_btn_ = nullptr;
    QPushButton* exec_btn_ = nullptr;
    QPushButton* refresh_btn_ = nullptr;
    QLabel* data_status_ = nullptr;
    QLabel* record_count_ = nullptr;
    QLabel* as_of_label_ = nullptr;

    // Status bar
    QLabel* status_left_ = nullptr;
    QLabel* status_source_ = nullptr;
    QLabel* status_endpoint_ = nullptr;

    bool is_table_view_ = true;
    bool first_show_done_ = false;
    bool endpoints_loading_ = false;

    // Bumped whenever the source changes; an endpoint list that finds a different value is stale
    // (the list of source A arriving after the user moved on to B) and must not touch the UI.
    int request_seq_ = 0;
    std::shared_ptr<std::atomic<bool>> list_cancel_;                  // endpoint-list run in flight
    QHash<quint64, std::shared_ptr<std::atomic<bool>>> query_tokens_; // pane request id -> its script run
    quint64 waiting_id_ = 0; // pane run waiting for the endpoint list (no endpoint known yet)

    // restore_state(): applied on the first show (nothing loads while the screen is hidden)
    QString restored_source_id_;
    QString pending_endpoint_;
};

} // namespace fincept::screens
