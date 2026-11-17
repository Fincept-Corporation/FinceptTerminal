#pragma once
// NotebookOutputs — how a cell's outputs look in the Code tab:
//
//   NbTextOutput   stdout / stderr / a value's repr: monospace, height fitted to
//                  the text up to a limit, "Show all N lines" past it, stderr
//                  marked with a STDERR tag (not by colour alone), the last
//                  5,000 lines kept on screen (the full text stays in the model).
//   NbTableOutput  a DataFrame / Series: a virtual table (NbTableModel) — sortable,
//                  numbers right-aligned and formatted through ui::formatting,
//                  sticky header, "1,500 × 6 · first 1,000 rows" caption, COPY,
//                  CSV export and EXPAND (inspector). Column widths are arithmetic
//                  (no resize-to-contents).
//   NbImageOutput  a matplotlib figure scaled to the cell, COPY / SAVE PNG / EXPAND,
//                  and in the inspector a FIT / 100% zoom.
//   NbErrorOutput  the plain message first ("ZeroDivisionError: division by zero"),
//                  the failing line of the cell with GO TO LINE, then the
//                  traceback with the cell's frames marked.
//
// make_output_view() builds the right one; `large` is the inspector's version.

#include "screens/code_editor/NotebookDocument.h"

#include <QAbstractTableModel>
#include <QJsonArray>
#include <QPixmap>
#include <QWidget>

class QLabel;
class QPlainTextEdit;
class QPushButton;
class QScrollArea;
class QTableView;
class QTextBrowser;
class QToolButton;

namespace fincept::screens {

/// Rows of a "table" output, formatted on demand (only visible cells are asked).
class NbTableModel : public QAbstractTableModel {
  public:
    static constexpr int SortRole = Qt::UserRole + 1;

    explicit NbTableModel(const QJsonObject& table, QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    int columnCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;

    int total_rows() const { return total_rows_; }
    int total_columns() const { return total_columns_; }
    /// Header + every row as text: `raw` = full precision in the C locale (CSV),
    /// else as shown.
    QStringList headers() const;
    QVector<QStringList> rows_text(bool raw) const;
    /// Width in average characters that fits the header and the first rows.
    int width_chars(int column) const;

  private:
    QString cell_text(int row, int column, bool raw) const;

    QStringList columns_;  ///< index header first, then the data columns
    QStringList dtypes_;   ///< per data column
    QJsonArray index_;
    QJsonArray rows_;
    QVector<int> decimals_; ///< per data column (numbers)
    int total_rows_ = 0;
    int total_columns_ = 0;
};

/// Common base: EXPAND and GO TO LINE requests go up to the cell.
class NbOutputView : public QWidget {
    Q_OBJECT
  public:
    explicit NbOutputView(QWidget* parent = nullptr) : QWidget(parent) {}
    /// The same output, changed (a live stream grew). Only text outputs change.
    virtual void update_output(const notebook::CellOutput&) {}
    /// Re-read colours that are not in the stylesheet (rich text).
    virtual void refresh_theme() {}
    /// Re-fit sizes after a font change.
    virtual void refit() {}

  signals:
    void expand_requested();
    void jump_to_line(int line);
};

class NbTextOutput : public NbOutputView {
  public:
    NbTextOutput(const notebook::CellOutput& out, bool large, QWidget* parent = nullptr);
    void update_output(const notebook::CellOutput& out) override;
    void refit() override;

  private:
    void fit();
    QPlainTextEdit* view_ = nullptr;
    QLabel* tag_ = nullptr;
    QPushButton* more_ = nullptr;
    QString text_;
    bool large_ = false;
    bool expanded_ = false;
};

class NbTableOutput : public NbOutputView {
  public:
    NbTableOutput(const notebook::CellOutput& out, bool large, QWidget* parent = nullptr);
    void refresh_theme() override;
    void refit() override;

  private:
    void fit();
    void copy_rows();
    void export_csv();
    NbTableModel* model_ = nullptr;
    QTableView* view_ = nullptr;
    QLabel* caption_ = nullptr;
    bool large_ = false;
};

class NbImageOutput : public NbOutputView {
  public:
    NbImageOutput(const notebook::CellOutput& out, bool large, QWidget* parent = nullptr);
    void refit() override;

  protected:
    void resizeEvent(QResizeEvent* e) override;
    bool eventFilter(QObject* watched, QEvent* event) override;

  private:
    void rescale();
    QPixmap pixmap_;
    QLabel* image_ = nullptr;
    QScrollArea* scroll_ = nullptr;
    QToolButton* zoom_ = nullptr;
    bool large_ = false;
    bool actual_size_ = false;
    int last_width_ = -1;
};

class NbErrorOutput : public NbOutputView {
  public:
    NbErrorOutput(const notebook::CellOutput& out, bool large, QWidget* parent = nullptr);
    void refresh_theme() override;
    void refit() override;

  private:
    void render_traceback();
    void fit();
    notebook::CellOutput out_;
    QTextBrowser* traceback_ = nullptr;
    QPushButton* toggle_ = nullptr;
    bool large_ = false;
};

NbOutputView* make_output_view(const notebook::CellOutput& out, bool large, QWidget* parent);

/// Line spacing of the output font (stylesheet px(-1)) — used for heights before polish.
int nb_output_line_height();

} // namespace fincept::screens
