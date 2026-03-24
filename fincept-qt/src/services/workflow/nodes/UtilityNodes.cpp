#include "services/workflow/nodes/UtilityNodes.h"

#include "services/workflow/NodeRegistry.h"

#include <QDateTime>

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
                // Pass through for now — full implementation in Phase 4
                auto data = inputs.isEmpty() ? QJsonValue{} : inputs[0];
                Q_UNUSED(params);
                cb(true, data, {});
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
            [](const QJsonObject&, const QVector<QJsonValue>& inputs,
               std::function<void(bool, QJsonValue, QString)> cb) {
                auto data = inputs.isEmpty() ? QJsonValue{} : inputs[0];
                cb(true, data, {});
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
            [](const QJsonObject&, const QVector<QJsonValue>& inputs,
               std::function<void(bool, QJsonValue, QString)> cb) {
                auto data = inputs.isEmpty() ? QJsonValue{} : inputs[0];
                cb(true, data, {});
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
            [](const QJsonObject&, const QVector<QJsonValue>& inputs,
               std::function<void(bool, QJsonValue, QString)> cb) {
                auto data = inputs.isEmpty() ? QJsonValue{} : inputs[0];
                cb(true, data, {});
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
            [](const QJsonObject&, const QVector<QJsonValue>& inputs,
               std::function<void(bool, QJsonValue, QString)> cb) {
                auto data = inputs.isEmpty() ? QJsonValue{} : inputs[0];
                cb(true, data, {});
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
            [](const QJsonObject&, const QVector<QJsonValue>& inputs,
               std::function<void(bool, QJsonValue, QString)> cb) {
                auto data = inputs.isEmpty() ? QJsonValue{} : inputs[0];
                cb(true, data, {});
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
            [](const QJsonObject&, const QVector<QJsonValue>& inputs,
               std::function<void(bool, QJsonValue, QString)> cb) {
                auto data = inputs.isEmpty() ? QJsonValue{} : inputs[0];
                cb(true, data, {});
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
            [](const QJsonObject&, const QVector<QJsonValue>& inputs,
               std::function<void(bool, QJsonValue, QString)> cb) {
                auto data = inputs.isEmpty() ? QJsonValue{} : inputs[0];
                cb(true, data, {});
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
        .execute = nullptr,
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
            [](const QJsonObject&, const QVector<QJsonValue>& inputs,
               std::function<void(bool, QJsonValue, QString)> cb) {
                cb(true, inputs.isEmpty() ? QJsonValue{} : inputs[0], {});
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
            [](const QJsonObject&, const QVector<QJsonValue>& inputs,
               std::function<void(bool, QJsonValue, QString)> cb) {
                cb(true, inputs.isEmpty() ? QJsonValue{} : inputs[0], {});
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
            [](const QJsonObject&, const QVector<QJsonValue>& inputs,
               std::function<void(bool, QJsonValue, QString)> cb) {
                cb(true, inputs.isEmpty() ? QJsonValue{} : inputs[0], {});
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
            [](const QJsonObject&, const QVector<QJsonValue>& inputs,
               std::function<void(bool, QJsonValue, QString)> cb) {
                cb(true, inputs.isEmpty() ? QJsonValue{} : inputs[0], {});
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
            [](const QJsonObject&, const QVector<QJsonValue>& inputs,
               std::function<void(bool, QJsonValue, QString)> cb) {
                cb(true, inputs.isEmpty() ? QJsonValue{} : inputs[0], {});
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
                // Placeholder: always passes
                auto data = inputs.isEmpty() ? QJsonValue{} : inputs[0];
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
                QJsonObject out;
                out["template"] = params.value("template").toString();
                out["rendered"] = params.value("template").toString(); // placeholder
                if (!inputs.isEmpty())
                    out["input"] = inputs[0];
                cb(true, out, {});
            },
    });
}

} // namespace fincept::workflow
