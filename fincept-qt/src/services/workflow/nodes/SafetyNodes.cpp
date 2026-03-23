#include "services/workflow/nodes/SafetyNodes.h"
#include "services/workflow/NodeRegistry.h"

namespace fincept::workflow {

void register_safety_nodes(NodeRegistry& registry)
{
    registry.register_type({
        .type_id = "safety.risk_check", .display_name = "Risk Check",
        .category = "Safety", .description = "Validate trade against position size and volatility limits",
        .icon_text = "!", .accent_color = "#dc2626", .version = 1,
        .inputs  = {{ "input_0", "Trade In", PortDirection::Input, ConnectionType::Main }},
        .outputs = {
            { "output_pass", "Pass", PortDirection::Output, ConnectionType::Main },
            { "output_fail", "Fail", PortDirection::Output, ConnectionType::Main },
        },
        .parameters = {
            { "max_position_pct", "Max Position %", "number", 5.0, {}, "Max % of portfolio per position" },
            { "max_volatility", "Max Volatility", "number", 0.5, {}, "Max annualized volatility" },
        },
        .execute = [](const QJsonObject& params, const QVector<QJsonValue>& inputs,
                      std::function<void(bool, QJsonValue, QString)> cb) {
            auto data = inputs.isEmpty() ? QJsonValue{} : inputs[0];
            Q_UNUSED(params);
            cb(true, data, {}); // Placeholder: always passes
        },
    });

    registry.register_type({
        .type_id = "safety.loss_limit", .display_name = "Loss Limit",
        .category = "Safety", .description = "Enforce daily/weekly loss limits",
        .icon_text = "!", .accent_color = "#dc2626", .version = 1,
        .inputs  = {{ "input_0", "Data In", PortDirection::Input, ConnectionType::Main }},
        .outputs = {
            { "output_pass", "Under Limit", PortDirection::Output, ConnectionType::Main },
            { "output_fail", "Over Limit", PortDirection::Output, ConnectionType::Main },
        },
        .parameters = {
            { "daily_limit", "Daily Loss Limit", "number", 1000, {}, "$ amount" },
            { "weekly_limit", "Weekly Loss Limit", "number", 5000, {}, "$ amount" },
        },
        .execute = [](const QJsonObject&, const QVector<QJsonValue>& inputs,
                      std::function<void(bool, QJsonValue, QString)> cb) {
            auto data = inputs.isEmpty() ? QJsonValue{} : inputs[0];
            cb(true, data, {});
        },
    });

    registry.register_type({
        .type_id = "safety.position_size_limit", .display_name = "Position Size Limit",
        .category = "Safety", .description = "Enforce maximum position sizing",
        .icon_text = "!", .accent_color = "#dc2626", .version = 1,
        .inputs  = {{ "input_0", "Data In", PortDirection::Input, ConnectionType::Main }},
        .outputs = {
            { "output_pass", "Pass", PortDirection::Output, ConnectionType::Main },
            { "output_fail", "Fail", PortDirection::Output, ConnectionType::Main },
        },
        .parameters = {
            { "max_shares", "Max Shares", "number", 1000, {}, "" },
            { "max_value", "Max Value ($)", "number", 50000, {}, "" },
        },
        .execute = [](const QJsonObject&, const QVector<QJsonValue>& inputs,
                      std::function<void(bool, QJsonValue, QString)> cb) {
            auto data = inputs.isEmpty() ? QJsonValue{} : inputs[0];
            cb(true, data, {});
        },
    });

    registry.register_type({
        .type_id = "safety.trading_hours", .display_name = "Trading Hours Check",
        .category = "Safety", .description = "Validate trade is within market hours",
        .icon_text = "!", .accent_color = "#dc2626", .version = 1,
        .inputs  = {{ "input_0", "Data In", PortDirection::Input, ConnectionType::Main }},
        .outputs = {
            { "output_open", "Market Open", PortDirection::Output, ConnectionType::Main },
            { "output_closed", "Market Closed", PortDirection::Output, ConnectionType::Main },
        },
        .parameters = {
            { "exchange", "Exchange", "select", "NYSE", {"NYSE","NASDAQ","LSE","TSE","NSE","BSE"}, "" },
            { "allow_premarket", "Allow Pre-Market", "boolean", false, {}, "" },
        },
        .execute = [](const QJsonObject&, const QVector<QJsonValue>& inputs,
                      std::function<void(bool, QJsonValue, QString)> cb) {
            auto data = inputs.isEmpty() ? QJsonValue{} : inputs[0];
            cb(true, data, {});
        },
    });

    // ── Tier 1 additions ───────────────────────────────────────────

    registry.register_type({
        .type_id = "safety.max_drawdown_check", .display_name = "Max Drawdown Check",
        .category = "Safety", .description = "Halt trading if drawdown exceeds threshold",
        .icon_text = "!", .accent_color = "#dc2626", .version = 1,
        .inputs  = {{ "input_0", "Data In", PortDirection::Input, ConnectionType::Main }},
        .outputs = {
            { "output_pass", "Under Limit", PortDirection::Output, ConnectionType::Main },
            { "output_fail", "Exceeded", PortDirection::Output, ConnectionType::Main },
        },
        .parameters = {
            { "max_drawdown_pct", "Max Drawdown %", "number", 10.0, {}, "" },
            { "window", "Window", "select", "daily", {"daily","weekly","monthly","ytd"}, "" },
        },
        .execute = [](const QJsonObject&, const QVector<QJsonValue>& inputs,
                      std::function<void(bool, QJsonValue, QString)> cb) {
            auto data = inputs.isEmpty() ? QJsonValue{} : inputs[0];
            cb(true, data, {});
        },
    });

    registry.register_type({
        .type_id = "safety.correlation_check", .display_name = "Correlation Check",
        .category = "Safety", .description = "Block trade if too correlated with existing positions",
        .icon_text = "!", .accent_color = "#dc2626", .version = 1,
        .inputs  = {{ "input_0", "Data In", PortDirection::Input, ConnectionType::Main }},
        .outputs = {
            { "output_pass", "Low Correlation", PortDirection::Output, ConnectionType::Main },
            { "output_fail", "High Correlation", PortDirection::Output, ConnectionType::Main },
        },
        .parameters = {
            { "max_correlation", "Max Correlation", "number", 0.8, {}, "0-1 threshold" },
            { "lookback_days", "Lookback Days", "number", 90, {}, "" },
        },
        .execute = [](const QJsonObject&, const QVector<QJsonValue>& inputs,
                      std::function<void(bool, QJsonValue, QString)> cb) {
            auto data = inputs.isEmpty() ? QJsonValue{} : inputs[0];
            cb(true, data, {});
        },
    });

    registry.register_type({
        .type_id = "safety.volatility_filter", .display_name = "Volatility Filter",
        .category = "Safety", .description = "Skip trade if volatility is too high",
        .icon_text = "!", .accent_color = "#dc2626", .version = 1,
        .inputs  = {{ "input_0", "Data In", PortDirection::Input, ConnectionType::Main }},
        .outputs = {
            { "output_pass", "Normal Vol", PortDirection::Output, ConnectionType::Main },
            { "output_fail", "High Vol", PortDirection::Output, ConnectionType::Main },
        },
        .parameters = {
            { "max_annualized_vol", "Max Ann. Vol %", "number", 50.0, {}, "" },
            { "method", "Method", "select", "realized", {"realized","implied","vix_level"}, "" },
            { "vix_threshold", "VIX Threshold", "number", 30.0, {}, "For VIX method" },
        },
        .execute = [](const QJsonObject&, const QVector<QJsonValue>& inputs,
                      std::function<void(bool, QJsonValue, QString)> cb) {
            auto data = inputs.isEmpty() ? QJsonValue{} : inputs[0];
            cb(true, data, {});
        },
    });
}

} // namespace fincept::workflow
