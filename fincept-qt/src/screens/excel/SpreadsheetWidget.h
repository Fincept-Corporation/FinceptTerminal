// src/screens/excel/SpreadsheetWidget.h
// Single-sheet spreadsheet widget with formula bar, context menu, cell editing,
// Excel-compatible clipboard and an undo stack. Formulas are evaluated by
// FormulaEngine (see FormulaEngine.h for the supported semantics).
#pragma once

#include "screens/excel/FormulaEngine.h"

#include <QAction>
#include <QHash>
#include <QLineEdit>
#include <QMenu>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>
#include <QWidget>

#include <memory>

class QLabel;
class QUndoStack;

namespace fincept::screens {

class SpreadsheetWidget;

// ── Formula-aware cell item ──────────────────────────────────────────────────

class SpreadsheetItem : public QTableWidgetItem {
  public:
    SpreadsheetItem();
    explicit SpreadsheetItem(const QString& text);

    QVariant data(int role) const override;
    void setData(int role, const QVariant& value) override;

    /// Qt clones the table's item prototype whenever the model has to
    /// materialise a cell that has no item yet (typing into an empty cell,
    /// pasting past the filled range, drag & drop). The base implementation
    /// slices back to a plain QTableWidgetItem, which would silently disable
    /// formula evaluation for that cell — the grid is not pre-populated, so
    /// this is the common path, not an edge case.
    QTableWidgetItem* clone() const override;

    /// The raw text (may be a formula starting with '=').
    QString raw_text() const { return raw_text_; }
    bool is_formula() const { return raw_text_.startsWith(QLatin1Char('=')); }
    /// Raw text before the most recent edit — lets an in-place edit be undone.
    QString previous_raw_text() const { return prev_raw_text_; }
    /// The value an imported workbook stored for this formula. Shown (marked as
    /// such) when the engine cannot evaluate the formula itself (#NAME?).
    void set_import_cached(const QString& value) { import_cached_ = value; }
    QString import_cached() const { return import_cached_; }

  private:
    friend class SpreadsheetWidget;
    const formula::Compiled& compiled() const;

    QString raw_text_;
    QString prev_raw_text_;
    QString import_cached_;
    // Evaluation cache, valid while cached_gen_ matches the workbook generation.
    mutable std::shared_ptr<formula::Compiled> compiled_;
    mutable formula::Value cached_value_;
    mutable quint64 cached_gen_ = 0;
    mutable bool on_stack_ = false; // evaluation in progress (cycle detection)
};

// ── Spreadsheet widget (one sheet) ───────────────────────────────────────────

class SpreadsheetWidget : public QWidget {
    Q_OBJECT
  public:
    explicit SpreadsheetWidget(const QString& sheet_name = "Sheet1", int rows = 100, int cols = 26,
                               QWidget* parent = nullptr);

    /// All cell data (raw text, formulas included) as a 2D array — the full grid.
    QVector<QVector<QString>> get_data() const;
    /// Only the used range (rows/cols up to the last non-empty cell).
    QVector<QVector<QString>> get_used_data() const;
    /// Last non-empty row/col + 1 (0×0 for an empty sheet).
    int used_rows() const;
    int used_cols() const;

    /// Replace the sheet contents. `cached` (optional) carries the values an
    /// imported workbook stored for its formulas, keyed by cache_key(row, col).
    void set_data(const QVector<QVector<QString>>& data, const QHash<qint64, QString>& cached = {});
    static qint64 cache_key(int row, int col) { return (static_cast<qint64>(row) << 20) | col; }

    /// Raw text of a cell.
    QString cell_text(int row, int col) const;
    /// Evaluated value of a cell (formulas computed, literals parsed).
    formula::Value value_at(int row, int col) const;
    /// What the grid shows for a cell.
    QString display_value(int row, int col) const;

    /// Set one cell (undoable).
    void set_cell(int row, int col, const QString& text);

    /// Current row/col count
    int row_count() const { return table_->rowCount(); }
    int col_count() const { return table_->columnCount(); }

