#include "services/workflow/nodes/ControlFlowNodes.h"

#include "services/workflow/ExpressionEngine.h"
#include "services/workflow/NodeRegistry.h"

#include <QTimer>
#include <QJsonArray>

namespace fincept::workflow {

void register_control_flow_nodes(NodeRegistry& registry) {
    // ── Switch ─────────────────────────────────────────────────────
    registry.register_type({
        .type_id = "control.switch",
        .display_name = "Switch",
        .category = "Control Flow",
        .description = "Route data to one of multiple outputs based on a value",
        .icon_text = "<>",
        .accent_color = "#808080",
        .version = 1,
        .inputs = {{"input_0", "Data In", PortDirection::Input, ConnectionType::Main}},
        .outputs =
            {
                {"output_0", "Case 1", PortDirection::Output, ConnectionType::Main},
                {"output_1", "Case 2", PortDirection::Output, ConnectionType::Main},
                {"output_2", "Default", PortDirection::Output, ConnectionType::Main},
            },
        .parameters =
            {
                {"field", "Field", "string", "", {}, "Field to switch on"},
                {"case1", "Case 1 Value", "string", "", {}, ""},
                {"case2", "Case 2 Value", "string", "", {}, ""},
            },
        .execute =
            [](const QJsonObject& params, const QVector<QJsonValue>& inputs,
               std::function<void(bool, QJsonValue, QString)> cb) {
                auto data = inputs.isEmpty() ? QJsonValue{} : inputs[0];
                QString field = params.value("field").toString();
                QString case1 = params.value("case1").toString();
                QString case2 = params.value("case2").toString();

                // Extract the field value from the input object
                QString actual;
                if (!field.isEmpty() && data.isObject())
                    actual = data.toObject().value(field).toVariant().toString();
                else if (data.isString())
                    actual = data.toString();
                else
                    actual = QString::number(data.toDouble());

                // Route: case1 → output_0, case2 → output_1, else → output_2 (default)
                // The workflow executor uses the first output by default; we encode
                // the route index in the result so ExecutionResultsPanel can show it.
                // Actual port routing requires WorkflowExecutor support — we annotate
                // the output with "_route" so downstream can branch.
                QJsonObject out = data.isObject() ? data.toObject() : QJsonObject{{"value", data}};
                if (!case1.isEmpty() && actual == case1)
                    out["_route"] = 0;
                else if (!case2.isEmpty() && actual == case2)
                    out["_route"] = 1;
                else
                    out["_route"] = 2;
                cb(true, out, {});
            },
    });

    // ── Loop ───────────────────────────────────────────────────────
    registry.register_type({
        .type_id = "control.loop",
        .display_name = "Loop",
        .category = "Control Flow",
        .description = "Iterate over items in an array",
        .icon_text = "<>",
        .accent_color = "#808080",
        .version = 1,
        .inputs = {{"input_0", "Array In", PortDirection::Input, ConnectionType::Main}},
        .outputs =
            {
                {"output_item", "Item", PortDirection::Output, ConnectionType::Main},
                {"output_done", "Done", PortDirection::Output, ConnectionType::Main},
            },
        .parameters =
            {
                {"max_iterations", "Max Iterations", "number", 100, {}, ""},
            },
        .execute =
            [](const QJsonObject&, const QVector<QJsonValue>& inputs,
               std::function<void(bool, QJsonValue, QString)> cb) {
                // Simplified: pass through the input array as output
                auto data = inputs.isEmpty() ? QJsonValue{} : inputs[0];
                cb(true, data, {});
            },
    });

    // ── Split (parallel) ───────────────────────────────────────────
    registry.register_type({
        .type_id = "control.split",
        .display_name = "Split",
        .category = "Control Flow",
        .description = "Split data flow into parallel branches",
        .icon_text = "<>",
        .accent_color = "#808080",
        .version = 1,
        .inputs = {{"input_0", "Data In", PortDirection::Input, ConnectionType::Main}},
        .outputs =
            {
                {"output_0", "Branch 1", PortDirection::Output, ConnectionType::Main},
                {"output_1", "Branch 2", PortDirection::Output, ConnectionType::Main},
            },
        .parameters = {},
        .execute =
            [](const QJsonObject&, const QVector<QJsonValue>& inputs,
               std::function<void(bool, QJsonValue, QString)> cb) {
                auto data = inputs.isEmpty() ? QJsonValue{} : inputs[0];
                cb(true, data, {});
            },
    });

    // ── Merge ──────────────────────────────────────────────────────
    registry.register_type({
        .type_id = "control.merge",
        .display_name = "Merge",
        .category = "Control Flow",
        .description = "Merge multiple branches into one",
        .icon_text = "<>",
        .accent_color = "#808080",
        .version = 1,
        .inputs =
            {
                {"input_0", "Branch 1", PortDirection::Input, ConnectionType::Main},
                {"input_1", "Branch 2", PortDirection::Input, ConnectionType::Main},
            },
        .outputs = {{"output_main", "Main", PortDirection::Output, ConnectionType::Main}},
        .parameters =
            {
                {"mode", "Mode", "select", "append", {"append", "merge_by_key", "keep_first"}, ""},
            },
        .execute =
            [](const QJsonObject&, const QVector<QJsonValue>& inputs,
               std::function<void(bool, QJsonValue, QString)> cb) {
                // Combine all inputs into an array
                QJsonArray merged;
                for (const auto& input : inputs)
                    merged.append(input);
                cb(true, merged, {});
            },
    });

    // ── Wait ───────────────────────────────────────────────────────
    registry.register_type({
        .type_id = "control.wait",
        .display_name = "Wait",
        .category = "Control Flow",
        .description = "Wait for a specified duration",
        .icon_text = "<>",
        .accent_color = "#808080",
        .version = 1,
        .inputs = {{"input_0", "Data In", PortDirection::Input, ConnectionType::Main}},
        .outputs = {{"output_main", "Main", PortDirection::Output, ConnectionType::Main}},
        .parameters =
            {
                {"seconds", "Seconds", "number", 1.0, {}, "Delay in seconds"},
            },
        .execute =
            [](const QJsonObject& params, const QVector<QJsonValue>& inputs,
               std::function<void(bool, QJsonValue, QString)> cb) {
                int ms = static_cast<int>(params.value("seconds").toDouble(1.0) * 1000);
                auto data = inputs.isEmpty() ? QJsonValue{} : inputs[0];
                QTimer::singleShot(ms, [cb, data]() { cb(true, data, {}); });
            },
    });

    // ── Error Handler ──────────────────────────────────────────────
    registry.register_type({
        .type_id = "control.error_handler",
        .display_name = "Error Handler",
        .category = "Control Flow",
        .description = "Catch errors from upstream nodes",
        .icon_text = "<>",
        .accent_color = "#808080",
        .version = 1,
        .inputs = {{"input_0", "Data In", PortDirection::Input, ConnectionType::Main}},
        .outputs =
            {
                {"output_success", "Success", PortDirection::Output, ConnectionType::Main},
                {"output_error", "Error", PortDirection::Output, ConnectionType::Main},
            },
        .parameters = {},
        .execute =
            [](const QJsonObject&, const QVector<QJsonValue>& inputs,
               std::function<void(bool, QJsonValue, QString)> cb) {
                auto data = inputs.isEmpty() ? QJsonValue{} : inputs[0];
                cb(true, data, {});
            },
    });
}

} // namespace fincept::workflow
