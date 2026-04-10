// src/screens/gov_data/GovDataProviderPanel.h
// Reusable panel for CKAN-style government data portals.
// Supports hierarchical browsing: Publishers/Orgs → Datasets → Resources + Search
#pragma once

#include "services/gov_data/GovDataService.h"

#include <QComboBox>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QStackedWidget>
#include <QTableWidget>
#include <QTimer>
#include <QWidget>

namespace fincept::screens {

struct GovProviderOptions {
    QString watermark_text;                // if non-empty, show a bottom status bar with this text
    QStringList portal_combo_items;        // if non-empty, show a portal selector bar at top
    QString portal_combo_tooltip;          // tooltip for the portal combobox
};

class GovDataProviderPanel : public QWidget {
    Q_OBJECT
  public:
    explicit GovDataProviderPanel(const QString& script, const QString& provider_color,
                                  const QString& org_label = "Publishers",
                                  const GovProviderOptions& options = {},
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
    void show_empty(const QString& message);
    void show_error(const QString& message);
    void populate_orgs(const QJsonArray& data);
    void populate_datasets(const QJsonArray& data, int total_count);
    void populate_resources(const QJsonArray& data);
    void update_toolbar_state();
    void update_breadcrumb();

    QString script_;
    QString color_;
    QString org_label_;
    GovProviderOptions options_;

    enum View { Orgs = 0, Datasets, Resources, Search };
    View current_view_ = Orgs;

    // Toolbar
    QPushButton* back_btn_ = nullptr;
    QPushButton* orgs_btn_ = nullptr;
    QPushButton* datasets_btn_ = nullptr;
    QLineEdit* search_input_ = nullptr;
    QPushButton* fetch_btn_ = nullptr;
    QPushButton* export_btn_ = nullptr;

    // Breadcrumb
    QWidget* breadcrumb_ = nullptr;
    QHBoxLayout* breadcrumb_layout_ = nullptr;  // rebuilt on every navigation
    QLabel* row_count_label_ = nullptr;

    // Content
    QStackedWidget* content_stack_ = nullptr;
    QTableWidget* orgs_table_ = nullptr;
    QTableWidget* datasets_table_ = nullptr;
    QTableWidget* resources_table_ = nullptr;
    QLabel* status_label_ = nullptr;

    // Loading animation
    QTimer* loading_timer_ = nullptr;
    QString loading_base_msg_;
    int loading_dots_ = 0;

    // Navigation state
    QString selected_org_;       // API id used for requests
    QString selected_org_name_;  // display name used in breadcrumb
    QString selected_dataset_;
    QJsonArray current_orgs_;
    QJsonArray current_datasets_;
    QJsonArray current_resources_;
};

// Shared utilities — available to all gov_data panels
void configure_table(QTableWidget* table);
void export_table_to_csv(QTableWidget* table, const QString& default_name, QWidget* parent);

// Shared panel stylesheet builder.
// Pass the provider accent color (hex string, e.g. "#2563EB").
// Optional extra_qss is appended verbatim — use it for panel-specific widgets
// (QSpinBox, QComboBox, QTextBrowser, etc.) that are not in the shared set.
QString make_gov_panel_style(const QString& provider_color, const QString& extra_qss = {});

} // namespace fincept::screens
