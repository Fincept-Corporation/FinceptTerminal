#include "services/workflow/nodes/ControlFlowNodes.h"

#include "services/workflow/ExpressionEngine.h"
#include "services/workflow/NodeRegistry.h"

#include <QJsonArray>
#include <QTimer>

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
        .description = "Emit array items on the Item port (capped by Max Iterations); Done signals completion",
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
            [](const QJsonObject& params, const QVector<QJsonValue>& inputs,
               std::function<void(bool, QJsonValue, QString)> cb) {
                // The executor is a single-pass acyclic DAG — it can't re-run the
                // downstream subgraph once per item. Instead: normalise the input to
                // an array, apply the max_iterations cap, and split the two output
                // ports via the "_loop" annotation that WorkflowExecutor::collect_inputs
                // understands — "output_item" gets the (capped) items, "output_done"
                // gets a completion summary.
                const QJsonValue in = inputs.isEmpty() ? QJsonValue{} : inputs[0];
                QJsonArray items;
                if (in.isArray())
                    items = in.toArray();
                else if (!in.isNull() && !in.isUndefined())
                    items.append(in);

                const int max_it = params.value("max_iterations").toInt(100);
                if (max_it >= 0 && items.size() > max_it) {
                    QJsonArray capped;
                    for (int i = 0; i < max_it; ++i)
                        capped.append(items.at(i));
                    items = capped;
                }

                QJsonObject out;
                out["_loop"] = true;
                out["items"] = items;
                out["count"] = items.size();
                out["done"] = QJsonObject{{"count", items.size()}, {"completed", true}};
                cb(true, out, {});
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
                {"input_2", "Branch 3", PortDirection::Input, ConnectionType::Main},
                {"input_3", "Branch 4", PortDirection::Input, ConnectionType::Main},
            },
        .outputs = {{"output_main", "Main", PortDirection::Output, ConnectionType::Main}},
        .parameters =
            {
                {"mode", "Mode", "select", "append", {"append", "merge_by_key", "keep_first"}, ""},
            },
        .execute =
            [](const QJsonObject& params, const QVector<QJsonValue>& inputs,
               std::function<void(bool, QJsonValue, QString)> cb) {
                // The "Mode" select was never read: every merge was an append.
                const QString mode = params.value("mode").toString("append");

                if (mode == "keep_first") {
                    // First branch that actually delivered data wins.
                    for (const auto& input : inputs) {
                        if (!input.isNull() && !input.isUndefined()) {
                            cb(true, input, {});
                            return;
                        }
                    }
                    cb(true, QJsonValue{}, {});
                    return;
                }

                if (mode == "merge_by_key") {
                    // Combine object branches into one object; later branches win on a key clash.
                    // (Anything that is not an object cannot be merged by key and falls through
                    // to the append behaviour.)
                    bool all_objects = !inputs.isEmpty();
                    for (const auto& input : inputs)
                        all_objects = all_objects && input.isObject();
                    if (all_objects) {
                        QJsonObject combined;
                        for (const auto& input : inputs) {
                            const QJsonObject o = input.toObject();
                            for (auto it = o.constBegin(); it != o.constEnd(); ++it)
                                combined.insert(it.key(), it.value());
                        }
                        cb(true, combined, {});
                        return;
                    }
                }

                // append: combine all inputs into an array
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
                // A negative interval never fires (the run would hang) and a huge one overflows
                // int; clamp to 0..24 h.
                const int ms = static_cast<int>(qBound(0.0, params.value("seconds").toDouble(1.0), 86400.0) * 1000.0);
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
                // WorkflowExecutor feeds this node an error record ({_error, error, node_id,
                // node_name}) when an upstream node failed. Annotate the branch so
                // output_error / output_success carry data only on the matching side.
                QJsonValue data = inputs.isEmpty() ? QJsonValue{} : inputs[0];
                bool had_error = false;
                for (const auto& in : inputs) {
                    if (in.isObject() && in.toObject().value("_error").toBool(false)) {
                        data = in;
                        had_error = true;
                        break;
                    }
                }
                QJsonObject out = data.isObject() ? data.toObject() : QJsonObject{{"value", data}};
                out["_branch"] = had_error ? "false" : "true";
                cb(true, out, {});
            },
    });
}

} // namespace fincept::workflow
