// src/services/workflow/nodes/UtilityNodes_Tier2.cpp
//
// Tier-2 data + utility nodes added after the initial registry pass.
//
// Part of the topic-based split of UtilityNodes.cpp.

#include "core/logging/Logger.h"
#include "services/workflow/ExpressionEngine.h"
#include "services/workflow/NodeRegistry.h"
#include "services/workflow/WorkflowCache.h"
#include "services/workflow/nodes/UtilityNodes.h"

#include <QCryptographicHash>
#include <QDate>
#include <QDateTime>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QRandomGenerator>
#include <QRegularExpression>
#include <QTimeZone>

#include <algorithm>

namespace fincept::workflow {

namespace {

// Epoch seconds of a history row: numeric `timestamp` (seconds, or milliseconds when > 1e11), or an
// ISO `date` / `datetime` string. Returns false when the row has no usable time.
bool t2_row_epoch(const QJsonObject& row, qint64* out) {
    static const char* const kKeys[] = {"timestamp", "time", "date", "datetime", "Date"};
    for (const char* key : kKeys) {
        const QJsonValue v = row.value(key);
        if (v.isDouble()) {
            double t = v.toDouble();
            if (t > 1e11)
                t /= 1000.0; // milliseconds
            *out = static_cast<qint64>(t);
            return true;
        }
        if (v.isString()) {
            QDateTime dt = QDateTime::fromString(v.toString(), Qt::ISODate);
            if (!dt.isValid())
                dt = QDateTime::fromString(v.toString(), QStringLiteral("yyyy-MM-dd HH:mm:ss"));
            if (dt.isValid()) {
                dt.setTimeZone(QTimeZone::utc());
                *out = dt.toSecsSinceEpoch();
                return true;
            }
        }
    }
    return false;
}

// Bucket id for a timestamp at a target frequency; -1 for an unknown frequency.
qint64 t2_bucket(qint64 epoch, const QString& freq) {
    static const QHash<QString, qint64> kSeconds = {{"1m", 60},    {"5m", 300},    {"15m", 900}, {"1h", 3600},
                                                    {"4h", 14400}, {"1d", 86400},  {"1w", 604800}};
    if (freq == "1M") {
        const QDate d = QDateTime::fromSecsSinceEpoch(epoch, QTimeZone::utc()).date();
        return static_cast<qint64>(d.year()) * 12 + (d.month() - 1);
    }
    const qint64 step = kSeconds.value(freq, 0);
    if (step <= 0)
        return -1;
    // Weeks start on Monday: day 0 of the Unix epoch is a Thursday, so shift by 3 days.
    return freq == "1w" ? (epoch + 3 * 86400) / step : epoch / step;
}

// Aggregate time-ordered rows into coarser bars. ohlc_mode: first open / max high / min low /
// last close / summed volume; otherwise the last row of each bucket is kept. Rows that carry no
// usable time are dropped.
QJsonArray t2_resample(const QJsonArray& rows, const QString& freq, bool ohlc_mode) {
    struct Bar {
        QJsonObject last;
        double open = 0, high = 0, low = 0, close = 0, volume = 0;
        bool has_ohlc = false;
        qint64 start = 0;
    };
    QVector<qint64> order;
    QHash<qint64, Bar> bars;
    for (const QJsonValue& rv : rows) {
        if (!rv.isObject())
            continue;
        const QJsonObject row = rv.toObject();
        qint64 epoch = 0;
        if (!t2_row_epoch(row, &epoch))
            continue;
        const qint64 b = t2_bucket(epoch, freq);
        if (b < 0)
            continue;
        const bool has_ohlc = row.value("open").isDouble() && row.value("high").isDouble() &&
                              row.value("low").isDouble() && row.value("close").isDouble();
        auto it = bars.find(b);
        if (it == bars.end()) {
            Bar bar;
            bar.start = epoch;
            if (has_ohlc) {
                bar.open = row.value("open").toDouble();
                bar.high = row.value("high").toDouble();
                bar.low = row.value("low").toDouble();
                bar.close = row.value("close").toDouble();
                bar.has_ohlc = true;
            }
            bar.volume = row.value("volume").toDouble();
            bar.last = row;
            bars.insert(b, bar);
            order.append(b);
        } else {
            Bar& bar = it.value();
            if (has_ohlc) {
                if (!bar.has_ohlc) {
                    bar.open = row.value("open").toDouble();
                    bar.high = row.value("high").toDouble();
                    bar.low = row.value("low").toDouble();
                    bar.has_ohlc = true;
                } else {
                    bar.high = std::max(bar.high, row.value("high").toDouble());
                    bar.low = std::min(bar.low, row.value("low").toDouble());
                }
                bar.close = row.value("close").toDouble();
            }
            bar.volume += row.value("volume").toDouble();
            bar.last = row;
        }
    }

    std::sort(order.begin(), order.end());
    QJsonArray out;
    for (qint64 b : order) {
        const Bar& bar = bars.value(b);
        QJsonObject row = bar.last;
        if (ohlc_mode && bar.has_ohlc) {
            row["open"] = bar.open;
            row["high"] = bar.high;
            row["low"] = bar.low;
            row["close"] = bar.close;
            if (row.contains("volume"))
                row["volume"] = bar.volume;
        }
        // Stamp the bar with the time of its first row.
        if (row.value("timestamp").isDouble())
            row["timestamp"] = static_cast<double>(bar.start);
        out.append(row);
    }
    return out;
}

} // namespace

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
                // The executor here used to be a copy of Reshape's (it read "operation" /
                // "key", which this node does not have) so none of Index / Columns /
                // Values / Aggregation did anything and the input came back unchanged.
                const QString index_field = params.value("index").toString();
                const QString column_field = params.value("columns").toString();
                const QString value_field = params.value("values").toString();
                const QString agg = params.value("agg").toString("sum");
                const QJsonValue data = inputs.isEmpty() ? QJsonValue{} : inputs[0];

