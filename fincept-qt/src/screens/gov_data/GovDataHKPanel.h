// src/screens/gov_data/GovDataHKPanel.h
// Hong Kong data.gov.hk panel — Categories → Datasets → Resources
// Script: data_gov_hk_api.py  Color: #F43F5E
// Note: HK has no text search — a filter term is matched against the full
//       dataset name list (datasets_list, fetched once per session).
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
#include <functional>
#include <memory>

namespace fincept::ui {
class AutoLoadPane;
class EmptyState;
class InputMemory;
} // namespace fincept::ui

namespace fincept::screens {

class GovTableModel;

class GovDataHKPanel : public QWidget {
    Q_OBJECT
  public:
    explicit GovDataHKPanel(QWidget* parent = nullptr);

  public slots:
    /// Called by the screen when the panel is first opened; the pane loads the categories on show.
    void load_initial_data();

  protected:
    void changeEvent(QEvent* event) override;

  private slots:
    void on_result(const QString& request_id, const services::GovDataResult& result);
    void on_category_clicked(int row);
    void on_dataset_clicked(int row);
    void on_tab_changed(int tab_index);
    void on_fetch();
    void on_back();
    void on_export_csv();

  private:
    /// "categories", "cat:<id>", "all" (the full name list), "filter:<term>", "res:<dataset id>".
    enum class Kind { Categories, Category, All, Filter, Resources };

    void build_ui();
    QWidget* build_toolbar();
    void build_pane(QWidget* parent);
    void retranslateUi();

    static Kind kind_of(const QString& key);
    static QString arg_of(const QString& key);
    void go(const QString& key, bool user, bool force = false);
    void start_run(quint64 run);
    /// Datasets of `names` whose name contains `term` (case-insensitive), as dataset rows. Pure:
    /// runs on the shaping thread (the name list holds every data.gov.hk dataset).
    static QJsonArray filter_names(const QJsonArray& names, const QString& term);
    /// The payload of view `key` from a script answer. Pure: runs on the shaping thread.
    static QJsonObject shape(const QString& key, const QJsonObject& data);
    /// Hands `run` the payload `shape` builds on the shaping thread (dropped when superseded).
    void deliver_shaped(quint64 run, std::function<QJsonObject()> shaper);
    void render(const QJsonValue& payload);
    void show_no_data(Kind kind, const QString& arg);

    void populate_categories(const QJsonArray& data);
    void populate_datasets(const QJsonArray& data);
    void populate_resources(const QJsonArray& data);

    void update_toolbar_state();
    void update_breadcrumb();

    /// The page on screen, the dataset list and their names (InputMemory values "view", "list",
    /// "labels"); restore_navigation() returns the remembered page ("" = categories).
    QString restore_navigation();
    void remember_navigation();
    static bool valid_view(const QString& key);

    QString last_list_view_;          // the dataset list a resources page was opened from
    QHash<QString, QString> labels_;  // view key -> category / dataset name
    QJsonArray categories_;           // last categories shown (the DATASETS tab opens the first)
    QJsonArray all_dataset_names_;    // full name list for the filter, fetched once
    QHash<quint64, std::shared_ptr<std::atomic<bool>>> tokens_;

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
    QLabel* as_of_label_ = nullptr;

    // Content pages
    ui::AutoLoadPane* pane_ = nullptr;
    QStackedWidget* content_stack_ = nullptr;
    QTableView* categories_table_ = nullptr;
    QTableView* datasets_table_ = nullptr;
    QTableView* resources_table_ = nullptr;
    GovTableModel* categories_model_ = nullptr;
    GovTableModel* datasets_model_ = nullptr;
    GovTableModel* resources_model_ = nullptr;
    ui::EmptyState* no_data_page_ = nullptr;
    ui::InputMemory* memory_ = nullptr;
};

} // namespace fincept::screens
