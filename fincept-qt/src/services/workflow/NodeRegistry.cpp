#include "services/workflow/NodeRegistry.h"

#include "services/workflow/adapters/ServiceBridges.h"
#include "services/workflow/nodes/AgentNodes.h"
#include "services/workflow/nodes/AnalyticsNodes.h"
#include "services/workflow/nodes/ControlFlowNodes.h"
#include "services/workflow/nodes/DataFormatNodes.h"
#include "services/workflow/nodes/FileNodes.h"
#include "services/workflow/nodes/IntegrationNodes.h"
#include "services/workflow/nodes/MarketDataNodes.h"
#include "services/workflow/nodes/NotificationNodes.h"
#include "services/workflow/nodes/SafetyNodes.h"
#include "services/workflow/nodes/TradingNodes.h"
#include "services/workflow/nodes/TriggerNodes.h"
#include "services/workflow/nodes/UtilityNodes.h"
#include "services/workflow/ExpressionEngine.h"

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
            [](const QJsonObject&, const QVector<QJsonValue>&, std::function<void(bool, QJsonValue, QString)> cb) {
                cb(true, QJsonObject{{"triggered", true}}, {});
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
                QJsonObject out = inputs.isEmpty() ? QJsonObject{} : inputs[0].toObject();
                out[params.value("key").toString()] = params.value("value");
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
                    // Strip ={{ }} wrapper if present
                    QString expr = cond_str;
                    if (expr.startsWith("={{") && expr.endsWith("}}"))
                        expr = expr.mid(3, expr.length() - 5).trimmed();

                    // Build context from input data
                    QJsonObject context;
                    if (data.isObject())
                        context = data.toObject();
                    else
                        context["value"] = data;

                    // Parse simple comparisons: field op value
                    // Supported: >, <, >=, <=, ==, !=
                    static QRegularExpression cmp_re(
                        R"(^\s*(\$?[\w.]+)\s*(>=|<=|!=|==|>|<)\s*(.+)\s*$)");
                    auto match = cmp_re.match(expr);

                    if (match.hasMatch()) {
                        QString lhs_path = match.captured(1).trimmed();
                        QString op = match.captured(2);
                        QString rhs_str = match.captured(3).trimmed();

                        // Resolve LHS from context
                        QJsonValue lhs_val = ExpressionEngine::evaluate(
                            QJsonValue("={{" + lhs_path + "}}"), context);
                        double lhs = lhs_val.toDouble(0);
                        double rhs = rhs_str.toDouble();

                        if (op == ">")       result = lhs > rhs;
                        else if (op == "<")  result = lhs < rhs;
                        else if (op == ">=") result = lhs >= rhs;
                        else if (op == "<=") result = lhs <= rhs;
                        else if (op == "==") result = lhs == rhs;
                        else if (op == "!=") result = lhs != rhs;
                    } else {
                        // Single value expression — resolve and check truthiness
                        QJsonValue val = ExpressionEngine::evaluate(
                            QJsonValue("={{" + expr + "}}"), context);
                        if (val.isBool())
                            result = val.toBool();
                        else if (val.isDouble())
                            result = val.toDouble() != 0.0;
                        else if (val.isString())
                            result = !val.toString().isEmpty() && val.toString() != "false";
                        else
                            result = !val.isNull() && !val.isUndefined();
                    }
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

    // Wire service bridges (connects nullptr executors to real services)
    wire_all_bridges(*this);
}

} // namespace fincept::workflow
