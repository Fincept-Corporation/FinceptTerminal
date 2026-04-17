#pragma once
#include <QStringList>
#include <QTableWidget>
#include <QVector>

namespace fincept::ui {

/// Reusable data table with alternating rows.
class DataTable : public QTableWidget {
    Q_OBJECT
  public:
    explicit DataTable(QWidget* parent = nullptr);

    void set_headers(const QStringList& headers);
    void set_data(const QVector<QStringList>& rows);
    void add_row(const QStringList& row);
    void clear_data();
    void set_column_widths(const QVector<int>& widths);

    // Color a specific cell
    void set_cell_color(int row, int col, const QString& color);
};

} // namespace fincept::ui
