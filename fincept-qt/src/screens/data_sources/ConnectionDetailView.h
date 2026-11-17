#pragma once
// ConnectionDetailView — the right pane of Data Sources for one saved (or new) connection:
//   header with the TRUE status of this connection (last real TEST: result, when, latency),
//   SETTINGS (inline form) · BROWSE (schema / entity list) · DATA (sample rows, exports,
//   Send to Data Mapping, Add to Report). Every request goes through ConnectionService on its
//   worker pool; saves and deletes run off the GUI thread too.

#include "screens/data_sources/DataWorkspace.h"
#include "services/data_sources/ConnectionDrivers.h"
#include "storage/repositories/DataSourceRepository.h"

#include <QWidget>

class QLabel;
class QLineEdit;
class QPushButton;
class QScrollArea;
class QSpinBox;
class QStackedWidget;
class QTabWidget;
class QTableView;
class QTimer;

namespace fincept::ui {
class EmptyState;
class LoadingSkeleton;
} // namespace fincept::ui

namespace fincept::screens::datasources {

class ConnectionForm;
struct ConnectorConfig;

class ConnectionDetailView : public QWidget {
    Q_OBJECT
  public:
    explicit ConnectionDetailView(QWidget* parent = nullptr);

    /// Show a saved connection (resets the tabs when it is another one).
    void show_connection(const DataSource& ds);
    /// A new connection of `type_id`; `duplicate_of` copies its settings.
    void show_new(const QString& type_id, const DataSource* duplicate_of = nullptr);
    /// The list was reloaded: refresh the status line if this is the connection on screen.
    void update_from(const DataSource& ds);
    QString connection_id() const { return id_; }
    bool has_unsaved_edits() const;
    /// Ask, then delete the connection on screen (context-menu entry of the list).
    void delete_with_confirm() { on_delete(); }

  signals:
    void saved(const QString& id);
    void deleted(const QString& id);
    void duplicate_requested(const QString& id);

  protected:
    void hideEvent(QHideEvent* e) override;

  private:
    void build();
    void set_type(const QString& type_id);
    void reset_tabs();
    void update_header();
    void set_status_line(const QString& text, const char* look);
    void show_banner(const QString& text, const char* look);

    void on_test();
    void on_save();
    void on_delete();
    void on_list();
    void on_fetch(const QString& entity_override = QString());
    void cancel_jobs();
    fincept::services::datasources::SourceRequest unsaved_request(fincept::services::datasources::SourceOp op) const;
    bool use_saved_config() const;
    void show_fetch_error(const fincept::services::datasources::SourceError& e);
    void show_list_error(const fincept::services::datasources::SourceError& e);
    void run_action(ErrorAction action);

    // State
    QString id_;           ///< "" while the connection is new
    DataSource ds_;        ///< last loaded copy (saved connections)
    QString type_id_;
    const ConnectorConfig* connector_ = nullptr;
    fincept::services::datasources::TypeSupport support_;
    quint64 test_job_ = 0;
    quint64 list_job_ = 0;
    quint64 fetch_job_ = 0;
    quint64 gen_ = 0;      ///< bumped when another connection is shown: late replies are dropped
    bool saving_ = false;
    int retry_left_ = 0;

    // Header
    QLabel* code_ = nullptr;
    QLabel* title_ = nullptr;
    QLabel* subtitle_ = nullptr;
    QPushButton* test_btn_ = nullptr;
    QPushButton* save_btn_ = nullptr;
    QPushButton* dup_btn_ = nullptr;
    QPushButton* delete_btn_ = nullptr;
    QLabel* status_ = nullptr;
    QLabel* support_banner_ = nullptr;
    QLabel* banner_ = nullptr;

    QTabWidget* tabs_ = nullptr;
    // SETTINGS
    QScrollArea* form_scroll_ = nullptr;
    ConnectionForm* form_ = nullptr;
    // BROWSE
    QStackedWidget* list_stack_ = nullptr;
    fincept::ui::EmptyState* list_state_ = nullptr;
    fincept::ui::LoadingSkeleton* list_skeleton_ = nullptr;
    QTableView* list_view_ = nullptr;
    TabularTableModel* list_model_ = nullptr;
    QLabel* list_info_ = nullptr;
    // DATA
    QLineEdit* entity_ = nullptr;
    QLineEdit* query_ = nullptr;
    QLabel* query_label_ = nullptr;
    QSpinBox* limit_ = nullptr;
    QPushButton* fetch_btn_ = nullptr;
    QStackedWidget* data_stack_ = nullptr;
    fincept::ui::EmptyState* data_state_ = nullptr;
    QWidget* data_loading_ = nullptr;
    fincept::ui::LoadingSkeleton* data_skeleton_ = nullptr;
    ResultPane* result_ = nullptr;
    ErrorAction data_primary_ = ErrorAction::None; ///< what the DATA page's buttons do
    ErrorAction data_secondary_ = ErrorAction::None;
    QTimer* retry_timer_ = nullptr;
};

} // namespace fincept::screens::datasources
