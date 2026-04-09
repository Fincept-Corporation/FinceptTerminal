// src/screens/gov_data/GovDataCKANPanel.h
// Universal CKAN portal browser — covers 8 portals, script: datagovuk_api.py
// Color: #10B981
// Includes a display-only portal selector combobox at the top.
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

class GovDataCKANPanel : public QWidget {
    Q_OBJECT
  public:
    explicit GovDataCKANPanel(QWidget* parent = nullptr);

  public slots:
    void load_initial_data();

  private slots:
    void on_result(const QString& request_id, const services::GovDataResult& result);
    void on_publisher_clicked(int row);
    void on_dataset_clicked(int row);
    void on_tab_changed(int tab_index);
    void on_search();
    void on_back();
    void on_export_csv();

  private:
    enum View { Publishers = 0, Datasets, Resources, Search };

    void build_ui();
    QWidget* build_portal_bar();
    QWidget* build_toolbar();

    void populate_publishers(const QJsonArray& data);
    void populate_datasets(const QJsonArray& data, int total_count);
    void populate_resources(const QJsonArray& data);

    void show_loading(const QString& message);
    void show_error(const QString& message);
    void show_status(const QString& message, bool is_error = false);

    void update_toolbar_state();
    void update_breadcrumb();

    static constexpr const char* kScript = "datagovuk_api.py";
    static constexpr const char* kColor = "#10B981";

    View current_view_ = Publishers;
    QString selected_publisher_;
    QString selected_publisher_name_;
    QString selected_dataset_;

    // Toolbar widgets
    QPushButton* back_btn_ = nullptr;
    QPushButton* publishers_btn_ = nullptr;
    QPushButton* datasets_btn_ = nullptr;
    QPushButton* search_btn_ = nullptr;
    QLineEdit* search_input_ = nullptr;
    QPushButton* fetch_btn_ = nullptr;
    QPushButton* export_btn_ = nullptr;

    // Portal selector (display-only)
    QComboBox* portal_combo_ = nullptr;

    // Breadcrumb
    QWidget* breadcrumb_ = nullptr;
    QLabel* breadcrumb_label_ = nullptr;
    QLabel* row_count_label_ = nullptr;

    // Content pages (stack indices match View enum + 1 for status)
    QStackedWidget* content_stack_ = nullptr;
    QTableWidget* publishers_table_ = nullptr; // page 0
    QTableWidget* datasets_table_ = nullptr;   // page 1
    QTableWidget* resources_table_ = nullptr;  // page 2
    QLabel* status_label_ = nullptr;           // page 3

    // Cached data
    QJsonArray current_publishers_;
    QJsonArray current_datasets_;
    QJsonArray current_resources_;
};

} // namespace fincept::screens
