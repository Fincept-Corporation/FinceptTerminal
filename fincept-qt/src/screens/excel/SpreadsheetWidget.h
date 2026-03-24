// src/screens/excel/SpreadsheetWidget.h
// Single-sheet spreadsheet widget with formula bar, context menu, and cell editing.
#pragma once

#include <QAction>
#include <QLineEdit>
#include <QMenu>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>
#include <QWidget>

namespace fincept::screens {

// ── Formula-aware cell item ──────────────────────────────────────────────────

class SpreadsheetItem : public QTableWidgetItem {
  public:
    SpreadsheetItem();
    explicit SpreadsheetItem(const QString& text);

    QVariant data(int role) const override;
    void setData(int role, const QVariant& value) override;

    /// The raw text (may be a formula starting with '=')
    QString raw_text() const { return raw_text_; }

  private:
    QVariant evaluate_formula() const;
    double resolve_cell_value(int row, int col) const;
    QVector<double> resolve_range(const QString& from, const QString& to) const;
    static int column_from_letters(const QString& letters);
    static void parse_cell_ref(const QString& ref, int& row, int& col);

    QString raw_text_;
};

// ── Spreadsheet widget (one sheet) ───────────────────────────────────────────

class SpreadsheetWidget : public QWidget {
    Q_OBJECT
  public:
    explicit SpreadsheetWidget(const QString& sheet_name = "Sheet1", int rows = 100, int cols = 26,
                               QWidget* parent = nullptr);

    /// Get all cell data as 2D string array
    QVector<QVector<QString>> get_data() const;

    /// Set data from 2D string array
    void set_data(const QVector<QVector<QString>>& data);

    /// Get raw text of a cell
    QString cell_text(int row, int col) const;

    /// Set cell text
    void set_cell(int row, int col, const QString& text);

    /// Current row/col count
    int row_count() const { return table_->rowCount(); }
    int col_count() const { return table_->columnCount(); }

    /// Sheet name
    QString sheet_name() const { return sheet_name_; }
    void set_sheet_name(const QString& name) { sheet_name_ = name; }

    /// Recalculate all formula cells
    void recalculate();

    /// Insert/delete rows/columns
    void insert_row_above();
    void insert_row_below();
    void insert_col_left();
    void insert_col_right();
    void delete_selected_rows();
    void delete_selected_cols();

  signals:
    void data_changed();

  private slots:
    void on_cell_changed(int row, int col);
    void on_current_cell_changed(int row, int col, int prev_row, int prev_col);
    void on_formula_bar_return();
    void on_context_menu(const QPoint& pos);

  private:
    void build_ui(int rows, int cols);
    void setup_headers(int cols);
    static QString column_label(int col);

    QTableWidget* table_ = nullptr;
    QLineEdit* formula_bar_ = nullptr;
    QLabel* cell_ref_label_ = nullptr;
    QString sheet_name_;

    bool updating_formula_bar_ = false;
};

} // namespace fincept::screens
