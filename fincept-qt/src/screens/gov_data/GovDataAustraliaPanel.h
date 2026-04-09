// src/screens/gov_data/GovDataAustraliaPanel.h
// Australia Government Data panel — data.gov.au (CKAN)
// Views: Agencies → Datasets → Resources + Recent + Search
// NOTE: org-datasets and search return data.datasets[] (nested object).
//       Resources are embedded inside each dataset — no extra fetch required.
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

class GovDataAustraliaPanel : public QWidget {
    Q_OBJECT
  public:
    explicit GovDataAustraliaPanel(QWidget* parent = nullptr);

  public slots:
    void load_initial_data();

  private slots:
    void on_result(const QString& request_id, const services::GovDataResult& result);
    void on_agency_doubleclicked(int row, int col);
    void on_dataset_doubleclicked(int row, int col);
    void on_fetch();
    void on_search();
    void on_recent();
    void on_back();
    void on_export_csv();

  private:
    // UI construction
    void build_ui();
    QWidget* build_toolbar();

    // Table population
    void populate_agencies(const QJsonArray& data);
    void populate_datasets(const QJsonArray& data, int total_count);
    void populate_resources(const QJsonArray& resources);

    // Unwrap org-datasets / search response: {data:{datasets:[...], count:N}}
    static QJsonArray unwrap_datasets(const services::GovDataResult& result, int& out_total);

    // Navigation helpers
    void show_loading(const QString& message);
    void show_error(const QString& message);
    void update_toolbar_state();
    void update_breadcrumb();

    // CSV export helper
    void export_table_csv(QTableWidget* table, const QString& default_name);

    // ── Constants ────────────────────────────────────────────────────────────
    static constexpr const char* kScript = "datagov_au_api.py";
    static constexpr const char* kColor = "#0EA5E9";

    // Enum order matches content_stack_ page indices
    enum View { Agencies = 0, Datasets, Resources, Status };
    View current_view_ = Agencies;

    // Indicates whether Datasets view is showing "recent" results
    bool showing_recent_ = false;

    // ── Toolbar widgets ───────────────────────────────────────────────────────
    QPushButton* back_btn_ = nullptr;
    QPushButton* agencies_btn_ = nullptr;
    QPushButton* datasets_btn_ = nullptr;
    QPushButton* recent_btn_ = nullptr;
    QLineEdit* search_input_ = nullptr;
    QPushButton* fetch_btn_ = nullptr;
    QPushButton* export_btn_ = nullptr;

    // ── Breadcrumb bar ────────────────────────────────────────────────────────
    QWidget* breadcrumb_ = nullptr;
    QLabel* breadcrumb_label_ = nullptr;
    QLabel* row_count_label_ = nullptr;

    // ── Content stack (index matches View enum) ───────────────────────────────
    QStackedWidget* content_stack_ = nullptr;
    QTableWidget* agencies_table_ = nullptr;
    QTableWidget* datasets_table_ = nullptr;
    QTableWidget* resources_table_ = nullptr;
    QLabel* status_label_ = nullptr;

    // ── Navigation state ──────────────────────────────────────────────────────
    QString selected_agency_;    // display name for breadcrumb
    QString selected_agency_id_; // id/name used in API call
    QString selected_dataset_title_;

    QJsonArray current_agencies_;
    QJsonArray current_datasets_; // stored so resources can be extracted without re-fetch
};

} // namespace fincept::screens
