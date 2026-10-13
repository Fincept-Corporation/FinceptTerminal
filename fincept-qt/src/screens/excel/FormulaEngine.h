// src/screens/excel/FormulaEngine.h
// Spreadsheet formula engine for the Excel screen — tokenizer, parser,
// evaluator and reference rewriting. QtCore only (no widgets), so it can be
// exercised without the UI.
//
// Semantics follow Excel where it matters for financial models:
//   * operator precedence  -x  %  ^  * /  + -  &  = <> < > <= >=  (^ is left-assoc, -2^2 = 4)
//   * errors propagate (#DIV/0!, #VALUE!, #REF!, #NAME?, #NUM!, #N/A); a model never
//     silently turns an error into 0
//   * aggregates over references skip text/blank cells; literal arguments are coerced
//   * typed numbers may carry thousands separators, currency symbols, a trailing % or
//     accounting parentheses — "1,234.50", "$1,234", "12.5%", "(1,200)" are numbers
//   * a leading apostrophe forces text ("'00123")
#pragma once

#include <QString>
#include <QStringList>
#include <QVector>

#include <memory>

namespace fincept::screens::formula {

/// One cell value as the engine sees it.
struct Value {
    enum class Type { Empty, Number, Text, Bool, Error };
    Type type = Type::Empty;
    double number = 0.0;
    bool boolean = false;
    QString text;   ///< Text payload, or the error code ("#DIV/0!") for Error.
    QString detail; ///< Errors only: human explanation ("Unknown function: XLOOKUP").

    static Value empty() { return {}; }
    static Value num(double d) {
        Value v;
        v.type = Type::Number;
        v.number = d;
        return v;
    }
    static Value str(const QString& s) {
        Value v;
        v.type = Type::Text;
        v.text = s;
        return v;
    }
    static Value boolean_of(bool b) {
        Value v;
        v.type = Type::Bool;
        v.boolean = b;
        return v;
    }
    static Value err(const QString& code, const QString& why = {}) {
        Value v;
        v.type = Type::Error;
        v.text = code;
        v.detail = why;
        return v;
    }

    bool is_empty() const { return type == Type::Empty; }
    bool is_number() const { return type == Type::Number; }
    bool is_text() const { return type == Type::Text; }
    bool is_bool() const { return type == Type::Bool; }
    bool is_error() const { return type == Type::Error; }
};

/// Error codes the engine produces.
namespace errors {
inline QString div0() {
    return QStringLiteral("#DIV/0!");
}
inline QString value() {
    return QStringLiteral("#VALUE!");
}
inline QString ref() {
    return QStringLiteral("#REF!");
}
inline QString name() {
    return QStringLiteral("#NAME?");
}
inline QString num() {
    return QStringLiteral("#NUM!");
}
inline QString na() {
    return QStringLiteral("#N/A");
}
inline QString cycle() {
    return QStringLiteral("#CYCLE!");
}
/// The formula text does not parse (not an Excel code — Excel refuses such input).
inline QString syntax() {
    return QStringLiteral("#ERROR!");
}
} // namespace errors

/// Read typed (non-formula) cell text as a number. Accepts thousands separators,
/// a currency symbol ($ € £ ¥ ₹), a trailing %, a leading +/- and accounting
/// parentheses. "2024-01-15", "1,2,3" and "abc" are not numbers.
bool parse_number(const QString& raw, double* out);

/// Interpret typed (non-formula) cell text: number, TRUE/FALSE, a typed error
/// code, a leading-apostrophe text literal, or plain text. Blank → Empty.
Value literal_value(const QString& raw);

/// Text shown for a value (Excel "General" format, 12 significant digits).
QString display_text(const Value& v);

/// Text written to CSV / clipboard values — numbers keep 15 significant digits.
QString export_text(const Value& v);

/// A1 column label for a 0-based index (0 → "A", 26 → "AA").
QString column_label(int col);
/// 0-based column index for column letters ("A" → 0, "AA" → 26); -1 if invalid.
int column_index(const QString& letters);
/// "B7" style label for a 0-based (row, col).
QString cell_label(int row, int col);

/// One rectangular block of cells a formula can read (single cell = 1×1).
struct RangeRef {
    QString sheet;  ///< empty = the formula's own sheet
    int row1 = 0;   ///< inclusive, 0-based, row1 <= row2
    int col1 = 0;   ///< inclusive, 0-based, col1 <= col2
    int row2 = 0;
    int col2 = 0;
    bool whole_col = false; ///< "A:C" — row bounds come from the sheet
    bool whole_row = false; ///< "1:3" — column bounds come from the sheet
};

struct Program; // compiled formula (opaque)

/// A compiled formula. `program` is null when the text does not parse.
struct Compiled {
    std::shared_ptr<const Program> program;
    QString error;          ///< parse error message when program is null
    QVector<RangeRef> refs; ///< every reference the formula can read
};

/// Compile formula text (with or without the leading '=').
Compiled compile(const QString& formula);

/// Supplies cell values during evaluation.
class EvalContext {
  public:
    virtual ~EvalContext() = default;
    /// Value of (row, col) on `sheet` ("" = the formula's own sheet). A sheet that
    /// does not exist must yield an #REF! error value.
    virtual Value cell(const QString& sheet, int row, int col) = 0;
    /// Used to bound whole-column / whole-row ranges; -1 for an unknown sheet.
    virtual int row_count(const QString& sheet) = 0;
    virtual int col_count(const QString& sheet) = 0;
};

/// Evaluate a compiled formula. Never throws; failures are Error values.
Value evaluate(const Compiled& compiled, EvalContext& ctx);

/// Copy/paste: move the relative part of every reference by (drow, dcol); `$`
/// parts stay. A reference pushed off the grid becomes #REF!. Returns the
/// formula with its leading '='.
QString shift_references(const QString& formula, int drow, int dcol);

enum class Axis { Rows, Cols };

/// Rows/columns were inserted (count > 0) or deleted (count < 0) at `at` on
/// `target_sheet`: rewrite the references that point at that sheet, as Excel
/// does (absolute and relative alike; ranges grow/shrink; a reference into a
/// deleted block becomes #REF!). `formula_sheet` is the sheet the formula lives
/// on — unqualified references point there. Returns the (possibly unchanged)
/// formula; `changed` reports whether anything was rewritten.
QString adjust_for_structure(const QString& formula, const QString& formula_sheet, const QString& target_sheet,
                             Axis axis, int at, int count, bool* changed = nullptr);

/// A sheet was renamed: rewrite "Old!A1" references to "New!A1" (quoting the
/// new name when needed). `changed` reports whether anything was rewritten.
QString rename_sheet_references(const QString& formula, const QString& old_name, const QString& new_name,
                                bool* changed = nullptr);

/// Formula text Excel accepts: engine-only aliases are renamed (AVG → AVERAGE).
QString to_excel(const QString& formula);

/// Sorted names of the functions the engine implements.
QStringList supported_functions();

/// Excel serial date (days since 1899-12-30) for an ISO "yyyy-MM-dd" text or a
/// number that already is a serial. Returns false when `v` is not a date.
bool date_serial(const Value& v, double* serial);

} // namespace fincept::screens::formula
