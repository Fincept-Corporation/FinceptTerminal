// src/screens/dbnomics/DBnomicsDataTable.h
#pragma once
#include "services/dbnomics/DBnomicsModels.h"

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

  private:
    void build_ui();

    QStackedWidget* stack_ = nullptr;
    QLabel* spin_label_ = nullptr;
    QTimer* spin_timer_ = nullptr;
    QTableWidget* table_ = nullptr;
    int frame_ = 0;
};

} // namespace fincept::screens
