#include "services/workflow/nodes/DataFormatNodes.h"

#include "services/workflow/NodeRegistry.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QRegularExpression>

namespace fincept::workflow {

namespace {

// Resolve a dot-notation path (e.g. "data.price") through a QJsonObject.
QJsonValue resolve_dot_path(const QJsonObject& root, const QString& path)
{
    QStringList parts = path.split('.', Qt::SkipEmptyParts);
    QJsonValue current = root;
    for (const QString& part : parts) {
        if (!current.isObject()) return QJsonValue{};
        current = current.toObject().value(part);
    }
    return current;
}

} // anonymous namespace

void register_data_format_nodes(NodeRegistry& registry) {
    registry.register_type({
        .type_id = "format.json",
        .display_name = "JSON",
        .category = "Data Format",
        .description = "Parse, stringify, or query JSON data",
        .icon_text = "{}",
        .accent_color = "#0891b2",
        .version = 1,
        .inputs = {{"input_0", "Data In", PortDirection::Input, ConnectionType::Main}},
        .outputs = {{"output_main", "Main", PortDirection::Output, ConnectionType::Main}},
        .parameters =
            {
                {"operation", "Operation", "select", "parse", {"parse", "stringify", "query"}, ""},
                {"query", "JSON Path", "string", "$", {}, "JSONPath query (for query operation)"},
            },
        .execute =
            [](const QJsonObject& params, const QVector<QJsonValue>& inputs,
               std::function<void(bool, QJsonValue, QString)> cb) {
                QJsonValue input = inputs.isEmpty() ? QJsonValue{} : inputs[0];
                QString op = params.value("operation").toString("parse");

                if (op == "stringify") {
                    // Convert the input value to a compact JSON string.
                    QJsonDocument doc;
                    if (input.isObject())      doc = QJsonDocument(input.toObject());
                    else if (input.isArray())  doc = QJsonDocument(input.toArray());
                    else {
                        // Scalar — wrap in a one-field object.
                        QJsonObject wrapper;
                        wrapper["value"] = input;
                        doc = QJsonDocument(wrapper);
                    }
                    QJsonObject out;
                    out["json_string"] = QString::fromUtf8(doc.toJson(QJsonDocument::Compact));
                    cb(true, out, {});
                    return;
                }

                if (op == "parse") {
                    // If input is already an object/array pass it through;
                    // if it is a string, parse it as JSON.
                    if (input.isObject() || input.isArray()) {
                        cb(true, input, {});
                        return;
                    }
                    // Try to find a string value: direct string or object with "text"/"json_string" key.
                    QString raw;
                    if (input.isString()) {
                        raw = input.toString();
                    } else if (input.isObject()) {
                        QJsonObject obj = input.toObject();
                        raw = obj.value("text").toString(obj.value("json_string").toString());
                    }
                    if (raw.isEmpty()) {
                        cb(false, {}, "format.json parse: no string input to parse");
                        return;
                    }
                    auto doc = QJsonDocument::fromJson(raw.toUtf8());
                    if (doc.isNull()) {
                        cb(false, {}, "format.json parse: invalid JSON");
                        return;
                    }
                    cb(true, doc.isObject() ? QJsonValue(doc.object()) : QJsonValue(doc.array()), {});
                    return;
                }

                if (op == "query") {
                    // Dot-notation path extraction.
                    QString path = params.value("query").toString();
                    // Strip leading "$." or "$" sentinel.
                    if (path.startsWith("$.")) path = path.mid(2);
                    else if (path == "$")       { cb(true, input, {}); return; }

                    if (!input.isObject()) {
                        cb(false, {}, "format.json query: input must be an object");
                        return;
                    }
                    QJsonObject out;
                    out["result"] = resolve_dot_path(input.toObject(), path);
                    cb(true, out, {});
                    return;
                }

                // Unknown operation — pass through.
                cb(true, input, {});
            },
    });

    registry.register_type({
        .type_id = "format.xml",
        .display_name = "XML",
        .category = "Data Format",
        .description = "Parse or generate XML data",
        .icon_text = "{}",
        .accent_color = "#0891b2",
        .version = 1,
        .inputs = {{"input_0", "Data In", PortDirection::Input, ConnectionType::Main}},
        .outputs = {{"output_main", "Main", PortDirection::Output, ConnectionType::Main}},
        .parameters =
            {
                {"operation", "Operation", "select", "parse", {"parse", "generate"}, ""},
            },
        .execute =
            [](const QJsonObject&, const QVector<QJsonValue>& inputs,
               std::function<void(bool, QJsonValue, QString)> cb) {
                auto data = inputs.isEmpty() ? QJsonValue{} : inputs[0];
                cb(true, data, {});
            },
    });

    registry.register_type({
        .type_id = "format.html_extract",
        .display_name = "HTML Extract",
        .category = "Data Format",
        .description = "Extract data from HTML using CSS selectors",
        .icon_text = "{}",
        .accent_color = "#0891b2",
        .version = 1,
        .inputs = {{"input_0", "HTML In", PortDirection::Input, ConnectionType::Main}},
        .outputs = {{"output_main", "Main", PortDirection::Output, ConnectionType::Main}},
        .parameters =
            {
                {"selector", "CSS Selector", "string", "", {}, "e.g. div.price > span", true},
                {"attribute", "Attribute", "string", "text", {}, "text, href, src, etc."},
            },
        .execute = nullptr,
    });

    registry.register_type({
        .type_id = "format.compare_datasets",
        .display_name = "Compare Datasets",
        .category = "Data Format",
        .description = "Compare two datasets and find differences",
        .icon_text = "{}",
        .accent_color = "#0891b2",
        .version = 1,
        .inputs =
            {
                {"input_a", "Dataset A", PortDirection::Input, ConnectionType::Main},
                {"input_b", "Dataset B", PortDirection::Input, ConnectionType::Main},
            },
        .outputs =
            {
                {"output_added", "Added", PortDirection::Output, ConnectionType::Main},
                {"output_removed", "Removed", PortDirection::Output, ConnectionType::Main},
                {"output_changed", "Changed", PortDirection::Output, ConnectionType::Main},
            },
        .parameters =
            {
                {"key_field", "Key Field", "string", "id", {}, "Field to match records by"},
            },
        .execute =
            [](const QJsonObject& params, const QVector<QJsonValue>& inputs,
               std::function<void(bool, QJsonValue, QString)> cb) {
                QJsonArray arr_a = (inputs.size() > 0 && inputs[0].isArray())
                    ? inputs[0].toArray() : QJsonArray{};
                QJsonArray arr_b = (inputs.size() > 1 && inputs[1].isArray())
                    ? inputs[1].toArray() : QJsonArray{};

                QString key_field = params.value("key_field").toString("id");

                // Build maps keyed by key_field value.
                QHash<QString, QJsonObject> map_a, map_b;
                for (const QJsonValue& item : arr_a) {
                    if (!item.isObject()) continue;
                    map_a.insert(item.toObject().value(key_field).toVariant().toString(),
                                 item.toObject());
                }
                for (const QJsonValue& item : arr_b) {
                    if (!item.isObject()) continue;
                    map_b.insert(item.toObject().value(key_field).toVariant().toString(),
                                 item.toObject());
                }

                QJsonArray added, removed, changed;

                // Items in B but not A → added.
                for (auto it = map_b.cbegin(); it != map_b.cend(); ++it) {
                    if (!map_a.contains(it.key()))
                        added.append(it.value());
                }
                // Items in A but not B → removed.
                for (auto it = map_a.cbegin(); it != map_a.cend(); ++it) {
                    if (!map_b.contains(it.key()))
                        removed.append(it.value());
                }
                // Items in both but with different values → changed.
                for (auto it = map_a.cbegin(); it != map_a.cend(); ++it) {
                    auto jt = map_b.find(it.key());
                    if (jt == map_b.end()) continue;
                    // Compact JSON comparison — field-order agnostic.
                    QByteArray doc_a = QJsonDocument(it.value()).toJson(QJsonDocument::Compact);
                    QByteArray doc_b = QJsonDocument(jt.value()).toJson(QJsonDocument::Compact);
                    if (doc_a != doc_b) {
                        QJsonObject diff;
                        diff["before"] = it.value();
                        diff["after"]  = jt.value();
                        changed.append(diff);
                    }
                }

                // The node has three output ports; pack all results into one object
                // so the WorkflowExecutor can fan them out to the correct ports.
                QJsonObject out;
                out["added"]   = added;
                out["removed"] = removed;
                out["changed"] = changed;
                cb(true, out, {});
            },
    });

    // ── Tier 2 additions ───────────────────────────────────────────

    registry.register_type({
        .type_id = "format.csv_parse",
        .display_name = "CSV Parse",
        .category = "Data Format",
        .description = "Parse CSV string to structured data",
        .icon_text = "{}",
        .accent_color = "#0891b2",
        .version = 1,
        .inputs = {{"input_0", "CSV In", PortDirection::Input, ConnectionType::Main}},
        .outputs = {{"output_main", "Main", PortDirection::Output, ConnectionType::Main}},
        .parameters =
            {
                {"delimiter", "Delimiter", "select", ",", {",", ";", "\\t", "|"}, ""},
                {"has_header", "Has Header Row", "boolean", true, {}, ""},
                {"encoding", "Encoding", "select", "utf-8", {"utf-8", "ascii", "latin-1"}, ""},
            },
        .execute =
            [](const QJsonObject& params, const QVector<QJsonValue>& inputs,
               std::function<void(bool, QJsonValue, QString)> cb) {
                if (inputs.isEmpty()) { cb(false, {}, "format.csv_parse: no input"); return; }

                // Accept either a raw string or an object with a "text" field.
                QString csv_text;
                QJsonValue input = inputs[0];
                if (input.isString()) {
                    csv_text = input.toString();
                } else if (input.isObject()) {
                    csv_text = input.toObject().value("text").toString(
                               input.toObject().value("content").toString());
                }
                if (csv_text.isEmpty()) {
                    cb(false, {}, "format.csv_parse: empty or non-string input");
                    return;
                }

                QString delim_param = params.value("delimiter").toString(",");
                QChar delimiter = (delim_param == "\\t") ? '\t' : delim_param.at(0);
                bool has_header = params.value("has_header").toBool(true);

                // Split into lines, handling \r\n and \n.
                QStringList lines = csv_text.split(QRegularExpression(R"(\r?\n)"), Qt::SkipEmptyParts);
                if (lines.isEmpty()) { cb(true, QJsonArray{}, {}); return; }

                // Helper: split a CSV line respecting double-quoted fields.
                auto split_csv_line = [&](const QString& line) -> QStringList {
                    QStringList fields;
                    QString field;
                    bool in_quotes = false;
                    for (int i = 0; i < line.size(); ++i) {
                        QChar c = line[i];
                        if (c == '"') {
                            // Handle escaped quote ("") inside a quoted field.
                            if (in_quotes && i + 1 < line.size() && line[i + 1] == '"') {
                                field += '"';
                                ++i;
                            } else {
                                in_quotes = !in_quotes;
                            }
                        } else if (c == delimiter && !in_quotes) {
                            fields.append(field);
                            field.clear();
                        } else {
                            field += c;
                        }
                    }
                    fields.append(field);
                    return fields;
                };

                QJsonArray result;

                if (has_header) {
                    QStringList headers = split_csv_line(lines[0]);
                    for (int row = 1; row < lines.size(); ++row) {
                        QStringList cols = split_csv_line(lines[row]);
                        QJsonObject obj;
                        for (int col = 0; col < headers.size(); ++col) {
                            QString val = (col < cols.size()) ? cols[col] : QString{};
                            // Try to parse as number; fall back to string.
                            bool ok = false;
                            double num = val.toDouble(&ok);
                            if (ok) obj.insert(headers[col], num);
                            else    obj.insert(headers[col], val);
                        }
                        result.append(obj);
                    }
                } else {
                    // No header: return array of arrays.
                    for (const QString& line : lines) {
                        QStringList cols = split_csv_line(line);
                        QJsonArray row_arr;
                        for (const QString& col : cols) {
                            bool ok = false;
                            double num = col.toDouble(&ok);
                            if (ok) row_arr.append(num);
                            else    row_arr.append(col);
                        }
                        result.append(row_arr);
                    }
                }
                cb(true, result, {});
            },
    });

    registry.register_type({
        .type_id = "format.regex_extract",
        .display_name = "Regex Extract",
        .category = "Data Format",
        .description = "Extract data using regular expression patterns",
        .icon_text = "{}",
        .accent_color = "#0891b2",
        .version = 1,
        .inputs = {{"input_0", "Text In", PortDirection::Input, ConnectionType::Main}},
        .outputs = {{"output_main", "Main", PortDirection::Output, ConnectionType::Main}},
        .parameters =
            {
                {"pattern", "Regex Pattern", "string", "", {}, "e.g. (\\d+\\.\\d+)", true},
                {"field", "Source Field", "string", "text", {}, "Field containing text"},
                {"group", "Capture Group", "number", 0, {}, "0=full match, 1+=groups"},
                {"all_matches", "All Matches", "boolean", false, {}, "Return all matches as array"},
            },
        .execute =
            [](const QJsonObject& params, const QVector<QJsonValue>& inputs,
               std::function<void(bool, QJsonValue, QString)> cb) {
                if (inputs.isEmpty()) { cb(false, {}, "format.regex_extract: no input"); return; }

                QString pattern_str = params.value("pattern").toString();
                if (pattern_str.isEmpty()) {
                    cb(false, {}, "format.regex_extract: pattern is required");
                    return;
                }

                QString field      = params.value("field").toString("text");
                int     group      = static_cast<int>(params.value("group").toDouble(0));
                bool    all_matches = params.value("all_matches").toBool(false);

                // Extract source text from input.
                QJsonValue input = inputs[0];
                QString source_text;
                if (input.isString()) {
                    source_text = input.toString();
                } else if (input.isObject()) {
                    QJsonValue fv = input.toObject().value(field);
                    source_text = fv.isString() ? fv.toString() : QString::number(fv.toDouble());
                }

                QRegularExpression re(pattern_str);
                if (!re.isValid()) {
                    cb(false, {}, "format.regex_extract: invalid regex: " + re.errorString());
                    return;
                }

                if (all_matches) {
                    QRegularExpressionMatchIterator it = re.globalMatch(source_text);
                    QJsonArray matches_arr;
                    while (it.hasNext()) {
                        QRegularExpressionMatch m = it.next();
                        int cap_count = m.lastCapturedIndex();
                        if (cap_count == 0 || group == 0) {
                            // Return full match string.
                            matches_arr.append(m.captured(0));
                        } else {
                            // Return requested capture group (if available).
                            matches_arr.append(group <= cap_count ? m.captured(group) : QString{});
                        }
                    }
                    cb(true, matches_arr, {});
                } else {
                    QRegularExpressionMatch m = re.match(source_text);
                    if (!m.hasMatch()) {
                        QJsonObject out;
                        out["match"] = QJsonValue::Null;
                        out["groups"] = QJsonArray{};
                        cb(true, out, {});
                        return;
                    }
                    QJsonArray groups_arr;
                    for (int i = 1; i <= m.lastCapturedIndex(); ++i)
                        groups_arr.append(m.captured(i));

                    QJsonObject out;
                    out["match"]  = m.captured(group <= m.lastCapturedIndex() ? group : 0);
                    out["groups"] = groups_arr;
                    cb(true, out, {});
                }
            },
    });
}

} // namespace fincept::workflow
