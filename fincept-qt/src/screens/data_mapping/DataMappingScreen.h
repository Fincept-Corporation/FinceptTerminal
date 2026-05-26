#pragma once

#include "screens/common/IStatefulScreen.h"
#include "storage/repositories/DataMappingRepository.h"

#include <QComboBox>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QStackedWidget>
#include <QTableWidget>
#include <QTextEdit>
#include <QTreeWidget>
#include <QWidget>

namespace fincept::screens {

/// Data Mapping configuration screen.
/// 5-step wizard: API Config → Schema Select → Field Mapping → Cache → Test & Save.
/// Supports predefined schemas (OHLCV, QUOTE, TICK, ORDER, POSITION, PORTFOLIO, INSTRUMENT),
/// custom schemas, multiple parser engines, and Indian broker templates.
class DataMappingScreen : public QWidget, public IStatefulScreen {
    Q_OBJECT
  public:
    explicit DataMappingScreen(QWidget* parent = nullptr);

    void restore_state(const QVariantMap& state) override;
    QVariantMap save_state() const override;
    QString state_key() const override { return "data_mapping"; }
    int state_version() const override { return 1; }

  private slots:
    void on_view_changed(int view); // 0=list, 1=create, 2=templates
    void on_step_changed(int step); // 0..4
    void on_next_step();
    void on_prev_step();
    void on_test_api();
    void on_test_mapping();
    void on_save_mapping();
    void on_run_mapping();
    void on_template_selected(int index);
    void on_new_mapping();
    void on_delete_mapping();

  private:
    void setup_ui();

    // Top-level layout builders
    QWidget* create_header();
    QWidget* create_step_bar();
    QWidget* create_main_area();
    QWidget* create_nav_footer();
    QWidget* create_status_bar();

    // Step panels (center content)
    QWidget* create_api_config_panel();
    QWidget* create_schema_panel();
    QWidget* create_field_mapping_panel();
    QWidget* create_cache_panel();
    QWidget* create_test_save_panel();

    // Side panels
    QWidget* create_left_panel();
    QWidget* create_right_panel();

    // View panels
    QWidget* create_list_view();
    QWidget* create_template_view();

    // Helpers
    QWidget* create_form_row(const QString& label, QWidget* input);
    QWidget* create_form_two_col(QWidget* left, QWidget* right);
    void update_step_indicators();
    void update_right_panel();
    void populate_json_tree(const QJsonValue& val, QTreeWidgetItem* parent, const QString& key);
    void populate_mapping_list();
    void refresh_saved_mappings();
    void load_mappings_from_db();
    void build_mapping_config(QJsonObject& config);

  protected:
    void showEvent(QShowEvent* e) override;

    // State
    int current_view_ = 0; // 0=list, 1=create, 2=templates
    int current_step_ = 0; // 0..4
    bool loading_ = false;

    // Saved mappings (persisted via DataMappingRepository)
    QVector<DataMapping> saved_mappings_;

    // View & step buttons
    QList<QPushButton*> view_btns_;
    QList<QPushButton*> step_btns_;

    // Stacked widgets
    QStackedWidget* view_stack_ = nullptr; // list | create | templates
    QStackedWidget* step_stack_ = nullptr; // 5 step panels

    // Navigation footer
    QPushButton* prev_btn_ = nullptr;
    QPushButton* next_btn_ = nullptr;
    QLabel* step_label_ = nullptr;
    QWidget* nav_footer_ = nullptr;

    // Status bar
    QLabel* status_view_ = nullptr;
    QLabel* status_step_ = nullptr;
    QLabel* status_mappings_ = nullptr;

    // ── Step 0: API Config ──
    QLineEdit* api_name_ = nullptr;
    QLineEdit* api_base_url_ = nullptr;
    QLineEdit* api_endpoint_ = nullptr;
    QComboBox* api_method_ = nullptr;
    QComboBox* api_auth_type_ = nullptr;
    QLineEdit* api_auth_value_ = nullptr;
    QPlainTextEdit* api_headers_ = nullptr;
    QPlainTextEdit* api_body_ = nullptr;
    QSpinBox* api_timeout_ = nullptr;
    QPushButton* api_test_btn_ = nullptr;
    QLabel* api_test_status_ = nullptr;

    // ── Step 1: Schema ──
    QComboBox* schema_type_ = nullptr;   // predefined | custom
    QComboBox* schema_select_ = nullptr; // OHLCV, QUOTE, etc.
    QLabel* schema_desc_ = nullptr;
    QTableWidget* schema_fields_table_ = nullptr;

    // ── Step 2: Field Mapping ──
    QTreeWidget* json_tree_ = nullptr;      // JSON explorer
    QTableWidget* mapping_table_ = nullptr; // target field → expression
    QComboBox* parser_engine_ = nullptr;

    // ── Step 3: Cache ──
    QSpinBox* cache_ttl_ = nullptr;
    QComboBox* cache_enabled_ = nullptr;

    // ── Step 4: Test & Save ──
    QPushButton* test_btn_ = nullptr;
    QTextEdit* test_output_ = nullptr;
    QLabel* test_status_ = nullptr;
    QPushButton* save_btn_ = nullptr;

    // ── List view ──
    QListWidget* mapping_list_ = nullptr;

    // ── Template view ──
    QListWidget* template_list_ = nullptr;
    QLabel* template_detail_ = nullptr;

    // ── Right panel info ──
    QLabel* right_engine_status_ = nullptr;
    QLabel* right_schema_info_ = nullptr;
    QLabel* right_fields_info_ = nullptr;
    QLabel* right_test_info_ = nullptr;

    // Sample data from API test
    QJsonDocument sample_data_;
    QJsonObject test_result_;
};

} // namespace fincept::screens
