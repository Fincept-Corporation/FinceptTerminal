// src/screens/gov_data/GovDataHKPanel.h
// Hong Kong data.gov.hk panel — Categories → Datasets → Resources
// Script: data_gov_hk_api.py  Color: #F43F5E
// Note: HK has no text search — FETCH on search input runs datasets_list
//       and filters client-side by title match.
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

class GovDataHKPanel : public QWidget {
    Q_OBJECT
  public:
    explicit GovDataHKPanel(QWidget* parent = nullptr);

  public slots:
    void load_initial_data();

  private slots:
    void on_result(const QString& request_id, const services::GovDataResult& result);
    void on_category_clicked(int row);
    void on_dataset_clicked(int row);
    void on_tab_changed(int tab_index);
    void on_fetch();
    void on_back();
    void on_export_csv();

  private:
    enum View { Categories = 0, Datasets, Resources, Status };

    void build_ui();
    QWidget* build_toolbar();

    void populate_categories(const QJsonArray& data);
    void populate_datasets(const QJsonArray& data);
    void populate_resources(const QJsonArray& data);

    // Client-side filter of datasets_list by title substring
    void filter_datasets_list(const QString& query);

    void show_loading(const QString& message);
    void show_error(const QString& message);
    void show_status(const QString& message, bool is_error = false);

    void update_toolbar_state();
    void update_breadcrumb(const QString& text);

    static constexpr const char* kScript = "data_gov_hk_api.py";
    static constexpr const char* kColor = "#F43F5E";

    View current_view_ = Categories;
    QString selected_category_id_;
    QString selected_category_name_;
    QString selected_dataset_id_;

    // Full datasets_list fetched once (strings) for client-side search
    QJsonArray all_dataset_names_;

    // Toolbar widgets
    QPushButton* back_btn_ = nullptr;
    QPushButton* categories_btn_ = nullptr;
    QPushButton* datasets_btn_ = nullptr;
    QLineEdit* search_input_ = nullptr;
    QPushButton* fetch_btn_ = nullptr;
    QPushButton* export_btn_ = nullptr;

    // Breadcrumb
    QWidget* breadcrumb_ = nullptr;
    QLabel* breadcrumb_label_ = nullptr;
    QLabel* row_count_label_ = nullptr;

    // Content pages
    QStackedWidget* content_stack_ = nullptr;
    QTableWidget* categories_table_ = nullptr; // page 0
    QTableWidget* datasets_table_ = nullptr;   // page 1
    QTableWidget* resources_table_ = nullptr;  // page 2
    QLabel* status_label_ = nullptr;           // page 3

    // Cached for CSV
    QJsonArray current_categories_;
    QJsonArray current_datasets_;
    QJsonArray current_resources_;
};

} // namespace fincept::screens
