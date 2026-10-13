// src/screens/excel/FormulaEngine.cpp
// See FormulaEngine.h for the semantics. QtCore only.

#include "screens/excel/FormulaEngine.h"

#include <QDate>
#include <QDateTime>
#include <QHash>
#include <QRegularExpression>

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <functional>
#include <limits>
#include <vector>

namespace fincept::screens::formula {

// ═════════════════════════════════════════════════════════════════════════════
// Number parsing / display
// ═════════════════════════════════════════════════════════════════════════════

namespace {

constexpr int kFeMaxCols = 16384;   // XFD
constexpr int kFeMaxRows = 1048576; // Excel's grid
constexpr int kFeMaxDepth = 200;    // parser nesting cap (stack safety)
constexpr qint64 kFeMaxRangeCells = 4'000'000;

bool fe_is_currency(QChar c) {
    return c == QLatin1Char('$') || c == QChar(0x20AC) /*€*/ || c == QChar(0x00A3) /*£*/ ||
           c == QChar(0x00A5) /*¥*/ || c == QChar(0x20B9) /*₹*/;
}

bool fe_is_ascii_letter(QChar c) {
    const ushort u = c.unicode();
    return (u >= 'A' && u <= 'Z') || (u >= 'a' && u <= 'z');
}

bool fe_is_ascii_digit(QChar c) {
    const ushort u = c.unicode();
    return u >= '0' && u <= '9';
}

// Relative tolerance used for "equal" comparisons — Excel compares at ~15
// significant digits, so 0.1 + 0.2 = 0.3 is TRUE there.
bool fe_nearly_equal(double a, double b) {
    if (a == b)
        return true;
    const double scale = std::max(std::fabs(a), std::fabs(b));
    return std::fabs(a - b) <= scale * 1e-14;
}

Value fe_finite(double x) {
    if (!std::isfinite(x))
        return Value::err(errors::num(), QStringLiteral("The result is not a finite number"));
    if (x == 0.0)
        x = 0.0; // fold -0
    return Value::num(x);
}

} // namespace

bool parse_number(const QString& raw, double* out) {
    QString s = raw.trimmed();
    if (s.isEmpty())
        return false;

    bool negative = false;
    // Accounting negative: "(1,234.56)".
    if (s.startsWith(QLatin1Char('(')) && s.endsWith(QLatin1Char(')'))) {
        negative = true;
        s = s.mid(1, s.size() - 2).trimmed();
    }
    auto take_sign = [&s, &negative]() {
        if (s.startsWith(QLatin1Char('-'))) {
            negative = !negative;
            s = s.mid(1).trimmed();
        } else if (s.startsWith(QLatin1Char('+'))) {
            s = s.mid(1).trimmed();
        }
    };
    take_sign();
    if (!s.isEmpty() && fe_is_currency(s.front())) {
        s = s.mid(1).trimmed();
        take_sign(); // "$-5"
    }
    if (!s.isEmpty() && fe_is_currency(s.back())) // "5€"
        s = s.left(s.size() - 1).trimmed();

    bool percent = false;
    if (s.endsWith(QLatin1Char('%'))) {
        percent = true;
        s = s.left(s.size() - 1).trimmed();
    }
    if (s.isEmpty())
        return false;

    // Only digits, one '.', thousands commas and an exponent may remain.
    if (s.contains(QLatin1Char(','))) {
        static const QRegularExpression kGrouped(QStringLiteral("^\\d{1,3}(,\\d{3})+(\\.\\d*)?$"));
        if (!kGrouped.match(s).hasMatch())
            return false;
        s.remove(QLatin1Char(','));
    }
    for (const QChar c : s) {
        if (!(fe_is_ascii_digit(c) || c == QLatin1Char('.') || c == QLatin1Char('e') || c == QLatin1Char('E') ||
              c == QLatin1Char('+') || c == QLatin1Char('-')))
            return false; // rejects "inf", "nan", "0x10", dates...
    }
    if (!fe_is_ascii_digit(s.front()) && s.front() != QLatin1Char('.'))
        return false;

    bool ok = false;
    double v = s.toDouble(&ok); // C locale
    if (!ok || !std::isfinite(v))
        return false;
    if (percent)
        v /= 100.0;
    if (negative)
        v = -v;
    if (out)
        *out = v;
    return true;
}

Value literal_value(const QString& raw) {
    if (raw.isEmpty())
        return Value::empty();
    if (raw.startsWith(QLatin1Char('\'')))
        return Value::str(raw.mid(1)); // forced text
    const QString t = raw.trimmed();
    if (t.isEmpty())
        return Value::str(raw);
    double d = 0.0;
    if (parse_number(t, &d))
        return Value::num(d);
    if (t.compare(QLatin1String("TRUE"), Qt::CaseInsensitive) == 0)
        return Value::boolean_of(true);
    if (t.compare(QLatin1String("FALSE"), Qt::CaseInsensitive) == 0)
        return Value::boolean_of(false);
    if (t.startsWith(QLatin1Char('#'))) {
        static const QStringList kCodes = {QStringLiteral("#DIV/0!"), QStringLiteral("#N/A"),
                                           QStringLiteral("#NAME?"),  QStringLiteral("#NULL!"),
                                           QStringLiteral("#NUM!"),   QStringLiteral("#REF!"),
                                           QStringLiteral("#VALUE!")};
        for (const QString& c : kCodes)
            if (t.compare(c, Qt::CaseInsensitive) == 0)
                return Value::err(c, QStringLiteral("Typed error value"));
    }
    return Value::str(raw);
}

QString display_text(const Value& v) {
    switch (v.type) {
        case Value::Type::Empty:
            return {};
        case Value::Type::Number: {
            const double x = v.number;
            if (x == 0.0)
                return QStringLiteral("0");
            if (std::floor(x) == x && std::fabs(x) < 1e15)
                return QString::number(static_cast<qlonglong>(x));
            return QString::number(x, 'g', 12);
        }
        case Value::Type::Text:
            return v.text;
        case Value::Type::Bool:
            return v.boolean ? QStringLiteral("TRUE") : QStringLiteral("FALSE");
        case Value::Type::Error:
            return v.text;
    }
    return {};
}

QString export_text(const Value& v) {
    if (v.type == Value::Type::Number) {
        const double x = v.number;
        if (x == 0.0)
            return QStringLiteral("0");
        if (std::floor(x) == x && std::fabs(x) < 1e15)
            return QString::number(static_cast<qlonglong>(x));
        return QString::number(x, 'g', 15);
    }
    return display_text(v);
}

QString column_label(int col) {
    QString label;
    if (col < 0)
        return label;
    while (col >= 0) {
        label.prepend(QChar('A' + (col % 26)));
        col = col / 26 - 1;
    }
    return label;
}

int column_index(const QString& letters) {
    if (letters.isEmpty() || letters.size() > 3)
        return -1;
    int col = 0;
    for (const QChar c : letters) {
        if (!fe_is_ascii_letter(c))
            return -1;
        col = col * 26 + (c.toUpper().unicode() - 'A' + 1);
    }
    col -= 1;
    return (col >= 0 && col < kFeMaxCols) ? col : -1;
}

QString cell_label(int row, int col) {
    return column_label(col) + QString::number(row + 1);
}

bool date_serial(const Value& v, double* serial) {
    static const QDate kEpoch(1899, 12, 30);
    if (v.type == Value::Type::Number) {
        if (v.number < 0)
            return false;
        *serial = v.number;
        return true;
    }
    if (v.type != Value::Type::Text)
        return false;
    const QString s = v.text.trimmed();
    QDate d = QDate::fromString(s.left(10), Qt::ISODate);
    if (!d.isValid())
        d = QDate::fromString(s, QStringLiteral("yyyy/MM/dd"));
    if (!d.isValid())
        return false;
    double frac = 0.0;
    if (s.size() > 10) {
        const QDateTime dt = QDateTime::fromString(s, Qt::ISODate);
        if (dt.isValid())
            frac = dt.time().msecsSinceStartOfDay() / 86400000.0;
    }
    *serial = static_cast<double>(kEpoch.daysTo(d)) + frac;
    return true;
}

// ═════════════════════════════════════════════════════════════════════════════
// Tokenizer
// ═════════════════════════════════════════════════════════════════════════════

namespace {

enum class FeTok { Number, String, Bool, Error, Ref, ColRef, RowRef, Ident, Op, LParen, RParen, Sep, Colon, End };

struct FeToken {
    FeTok kind = FeTok::End;
    int pos = 0; // offset in the formula body (text after '=')
    int len = 0;
    QString text; // ident (upper-cased) / operator / string payload / error code
    double number = 0.0;
    bool boolean = false;
    bool integral = false; // Number: plain digits (row-range candidate)
    // References (Ref / ColRef / RowRef, and Ident when it spells a column)
    QString sheet;
    bool has_sheet = false;
    int row = -1;
    int col = -1;
    bool abs_row = false;
    bool abs_col = false;
};

// "$AB$12", "AB12", "$AB", "AB", "$12" — classify one reference word.
// Returns 0 = not a reference, 1 = cell, 2 = column only, 3 = row only.
int fe_classify_ref_word(const QString& w, FeToken& t) {
    int i = 0;
    const int n = w.size();
    bool abs_col = false;
    if (i < n && w[i] == QLatin1Char('$')) {
        abs_col = true;
        ++i;
    }
    const int letters_start = i;
    while (i < n && fe_is_ascii_letter(w[i]))
        ++i;
    const QString letters = w.mid(letters_start, i - letters_start);
    bool abs_row = false;
    if (i < n && w[i] == QLatin1Char('$')) {
        abs_row = true;
        ++i;
    }
    const int digits_start = i;
    while (i < n && fe_is_ascii_digit(w[i]))
        ++i;
    const QString digits = w.mid(digits_start, i - digits_start);
    if (i != n)
        return 0;

    if (!letters.isEmpty() && !digits.isEmpty()) {
        const int col = column_index(letters);
        bool ok = false;
        const int row = digits.toInt(&ok);
        if (col < 0 || !ok || row < 1 || row > kFeMaxRows)
            return 0;
        t.col = col;
        t.row = row - 1;
        t.abs_col = abs_col;
        t.abs_row = abs_row;
        return 1;
    }
    if (!letters.isEmpty() && digits.isEmpty() && !abs_row) {
        const int col = column_index(letters);
        if (col < 0)
            return 0;
        t.col = col;
        t.abs_col = abs_col;
        return 2;
    }
    if (letters.isEmpty() && !digits.isEmpty()) {
        // "$12" lexes with abs_col set (the first '$' is read before any letters).
        bool ok = false;
        const int row = digits.toInt(&ok);
        if (!ok || row < 1 || row > kFeMaxRows)
            return 0;
        t.row = row - 1;
        t.abs_row = abs_col || abs_row;
        return 3;
    }
    return 0;
}

bool fe_word_char(QChar c) {
    return fe_is_ascii_letter(c) || fe_is_ascii_digit(c) || c == QLatin1Char('_') || c == QLatin1Char('.') ||
           c == QLatin1Char('$');
}

int fe_skip_spaces(const QString& s, int i) {
    while (i < s.size() && s[i].isSpace())
        ++i;
    return i;
}

// Lex the reference that follows "Sheet!" starting at j; advances j.
bool fe_lex_ref_after_sheet(const QString& s, int& j, const QString& sheet, FeToken& t, QString* error) {
    int k = j;
    while (k < s.size() && fe_word_char(s[k]) && s[k] != QLatin1Char('.'))
        ++k;
    const QString w = s.mid(j, k - j);
    const int cls = fe_classify_ref_word(w, t);
    if (cls == 0) {
        *error = QStringLiteral("Invalid reference after sheet name '%1'").arg(sheet);
        return false;
    }
    t.kind = cls == 1 ? FeTok::Ref : cls == 2 ? FeTok::ColRef : FeTok::RowRef;
    t.sheet = sheet;
    t.has_sheet = true;
    j = k;
    return true;
}

QVector<FeToken> fe_tokenize(const QString& s, QString* error) {
    QVector<FeToken> out;
    const int n = s.size();
    int i = 0;
    while (i < n) {
        const QChar c = s[i];
        if (c.isSpace()) {
            ++i;
            continue;
        }
        FeToken t;
        t.pos = i;

        if (c == QLatin1Char('"')) { // text literal, "" escapes a quote
            QString val;
            int j = i + 1;
            bool closed = false;
            while (j < n) {
                if (s[j] == QLatin1Char('"')) {
                    if (j + 1 < n && s[j + 1] == QLatin1Char('"')) {
                        val += QLatin1Char('"');
                        j += 2;
                        continue;
                    }
                    closed = true;
                    ++j;
                    break;
                }
                val += s[j++];
            }
            if (!closed) {
                *error = QStringLiteral("Unterminated text literal");
                return {};
            }
            t.kind = FeTok::String;
            t.text = val;
            t.len = j - i;
            out.push_back(t);
            i = j;
            continue;
        }

        if (c == QLatin1Char('#')) {
            static const QStringList kErrs = {QStringLiteral("#DIV/0!"), QStringLiteral("#N/A"),
                                              QStringLiteral("#NAME?"),  QStringLiteral("#NULL!"),
                                              QStringLiteral("#NUM!"),   QStringLiteral("#REF!"),
                                              QStringLiteral("#VALUE!"), QStringLiteral("#CYCLE!")};
            bool matched = false;
            for (const QString& e : kErrs) {
                if (s.mid(i, e.size()).compare(e, Qt::CaseInsensitive) == 0) {
                    t.kind = FeTok::Error;
                    t.text = e;
                    t.len = static_cast<int>(e.size());
                    out.push_back(t);
                    i += t.len;
                    matched = true;
                    break;
                }
            }
            if (!matched) {
                *error = QStringLiteral("Unknown error value");
                return {};
            }
            continue;
        }

        if (fe_is_ascii_digit(c) || (c == QLatin1Char('.') && i + 1 < n && fe_is_ascii_digit(s[i + 1]))) {
            int j = i;
            while (j < n && fe_is_ascii_digit(s[j]))
                ++j;
            bool integral = true;
            if (j < n && s[j] == QLatin1Char('.')) {
                integral = false;
                ++j;
                while (j < n && fe_is_ascii_digit(s[j]))
                    ++j;
            }
            if (j < n && (s[j] == QLatin1Char('e') || s[j] == QLatin1Char('E'))) {
                int k = j + 1;
                if (k < n && (s[k] == QLatin1Char('+') || s[k] == QLatin1Char('-')))
                    ++k;
                if (k < n && fe_is_ascii_digit(s[k])) {
                    integral = false;
                    j = k;
                    while (j < n && fe_is_ascii_digit(s[j]))
                        ++j;
                }
            }
            bool ok = false;
            t.number = s.mid(i, j - i).toDouble(&ok);
            if (!ok) {
                *error = QStringLiteral("Invalid number");
                return {};
            }
            t.kind = FeTok::Number;
            t.integral = integral;
            t.text = s.mid(i, j - i);
            t.len = j - i;
            out.push_back(t);
            i = j;
            continue;
        }

        if (c == QLatin1Char('\'')) { // 'Quoted Sheet'!A1
            QString name;
            int j = i + 1;
            bool closed = false;
            while (j < n) {
                if (s[j] == QLatin1Char('\'')) {
                    if (j + 1 < n && s[j + 1] == QLatin1Char('\'')) {
                        name += QLatin1Char('\'');
                        j += 2;
                        continue;
                    }
                    closed = true;
                    ++j;
                    break;
                }
                name += s[j++];
            }
            if (!closed || j >= n || s[j] != QLatin1Char('!')) {
                *error = QStringLiteral("Invalid sheet reference");
                return {};
            }
            ++j;
            if (!fe_lex_ref_after_sheet(s, j, name, t, error))
                return {};
            t.pos = i;
            t.len = j - i;
            out.push_back(t);
            i = j;
            continue;
        }

        if (c == QLatin1Char('$') || fe_is_ascii_letter(c) || c == QLatin1Char('_')) {
            int j = i;
            while (j < n && fe_word_char(s[j]))
                ++j;
            const QString w = s.mid(i, j - i);
            // Sheet prefix: "Sheet1!A1"
            if (j < n && s[j] == QLatin1Char('!') && !w.contains(QLatin1Char('$'))) {
                int k = j + 1;
                if (!fe_lex_ref_after_sheet(s, k, w, t, error))
                    return {};
                t.pos = i;
                t.len = k - i;
                out.push_back(t);
                i = k;
                continue;
            }
            const int next = fe_skip_spaces(s, j);
            const bool call = next < n && s[next] == QLatin1Char('(');
            FeToken probe;
            const int cls = call ? 0 : fe_classify_ref_word(w, probe);
            if (cls == 1) {
                probe.kind = FeTok::Ref;
                probe.pos = i;
                probe.len = j - i;
                out.push_back(probe);
            } else if (cls == 3) {
                probe.kind = FeTok::RowRef;
                probe.pos = i;
                probe.len = j - i;
                out.push_back(probe);
            } else if (!call && (w.compare(QLatin1String("TRUE"), Qt::CaseInsensitive) == 0 ||
                                 w.compare(QLatin1String("FALSE"), Qt::CaseInsensitive) == 0)) {
                t.kind = FeTok::Bool;
                t.boolean = w.compare(QLatin1String("TRUE"), Qt::CaseInsensitive) == 0;
                t.len = j - i;
                out.push_back(t);
            } else {
                if (w.contains(QLatin1Char('$')) && cls != 2) {
                    *error = QStringLiteral("Invalid reference '%1'").arg(w);
                    return {};
                }
                t.kind = FeTok::Ident;
                t.text = w.toUpper();
                t.len = j - i;
                if (cls == 2) { // spells a column — usable as "A:C"
                    t.col = probe.col;
                    t.abs_col = probe.abs_col;
                }
                out.push_back(t);
            }
            i = j;
            continue;
        }

        // Operators / punctuation
        if (c == QLatin1Char('(')) {
            t.kind = FeTok::LParen;
        } else if (c == QLatin1Char(')')) {
            t.kind = FeTok::RParen;
        } else if (c == QLatin1Char(',') || c == QLatin1Char(';')) {
            t.kind = FeTok::Sep;
        } else if (c == QLatin1Char(':')) {
            t.kind = FeTok::Colon;
        } else if (c == QLatin1Char('<') || c == QLatin1Char('>')) {
            t.kind = FeTok::Op;
            if (i + 1 < n && (s[i + 1] == QLatin1Char('=') || (c == QLatin1Char('<') && s[i + 1] == QLatin1Char('>')))) {
                t.text = s.mid(i, 2);
                t.len = 2;
                out.push_back(t);
                i += 2;
                continue;
            }
            t.text = QString(c);
        } else if (QStringLiteral("+-*/^&%=").contains(c)) {
            t.kind = FeTok::Op;
            t.text = QString(c);
        } else {
            *error = QStringLiteral("Unexpected character '%1'").arg(c);
            return {};
        }
        t.len = 1;
        out.push_back(t);
        ++i;
    }
    FeToken end;
    end.kind = FeTok::End;
    end.pos = n;
    out.push_back(end);
    return out;
}

} // namespace

// ═════════════════════════════════════════════════════════════════════════════
// AST + parser
// ═════════════════════════════════════════════════════════════════════════════

struct FeNode;
using FeNodePtr = std::unique_ptr<FeNode>;

struct FeNode {
    enum class K { Num, Str, Bool, Err, EmptyArg, Name, Ref, Range, Neg, Pct, Bin, Call };
    K k = K::Num;
    double num = 0.0;
    QString str; // Str payload / Err code / Name / Call function / Bin operator
    bool b = false;
    RangeRef range; // Ref (1×1) and Range
    std::vector<FeNodePtr> kids;
};

struct Program {
    FeNodePtr root;
};

namespace {

class FeParser {
  public:
    FeParser(const QVector<FeToken>& toks, QVector<RangeRef>* refs) : t_(toks), refs_(refs) {}

