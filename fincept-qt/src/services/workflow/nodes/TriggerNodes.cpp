#include "services/workflow/nodes/TriggerNodes.h"

#include "services/workflow/NodeRegistry.h"

namespace fincept::workflow {

void register_trigger_nodes(NodeRegistry& registry) {
    // ManualTrigger — already registered as builtin in NodeRegistry constructor.
    // ScheduleTrigger — already registered as builtin.

    // ── Price Alert Trigger ────────────────────────────────────────
    registry.register_type({
        .type_id = "trigger.price_alert",
        .display_name = "Price Alert",
        .category = "Triggers",
        .description = "Trigger when price crosses a threshold",
        .icon_text = ">>",
        .accent_color = "#d97706",
        .version = 1,
        .inputs = {},
        .outputs = {{"output_main", "Main", PortDirection::Output, ConnectionType::Main}},
        .parameters =
            {
                {"symbol", "Symbol", "string", "AAPL", {}, "Ticker symbol", true},
                {"condition", "Condition", "select", "above", {"above", "below", "crosses"}, ""},
                {"price", "Price", "number", 0.0, {}, "Threshold price", true},
            },
        .execute =
            [](const QJsonObject& params, const QVector<QJsonValue>&,
               std::function<void(bool, QJsonValue, QString)> cb) {
                QJsonObject out;
                out["symbol"] = params.value("symbol").toString();
                out["condition"] = params.value("condition").toString();
                out["price"] = params.value("price").toDouble();
                out["triggered"] = true;
                cb(true, out, {});
            },
    });

    // ── News Event Trigger ─────────────────────────────────────────
    registry.register_type({
        .type_id = "trigger.news_event",
        .display_name = "News Event",
        .category = "Triggers",
        .description = "Trigger on news keyword match",
        .icon_text = ">>",
        .accent_color = "#d97706",
        .version = 1,
        .inputs = {},
        .outputs = {{"output_main", "Main", PortDirection::Output, ConnectionType::Main}},
        .parameters =
            {
                {"keywords", "Keywords", "string", "", {}, "Comma-separated keywords", true},
            },
        .execute =
            [](const QJsonObject& params, const QVector<QJsonValue>&,
               std::function<void(bool, QJsonValue, QString)> cb) {
                QJsonObject out;
                out["keywords"] = params.value("keywords").toString();
                out["triggered"] = true;
                cb(true, out, {});
            },
    });

    // ── Webhook Trigger ────────────────────────────────────────────
    registry.register_type({
        .type_id = "trigger.webhook",
        .display_name = "Webhook",
        .category = "Triggers",
        .description = "Trigger from external webhook call",
        .icon_text = ">>",
        .accent_color = "#d97706",
        .version = 1,
        .inputs = {},
        .outputs = {{"output_main", "Main", PortDirection::Output, ConnectionType::Main}},
        .parameters =
            {
                {"path", "Path", "string", "/webhook", {}, "/my-hook"},
                {"method", "Method", "select", "POST", {"GET", "POST", "PUT"}, ""},
            },
        .execute =
            [](const QJsonObject& params, const QVector<QJsonValue>&,
               std::function<void(bool, QJsonValue, QString)> cb) {
                QJsonObject out;
                out["path"] = params.value("path").toString();
                out["method"] = params.value("method").toString();
                out["triggered"] = true;
                cb(true, out, {});
            },
    });

    // ── Tier 1 additions ───────────────────────────────────────────

    registry.register_type({
        .type_id = "trigger.cron_market",
        .display_name = "Market Hours Cron",
        .category = "Triggers",
        .description = "Scheduled trigger that only fires during market hours",
        .icon_text = ">>",
        .accent_color = "#d97706",
        .version = 1,
        .inputs = {},
        .outputs = {{"output_main", "Main", PortDirection::Output, ConnectionType::Main}},
        .parameters =
            {
                {"cron", "Cron Expression", "string", "*/15 * * * *", {}, ""},
                {"exchange", "Exchange", "select", "NYSE", {"NYSE", "NASDAQ", "LSE", "TSE", "NSE"}, ""},
                {"include_premarket", "Include Pre-Market", "boolean", false, {}, ""},
            },
        .execute =
            [](const QJsonObject& params, const QVector<QJsonValue>&,
               std::function<void(bool, QJsonValue, QString)> cb) {
                QJsonObject out;
                out["cron"] = params.value("cron").toString();
                out["exchange"] = params.value("exchange").toString();
                out["triggered"] = true;
                cb(true, out, {});
            },
    });

    registry.register_type({
        .type_id = "trigger.portfolio_drift",
        .display_name = "Portfolio Drift",
        .category = "Triggers",
        .description = "Trigger when portfolio drifts from target allocation",
        .icon_text = ">>",
        .accent_color = "#d97706",
        .version = 1,
        .inputs = {},
        .outputs = {{"output_main", "Main", PortDirection::Output, ConnectionType::Main}},
        .parameters =
            {
                {"drift_threshold_pct",
                 "Drift Threshold %",
                 "number",
                 5.0,
                 {},
                 "Fire when any position drifts by this %"},
                {"check_interval_min", "Check Interval (min)", "number", 60, {}, ""},
            },
        .execute =
            [](const QJsonObject& params, const QVector<QJsonValue>&,
               std::function<void(bool, QJsonValue, QString)> cb) {
                QJsonObject out;
                out["drift_threshold"] = params.value("drift_threshold_pct").toDouble();
                out["triggered"] = true;
                cb(true, out, {});
            },
    });
}

} // namespace fincept::workflow
