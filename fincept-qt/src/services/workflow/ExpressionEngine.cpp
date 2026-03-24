#include "services/workflow/ExpressionEngine.h"

#include <QJsonArray>
#include <QRegularExpression>

namespace fincept::workflow {

QJsonValue ExpressionEngine::evaluate(const QJsonValue& param_value, const QJsonObject& context) {
    if (!param_value.isString())
        return param_value;

    QString str = param_value.toString();

    // Full expression: ={{...}}
    if (str.startsWith("={{") && str.endsWith("}}")) {
        QString expr = str.mid(3, str.length() - 5).trimmed();
        return resolve_expression(expr, context);
    }

    // Template interpolation: text with {{...}} placeholders
    static QRegularExpression re("\\{\\{(.+?)\\}\\}");
    auto it = re.globalMatch(str);
    if (!it.hasNext())
        return param_value; // no templates

    QString result = str;
    while (it.hasNext()) {
        auto match = it.next();
        QString expr = match.captured(1).trimmed();
        QJsonValue resolved = resolve_expression(expr, context);

        QString replacement;
        if (resolved.isString())
            replacement = resolved.toString();
        else if (resolved.isDouble())
            replacement = QString::number(resolved.toDouble());
        else if (resolved.isBool())
            replacement = resolved.toBool() ? "true" : "false";
        else
            replacement = "null";

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
    // Support dotted paths: $input.key.nested
    // The context is the merged upstream output
    QString path = expr;

    // Strip $input. prefix if present
    if (path.startsWith("$input."))
        path = path.mid(7);
    else if (path.startsWith("$"))
        path = path.mid(1);

    // Walk the path
    QStringList parts = path.split('.');
    QJsonValue current = QJsonValue(context);

    for (const auto& part : parts) {
        if (current.isObject()) {
            current = current.toObject().value(part);
        } else if (current.isArray()) {
            bool ok = false;
            int idx = part.toInt(&ok);
            if (ok && idx >= 0 && idx < current.toArray().size())
                current = current.toArray().at(idx);
            else
                return QJsonValue(); // path not found
        } else {
            return QJsonValue(); // path not found
        }
    }

    return current;
}

} // namespace fincept::workflow
