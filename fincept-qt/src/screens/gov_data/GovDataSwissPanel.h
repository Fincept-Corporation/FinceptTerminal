// src/screens/gov_data/GovDataSwissPanel.h
// Switzerland Open Government Data panel (opendata.swiss / CKAN).
// 3-view hierarchy: Publishers → Datasets → Resources + Search.
// Adds original_name tooltip on publisher rows and a "opendata.swiss" watermark.
#pragma once

#include "services/gov_data/GovDataService.h"

#include <QJsonArray>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QStackedWidget>
#include <QTableWidget>
#include <QWidget>

namespace fincept::screens {

class GovDataSwissPanel : public QWidget {
    Q_OBJECT
  public:
    explicit GovDataSwissPanel(QWidget* parent = nullptr);

  public slots:
    void load_initial_data();

  private slots:
    void on_result(const QString& request_id, const services::GovDataResult& result);
    void on_publisher_clicked(int row);
    void on_dataset_clicked(int row);
    void on_view_changed(int index);
    void on_search();
    void on_back();
    void on_export_csv();

  private:
    void     build_ui();
    QWidget* build_toolbar();
    QWidget* build_status_bar();
    void     show_loading(const QString& message);
    void     show_error(const QString& message);
    void     show_empty(const QString& message);
    void     populate_publishers(const QJsonArray& data);
    void     populate_datasets(const QJsonArray& data, int total_count);
    void     populate_resources(const QJsonArray& data);
    void     update_toolbar_state();
    void     update_breadcrumb();

    static constexpr const char* kScript = "swiss_gov_api.py";
    static constexpr const char* kColor  = "#E11D48";

    enum View { Publishers = 0, Datasets, Resources, Search };
    View current_view_ = Publishers;

    // Toolbar
    QPushButton* back_btn_       = nullptr;
    QPushButton* publishers_btn_ = nullptr;
    QPushButton* datasets_btn_   = nullptr;
    QPushButton* search_btn_     = nullptr;
    QLineEdit*   search_input_   = nullptr;
    QPushButton* fetch_btn_      = nullptr;
    QPushButton* export_btn_     = nullptr;

    // Breadcrumb
    QWidget* breadcrumb_       = nullptr;
    QLabel*  breadcrumb_label_ = nullptr;
    QLabel*  row_count_label_  = nullptr;

    // Content
    QStackedWidget* content_stack_    = nullptr;
    QTableWidget*   publishers_table_ = nullptr;
    QTableWidget*   datasets_table_   = nullptr;
    QTableWidget*   resources_table_  = nullptr;
    QLabel*         status_label_     = nullptr;

    // Status bar watermark label
    QLabel* watermark_label_ = nullptr;

    // Navigation state
    QString    selected_publisher_;
    QString    selected_publisher_name_;
    QString    selected_dataset_;
    QJsonArray current_publishers_;
    QJsonArray current_datasets_;
    QJsonArray current_resources_;
};

} // namespace fincept::screens
