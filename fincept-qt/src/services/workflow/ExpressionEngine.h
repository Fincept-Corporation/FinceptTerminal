#pragma once

#include <QJsonObject>
#include <QJsonValue>
#include <QString>

namespace fincept::workflow {

/// Evaluates simple template expressions in node parameters.
/// Supports ={{$input.key}} variable substitution from upstream data.
class ExpressionEngine {
  public:
    /// Evaluate a parameter value. If it starts with "={{" and ends with "}}",
    /// resolve it against the input data context. Otherwise return as-is.
    static QJsonValue evaluate(const QJsonValue& param_value, const QJsonObject& context);

    /// Resolve all string values in a parameters object.
    static QJsonObject resolve_params(const QJsonObject& params, const QJsonObject& context);

  private:
    /// Resolve a single expression string like "$input.key" or "$input.nested.key".
    static QJsonValue resolve_expression(const QString& expr, const QJsonObject& context);
};

} // namespace fincept::workflow