                if (!data.isArray()) {
                    cb(false, {}, "transform.pivot needs an array of records as input");
                    return;
                }
                if (index_field.isEmpty() || column_field.isEmpty() || value_field.isEmpty()) {
                    cb(false, {}, "transform.pivot needs the Index, Column and Values fields");
                    return;
                }

                struct PivotCell {
                    double sum = 0;
                    double first = 0;
                    double last = 0;
                    int n = 0;
                };
                QStringList row_order;
                QStringList col_order;
                QHash<QString, QHash<QString, PivotCell>> cells;

                for (const QJsonValue& item : data.toArray()) {
                    if (!item.isObject())
                        continue;
                    const QJsonObject rec = item.toObject();
                    const QString row = ExpressionEngine::value_to_string(rec.value(index_field));
                    const QString col = ExpressionEngine::value_to_string(rec.value(column_field));
                    const QJsonValue raw = rec.value(value_field);
                    const double v = raw.isDouble() ? raw.toDouble() : raw.toString().toDouble();

                    if (!row_order.contains(row))
                        row_order << row;
                    if (!col_order.contains(col))
                        col_order << col;

                    PivotCell& cell = cells[row][col];
                    if (cell.n == 0)
                        cell.first = v;
                    cell.last = v;
                    cell.sum += v;
                    ++cell.n;
                }

