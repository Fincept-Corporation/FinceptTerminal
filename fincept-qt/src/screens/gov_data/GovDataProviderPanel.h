// src/screens/gov_data/GovDataProviderPanel.h
//
// Reusable panel for CKAN-style government data portals.
// Supports hierarchical browsing: Publishers/Orgs → Datasets → Resources
// Plus search and recent views.
// Used by: Canada, Switzerland, Hong Kong, openAFRICA, Universal CKAN, Australia, etc.
#pragma once

#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QStackedWidget>
#include <QTableWidget>
#include <QVBoxLayout>
#include <QWidget>

#include "services/gov_data/GovDataService.h"

namespace fincept::screens {

class GovDataProviderPanel : public QWidget {
    Q_OBJECT
  public:
    /// @param script       Python script filename
    /// @param provider_color  Accent color hex string
    /// @param org_label    Label for top-level entities ("Publishers", "Organizations", "Categories")
    explicit GovDataProviderPanel(const QString& script,
                                   const QString& provider_color,
                                   const QString& org_label = "Publishers",
                                   QWidget* parent = nullptr);

  public slots:
    void load_initial_data();

  private slots:
    void on_result(const QString& request_id, const services::GovDataResult& result);
    void on_org_clicked(int row);
    void on_dataset_clicked(int row);
    void on_view_changed(int index);
    void on_search();
    void on_back();
    void on_export_csv();

  private:
    void build_ui();
    QWidget* build_toolbar();
    void show_loading(const QString& message);
    void show_error(const QString& message);
    void show_empty(const QString& message);
    void populate_orgs(const QJsonArray& data);
    void populate_datasets(const QJsonArray& data, int total_count);
    void populate_resources(const QJsonArray& data);

    QString script_;
    QString color_;
    QString org_label_;

    // View state
    enum View { Orgs = 0, Datasets, Resources, Search };
    View current_view_ = Orgs;

    // Toolbar
    QPushButton* back_btn_ = nullptr;
    QPushButton* orgs_btn_ = nullptr;
    QPushButton* datasets_btn_ = nullptr;
    QPushButton* search_btn_ = nullptr;
    QLineEdit* search_input_ = nullptr;
    QPushButton* fetch_btn_ = nullptr;
    QPushButton* export_btn_ = nullptr;

    // Content
    QStackedWidget* content_stack_ = nullptr;
    QTableWidget* orgs_table_ = nullptr;
    QTableWidget* datasets_table_ = nullptr;
    QTableWidget* resources_table_ = nullptr;
    QLabel* status_label_ = nullptr;

    void update_toolbar_state();

    // Navigation state
    QString selected_org_;
    QString selected_dataset_;
    QJsonArray current_orgs_;
    QJsonArray current_datasets_;
    QJsonArray current_resources_;
};

} // namespace fincept::screens
