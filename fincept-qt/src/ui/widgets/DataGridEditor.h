#pragma once
// DataGridEditor — an editable grid for data a user types, pastes from a spreadsheet or imports from
// a CSV file: dated cash flows, return series, deal comparables, small tables.
//
//   QVector<ui::DataGridColumn> cols = {
//       ui::DataGridColumn::date("date", tr("DATE")),
//       ui::DataGridColumn::choice("type", tr("TYPE"), {"call", "distribution", "nav"},
//                                  {tr("Call"), tr("Distribution"), tr("NAV")}),
//       ui::DataGridColumn::number("amount", tr("AMOUNT")),
//       ui::DataGridColumn::text("note", tr("NOTE")),
//   };
//   auto* grid = new ui::DataGridEditor(cols, this);
//   grid->from_json(rows);                         // [{"date": "2020-03-31", "amount": 1e6, ...}]
//   connect(grid, &ui::DataGridEditor::changed, this, &MyPanel::on_inputs_changed);
//   const QJsonArray valid = grid->to_json();      // valid rows only, typed (ISO dates, doubles)
//   const QJsonArray all = grid->raw_json();       // every row as typed, for saving a draft
//
// Model / view (QTableView over a QAbstractTableModel). Keyboard: arrows / Tab move, typing edits,
// Enter on the last row adds a row, Insert adds a row, Ctrl+Delete removes the selected rows,
// Delete clears the selected cells, Ctrl+C copies them as tab-separated text, Ctrl+V pastes at the
// current row. PASTE replaces every row with the clipboard table; IMPORT CSV does the same from a
// file (read and parsed on a worker thread).
//
// Pasted / imported text may be tab-, semicolon- or comma-separated (quotes per RFC 4180), with or
// without a header row. Numbers may carry thousands separators, a currency sign, a trailing % and
// accounting brackets "(1,000)"; a money column (DataGridColumn::amount) reads its cells with the shared
// amount reader (the user's number settings, K / M / B, lakh / crore). Dates may be ISO (2020-03-31), d/m/y or m/d/y (the system's order
// when a date could be either), "31 Mar 2020" or "Mar 31, 2020". A row that does not validate stays
// in the grid, tinted with the theme's DANGER colour and its reason in the tooltip, is left out of
// to_json() and is counted in the line under the grid: "2 rows ignored: date not recognised".
//
// Theme: every colour and size comes from ui::colors / ui::fonts and follows theme_changed.

#include <QDate>
#include <QHash>
#include <QJsonArray>
#include <QPointer>
#include <QString>
#include <QStringList>
#include <QVector>
#include <QWidget>

#include <functional>

class QLabel;
class QPushButton;
class QTableView;

namespace fincept::ui {

struct DataGridColumn {
    enum class Type { Date, Number, Text, Choice };

    QString key;   ///< JSON key of the column
    QString title; ///< header text
    Type type = Type::Text;
    /// An empty required cell makes the row invalid; an empty optional cell is left out of to_json().
    bool required = true;
    /// Choice: canonical values (written to JSON); the first is the default of a new row.
    QStringList choices;
    /// Choice: display text per value (empty = the values themselves).
    QStringList choice_labels;
    /// Choice: extra lower-case spellings accepted on paste -> canonical value ("capital call" -> "call").
    QHash<QString, QString> synonyms;
    /// Number: decimals shown when the cell is not being edited.
    int decimals = 2;
    QString tooltip;
    /// Number: a money column (see amount()). Cells are read with ui::formatting::read_amount_input —
    /// the user's separators, K / M / B, lakh / crore, currency symbols, accounting negatives — and a
    /// stored value is written back in the user's number format.
    bool money = false;
    /// Money column: its currency ("" = none fixed). A cell naming another currency is refused with
    /// both currencies named; it is never converted.
    QString currency;

