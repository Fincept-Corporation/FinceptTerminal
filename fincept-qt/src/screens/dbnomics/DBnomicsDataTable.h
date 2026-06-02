// src/screens/dbnomics/DBnomicsDataTable.h
#pragma once
#include "services/dbnomics/DBnomicsModels.h"

#include <QEvent>
#include <QLabel>
#include <QStackedWidget>
#include <QTableWidget>
#include <QTimer>
#include <QWidget>

namespace fincept::screens {

class DBnomicsDataTable : public QWidget {
    Q_OBJECT
  public:
    explicit DBnomicsDataTable(QWidget* parent = nullptr);

    void set_data(const QVector<services::DbnDataPoint>& series);
    void clear();
    void set_loading(bool on);

  protected:
    void changeEvent(QEvent* event) override;

  private:
    void build_ui();
    void retranslateUi();

    QStackedWidget* stack_ = nullptr;
    QLabel* header_label_ = nullptr; // (cached for retranslateUi)
    QLabel* spin_label_ = nullptr;
    QTimer* spin_timer_ = nullptr;
    QTableWidget* table_ = nullptr;
    int frame_ = 0;
    bool showing_placeholder_ = false; // table currently shows the empty-state cell
};

} // namespace fincept::screens
