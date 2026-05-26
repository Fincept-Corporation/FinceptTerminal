// src/services/workflow/nodes/UtilityNodes_Tier2.cpp
//
// Tier-2 data + utility nodes added after the initial registry pass.
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

void register_utility_tier2(NodeRegistry& registry) {
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
                QString op = params.value("operation").toString("flatten");
                QString key = params.value("key").toString();
                QJsonValue data = inputs.isEmpty() ? QJsonValue{} : inputs[0];

                if (op == "flatten" && data.isArray()) {
                    QJsonArray out;
                    for (const QJsonValue& v : data.toArray()) {
                        if (v.isArray())
                            for (const QJsonValue& inner : v.toArray())
                                out.append(inner);
                        else
                            out.append(v);
                    }
                    cb(true, out, {});
                    return;
                }
                if (op == "nest" && data.isArray()) {
                    // Wrap each item under "key" if provided, else wrap all items under "items"
                    QJsonObject out;
                    out[key.isEmpty() ? "items" : key] = data.toArray();
                    cb(true, out, {});
                    return;
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
                QString field = params.value("field").toString();
                QJsonValue data = inputs.isEmpty() ? QJsonValue{} : inputs[0];
                if (!data.isArray()) {
                    cb(true, data, {});
                    return;
                }

                QJsonArray arr = data.toArray();
                // Extract numeric values
                QVector<double> vals;
                vals.reserve(arr.size());
                for (const QJsonValue& v : arr)
                    vals.append(v.isObject() ? v.toObject().value(field).toDouble() : v.toDouble());

                if (vals.isEmpty()) {
                    cb(true, data, {});
                    return;
                }

                double min_v = *std::min_element(vals.begin(), vals.end());
                double max_v = *std::max_element(vals.begin(), vals.end());
                double mean_v = 0;
                for (double d : vals)
                    mean_v += d;
                mean_v /= vals.size();
                double std_v = 0;
                for (double d : vals)
                    std_v += (d - mean_v) * (d - mean_v);
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
                    else // robust: (x - median) / IQR — approximate with mean/std
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
                int window = static_cast<int>(params.value("window").toDouble(20));
                QString op = params.value("operation").toString("mean");
                QString field = params.value("field").toString("close");
                QJsonValue data = inputs.isEmpty() ? QJsonValue{} : inputs[0];
                if (!data.isArray() || window < 1) {
                    cb(true, data, {});
                    return;
                }

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
                        for (double d : w)
                            result += d;
                    } else if (op == "min") {
                        result = *std::min_element(w.begin(), w.end());
                    } else if (op == "max") {
                        result = *std::max_element(w.begin(), w.end());
                    } else if (op == "std") {
                        double m = 0;
                        for (double d : w)
                            m += d;
                        m /= w.size();
                        for (double d : w)
                            result += (d - m) * (d - m);
                        result = std::sqrt(result / w.size());
                    } else if (op == "median") {
                        QVector<double> s = w;
                        std::sort(s.begin(), s.end());
                        result = s[s.size() / 2];
                    } else { // mean
                        for (double d : w)
                            result += d;
                        result /= w.size();
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
                int periods = static_cast<int>(params.value("periods").toDouble(1));
                QString field = params.value("field").toString();
                QString fill = params.value("fill").toString("null");
                QJsonValue data = inputs.isEmpty() ? QJsonValue{} : inputs[0];
                if (!data.isArray()) {
                    cb(true, data, {});
                    return;
                }

                QJsonArray arr = data.toArray();
                QJsonArray out;
                int n = arr.size();
                for (int i = 0; i < n; ++i) {
                    int src = i - periods;
                    QJsonValue lagged;
                    if (src >= 0 && src < n) {
                        lagged = field.isEmpty() ? arr[src] : QJsonValue(arr[src].toObject().value(field));
                    } else {
                        if (fill == "zero")
                            lagged = 0.0;
                        else if (fill == "forward_fill" && i > 0)
                            lagged = field.isEmpty() ? arr[i - 1] : QJsonValue(arr[i - 1].toObject().value(field));
                        else if (fill == "backward_fill" && src + n >= 0)
                            lagged = field.isEmpty() ? arr[qMax(0, src + n)]
                                                     : QJsonValue(arr[qMax(0, src + n)].toObject().value(field));
                        else
                            lagged = QJsonValue::Null;
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
                auto data = inputs.isEmpty() ? QJsonValue{} : inputs[0];
                int ttl = static_cast<int>(params.value("ttl_seconds").toDouble(300)) * 1000;
                QString key = params.value("cache_key").toString();
                if (key.isEmpty()) {
                    // Auto-key from data fingerprint
                    key =
                        data.isObject()
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
                QString message = params.value("message").toString("Assertion failed");

                // Evaluate simple expressions: field op value
                // Supports: {{field}} > value, {{field}} == value, {{field}} != value
                bool passed = true;
                if (!condition.isEmpty() && data.isObject()) {
                    QJsonObject obj = data.toObject();
                    static const QRegularExpression expr_re(R"(\{\{(\w+)\}\}\s*(==|!=|>|<|>=|<=|contains)\s*(.+))");
                    QRegularExpressionMatch m = expr_re.match(condition.trimmed());
                    if (m.hasMatch()) {
                        QString field = m.captured(1);
                        QString op = m.captured(2).trimmed();
                        QString rhs = m.captured(3).trimmed();
                        QJsonValue lhsv = obj.value(field);
                        double lhsd = lhsv.toDouble();
                        double rhsd = rhs.toDouble();
                        QString lhss = lhsv.isString() ? lhsv.toString() : QString::number(lhsd);
                        if (op == "==")
                            passed = (lhss == rhs);
                        else if (op == "!=")
                            passed = (lhss != rhs);
                        else if (op == ">")
                            passed = (lhsd > rhsd);
                        else if (op == "<")
                            passed = (lhsd < rhsd);
                        else if (op == ">=")
                            passed = (lhsd >= rhsd);
                        else if (op == "<=")
                            passed = (lhsd <= rhsd);
                        else if (op == "contains")
                            passed = lhss.contains(rhs);
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
                    QString key = m.captured(1);        // e.g. "name"
                    QJsonValue val = input_obj.value(key);
                    QString replacement = val.isString() ? val.toString() : QString::number(val.toDouble());
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
