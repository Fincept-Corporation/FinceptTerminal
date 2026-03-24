// src/screens/gov_data/GovDataTreasuryPanel.h
// Specialized panel for US Treasury data: Prices, Auctions, Summary
#pragma once

#include "services/gov_data/GovDataService.h"

#include <QComboBox>
#include <QDateEdit>
#include <QLabel>
#include <QPushButton>
#include <QStackedWidget>
#include <QTableWidget>
#include <QVBoxLayout>
#include <QWidget>

namespace fincept::screens {

class GovDataTreasuryPanel : public QWidget {
    Q_OBJECT
  public:
    explicit GovDataTreasuryPanel(QWidget* parent = nullptr);

  public slots:
    void load_initial_data();

  private slots:
    void on_result(const QString& request_id, const services::GovDataResult& result);
    void on_endpoint_changed(int index);
    void on_fetch();
    void on_export_csv();

  private:
    void build_ui();
    QWidget* build_toolbar();
    void show_loading(const QString& message);
    void show_error(const QString& message);
    void populate_prices(const QJsonObject& data);
    void populate_auctions(const QJsonObject& data);
    void populate_summary(const QJsonObject& data);

    enum Endpoint { Prices = 0, Auctions, Summary };
    Endpoint active_endpoint_ = Prices;

    // Toolbar
    QPushButton* prices_btn_ = nullptr;
    QPushButton* auctions_btn_ = nullptr;
    QPushButton* summary_btn_ = nullptr;
    QDateEdit* start_date_ = nullptr;
    QDateEdit* end_date_ = nullptr;
    QComboBox* security_type_ = nullptr;
    QPushButton* fetch_btn_ = nullptr;
    QPushButton* export_btn_ = nullptr;

    // Content
    QStackedWidget* content_stack_ = nullptr;
    QTableWidget* prices_table_ = nullptr;
    QTableWidget* auctions_table_ = nullptr;
    QWidget* summary_widget_ = nullptr;
    QLabel* status_label_ = nullptr;

    // Summary labels
    QLabel* total_securities_label_ = nullptr;
    QLabel* min_rate_label_ = nullptr;
    QLabel* max_rate_label_ = nullptr;
    QLabel* avg_rate_label_ = nullptr;
    QLabel* min_price_label_ = nullptr;
    QLabel* max_price_label_ = nullptr;
    QLabel* avg_price_label_ = nullptr;
    QTableWidget* type_breakdown_table_ = nullptr;
};

} // namespace fincept::screens
