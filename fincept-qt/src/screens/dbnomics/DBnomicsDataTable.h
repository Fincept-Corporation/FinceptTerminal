// src/screens/dbnomics/DBnomicsDataTable.h
#pragma once
#include "services/dbnomics/DBnomicsModels.h"
#include <QWidget>
#include <QTableWidget>

namespace fincept::screens {

class DBnomicsDataTable : public QWidget {
    Q_OBJECT
public:
    explicit DBnomicsDataTable(QWidget* parent = nullptr);

    // Replace the entire table with a new set of series
    void set_data(const QVector<services::DbnDataPoint>& series);
    void clear();

private:
    void build_ui();

    QTableWidget* table_ = nullptr;
};

} // namespace fincept::screens