    FeNodePtr parse(QString* error) {
        FeNodePtr n = expr();
        if (n && cur().kind != FeTok::End)
            fail(QStringLiteral("Unexpected '%1'").arg(describe(cur())));
        if (!err_.isEmpty()) {
            *error = err_;
            return nullptr;
        }
        return n;
    }

  private:
    const FeToken& cur() const { return t_[std::min<qsizetype>(p_, t_.size() - 1)]; }
    const FeToken& at(int k) const { return t_[std::min<qsizetype>(p_ + k, t_.size() - 1)]; }
    bool is_op(const QString& op) const { return cur().kind == FeTok::Op && cur().text == op; }
    void fail(const QString& msg) {
        if (err_.isEmpty())
            err_ = msg;
    }
    static QString describe(const FeToken& t) {
        switch (t.kind) {
            case FeTok::End:
                return QStringLiteral("end of formula");
            case FeTok::RParen:
                return QStringLiteral(")");
            case FeTok::LParen:
                return QStringLiteral("(");
            case FeTok::Sep:
                return QStringLiteral(",");
            case FeTok::Colon:
                return QStringLiteral(":");
            default:
                return t.text.isEmpty() ? QStringLiteral("token") : t.text;
        }
    }

    FeNodePtr make(FeNode::K k) {
        auto n = std::make_unique<FeNode>();
        n->k = k;
        return n;
    }
    FeNodePtr bin(const QString& op, FeNodePtr l, FeNodePtr r) {
        auto n = make(FeNode::K::Bin);
        n->str = op;
        n->kids.push_back(std::move(l));
        n->kids.push_back(std::move(r));
        return n;
    }

    struct DepthGuard {
        int& d;
        explicit DepthGuard(int& depth) : d(depth) { ++d; }
        ~DepthGuard() { --d; }
    };

    FeNodePtr expr() {
        DepthGuard g(depth_);
        if (depth_ > kFeMaxDepth) {
            fail(QStringLiteral("Formula is nested too deeply"));
            return nullptr;
        }
        return compare();
    }

    FeNodePtr compare() {
        FeNodePtr l = concat();
        while (l && cur().kind == FeTok::Op &&
               (cur().text == QLatin1String("=") || cur().text == QLatin1String("<>") ||
                cur().text == QLatin1String("<") || cur().text == QLatin1String(">") ||
                cur().text == QLatin1String("<=") || cur().text == QLatin1String(">="))) {
            const QString op = cur().text;
            ++p_;
            FeNodePtr r = concat();
            if (!r)
                return nullptr;
            l = bin(op, std::move(l), std::move(r));
        }
        return l;
    }
    FeNodePtr concat() {
        FeNodePtr l = additive();
        while (l && is_op(QStringLiteral("&"))) {
            ++p_;
            FeNodePtr r = additive();
            if (!r)
                return nullptr;
            l = bin(QStringLiteral("&"), std::move(l), std::move(r));
        }
        return l;
    }
    FeNodePtr additive() {
        FeNodePtr l = multiplicative();
        while (l && (is_op(QStringLiteral("+")) || is_op(QStringLiteral("-")))) {
            const QString op = cur().text;
            ++p_;
            FeNodePtr r = multiplicative();
            if (!r)
                return nullptr;
            l = bin(op, std::move(l), std::move(r));
        }
        return l;
    }
    FeNodePtr multiplicative() {
        FeNodePtr l = power();
        while (l && (is_op(QStringLiteral("*")) || is_op(QStringLiteral("/")))) {
            const QString op = cur().text;
            ++p_;
            FeNodePtr r = power();
            if (!r)
                return nullptr;
            l = bin(op, std::move(l), std::move(r));
        }
        return l;
    }
    FeNodePtr power() { // left-associative, as in Excel: 2^3^2 = 64
        FeNodePtr l = percent();
        while (l && is_op(QStringLiteral("^"))) {
            ++p_;
            FeNodePtr r = percent();
            if (!r)
                return nullptr;
            l = bin(QStringLiteral("^"), std::move(l), std::move(r));
        }
        return l;
    }
    FeNodePtr percent() {
        FeNodePtr n = unary();
        while (n && is_op(QStringLiteral("%"))) {
            ++p_;
            auto pct = make(FeNode::K::Pct);
            pct->kids.push_back(std::move(n));
            n = std::move(pct);
        }
        return n;
    }
    FeNodePtr unary() {
        if (is_op(QStringLiteral("-")) || is_op(QStringLiteral("+"))) {
            const bool neg = cur().text == QLatin1String("-");
            ++p_;
            DepthGuard g(depth_);
            if (depth_ > kFeMaxDepth) {
                fail(QStringLiteral("Formula is nested too deeply"));
                return nullptr;
            }
            FeNodePtr inner = unary();
            if (!inner)
                return nullptr;
            if (!neg)
                return inner;
            auto n = make(FeNode::K::Neg);
            n->kids.push_back(std::move(inner));
            return n;
        }
        return primary();
    }

    FeNodePtr ref_node(const RangeRef& r, bool single) {
        auto n = make(single ? FeNode::K::Ref : FeNode::K::Range);
        n->range = r;
        if (refs_)
            refs_->push_back(r);
        return n;
    }

    // "1:3" / "$1:$3" / "Sheet!1:3"
    bool row_bound(const FeToken& t, int* row) const {
        if (t.kind == FeTok::RowRef) {
            *row = t.row;
            return true;
        }
        if (t.kind == FeTok::Number && t.integral && t.number >= 1 && t.number <= kFeMaxRows) {
            *row = static_cast<int>(t.number) - 1;
            return true;
        }
        return false;
    }
    // "A:C" / "$A:$C" / "Sheet!A:C"
    static bool col_bound(const FeToken& t, int* col) {
        if ((t.kind == FeTok::ColRef || t.kind == FeTok::Ident) && t.col >= 0) {
            *col = t.col;
            return true;
        }
        return false;
    }

