#include "services/workflow/ExpressionEngine.h"

#include "services/workflow/Extensions.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QRegularExpression>
#include <QStringList>

#include <cmath>

namespace fincept::workflow {

namespace {

// ── Parsing helpers (file-unique names: this TU is concatenated into a unity batch) ──

// Split `s` at every top-level `sep` — i.e. not inside quotes, (), [] or {}.
QStringList expr_eng_split_top(const QString& s, QChar sep) {
    QStringList parts;
    QString cur;
    int depth = 0;
    QChar quote;
    for (int i = 0; i < s.size(); ++i) {
        const QChar c = s[i];
        if (!quote.isNull()) {
            cur += c;
            if (c == quote)
                quote = QChar();
            continue;
        }
        if (c == QLatin1Char('"') || c == QLatin1Char('\'')) {
            quote = c;
            cur += c;
        } else if (c == QLatin1Char('(') || c == QLatin1Char('[') || c == QLatin1Char('{')) {
            ++depth;
            cur += c;
        } else if (c == QLatin1Char(')') || c == QLatin1Char(']') || c == QLatin1Char('}')) {
            if (depth > 0)
                --depth;
            cur += c;
        } else if (c == sep && depth == 0) {
            parts << cur;
            cur.clear();
        } else {
            cur += c;
        }
    }
    parts << cur;
    return parts;
}

// Same as above for a two-character separator ("&&", "||").
QStringList expr_eng_split_logic(const QString& s, const QString& sep) {
    QStringList parts;
    int depth = 0;
    QChar quote;
    int start = 0;
    for (int i = 0; i < s.size(); ++i) {
        const QChar c = s[i];
        if (!quote.isNull()) {
            if (c == quote)
                quote = QChar();
            continue;
        }
        if (c == QLatin1Char('"') || c == QLatin1Char('\'')) {
            quote = c;
        } else if (c == QLatin1Char('(') || c == QLatin1Char('[') || c == QLatin1Char('{')) {
            ++depth;
        } else if (c == QLatin1Char(')') || c == QLatin1Char(']') || c == QLatin1Char('}')) {
            if (depth > 0)
                --depth;
        } else if (depth == 0 && s.mid(i, sep.size()) == sep) {
            parts << s.mid(start, i - start);
            i += sep.size() - 1;
            start = i + 1;
        }
    }
    parts << s.mid(start);
    return parts;
}

// "No such key / index" — distinct from a JSON null that IS present in the data.
QJsonValue expr_eng_undefined() {
    return QJsonValue(QJsonValue::Undefined);
}

bool expr_eng_is_quoted(const QString& t) {
    return t.size() >= 2 && ((t.startsWith(QLatin1Char('"')) && t.endsWith(QLatin1Char('"'))) ||
                             (t.startsWith(QLatin1Char('\'')) && t.endsWith(QLatin1Char('\''))));
}

// A literal argument inside `name(...)`: quoted string, number, true/false, else the raw text.
QJsonValue expr_eng_parse_arg(const QString& text) {
    const QString t = text.trimmed();
    if (t.isEmpty())
        return QJsonValue();
    if (expr_eng_is_quoted(t))
        return t.mid(1, t.size() - 2);
    if (t == QLatin1String("true"))
        return true;
    if (t == QLatin1String("false"))
        return false;
    bool ok = false;
    const double d = t.toDouble(&ok);
    if (ok)
        return d;
    return t;
}

// One lookup step: object key, or array index (negative counts from the end).
QJsonValue expr_eng_lookup(const QJsonValue& v, const QString& key) {
    if (v.isObject())
        return v.toObject().value(key);
    if (v.isArray()) {
        bool ok = false;
        int idx = key.toInt(&ok);
        const QJsonArray arr = v.toArray();
        if (ok && idx < 0)
            idx += arr.size();
        if (ok && idx >= 0 && idx < arr.size())
            return arr.at(idx);
    }
    return expr_eng_undefined(); // not found
}

// Numbers, and strings that are plainly numeric ("12.5", "-3"), count as numbers when comparing.
bool expr_eng_to_number(const QJsonValue& v, double* out) {
    if (v.isDouble()) {
        *out = v.toDouble();
        return true;
    }
    if (v.isString()) {
        const QString s = v.toString().trimmed();
        if (s.isEmpty())
            return false;
        const QChar first = s[0];
        if (!(first.isDigit() || first == QLatin1Char('-') || first == QLatin1Char('+') || first == QLatin1Char('.')))
            return false; // keeps "inf" / "nan" / "e5" from parsing as numbers
        bool ok = false;
        const double d = s.toDouble(&ok);
        if (ok && std::isfinite(d)) {
            *out = d;
            return true;
        }
    }
    return false;
}

bool expr_eng_truthy(const QJsonValue& v) {
    if (v.isBool())
        return v.toBool();
    if (v.isDouble())
        return v.toDouble() != 0.0;
    if (v.isString())
        return !v.toString().isEmpty() && v.toString() != QLatin1String("false");
    return !v.isNull() && !v.isUndefined();
}

// Evaluate one side of a comparison.
QJsonValue expr_eng_operand(const QString& text, const QJsonValue& root, bool allow_bare_word) {
    QString t = text.trimmed();
    if (t.isEmpty())
        return expr_eng_undefined(); // `> 5` / `price >` — nothing to compare
    if (expr_eng_is_quoted(t))
        return t.mid(1, t.size() - 2);
    if (t.startsWith(QLatin1String("{{")) && t.endsWith(QLatin1String("}}")))
        t = t.mid(2, t.size() - 4).trimmed();
    if (t == QLatin1String("true"))
        return true;
    if (t == QLatin1String("false"))
        return false;
    if (t == QLatin1String("null"))
        return QJsonValue::Null;

    double d = 0;
    if (!t.isEmpty() && expr_eng_to_number(QJsonValue(t), &d))
        return d;

    const QJsonValue resolved = ExpressionEngine::resolve_path(root, t);
    if (!resolved.isUndefined())
        return resolved;
    if (allow_bare_word && !t.isEmpty() && !t.startsWith(QLatin1Char('$')))
        return t; // `status == active`
    return expr_eng_undefined();
}

bool expr_eng_compare(const QJsonValue& l, const QString& op, const QJsonValue& r) {
    // Text operators — case-insensitive, because the usual subject is free text
    // (an LLM answer, a headline) whose casing nobody controls.
    if (op == QLatin1String("contains")) {
        if (l.isArray()) {
            for (const QJsonValue& item : l.toArray()) {
                if (expr_eng_compare(item, QStringLiteral("=="), r))
                    return true;
            }
            return false;
        }
        return ExpressionEngine::value_to_string(l).contains(ExpressionEngine::value_to_string(r),
                                                             Qt::CaseInsensitive);
    }
    if (op == QLatin1String("startsWith"))
        return ExpressionEngine::value_to_string(l).startsWith(ExpressionEngine::value_to_string(r),
                                                               Qt::CaseInsensitive);
    if (op == QLatin1String("endsWith"))
        return ExpressionEngine::value_to_string(l).endsWith(ExpressionEngine::value_to_string(r),
                                                             Qt::CaseInsensitive);

    double a = 0;
    double b = 0;
    if (expr_eng_to_number(l, &a) && expr_eng_to_number(r, &b)) {
        if (op == QLatin1String(">"))
            return a > b;
        if (op == QLatin1String("<"))
            return a < b;
        if (op == QLatin1String(">="))
            return a >= b;
        if (op == QLatin1String("<="))
            return a <= b;
        if (op == QLatin1String("=="))
            return a == b;
        if (op == QLatin1String("!="))
            return a != b;
        return false;
    }

    const bool l_null = l.isNull() || l.isUndefined();
    const bool r_null = r.isNull() || r.isUndefined();
    if (l_null || r_null) {
        if (op == QLatin1String("=="))
            return l_null && r_null;
        if (op == QLatin1String("!="))
            return l_null != r_null;
        return false; // ordering against a missing value is never true
    }

    if (l.isBool() && r.isBool()) {
        if (op == QLatin1String("=="))
            return l.toBool() == r.toBool();
        if (op == QLatin1String("!="))
            return l.toBool() != r.toBool();
    }

    const int c = ExpressionEngine::value_to_string(l).compare(ExpressionEngine::value_to_string(r));
    if (op == QLatin1String("=="))
        return c == 0;
    if (op == QLatin1String("!="))
        return c != 0;
    if (op == QLatin1String(">"))
        return c > 0;
    if (op == QLatin1String("<"))
        return c < 0;
    if (op == QLatin1String(">="))
        return c >= 0;
    if (op == QLatin1String("<="))
        return c <= 0;
    return false;
}

// One `lhs OP rhs` clause (or a bare path tested for truthiness).
bool expr_eng_eval_clause(const QString& clause, const QJsonValue& root) {
    const QString text = clause.trimmed();
    if (text.isEmpty())
        return false;

    // Mask quoted text and bracketed groups so operators inside them are not found.
    QString masked = text;
    {
        int depth = 0;
        QChar quote;
        for (int i = 0; i < masked.size(); ++i) {
            const QChar c = masked[i];
            if (!quote.isNull()) {
                if (c == quote)
                    quote = QChar();
                masked[i] = QLatin1Char('x');
                continue;
            }
            if (c == QLatin1Char('"') || c == QLatin1Char('\'')) {
                quote = c;
                masked[i] = QLatin1Char('x');
            } else if (c == QLatin1Char('(') || c == QLatin1Char('[') || c == QLatin1Char('{')) {
                ++depth;
                masked[i] = QLatin1Char('x');
            } else if (c == QLatin1Char(')') || c == QLatin1Char(']') || c == QLatin1Char('}')) {
                if (depth > 0)
                    --depth;
                masked[i] = QLatin1Char('x');
            } else if (depth > 0) {
                masked[i] = QLatin1Char('x');
            }
        }
    }

    // Earliest symbolic operator.
    int op_pos = -1;
    int op_len = 0;
    QString op;
    for (int i = 0; i < masked.size(); ++i) {
        const QString two = masked.mid(i, 2);
        if (two == QLatin1String(">=") || two == QLatin1String("<=") || two == QLatin1String("!=") ||
            two == QLatin1String("==")) {
            op_pos = i;
            op_len = 2;
            op = two;
            break;
        }
        const QChar c = masked[i];
        if (c == QLatin1Char('>') || c == QLatin1Char('<')) {
            op_pos = i;
            op_len = 1;
            op = QString(c);
            break;
        }
        if (c == QLatin1Char('=')) { // a lone "=" is accepted as equality
            op_pos = i;
            op_len = 1;
            op = QStringLiteral("==");
            break;
        }
    }

    // Word operators, only when no symbolic operator was found.
    if (op_pos < 0) {
        static const QRegularExpression word_re(QStringLiteral(R"(\s(contains|startsWith|endsWith)\s)"),
                                                QRegularExpression::CaseInsensitiveOption);
        const QRegularExpressionMatch m = word_re.match(masked);
        if (m.hasMatch()) {
            op_pos = static_cast<int>(m.capturedStart(0));
            op_len = static_cast<int>(m.capturedLength(0));
            const QString w = m.captured(1).toLower();
            op = w == QLatin1String("startswith") ? QStringLiteral("startsWith")
                 : w == QLatin1String("endswith") ? QStringLiteral("endsWith")
                                                  : QStringLiteral("contains");
        }
    }

    if (op_pos < 0)
        return expr_eng_truthy(expr_eng_operand(text, root, /*allow_bare_word=*/false));

    const QJsonValue lhs = expr_eng_operand(text.left(op_pos), root, /*allow_bare_word=*/false);
    const QJsonValue rhs = expr_eng_operand(text.mid(op_pos + op_len), root, /*allow_bare_word=*/true);
    return expr_eng_compare(lhs, op, rhs);
}

} // namespace

// ── Public helpers ─────────────────────────────────────────────────────

QString ExpressionEngine::number_to_string(double value) {
    if (!std::isfinite(value))
        return QString::number(value); // "inf" / "nan" — nothing better to say
    return QString::number(value, 'g', 15);
}

QString ExpressionEngine::value_to_string(const QJsonValue& value) {
    switch (value.type()) {
        case QJsonValue::String:
            return value.toString();
        case QJsonValue::Double:
            return number_to_string(value.toDouble());
        case QJsonValue::Bool:
            return value.toBool() ? QStringLiteral("true") : QStringLiteral("false");
        case QJsonValue::Array:
            return QString::fromUtf8(QJsonDocument(value.toArray()).toJson(QJsonDocument::Compact));
        case QJsonValue::Object:
            return QString::fromUtf8(QJsonDocument(value.toObject()).toJson(QJsonDocument::Compact));
        case QJsonValue::Null:
        case QJsonValue::Undefined:
            break;
    }
    return {};
}

bool ExpressionEngine::evaluate_condition(const QString& expression, const QJsonValue& data) {
    QString expr = expression.trimmed();
    if (expr.startsWith(QLatin1String("={{")) && expr.endsWith(QLatin1String("}}")))
        expr = expr.mid(3, expr.length() - 5).trimmed();
    else if (expr.startsWith(QLatin1String("{{")) && expr.endsWith(QLatin1String("}}")) &&
             expr.count(QLatin1String("{{")) == 1)
        expr = expr.mid(2, expr.length() - 4).trimmed();
    if (expr.isEmpty())
        return false;

    const QJsonObject context = data.isObject() ? data.toObject() : QJsonObject{{QStringLiteral("value"), data}};
    const QJsonValue root(context);

    for (const QString& or_part : expr_eng_split_logic(expr, QStringLiteral("||"))) {
        bool all = true;
        for (const QString& and_part : expr_eng_split_logic(or_part, QStringLiteral("&&"))) {
            if (!expr_eng_eval_clause(and_part, root)) {
                all = false;
                break;
            }
        }
        if (all)
            return true;
    }
    return false;
}

QJsonValue ExpressionEngine::evaluate(const QJsonValue& param_value, const QJsonObject& context) {
    if (!param_value.isString())
        return param_value;

    QString str = param_value.toString();

    // Full expression: ={{...}} (a single expression — not "={{a}} text {{b}}")
    if (str.startsWith(QLatin1String("={{")) && str.endsWith(QLatin1String("}}")) &&
        str.indexOf(QLatin1String("}}")) == str.length() - 2) {
        QString expr = str.mid(3, str.length() - 5).trimmed();
        return resolve_expression(expr, context);
    }
    if (str.startsWith(QLatin1String("={{")))
        str = str.mid(1); // "={{a}} and {{b}}" — the leading '=' only marks it as an expression

    // Template interpolation: text with {{...}} placeholders
    static const QRegularExpression re(QStringLiteral("\\{\\{(.+?)\\}\\}"));
    auto it = re.globalMatch(str);
    if (!it.hasNext())
        return param_value; // no templates

    QString result = str;
    while (it.hasNext()) {
        auto match = it.next();
        QString expr = match.captured(1).trimmed();
        QJsonValue resolved = resolve_expression(expr, context);

        // Qt's QString::number(double) keeps 6 significant digits ("1.23457e+06" for a
        // volume of 1234567); numbers are rendered at full precision instead, and
        // objects / arrays as JSON rather than the old literal "null".
        const QString replacement = (resolved.isNull() || resolved.isUndefined())
                                        ? QStringLiteral("null")
                                        : value_to_string(resolved);

        result.replace(match.captured(0), replacement);
    }

    return QJsonValue(result);
}

QJsonObject ExpressionEngine::resolve_params(const QJsonObject& params, const QJsonObject& context) {
    QJsonObject resolved;
    for (auto it = params.constBegin(); it != params.constEnd(); ++it) {
        resolved[it.key()] = evaluate(it.value(), context);
    }
    return resolved;
}

QJsonValue ExpressionEngine::resolve_expression(const QString& expr, const QJsonObject& context) {
    return resolve_path(QJsonValue(context), expr);
}

QJsonValue ExpressionEngine::resolve_path(const QJsonValue& root, const QString& path_in) {
    QString path = path_in.trimmed();
    if (path == QLatin1String("$input"))
        return root;
    if (path.startsWith(QLatin1String("$input.")))
        path = path.mid(7);
    else if (path.startsWith(QLatin1String("$input[")))
        path = path.mid(6);
    else if (path.startsWith(QLatin1Char('$')))
        path = path.mid(1);
    if (path.isEmpty())
        return root;

    static const QRegularExpression call_re(QStringLiteral(R"(^([A-Za-z_]\w*)\((.*)\)$)"),
                                            QRegularExpression::DotMatchesEverythingOption);

    QJsonValue current = root;
    for (const QString& raw_segment : expr_eng_split_top(path, QLatin1Char('.'))) {
        const QString seg = raw_segment.trimmed();
        if (seg.isEmpty())
            continue;

        // Helper call: sum(), toUpper(), round(2), contains("x"), ...
        const QRegularExpressionMatch call = call_re.match(seg);
        if (call.hasMatch()) {
            current = call_extension(call.captured(1), current, expr_eng_parse_arg(call.captured(2)));
            if (current.isUndefined())
                return expr_eng_undefined(); // unknown helper for this value type
            continue;
        }

        // key, key[0], key["a"][1], [0]
        const int bracket = static_cast<int>(seg.indexOf(QLatin1Char('[')));
        const QString name = bracket < 0 ? seg : seg.left(bracket);
        if (!name.isEmpty()) {
            if (name == QLatin1String("length") && current.isArray())
                current = static_cast<int>(current.toArray().size());
            else if (name == QLatin1String("length") && current.isString())
                current = static_cast<int>(current.toString().size());
            else
                current = expr_eng_lookup(current, name);
        }
        int open = bracket;
        while (open >= 0) {
            const int close = static_cast<int>(seg.indexOf(QLatin1Char(']'), open));
            if (close < 0)
                return expr_eng_undefined(); // unbalanced subscript
            QString key = seg.mid(open + 1, close - open - 1).trimmed();
            if (expr_eng_is_quoted(key))
                key = key.mid(1, key.size() - 2);
            current = expr_eng_lookup(current, key);
            open = static_cast<int>(seg.indexOf(QLatin1Char('['), close));
        }

        if (current.isUndefined())
            return expr_eng_undefined(); // path not found
    }

    return current;
}

} // namespace fincept::workflow
