// src/screens/gov_data/GovDataCongressPanel.h
// Specialized panel for US Congress bills and legislative data
#pragma once

#include <QComboBox>
#include <QLabel>
#include <QPushButton>
#include <QSpinBox>
#include <QStackedWidget>
#include <QTableWidget>
#include <QTextBrowser>
#include <QVBoxLayout>
#include <QWidget>

#include "services/gov_data/GovDataService.h"

namespace fincept::screens {

class GovDataCongressPanel : public QWidget {
    Q_OBJECT
  public:
    explicit GovDataCongressPanel(QWidget* parent = nullptr);

  public slots:
    void load_initial_data();

  private slots:
    void on_result(const QString& request_id, const services::GovDataResult& result);
    void on_view_changed(int index);
    void on_fetch_bills();
    void on_bill_clicked(int row);
    void on_back_to_list();
    void on_export_csv();

  private:
    void build_ui();
    QWidget* build_toolbar();
    void show_loading(const QString& message);
    void show_error(const QString& message);
    void populate_bills(const QJsonObject& data);
    void populate_bill_detail(const QJsonObject& data);
    void populate_summary(const QJsonObject& data);

    enum View { Bills = 0, BillDetail, SummaryView };
    View current_view_ = Bills;

    // Toolbar
    QPushButton* bills_btn_ = nullptr;
    QPushButton* summary_btn_ = nullptr;
    QPushButton* back_btn_ = nullptr;
    QComboBox* bill_type_combo_ = nullptr;
    QSpinBox* congress_num_ = nullptr;
    QPushButton* fetch_btn_ = nullptr;
    QPushButton* export_btn_ = nullptr;

    // Content
    QStackedWidget* content_stack_ = nullptr;
    QTableWidget* bills_table_ = nullptr;
    QTextBrowser* detail_browser_ = nullptr;
    QWidget* summary_widget_ = nullptr;
    QLabel* status_label_ = nullptr;

    // Summary sub-widgets
    QTableWidget* summary_table_ = nullptr;

    // Pagination
    int current_offset_ = 0;
    int page_size_ = 50;
};

} // namespace fincept::screens
