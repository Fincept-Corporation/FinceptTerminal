// SchemaValidator.cpp — Validate + normalize tool args against a ToolSchema.

#include "mcp/SchemaValidator.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QRegularExpression>

namespace fincept::mcp {

namespace {

// ── JSON Schema 2020-12 type check ──────────────────────────────────────────
// QJsonValue::isDouble() is true for both integer and floating values
// (Qt stores all numbers as double). For "integer" we additionally require
// the value to be representable losslessly as an int64.
bool type_matches(const QString& expected, const QJsonValue& v) {
    if (expected == "string")  return v.isString();
    if (expected == "boolean") return v.isBool();
    if (expected == "array")   return v.isArray();
    if (expected == "object")  return v.isObject();
    if (expected == "number")  return v.isDouble();
    if (expected == "integer") {
        if (!v.isDouble()) return false;
        const double d = v.toDouble();
        return d == static_cast<double>(static_cast<qint64>(d));
    }
    // Unknown type → permissive (don't break tools using non-standard types).
    return true;
}

QString fail_msg(const QString& key, const QString& reason) {
    return "Invalid argument: " + key + ": " + reason;
}

// ── Validate one param against either the structured ToolParam OR the
// legacy raw QJsonObject schema fragment. Returns empty string on success,
// error reason on failure.
QString validate_typed(const QString& /*key*/, const ToolParam& p, const QJsonValue& v) {
    if (!type_matches(p.type, v))
        return "expected " + p.type;

    if (p.type == "string") {
        const QString s = v.toString();
        if (p.min_length && s.size() < *p.min_length)
            return QString("min length %1").arg(*p.min_length);
        if (p.max_length && s.size() > *p.max_length)
            return QString("max length %1").arg(*p.max_length);
        if (!p.enum_values.isEmpty() && !p.enum_values.contains(s))
            return "must be one of [" + p.enum_values.join(",") + "]";
        if (!p.pattern.isEmpty()) {
            QRegularExpression rx(p.pattern);
            if (!rx.match(s).hasMatch())
                return "does not match pattern " + p.pattern;
        }
    } else if (p.type == "number" || p.type == "integer") {
        const double d = v.toDouble();
        if (p.minimum && d < *p.minimum)
            return QString("minimum %1").arg(*p.minimum);
        if (p.maximum && d > *p.maximum)
            return QString("maximum %1").arg(*p.maximum);
    }
    return {};
}

QString validate_legacy(const QString& /*key*/, const QJsonObject& spec, const QJsonValue& v) {
    const QString expected = spec["type"].toString();
    if (!expected.isEmpty() && !type_matches(expected, v))
        return "expected " + expected;

    if (spec.contains("enum")) {
        const QJsonArray arr = spec["enum"].toArray();
        bool found = false;
        for (const auto& e : arr) {
            if (e == v) { found = true; break; }
        }
        if (!found) {
            QStringList allowed;
            for (const auto& e : arr) allowed.append(e.toString());
            return "must be one of [" + allowed.join(",") + "]";
        }
    }
    if (expected == "string" && spec.contains("pattern")) {
        QRegularExpression rx(spec["pattern"].toString());
        if (!rx.match(v.toString()).hasMatch())
            return "does not match pattern " + spec["pattern"].toString();
    }
    if ((expected == "number" || expected == "integer")) {
        const double d = v.toDouble();
        if (spec.contains("minimum") && d < spec["minimum"].toDouble())
            return QString("minimum %1").arg(spec["minimum"].toDouble());
        if (spec.contains("maximum") && d > spec["maximum"].toDouble())
            return QString("maximum %1").arg(spec["maximum"].toDouble());
    }
    return {};
}

} // anonymous namespace

Result<void> validate_args(const ToolSchema& schema, QJsonObject& args) {
    // ── Step 1: inject defaults from structured params ──────────────────
    for (auto it = schema.params.constBegin(); it != schema.params.constEnd(); ++it) {
        const QString& key = it.key();
        const ToolParam& p = it.value();
        if (!args.contains(key) && !p.default_value.isNull() && !p.default_value.isUndefined())
            args[key] = p.default_value;
    }
    // Legacy `properties` may also declare defaults inline.
    for (auto it = schema.properties.constBegin(); it != schema.properties.constEnd(); ++it) {
        const QString& key = it.key();
        if (args.contains(key)) continue;
        const QJsonObject spec = it.value().toObject();
        if (spec.contains("default") && !schema.params.contains(key))
            args[key] = spec["default"];
    }

    // ── Step 2: required check ──────────────────────────────────────────
    QStringList all_required = schema.required;
    for (auto it = schema.params.constBegin(); it != schema.params.constEnd(); ++it) {
        if (it.value().required && !all_required.contains(it.key()))
            all_required.append(it.key());
    }
    for (const QString& key : all_required) {
        if (!args.contains(key))
            return Result<void>::err(("Missing required parameter: " + key).toStdString());
    }

    // ── Step 2.5: coerce stringified primitives, objects, arrays ─────────
    // Two common LLM mistakes:
    //   - emit nested JSON as an escaped string ("config": "{\"a\":1}")
    //   - emit numbers / booleans as strings from <parameter> XML bodies
    //     ("id": "2", "enabled": "true") because text-tool extraction can't
    //     tell scalars apart from prose without type hints.
    // Coerce based on the schema's expected type before rejection.
    auto expected_type = [&](const QString& key) -> QString {
        if (schema.params.contains(key))
            return schema.params[key].type;
        if (schema.properties.contains(key))
            return schema.properties[key].toObject()["type"].toString();
        return {};
    };
    const QStringList keys = args.keys();
    for (const QString& key : keys) {
        const QString want = expected_type(key);
        const QJsonValue cur = args.value(key);
        if (!cur.isString())
            continue;
        const QString s = cur.toString().trimmed();
        if (s.isEmpty())
            continue;

        if (want == "object" || want == "array") {
            const auto doc = QJsonDocument::fromJson(s.toUtf8());
            if (doc.isNull())
                continue;
            if (want == "object" && doc.isObject())
                args[key] = doc.object();
            else if (want == "array" && doc.isArray())
                args[key] = doc.array();
        } else if (want == "integer") {
            bool ok = false;
            const qint64 n = s.toLongLong(&ok);
            if (ok)
                args[key] = static_cast<double>(n);
        } else if (want == "number") {
            bool ok = false;
            const double d = s.toDouble(&ok);
            if (ok)
                args[key] = d;
        } else if (want == "boolean") {
            const QString lower = s.toLower();
            if (lower == "true" || lower == "1")
                args[key] = true;
            else if (lower == "false" || lower == "0")
                args[key] = false;
        }
    }

    // ── Step 3: per-param type / range / enum / pattern ─────────────────
    // Structured params take precedence. For keys that exist only in the
    // legacy properties object (unmigrated tools), validate against the raw
    // JSON spec.
    for (auto it = args.constBegin(); it != args.constEnd(); ++it) {
        const QString& key = it.key();

        if (schema.params.contains(key)) {
            const QString reason = validate_typed(key, schema.params[key], it.value());
            if (!reason.isEmpty())
                return Result<void>::err(fail_msg(key, reason).toStdString());
            continue;
        }
        if (schema.properties.contains(key)) {
            const QString reason = validate_legacy(key, schema.properties[key].toObject(), it.value());
            if (!reason.isEmpty())
                return Result<void>::err(fail_msg(key, reason).toStdString());
        }
        // Unknown keys are silently allowed — JSON Schema's
        // additionalProperties default is true, and tools may accept extra
        // metadata (e.g. _meta.timeout_ms) that we don't want to reject.
    }

    return Result<void>::ok();
}

} // namespace fincept::mcp
