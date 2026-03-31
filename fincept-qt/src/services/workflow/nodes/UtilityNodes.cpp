#include "services/workflow/nodes/UtilityNodes.h"

#include "services/workflow/NodeRegistry.h"

#include <QCryptographicHash>
#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QRandomGenerator>
#include <QRegularExpression>

namespace fincept::workflow {

void register_utility_nodes(NodeRegistry& registry) {
    // ── DateTime ───────────────────────────────────────────────────
    registry.register_type({
        .type_id = "utility.datetime",
        .display_name = "Date/Time",
        .category = "Utilities",
        .description = "Get current date/time or format a timestamp",
        .icon_text = "#",
        .accent_color = "#808080",
        .version = 1,
        .inputs = {{"input_0", "Data In", PortDirection::Input, ConnectionType::Main}},
        .outputs = {{"output_main", "Main", PortDirection::Output, ConnectionType::Main}},
        .parameters =
            {
                {"format", "Format", "string", "yyyy-MM-dd HH:mm:ss", {}, "Qt date format"},
                {"timezone", "Timezone", "select", "UTC", {"UTC", "Local"}, ""},
            },
        .execute =
            [](const QJsonObject& params, const QVector<QJsonValue>& inputs,
               std::function<void(bool, QJsonValue, QString)> cb) {
                QString format = params.value("format").toString("yyyy-MM-dd HH:mm:ss");
                bool local = params.value("timezone").toString() == "Local";
                QDateTime now = local ? QDateTime::currentDateTime() : QDateTime::currentDateTimeUtc();

                QJsonObject out = inputs.isEmpty() ? QJsonObject{} : inputs[0].toObject();
                out["datetime"] = now.toString(format);
                out["timestamp"] = now.toMSecsSinceEpoch();
                cb(true, out, {});
            },
    });

    // ── Filter ─────────────────────────────────────────────────────
    registry.register_type({
        .type_id = "transform.filter",
        .display_name = "Filter",
        .category = "Data Transform",
        .description = "Filter items by a field value",
        .icon_text = "~",
        .accent_color = "#0891b2",
        .version = 1,
        .inputs = {{"input_0", "Data In", PortDirection::Input, ConnectionType::Main}},
        .outputs = {{"output_main", "Main", PortDirection::Output, ConnectionType::Main}},
        .parameters =
            {
                {"field", "Field", "string", "", {}, "Field name to filter on", true},
                {"operator",
                 "Operator",
                 "select",
                 "equals",
                 {"equals", "not_equals", "contains", "greater_than", "less_than"},
                 ""},
                {"value", "Value", "string", "", {}, "Comparison value", true},
            },
        .execute =
            [](const QJsonObject& params, const QVector<QJsonValue>& inputs,
               std::function<void(bool, QJsonValue, QString)> cb) {
                if (inputs.isEmpty()) { cb(true, QJsonArray{}, {}); return; }

                QString field    = params.value("field").toString();
                QString op       = params.value("operator").toString("equals");
                QString value    = params.value("value").toString();

                auto matches = [&](const QJsonValue& item) -> bool {
                    QJsonValue field_val = item.isObject() ? item.toObject().value(field) : QJsonValue{};
                    QString field_str = field_val.isString()
                        ? field_val.toString()
                        : QString::number(field_val.toDouble());

                    if (op == "equals")       return field_str == value;
                    if (op == "not_equals")   return field_str != value;
                    if (op == "contains")     return field_str.contains(value, Qt::CaseInsensitive);
                    if (op == "greater_than") return field_val.toDouble() > value.toDouble();
                    if (op == "less_than")    return field_val.toDouble() < value.toDouble();
                    return false;
                };

                QJsonValue input = inputs[0];

                if (input.isArray()) {
                    QJsonArray src = input.toArray();
                    QJsonArray out;
                    for (const QJsonValue& item : src) {
                        if (matches(item))
                            out.append(item);
                    }
                    cb(true, out, {});
                } else if (input.isObject()) {
                    // Treat the object itself as a single item.
                    cb(true, matches(input) ? input : QJsonValue(QJsonArray{}), {});
                } else {
                    cb(true, input, {});
                }
            },
    });

    // ── Map ────────────────────────────────────────────────────────
    registry.register_type({
        .type_id = "transform.map",
        .display_name = "Map",
        .category = "Data Transform",
        .description = "Transform each item in a dataset",
        .icon_text = "~",
        .accent_color = "#0891b2",
        .version = 1,
        .inputs = {{"input_0", "Data In", PortDirection::Input, ConnectionType::Main}},
        .outputs = {{"output_main", "Main", PortDirection::Output, ConnectionType::Main}},
        .parameters =
            {
                {"expression", "Expression", "expression", "", {}, "={{$input.field}}"},
            },
        .execute =
            [](const QJsonObject&, const QVector<QJsonValue>& inputs,
               std::function<void(bool, QJsonValue, QString)> cb) {
                auto data = inputs.isEmpty() ? QJsonValue{} : inputs[0];
                cb(true, data, {});
            },
    });

    // ── Aggregate ──────────────────────────────────────────────────
    registry.register_type({
        .type_id = "transform.aggregate",
        .display_name = "Aggregate",
        .category = "Data Transform",
        .description = "Summarize data (sum, avg, min, max, count)",
        .icon_text = "~",
        .accent_color = "#0891b2",
        .version = 1,
        .inputs = {{"input_0", "Data In", PortDirection::Input, ConnectionType::Main}},
        .outputs = {{"output_main", "Main", PortDirection::Output, ConnectionType::Main}},
        .parameters =
            {
                {"field", "Field", "string", "", {}, "Field to aggregate"},
                {"operation", "Operation", "select", "sum", {"sum", "avg", "min", "max", "count"}, ""},
            },
        .execute =
            [](const QJsonObject& params, const QVector<QJsonValue>& inputs,
               std::function<void(bool, QJsonValue, QString)> cb) {
                if (inputs.isEmpty() || !inputs[0].isArray()) {
                    cb(false, {}, "transform.aggregate requires a JSON array input");
                    return;
                }

                QString field = params.value("field").toString();
                QString op    = params.value("operation").toString("sum");
                QJsonArray arr = inputs[0].toArray();

                double result  = 0.0;
                double minimum = std::numeric_limits<double>::max();
                double maximum = std::numeric_limits<double>::lowest();
                int    count   = 0;

                for (const QJsonValue& item : arr) {
                    if (!item.isObject()) continue;
                    QJsonValue fv = item.toObject().value(field);
                    double v = fv.toDouble();
                    result  += v;
                    minimum  = std::min(minimum, v);
                    maximum  = std::max(maximum, v);
                    ++count;
                }

                double out_val = 0.0;
                if (op == "sum")   out_val = result;
                else if (op == "avg")   out_val = count > 0 ? result / count : 0.0;
                else if (op == "min")   out_val = count > 0 ? minimum : 0.0;
                else if (op == "max")   out_val = count > 0 ? maximum : 0.0;
                else if (op == "count") out_val = static_cast<double>(count);

                QJsonObject out;
                out["result"]    = out_val;
                out["field"]     = field;
                out["operation"] = op;
                out["count"]     = count;
                cb(true, out, {});
            },
    });

    // ── Sort ───────────────────────────────────────────────────────
    registry.register_type({
        .type_id = "transform.sort",
        .display_name = "Sort",
        .category = "Data Transform",
        .description = "Sort data by a field",
        .icon_text = "~",
        .accent_color = "#0891b2",
        .version = 1,
        .inputs = {{"input_0", "Data In", PortDirection::Input, ConnectionType::Main}},
        .outputs = {{"output_main", "Main", PortDirection::Output, ConnectionType::Main}},
        .parameters =
            {
                {"field", "Field", "string", "", {}, "Field to sort by"},
                {"direction", "Direction", "select", "asc", {"asc", "desc"}, ""},
            },
        .execute =
            [](const QJsonObject& params, const QVector<QJsonValue>& inputs,
               std::function<void(bool, QJsonValue, QString)> cb) {
                if (inputs.isEmpty() || !inputs[0].isArray()) {
                    cb(true, inputs.isEmpty() ? QJsonValue{} : inputs[0], {});
                    return;
                }

                QString field = params.value("field").toString();
                bool    desc  = params.value("direction").toString("asc") == "desc";

                QJsonArray arr = inputs[0].toArray();
                // Copy into a sortable list.
                QVector<QJsonValue> items;
                items.reserve(arr.size());
                for (const QJsonValue& v : arr)
                    items.append(v);

                std::stable_sort(items.begin(), items.end(),
                    [&](const QJsonValue& a, const QJsonValue& b) {
                        QJsonValue av = a.isObject() ? a.toObject().value(field) : QJsonValue{};
                        QJsonValue bv = b.isObject() ? b.toObject().value(field) : QJsonValue{};

                        // Numeric comparison if both are numbers.
                        if ((av.isDouble() || av.isUndefined()) &&
                            (bv.isDouble() || bv.isUndefined())) {
                            double da = av.toDouble();
                            double db = bv.toDouble();
                            return desc ? da > db : da < db;
                        }
                        // Fall back to string comparison.
                        QString sa = av.isString() ? av.toString() : QString::number(av.toDouble());
                        QString sb = bv.isString() ? bv.toString() : QString::number(bv.toDouble());
                        return desc ? sa > sb : sa < sb;
                    });

                QJsonArray out;
                for (const QJsonValue& v : items)
                    out.append(v);
                cb(true, out, {});
            },
    });

    // ── Join ───────────────────────────────────────────────────────
    registry.register_type({
        .type_id = "transform.join",
        .display_name = "Join",
        .category = "Data Transform",
        .description = "Join two datasets on a common key",
        .icon_text = "~",
        .accent_color = "#0891b2",
        .version = 1,
        .inputs =
            {
                {"input_a", "Dataset A", PortDirection::Input, ConnectionType::Main},
                {"input_b", "Dataset B", PortDirection::Input, ConnectionType::Main},
            },
        .outputs = {{"output_main", "Main", PortDirection::Output, ConnectionType::Main}},
        .parameters =
            {
                {"join_key", "Join Key", "string", "id", {}, "Field to join on", true},
                {"join_type", "Join Type", "select", "inner", {"inner", "left", "right", "outer"}, ""},
            },
        .execute =
            [](const QJsonObject& params, const QVector<QJsonValue>& inputs,
               std::function<void(bool, QJsonValue, QString)> cb) {
                // Expect input_a at index 0, input_b at index 1.
                QJsonArray arr_a = (inputs.size() > 0 && inputs[0].isArray())
                    ? inputs[0].toArray() : QJsonArray{};
                QJsonArray arr_b = (inputs.size() > 1 && inputs[1].isArray())
                    ? inputs[1].toArray() : QJsonArray{};

                QString join_key = params.value("join_key").toString("id");

                // Build a lookup map from B keyed by join_key.
                QHash<QString, QJsonObject> b_map;
                for (const QJsonValue& item : arr_b) {
                    if (!item.isObject()) continue;
                    QJsonObject obj = item.toObject();
                    QString key_val = obj.value(join_key).toVariant().toString();
                    b_map.insert(key_val, obj);
                }

                // Inner join: only emit rows where key exists in both.
                QJsonArray out;
                for (const QJsonValue& item : arr_a) {
                    if (!item.isObject()) continue;
                    QJsonObject obj_a = item.toObject();
                    QString key_val = obj_a.value(join_key).toVariant().toString();
                    auto it = b_map.find(key_val);
                    if (it == b_map.end()) continue;

                    // Merge: B fields overwrite A on collision.
                    QJsonObject merged = obj_a;
                    const QJsonObject& obj_b = it.value();
                    for (auto bi = obj_b.begin(); bi != obj_b.end(); ++bi)
                        merged.insert(bi.key(), bi.value());
                    out.append(merged);
                }
                cb(true, out, {});
            },
    });

    // ── GroupBy ────────────────────────────────────────────────────
    registry.register_type({
        .type_id = "transform.group_by",
        .display_name = "Group By",
        .category = "Data Transform",
        .description = "Group data by a field",
        .icon_text = "~",
        .accent_color = "#0891b2",
        .version = 1,
        .inputs = {{"input_0", "Data In", PortDirection::Input, ConnectionType::Main}},
        .outputs = {{"output_main", "Main", PortDirection::Output, ConnectionType::Main}},
        .parameters =
            {
                {"field", "Group Field", "string", "", {}, "Field to group by", true},
            },
        .execute =
            [](const QJsonObject& params, const QVector<QJsonValue>& inputs,
               std::function<void(bool, QJsonValue, QString)> cb) {
                if (inputs.isEmpty() || !inputs[0].isArray()) {
                    cb(true, inputs.isEmpty() ? QJsonValue{} : inputs[0], {});
                    return;
                }

                QString field  = params.value("field").toString();
                QJsonArray arr = inputs[0].toArray();

                // Preserve insertion order using a list of keys alongside the map.
                QJsonObject groups;
                QStringList key_order;

                for (const QJsonValue& item : arr) {
                    if (!item.isObject()) continue;
                    QString group_val = item.toObject().value(field).toVariant().toString();
                    if (!groups.contains(group_val)) {
                        groups.insert(group_val, QJsonArray{});
                        key_order.append(group_val);
                    }
                    QJsonArray bucket = groups.value(group_val).toArray();
                    bucket.append(item);
                    groups.insert(group_val, bucket);
                }
                cb(true, groups, {});
            },
    });

    // ── Deduplicate ────────────────────────────────────────────────
    registry.register_type({
        .type_id = "transform.deduplicate",
        .display_name = "Deduplicate",
        .category = "Data Transform",
        .description = "Remove duplicate items",
        .icon_text = "~",
        .accent_color = "#0891b2",
        .version = 1,
        .inputs = {{"input_0", "Data In", PortDirection::Input, ConnectionType::Main}},
        .outputs = {{"output_main", "Main", PortDirection::Output, ConnectionType::Main}},
        .parameters =
            {
                {"field", "Key Field", "string", "", {}, "Field to check for duplicates"},
            },
        .execute =
            [](const QJsonObject& params, const QVector<QJsonValue>& inputs,
               std::function<void(bool, QJsonValue, QString)> cb) {
                if (inputs.isEmpty() || !inputs[0].isArray()) {
                    cb(true, inputs.isEmpty() ? QJsonValue{} : inputs[0], {});
                    return;
                }

                QString field  = params.value("field").toString();
                QJsonArray arr = inputs[0].toArray();
                QJsonArray out;
                QSet<QString> seen;

                for (const QJsonValue& item : arr) {
                    QString key_val;
                    if (field.isEmpty()) {
                        // Whole-object dedup: use compact JSON as key.
                        QJsonDocument doc(item.toObject());
                        key_val = QString::fromUtf8(doc.toJson(QJsonDocument::Compact));
                    } else if (item.isObject()) {
                        key_val = item.toObject().value(field).toVariant().toString();
                    } else {
                        key_val = item.toVariant().toString();
                    }

                    if (!seen.contains(key_val)) {
                        seen.insert(key_val);
                        out.append(item);
                    }
                }
                cb(true, out, {});
            },
    });

    // ── Limit ──────────────────────────────────────────────────────
    registry.register_type({
        .type_id = "utility.limit",
        .display_name = "Limit",
        .category = "Utilities",
        .description = "Limit output to first N items",
        .icon_text = "#",
        .accent_color = "#808080",
        .version = 1,
        .inputs = {{"input_0", "Data In", PortDirection::Input, ConnectionType::Main}},
        .outputs = {{"output_main", "Main", PortDirection::Output, ConnectionType::Main}},
        .parameters =
            {
                {"count", "Count", "number", 10, {}, "Max items to output"},
            },
        .execute =
            [](const QJsonObject& params, const QVector<QJsonValue>& inputs,
               std::function<void(bool, QJsonValue, QString)> cb) {
                if (inputs.isEmpty()) { cb(true, QJsonArray{}, {}); return; }
                if (!inputs[0].isArray()) { cb(true, inputs[0], {}); return; }

                int n = static_cast<int>(params.value("count").toDouble(10));
                if (n < 0) n = 0;

                QJsonArray arr = inputs[0].toArray();
                QJsonArray out;
                for (int i = 0; i < n && i < arr.size(); ++i)
                    out.append(arr[i]);
                cb(true, out, {});
            },
    });

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
                    if (delim.isEmpty()) delim = ",";
                    QString text = data.isString() ? data.toString()
                                 : data.isObject() ? QString::fromUtf8(
                                       QJsonDocument(data.toObject()).toJson(QJsonDocument::Compact))
                                 : QString{};
                    QJsonArray out;
                    for (const QString& s : text.split(delim))
                        out.append(s.trimmed());
                    cb(true, out, {});
                    return;
                }

