#pragma once
// DataMappingScreen — the Data Mapping tab: SOURCE → MAPPING GRID → PREVIEW.
//
//   Source: a saved Data Sources connection (table / path / query), a catalogue connector
//     (command + arguments), a local file (CSV / JSON / XML / Excel) or an HTTP request (mappings
//     made before v062). Rows also arrive from Data Sources through "Send to Data Mapping".
//   Grid: one row per target field — type, source column, format (dates / periods chosen per
//     column; an undecidable day/month order is flagged, never guessed), scale (minor units,
//     thousands), default, required — checked live against the loaded rows.
//   Preview: the mapped rows, every cell that could not be cast marked with its reason, and a
//     validation summary. SAVE keeps a new revision; RUN maps every row, stores the output and
//     publishes it on DataHub "mapping:<id>"; EXPORT writes CSV, JSON or a SQLite table.
// Auth values of HTTP sources live only in SecureStorage (DataMappingRepository). Built on first
// show; lists are model/view; loading, mapping, saving and exporting run off the GUI thread.

#include "screens/common/ISectionLinked.h"
#include "screens/common/IStatefulScreen.h"
#include "services/data_mapping/MappingEngine.h"
#include "services/data_mapping/MappingSpec.h"
#include "services/data_sources/TabularData.h"
#include "storage/repositories/DataMappingRepository.h"

#include <QVariantMap>
#include <QWidget>

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QSplitter;
class QStackedWidget;
class QTableView;
class QTimer;
class QToolButton;

namespace fincept::ui {
class EmptyState;
class LoadingSkeleton;
} // namespace fincept::ui

namespace fincept::services::datamapping {
struct MappingHandoff;
}

namespace fincept::screens {

namespace datasources {
class TabularTableModel;
class TabularFilterProxy;
} // namespace datasources

namespace datamapping {
class FieldGridModel;
class MappingListModel;
} // namespace datamapping

class DataMappingScreen : public QWidget, public IStatefulScreen, public fincept::screens::ISectionLinked {
    Q_OBJECT
  public:
    explicit DataMappingScreen(QWidget* parent = nullptr);

    void restore_state(const QVariantMap& state) override;
    QVariantMap save_state() const override;
    QString state_key() const override { return QStringLiteral("data_mapping"); }
    int state_version() const override { return 2; }

    // ISectionLinked — no tabs: "new" (+ NEW), "mapping:<id>" (a saved mapping, selected in the
    // list as a click does; its own unsaved-changes question first). `focus` = a mapping id (with
    // an empty section). Before the list has loaded the link is kept and applied then.
    bool open_section(const QString& section, const QString& focus) override;

  protected:
    void showEvent(QShowEvent* event) override;

  private:
    // Build
    void build_workspace();
    QWidget* build_list_pane(QWidget* parent);
    QWidget* build_editor(QWidget* parent);
    QWidget* build_source_box(QWidget* parent);
    QWidget* build_grid_box(QWidget* parent);
    QWidget* build_preview_box(QWidget* parent);

    // List / open / new
    void reload_list(const QString& select_id = QString());
    void load_choices();
    void open_mapping(const QString& id);
    void new_mapping(const fincept::services::datamapping::MappingHandoff* from);
    void take_handoff();
    bool confirm_discard();

    // Editor <-> spec
    void apply_spec_to_ui();
    void read_source_from_ui();
    void read_options_from_ui();
    void set_dirty(bool dirty);
    void update_header();
    void update_kind_page();
    void update_connector_commands();
    /// An Excel file in the file source: list its sheets (data_local_file "sheets", off-thread).
    void refresh_file_sheets();
    void show_banner(const QString& text, const char* look);

    // Source / preview
    void on_load_source();
    void set_source_data(const fincept::services::datasources::TabularResult& data, const QStringList& notes,
                         const QString& label);
    void show_source_table(int index);
    void auto_fields(bool replace);
    void on_schema_changed();
    void schedule_preview();
    void apply_preview(const fincept::services::datamapping::MappingOutput& out, bool full_run);
    void show_preview_state(const QString& title, const QString& message, const QString& action);

    // Actions
    void on_save(bool then_run);
    void on_run();
    void on_delete();
    void on_export(int kind);
    void on_revision_chosen(int index);
    fincept::DataMapping mapping_for_save() const;

