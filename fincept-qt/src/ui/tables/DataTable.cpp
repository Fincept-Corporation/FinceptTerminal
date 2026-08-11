#include "ui/tables/DataTable.h"

#include "ui/theme/Theme.h"

#include <QHeaderView>

namespace fincept::ui {

namespace {
/// Uniform row height, applied once on the vertical header in the constructor
/// rather than per row. Kept in sync with the `height: 26px` item rule in the
/// stylesheet below.
constexpr int kRowHeightPx = 26;
} // namespace

DataTable::DataTable(QWidget* parent) : QTableWidget(parent) {
    setAlternatingRowColors(true);
    setSelectionBehavior(QAbstractItemView::SelectRows);
    setSelectionMode(QAbstractItemView::SingleSelection);
    setEditTriggers(QAbstractItemView::NoEditTriggers);
    setShowGrid(false);
    verticalHeader()->setVisible(false);
    // Row height is uniform, so set it once here rather than calling
    // setRowHeight() per row (each call triggers a geometry recalculation).
    // setSectionResizeMode(Fixed) also lets the view skip per-row height
    // queries entirely when laying out.
    verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);
    verticalHeader()->setDefaultSectionSize(kRowHeightPx);
    horizontalHeader()->setStretchLastSection(true);
    horizontalHeader()->setDefaultAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    // NOTE: sorting is intentionally NOT enabled here.
    // Callers that need sorting (e.g. WatchlistScreen) opt in with
    // setSortingEnabled(true) after attaching numeric EditRole values.
    // Enabling it globally causes lexicographic sort on tables that only
    // use setText() — e.g. "$10" sorts before "$2", "100K" before "2M".
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
    // Size the model once, then fill. The previous form called add_row() per
    // row, and add_row() calls insertRow() — an O(n) model mutation that emits
    // rowsInserted every time, so the attached view ran a layout pass per row
    // and the whole fill was O(n²) in view work. This is the one shared table
    // abstraction in the codebase, so the pattern propagated by example.
    //
    // Bulk updates are also suppressed and re-enabled around the fill; without
    // it the viewport repaints on every setItem.
    const bool had_updates = updatesEnabled();
    setUpdatesEnabled(false);
    const bool was_sorting = isSortingEnabled();
    setSortingEnabled(false); // sorting during a fill reorders rows mid-insert

    setRowCount(rows.size());
    for (int r = 0; r < rows.size(); ++r) {
        const QStringList& row = rows[r];
        for (int c = 0; c < row.size() && c < columnCount(); ++c) {
            auto* item = new QTableWidgetItem(row[c]);
            item->setForeground(QColor(colors::WHITE()));
            setItem(r, c, item);
        }
    }

    setSortingEnabled(was_sorting);
    setUpdatesEnabled(had_updates);
}

/// Appends a single row. Kept incremental for callers that genuinely add one
/// row at a time; prefer set_data() for a bulk fill — see the note there.
void DataTable::add_row(const QStringList& row) {
    int r = rowCount();
    insertRow(r);
    for (int c = 0; c < row.size() && c < columnCount(); ++c) {
        auto* item = new QTableWidgetItem(row[c]);
        item->setForeground(QColor(colors::WHITE()));
        setItem(r, c, item);
    }
    // Row height is a per-table constant, not a per-row property — it is set
    // once on the vertical header in the constructor. Calling setRowHeight()
    // here forced a geometry recalculation for every appended row.
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

void DataTable::set_cell_numeric(int row, int col, double value) {
    auto* it = item(row, col);
    if (it)
        it->setData(Qt::EditRole, value);
}

} // namespace fincept::ui
