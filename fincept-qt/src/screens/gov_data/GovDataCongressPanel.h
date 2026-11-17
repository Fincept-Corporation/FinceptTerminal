// src/screens/gov_data/GovDataCongressPanel.h
// Specialized panel for US Congress bills and legislative data
#pragma once

#include "services/gov_data/GovDataService.h"

#include <QComboBox>
#include <QHash>
#include <QJsonObject>
#include <QLabel>
#include <QPushButton>
#include <QSpinBox>
#include <QStackedWidget>
#include <QStringList>
#include <QTableView>
#include <QTableWidget>
#include <QTextBrowser>
#include <QVBoxLayout>
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

class GovDataCongressPanel : public QWidget {
    Q_OBJECT
  public:
    explicit GovDataCongressPanel(QWidget* parent = nullptr);

  public slots:
    /// Called by the screen when the panel is first opened. Without a Congress.gov key the
    /// pane shows the needs-setup page; with one it loads the bills of the sitting Congress.
    void load_initial_data();

  protected:
    void changeEvent(QEvent* event) override;

  private slots:
    void on_result(const QString& request_id, const services::GovDataResult& result);
    void on_view_changed(int index);
    void on_fetch_bills();
    void on_bill_clicked(int row);
    void on_back_to_list();
    void on_export_csv();

  private:
    enum View { Bills = 0, BillDetail, SummaryView };

    void build_ui();
    QWidget* build_toolbar();
    void build_pane(QWidget* parent);
    void retranslateUi();

    /// "bills|<congress>|<type>", "summary|<congress>", "bill|<congress>|<type>|<number>".
    QString view_key(View v) const;
    static View view_of(const QString& key);
    void go(const QString& key, bool user, bool force = false);
    void start_run(quint64 run);
    /// The payload of view `view` from a script answer; `labels` = the translated "Type:",
    /// "Congress:", "Status:" of a bill detail built from bill_data. Pure: runs on the shaping thread.
    static QJsonObject shape(View view, const QJsonObject& data, const QStringList& labels);
    void render(const QJsonValue& payload);

    void populate_bills(const QJsonArray& bills);
    void populate_summary(const QJsonObject& types);
    /// Wraps detail_body_ in the active theme's colours and shows it (also once the appearance settled).
    void render_bill_detail();
    /// Item brushes and the detail document are not reached by stylesheets: redo them.
    void refresh_theme();
    void update_toolbar_state();

    View current_view_ = Bills;
    QString last_list_view_; // the bills list the detail was opened from

    // Toolbar
    QPushButton* bills_btn_ = nullptr;
    QPushButton* summary_btn_ = nullptr;
    QPushButton* back_btn_ = nullptr;
    QComboBox* bill_type_combo_ = nullptr;
    QSpinBox* congress_num_ = nullptr;
    QPushButton* fetch_btn_ = nullptr;
    QPushButton* export_btn_ = nullptr;
    QLabel* congress_lbl_ = nullptr;
    QLabel* type_lbl_ = nullptr;
    QLabel* as_of_label_ = nullptr;
    QLabel* info_label_ = nullptr;

    // Content
    ui::AutoLoadPane* pane_ = nullptr;
    QStackedWidget* content_stack_ = nullptr;
    QTableView* bills_table_ = nullptr; // up to 250 bills: a model, cells formatted when shown
    GovTableModel* bills_model_ = nullptr;
    QTextBrowser* detail_browser_ = nullptr;
    QString detail_body_; // bill detail as HTML, heading colour left as the @ACCENT@ marker
    QTableWidget* summary_table_ = nullptr;
    ui::EmptyState* no_data_page_ = nullptr;

    ui::InputMemory* memory_ = nullptr;
    QHash<quint64, std::shared_ptr<std::atomic<bool>>> tokens_;
};

} // namespace fincept::screens
