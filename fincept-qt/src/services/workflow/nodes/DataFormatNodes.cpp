#include "services/workflow/nodes/DataFormatNodes.h"

#include "services/workflow/NodeRegistry.h"

namespace fincept::workflow {

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
                auto data = inputs.isEmpty() ? QJsonValue{} : inputs[0];
                Q_UNUSED(params);
                cb(true, data, {});
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
            [](const QJsonObject&, const QVector<QJsonValue>& inputs,
               std::function<void(bool, QJsonValue, QString)> cb) {
                auto data = inputs.isEmpty() ? QJsonValue{} : inputs[0];
                cb(true, data, {});
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
            [](const QJsonObject&, const QVector<QJsonValue>& inputs,
               std::function<void(bool, QJsonValue, QString)> cb) {
                cb(true, inputs.isEmpty() ? QJsonValue{} : inputs[0], {});
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
            [](const QJsonObject&, const QVector<QJsonValue>& inputs,
               std::function<void(bool, QJsonValue, QString)> cb) {
                cb(true, inputs.isEmpty() ? QJsonValue{} : inputs[0], {});
            },
    });
}

} // namespace fincept::workflow
