#pragma once
// Data Sources Screen — gallery of 78 connectors + connection management

#include "screens/data_sources/DataSourceTypes.h"

#include <QHideEvent>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QShowEvent>
#include <QStackedWidget>
#include <QTableWidget>
#include <QWidget>

namespace fincept::screens::datasources {

class DataSourcesScreen : public QWidget {
    Q_OBJECT
  public:
    explicit DataSourcesScreen(QWidget* parent = nullptr);

  protected:
    void showEvent(QShowEvent* event) override;
    void hideEvent(QHideEvent* event) override;

  private slots:
    void on_view_gallery();
    void on_view_connections();
    void on_category_filter(int idx);
    void on_search_changed(const QString& text);
    void on_connector_clicked(const QString& connector_id);
    void on_connection_add();
    void on_connection_delete(const QString& conn_id);
    void on_connection_test(const QString& conn_id);
    void refresh_connections();

  private:
    void setup_ui();
    void build_gallery();
    void build_connections_table();
    void show_config_dialog(const ConnectorConfig& config, const QString& edit_id = "");

    // Top bar
    QPushButton* gallery_btn_ = nullptr;
    QPushButton* connections_btn_ = nullptr;
    QLineEdit* search_edit_ = nullptr;
    QPushButton* category_btns_[9] = {};
    QLabel* count_label_ = nullptr;

    // Views
    QStackedWidget* stack_ = nullptr;

    // Gallery view
    QScrollArea* gallery_scroll_ = nullptr;
    QWidget* gallery_grid_ = nullptr;

    // Connections view
    QTableWidget* connections_table_ = nullptr;

    // State
    ViewMode view_mode_ = ViewMode::Gallery;
    Category active_category_ = Category::Database;
    bool show_all_categories_ = true;
};

} // namespace fincept::screens::datasources

// Expose to fincept::screens namespace for MainWindow registration
namespace fincept::screens {
using DataSourcesScreen = datasources::DataSourcesScreen;
}
