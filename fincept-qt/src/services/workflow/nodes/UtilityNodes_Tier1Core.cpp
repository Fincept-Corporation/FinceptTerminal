// src/services/workflow/nodes/UtilityNodes_Tier1Core.cpp
//
// Tier-1 data-operation nodes: DateTime, Filter, Map, Aggregate, Sort, Join,
// GroupBy, Deduplicate, Limit.
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

void register_utility_tier1_core(NodeRegistry& registry) {
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
                if (inputs.isEmpty()) {
                    cb(true, QJsonArray{}, {});
                    return;
                }

                QString field = params.value("field").toString();
                QString op = params.value("operator").toString("equals");
                QString value = params.value("value").toString();

                auto matches = [&](const QJsonValue& item) -> bool {
                    QJsonValue field_val = item.isObject() ? item.toObject().value(field) : QJsonValue{};
                    QString field_str =
                        field_val.isString() ? field_val.toString() : QString::number(field_val.toDouble());

                    if (op == "equals")
                        return field_str == value;
                    if (op == "not_equals")
                        return field_str != value;
                    if (op == "contains")
                        return field_str.contains(value, Qt::CaseInsensitive);
                    if (op == "greater_than")
                        return field_val.toDouble() > value.toDouble();
                    if (op == "less_than")
                        return field_val.toDouble() < value.toDouble();
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
                QString op = params.value("operation").toString("sum");
                QJsonArray arr = inputs[0].toArray();

                double result = 0.0;
                double minimum = std::numeric_limits<double>::max();
                double maximum = std::numeric_limits<double>::lowest();
                int count = 0;

                for (const QJsonValue& item : arr) {
                    if (!item.isObject())
                        continue;
                    QJsonValue fv = item.toObject().value(field);
                    double v = fv.toDouble();
                    result += v;
                    minimum = std::min(minimum, v);
                    maximum = std::max(maximum, v);
                    ++count;
                }

                double out_val = 0.0;
                if (op == "sum")
                    out_val = result;
                else if (op == "avg")
                    out_val = count > 0 ? result / count : 0.0;
                else if (op == "min")
                    out_val = count > 0 ? minimum : 0.0;
                else if (op == "max")
                    out_val = count > 0 ? maximum : 0.0;
                else if (op == "count")
                    out_val = static_cast<double>(count);

                QJsonObject out;
                out["result"] = out_val;
                out["field"] = field;
                out["operation"] = op;
                out["count"] = count;
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
                bool desc = params.value("direction").toString("asc") == "desc";

                QJsonArray arr = inputs[0].toArray();
                // Copy into a sortable list.
                QVector<QJsonValue> items;
                items.reserve(arr.size());
                for (const QJsonValue& v : arr)
                    items.append(v);

                std::stable_sort(items.begin(), items.end(), [&](const QJsonValue& a, const QJsonValue& b) {
                    QJsonValue av = a.isObject() ? a.toObject().value(field) : QJsonValue{};
                    QJsonValue bv = b.isObject() ? b.toObject().value(field) : QJsonValue{};

                    // Numeric comparison if both are numbers.
                    if ((av.isDouble() || av.isUndefined()) && (bv.isDouble() || bv.isUndefined())) {
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
                QJsonArray arr_a = (inputs.size() > 0 && inputs[0].isArray()) ? inputs[0].toArray() : QJsonArray{};
                QJsonArray arr_b = (inputs.size() > 1 && inputs[1].isArray()) ? inputs[1].toArray() : QJsonArray{};

                QString join_key = params.value("join_key").toString("id");

                // Build a lookup map from B keyed by join_key.
                QHash<QString, QJsonObject> b_map;
                for (const QJsonValue& item : arr_b) {
                    if (!item.isObject())
                        continue;
                    QJsonObject obj = item.toObject();
                    QString key_val = obj.value(join_key).toVariant().toString();
                    b_map.insert(key_val, obj);
                }

                // Inner join: only emit rows where key exists in both.
                QJsonArray out;
                for (const QJsonValue& item : arr_a) {
                    if (!item.isObject())
                        continue;
                    QJsonObject obj_a = item.toObject();
                    QString key_val = obj_a.value(join_key).toVariant().toString();
                    auto it = b_map.find(key_val);
                    if (it == b_map.end())
                        continue;

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

                QString field = params.value("field").toString();
                QJsonArray arr = inputs[0].toArray();

                // Preserve insertion order using a list of keys alongside the map.
                QJsonObject groups;
                QStringList key_order;

                for (const QJsonValue& item : arr) {
                    if (!item.isObject())
                        continue;
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

                QString field = params.value("field").toString();
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
                if (inputs.isEmpty()) {
                    cb(true, QJsonArray{}, {});
                    return;
                }
                if (!inputs[0].isArray()) {
                    cb(true, inputs[0], {});
                    return;
                }

                int n = static_cast<int>(params.value("count").toDouble(10));
                if (n < 0)
                    n = 0;

                QJsonArray arr = inputs[0].toArray();
                QJsonArray out;
                for (int i = 0; i < n && i < arr.size(); ++i)
                    out.append(arr[i]);
                cb(true, out, {});
            },
    });

}
} // namespace fincept::workflow
