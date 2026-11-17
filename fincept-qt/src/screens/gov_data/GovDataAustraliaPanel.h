// src/screens/gov_data/GovDataAustraliaPanel.h
// Australia Government Data panel — data.gov.au (CKAN)
// Views: Agencies → Datasets → Resources + Recent + Search
// NOTE: org-datasets and search return data.datasets[] (nested object).
//       Resources are embedded inside each dataset — no extra fetch required.
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

class GovDataAustraliaPanel : public QWidget {
    Q_OBJECT
  public:
    explicit GovDataAustraliaPanel(QWidget* parent = nullptr);

  public slots:
    /// Called by the screen when the panel is first opened; the pane loads the agencies on show.
    void load_initial_data();

  protected:
    void changeEvent(QEvent* event) override;

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
    /// "agencies", "recent", "agency:<id>", "search:<query>", "res:<dataset id>".
    enum class Kind { Agencies, Recent, Agency, Search, Resources };

    // UI construction
    void build_ui();
    QWidget* build_toolbar();
    void build_pane(QWidget* parent);
    void retranslateUi();

    static Kind kind_of(const QString& key);
    static QString arg_of(const QString& key);
    void go(const QString& key, bool user, bool force = false);
    void start_run(quint64 run);
    /// The payload of a view of kind `kind` from a script answer. Pure: runs on the shaping thread.
    static QJsonObject shape(Kind kind, const QJsonObject& data);
    void render(const QJsonValue& payload);
    void show_no_data(Kind kind, const QString& arg);

    // Table population
    void populate_agencies(const QJsonArray& data);
    void populate_datasets(const QJsonArray& data, int total_count);
    void populate_resources(const QJsonArray& resources);

    void update_toolbar_state();
    void update_breadcrumb();

    /// The page on screen, the dataset list and their names (InputMemory values "view", "list",
    /// "labels"); restore_navigation() returns the remembered page ("" = agencies).
    QString restore_navigation();
    void remember_navigation();
    static bool valid_view(const QString& key);

    QString last_list_view_;                      // the dataset list a resources page was opened from
    QHash<QString, QString> labels_;              // view key -> agency / dataset name
    QHash<QString, QJsonArray> dataset_resources_; // dataset id -> its embedded resources (compact)

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
    QLabel* as_of_label_ = nullptr;

    // ── Content ───────────────────────────────────────────────────────────────
    ui::AutoLoadPane* pane_ = nullptr;
    QStackedWidget* content_stack_ = nullptr;
    QTableView* agencies_table_ = nullptr;
    QTableView* datasets_table_ = nullptr;
    QTableView* resources_table_ = nullptr;
    GovTableModel* agencies_model_ = nullptr;
    GovTableModel* datasets_model_ = nullptr;
    GovTableModel* resources_model_ = nullptr;
    ui::EmptyState* no_data_page_ = nullptr;

    ui::InputMemory* memory_ = nullptr;
    QHash<quint64, std::shared_ptr<std::atomic<bool>>> tokens_;
};

} // namespace fincept::screens
