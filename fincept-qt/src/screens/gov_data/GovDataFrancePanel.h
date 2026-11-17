// src/screens/gov_data/GovDataFrancePanel.h
// France data.gouv.fr panel — tab-based: Data Services | Datasets | Geo Search
// Script: french_gov_api.py  Color: #2563EB
#pragma once

#include "services/gov_data/GovDataService.h"

#include <QHash>
#include <QJsonArray>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QStackedWidget>
#include <QTableView>
#include <QWidget>

#include <atomic>
#include <memory>

namespace fincept::ui {
class AutoLoadPane;
class EmptyState;
class InputMemory;
} // namespace fincept::ui

namespace fincept::screens {

class GovTableModel;

class GovDataFrancePanel : public QWidget {
    Q_OBJECT
  public:
    explicit GovDataFrancePanel(QWidget* parent = nullptr);

  public slots:
    /// Called by the screen when the panel is first opened; the pane loads the tab on show.
    void load_initial_data();

  protected:
    void changeEvent(QEvent* event) override;

  private slots:
    void on_result(const QString& request_id, const services::GovDataResult& result);
    void on_dataset_row_double_clicked(int row);
    void on_tab_changed(int tab_index);
    void on_fetch();
    void on_back();
    void on_export_csv();

  private:
    // Tabs; Resources is the drill-down from a dataset (its column schema)
    enum View { Services = 0, Datasets, Geo, Resources };

    void build_ui();
    QWidget* build_toolbar();
    void build_pane(QWidget* parent);
    void retranslateUi();

    /// "services", "datasets:<term>" (empty term = the catalogue's list), "geo:<term>", "schema:<id>".
    static QString view_key(View v, const QString& arg = {});
    static View view_of(const QString& key);
    static QString arg_of(const QString& key);
    void go(const QString& key, bool user, bool force = false);
    /// The search box shows the term of `v` (remembered per tab; Geo starts on "Paris").
    void load_term(View v);
    void start_run(quint64 run);
    /// The payload kind of a view ("services", "datasets", "geo", "schema").
    static QString payload_kind(View v);
    /// The payload of view `v` from a script answer. Pure: runs on the shaping thread.
    static QJsonObject shape(View v, const QJsonObject& data);
    void render(const QJsonValue& payload);
    void show_no_data(View v, const QString& arg);

    void populate_services(const QJsonArray& data);
    void populate_datasets(const QJsonArray& data);
    void populate_geo(const QJsonArray& data);
    void populate_resources(const QJsonArray& data);

    void update_toolbar_state();
    void update_breadcrumb();

    View current_view_ = Services;
    QString last_datasets_view_; // the dataset list a column schema was opened from
    QHash<QString, QString> labels_;

    // Toolbar widgets
    QPushButton* back_btn_ = nullptr;
    QPushButton* services_btn_ = nullptr;
    QPushButton* datasets_btn_ = nullptr;
    QPushButton* geo_btn_ = nullptr;
    QLineEdit* search_input_ = nullptr;
    QPushButton* fetch_btn_ = nullptr;
    QPushButton* export_btn_ = nullptr;

    // Breadcrumb
    QWidget* breadcrumb_ = nullptr;
    QLabel* breadcrumb_label_ = nullptr;
    QLabel* row_count_label_ = nullptr;
    QLabel* as_of_label_ = nullptr;

    // Content pages
    ui::AutoLoadPane* pane_ = nullptr;
    QStackedWidget* content_stack_ = nullptr;
    QTableView* services_table_ = nullptr;
    QTableView* datasets_table_ = nullptr;
    QTableView* geo_table_ = nullptr;
    QTableView* resources_table_ = nullptr; // column schema
    GovTableModel* services_model_ = nullptr;
    GovTableModel* datasets_model_ = nullptr;
    GovTableModel* geo_model_ = nullptr;
    GovTableModel* resources_model_ = nullptr;
    ui::EmptyState* no_data_page_ = nullptr;

    ui::InputMemory* tabs_memory_ = nullptr; // the tab
    ui::InputMemory* term_memory_ = nullptr; // the search term, per tab
    QHash<quint64, std::shared_ptr<std::atomic<bool>>> tokens_;
};

} // namespace fincept::screens
