#pragma once

#include <QJsonObject>
#include <QJsonValue>
#include <QString>

namespace fincept::workflow {

/// Evaluates simple template expressions in node parameters.
/// Supports ={{$input.key}} variable substitution from upstream data, dotted
/// paths with [n] subscripts (items[0].price, items[-1]), and the array / string /
/// number / object helper methods from Extensions (`$input.prices.sum()`,
/// `$input.name.toUpper()`, `$input.price.round(2)`).
class ExpressionEngine {
  public:
    /// Evaluate a parameter value. If it starts with "={{" and ends with "}}",
    /// resolve it against the input data context. Otherwise return as-is.
    static QJsonValue evaluate(const QJsonValue& param_value, const QJsonObject& context);

    /// Resolve all string values in a parameters object.
    static QJsonObject resolve_params(const QJsonObject& params, const QJsonObject& context);

    /// Resolve a path expression ("$input.a.b[0].c", "a.b", "prices.sum()") against
    /// `root`. Missing keys / out-of-range indices yield an undefined QJsonValue.
    static QJsonValue resolve_path(const QJsonValue& root, const QString& path);

    /// Evaluate a boolean condition against `data` (an object, or any value which is
    /// exposed to the expression as `value`). Accepts an optional "={{ ... }}" wrapper.
    ///
    /// Supported: comparisons (> < >= <= == !=) with numeric comparison when both sides
    /// are numbers (or numeric strings) and exact string comparison otherwise;
    /// `contains` / `startsWith` / `endsWith` (case-insensitive); `&&` / `||`; and a bare
    /// path, which is tested for truthiness. A right-hand side that is neither a quoted
    /// literal, a number, true/false/null nor a resolvable path is taken as a bare word
    /// (`status == active`). Never throws; a malformed expression is simply false.
    static bool evaluate_condition(const QString& expression, const QJsonValue& data);

    /// Number -> text without Qt's default 6-significant-digit truncation
    /// (QString::number(1234567.0) is "1.23457e+06"; this gives "1234567").
    static QString number_to_string(double value);

    /// Scalar / container -> display text (objects and arrays as compact JSON,
    /// null/undefined as an empty string).
    static QString value_to_string(const QJsonValue& value);

  private:
    /// Resolve a single expression string like "$input.key" or "$input.nested.key".
    static QJsonValue resolve_expression(const QString& expr, const QJsonObject& context);
};

} // namespace fincept::workflow
