#pragma once
// Data Sources Screen — data connector command center.

#include "screens/IStatefulScreen.h"
#include "screens/data_sources/DataSourceTypes.h"
#include "storage/repositories/DataSourceRepository.h"

#include <QCheckBox>
#include <QHideEvent>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QResizeEvent>
#include <QShowEvent>
#include <QStackedWidget>
#include <QTableWidget>
#include <QTimer>
#include <QWidget>

namespace fincept::screens::datasources {

class DataSourcesScreen : public QWidget, public fincept::screens::IStatefulScreen {
    Q_OBJECT
  public:
    explicit DataSourcesScreen(QWidget* parent = nullptr);

    void restore_state(const QVariantMap& state) override;
    QVariantMap save_state() const override;
    QString state_key() const override { return "data_sources"; }
    int state_version() const override { return 1; }

  protected:
    void showEvent(QShowEvent* event) override;
    void hideEvent(QHideEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    bool eventFilter(QObject* obj, QEvent* event) override;

  private slots:
    void on_category_filter(int idx);
    void on_search_changed(const QString& text);
    void on_connector_clicked(const QString& connector_id);
    void on_connection_add();
    void on_connection_edit(const QString& conn_id);
    void on_connection_delete(const QString& conn_id);
    void on_connection_duplicate(const QString& conn_id);
    void on_connection_test(const QString& conn_id);
    void on_connection_enabled_changed(const QString& conn_id, bool enabled);
    void on_connector_selection_changed();
    void on_connection_selection_changed();
    void on_category_selection_changed(int row);
    void on_provider_ladder_activated(QListWidgetItem* item);
    void on_detail_connection_activated(QListWidgetItem* item);
    void on_bulk_enable_all();
    void on_bulk_disable_all();
    void on_bulk_delete_selected();
    void on_export_connections();
    void on_import_connections();
    void on_download_template();
    void on_stat_box_clicked(int stat_index);
    void on_connections_search_changed(const QString& text);
    void on_view_mode_toggle();
    void on_poll_timer();
    void refresh_connections();
    void update_clock();

  private:
    // ── UI build ──────────────────────────────────────────────────────────────
    void setup_ui();
    void apply_screen_styles();
    QWidget* build_screen_header();
    QWidget* build_command_bar(); // stub — kept for compat
    QWidget* build_stats_strip();
    QWidget* build_tab_bar();
    QWidget* build_browse_page();
    QWidget* build_connections_page();
    QWidget* build_category_panel();
    QWidget* build_connector_panel();
    QWidget* build_detail_panel();

    // ── Data / update ─────────────────────────────────────────────────────────
    void build_category_ladder();
    void build_connector_table();
    void build_connections_table();
    void update_stats_strip();
    void update_provider_ladder();
    void update_detail_panel();
    void update_action_states();
    void show_config_dialog(const ConnectorConfig& config, const QString& edit_id = "", bool duplicate = false);
    void update_connection_status_cell(const QString& conn_id, bool ok, const QString& msg);
    void apply_stat_filter(int stat_index);

    QVector<ConnectorConfig> filtered_connectors() const;
    QVector<DataSource> filtered_connection_rows() const;
    void select_connector_by_id(const QString& connector_id);
    QString effective_detail_connection_id() const;
    QString preferred_connection_for_connector(const QString& connector_id) const;

    // ── Screen header ─────────────────────────────────────────────────────────
    QLineEdit* search_edit_ = nullptr;
    QLabel* clock_label_ = nullptr;
    QLabel* count_label_ = nullptr;

    // ── Connections toolbar ───────────────────────────────────────────────────
    QLineEdit* conn_search_edit_ = nullptr;
    QPushButton* bulk_enable_btn_ = nullptr;
    QPushButton* bulk_disable_btn_ = nullptr;
    QPushButton* bulk_delete_btn_ = nullptr;

    // ── Left sidebar ──────────────────────────────────────────────────────────
    QListWidget* category_list_ = nullptr;
    QListWidget* provider_ladder_ = nullptr;

    // ── Connector table ───────────────────────────────────────────────────────
    QTableWidget* connector_table_ = nullptr;

    // ── Detail inspector ──────────────────────────────────────────────────────
    QLabel* detail_title_ = nullptr;
    QLabel* detail_symbol_ = nullptr;
    QLabel* detail_description_ = nullptr;
    QLabel* detail_category_value_ = nullptr;
    QLabel* detail_transport_value_ = nullptr;
    QLabel* detail_auth_value_ = nullptr;
    QLabel* detail_test_value_ = nullptr;
    QLabel* detail_fields_value_ = nullptr;
    QLabel* detail_configured_value_ = nullptr;
    QLabel* detail_enabled_value_ = nullptr;
    QLabel* detail_last_status_value_ = nullptr;
    QTableWidget* field_table_ = nullptr;
    QListWidget* detail_connections_list_ = nullptr;
    QPushButton* new_connection_btn_ = nullptr;
    QPushButton* edit_connection_btn_ = nullptr;
    QPushButton* test_connection_btn_ = nullptr;

    // ── Stats strip ───────────────────────────────────────────────────────────
    QLabel* universe_stat_value_ = nullptr;
    QLabel* configured_stat_value_ = nullptr;
    QLabel* active_stat_value_ = nullptr;
    QLabel* auth_stat_value_ = nullptr;

    // ── Connections table ─────────────────────────────────────────────────────
    QTableWidget* connections_table_ = nullptr;

    // ── Page stack ────────────────────────────────────────────────────────────
    QStackedWidget* page_stack_ = nullptr;

    // ── Timers ────────────────────────────────────────────────────────────────
    QTimer* clock_timer_ = nullptr;
    QTimer* poll_timer_ = nullptr;

    // ── State ─────────────────────────────────────────────────────────────────
    enum class ViewMode { Gallery, Connections };

    QVector<DataSource> connections_cache_;
    QString selected_connector_id_;
    QString selected_connection_id_;
    Category active_category_ = Category::Database;
    bool show_all_categories_ = true;
    ViewMode view_mode_ = ViewMode::Gallery;
    int stat_filter_ = -1; // -1 = none, 0-3 = stat box index
    QString conn_search_text_;
    QMap<QString, QPair<bool, QString>> live_status_cache_;
};

} // namespace fincept::screens::datasources

namespace fincept::screens {
using DataSourcesScreen = datasources::DataSourcesScreen;
}
