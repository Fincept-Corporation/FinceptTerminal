#include "services/workflow/ParameterProcessor.h"
#include "services/workflow/ExpressionEngine.h"

#include <QJsonArray>
#include <QRegularExpression>
#include <QUrl>
#include <QUrlQuery>

namespace fincept::workflow {

QJsonValue ParameterProcessor::convert(const QJsonValue& raw, const QString& target_type)
{
    if (target_type == "string") {
        if (raw.isString()) return raw;
        if (raw.isDouble()) return QString::number(raw.toDouble());
        if (raw.isBool())   return raw.toBool() ? "true" : "false";
        return raw.toString();
    }
    if (target_type == "number") {
        if (raw.isDouble()) return raw;
        if (raw.isString()) {
            bool ok = false;
            double v = raw.toString().toDouble(&ok);
            return ok ? QJsonValue(v) : QJsonValue{};
        }
        return QJsonValue{};
    }
    if (target_type == "boolean") {
        if (raw.isBool()) return raw;
        if (raw.isString()) return raw.toString().toLower() == "true";
        if (raw.isDouble()) return raw.toDouble() != 0.0;
        return false;
    }
    if (target_type == "json" || target_type == "object") {
        if (raw.isObject()) return raw;
        if (raw.isString()) {
            QJsonDocument doc = QJsonDocument::fromJson(raw.toString().toUtf8());
            if (doc.isObject()) return QJsonValue(doc.object());
            if (doc.isArray())  return QJsonValue(doc.array());
        }
        return QJsonValue{};
    }
    if (target_type == "array") {
        if (raw.isArray()) return raw;
        if (raw.isString()) {
            // Comma-separated to array
            QJsonArray arr;
            for (const auto& s : raw.toString().split(","))
                arr.append(s.trimmed());
            return QJsonValue(arr);
        }
        return QJsonValue{};
    }
    return raw; // passthrough for unknown types
}

bool ParameterProcessor::validate(const QJsonValue& value, const ParamDef& def, QString* error)
{
    if (def.required && (value.isNull() || value.isUndefined() ||
        (value.isString() && value.toString().isEmpty()))) {
        if (error) *error = QString("'%1' is required").arg(def.label);
        return false;
    }

    if (value.isNull() || value.isUndefined()) return true; // optional + empty is OK

    if (def.type == "string" || def.type == "expression" || def.type == "code") {
        if (!value.isString() && !value.isDouble() && !value.isBool()) {
            if (error) *error = QString("'%1' must be a string").arg(def.label);
            return false;
        }
    }
    else if (def.type == "number") {
        if (!value.isDouble() && !(value.isString() && !value.toString().isEmpty())) {
            if (error) *error = QString("'%1' must be a number").arg(def.label);
            return false;
        }
    }
    else if (def.type == "boolean") {
        // Any value can be boolean-ified
    }
    else if (def.type == "select") {
        if (!def.options.isEmpty() && !def.options.contains(value.toString())) {
            if (error) *error = QString("'%1' must be one of: %2").arg(def.label, def.options.join(", "));
            return false;
        }
    }

    return true;
}

bool ParameterProcessor::validate_all(const QJsonObject& params, const QVector<ParamDef>& defs,
                                       QStringList* errors)
{
    bool all_ok = true;
    for (const auto& def : defs) {
        QJsonValue val = params.value(def.key);
        QString err;
        if (!validate(val, def, &err)) {
            all_ok = false;
            if (errors) errors->append(err);
        }
    }
    return all_ok;
}

QJsonObject ParameterProcessor::process(const QJsonObject& raw_params,
                                         const QVector<ParamDef>& defs,
                                         const QJsonObject& context)
{
    QJsonObject result;
    for (const auto& def : defs) {
        QJsonValue raw = raw_params.value(def.key);
        if (raw.isUndefined() || raw.isNull())
            raw = def.default_value;

        // Resolve expressions
        QJsonValue resolved = ExpressionEngine::evaluate(raw, context);

        // Type convert
        QJsonValue converted = convert(resolved, def.type);

        result[def.key] = converted;
    }
    return result;
}

RoutedRequest ParameterProcessor::build_request(const QJsonObject& params,
                                                 const QVector<ParamDef>& defs,
                                                 const QString& base_url)
{
    RoutedRequest req;
    req.url = base_url;
    req.method = params.value("method").toString("GET");

    for (const auto& def : defs) {
        QJsonValue val = params.value(def.key);
        if (val.isUndefined() || val.isNull()) continue;

        // Simple routing by key convention
        if (def.key == "url" || def.key == "method") continue; // already handled
        if (def.key.startsWith("header_") || def.key == "headers") {
            if (val.isObject()) {
                for (auto it = val.toObject().constBegin(); it != val.toObject().constEnd(); ++it)
                    req.headers[it.key()] = it.value();
            }
        }
        else if (def.key == "body" || def.type == "json") {
            if (val.isObject())
                req.body = val.toObject();
        }
        else {
            // Default: add to query string
            req.query[def.key] = val;
        }
    }

    // Build URL with query params
    if (!req.query.isEmpty()) {
        QUrl url(req.url);
        QUrlQuery query;
        for (auto it = req.query.constBegin(); it != req.query.constEnd(); ++it)
            query.addQueryItem(it.key(), it.value().toString());
        url.setQuery(query);
        req.url = url.toString();
    }

    return req;
}

QJsonValue ParameterProcessor::extract_path(const QJsonObject& obj, const QString& path)
{
    QStringList parts = path.split('.');
    QJsonValue current = QJsonValue(obj);

    for (const auto& part : parts) {
        if (current.isObject())
            current = current.toObject().value(part);
        else if (current.isArray()) {
            bool ok = false;
            int idx = part.toInt(&ok);
            if (ok && idx >= 0 && idx < current.toArray().size())
                current = current.toArray().at(idx);
            else
                return {};
        }
        else
            return {};
    }
    return current;
}

} // namespace fincept::workflow
