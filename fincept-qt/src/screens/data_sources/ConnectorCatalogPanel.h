#pragma once
// ConnectorCatalogPanel — the Data Sources › CATALOG tab.
//
// Browse every Python data connector in resources/data_connectors.json, add a missing API key
// inline, pick a command, run it and inspect the result as a table / JSON / raw text, export CSV
// or copy the equivalent AI tool call. All fetching goes through ConnectorService (the same
// runner + cache the MCP data_* tools use); this widget never touches PythonRunner.

#include "services/connectors/ConnectorCatalog.h"
#include "services/connectors/ConnectorService.h"

#include <QPointer>
#include <QString>
#include <QStringList>
#include <QWidget>

class QCheckBox;
class QComboBox;
class QFrame;
class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QPushButton;
class QSortFilterProxyModel;
class QStackedWidget;
class QTabWidget;
class QTableView;
class QVBoxLayout;

namespace fincept::screens::datasources {

class CatalogFilterProxy;
class CatalogListModel;
class ConnectorResultModel;

class ConnectorCatalogPanel : public QWidget {
    Q_OBJECT
  public:
    explicit ConnectorCatalogPanel(QWidget* parent = nullptr);

    QString selected_connector() const { return current_name_; }
    /// Selects a connector by catalogue name (after the catalogue has loaded).
    void select_connector(const QString& name);

  protected:
    void showEvent(QShowEvent* event) override;
    void changeEvent(QEvent* event) override;

  private:
    enum class ResultState { Empty, Loading, Results, Error };

    void build_ui();
    QWidget* build_list_pane();
    QWidget* build_detail_pane();
    void refresh_theme();
    void retranslate_ui();
    void load_catalog();
    void populate_categories();
    void update_count_label();

    void on_selection_changed();
    void show_connector(const fincept::services::connectors::ConnectorInfo* c);
    void rebuild_key_rows(const fincept::services::connectors::ConnectorInfo* c);
    void update_key_status();
    void on_command_changed();
    void on_run();
    void on_result(const QString& request_id, const fincept::services::connectors::ConnectorResult& result);
    void on_error(const QString& request_id, const QString& message);
    void on_export_csv();
    void on_copy_tool_call();
    void save_key(const QString& env_key, QLineEdit* field);

    void set_result_state(ResultState state, const QString& message = {});
    void set_status(const QString& text, const QString& state);
    QStringList current_args() const;
    static QString age_text(qint64 ms);

    // ── List pane ──
    QLabel* list_title_ = nullptr;
    QLabel* count_label_ = nullptr;
    QLineEdit* search_ = nullptr;
    QComboBox* category_ = nullptr;
    QCheckBox* keyless_only_ = nullptr;
    QCheckBox* configured_only_ = nullptr;
    QTableView* list_ = nullptr;
    QLabel* list_empty_ = nullptr;
    CatalogListModel* list_model_ = nullptr;
    CatalogFilterProxy* proxy_ = nullptr;

    // ── Detail pane ──
    QLabel* detail_title_ = nullptr;
    QLabel* title_ = nullptr;
    QLabel* meta_ = nullptr;
    QLabel* desc_ = nullptr;
    QLabel* docs_ = nullptr;
    QLabel* broken_ = nullptr;
    QFrame* key_box_ = nullptr;
    QLabel* key_status_ = nullptr;
    QVBoxLayout* key_rows_ = nullptr;
    QLabel* command_label_ = nullptr;
    QComboBox* command_ = nullptr;
    QLabel* hint_ = nullptr;
    QLabel* args_label_ = nullptr;
    QPlainTextEdit* args_ = nullptr;
    QPushButton* run_btn_ = nullptr;
    QCheckBox* force_ = nullptr;
    QPushButton* export_btn_ = nullptr;
    QPushButton* copy_btn_ = nullptr;
    QLabel* status_ = nullptr;
    QStackedWidget* result_stack_ = nullptr;
    QLabel* empty_label_ = nullptr;
    QLabel* loading_label_ = nullptr;
    QLabel* error_label_ = nullptr;
    QTabWidget* tabs_ = nullptr;
    QTableView* result_table_ = nullptr;
    QPlainTextEdit* json_view_ = nullptr;
    QPlainTextEdit* raw_view_ = nullptr;
    ConnectorResultModel* result_model_ = nullptr;
    QSortFilterProxyModel* result_proxy_ = nullptr;

    // ── State ──
    bool loaded_ = false;
    QString current_name_;
    QString pending_request_;
    QString pending_select_; ///< restore target requested before the catalogue loaded
    fincept::services::connectors::ConnectorResult last_result_;
    bool has_result_ = false;
};

} // namespace fincept::screens::datasources