                // All remaining ops work on arrays
                if (!data.isArray()) { cb(true, data, {}); return; }
                QJsonArray arr = data.toArray();

                if (op == "reverse") {
                    QJsonArray out;
                    for (int i = arr.size() - 1; i >= 0; --i) out.append(arr[i]);
                    cb(true, out, {});
                } else if (op == "unique") {
                    QSet<QString> seen;
                    QJsonArray out;
                    for (const QJsonValue& v : arr) {
                        QString key = v.isObject()
                            ? QString::fromUtf8(QJsonDocument(v.toObject()).toJson(QJsonDocument::Compact))
                            : v.toVariant().toString();
                        if (!seen.contains(key)) { seen.insert(key); out.append(v); }
                    }
                    cb(true, out, {});
                } else if (op == "flatten") {
                    QJsonArray out;
                    for (const QJsonValue& v : arr) {
                        if (v.isArray()) { for (const QJsonValue& inner : v.toArray()) out.append(inner); }
                        else out.append(v);
                    }
                    cb(true, out, {});
                } else if (op == "shuffle") {
                    QVector<QJsonValue> vec;
                    vec.reserve(arr.size());
                    for (const QJsonValue& v : arr) vec.append(v);
                    for (int i = vec.size() - 1; i > 0; --i) {
                        int j = static_cast<int>(QRandomGenerator::global()->bounded(i + 1));
                        std::swap(vec[i], vec[j]);
                    }
                    QJsonArray out;
                    for (const QJsonValue& v : vec) out.append(v);
                    cb(true, out, {});
                } else if (op == "concatenate") {
                    // Merge all sub-arrays in inputs into one
                    QJsonArray out = arr;
                    for (int i = 1; i < inputs.size(); ++i)
                        if (inputs[i].isArray())
                            for (const QJsonValue& v : inputs[i].toArray()) out.append(v);
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
                             : data.isObject() ? QString::fromUtf8(
                                   QJsonDocument(data.toObject()).toJson(QJsonDocument::Compact))
                             : QString::number(data.toDouble());

                if (op == "base64_encode") {
                    QString encoded = QString::fromLatin1(text.toUtf8().toBase64());
                    cb(true, QJsonObject{{"result", encoded}, {"operation", op}}, {});
                } else if (op == "base64_decode") {
                    QString decoded = QString::fromUtf8(QByteArray::fromBase64(text.toLatin1()));
                    cb(true, QJsonObject{{"result", decoded}, {"operation", op}}, {});
                } else if (op == "md5") {
                    QString hash = QString::fromLatin1(
                        QCryptographicHash::hash(text.toUtf8(), QCryptographicHash::Md5).toHex());
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

    // ── Tier 2: Data & Transformation ──────────────────────────────

    registry.register_type({
        .type_id = "transform.pivot",
        .display_name = "Pivot Table",
        .category = "Data Transform",
        .description = "Pivot rows to columns",
        .icon_text = "~",
        .accent_color = "#0891b2",
        .version = 1,
        .inputs = {{"input_0", "Data In", PortDirection::Input, ConnectionType::Main}},
        .outputs = {{"output_main", "Main", PortDirection::Output, ConnectionType::Main}},
        .parameters =
            {
                {"index", "Index Field", "string", "", {}, "Row identifier field", true},
                {"columns", "Column Field", "string", "", {}, "Field to pivot into columns", true},
                {"values", "Values Field", "string", "", {}, "Field for cell values", true},
                {"agg", "Aggregation", "select", "sum", {"sum", "mean", "count", "first", "last"}, ""},
            },
        .execute =
            [](const QJsonObject& params, const QVector<QJsonValue>& inputs,
               std::function<void(bool, QJsonValue, QString)> cb) {
                QString op  = params.value("operation").toString("flatten");
                QString key = params.value("key").toString();
                QJsonValue data = inputs.isEmpty() ? QJsonValue{} : inputs[0];

                if (op == "flatten" && data.isArray()) {
                    QJsonArray out;
                    for (const QJsonValue& v : data.toArray()) {
                        if (v.isArray()) for (const QJsonValue& inner : v.toArray()) out.append(inner);
                        else out.append(v);
                    }
                    cb(true, out, {}); return;
                }
                if (op == "nest" && data.isArray()) {
                    // Wrap each item under "key" if provided, else wrap all items under "items"
                    QJsonObject out;
                    out[key.isEmpty() ? "items" : key] = data.toArray();
                    cb(true, out, {}); return;
                }
                cb(true, data, {});
            },
    });

    registry.register_type({
        .type_id = "transform.normalize",
        .display_name = "Normalize",
        .category = "Data Transform",
        .description = "Min-max or z-score normalization",
        .icon_text = "~",
        .accent_color = "#0891b2",
        .version = 1,
        .inputs = {{"input_0", "Data In", PortDirection::Input, ConnectionType::Main}},
        .outputs = {{"output_main", "Main", PortDirection::Output, ConnectionType::Main}},
        .parameters =
            {
                {"method", "Method", "select", "min_max", {"min_max", "z_score", "robust", "log"}, ""},
                {"field", "Field", "string", "", {}, "Field to normalize"},
            },
        .execute =
            [](const QJsonObject& params, const QVector<QJsonValue>& inputs,
               std::function<void(bool, QJsonValue, QString)> cb) {
                QString method = params.value("method").toString("min_max");
                QString field  = params.value("field").toString();
                QJsonValue data = inputs.isEmpty() ? QJsonValue{} : inputs[0];
                if (!data.isArray()) { cb(true, data, {}); return; }

                QJsonArray arr = data.toArray();
                // Extract numeric values
                QVector<double> vals;
                vals.reserve(arr.size());
                for (const QJsonValue& v : arr)
                    vals.append(v.isObject() ? v.toObject().value(field).toDouble() : v.toDouble());

                if (vals.isEmpty()) { cb(true, data, {}); return; }

                double min_v = *std::min_element(vals.begin(), vals.end());
                double max_v = *std::max_element(vals.begin(), vals.end());
                double mean_v = 0;
                for (double d : vals) mean_v += d;
                mean_v /= vals.size();
                double std_v = 0;
                for (double d : vals) std_v += (d - mean_v) * (d - mean_v);
                std_v = std::sqrt(std_v / vals.size());

                QJsonArray out;
                for (int i = 0; i < arr.size(); ++i) {
                    double norm = 0;
                    if (method == "min_max")
                        norm = (max_v - min_v) > 0 ? (vals[i] - min_v) / (max_v - min_v) : 0;
                    else if (method == "z_score")
                        norm = std_v > 0 ? (vals[i] - mean_v) / std_v : 0;
                    else if (method == "log")
                        norm = vals[i] > 0 ? std::log(vals[i]) : 0;
                    else  // robust: (x - median) / IQR — approximate with mean/std
                        norm = std_v > 0 ? (vals[i] - mean_v) / std_v : 0;

                    if (arr[i].isObject()) {
                        QJsonObject obj = arr[i].toObject();
                        obj[field + "_norm"] = norm;
                        out.append(obj);
                    } else {
                        out.append(norm);
                    }
                }
                cb(true, out, {});
            },
    });

    registry.register_type({
        .type_id = "transform.rolling_window",
        .display_name = "Rolling Window",
        .category = "Data Transform",
        .description = "Rolling calculations (MA, sum, std, etc.)",
        .icon_text = "~",
        .accent_color = "#0891b2",
        .version = 1,
        .inputs = {{"input_0", "Data In", PortDirection::Input, ConnectionType::Main}},
        .outputs = {{"output_main", "Main", PortDirection::Output, ConnectionType::Main}},
        .parameters =
            {
                {"window", "Window Size", "number", 20, {}, "Number of periods"},
                {"operation", "Operation", "select", "mean", {"mean", "sum", "std", "min", "max", "median"}, ""},
                {"field", "Field", "string", "close", {}, "Field to compute on"},
            },
        .execute =
            [](const QJsonObject& params, const QVector<QJsonValue>& inputs,
               std::function<void(bool, QJsonValue, QString)> cb) {
                int    window = static_cast<int>(params.value("window").toDouble(20));
                QString op    = params.value("operation").toString("mean");
                QString field = params.value("field").toString("close");
                QJsonValue data = inputs.isEmpty() ? QJsonValue{} : inputs[0];
                if (!data.isArray() || window < 1) { cb(true, data, {}); return; }

                QJsonArray arr = data.toArray();
                QVector<double> vals;
                vals.reserve(arr.size());
                for (const QJsonValue& v : arr)
                    vals.append(v.isObject() ? v.toObject().value(field).toDouble() : v.toDouble());

                QJsonArray out;
                for (int i = 0; i < arr.size(); ++i) {
                    int start = qMax(0, i - window + 1);
                    QVector<double> w = vals.mid(start, i - start + 1);
                    double result = 0;
                    if (op == "sum") {
                        for (double d : w) result += d;
                    } else if (op == "min") {
                        result = *std::min_element(w.begin(), w.end());
                    } else if (op == "max") {
                        result = *std::max_element(w.begin(), w.end());
                    } else if (op == "std") {
                        double m = 0; for (double d : w) m += d; m /= w.size();
                        for (double d : w) result += (d - m) * (d - m);
                        result = std::sqrt(result / w.size());
                    } else if (op == "median") {
                        QVector<double> s = w; std::sort(s.begin(), s.end());
                        result = s[s.size() / 2];
                    } else {  // mean
                        for (double d : w) result += d; result /= w.size();
                    }
                    if (arr[i].isObject()) {
                        QJsonObject obj = arr[i].toObject();
                        obj[field + "_" + op + "_" + QString::number(window)] = result;
                        out.append(obj);
                    } else {
                        out.append(result);
                    }
                }
                cb(true, out, {});
            },
    });

    registry.register_type({
        .type_id = "transform.lag",
        .display_name = "Lag / Lead",
        .category = "Data Transform",
        .description = "Time-shift data by N periods",
        .icon_text = "~",
        .accent_color = "#0891b2",
        .version = 1,
        .inputs = {{"input_0", "Data In", PortDirection::Input, ConnectionType::Main}},
        .outputs = {{"output_main", "Main", PortDirection::Output, ConnectionType::Main}},
        .parameters =
            {
                {"periods", "Periods", "number", 1, {}, "Positive=lag, negative=lead"},
                {"field", "Field", "string", "", {}, "Field to shift"},
                {"fill", "Fill Value", "select", "null", {"null", "zero", "forward_fill", "backward_fill"}, ""},
            },
        .execute =
            [](const QJsonObject& params, const QVector<QJsonValue>& inputs,
               std::function<void(bool, QJsonValue, QString)> cb) {
                int     periods = static_cast<int>(params.value("periods").toDouble(1));
                QString field   = params.value("field").toString();
                QString fill    = params.value("fill").toString("null");
                QJsonValue data = inputs.isEmpty() ? QJsonValue{} : inputs[0];
                if (!data.isArray()) { cb(true, data, {}); return; }

                QJsonArray arr = data.toArray();
                QJsonArray out;
                int n = arr.size();
                for (int i = 0; i < n; ++i) {
                    int src = i - periods;
                    QJsonValue lagged;
                    if (src >= 0 && src < n) {
                        lagged = field.isEmpty() ? arr[src]
                                                 : QJsonValue(arr[src].toObject().value(field));
                    } else {
                        if (fill == "zero")           lagged = 0.0;
                        else if (fill == "forward_fill" && i > 0) lagged = field.isEmpty() ? arr[i-1] : QJsonValue(arr[i-1].toObject().value(field));
                        else if (fill == "backward_fill" && src+n >= 0) lagged = field.isEmpty() ? arr[qMax(0,src+n)] : QJsonValue(arr[qMax(0,src+n)].toObject().value(field));
                        else lagged = QJsonValue::Null;
                    }
                    if (field.isEmpty()) {
                        out.append(lagged);
                    } else {
                        QJsonObject obj = arr[i].isObject() ? arr[i].toObject() : QJsonObject{};
                        obj[field + "_lag_" + QString::number(periods)] = lagged;
                        out.append(obj);
                    }
                }
                cb(true, out, {});
            },
    });

    registry.register_type({
        .type_id = "transform.resample",
        .display_name = "Resample",
        .category = "Data Transform",
        .description = "Change time frequency (1min→1h, daily→weekly)",
        .icon_text = "~",
        .accent_color = "#0891b2",
        .version = 1,
        .inputs = {{"input_0", "Data In", PortDirection::Input, ConnectionType::Main}},
        .outputs = {{"output_main", "Main", PortDirection::Output, ConnectionType::Main}},
        .parameters =
            {
                {"target_freq",
                 "Target Frequency",
                 "select",
                 "1h",
                 {"1m", "5m", "15m", "1h", "4h", "1d", "1w", "1M"},
                 ""},
                {"ohlc_mode", "OHLC Mode", "boolean", true, {}, "Use OHLC resampling for price data"},
            },
        .execute =
            [](const QJsonObject&, const QVector<QJsonValue>& inputs,
               std::function<void(bool, QJsonValue, QString)> cb) {
                cb(true, inputs.isEmpty() ? QJsonValue{} : inputs[0], {});
            },
    });

    // ── Tier 2: Utility additions ──────────────────────────────────

    registry.register_type({
        .type_id = "utility.cache_node",
        .display_name = "Cache",
        .category = "Utilities",
        .description = "Cache node output with TTL to avoid redundant computation",
        .icon_text = "#",
        .accent_color = "#808080",
        .version = 1,
        .inputs = {{"input_0", "Data In", PortDirection::Input, ConnectionType::Main}},
        .outputs = {{"output_main", "Main", PortDirection::Output, ConnectionType::Main}},
        .parameters =
            {
                {"ttl_seconds", "TTL (sec)", "number", 300, {}, "Cache lifetime in seconds"},
                {"cache_key", "Cache Key", "string", "", {}, "Custom key (auto-generated if empty)"},
            },
        .execute =
            [](const QJsonObject& params, const QVector<QJsonValue>& inputs,
               std::function<void(bool, QJsonValue, QString)> cb) {
                static QHash<QString, QPair<QJsonValue, qint64>> cache;
                auto data  = inputs.isEmpty() ? QJsonValue{} : inputs[0];
                int  ttl   = static_cast<int>(params.value("ttl_seconds").toDouble(300)) * 1000;
                QString key = params.value("cache_key").toString();
                if (key.isEmpty()) {
                    // Auto-key from data fingerprint
                    key = data.isObject()
                        ? QString::fromUtf8(QJsonDocument(data.toObject()).toJson(QJsonDocument::Compact)).left(64)
                        : data.toVariant().toString().left(64);
                }
                qint64 now = QDateTime::currentMSecsSinceEpoch();
                auto it = cache.find(key);
                if (it != cache.end() && (now - it.value().second) < ttl) {
                    cb(true, it.value().first, {});
                    return;
                }
                cache.insert(key, {data, now});
                cb(true, data, {});
            },
    });

    registry.register_type({
        .type_id = "utility.delay_node",
        .display_name = "Delay",
        .category = "Utilities",
        .description = "Configurable delay between nodes",
        .icon_text = "#",
        .accent_color = "#808080",
        .version = 1,
        .inputs = {{"input_0", "Data In", PortDirection::Input, ConnectionType::Main}},
        .outputs = {{"output_main", "Main", PortDirection::Output, ConnectionType::Main}},
        .parameters =
            {
                {"delay_ms", "Delay (ms)", "number", 1000, {}, "Milliseconds to wait"},
            },
        .execute =
            [](const QJsonObject& params, const QVector<QJsonValue>& inputs,
               std::function<void(bool, QJsonValue, QString)> cb) {
                int ms = static_cast<int>(params.value("delay_ms").toDouble(1000));
                auto data = inputs.isEmpty() ? QJsonValue{} : inputs[0];
                QTimer::singleShot(ms, [cb, data]() { cb(true, data, {}); });
            },
    });

    registry.register_type({
        .type_id = "utility.log_node",
        .display_name = "Log",
        .category = "Utilities",
        .description = "Log data to console/file for debugging",
        .icon_text = "#",
        .accent_color = "#808080",
        .version = 1,
        .inputs = {{"input_0", "Data In", PortDirection::Input, ConnectionType::Main}},
        .outputs = {{"output_main", "Main", PortDirection::Output, ConnectionType::Main}},
        .parameters =
            {
                {"label", "Label", "string", "DEBUG", {}, "Log prefix label"},
                {"level", "Level", "select", "info", {"debug", "info", "warn", "error"}, ""},
            },
        .execute =
            [](const QJsonObject& params, const QVector<QJsonValue>& inputs,
               std::function<void(bool, QJsonValue, QString)> cb) {
                auto data = inputs.isEmpty() ? QJsonValue{} : inputs[0];
                QJsonObject out;
                out["label"] = params.value("label").toString("DEBUG");
                out["logged_data"] = data;
                cb(true, out, {});
            },
    });

    registry.register_type({
        .type_id = "utility.assert_node",
        .display_name = "Assert",
        .category = "Utilities",
        .description = "Assert a condition — fail workflow if false",
        .icon_text = "#",
        .accent_color = "#808080",
        .version = 1,
        .inputs = {{"input_0", "Data In", PortDirection::Input, ConnectionType::Main}},
        .outputs = {{"output_main", "Main", PortDirection::Output, ConnectionType::Main}},
        .parameters =
            {
                {"condition", "Condition", "expression", "", {}, "={{$input.value > 0}}"},
                {"message", "Fail Message", "string", "Assertion failed", {}, ""},
            },
        .execute =
            [](const QJsonObject& params, const QVector<QJsonValue>& inputs,
               std::function<void(bool, QJsonValue, QString)> cb) {
                auto data = inputs.isEmpty() ? QJsonValue{} : inputs[0];
                QString condition = params.value("condition").toString();
                QString message   = params.value("message").toString("Assertion failed");

                // Evaluate simple expressions: field op value
                // Supports: {{field}} > value, {{field}} == value, {{field}} != value
                bool passed = true;
                if (!condition.isEmpty() && data.isObject()) {
                    QJsonObject obj = data.toObject();
                    static const QRegularExpression expr_re(
                        R"(\{\{(\w+)\}\}\s*(==|!=|>|<|>=|<=|contains)\s*(.+))");
                    QRegularExpressionMatch m = expr_re.match(condition.trimmed());
                    if (m.hasMatch()) {
                        QString field = m.captured(1);
                        QString op    = m.captured(2).trimmed();
                        QString rhs   = m.captured(3).trimmed();
                        QJsonValue lhsv = obj.value(field);
                        double lhsd = lhsv.toDouble();
                        double rhsd = rhs.toDouble();
                        QString lhss = lhsv.isString() ? lhsv.toString() : QString::number(lhsd);
                        if      (op == "==")       passed = (lhss == rhs);
                        else if (op == "!=")       passed = (lhss != rhs);
                        else if (op == ">")        passed = (lhsd > rhsd);
                        else if (op == "<")        passed = (lhsd < rhsd);
                        else if (op == ">=")       passed = (lhsd >= rhsd);
                        else if (op == "<=")       passed = (lhsd <= rhsd);
                        else if (op == "contains") passed = lhss.contains(rhs);
                    }
                }
                if (!passed)
                    cb(false, {}, message);
                else
                    cb(true, data, {});
            },
    });

    registry.register_type({
        .type_id = "utility.template_render",
        .display_name = "Template",
        .category = "Utilities",
        .description = "Render string template with {{variable}} substitution",
        .icon_text = "#",
        .accent_color = "#808080",
        .version = 1,
        .inputs = {{"input_0", "Data In", PortDirection::Input, ConnectionType::Main}},
        .outputs = {{"output_main", "Main", PortDirection::Output, ConnectionType::Main}},
        .parameters =
            {
                {"template", "Template", "code", "", {}, "Hello {{name}}, price is {{price}}"},
                {"output_key", "Output Key", "string", "rendered", {}, "Key to store result"},
            },
        .execute =
            [](const QJsonObject& params, const QVector<QJsonValue>& inputs,
               std::function<void(bool, QJsonValue, QString)> cb) {
                QString tmpl = params.value("template").toString();
                QJsonObject input_obj = inputs.isEmpty() ? QJsonObject{} : inputs[0].toObject();

                // Replace all {{field}} occurrences with the corresponding input field.
                static const QRegularExpression placeholder_re(R"(\{\{(\w+)\}\})");
                QString rendered = tmpl;
                QRegularExpressionMatchIterator it = placeholder_re.globalMatch(tmpl);
                // Collect replacements first to avoid offset issues.
                QVector<std::pair<QString, QString>> replacements;
                while (it.hasNext()) {
                    QRegularExpressionMatch m = it.next();
                    QString full_match = m.captured(0); // e.g. "{{name}}"
                    QString key        = m.captured(1); // e.g. "name"
                    QJsonValue val = input_obj.value(key);
                    QString replacement = val.isString()
                        ? val.toString()
                        : QString::number(val.toDouble());
                    replacements.append({full_match, replacement});
                }
                for (const auto& [placeholder, value] : replacements)
                    rendered.replace(placeholder, value);

                QJsonObject out;
                out["rendered"] = rendered;
                cb(true, out, {});
            },
    });
}

} // namespace fincept::workflow
