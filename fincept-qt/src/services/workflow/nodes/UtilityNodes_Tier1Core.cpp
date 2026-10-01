// src/services/workflow/nodes/UtilityNodes_Tier1Core.cpp
//
// Tier-1 data-operation nodes: DateTime, Filter, Map, Aggregate, Sort, Join,
// GroupBy, Deduplicate, Limit.
//
// Part of the topic-based split of UtilityNodes.cpp.

#include "services/workflow/ExpressionEngine.h"
#include "services/workflow/NodeRegistry.h"
#include "services/workflow/nodes/UtilityNodes.h"

#include <QCryptographicHash>
#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QRandomGenerator>
#include <QRegularExpression>

namespace fincept::workflow {

namespace {

// A number out of a JSON value: real numbers, and strings that are plainly numeric
// ("12.5"). Data coming from CSV / HTTP is routinely numeric text; reading it with
// QJsonValue::toDouble() silently gave 0.
bool t1c_number(const QJsonValue& v, double* out) {
    if (v.isDouble()) {
        *out = v.toDouble();
        return true;
    }
    if (v.isString()) {
        const QString s = v.toString().trimmed();
        if (s.isEmpty())
            return false;
        const QChar c = s[0];
        if (!(c.isDigit() || c == QLatin1Char('-') || c == QLatin1Char('+') || c == QLatin1Char('.')))
            return false;
        bool ok = false;
        const double d = s.toDouble(&ok);
        if (ok) {
            *out = d;
            return true;
        }
    }
    return false;
}

} // namespace

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

                // Compare numerically when both sides are numbers (or numeric text), as text
                // otherwise. The old code stringified numbers with QString::number() — 6
                // significant digits, so 1234567 never "equals" 1234567 — and read numeric
                // text as 0 for greater/less-than.
                auto matches = [&](const QJsonValue& item) -> bool {
                    QJsonValue field_val = item.isObject() ? item.toObject().value(field) : QJsonValue{};
                    const QString field_str = ExpressionEngine::value_to_string(field_val);

                    double a = 0;
                    double b = 0;
                    const bool numeric = t1c_number(field_val, &a) && t1c_number(QJsonValue(value), &b);

                    if (op == "equals")
                        return numeric ? a == b : field_str == value;
                    if (op == "not_equals")
                        return numeric ? a != b : field_str != value;
                    if (op == "contains")
                        return field_str.contains(value, Qt::CaseInsensitive);
                    if (op == "greater_than")
                        return numeric ? a > b : field_str > value;
                    if (op == "less_than")
                        return numeric ? a < b : field_str < value;
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
            [](const QJsonObject& params, const QVector<QJsonValue>& inputs,
               std::function<void(bool, QJsonValue, QString)> cb) {
                auto data = inputs.isEmpty() ? QJsonValue{} : inputs[0];

                // The Expression parameter was ignored (the node returned its input as-is).
                // Each item is the expression's context: ={{$input.price}}, {{name}} text
                // templates, helpers like ={{prices.sum()}}; a bare path ("price") also works.
                QString expr = params.value("expression").toString().trimmed();
                if (expr.isEmpty()) {
                    cb(true, data, {});
                    return;
                }
                if (!expr.contains(QLatin1String("{{")))
                    expr = QStringLiteral("={{") + expr + QStringLiteral("}}");

                auto map_one = [&expr](const QJsonValue& item) -> QJsonValue {
                    const QJsonObject ctx = item.isObject() ? item.toObject() : QJsonObject{{"value", item}};
                    QJsonValue v = ExpressionEngine::evaluate(QJsonValue(expr), ctx);
                    return v.isUndefined() ? QJsonValue(QJsonValue::Null) : v;
                };

                if (data.isArray()) {
                    QJsonArray out;
                    for (const QJsonValue& item : data.toArray())
                        out.append(map_one(item));
                    cb(true, out, {});
                } else {
                    cb(true, map_one(data), {});
                }
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
                    // [1, 2, 3] with no field aggregates the numbers themselves.
                    if (!item.isObject() && !field.isEmpty())
                        continue;
                    QJsonValue fv = item.isObject() ? item.toObject().value(field) : item;
                    double v = 0.0;
                    if (!t1c_number(fv, &v) && op != "count")
                        continue; // not a number (missing / text) — don't drag sum/min/max/avg toward 0
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

                // The Join Type select was ignored — every join was an inner join.
                const QString join_type = params.value("join_type").toString("inner");
                const bool keep_unmatched_a = (join_type == "left" || join_type == "outer");
                const bool keep_unmatched_b = (join_type == "right" || join_type == "outer");

                // Build a lookup map from B keyed by join_key.
                QHash<QString, QJsonObject> b_map;
                QStringList b_order;
                for (const QJsonValue& item : arr_b) {
                    if (!item.isObject())
                        continue;
                    QJsonObject obj = item.toObject();
                    QString key_val = obj.value(join_key).toVariant().toString();
                    if (!b_map.contains(key_val))
                        b_order << key_val;
                    b_map.insert(key_val, obj);
                }

                QJsonArray out;
                QSet<QString> matched_b;
                for (const QJsonValue& item : arr_a) {
                    if (!item.isObject())
                        continue;
                    QJsonObject obj_a = item.toObject();
                    QString key_val = obj_a.value(join_key).toVariant().toString();
                    auto it = b_map.find(key_val);
                    if (it == b_map.end()) {
                        if (keep_unmatched_a)
                            out.append(obj_a);
                        continue;
                    }
                    matched_b.insert(key_val);

                    // Merge: B fields overwrite A on collision.
                    QJsonObject merged = obj_a;
                    const QJsonObject& obj_b = it.value();
                    for (auto bi = obj_b.begin(); bi != obj_b.end(); ++bi)
                        merged.insert(bi.key(), bi.value());
                    out.append(merged);
                }
                if (keep_unmatched_b) {
                    for (const QString& key_val : b_order) {
                        if (!matched_b.contains(key_val))
                            out.append(b_map.value(key_val));
                    }
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

                // Bucket in a hash and build the result object once. The old code re-read,
                // copied and re-inserted the whole bucket array for every row (O(n^2)).
                QHash<QString, QJsonArray> buckets;
                for (const QJsonValue& item : arr) {
                    if (!item.isObject())
                        continue;
                    buckets[item.toObject().value(field).toVariant().toString()].append(item);
                }
                QJsonObject groups;
                for (auto it = buckets.constBegin(); it != buckets.constEnd(); ++it)
                    groups.insert(it.key(), it.value());
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
                        // Whole-item dedup: the item's own JSON is the key. (item.toObject() made
                        // every scalar "{}", so [1, 2, 3] collapsed to a single element.)
                        key_val = ExpressionEngine::value_to_string(item);
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
