// src/screens/gov_data/GovDataFrancePanel.h
// France data.gouv.fr panel — tab-based: Data Services | Datasets | Geo Search
// Script: french_gov_api.py  Color: #2563EB
#pragma once

#include "services/gov_data/GovDataService.h"

#include <QComboBox>
#include <QJsonArray>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QStackedWidget>
#include <QTableWidget>
#include <QWidget>

namespace fincept::screens {

class GovDataFrancePanel : public QWidget {
    Q_OBJECT
  public:
    explicit GovDataFrancePanel(QWidget* parent = nullptr);

  public slots:
    void load_initial_data();

  private slots:
    void on_result(const QString& request_id, const services::GovDataResult& result);
    void on_dataset_row_double_clicked(int row);
    void on_tab_changed(int tab_index);
    void on_fetch();
    void on_back();
    void on_export_csv();

  private:
    // View indices in content_stack_
    enum View { Services = 0, Datasets, Geo, Resources, Status };

    void     build_ui();
    QWidget* build_toolbar();

    void populate_services(const QJsonArray& data);
    void populate_datasets(const QJsonArray& data);
    void populate_geo(const QJsonArray& data);
    void populate_resources(const QJsonArray& data);

    void show_loading(const QString& message);
    void show_error(const QString& message);
    void show_status(const QString& message, bool is_error = false);

    void update_toolbar_state();
    void update_breadcrumb(const QString& text);

    static constexpr const char* kScript = "french_gov_api.py";
    static constexpr const char* kColor  = "#2563EB";

    View current_view_ = Services;
    QString selected_dataset_id_;   // for resources drill-down
    QString search_query_;          // last geo / dataset search

    // Toolbar widgets
    QPushButton* back_btn_      = nullptr;
    QPushButton* services_btn_  = nullptr;
    QPushButton* datasets_btn_  = nullptr;
    QPushButton* geo_btn_       = nullptr;
    QLineEdit*   search_input_  = nullptr;
    QPushButton* fetch_btn_     = nullptr;
    QPushButton* export_btn_    = nullptr;

    // Breadcrumb
    QWidget* breadcrumb_       = nullptr;
    QLabel*  breadcrumb_label_ = nullptr;
    QLabel*  row_count_label_  = nullptr;

    // Content pages
    QStackedWidget* content_stack_    = nullptr;
    QTableWidget*   services_table_   = nullptr;  // page 0
    QTableWidget*   datasets_table_   = nullptr;  // page 1
    QTableWidget*   geo_table_        = nullptr;  // page 2
    QTableWidget*   resources_table_  = nullptr;  // page 3 (column schema)
    QLabel*         status_label_     = nullptr;  // page 4

    // Cached data for CSV export
    QJsonArray current_services_;
    QJsonArray current_datasets_;
    QJsonArray current_geo_;
    QJsonArray current_resources_;
};

} // namespace fincept::screens
