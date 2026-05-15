// src/services/workflow/nodes/UtilityNodes_Tier1Extra.cpp
//
// Tier-1 utility nodes: Item Lists, RSS Read, Crypto/Hash, Database,
// Execute Workflow, Market Event Trigger, Reshape.
//
// Part of the topic-based split of UtilityNodes.cpp.

#include "services/workflow/nodes/UtilityNodes.h"

#include "services/workflow/NodeRegistry.h"

#include <QCryptographicHash>
#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QRandomGenerator>
#include <QRegularExpression>

namespace fincept::workflow {

void register_utility_tier1_extra(NodeRegistry& registry) {
    // ── Item Lists ────────────────────────────────────────────────
    registry.register_type({
        .type_id = "utility.item_lists",
        .display_name = "Item Lists",
        .category = "Utilities",
        .description = "Array operations: split, concatenate, flatten, unique",
        .icon_text = "#",
        .accent_color = "#808080",
        .version = 1,
        .inputs = {{"input_0", "Data In", PortDirection::Input, ConnectionType::Main}},
        .outputs = {{"output_main", "Main", PortDirection::Output, ConnectionType::Main}},
        .parameters =
            {
                {"operation",
                 "Operation",
                 "select",
                 "split",
                 {"split", "concatenate", "flatten", "unique", "reverse", "shuffle"},
                 ""},
                {"field", "Field", "string", "", {}, "Field to operate on"},
            },
        .execute =
            [](const QJsonObject& params, const QVector<QJsonValue>& inputs,
               std::function<void(bool, QJsonValue, QString)> cb) {
                QString op = params.value("operation").toString("split");
                QJsonValue data = inputs.isEmpty() ? QJsonValue{} : inputs[0];

                // split: string → array by delimiter (field param used as delimiter)
                if (op == "split") {
                    QString delim = params.value("field").toString(",");
                    if (delim.isEmpty())
                        delim = ",";
                    QString text =
                        data.isString() ? data.toString()
                        : data.isObject()
                            ? QString::fromUtf8(QJsonDocument(data.toObject()).toJson(QJsonDocument::Compact))
                            : QString{};
                    QJsonArray out;
                    for (const QString& s : text.split(delim))
                        out.append(s.trimmed());
                    cb(true, out, {});
                    return;
                }

                // All remaining ops work on arrays
                if (!data.isArray()) {
                    cb(true, data, {});
                    return;
                }
                QJsonArray arr = data.toArray();

                if (op == "reverse") {
                    QJsonArray out;
                    for (int i = arr.size() - 1; i >= 0; --i)
                        out.append(arr[i]);
                    cb(true, out, {});
                } else if (op == "unique") {
                    QSet<QString> seen;
                    QJsonArray out;
                    for (const QJsonValue& v : arr) {
                        QString key =
                            v.isObject() ? QString::fromUtf8(QJsonDocument(v.toObject()).toJson(QJsonDocument::Compact))
                                         : v.toVariant().toString();
                        if (!seen.contains(key)) {
                            seen.insert(key);
                            out.append(v);
                        }
                    }
                    cb(true, out, {});
                } else if (op == "flatten") {
                    QJsonArray out;
                    for (const QJsonValue& v : arr) {
                        if (v.isArray()) {
                            for (const QJsonValue& inner : v.toArray())
                                out.append(inner);
                        } else
                            out.append(v);
                    }
                    cb(true, out, {});
                } else if (op == "shuffle") {
                    QVector<QJsonValue> vec;
                    vec.reserve(arr.size());
                    for (const QJsonValue& v : arr)
                        vec.append(v);
                    for (int i = vec.size() - 1; i > 0; --i) {
                        int j = static_cast<int>(QRandomGenerator::global()->bounded(i + 1));
                        std::swap(vec[i], vec[j]);
                    }
                    QJsonArray out;
                    for (const QJsonValue& v : vec)
                        out.append(v);
                    cb(true, out, {});
                } else if (op == "concatenate") {
                    // Merge all sub-arrays in inputs into one
                    QJsonArray out = arr;
                    for (int i = 1; i < inputs.size(); ++i)
                        if (inputs[i].isArray())
                            for (const QJsonValue& v : inputs[i].toArray())
                                out.append(v);
                    cb(true, out, {});
                } else {
                    cb(true, arr, {});
                }
            },
    });

    // ── RSS Read ──────────────────────────────────────────────────
    registry.register_type({
        .type_id = "utility.rss_read",
        .display_name = "RSS Read",
        .category = "Utilities",
        .description = "Parse an RSS/Atom feed",
        .icon_text = "#",
        .accent_color = "#808080",
        .version = 1,
        .inputs = {{"input_0", "Data In", PortDirection::Input, ConnectionType::Main}},
        .outputs = {{"output_main", "Main", PortDirection::Output, ConnectionType::Main}},
        .parameters =
            {
                {"url", "Feed URL", "string", "", {}, "RSS/Atom feed URL", true},
                {"limit", "Limit", "number", 20, {}, "Max items"},
            },
        .execute = nullptr,
    });

    // ── Crypto/Hash ───────────────────────────────────────────────
    registry.register_type({
        .type_id = "utility.crypto",
        .display_name = "Crypto/Hash",
        .category = "Utilities",
        .description = "Encrypt, decrypt, hash, or encode data",
        .icon_text = "#",
        .accent_color = "#808080",
        .version = 1,
        .inputs = {{"input_0", "Data In", PortDirection::Input, ConnectionType::Main}},
        .outputs = {{"output_main", "Main", PortDirection::Output, ConnectionType::Main}},
        .parameters =
            {
                {"operation",
                 "Operation",
                 "select",
                 "sha256",
                 {"sha256", "md5", "base64_encode", "base64_decode", "hmac_sha256"},
                 ""},
                {"key", "Key", "string", "", {}, "Key for HMAC operations"},
            },
        .execute =
            [](const QJsonObject& params, const QVector<QJsonValue>& inputs,
               std::function<void(bool, QJsonValue, QString)> cb) {
                QString op = params.value("operation").toString("sha256");
                QJsonValue data = inputs.isEmpty() ? QJsonValue{} : inputs[0];

                // Get text to operate on
                QString text = data.isString() ? data.toString()
                               : data.isObject()
                                   ? QString::fromUtf8(QJsonDocument(data.toObject()).toJson(QJsonDocument::Compact))
                                   : QString::number(data.toDouble());

                if (op == "base64_encode") {
                    QString encoded = QString::fromLatin1(text.toUtf8().toBase64());
                    cb(true, QJsonObject{{"result", encoded}, {"operation", op}}, {});
                } else if (op == "base64_decode") {
                    QString decoded = QString::fromUtf8(QByteArray::fromBase64(text.toLatin1()));
                    cb(true, QJsonObject{{"result", decoded}, {"operation", op}}, {});
                } else if (op == "md5") {
                    QString hash =
                        QString::fromLatin1(QCryptographicHash::hash(text.toUtf8(), QCryptographicHash::Md5).toHex());
                    cb(true, QJsonObject{{"result", hash}, {"operation", op}}, {});
                } else if (op == "hmac_sha256") {
                    // Qt doesn't have built-in HMAC — return sha256 of key+text as fallback
                    QString key = params.value("key").toString();
                    QString combined = key + text;
                    QString hash = QString::fromLatin1(
                        QCryptographicHash::hash(combined.toUtf8(), QCryptographicHash::Sha256).toHex());
                    cb(true, QJsonObject{{"result", hash}, {"operation", op}, {"note", "simplified_hmac"}}, {});
                } else {
                    // sha256 default
                    QString hash = QString::fromLatin1(
                        QCryptographicHash::hash(text.toUtf8(), QCryptographicHash::Sha256).toHex());
                    cb(true, QJsonObject{{"result", hash}, {"operation", op}}, {});
                }
            },
    });

    // ── Database ──────────────────────────────────────────────────
    registry.register_type({
        .type_id = "utility.database",
        .display_name = "Database",
        .category = "Utilities",
        .description = "Execute SQL queries against a database",
        .icon_text = "#",
        .accent_color = "#808080",
        .version = 1,
        .inputs = {{"input_0", "Data In", PortDirection::Input, ConnectionType::Main}},
        .outputs = {{"output_main", "Main", PortDirection::Output, ConnectionType::Main}},
        .parameters =
            {
                {"operation", "Operation", "select", "select", {"select", "insert", "update", "delete", "raw"}, ""},
                {"query", "Query", "code", "", {}, "SQL query"},
                {"database", "Database", "select", "local", {"local", "custom"}, ""},
            },
        .execute = nullptr,
    });

    // ── Execute Workflow ──────────────────────────────────────────
    registry.register_type({
        .type_id = "control.execute_workflow",
        .display_name = "Execute Workflow",
        .category = "Control Flow",
        .description = "Execute another workflow as a sub-workflow",
        .icon_text = "<>",
        .accent_color = "#808080",
        .version = 1,
        .inputs = {{"input_0", "Data In", PortDirection::Input, ConnectionType::Main}},
        .outputs = {{"output_main", "Main", PortDirection::Output, ConnectionType::Main}},
        .parameters =
            {
                {"workflow_id", "Workflow ID", "string", "", {}, "ID of workflow to execute", true},
            },
        .execute = nullptr,
    });

    // ── Market Event Trigger ─────────────────────────────────────
    registry.register_type({
        .type_id = "trigger.market_event",
        .display_name = "Market Event",
        .category = "Triggers",
        .description = "Trigger on market events (high volatility, circuit breaker, etc.)",
        .icon_text = ">>",
        .accent_color = "#d97706",
        .version = 1,
        .inputs = {},
        .outputs = {{"output_main", "Main", PortDirection::Output, ConnectionType::Main}},
        .parameters =
            {
                {"event_type",
                 "Event",
                 "select",
                 "volatility_spike",
                 {"volatility_spike", "circuit_breaker", "market_open", "market_close", "earnings_release"},
                 ""},
                {"threshold", "Threshold", "number", 2.0, {}, "Event threshold"},
            },
        .execute =
            [](const QJsonObject& params, const QVector<QJsonValue>&,
               std::function<void(bool, QJsonValue, QString)> cb) {
                QJsonObject out;
                out["event_type"] = params.value("event_type").toString();
                out["triggered"] = true;
                cb(true, out, {});
            },
    });

    // ── Reshape ────────────────────────────────────────────────────
    registry.register_type({
        .type_id = "transform.reshape",
        .display_name = "Reshape",
        .category = "Data Transform",
        .description = "Restructure data (pivot, unpivot, flatten, nest)",
        .icon_text = "~",
        .accent_color = "#0891b2",
        .version = 1,
        .inputs = {{"input_0", "Data In", PortDirection::Input, ConnectionType::Main}},
        .outputs = {{"output_main", "Main", PortDirection::Output, ConnectionType::Main}},
        .parameters =
            {
                {"operation", "Operation", "select", "flatten", {"flatten", "nest", "pivot", "unpivot"}, ""},
                {"key", "Key Field", "string", "", {}, ""},
            },
        .execute =
            [](const QJsonObject&, const QVector<QJsonValue>& inputs,
               std::function<void(bool, QJsonValue, QString)> cb) {
                auto data = inputs.isEmpty() ? QJsonValue{} : inputs[0];
                cb(true, data, {});
            },
    });

}
} // namespace fincept::workflow