    FeNodePtr primary() {
        const FeToken t = cur();
        switch (t.kind) {
            case FeTok::Number:
            case FeTok::RowRef: {
                int r1 = 0;
                int r2 = 0;
                if (at(1).kind == FeTok::Colon && row_bound(t, &r1) && row_bound(at(2), &r2)) {
                    p_ += 3;
                    RangeRef rr;
                    rr.sheet = t.sheet;
                    rr.row1 = std::min(r1, r2);
                    rr.row2 = std::max(r1, r2);
                    rr.col1 = 0;
                    rr.col2 = kFeMaxCols - 1;
                    rr.whole_row = true;
                    return ref_node(rr, false);
                }
                if (t.kind == FeTok::RowRef) {
                    fail(QStringLiteral("A row reference needs a range, e.g. 1:3"));
                    return nullptr;
                }
                ++p_;
                auto n = make(FeNode::K::Num);
                n->num = t.number;
                return n;
            }
            case FeTok::String: {
                ++p_;
                auto n = make(FeNode::K::Str);
                n->str = t.text;
                return n;
            }
            case FeTok::Bool: {
                ++p_;
                auto n = make(FeNode::K::Bool);
                n->b = t.boolean;
                return n;
            }
            case FeTok::Error: {
                ++p_;
                auto n = make(FeNode::K::Err);
                n->str = t.text;
                return n;
            }
            case FeTok::Ref: {
                ++p_;
                RangeRef rr;
                rr.sheet = t.sheet;
                rr.row1 = rr.row2 = t.row;
                rr.col1 = rr.col2 = t.col;
                if (cur().kind == FeTok::Colon) {
                    const FeToken& t2 = at(1);
                    if (t2.kind != FeTok::Ref || (t2.has_sheet && t2.sheet.compare(t.sheet, Qt::CaseInsensitive) != 0)) {
                        fail(QStringLiteral("Invalid range"));
                        return nullptr;
                    }
                    p_ += 2;
                    rr.row1 = std::min(t.row, t2.row);
                    rr.row2 = std::max(t.row, t2.row);
                    rr.col1 = std::min(t.col, t2.col);
                    rr.col2 = std::max(t.col, t2.col);
                    return ref_node(rr, rr.row1 == rr.row2 && rr.col1 == rr.col2);
                }
                return ref_node(rr, true);
            }
            case FeTok::ColRef:
            case FeTok::Ident: {
                int c1 = 0;
                int c2 = 0;
                if (at(1).kind == FeTok::Colon && col_bound(t, &c1) && col_bound(at(2), &c2)) {
                    p_ += 3;
                    RangeRef rr;
                    rr.sheet = t.sheet;
                    rr.col1 = std::min(c1, c2);
                    rr.col2 = std::max(c1, c2);
                    rr.row1 = 0;
                    rr.row2 = kFeMaxRows - 1;
                    rr.whole_col = true;
                    return ref_node(rr, false);
                }
                if (t.kind == FeTok::ColRef) {
                    fail(QStringLiteral("A column reference needs a range, e.g. A:C"));
                    return nullptr;
                }
                ++p_;
                if (cur().kind == FeTok::LParen) {
                    ++p_;
                    auto call = make(FeNode::K::Call);
                    call->str = t.text;
                    if (cur().kind == FeTok::RParen) {
                        ++p_;
                        return call;
                    }
                    while (true) {
                        if (cur().kind == FeTok::Sep || cur().kind == FeTok::RParen) {
                            call->kids.push_back(make(FeNode::K::EmptyArg)); // IF(A1,,0)
                        } else {
                            FeNodePtr a = expr();
                            if (!a)
                                return nullptr;
                            call->kids.push_back(std::move(a));
                        }
                        if (cur().kind == FeTok::Sep) {
                            ++p_;
                            continue;
                        }
                        if (cur().kind == FeTok::RParen) {
                            ++p_;
                            break;
                        }
                        fail(QStringLiteral("Expected ',' or ')' in %1(...)").arg(t.text));
                        return nullptr;
                    }
                    return call;
                }
                auto n = make(FeNode::K::Name);
                n->str = t.text;
                return n;
            }
            case FeTok::LParen: {
                ++p_;
                FeNodePtr e = expr();
                if (!e)
                    return nullptr;
                if (cur().kind != FeTok::RParen) {
                    fail(QStringLiteral("Missing ')'"));
                    return nullptr;
                }
                ++p_;
                return e;
            }
            default:
                fail(QStringLiteral("Unexpected '%1'").arg(describe(t)));
                return nullptr;
        }
    }

    const QVector<FeToken>& t_;
    QVector<RangeRef>* refs_;
    qsizetype p_ = 0;
    int depth_ = 0;
    QString err_;
};

QString fe_body(const QString& formula) {
    QString s = formula.trimmed();
    if (s.startsWith(QLatin1Char('=')))
        s = s.mid(1);
    return s;
}

} // namespace

Compiled compile(const QString& formula) {
    Compiled out;
    const QString body = fe_body(formula);
    if (body.trimmed().isEmpty()) {
        out.error = QStringLiteral("Empty formula");
        return out;
    }
    QString error;
    const QVector<FeToken> toks = fe_tokenize(body, &error);
    if (!error.isEmpty()) {
        out.error = error;
        return out;
    }
    FeParser parser(toks, &out.refs);
    auto root = parser.parse(&error);
    if (!root) {
        out.error = error.isEmpty() ? QStringLiteral("Invalid formula") : error;
        out.refs.clear();
        return out;
    }
    auto program = std::make_shared<Program>();
    program->root = std::move(root);
    out.program = std::move(program);
    return out;
}

// ═════════════════════════════════════════════════════════════════════════════
// Evaluation
// ═════════════════════════════════════════════════════════════════════════════

namespace {

class FeEval;
using FeFn = std::function<Value(FeEval&, const std::vector<FeNodePtr>&)>;

struct FeFnDef {
    int min_args = 0;
    int max_args = -1; // -1 = variadic
    FeFn fn;
};

const QHash<QString, FeFnDef>& fe_functions();

/// Expanded argument: a reference (with its shape) or a single computed value.
struct FeArg {
    bool is_ref = false;
    int rows = 1;
    int cols = 1;
    QVector<Value> values; // row-major
    const Value& at(int r, int c) const { return values[r * cols + c]; }
};

bool fe_to_number(const Value& v, double* out, Value* err) {
    switch (v.type) {
        case Value::Type::Empty:
            *out = 0.0;
            return true;
        case Value::Type::Number:
            *out = v.number;
            return true;
        case Value::Type::Bool:
            *out = v.boolean ? 1.0 : 0.0;
            return true;
        case Value::Type::Text:
            if (parse_number(v.text, out))
                return true;
            *err = Value::err(errors::value(), QStringLiteral("\"%1\" is not a number").arg(v.text.left(40)));
            return false;
        case Value::Type::Error:
            *err = v;
            return false;
    }
    return false;
}

bool fe_to_bool(const Value& v, bool* out, Value* err) {
    switch (v.type) {
        case Value::Type::Empty:
            *out = false;
            return true;
        case Value::Type::Number:
            *out = v.number != 0.0;
            return true;
        case Value::Type::Bool:
            *out = v.boolean;
            return true;
        case Value::Type::Text:
            if (v.text.compare(QLatin1String("TRUE"), Qt::CaseInsensitive) == 0) {
                *out = true;
                return true;
            }
            if (v.text.compare(QLatin1String("FALSE"), Qt::CaseInsensitive) == 0) {
                *out = false;
                return true;
            }
            *err = Value::err(errors::value(), QStringLiteral("\"%1\" is not TRUE/FALSE").arg(v.text.left(40)));
            return false;
        case Value::Type::Error:
            *err = v;
            return false;
    }
    return false;
}

bool fe_to_text(const Value& v, QString* out, Value* err) {
    if (v.is_error()) {
        *err = v;
        return false;
    }
    *out = display_text(v);
    return true;
}

// Excel ordering across types: numbers < text < booleans; text is case-insensitive.
int fe_type_rank(const Value& v) {
    switch (v.type) {
        case Value::Type::Text:
            return 1;
        case Value::Type::Bool:
            return 2;
        default:
            return 0;
    }
}

int fe_compare(Value a, Value b) {
    if (a.is_empty() && b.is_empty())
        return 0;
    if (a.is_empty())
        a = b.is_text() ? Value::str(QString()) : b.is_bool() ? Value::boolean_of(false) : Value::num(0);
    if (b.is_empty())
        b = a.is_text() ? Value::str(QString()) : a.is_bool() ? Value::boolean_of(false) : Value::num(0);
    const int ra = fe_type_rank(a);
    const int rb = fe_type_rank(b);
    if (ra != rb)
        return ra < rb ? -1 : 1;
    if (a.is_text())
        return a.text.compare(b.text, Qt::CaseInsensitive) < 0 ? -1 : (a.text.compare(b.text, Qt::CaseInsensitive) > 0 ? 1 : 0);
    if (a.is_bool())
        return a.boolean == b.boolean ? 0 : (a.boolean ? 1 : -1);
    if (fe_nearly_equal(a.number, b.number))
        return 0;
    return a.number < b.number ? -1 : 1;
}

QRegularExpression fe_wildcard(const QString& pattern) {
    // Excel wildcards: * any run, ? one char, ~ escapes the next char.
    QString rx = QStringLiteral("^");
    for (int i = 0; i < pattern.size(); ++i) {
        const QChar c = pattern[i];
        if (c == QLatin1Char('~') && i + 1 < pattern.size()) {
            rx += QRegularExpression::escape(QString(pattern[++i]));
        } else if (c == QLatin1Char('*')) {
            rx += QStringLiteral(".*");
        } else if (c == QLatin1Char('?')) {
            rx += QLatin1Char('.');
        } else {
            rx += QRegularExpression::escape(QString(c));
        }
    }
    rx += QLatin1Char('$');
    return QRegularExpression(rx, QRegularExpression::CaseInsensitiveOption |
                                      QRegularExpression::DotMatchesEverythingOption);
}

bool fe_has_wildcard(const QString& s) {
    return s.contains(QLatin1Char('*')) || s.contains(QLatin1Char('?'));
}

// Exact-match equality used by lookups and criteria ("=" semantics).
bool fe_lookup_equal(const Value& needle, const Value& cell) {
    if (needle.is_number() && cell.is_number())
        return fe_nearly_equal(needle.number, cell.number);
    if (needle.is_text() && cell.is_text()) {
        if (fe_has_wildcard(needle.text))
            return fe_wildcard(needle.text).match(cell.text).hasMatch();
        return needle.text.compare(cell.text, Qt::CaseInsensitive) == 0;
    }
    if (needle.is_bool() && cell.is_bool())
        return needle.boolean == cell.boolean;
    if (needle.is_empty())
        return cell.is_empty() || (cell.is_text() && cell.text.isEmpty());
    return false;
}

// COUNTIF / SUMIF / AVERAGEIF criteria.
struct FeCriterion {
    QString op = QStringLiteral("=");
    Value operand;
};

FeCriterion fe_parse_criterion(const Value& c) {
    FeCriterion cr;
    if (!c.is_text()) {
        cr.operand = c;
        return cr;
    }
    QString s = c.text;
    for (const char* op : {"<=", ">=", "<>", "<", ">", "="}) {
        if (s.startsWith(QLatin1String(op))) {
            cr.op = QLatin1String(op);
            s = s.mid(static_cast<int>(qstrlen(op)));
            break;
        }
    }
    double d = 0.0;
    if (parse_number(s, &d))
        cr.operand = Value::num(d);
    else if (s.compare(QLatin1String("TRUE"), Qt::CaseInsensitive) == 0)
        cr.operand = Value::boolean_of(true);
    else if (s.compare(QLatin1String("FALSE"), Qt::CaseInsensitive) == 0)
        cr.operand = Value::boolean_of(false);
    else if (s.isEmpty())
        cr.operand = Value::empty();
    else
        cr.operand = Value::str(s);
    return cr;
}

bool fe_criterion_match(const FeCriterion& cr, const Value& v) {
    if (v.is_error())
        return false;
    const Value& o = cr.operand;
    if (cr.op == QLatin1String("=") || cr.op == QLatin1String("<>")) {
        bool eq;
        if (o.is_empty()) {
            eq = v.is_empty() || (v.is_text() && v.text.isEmpty());
        } else if (o.is_number()) {
            double d = 0.0;
            eq = (v.is_number() && fe_nearly_equal(v.number, o.number)) ||
                 (v.is_text() && parse_number(v.text, &d) && fe_nearly_equal(d, o.number));
        } else {
            eq = fe_lookup_equal(o, v);
        }
        return cr.op == QLatin1String("=") ? eq : !eq;
    }
    // Ordering operators only compare like with like.
    if (o.is_number()) {
        if (!v.is_number())
            return false;
    } else if (o.is_text()) {
        if (!v.is_text())
            return false;
    } else {
        return false;
    }
    const int c = fe_compare(v, o);
    if (cr.op == QLatin1String("<"))
        return c < 0;
    if (cr.op == QLatin1String(">"))
        return c > 0;
    if (cr.op == QLatin1String("<="))
        return c <= 0;
    return c >= 0;
}

// Excel rounds at ~15 significant digits, so ROUND(1.005, 2) is 1.01 there
// even though the binary double is 1.00499999...; nudge by a few ulps.
double fe_nudge(double scaled, bool toward_zero) {
    const double eps = std::fabs(scaled) * 4.0 * DBL_EPSILON;
    if (toward_zero)
        return scaled > 0 ? scaled - eps : scaled + eps;
    return scaled > 0 ? scaled + eps : scaled - eps;
}

double fe_round(double x, int digits, int mode /*0 nearest, 1 up(away), 2 down(toward)*/) {
    const double p = std::pow(10.0, digits);
    if (!std::isfinite(p) || p == 0.0)
        return x;
    const double scaled = x * p;
    double r;
    if (mode == 0)
        r = std::round(fe_nudge(scaled, false)); // half away from zero
    else if (mode == 1)
        r = scaled >= 0 ? std::ceil(fe_nudge(scaled, true)) : std::floor(fe_nudge(scaled, true));
    else
        r = scaled >= 0 ? std::floor(fe_nudge(scaled, false)) : std::ceil(fe_nudge(scaled, false));
    return r / p;
}

class FeEval {
  public:
    explicit FeEval(EvalContext& ctx) : ctx_(ctx) {}

