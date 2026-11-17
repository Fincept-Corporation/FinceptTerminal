// src/screens/gov_data/GovDataUKPanel.h
// UK Government Data panel — data.gov.uk (CKAN)
// Views: Publishers → Datasets → Resources + Search + Recent + Popular
#pragma once

#include "services/gov_data/GovDataService.h"
#include "ui/widgets/PaginationBar.h"

#include <QHash>
#include <QJsonArray>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QStackedWidget>
#include <QTableWidget>
#include <QWidget>

#include <atomic>
#include <memory>

namespace fincept::ui {
class AutoLoadPane;
class EmptyState;
class InputMemory;
} // namespace fincept::ui

namespace fincept::screens {

class GovDataUKPanel : public QWidget {
    Q_OBJECT
  public:
    explicit GovDataUKPanel(QWidget* parent = nullptr);

  public slots:
    /// Called by the screen when the panel is first opened; the pane loads the publishers on show.
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
    /// "publishers", "popular", "recent", "pub:<id>", "search:<query>", "res:<dataset id>".
    enum class Kind { Publishers, Popular, Recent, Publisher, Search, Resources };

    // UI construction
    void build_ui();
    QWidget* build_toolbar();
    void build_pane(QWidget* parent);
    void retranslateUi();

    static Kind kind_of(const QString& key);
    static QString arg_of(const QString& key);
    void go(const QString& key, bool user, bool force = false);
    void go_datasets_tab();
    void start_run(quint64 run);
    /// The payload kind of a view ("publishers", "datasets", "resources").
    static QString payload_kind(Kind kind);
    /// The payload of a view of kind `kind` from a script answer (data.gov.uk lists ~1,800
    /// publishers). Pure: runs on the shaping thread.
    static QJsonObject shape(Kind kind, const QJsonObject& data);
    void render(const QJsonValue& payload);
    void show_no_data(Kind kind, const QString& arg);

    // Table population (the pagers page through the rows held here)
    void populate_publishers(const QJsonArray& data);
    void populate_datasets(const QJsonArray& data, int total_count);
    void populate_resources(const QJsonArray& data);
    void render_publishers_page();
    void render_datasets_page();
    void render_resources_page();

    void update_toolbar_state();
    void update_breadcrumb();
    void recolor_items();

    /// The page on screen, the dataset list and their names (InputMemory values "view", "list",
    /// "labels"); restore_navigation() returns the remembered page ("" = publishers).
    QString restore_navigation();
    void remember_navigation();
    static bool valid_view(const QString& key);

    QString last_list_view_;         // the dataset list a resources page was opened from
    QHash<QString, QString> labels_; // view key -> publisher / dataset name

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
    QLabel* as_of_label_ = nullptr;

    // ── Content: the pane; inside it the three paged tables and the no-data page ──
    ui::AutoLoadPane* pane_ = nullptr;
    QStackedWidget* content_stack_ = nullptr;
    QWidget* publishers_page_ = nullptr;
    QWidget* datasets_page_ = nullptr;
    QWidget* resources_page_ = nullptr;
    QTableWidget* publishers_table_ = nullptr;
    QTableWidget* datasets_table_ = nullptr;
    QTableWidget* resources_table_ = nullptr;
    ui::EmptyState* no_data_page_ = nullptr;

    QJsonArray current_publishers_;
    QJsonArray current_datasets_;
    QJsonArray current_resources_;

    ui::PaginationBar* pub_pager_ = nullptr;
    ui::PaginationBar* ds_pager_ = nullptr;
    ui::PaginationBar* res_pager_ = nullptr;

    ui::InputMemory* memory_ = nullptr;
    QHash<quint64, std::shared_ptr<std::atomic<bool>>> tokens_;
};

} // namespace fincept::screens
