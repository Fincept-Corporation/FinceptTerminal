#include "ui/tables/DataTable.h"

#include "ui/theme/Theme.h"

#include <QHeaderView>

namespace fincept::ui {

DataTable::DataTable(QWidget* parent) : QTableWidget(parent) {
    setAlternatingRowColors(true);
    setSelectionBehavior(QAbstractItemView::SelectRows);
    setSelectionMode(QAbstractItemView::SingleSelection);
    setEditTriggers(QAbstractItemView::NoEditTriggers);
    setShowGrid(false);
    verticalHeader()->setVisible(false);
    horizontalHeader()->setStretchLastSection(true);
    horizontalHeader()->setDefaultAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    setStyleSheet(QString("QTableWidget { background: %1; alternate-background-color: %2; "
                          "gridline-color: %3; border: none; }"
                          "QTableWidget::item { padding: 4px 8px; height: 26px; }"
                          "QTableWidget::item:selected { background: #111111; }")
                      .arg(colors::DARK(), colors::ROW_ALT(), colors::BORDER()));
}

void DataTable::set_headers(const QStringList& headers) {
    setColumnCount(headers.size());
    setHorizontalHeaderLabels(headers);
}

void DataTable::set_data(const QVector<QStringList>& rows) {
    setRowCount(0);
    for (const auto& row : rows) {
        add_row(row);
    }
}

void DataTable::add_row(const QStringList& row) {
    int r = rowCount();
    insertRow(r);
    for (int c = 0; c < row.size() && c < columnCount(); ++c) {
        auto* item = new QTableWidgetItem(row[c]);
        item->setForeground(QColor(colors::WHITE()));
        setItem(r, c, item);
    }
    setRowHeight(r, 26);
}

void DataTable::clear_data() {
    setRowCount(0);
}

void DataTable::set_column_widths(const QVector<int>& widths) {
    for (int i = 0; i < widths.size() && i < columnCount(); ++i) {
        setColumnWidth(i, widths[i]);
    }
}

void DataTable::set_cell_color(int row, int col, const QString& color) {
    auto* it = item(row, col);
    if (it)
        it->setForeground(QColor(color));
}

} // namespace fincept::ui