    Value eval(const FeNode& n) {
        switch (n.k) {
            case FeNode::K::Num:
                return Value::num(n.num);
            case FeNode::K::Str:
                return Value::str(n.str);
            case FeNode::K::Bool:
                return Value::boolean_of(n.b);
            case FeNode::K::Err:
                return Value::err(n.str);
            case FeNode::K::EmptyArg:
                return Value::empty();
            case FeNode::K::Name:
                return Value::err(errors::name(), QStringLiteral("Unknown name: %1").arg(n.str));
            case FeNode::K::Ref:
                return ctx_.cell(n.range.sheet, n.range.row1, n.range.col1);
            case FeNode::K::Range: {
                // A multi-cell range has no single value (Excel's implicit
                // intersection is not modelled).
                return Value::err(errors::value(), QStringLiteral("A range can't be used as a single value here"));
            }
            case FeNode::K::Neg: {
                const Value v = eval(*n.kids[0]);
                double d = 0.0;
                Value e;
                if (!fe_to_number(v, &d, &e))
                    return e;
                return fe_finite(-d);
            }
            case FeNode::K::Pct: {
                const Value v = eval(*n.kids[0]);
                double d = 0.0;
                Value e;
                if (!fe_to_number(v, &d, &e))
                    return e;
                return fe_finite(d / 100.0);
            }
            case FeNode::K::Bin:
                return binary(n);
            case FeNode::K::Call:
                return call(n);
        }
        return Value::err(errors::value());
    }

    /// Expand a node into an argument: references keep their shape; anything
    /// else is a single value.
    FeArg expand(const FeNode& n) {
        FeArg a;
        if (n.k != FeNode::K::Ref && n.k != FeNode::K::Range) {
            a.values.push_back(eval(n));
            return a;
        }
        a.is_ref = true;
        RangeRef r = n.range;
        if (r.whole_col) {
            const int rows = ctx_.row_count(r.sheet);
            if (rows < 0) {
                a.is_ref = false;
                a.values.push_back(Value::err(errors::ref(), QStringLiteral("Unknown sheet: %1").arg(r.sheet)));
                return a;
            }
            r.row1 = 0;
            r.row2 = std::max(0, rows - 1);
        }
        if (r.whole_row) {
            const int cols = ctx_.col_count(r.sheet);
            if (cols < 0) {
                a.is_ref = false;
                a.values.push_back(Value::err(errors::ref(), QStringLiteral("Unknown sheet: %1").arg(r.sheet)));
                return a;
            }
            r.col1 = 0;
            r.col2 = std::max(0, cols - 1);
        }
        const qint64 cells = static_cast<qint64>(r.row2 - r.row1 + 1) * (r.col2 - r.col1 + 1);
        if (cells > kFeMaxRangeCells) {
            a.is_ref = false;
            a.values.push_back(Value::err(errors::num(), QStringLiteral("Range is too large")));
            return a;
        }
        a.rows = r.row2 - r.row1 + 1;
        a.cols = r.col2 - r.col1 + 1;
        a.values.reserve(static_cast<qsizetype>(cells));
        for (int row = r.row1; row <= r.row2; ++row)
            for (int col = r.col1; col <= r.col2; ++col)
                a.values.push_back(ctx_.cell(r.sheet, row, col));
        return a;
    }

    // Aggregate semantics: references contribute only their numbers (text,
    // booleans and blanks are skipped); literal arguments are coerced. Any
    // error aborts. Returns false with *err set on error.
    bool numbers(const std::vector<FeNodePtr>& args, size_t first, QVector<double>& out, Value* err) {
        for (size_t i = first; i < args.size(); ++i) {
            const FeNode& node = *args[i];
            if (node.k == FeNode::K::Ref || node.k == FeNode::K::Range) {
                const FeArg a = expand(node);
                for (const Value& v : a.values) {
                    if (v.is_error()) {
                        *err = v;
                        return false;
                    }
                    if (v.is_number())
                        out.push_back(v.number);
                }
            } else {
                const Value v = eval(node);
                if (v.is_error()) {
                    *err = v;
                    return false;
                }
                if (v.is_empty())
                    continue;
                double d = 0.0;
                if (!fe_to_number(v, &d, err))
                    return false;
                out.push_back(d);
            }
        }
        return true;
    }

    bool number_arg(const FeNode& node, double* out, Value* err) { return fe_to_number(eval(node), out, err); }

    bool optional_number(const std::vector<FeNodePtr>& args, size_t i, double fallback, double* out, Value* err) {
        if (i >= args.size() || args[i]->k == FeNode::K::EmptyArg) {
            *out = fallback;
            return true;
        }
        return number_arg(*args[i], out, err);
    }

    EvalContext& ctx() { return ctx_; }

  private:
    Value binary(const FeNode& n) {
        const QString& op = n.str;
        const Value l = eval(*n.kids[0]);
        const Value r = eval(*n.kids[1]);

        if (op == QLatin1String("&")) {
            QString a;
            QString b;
            Value e;
            if (!fe_to_text(l, &a, &e) || !fe_to_text(r, &b, &e))
                return e;
            return Value::str(a + b);
        }
        if (op == QLatin1String("=") || op == QLatin1String("<>") || op == QLatin1String("<") ||
            op == QLatin1String(">") || op == QLatin1String("<=") || op == QLatin1String(">=")) {
            if (l.is_error())
                return l;
            if (r.is_error())
                return r;
            const int c = fe_compare(l, r);
            bool res = false;
            if (op == QLatin1String("="))
                res = c == 0;
            else if (op == QLatin1String("<>"))
                res = c != 0;
            else if (op == QLatin1String("<"))
                res = c < 0;
            else if (op == QLatin1String(">"))
                res = c > 0;
            else if (op == QLatin1String("<="))
                res = c <= 0;
            else
                res = c >= 0;
            return Value::boolean_of(res);
        }

        double a = 0.0;
        double b = 0.0;
        Value e;
        if (!fe_to_number(l, &a, &e) || !fe_to_number(r, &b, &e))
            return e;
        if (op == QLatin1String("+"))
            return fe_finite(a + b);
        if (op == QLatin1String("-"))
            return fe_finite(a - b);
        if (op == QLatin1String("*"))
            return fe_finite(a * b);
        if (op == QLatin1String("/")) {
            if (b == 0.0)
                return Value::err(errors::div0(), QStringLiteral("Division by zero"));
            return fe_finite(a / b);
        }
        if (op == QLatin1String("^")) {
            if (a == 0.0 && b < 0.0)
                return Value::err(errors::div0(), QStringLiteral("Zero raised to a negative power"));
            if (a < 0.0 && std::floor(b) != b)
                return Value::err(errors::num(), QStringLiteral("Negative number raised to a fractional power"));
            return fe_finite(std::pow(a, b));
        }
        return Value::err(errors::value());
    }

    Value call(const FeNode& n) {
        const auto& table = fe_functions();
        const auto it = table.constFind(n.str);
        if (it == table.constEnd())
            return Value::err(errors::name(), QStringLiteral("Unknown function: %1").arg(n.str));
        const int argc = static_cast<int>(n.kids.size());
        if (argc < it->min_args || (it->max_args >= 0 && argc > it->max_args))
            return Value::err(errors::value(),
                              QStringLiteral("%1 takes %2 argument(s), got %3")
                                  .arg(n.str)
                                  .arg(it->max_args == it->min_args ? QString::number(it->min_args)
                                       : it->max_args < 0
                                           ? QStringLiteral("at least %1").arg(it->min_args)
                                           : QStringLiteral("%1-%2").arg(it->min_args).arg(it->max_args))
                                  .arg(argc));
        return it->fn(*this, n.kids);
    }