    /// Sheet name
    QString sheet_name() const { return sheet_name_; }
    /// Renames the sheet and rewrites every formula in the workbook that
    /// referenced the old name.
    void set_sheet_name(const QString& name);

    /// Repaint every formula cell (values are cached per edit generation).
    void recalculate();

    /// Insert/delete rows/columns at the current selection (undoable; formula
    /// references across the workbook move the way Excel moves them).
    void insert_row_above();
    void insert_row_below();
    void insert_col_left();
    void insert_col_right();
    void delete_selected_rows();
    void delete_selected_cols();
    /// Same, at explicit 0-based positions.
    void insert_rows(int at, int count);
    void delete_rows(int at, int count);
    void insert_cols(int at, int count);
    void delete_cols(int at, int count);

    /// Clipboard over the whole selected range. Text goes out as TSV of the
    /// DISPLAYED values (what Excel puts on the clipboard); a private format
    /// carries the raw formulas so a paste inside the terminal keeps them and
    /// shifts their relative references, as Excel does.
    void copy_selection(bool cut = false);
    void paste_clipboard(bool values_only = false);
    /// Blank every cell in the selection (Delete key).
    void clear_selection();
    /// Prompt for text and select the next matching cell (Ctrl+F / F3).
    void find_next(bool prompt = true);

    QUndoStack* undo_stack() const { return undo_stack_; }
    void undo();
    void redo();

    /// Excel-style status summary of the selected cells ("SUM 1,234 · AVG 617 ·
    /// COUNT 2"); empty when the selection holds fewer than two values.
    QString selection_summary() const;

  signals:
    /// Emitted on any mutation of sheet content or structure — cell edits,
    /// paste, clear, undo/redo and row/column insert/delete. ExcelScreen uses
    /// it to drive the dirty marker and the unsaved-changes guard.
    void data_changed();
    /// The selection moved or changed size (status-bar summary refresh).
    void selection_changed();

  private slots:
    void on_cell_changed(int row, int col);
    void on_current_cell_changed(int row, int col, int prev_row, int prev_col);
    void on_formula_bar_return();
    void on_context_menu(const QPoint& pos);

  private:
    friend class SpreadsheetItem;
    friend class SheetEditCommand;
    friend class SheetStructureCommand;

    struct CellEdit {
        int row = 0;
        int col = 0;
        QString before;
        QString after;
    };

    void build_ui(int rows, int cols);
    void setup_headers(int cols);
    /// Size each column to its widest value (clamped). Measured from the source
    /// text so formulas don't have to be evaluated just to lay the grid out.
    void autofit_columns(const QVector<QVector<QString>>& cells);
    void grow_column_for(int col, const QString& text);
    void install_shortcuts();
    static QString column_label(int col);

    /// Write raw text without touching the undo stack (commands use this).
    void write_raw(int row, int col, const QString& text);
    /// Apply a batch of edits as one undo step.
    void apply_edits(const QVector<CellEdit>& edits, const QString& label);
    /// Structural change (rows or cols) as one undo step.
    void apply_structure(formula::Axis axis, int at, int count, const QString& label);

    /// Sheets of the same workbook (siblings in the QTabWidget), this one included.
    QVector<SpreadsheetWidget*> workbook_sheets() const;
    const SpreadsheetWidget* sheet_named(const QString& name) const;
    formula::Value evaluate_item(const SpreadsheetItem* item) const;
    formula::Value literal_or_cached(const SpreadsheetWidget* sheet, int row, int col) const;
    QString cell_tooltip(const SpreadsheetItem* item) const;

    QTableWidget* table_ = nullptr;
    QLineEdit* formula_bar_ = nullptr;
    QLabel* cell_ref_label_ = nullptr;
    QString sheet_name_;
    QUndoStack* undo_stack_ = nullptr;

    bool updating_formula_bar_ = false;
    bool applying_command_ = false; // writes from undo/redo commands are not re-recorded
    QString last_find_;             // remembered Ctrl+F term so F3 can repeat it
};

} // namespace fincept::screens
