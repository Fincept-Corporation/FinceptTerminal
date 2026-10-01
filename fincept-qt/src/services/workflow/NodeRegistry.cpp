#include "services/workflow/NodeRegistry.h"

#include "services/workflow/ExpressionEngine.h"
#include "services/workflow/adapters/ServiceBridges.h"
#include "services/workflow/nodes/AgentNodes.h"
#include "services/workflow/nodes/AnalyticsNodes.h"
#include "services/workflow/nodes/ControlFlowNodes.h"
#include "services/workflow/nodes/DataFormatNodes.h"
#include "services/workflow/nodes/DataSourceNodes.h"
#include "services/workflow/nodes/FileNodes.h"
#include "services/workflow/nodes/IntegrationNodes.h"
#include "services/workflow/nodes/MarketDataNodes.h"
#include "services/workflow/nodes/NotificationNodes.h"
#include "services/workflow/nodes/SafetyNodes.h"
#include "services/workflow/nodes/TradingNodes.h"
#include "services/workflow/nodes/TriggerNodes.h"
#include "services/workflow/nodes/UtilityNodes.h"

#include <QRegularExpression>

namespace fincept::workflow {

NodeRegistry& NodeRegistry::instance() {
    static NodeRegistry inst;
    return inst;
}

NodeRegistry::NodeRegistry() {
    register_builtin_nodes();
}

void NodeRegistry::register_type(NodeTypeDef def) {
    registry_.insert(def.type_id, std::move(def));
}

const NodeTypeDef* NodeRegistry::find(const QString& type_id) const {
    auto it = registry_.constFind(type_id);
    return it != registry_.constEnd() ? &(*it) : nullptr;
}

QVector<NodeTypeDef> NodeRegistry::all() const {
    QVector<NodeTypeDef> result;
    result.reserve(registry_.size());
    for (auto it = registry_.constBegin(); it != registry_.constEnd(); ++it)
        result.append(it.value());
    return result;
}

QVector<NodeTypeDef> NodeRegistry::by_category(const QString& category) const {
    QVector<NodeTypeDef> result;
    for (auto it = registry_.constBegin(); it != registry_.constEnd(); ++it) {
        if (it->category == category)
            result.append(it.value());
    }
    return result;
}

QStringList NodeRegistry::categories() const {
    QStringList cats;
    for (auto it = registry_.constBegin(); it != registry_.constEnd(); ++it) {
        if (!cats.contains(it->category))
            cats.append(it->category);
    }
    return cats;
}

void NodeRegistry::register_builtin_nodes() {
    // ── Triggers ───────────────────────────────────────────────────────
    register_type({
        .type_id = "trigger.manual",
        .display_name = "Manual Trigger",
        .category = "Triggers",
        .description = "Start workflow manually",
        .icon_text = ">>",
        .accent_color = "#d97706",
        .version = 1,
        .inputs = {},
        .outputs = {{"output_main", "Main", PortDirection::Output, ConnectionType::Main}},
        .parameters = {},
        .execute =
            [](const QJsonObject&, const QVector<QJsonValue>& inputs, std::function<void(bool, QJsonValue, QString)> cb) {
                QJsonObject out{{"triggered", true}};
                // Started by a parent workflow (control.execute_workflow)? The executor hands
                // its payload to entry nodes — pass it on instead of discarding it.
                if (!inputs.isEmpty() && !inputs[0].isNull() && !inputs[0].isUndefined()) {
                    if (inputs[0].isObject()) {
                        const QJsonObject in = inputs[0].toObject();
                        for (auto it = in.constBegin(); it != in.constEnd(); ++it)
                            out.insert(it.key(), it.value());
                    } else {
                        out["input"] = inputs[0];
                    }
                }
                cb(true, out, {});
            },
    });

    register_type({
        .type_id = "trigger.schedule",
        .display_name = "Schedule Trigger",
        .category = "Triggers",
        .description = "Start workflow on a schedule",
        .icon_text = ">>",
        .accent_color = "#d97706",
        .version = 1,
        .inputs = {},
        .outputs = {{"output_main", "Main", PortDirection::Output, ConnectionType::Main}},
        .parameters =
            {
                {"cron", "Cron Expression", "string", "*/5 * * * *", {}, "e.g. */5 * * * *"},
            },
        .execute =
            [](const QJsonObject& params, const QVector<QJsonValue>&,
               std::function<void(bool, QJsonValue, QString)> cb) {
                QJsonObject out;
                out["cron"] = params.value("cron").toString("*/5 * * * *");
                out["triggered"] = true;
                cb(true, out, {});
            },
    });

    // ── Output ─────────────────────────────────────────────────────────
    register_type({
        .type_id = "output.results_display",
        .display_name = "Results Display",
        .category = "Core",
        .description = "Display execution results — attach at end of workflow",
        .icon_text = ">>",
        .accent_color = "#f59e0b",
        .version = 1,
        .inputs = {{"input_0", "Data In", PortDirection::Input, ConnectionType::Main}},
        .outputs = {},
        .parameters =
            {
                {"label", "Label", "string", "Output", {}, "Display label"},
            },
        .execute =
            [](const QJsonObject& params, const QVector<QJsonValue>& inputs,
               std::function<void(bool, QJsonValue, QString)> cb) {
                QJsonObject out;
                out["label"] = params.value("label").toString("Output");
                out["data"] = inputs.isEmpty() ? QJsonValue{} : inputs[0];
                out["display"] = true;
                cb(true, out, {});
            },
    });

    // ── Core ───────────────────────────────────────────────────────────
    register_type({
        .type_id = "core.set",
        .display_name = "Set Variable",
        .category = "Core",
        .description = "Set a key-value pair in the data flow",
        .icon_text = "=",
        .accent_color = "#e5e5e5",
        .version = 1,
        .inputs = {{"input_0", "Data In", PortDirection::Input, ConnectionType::Main}},
        .outputs = {{"output_main", "Main", PortDirection::Output, ConnectionType::Main}},
        .parameters =
            {
                {"key", "Key", "string", "", {}, "Variable name", true},
                {"value", "Value", "string", "", {}, "Variable value", true},
            },
        .execute =
            [](const QJsonObject& params, const QVector<QJsonValue>& inputs,
               std::function<void(bool, QJsonValue, QString)> cb) {
                const QString key = params.value("key").toString().trimmed();
                if (key.isEmpty()) {
                    cb(false, {}, "Set Variable needs a Key"); // an empty key silently wrote a junk "" field
                    return;
                }
                QJsonObject out = inputs.isEmpty() ? QJsonObject{} : inputs[0].toObject();

                // The Value field is plain text, but gates and analytics read numbers: a text "10"
                // is 0 to every toDouble() downstream (Risk Check then fails closed on quantity 0).
                // A value that is exactly a plain number or true/false is stored as that type;
                // anything else, and numbers with leading zeros (IDs, zip codes), stay text.
                QJsonValue value = params.value("value");
                if (value.isString()) {
                    const QString text = value.toString().trimmed();
                    bool is_number = false;
                    const double d = text.toDouble(&is_number);
                    const bool leading_zero = text.size() > 1 && text[0] == QLatin1Char('0') && text[1].isDigit();
                    if (text == QLatin1String("true") || text == QLatin1String("false"))
                        value = (text == QLatin1String("true"));
                    else if (is_number && !leading_zero && !text.isEmpty() &&
                             (text[0].isDigit() || text[0] == QLatin1Char('-') || text[0] == QLatin1Char('.')))
                        value = d;
                }
                out[key] = value;
                cb(true, out, {});
            },
    });

    // ── Control Flow ───────────────────────────────────────────────────
    register_type({
        .type_id = "control.if_else",
        .display_name = "If / Else",
        .category = "Control Flow",
        .description = "Branch based on a condition",
        .icon_text = "<>",
        .accent_color = "#808080",
        .version = 1,
        .inputs = {{"input_0", "Data In", PortDirection::Input, ConnectionType::Main}},
        .outputs =
            {
                {"output_true", "True", PortDirection::Output, ConnectionType::Main},
                {"output_false", "False", PortDirection::Output, ConnectionType::Main},
            },
        .parameters =
            {
                {"condition", "Condition", "expression", "", {}, "={{$input.value > 0}}"},
            },
        .execute =
            [](const QJsonObject& params, const QVector<QJsonValue>& inputs,
               std::function<void(bool, QJsonValue, QString)> cb) {
                auto data = inputs.isEmpty() ? QJsonValue{} : inputs[0];
                QString cond_str = params.value("condition").toString().trimmed();

                bool result = false;

                if (cond_str.isEmpty()) {
                    // No condition — evaluate truthiness of input
                    if (data.isBool())
                        result = data.toBool();
                    else if (data.isDouble())
                        result = data.toDouble() != 0.0;
                    else if (data.isString())
                        result = !data.toString().isEmpty();
                    else if (data.isObject() || data.isArray())
                        result = true;
                } else {
                    // The shared evaluator handles comparisons (numeric when both sides are
                    // numbers, exact text otherwise), contains / startsWith / endsWith,
                    // && / ||, paths with [n] and helper calls, and bare-path truthiness.
                    // The old inline parser compared every right-hand side with
                    // QString::toDouble(), so `status == active` read as `0 == 0` and was
                    // TRUE for any string — an If/Else on text always took the true branch.
                    result = ExpressionEngine::evaluate_condition(cond_str, data);
                }

                // Annotate the output with the branch taken
                QJsonObject out;
                if (data.isObject())
                    out = data.toObject();
                else
                    out["value"] = data;
                out["_branch"] = result ? "true" : "false";
                cb(true, out, {});
            },
    });

    // ── Utilities ──────────────────────────────────────────────────────
    register_type({
        .type_id = "utility.http_request",
        .display_name = "HTTP Request",
        .category = "Utilities",
        .description = "Make an HTTP request",
        .icon_text = "#",
        .accent_color = "#808080",
        .version = 1,
        .inputs = {{"input_0", "Data In", PortDirection::Input, ConnectionType::Main}},
        .outputs = {{"output_main", "Main", PortDirection::Output, ConnectionType::Main}},
        .parameters =
            {
                {"url", "URL", "string", "", {}, "https://api.example.com", true},
                {"method", "Method", "select", "GET", {"GET", "POST", "PUT", "DELETE"}, ""},
                {"headers", "Headers", "json", QJsonObject{}, {}, "{}"},
                {"body", "Body", "json", QJsonObject{}, {}, "{}"},
            },
        .execute = nullptr, // wired in Phase 3 via HttpClient
    });

    register_type({
        .type_id = "utility.code",
        .display_name = "Code",
        .category = "Utilities",
        .description = "Execute custom code",
        .icon_text = "#",
        .accent_color = "#808080",
        .version = 1,
        .inputs = {{"input_0", "Data In", PortDirection::Input, ConnectionType::Main}},
        .outputs = {{"output_main", "Main", PortDirection::Output, ConnectionType::Main}},
        .parameters =
            {
                {"language", "Language", "select", "python", {"python", "javascript"}, ""},
                {"code", "Code", "code", "", {}, "# Write your code here"},
            },
        .execute = nullptr, // wired in Phase 3 via PythonRunner
    });

    // ── Register additional node categories ────────────────────────
    register_trigger_nodes(*this);
    register_control_flow_nodes(*this);
    register_utility_nodes(*this);
    register_market_data_nodes(*this);
    register_trading_nodes(*this);
    register_analytics_nodes(*this);
    register_safety_nodes(*this);
    register_notification_nodes(*this);
    register_agent_nodes(*this);
    register_file_nodes(*this);
    register_data_format_nodes(*this);
    register_integration_nodes(*this);

    // ── Register Data Source connectors dynamically ────────────────
    register_datasource_nodes(*this);

    // Wire service bridges (connects nullptr executors to real services)
    wire_all_bridges(*this);
}

} // namespace fincept::workflow