    static DataGridColumn date(const QString& key, const QString& title, bool required = true);
    static DataGridColumn number(const QString& key, const QString& title, int decimals = 2, bool required = true);
    /// A money column: number() whose cells are read as amounts ("68.7B", "$1,250,000", "(2.5M)").
    static DataGridColumn amount(const QString& key, const QString& title, const QString& currency = QString(),
                                 int decimals = 2, bool required = true);
    static DataGridColumn text(const QString& key, const QString& title, bool required = false);
    static DataGridColumn choice(const QString& key, const QString& title, const QStringList& choices,
                                 const QStringList& labels = {}, bool required = true);
};

class DataGridModel;

class DataGridEditor : public QWidget {
    Q_OBJECT
  public:
    /// Maps pasted / imported cells (rows x pasted columns, header removed) onto the grid's columns.
    /// The default: the first N pasted columns fill the first N grid columns.
    using PasteAdapter = std::function<QVector<QStringList>(const QVector<QStringList>& cells)>;

    explicit DataGridEditor(const QVector<DataGridColumn>& columns, QWidget* parent = nullptr);
    ~DataGridEditor() override;

    void set_paste_adapter(PasteAdapter adapter);
    /// Rows kept at most (paste / import beyond it are dropped and reported). Default 5000.
    void set_max_rows(int rows);
    /// Rows the grid is sized to show before it scrolls. Default 8.
    void set_visible_rows(int rows);
    /// Extra text in the line under the grid when there is nothing to report ("Paste from a spreadsheet").
    void set_hint(const QString& hint);
    /// The currency of money column `key` changed (another fund / deal is shown): its cells are checked
    /// again. Does not emit changed() — call it before from_json() of the new rows.
    void set_column_currency(const QString& key, const QString& currency);

    // ── Data ──────────────────────────────────────────────────────────────────
    /// Replace every row. Values may be typed (numbers) or text; invalid rows are kept as entered.
    void from_json(const QJsonArray& rows);
    /// Valid rows only, typed: Date -> "yyyy-MM-dd", Number -> double, Choice -> canonical value.
    QJsonArray to_json() const;
    /// Every non-empty row exactly as entered (strings) — for saving a draft.
    QJsonArray raw_json() const;

    int row_count() const;
    int valid_row_count() const;
    int invalid_row_count() const;
    /// The text of the line under the grid ("" when every row is valid and there is no hint).
    QString problems_text() const;

    /// Header titles and every row as display text — for copy / export by the host.
    QStringList headers() const;
    QVector<QStringList> rows_text() const;

    // ── Actions (also on the toolbar / keyboard) ──────────────────────────────
    void add_row();
    void remove_selected_rows();
    void clear_rows();
    /// Paste delimited `text`: `replace` = replace every row, else overwrite from the current row down.
    /// Returns the number of rows taken.
    int paste_text(const QString& text, bool replace);
    void paste_from_clipboard(bool replace);
    /// Ask for a CSV / TSV file and replace the rows with it (file read + parse off the GUI thread).
    void import_csv();

    // ── Parsing (static, usable without a widget) ─────────────────────────────
    /// Delimited text -> cells. Tab if the text has tabs, else ';' if it has more ';' than ',', else ','.
    static QVector<QStringList> parse_delimited(const QString& text);
    /// "1,234.5", "(1,000)", "$ 2.5", "-3%", "1.234,5" -> double. False when not a number.
    static bool parse_number(const QString& text, double* out);
    /// ISO, d/m/y, m/d/y, "31 Mar 2020", "Mar 31, 2020", "2020/03/31" -> QDate. False when not a date.
    static bool parse_date(const QString& text, QDate* out);

  signals:
    /// Any edit, paste, import, add or remove.
    void changed();

  protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

  private:
    void build_ui();
    void restyle();
    void update_status();
    void copy_selection() const;
    void clear_selected_cells();
    QVector<QStringList> adapt(const QVector<QStringList>& cells) const;
    int apply_cells(const QVector<QStringList>& cells, bool replace, int start_row);
    void on_import_parsed(const QVector<QStringList>& cells, const QString& error, const QString& file_name);

    QVector<DataGridColumn> columns_;
    DataGridModel* model_ = nullptr;
    QTableView* view_ = nullptr;
    QPushButton* add_btn_ = nullptr;
    QPushButton* remove_btn_ = nullptr;
    QPushButton* paste_btn_ = nullptr;
    QPushButton* import_btn_ = nullptr;
    QLabel* status_ = nullptr;
    PasteAdapter adapter_;
    QString hint_;
    QString last_note_; ///< one-off note from the last paste / import ("12 rows imported")
    int max_rows_ = 5000;
    int visible_rows_ = 8;
    bool importing_ = false;
};

} // namespace fincept::ui