    EvalContext& ctx_;
};

// ── Function helpers ─────────────────────────────────────────────────────────

using FeArgs = std::vector<FeNodePtr>;

Value fe_aggregate(FeEval& ev, const FeArgs& a, const std::function<Value(const QVector<double>&)>& fn) {
    QVector<double> xs;
    Value e;
    if (!ev.numbers(a, 0, xs, &e))
        return e;
    return fn(xs);
}

Value fe_variance(const QVector<double>& xs, bool sample, bool sqrt_result) {
    const qsizetype n = xs.size();
    if (n < (sample ? 2 : 1))
        return Value::err(errors::div0(), QStringLiteral("Not enough numbers"));
    double mean = 0.0;
    for (double x : xs)
        mean += x;
    mean /= static_cast<double>(n);
    double ss = 0.0;
    for (double x : xs)
        ss += (x - mean) * (x - mean);
    const double var = ss / static_cast<double>(sample ? n - 1 : n);
    return fe_finite(sqrt_result ? std::sqrt(var) : var);
}

Value fe_math1(FeEval& ev, const FeArgs& a, const std::function<Value(double)>& fn) {
    double x = 0.0;
    Value e;
    if (!ev.number_arg(*a[0], &x, &e))
        return e;
    return fn(x);
}

Value fe_round_fn(FeEval& ev, const FeArgs& a, int mode) {
    double x = 0.0;
    double d = 0.0;
    Value e;
    if (!ev.number_arg(*a[0], &x, &e) || !ev.optional_number(a, 1, 0.0, &d, &e))
        return e;
    return fe_finite(fe_round(x, static_cast<int>(std::trunc(d)), mode));
}

// Values of a 1-D or 2-D reference argument, numbers only (cash-flow series).
bool fe_series(FeEval& ev, const FeNode& node, QVector<double>& out, Value* err) {
    const FeArg a = ev.expand(node);
    for (const Value& v : a.values) {
        if (v.is_error()) {
            *err = v;
            return false;
        }
        if (v.is_number())
            out.push_back(v.number);
        else if (!a.is_ref && !v.is_empty()) {
            double d = 0.0;
            if (!fe_to_number(v, &d, err))
                return false;
            out.push_back(d);
        }
    }
    return true;
}

double fe_npv(double rate, const QVector<double>& flows, int first_exponent) {
    double npv = 0.0;
    for (qsizetype i = 0; i < flows.size(); ++i)
        npv += flows[i] / std::pow(1.0 + rate, static_cast<double>(i + first_exponent));
    return npv;
}

// Root of f on [lo, hi] by bisection once a sign change is bracketed.
bool fe_bisect(const std::function<double(double)>& f, double lo, double hi, double* root) {
    double flo = f(lo);
    double fhi = f(hi);
    if (!std::isfinite(flo) || !std::isfinite(fhi) || (flo > 0) == (fhi > 0))
        return false;
    for (int i = 0; i < 300; ++i) {
        const double mid = 0.5 * (lo + hi);
        const double fm = f(mid);
        if (!std::isfinite(fm))
            return false;
        if (std::fabs(fm) < 1e-10 || (hi - lo) < 1e-14) {
            *root = mid;
            return true;
        }
        if ((fm > 0) == (flo > 0)) {
            lo = mid;
            flo = fm;
        } else {
            hi = mid;
        }
    }
    *root = 0.5 * (lo + hi);
    return true;
}

// Newton from `guess`, falling back to a bracketed bisection over [-0.9999, 100].
bool fe_solve_rate(const std::function<double(double)>& f, double guess, double* root) {
    double r = guess;
    for (int i = 0; i < 100; ++i) {
        const double fr = f(r);
        if (!std::isfinite(fr))
            break;
        if (std::fabs(fr) < 1e-10) {
            *root = r;
            return true;
        }
        const double h = 1e-7 * std::max(1.0, std::fabs(r));
        const double d = (f(r + h) - fr) / h;
        if (!std::isfinite(d) || d == 0.0)
            break;
        const double next = r - fr / d;
        if (!std::isfinite(next) || next <= -1.0)
            break;
        if (std::fabs(next - r) < 1e-12) {
            *root = next;
            return true;
        }
        r = next;
    }
    // Scan for a sign change, then bisect.
    const double grid[] = {-0.9999, -0.99, -0.9, -0.75, -0.5, -0.25, -0.1, 0.0,  0.05, 0.1,
                           0.2,     0.3,   0.5,  0.75,  1.0,  2.0,   5.0,  10.0, 50.0, 100.0};
    for (size_t i = 0; i + 1 < sizeof(grid) / sizeof(grid[0]); ++i) {
        if (fe_bisect(f, grid[i], grid[i + 1], root))
            return true;
    }
    return false;
}

// Lookup in a 1-D vector for MATCH / approximate VLOOKUP.
// type 0 = exact, 1 = largest <= needle (ascending), -1 = smallest >= needle (descending).
int fe_match(const QVector<Value>& vec, const Value& needle, int type) {
    if (type == 0) {
        for (qsizetype i = 0; i < vec.size(); ++i)
            if (fe_lookup_equal(needle, vec[i]))
                return static_cast<int>(i);
        return -1;
    }
    int best = -1;
    for (qsizetype i = 0; i < vec.size(); ++i) {
        const Value& v = vec[i];
        if (v.is_empty() || v.is_error() || fe_type_rank(v) != fe_type_rank(needle))
            continue;
        const int c = fe_compare(v, needle);
        if (type > 0) {
            if (c <= 0)
                best = static_cast<int>(i);
            else
                break; // sorted ascending: nothing later can qualify
        } else {
            if (c >= 0)
                best = static_cast<int>(i);
            else
                break;
        }
    }
    return best;
}

Value fe_lookup_result(const Value& v) {
    return v.is_empty() ? Value::num(0) : v; // Excel returns 0 for a blank hit
}

Value fe_vhlookup(FeEval& ev, const FeArgs& a, bool vertical) {
    const Value needle = ev.eval(*a[0]);
    if (needle.is_error())
        return needle;
    if (a[1]->k != FeNode::K::Range && a[1]->k != FeNode::K::Ref)
        return Value::err(errors::value(), QStringLiteral("The lookup table must be a range"));
    const FeArg table = ev.expand(*a[1]);
    double idx = 0.0;
    Value e;
    if (!ev.number_arg(*a[2], &idx, &e))
        return e;
    bool approx = true;
    if (a.size() > 3 && a[3]->k != FeNode::K::EmptyArg && !fe_to_bool(ev.eval(*a[3]), &approx, &e))
        return e;
    const int k = static_cast<int>(std::trunc(idx));
    if (k < 1)
        return Value::err(errors::value(), QStringLiteral("Index must be at least 1"));
    if (k > (vertical ? table.cols : table.rows))
        return Value::err(errors::ref(), QStringLiteral("Index is outside the lookup table"));
    QVector<Value> keys;
    const int n = vertical ? table.rows : table.cols;
    keys.reserve(n);
    for (int i = 0; i < n; ++i)
        keys.push_back(vertical ? table.at(i, 0) : table.at(0, i));
    const int hit = fe_match(keys, needle, approx ? 1 : 0);
    if (hit < 0)
        return Value::err(errors::na(), QStringLiteral("No match for %1").arg(display_text(needle)));
    return fe_lookup_result(vertical ? table.at(hit, k - 1) : table.at(k - 1, hit));
}

Value fe_conditional(FeEval& ev, const FeArgs& a, int mode /*0 count,1 sum,2 avg*/) {
    if (a[0]->k != FeNode::K::Range && a[0]->k != FeNode::K::Ref)
        return Value::err(errors::value(), QStringLiteral("The criteria range must be a range"));
    const FeArg range = ev.expand(*a[0]);
    const Value crit_v = ev.eval(*a[1]);
    if (crit_v.is_error())
        return crit_v;
    const FeCriterion cr = fe_parse_criterion(crit_v);

    FeArg values;
    const bool has_sum = mode != 0 && a.size() > 2 && a[2]->k != FeNode::K::EmptyArg;
    if (has_sum) {
        if (a[2]->k != FeNode::K::Range && a[2]->k != FeNode::K::Ref)
            return Value::err(errors::value(), QStringLiteral("The sum range must be a range"));
        // Excel: same size and shape as the criteria range, anchored at the
        // sum range's top-left cell (whole-row/column ranges are used as given).
        FeNode anchored;
        anchored.k = FeNode::K::Range;
        anchored.range = a[2]->range;
        if (!a[0]->range.whole_col && !a[0]->range.whole_row && !anchored.range.whole_col &&
            !anchored.range.whole_row) {
            anchored.range.row2 = anchored.range.row1 + range.rows - 1;
            anchored.range.col2 = anchored.range.col1 + range.cols - 1;
        }
        values = ev.expand(anchored);
    }

    double sum = 0.0;
    int count = 0;
    for (qsizetype i = 0; i < range.values.size(); ++i) {
        if (!fe_criterion_match(cr, range.values[i]))
            continue;
        if (mode == 0) {
            ++count;
            continue;
        }
        const Value v = has_sum ? (i < values.values.size() ? values.values[i] : Value()) : range.values[i];
        if (v.is_error())
            return v;
        if (v.is_number()) {
            sum += v.number;
            ++count;
        }
    }
    if (mode == 0)
        return Value::num(count);
    if (mode == 1)
        return fe_finite(sum);
    if (count == 0)
        return Value::err(errors::div0(), QStringLiteral("No cells matched the criteria"));
    return fe_finite(sum / count);
}

bool fe_dated_series(FeEval& ev, const FeArgs& a, size_t vi, size_t di, QVector<double>& flows,
                     QVector<double>& dates, Value* err) {
    const FeArg v = ev.expand(*a[vi]);
    const FeArg d = ev.expand(*a[di]);
    if (v.values.size() != d.values.size()) {
        *err = Value::err(errors::num(), QStringLiteral("Values and dates must be the same size"));
        return false;
    }
    for (qsizetype i = 0; i < v.values.size(); ++i) {
        const Value& fv = v.values[i];
        const Value& dv = d.values[i];
        if (fv.is_error()) {
            *err = fv;
            return false;
        }
        if (dv.is_error()) {
            *err = dv;
            return false;
        }
        if (fv.is_empty() && dv.is_empty())
            continue;
        double x = 0.0;
        if (!fe_to_number(fv, &x, err))
            return false;
        double serial = 0.0;
        if (!date_serial(dv, &serial)) {
            *err = Value::err(errors::value(), QStringLiteral("\"%1\" is not a date").arg(display_text(dv)));
            return false;
        }
        flows.push_back(x);
        dates.push_back(serial);
    }
    if (flows.isEmpty()) {
        *err = Value::err(errors::num(), QStringLiteral("No cash flows"));
        return false;
    }
    for (double dt : dates) {
        if (dt < dates.front()) {
            *err = Value::err(errors::num(), QStringLiteral("A date precedes the first date"));
            return false;
        }
    }
    return true;
}

double fe_xnpv(double rate, const QVector<double>& flows, const QVector<double>& dates) {
    double s = 0.0;
    for (qsizetype i = 0; i < flows.size(); ++i)
        s += flows[i] / std::pow(1.0 + rate, (dates[i] - dates[0]) / 365.0);
    return s;
}

bool fe_has_sign_change(const QVector<double>& flows) {
    bool pos = false;
    bool neg = false;
    for (double f : flows) {
        pos = pos || f > 0;
        neg = neg || f < 0;
    }
    return pos && neg;
}

// Time-value-of-money arguments shared by PMT/PV/FV/NPER/RATE.
bool fe_tvm_args(FeEval& ev, const FeArgs& a, double* v0, double* v1, double* v2, double* v3, double* v4,
                 Value* e) {
    if (!ev.number_arg(*a[0], v0, e) || !ev.number_arg(*a[1], v1, e) || !ev.number_arg(*a[2], v2, e) ||
        !ev.optional_number(a, 3, 0.0, v3, e) || !ev.optional_number(a, 4, 0.0, v4, e))
        return false;
    return true;
}

const QHash<QString, FeFnDef>& fe_functions() {
    static const QHash<QString, FeFnDef> table = [] {
        QHash<QString, FeFnDef> t;

        // ── Aggregates ──────────────────────────────────────────────────────
        t.insert(QStringLiteral("SUM"), {1, -1, [](FeEval& ev, const FeArgs& a) {
                     return fe_aggregate(ev, a, [](const QVector<double>& xs) {
                         double s = 0.0;
                         for (double x : xs)
                             s += x;
                         return fe_finite(s);
                     });
                 }});
        const FeFnDef average{1, -1, [](FeEval& ev, const FeArgs& a) {
                                  return fe_aggregate(ev, a, [](const QVector<double>& xs) {
                                      if (xs.isEmpty())
                                          return Value::err(errors::div0(), QStringLiteral("No numbers to average"));
                                      double s = 0.0;
                                      for (double x : xs)
                                          s += x;
                                      return fe_finite(s / static_cast<double>(xs.size()));
                                  });
                              }};
        t.insert(QStringLiteral("AVERAGE"), average);
        t.insert(QStringLiteral("AVG"), average); // engine alias — exported as AVERAGE
        t.insert(QStringLiteral("MIN"), {1, -1, [](FeEval& ev, const FeArgs& a) {
                     return fe_aggregate(ev, a, [](const QVector<double>& xs) {
                         return xs.isEmpty() ? Value::num(0) : Value::num(*std::min_element(xs.begin(), xs.end()));
                     });
                 }});
        t.insert(QStringLiteral("MAX"), {1, -1, [](FeEval& ev, const FeArgs& a) {
                     return fe_aggregate(ev, a, [](const QVector<double>& xs) {
                         return xs.isEmpty() ? Value::num(0) : Value::num(*std::max_element(xs.begin(), xs.end()));
                     });
                 }});
        t.insert(QStringLiteral("PRODUCT"), {1, -1, [](FeEval& ev, const FeArgs& a) {
                     return fe_aggregate(ev, a, [](const QVector<double>& xs) {
                         if (xs.isEmpty())
                             return Value::num(0);
                         double p = 1.0;
                         for (double x : xs)
                             p *= x;
                         return fe_finite(p);
                     });
                 }});
        t.insert(QStringLiteral("MEDIAN"), {1, -1, [](FeEval& ev, const FeArgs& a) {
                     return fe_aggregate(ev, a, [](QVector<double> xs) {
                         if (xs.isEmpty())
                             return Value::err(errors::num(), QStringLiteral("No numbers"));
                         std::sort(xs.begin(), xs.end());
                         const qsizetype n = xs.size();
                         return Value::num(n % 2 ? xs[n / 2] : 0.5 * (xs[n / 2 - 1] + xs[n / 2]));
                     });
                 }});
        auto var_fn = [](bool sample, bool root) {
            return FeFnDef{1, -1, [sample, root](FeEval& ev, const FeArgs& a) {
                               return fe_aggregate(ev, a, [sample, root](const QVector<double>& xs) {
                                   return fe_variance(xs, sample, root);
                               });
                           }};
        };
        t.insert(QStringLiteral("STDEV"), var_fn(true, true));
        t.insert(QStringLiteral("STDEV.S"), var_fn(true, true));
        t.insert(QStringLiteral("STDEVP"), var_fn(false, true));
        t.insert(QStringLiteral("STDEV.P"), var_fn(false, true));
        t.insert(QStringLiteral("VAR"), var_fn(true, false));
        t.insert(QStringLiteral("VAR.S"), var_fn(true, false));
        t.insert(QStringLiteral("VARP"), var_fn(false, false));
        t.insert(QStringLiteral("VAR.P"), var_fn(false, false));
        auto kth = [](bool largest) {
            return FeFnDef{2, 2, [largest](FeEval& ev, const FeArgs& a) {
                               QVector<double> xs;
                               Value e;
                               if (!fe_series(ev, *a[0], xs, &e))
                                   return e;
                               double k = 0.0;
                               if (!ev.number_arg(*a[1], &k, &e))
                                   return e;
                               const int ki = static_cast<int>(std::ceil(k - 1e-9));
                               if (ki < 1 || ki > xs.size())
                                   return Value::err(errors::num(), QStringLiteral("k is out of range"));
                               std::sort(xs.begin(), xs.end());
                               return Value::num(largest ? xs[xs.size() - ki] : xs[ki - 1]);
                           }};
        };
        t.insert(QStringLiteral("LARGE"), kth(true));
        t.insert(QStringLiteral("SMALL"), kth(false));
        t.insert(QStringLiteral("COUNT"), {1, -1, [](FeEval& ev, const FeArgs& a) {
                     int n = 0;
                     for (const auto& node : a) {
                         if (node->k == FeNode::K::Ref || node->k == FeNode::K::Range) {
                             for (const Value& v : ev.expand(*node).values)
                                 n += v.is_number() ? 1 : 0;
                         } else {
                             const Value v = ev.eval(*node);
                             double d = 0.0;
                             if (v.is_number() || v.is_bool() || (v.is_text() && parse_number(v.text, &d)))
                                 ++n;
                         }
                     }
                     return Value::num(n);
                 }});
        t.insert(QStringLiteral("COUNTA"), {1, -1, [](FeEval& ev, const FeArgs& a) {
                     int n = 0;
                     for (const auto& node : a) {
                         if (node->k == FeNode::K::EmptyArg)
                             continue;
                         for (const Value& v : ev.expand(*node).values)
                             n += v.is_empty() ? 0 : 1;
                     }
                     return Value::num(n);
                 }});
        t.insert(QStringLiteral("COUNTBLANK"), {1, 1, [](FeEval& ev, const FeArgs& a) {
                     int n = 0;
                     for (const Value& v : ev.expand(*a[0]).values)
                         n += (v.is_empty() || (v.is_text() && v.text.isEmpty())) ? 1 : 0;
                     return Value::num(n);
                 }});
        t.insert(QStringLiteral("COUNTIF"),
                 {2, 2, [](FeEval& ev, const FeArgs& a) { return fe_conditional(ev, a, 0); }});
        t.insert(QStringLiteral("SUMIF"), {2, 3, [](FeEval& ev, const FeArgs& a) { return fe_conditional(ev, a, 1); }});
        t.insert(QStringLiteral("AVERAGEIF"),
                 {2, 3, [](FeEval& ev, const FeArgs& a) { return fe_conditional(ev, a, 2); }});
        t.insert(QStringLiteral("SUMPRODUCT"), {1, -1, [](FeEval& ev, const FeArgs& a) {
                     QVector<FeArg> arrays;
                     for (const auto& node : a)
                         arrays.push_back(ev.expand(*node));
                     const qsizetype n = arrays[0].values.size();
                     for (const FeArg& arr : arrays)
                         if (arr.values.size() != n || arr.rows != arrays[0].rows)
                             return Value::err(errors::value(), QStringLiteral("Ranges must be the same size"));
                     double s = 0.0;
                     for (qsizetype i = 0; i < n; ++i) {
                         double p = 1.0;
                         for (const FeArg& arr : arrays) {
                             const Value& v = arr.values[i];
                             if (v.is_error())
                                 return v;
                             p *= v.is_number() ? v.number : 0.0;
                         }
                         s += p;
                     }
                     return fe_finite(s);
                 }});
        t.insert(QStringLiteral("CORREL"), {2, 2, [](FeEval& ev, const FeArgs& a) {
                     const FeArg x = ev.expand(*a[0]);
                     const FeArg y = ev.expand(*a[1]);
                     if (x.values.size() != y.values.size())
                         return Value::err(errors::na(), QStringLiteral("Ranges must be the same size"));
                     QVector<double> xs;
                     QVector<double> ys;
                     for (qsizetype i = 0; i < x.values.size(); ++i) {
                         if (x.values[i].is_error())
                             return x.values[i];
                         if (y.values[i].is_error())
                             return y.values[i];
                         if (x.values[i].is_number() && y.values[i].is_number()) {
                             xs.push_back(x.values[i].number);
                             ys.push_back(y.values[i].number);
                         }
                     }
                     if (xs.size() < 2)
                         return Value::err(errors::div0(), QStringLiteral("Not enough paired numbers"));
                     double mx = 0.0;
                     double my = 0.0;
                     for (qsizetype i = 0; i < xs.size(); ++i) {
                         mx += xs[i];
                         my += ys[i];
                     }
                     mx /= static_cast<double>(xs.size());
                     my /= static_cast<double>(ys.size());
                     double sxy = 0.0;
                     double sxx = 0.0;
                     double syy = 0.0;
                     for (qsizetype i = 0; i < xs.size(); ++i) {
                         sxy += (xs[i] - mx) * (ys[i] - my);
                         sxx += (xs[i] - mx) * (xs[i] - mx);
                         syy += (ys[i] - my) * (ys[i] - my);
                     }
                     if (sxx == 0.0 || syy == 0.0)
                         return Value::err(errors::div0(), QStringLiteral("A series has zero variance"));
                     return fe_finite(sxy / std::sqrt(sxx * syy));
                 }});

        // ── Math ────────────────────────────────────────────────────────────
        t.insert(QStringLiteral("ABS"), {1, 1, [](FeEval& ev, const FeArgs& a) {
                     return fe_math1(ev, a, [](double x) { return fe_finite(std::fabs(x)); });
                 }});
        t.insert(QStringLiteral("ROUND"), {1, 2, [](FeEval& ev, const FeArgs& a) { return fe_round_fn(ev, a, 0); }});
        t.insert(QStringLiteral("ROUNDUP"), {1, 2, [](FeEval& ev, const FeArgs& a) { return fe_round_fn(ev, a, 1); }});
        t.insert(QStringLiteral("ROUNDDOWN"),
                 {1, 2, [](FeEval& ev, const FeArgs& a) { return fe_round_fn(ev, a, 2); }});
        t.insert(QStringLiteral("TRUNC"), {1, 2, [](FeEval& ev, const FeArgs& a) { return fe_round_fn(ev, a, 2); }});
        t.insert(QStringLiteral("INT"), {1, 1, [](FeEval& ev, const FeArgs& a) {
                     return fe_math1(ev, a, [](double x) { return fe_finite(std::floor(x)); });
                 }});
        t.insert(QStringLiteral("SIGN"), {1, 1, [](FeEval& ev, const FeArgs& a) {
                     return fe_math1(ev, a, [](double x) { return Value::num(x > 0 ? 1 : x < 0 ? -1 : 0); });
                 }});
        t.insert(QStringLiteral("SQRT"), {1, 1, [](FeEval& ev, const FeArgs& a) {
                     return fe_math1(ev, a, [](double x) {
                         return x < 0 ? Value::err(errors::num(), QStringLiteral("Square root of a negative number"))
                                      : fe_finite(std::sqrt(x));
                     });
                 }});
        t.insert(QStringLiteral("EXP"), {1, 1, [](FeEval& ev, const FeArgs& a) {
                     return fe_math1(ev, a, [](double x) { return fe_finite(std::exp(x)); });
                 }});
        t.insert(QStringLiteral("LN"), {1, 1, [](FeEval& ev, const FeArgs& a) {
                     return fe_math1(ev, a, [](double x) {
                         return x <= 0 ? Value::err(errors::num(), QStringLiteral("Logarithm of a non-positive number"))
                                       : fe_finite(std::log(x));
                     });
                 }});
        t.insert(QStringLiteral("LOG10"), {1, 1, [](FeEval& ev, const FeArgs& a) {
                     return fe_math1(ev, a, [](double x) {
                         return x <= 0 ? Value::err(errors::num(), QStringLiteral("Logarithm of a non-positive number"))
                                       : fe_finite(std::log10(x));
                     });
                 }});
        t.insert(QStringLiteral("LOG"), {1, 2, [](FeEval& ev, const FeArgs& a) {
                     double x = 0.0;
                     double base = 10.0;
                     Value e;
                     if (!ev.number_arg(*a[0], &x, &e) || !ev.optional_number(a, 1, 10.0, &base, &e))
                         return e;
                     if (x <= 0 || base <= 0 || base == 1.0)
                         return Value::err(errors::num(), QStringLiteral("Invalid logarithm argument"));
                     return fe_finite(std::log(x) / std::log(base));
                 }});
        t.insert(QStringLiteral("POWER"), {2, 2, [](FeEval& ev, const FeArgs& a) {
                     double x = 0.0;
                     double y = 0.0;
                     Value e;
                     if (!ev.number_arg(*a[0], &x, &e) || !ev.number_arg(*a[1], &y, &e))
                         return e;
                     if (x == 0.0 && y < 0.0)
                         return Value::err(errors::div0(), QStringLiteral("Zero raised to a negative power"));
                     if (x < 0.0 && std::floor(y) != y)
                         return Value::err(errors::num(), QStringLiteral("Negative number raised to a fractional power"));
                     return fe_finite(std::pow(x, y));
                 }});
        t.insert(QStringLiteral("MOD"), {2, 2, [](FeEval& ev, const FeArgs& a) {
                     double n = 0.0;
                     double d = 0.0;
                     Value e;
                     if (!ev.number_arg(*a[0], &n, &e) || !ev.number_arg(*a[1], &d, &e))
                         return e;
                     if (d == 0.0)
                         return Value::err(errors::div0(), QStringLiteral("MOD by zero"));
                     return fe_finite(n - d * std::floor(n / d)); // sign follows the divisor
                 }});
        t.insert(QStringLiteral("PI"), {0, 0, [](FeEval&, const FeArgs&) { return Value::num(3.14159265358979323846); }});

        // ── Logical ─────────────────────────────────────────────────────────
        t.insert(QStringLiteral("IF"), {1, 3, [](FeEval& ev, const FeArgs& a) {
                     bool c = false;
                     Value e;
                     if (!fe_to_bool(ev.eval(*a[0]), &c, &e))
                         return e;
                     const size_t branch = c ? 1 : 2;
                     if (branch >= a.size())
                         return c ? Value::boolean_of(true) : Value::boolean_of(false);
                     const Value v = ev.eval(*a[branch]); // only the taken branch is evaluated
                     return a[branch]->k == FeNode::K::EmptyArg ? Value::num(0) : v;
                 }});
        t.insert(QStringLiteral("IFERROR"), {2, 2, [](FeEval& ev, const FeArgs& a) {
                     const Value v = ev.eval(*a[0]);
                     return v.is_error() ? ev.eval(*a[1]) : v;
                 }});
        t.insert(QStringLiteral("IFNA"), {2, 2, [](FeEval& ev, const FeArgs& a) {
                     const Value v = ev.eval(*a[0]);
                     return (v.is_error() && v.text == errors::na()) ? ev.eval(*a[1]) : v;
                 }});
        auto andor = [](bool is_and) {
            return FeFnDef{1, -1, [is_and](FeEval& ev, const FeArgs& a) {
                               bool any = false;
                               bool acc = is_and;
                               for (const auto& node : a) {
                                   const FeArg arg = ev.expand(*node);
                                   for (const Value& v : arg.values) {
                                       if (v.is_error())
                                           return v;
                                       bool b = false;
                                       if (v.is_bool() || v.is_number()) {
                                           b = v.is_bool() ? v.boolean : v.number != 0.0;
                                       } else if (!arg.is_ref && !v.is_empty()) {
                                           Value e;
                                           if (!fe_to_bool(v, &b, &e))
                                               return e;
                                       } else {
                                           continue; // text / blanks inside references are ignored
                                       }
                                       any = true;
                                       acc = is_and ? (acc && b) : (acc || b);
                                   }
                               }
                               if (!any)
                                   return Value::err(errors::value(), QStringLiteral("No logical values"));
                               return Value::boolean_of(acc);
                           }};
        };
        t.insert(QStringLiteral("AND"), andor(true));
        t.insert(QStringLiteral("OR"), andor(false));
        t.insert(QStringLiteral("NOT"), {1, 1, [](FeEval& ev, const FeArgs& a) {
                     bool b = false;
                     Value e;
                     if (!fe_to_bool(ev.eval(*a[0]), &b, &e))
                         return e;
                     return Value::boolean_of(!b);
                 }});
        t.insert(QStringLiteral("TRUE"), {0, 0, [](FeEval&, const FeArgs&) { return Value::boolean_of(true); }});
        t.insert(QStringLiteral("FALSE"), {0, 0, [](FeEval&, const FeArgs&) { return Value::boolean_of(false); }});

        // ── Lookup ──────────────────────────────────────────────────────────
        t.insert(QStringLiteral("VLOOKUP"),
                 {3, 4, [](FeEval& ev, const FeArgs& a) { return fe_vhlookup(ev, a, true); }});
        t.insert(QStringLiteral("HLOOKUP"),
                 {3, 4, [](FeEval& ev, const FeArgs& a) { return fe_vhlookup(ev, a, false); }});
        t.insert(QStringLiteral("MATCH"), {2, 3, [](FeEval& ev, const FeArgs& a) {
                     const Value needle = ev.eval(*a[0]);
                     if (needle.is_error())
                         return needle;
                     const FeArg range = ev.expand(*a[1]);
                     if (range.rows != 1 && range.cols != 1)
                         return Value::err(errors::na(), QStringLiteral("MATCH needs a single row or column"));
                     double type = 1.0;
                     Value e;
                     if (!ev.optional_number(a, 2, 1.0, &type, &e))
                         return e;
                     const int hit = fe_match(range.values, needle, type > 0 ? 1 : type < 0 ? -1 : 0);
                     if (hit < 0)
                         return Value::err(errors::na(), QStringLiteral("No match for %1").arg(display_text(needle)));
                     return Value::num(hit + 1);
                 }});
        t.insert(QStringLiteral("INDEX"), {2, 3, [](FeEval& ev, const FeArgs& a) {
                     const FeArg range = ev.expand(*a[0]);
                     double r = 0.0;
                     double c = 0.0;
                     Value e;
                     if (!ev.number_arg(*a[1], &r, &e) || !ev.optional_number(a, 2, 0.0, &c, &e))
                         return e;
                     int ri = static_cast<int>(std::trunc(r));
                     int ci = static_cast<int>(std::trunc(c));
                     if (a.size() < 3 || a[2]->k == FeNode::K::EmptyArg) {
                         if (range.rows == 1) { // a single row: the one index addresses columns
                             ci = ri;
                             ri = 1;
                         } else {
                             ci = 1;
                         }
                     }
                     if (ri < 1 || ci < 1 || ri > range.rows || ci > range.cols)
                         return Value::err(errors::ref(), QStringLiteral("Index is outside the range"));
                     return fe_lookup_result(range.at(ri - 1, ci - 1));
                 }});

        // ── Financial ───────────────────────────────────────────────────────
        t.insert(QStringLiteral("NPV"), {2, -1, [](FeEval& ev, const FeArgs& a) {
                     double rate = 0.0;
                     Value e;
                     if (!ev.number_arg(*a[0], &rate, &e))
                         return e;
                     QVector<double> flows;
                     if (!ev.numbers(a, 1, flows, &e))
                         return e;
                     if (rate == -1.0)
                         return Value::err(errors::div0(), QStringLiteral("Rate of -100%"));
                     return fe_finite(fe_npv(rate, flows, 1));
                 }});
        t.insert(QStringLiteral("IRR"), {1, 2, [](FeEval& ev, const FeArgs& a) {
                     QVector<double> flows;
                     Value e;
                     if (!fe_series(ev, *a[0], flows, &e))
                         return e;
                     double guess = 0.1;
                     if (!ev.optional_number(a, 1, 0.1, &guess, &e))
                         return e;
                     if (!fe_has_sign_change(flows))
                         return Value::err(errors::num(), QStringLiteral("IRR needs both positive and negative cash flows"));
                     double r = 0.0;
                     if (!fe_solve_rate([&flows](double x) { return fe_npv(x, flows, 0); }, guess, &r))
                         return Value::err(errors::num(), QStringLiteral("IRR did not converge"));
                     return fe_finite(r);
                 }});
        t.insert(QStringLiteral("XNPV"), {3, 3, [](FeEval& ev, const FeArgs& a) {
                     double rate = 0.0;
                     Value e;
                     if (!ev.number_arg(*a[0], &rate, &e))
                         return e;
                     QVector<double> flows;
                     QVector<double> dates;
                     if (!fe_dated_series(ev, a, 1, 2, flows, dates, &e))
                         return e;
                     if (rate <= -1.0)
                         return Value::err(errors::num(), QStringLiteral("Rate must be greater than -100%"));
                     return fe_finite(fe_xnpv(rate, flows, dates));
                 }});
        t.insert(QStringLiteral("XIRR"), {2, 3, [](FeEval& ev, const FeArgs& a) {
                     QVector<double> flows;
                     QVector<double> dates;
                     Value e;
                     if (!fe_dated_series(ev, a, 0, 1, flows, dates, &e))
                         return e;
                     double guess = 0.1;
                     if (!ev.optional_number(a, 2, 0.1, &guess, &e))
                         return e;
                     if (!fe_has_sign_change(flows))
                         return Value::err(errors::num(), QStringLiteral("XIRR needs both positive and negative cash flows"));
                     double r = 0.0;
                     if (!fe_solve_rate([&](double x) { return fe_xnpv(x, flows, dates); }, guess, &r))
                         return Value::err(errors::num(), QStringLiteral("XIRR did not converge"));
                     return fe_finite(r);
                 }});
        t.insert(QStringLiteral("PMT"), {3, 5, [](FeEval& ev, const FeArgs& a) {
                     double rate, nper, pv, fv, type;
                     Value e;
                     if (!fe_tvm_args(ev, a, &rate, &nper, &pv, &fv, &type, &e))
                         return e;
                     if (nper == 0.0)
                         return Value::err(errors::num(), QStringLiteral("Number of periods is zero"));
                     if (rate == 0.0)
                         return fe_finite(-(pv + fv) / nper);
                     const double f = std::pow(1.0 + rate, nper);
                     return fe_finite(-(rate * (fv + pv * f)) / ((1.0 + rate * (type != 0 ? 1 : 0)) * (f - 1.0)));
                 }});
        t.insert(QStringLiteral("PV"), {3, 5, [](FeEval& ev, const FeArgs& a) {
                     double rate, nper, pmt, fv, type;
                     Value e;
                     if (!fe_tvm_args(ev, a, &rate, &nper, &pmt, &fv, &type, &e))
                         return e;
                     if (rate == 0.0)
                         return fe_finite(-(fv + pmt * nper));
                     const double f = std::pow(1.0 + rate, nper);
                     return fe_finite(-(fv + pmt * (1.0 + rate * (type != 0 ? 1 : 0)) * (f - 1.0) / rate) / f);
                 }});
        t.insert(QStringLiteral("FV"), {3, 5, [](FeEval& ev, const FeArgs& a) {
                     double rate, nper, pmt, pv, type;
                     Value e;
                     if (!fe_tvm_args(ev, a, &rate, &nper, &pmt, &pv, &type, &e))
                         return e;
                     if (rate == 0.0)
                         return fe_finite(-(pv + pmt * nper));
                     const double f = std::pow(1.0 + rate, nper);
                     return fe_finite(-(pv * f + pmt * (1.0 + rate * (type != 0 ? 1 : 0)) * (f - 1.0) / rate));
                 }});
        t.insert(QStringLiteral("NPER"), {3, 5, [](FeEval& ev, const FeArgs& a) {
                     double rate, pmt, pv, fv, type;
                     Value e;
                     if (!fe_tvm_args(ev, a, &rate, &pmt, &pv, &fv, &type, &e))
                         return e;
                     if (rate == 0.0) {
                         if (pmt == 0.0)
                             return Value::err(errors::num(), QStringLiteral("Payment is zero"));
                         return fe_finite(-(pv + fv) / pmt);
                     }
                     const double k = pmt * (1.0 + rate * (type != 0 ? 1 : 0));
                     const double num = k - fv * rate;
                     const double den = k + pv * rate;
                     if (num / den <= 0.0)
                         return Value::err(errors::num(), QStringLiteral("No number of periods solves these inputs"));
                     return fe_finite(std::log(num / den) / std::log(1.0 + rate));
                 }});
        t.insert(QStringLiteral("RATE"), {3, 6, [](FeEval& ev, const FeArgs& a) {
                     double nper, pmt, pv, fv, type, guess;
                     Value e;
                     if (!fe_tvm_args(ev, a, &nper, &pmt, &pv, &fv, &type, &e) ||
                         !ev.optional_number(a, 5, 0.1, &guess, &e))
                         return e;
                     const double t1 = type != 0 ? 1.0 : 0.0;
                     auto f = [=](double r) {
                         if (std::fabs(r) < 1e-12)
                             return pv + pmt * nper + fv;
                         const double g = std::pow(1.0 + r, nper);
                         return pv * g + pmt * (1.0 + r * t1) * (g - 1.0) / r + fv;
                     };
                     double r = 0.0;
                     if (!fe_solve_rate(f, guess, &r))
                         return Value::err(errors::num(), QStringLiteral("RATE did not converge"));
                     return fe_finite(r);
                 }});

        // ── Text ────────────────────────────────────────────────────────────
        auto concat_fn = FeFnDef{1, -1, [](FeEval& ev, const FeArgs& a) {
                                     QString out;
                                     for (const auto& node : a) {
                                         for (const Value& v : ev.expand(*node).values) {
                                             if (v.is_error())
                                                 return v;
                                             out += display_text(v);
                                         }
                                     }
                                     return Value::str(out);
                                 }};
        t.insert(QStringLiteral("CONCAT"), concat_fn);
        t.insert(QStringLiteral("CONCATENATE"), concat_fn);
        auto text1 = [](std::function<Value(const QString&)> fn) {
            return FeFnDef{1, 1, [fn](FeEval& ev, const FeArgs& a) {
                               QString s;
                               Value e;
                               if (!fe_to_text(ev.eval(*a[0]), &s, &e))
                                   return e;
                               return fn(s);
                           }};
        };
        t.insert(QStringLiteral("LEN"), text1([](const QString& s) { return Value::num(static_cast<double>(s.size())); }));
        t.insert(QStringLiteral("UPPER"), text1([](const QString& s) { return Value::str(s.toUpper()); }));
        t.insert(QStringLiteral("LOWER"), text1([](const QString& s) { return Value::str(s.toLower()); }));
        t.insert(QStringLiteral("TRIM"), text1([](const QString& s) { return Value::str(s.simplified()); }));
        t.insert(QStringLiteral("VALUE"), text1([](const QString& s) {
                     double d = 0.0;
                     if (parse_number(s, &d))
                         return Value::num(d);
                     return Value::err(errors::value(), QStringLiteral("\"%1\" is not a number").arg(s.left(40)));
                 }));
        auto side = [](bool left) {
            return FeFnDef{1, 2, [left](FeEval& ev, const FeArgs& a) {
                               QString s;
                               double n = 1.0;
                               Value e;
                               if (!fe_to_text(ev.eval(*a[0]), &s, &e) || !ev.optional_number(a, 1, 1.0, &n, &e))
                                   return e;
                               if (n < 0)
                                   return Value::err(errors::value(), QStringLiteral("Length can't be negative"));
                               const int k = static_cast<int>(std::trunc(n));
                               return Value::str(left ? s.left(k) : s.right(k));
                           }};
        };
        t.insert(QStringLiteral("LEFT"), side(true));
        t.insert(QStringLiteral("RIGHT"), side(false));
        t.insert(QStringLiteral("MID"), {3, 3, [](FeEval& ev, const FeArgs& a) {
                     QString s;
                     double start = 0.0;
                     double n = 0.0;
                     Value e;
                     if (!fe_to_text(ev.eval(*a[0]), &s, &e) || !ev.number_arg(*a[1], &start, &e) ||
                         !ev.number_arg(*a[2], &n, &e))
                         return e;
                     if (start < 1 || n < 0)
                         return Value::err(errors::value(), QStringLiteral("Invalid start or length"));
                     return Value::str(s.mid(static_cast<int>(start) - 1, static_cast<int>(n)));
                 }});

        // ── Information ─────────────────────────────────────────────────────
        auto is_fn = [](std::function<bool(const Value&)> pred) {
            return FeFnDef{1, 1, [pred](FeEval& ev, const FeArgs& a) {
                               return Value::boolean_of(pred(ev.eval(*a[0])));
                           }};
        };
        t.insert(QStringLiteral("ISBLANK"), is_fn([](const Value& v) { return v.is_empty(); }));
        t.insert(QStringLiteral("ISNUMBER"), is_fn([](const Value& v) { return v.is_number(); }));
        t.insert(QStringLiteral("ISTEXT"), is_fn([](const Value& v) { return v.is_text(); }));
        t.insert(QStringLiteral("ISERROR"), is_fn([](const Value& v) { return v.is_error(); }));
        t.insert(QStringLiteral("ISNA"), is_fn([](const Value& v) { return v.is_error() && v.text == errors::na(); }));
        t.insert(QStringLiteral("NA"), {0, 0, [](FeEval&, const FeArgs&) { return Value::err(errors::na()); }});
        return t;
    }();
    return table;
}

} // namespace

Value evaluate(const Compiled& compiled, EvalContext& ctx) {
    if (!compiled.program || !compiled.program->root)
        return Value::err(errors::syntax(), compiled.error.isEmpty() ? QStringLiteral("Invalid formula") : compiled.error);
    FeEval ev(ctx);
    Value v = ev.eval(*compiled.program->root);
    if (v.is_number() && !std::isfinite(v.number))
        return Value::err(errors::num(), QStringLiteral("The result is not a finite number"));
    return v;
}

QStringList supported_functions() {
    QStringList names = fe_functions().keys();
    names.sort();
    return names;
}

// ═════════════════════════════════════════════════════════════════════════════
// Reference rewriting
// ═════════════════════════════════════════════════════════════════════════════

namespace {

QString fe_quote_sheet(const QString& sheet) {
    static const QRegularExpression kPlain(QStringLiteral("^[A-Za-z_][A-Za-z0-9_.]*$"));
    if (kPlain.match(sheet).hasMatch() && column_index(sheet) < 0)
        return sheet;
    QString q = sheet;
    q.replace(QLatin1Char('\''), QStringLiteral("''"));
    return QLatin1Char('\'') + q + QLatin1Char('\'');
}

QString fe_sheet_prefix(const FeToken& t) {
    return t.has_sheet ? fe_quote_sheet(t.sheet) + QLatin1Char('!') : QString();
}

QString fe_cell_text(const FeToken& t, int row, int col, bool with_sheet) {
    return (with_sheet ? fe_sheet_prefix(t) : QString()) + (t.abs_col ? QStringLiteral("$") : QString()) +
           column_label(col) + (t.abs_row ? QStringLiteral("$") : QString()) + QString::number(row + 1);
}

QString fe_col_text(const FeToken& t, int col, bool with_sheet) {
    return (with_sheet ? fe_sheet_prefix(t) : QString()) + (t.abs_col ? QStringLiteral("$") : QString()) +
           column_label(col);
}

QString fe_row_text(const FeToken& t, int row, bool with_sheet) {
    return (with_sheet ? fe_sheet_prefix(t) : QString()) + (t.abs_row ? QStringLiteral("$") : QString()) +
           QString::number(row + 1);
}

/// One reference "unit" in a token stream: a single cell, a cell range, a
/// whole-column range or a whole-row range.
struct FeUnit {
    enum class Kind { Cell, Range, Cols, Rows };
    Kind kind = Kind::Cell;
    int first = 0; // token index of the first part
    int last = 0;  // token index of the last part
};

bool fe_row_token(const FeToken& t) {
    return t.kind == FeTok::RowRef || (t.kind == FeTok::Number && t.integral && t.number >= 1);
}
bool fe_col_token(const FeToken& t) {
    return (t.kind == FeTok::ColRef || t.kind == FeTok::Ident) && t.col >= 0;
}

QVector<FeUnit> fe_units(const QVector<FeToken>& toks) {
    QVector<FeUnit> units;
    for (int i = 0; i < toks.size(); ++i) {
        const FeToken& t = toks[i];
        const bool colon_next = i + 2 < toks.size() && toks[i + 1].kind == FeTok::Colon;
        if (t.kind == FeTok::Ref) {
            if (colon_next && toks[i + 2].kind == FeTok::Ref) {
                units.push_back({FeUnit::Kind::Range, i, i + 2});
                i += 2;
            } else {
                units.push_back({FeUnit::Kind::Cell, i, i});
            }
        } else if (colon_next && fe_col_token(t) && fe_col_token(toks[i + 2])) {
            units.push_back({FeUnit::Kind::Cols, i, i + 2});
            i += 2;
        } else if (colon_next && fe_row_token(t) && fe_row_token(toks[i + 2])) {
            units.push_back({FeUnit::Kind::Rows, i, i + 2});
            i += 2;
        }
    }
    return units;
}

int fe_row_of(const FeToken& t) {
    return t.kind == FeTok::Number ? static_cast<int>(t.number) - 1 : t.row;
}

/// Splice replacements into `body`; `repl` maps token-span starts to text.
QString fe_splice(const QString& body, const QVector<FeToken>& toks, const QVector<QPair<FeUnit, QString>>& repl) {
    QString out;
    int cursor = 0;
    for (const auto& [u, text] : repl) {
        const int start = toks[u.first].pos;
        const int end = toks[u.last].pos + toks[u.last].len;
        out += body.mid(cursor, start - cursor);
        out += text;
        cursor = end;
    }
    out += body.mid(cursor);
    return out;
}

// Adjust one row/col index for an insert/delete. Returns false when the
// index falls inside a deleted block.
bool fe_adjust_index(int& idx, int at, int count) {
    if (count > 0) {
        if (idx >= at)
            idx += count;
        return true;
    }
    const int n = -count;
    if (idx >= at && idx < at + n)
        return false;
    if (idx >= at + n)
        idx -= n;
    return true;
}

// Adjust a span [lo, hi] (inclusive). Returns false when it vanishes.
bool fe_adjust_span(int& lo, int& hi, int at, int count) {
    if (count > 0) {
        if (lo >= at)
            lo += count;
        if (hi >= at)
            hi += count;
        return true;
    }
    const int n = -count;
    const int end = at + n; // first index after the deleted block
    const int nlo = lo < at ? lo : (lo < end ? at : lo - n);
    const int nhi = hi < at ? hi : (hi < end ? at - 1 : hi - n);
    if (nlo > nhi)
        return false;
    lo = nlo;
    hi = nhi;
    return true;
}

} // namespace

QString shift_references(const QString& formula, int drow, int dcol) {
    const QString body = fe_body(formula);
    QString error;
    const QVector<FeToken> toks = fe_tokenize(body, &error);
    if (!error.isEmpty())
        return QLatin1Char('=') + body;
    QVector<QPair<FeUnit, QString>> repl;
    for (const FeUnit& u : fe_units(toks)) {
        const FeToken& a = toks[u.first];
        const FeToken& b = toks[u.last];
        auto mv_row = [drow](const FeToken& t, int row) { return t.abs_row ? row : row + drow; };
        auto mv_col = [dcol](const FeToken& t, int col) { return t.abs_col ? col : col + dcol; };
        QString text;
        bool ok = true;
        switch (u.kind) {
            case FeUnit::Kind::Cell: {
                const int r = mv_row(a, a.row);
                const int c = mv_col(a, a.col);
                ok = r >= 0 && c >= 0 && r < kFeMaxRows && c < kFeMaxCols;
                text = fe_cell_text(a, r, c, true);
                break;
            }
            case FeUnit::Kind::Range: {
                const int r1 = mv_row(a, a.row);
                const int c1 = mv_col(a, a.col);
                const int r2 = mv_row(b, b.row);
                const int c2 = mv_col(b, b.col);
                ok = std::min(r1, r2) >= 0 && std::min(c1, c2) >= 0 && std::max(r1, r2) < kFeMaxRows &&
                     std::max(c1, c2) < kFeMaxCols;
                text = fe_cell_text(a, r1, c1, true) + QLatin1Char(':') + fe_cell_text(b, r2, c2, b.has_sheet);
                break;
            }
            case FeUnit::Kind::Cols: {
                const int c1 = mv_col(a, a.col);
                const int c2 = mv_col(b, b.col);
                ok = std::min(c1, c2) >= 0 && std::max(c1, c2) < kFeMaxCols;
                text = fe_col_text(a, c1, true) + QLatin1Char(':') + fe_col_text(b, c2, b.has_sheet);
                break;
            }
            case FeUnit::Kind::Rows: {
                const int r1 = mv_row(a, fe_row_of(a));
                const int r2 = mv_row(b, fe_row_of(b));
                ok = std::min(r1, r2) >= 0 && std::max(r1, r2) < kFeMaxRows;
                text = fe_row_text(a, r1, true) + QLatin1Char(':') + fe_row_text(b, r2, b.has_sheet);
                break;
            }
        }
        repl.push_back({u, ok ? text : errors::ref()});
    }
    return QLatin1Char('=') + fe_splice(body, toks, repl);
}

QString adjust_for_structure(const QString& formula, const QString& formula_sheet, const QString& target_sheet,
                             Axis axis, int at, int count, bool* changed) {
    if (changed)
        *changed = false;
    const QString body = fe_body(formula);
    if (count == 0)
        return QLatin1Char('=') + body;
    QString error;
    const QVector<FeToken> toks = fe_tokenize(body, &error);
    if (!error.isEmpty())
        return QLatin1Char('=') + body;

    QVector<QPair<FeUnit, QString>> repl;
    for (const FeUnit& u : fe_units(toks)) {
        const FeToken& a = toks[u.first];
        const FeToken& b = toks[u.last];
        const QString& sheet = a.has_sheet ? a.sheet : formula_sheet;
        if (sheet.compare(target_sheet, Qt::CaseInsensitive) != 0)
            continue;
        QString text;
        bool ok = true;
        switch (u.kind) {
            case FeUnit::Kind::Cell: {
                int r = a.row;
                int c = a.col;
                ok = axis == Axis::Rows ? fe_adjust_index(r, at, count) : fe_adjust_index(c, at, count);
                if (r == a.row && c == a.col && ok)
                    continue;
                text = fe_cell_text(a, r, c, true);
                break;
            }
            case FeUnit::Kind::Range: {
                int r1 = std::min(a.row, b.row);
                int r2 = std::max(a.row, b.row);
                int c1 = std::min(a.col, b.col);
                int c2 = std::max(a.col, b.col);
                ok = axis == Axis::Rows ? fe_adjust_span(r1, r2, at, count) : fe_adjust_span(c1, c2, at, count);
                if (ok && r1 == std::min(a.row, b.row) && r2 == std::max(a.row, b.row) &&
                    c1 == std::min(a.col, b.col) && c2 == std::max(a.col, b.col))
                    continue;
                text = fe_cell_text(a, r1, c1, true) + QLatin1Char(':') + fe_cell_text(b, r2, c2, b.has_sheet);
                break;
            }
            case FeUnit::Kind::Cols: {
                if (axis != Axis::Cols)
                    continue;
                int c1 = std::min(a.col, b.col);
                int c2 = std::max(a.col, b.col);
                ok = fe_adjust_span(c1, c2, at, count);
                if (ok && c1 == std::min(a.col, b.col) && c2 == std::max(a.col, b.col))
                    continue;
                text = fe_col_text(a, c1, true) + QLatin1Char(':') + fe_col_text(b, c2, b.has_sheet);
                break;
            }
            case FeUnit::Kind::Rows: {
                if (axis != Axis::Rows)
                    continue;
                const int a_row = fe_row_of(a);
                const int b_row = fe_row_of(b);
                int r1 = std::min(a_row, b_row);
                int r2 = std::max(a_row, b_row);
                ok = fe_adjust_span(r1, r2, at, count);
                if (ok && r1 == std::min(a_row, b_row) && r2 == std::max(a_row, b_row))
                    continue;
                text = fe_row_text(a, r1, true) + QLatin1Char(':') + fe_row_text(b, r2, b.has_sheet);
                break;
            }
        }
        repl.push_back({u, ok ? text : errors::ref()});
    }
    if (repl.isEmpty())
        return QLatin1Char('=') + body;
    if (changed)
        *changed = true;
    return QLatin1Char('=') + fe_splice(body, toks, repl);
}

QString rename_sheet_references(const QString& formula, const QString& old_name, const QString& new_name,
                                bool* changed) {
    if (changed)
        *changed = false;
    const QString body = fe_body(formula);
    QString error;
    const QVector<FeToken> toks = fe_tokenize(body, &error);
    if (!error.isEmpty())
        return QLatin1Char('=') + body;
    QString out;
    int cursor = 0;
    for (const FeToken& t : toks) {
        if (!t.has_sheet || t.sheet.compare(old_name, Qt::CaseInsensitive) != 0)
            continue;
        FeToken renamed = t;
        renamed.sheet = new_name;
        QString text;
        if (t.kind == FeTok::Ref)
            text = fe_cell_text(renamed, t.row, t.col, true);
        else if (t.kind == FeTok::ColRef)
            text = fe_col_text(renamed, t.col, true);
        else if (t.kind == FeTok::RowRef)
            text = fe_row_text(renamed, t.row, true);
        else
            continue;
        out += body.mid(cursor, t.pos - cursor);
        out += text;
        cursor = t.pos + t.len;
        if (changed)
            *changed = true;
    }
    out += body.mid(cursor);
    return QLatin1Char('=') + out;
}

QString to_excel(const QString& formula) {
    const QString body = fe_body(formula);
    QString error;
    const QVector<FeToken> toks = fe_tokenize(body, &error);
    if (!error.isEmpty())
        return QLatin1Char('=') + body;
    QString out;
    int cursor = 0;
    for (int i = 0; i + 1 < toks.size(); ++i) {
        const FeToken& t = toks[i];
        if (t.kind == FeTok::Ident && t.text == QLatin1String("AVG") && toks[i + 1].kind == FeTok::LParen) {
            out += body.mid(cursor, t.pos - cursor);
            out += QStringLiteral("AVERAGE");
            cursor = t.pos + t.len;
        }
    }
    out += body.mid(cursor);
    return QLatin1Char('=') + out;
}

} // namespace fincept::screens::formula
