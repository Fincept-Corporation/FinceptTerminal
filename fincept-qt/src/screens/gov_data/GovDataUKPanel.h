// src/screens/gov_data/GovDataUKPanel.h
// UK Government Data panel — data.gov.uk (CKAN)
// Views: Publishers → Datasets → Resources + Search
#pragma once

#include "services/gov_data/GovDataService.h"
#include "ui/widgets/PaginationBar.h"

#include <QJsonArray>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QStackedWidget>
#include <QTableWidget>
#include <QWidget>

namespace fincept::screens {

class GovDataUKPanel : public QWidget {
    Q_OBJECT
  public:
    explicit GovDataUKPanel(QWidget* parent = nullptr);

  public slots:
    void load_initial_data();

  protected:
    void changeEvent(QEvent* event) override;

  private slots:
    void on_result(const QString& request_id, const services::GovDataResult& result);
    void on_publisher_doubleclicked(int row, int col);
    void on_dataset_doubleclicked(int row, int col);
    void on_fetch();
    void on_search();
    void on_back();
    void on_popular();
    void on_export_csv();

  private:
    // UI construction
    void build_ui();
    QWidget* build_toolbar();
    void retranslateUi();

    // Table population
    void populate_publishers(const QJsonArray& data);
    void populate_datasets(const QJsonArray& data, int total_count);
    void populate_resources(const QJsonArray& data);

    // Navigation helpers
    void show_loading(const QString& message);
    void show_error(const QString& message);
    void update_toolbar_state();
    void update_breadcrumb();

    // ── Constants ────────────────────────────────────────────────────────────
    static constexpr const char* kScript = "datagovuk_api.py";
    static constexpr const char* kColor = "#10B981";

    enum View { Publishers = 0, Datasets, Resources, Status };
    View current_view_ = Publishers;

    // ── Toolbar widgets ───────────────────────────────────────────────────────
    QPushButton* back_btn_ = nullptr;
    QPushButton* publishers_btn_ = nullptr;
    QPushButton* datasets_btn_ = nullptr;
    QPushButton* popular_btn_ = nullptr;
    QLineEdit* search_input_ = nullptr;
    QPushButton* fetch_btn_ = nullptr;
    QPushButton* export_btn_ = nullptr;

    // ── Breadcrumb bar ────────────────────────────────────────────────────────
    QWidget* breadcrumb_ = nullptr;
    QLabel* breadcrumb_label_ = nullptr;
    QLabel* row_count_label_ = nullptr;

    // ── Content stack (index matches View enum) ───────────────────────────────
    QStackedWidget* content_stack_ = nullptr;
    QTableWidget* publishers_table_ = nullptr;
    QTableWidget* datasets_table_ = nullptr;
    QTableWidget* resources_table_ = nullptr;
    QLabel* status_label_ = nullptr;

    // ── Navigation state ──────────────────────────────────────────────────────
    QString selected_publisher_;    // display name, for breadcrumb
    QString selected_publisher_id_; // id used in API call
    QString selected_dataset_id_;

    QJsonArray current_publishers_;
    QJsonArray current_datasets_;
    QJsonArray current_resources_;

    ui::PaginationBar* pub_pager_ = nullptr;
    ui::PaginationBar* ds_pager_ = nullptr;
    ui::PaginationBar* res_pager_ = nullptr;

    void render_publishers_page();
    void render_datasets_page();
    void render_resources_page();
};

} // namespace fincept::screens