    // ── State ──
    bool built_ = false;
    bool list_loaded_ = false; ///< the mapping list has been read once
    QVariantMap pending_restore_;
    // A deep link (open_section) that arrived before the list.
    QString pending_link_section_;
    QString pending_link_focus_;
    bool has_pending_link_ = false;
    QString current_id_;   ///< "" = new, not saved yet
    fincept::DataMapping current_;
    fincept::services::datamapping::MappingSpec spec_;
    bool dirty_ = false;
    bool loading_ui_ = false;
    bool auth_unreadable_ = false;
    QString auth_error_;
    fincept::services::datasources::TabularResult source_data_;
    int source_table_ = 0;
    bool has_source_ = false;
    QString source_label_;
    fincept::services::datamapping::MappingOutput preview_;
    bool has_preview_ = false;
    bool preview_is_run_ = false;
    quint64 load_job_ = 0;
    quint64 gen_ = 0;
    quint64 preview_gen_ = 0;
    bool busy_ = false;

    // ── Widgets ──
    QStackedWidget* root_stack_ = nullptr;
    QSplitter* splitter_ = nullptr;
    QSplitter* editor_split_ = nullptr;
    // list
    QStackedWidget* list_stack_ = nullptr;
    QTableView* list_view_ = nullptr;
    datamapping::MappingListModel* list_model_ = nullptr;
    fincept::ui::EmptyState* list_empty_ = nullptr;
    // editor shell
    QStackedWidget* right_stack_ = nullptr;
    fincept::ui::EmptyState* intro_ = nullptr;
    QWidget* editor_ = nullptr;
    QLineEdit* name_ = nullptr;
    QComboBox* schema_ = nullptr;
    QComboBox* revision_ = nullptr;
    QPushButton* save_btn_ = nullptr;
    QPushButton* run_btn_ = nullptr;
    QToolButton* export_btn_ = nullptr;
    QPushButton* delete_btn_ = nullptr;
    QLabel* status_ = nullptr;
    QLabel* banner_ = nullptr;
    QTimer* banner_timer_ = nullptr;
    // source
    QComboBox* kind_ = nullptr;
    QStackedWidget* kind_pages_ = nullptr;
    QComboBox* conn_pick_ = nullptr;
    QLineEdit* conn_entity_ = nullptr;
    QLineEdit* conn_query_ = nullptr;
    QComboBox* connector_pick_ = nullptr;
    QComboBox* command_pick_ = nullptr;
    QLineEdit* args_edit_ = nullptr;
    QLineEdit* file_path_ = nullptr;
    QComboBox* file_detail_ = nullptr; ///< Excel: sheet (picker fed by data_local_file "sheets"); JSON / XML: rows path
    QString sheets_request_;
    QLineEdit* http_base_ = nullptr;
    QLineEdit* http_endpoint_ = nullptr;
    QComboBox* http_method_ = nullptr;
    QComboBox* http_auth_type_ = nullptr;
    QLineEdit* http_auth_value_ = nullptr;
    QLineEdit* http_headers_ = nullptr;
    QLineEdit* http_rows_path_ = nullptr;
    QPushButton* load_btn_ = nullptr;
    QComboBox* table_pick_ = nullptr;
    QLabel* source_info_ = nullptr;
    // grid
    QStackedWidget* grid_stack_ = nullptr;
    QTableView* grid_view_ = nullptr;
    datamapping::FieldGridModel* grid_model_ = nullptr;
    fincept::ui::EmptyState* grid_empty_ = nullptr;
    QPushButton* add_field_btn_ = nullptr;
    QPushButton* remove_field_btn_ = nullptr;
    QPushButton* auto_btn_ = nullptr;
    QComboBox* decimal_ = nullptr;
    QComboBox* duplicates_ = nullptr;
    QLineEdit* key_fields_ = nullptr;
    QCheckBox* drop_invalid_ = nullptr;
    // preview
    QLabel* summary_ = nullptr;
    QCheckBox* problems_only_ = nullptr;
    QStackedWidget* preview_stack_ = nullptr;
    fincept::ui::EmptyState* preview_empty_ = nullptr;
    fincept::ui::LoadingSkeleton* preview_skeleton_ = nullptr;
    QTableView* preview_view_ = nullptr;
    datasources::TabularTableModel* preview_model_ = nullptr;
    datasources::TabularFilterProxy* preview_proxy_ = nullptr;
    QTimer* preview_timer_ = nullptr;
};

} // namespace fincept::screens