                QJsonArray out;
                for (const QString& row : row_order) {
                    QJsonObject out_row;
                    out_row[index_field] = row;
                    for (const QString& col : col_order) {
                        const auto row_it = cells.constFind(row);
                        const auto cell_it = row_it->constFind(col);
                        if (cell_it == row_it->constEnd()) {
                            out_row[col] = QJsonValue::Null;
                            continue;
                        }
                        const PivotCell& c = cell_it.value();
                        double value = c.sum;
                        if (agg == "mean")
                            value = c.n > 0 ? c.sum / c.n : 0.0;
                        else if (agg == "count")
                            value = c.n;
                        else if (agg == "first")
                            value = c.first;
                        else if (agg == "last")
                            value = c.last;
                        out_row[col] = value;
                    }
                    out.append(out_row);
                }
                cb(true, out, {});
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
            [](const QJsonObject& params, const QVector<QJsonValue>& inputs,
               std::function<void(bool, QJsonValue, QString)> cb) {
                // Used to hand the input back unchanged (Target Frequency / OHLC Mode were ignored).
                const QJsonValue data = inputs.isEmpty() ? QJsonValue{} : inputs[0];
                const QString freq = params.value("target_freq").toString("1h");
                const bool ohlc_mode = params.value("ohlc_mode").toBool(true);

                if (!data.isArray()) {
                    cb(true, data, {});
                    return;
                }
                if (t2_bucket(0, freq) < 0) {
                    cb(false, {}, QString("Unknown target frequency '%1'").arg(freq));
                    return;
                }
                const QJsonArray out = t2_resample(data.toArray(), freq, ohlc_mode);
                if (out.isEmpty() && !data.toArray().isEmpty()) {
                    cb(false, {}, "Resample needs rows with a timestamp / date field");
                    return;
                }
                cb(true, out, {});
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
                auto data = inputs.isEmpty() ? QJsonValue{} : inputs[0];
                // TTL in milliseconds, computed in double and clamped (ttl_seconds * 1000 overflowed int).
                const int ttl_ms = static_cast<int>(qBound(1.0, params.value("ttl_seconds").toDouble(300), 2000000.0) * 1000.0);
                QString key = params.value("cache_key").toString();
                if (key.isEmpty()) {
                    // Auto-key from a hash of the WHOLE input. The old key was the first 64 characters
                    // of its JSON, so two different datasets that start alike (rows of the same table)
                    // shared a key and the second one got the first one's data back.
                    const QByteArray json = data.isObject()  ? QJsonDocument(data.toObject()).toJson(QJsonDocument::Compact)
                                            : data.isArray() ? QJsonDocument(data.toArray()).toJson(QJsonDocument::Compact)
                                                             : data.toVariant().toString().toUtf8();
                    key = QString::fromLatin1(QCryptographicHash::hash(json, QCryptographicHash::Sha1).toHex());
                }
                // The shared, bounded, TTL-aware WorkflowCache (CacheManager) replaces a private
                // process-wide hash that never expired anything or shrank.
                auto& cache = WorkflowCache::instance();
                const QString cache_key = QStringLiteral("node:") + key;
                if (cache.has(cache_key)) {
                    cb(true, cache.get(cache_key), {});
                    return;
                }
                cache.put(cache_key, data, ttl_ms);
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
                // QTimer::singleShot() ignores a negative interval, so the node would never
                // report back and the run would hang; clamp to a sane window.
                const int ms = static_cast<int>(qBound(0.0, params.value("delay_ms").toDouble(1000), 86400000.0));
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
                const QString label = params.value("label").toString("DEBUG");
                const QString level = params.value("level").toString("info");

                // "Log data to console/file for debugging" — it never wrote anything to the log;
                // the Level select was ignored too. Long payloads are cut so one node cannot flood it.
                const QString text =
                    QString("[%1] %2").arg(label, ExpressionEngine::value_to_string(data).left(2000));
                if (level == "debug")
                    LOG_DEBUG("WorkflowLog", text);
                else if (level == "warn")
                    LOG_WARN("WorkflowLog", text);
                else if (level == "error")
                    LOG_ERROR("WorkflowLog", text);
                else
                    LOG_INFO("WorkflowLog", text);

                QJsonObject out;
                out["label"] = label;
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

                // Same evaluator as If / Else — the placeholder this node advertises
                // (={{$input.value > 0}}) matched none of the old regex, so with a non-empty
                // condition the assertion silently PASSED; non-object input passed too.
                bool passed = true;
                if (!condition.trimmed().isEmpty())
                    passed = ExpressionEngine::evaluate_condition(condition, data);
                if (!passed)
                    cb(false, {}, QString("%1 [%2]").arg(message, condition.trimmed()));
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
                QString out_key = params.value("output_key").toString("rendered").trimmed();
                if (out_key.isEmpty())
                    out_key = "rendered";
                const QJsonValue input = inputs.isEmpty() ? QJsonValue{} : inputs[0];
                const QJsonObject input_obj = input.isObject() ? input.toObject() : QJsonObject{{"value", input}};

                // {{name}}, {{$input.quote.price}}, {{items[0].p}}, {{prices.sum()}} ... The old
                // \w+-only regex could not follow a path, printed a missing field as "0", and
                // QString::number() cut every number to 6 significant digits.
                const QString rendered = ExpressionEngine::value_to_string(ExpressionEngine::evaluate(tmpl, input_obj));

                QJsonObject out;
                out[out_key] = rendered; // "Output Key" was declared but ignored (always "rendered")
                cb(true, out, {});
            },
    });
}
} // namespace fincept::workflow
