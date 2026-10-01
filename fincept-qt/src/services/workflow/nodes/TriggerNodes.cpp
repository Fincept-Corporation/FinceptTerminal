#include "services/workflow/nodes/TriggerNodes.h"

#include "python/PythonRunner.h"
#include "services/workflow/NodeRegistry.h"

#include <QJsonDocument>

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
                const QString symbol = params.value("symbol").toString().trimmed();
                const QString condition = params.value("condition").toString("above");
                const double threshold = params.value("price").toDouble();
                if (symbol.isEmpty()) {
                    cb(false, {}, "Price Alert needs a symbol");
                    return;
                }

                // This trigger used to report "triggered" unconditionally, so a workflow like
                // "buy AAPL when it is above $200" fired its order at ANY price. A run is a
                // manual "check now" (there is no scheduler), so look at the live quote and only
                // let the workflow continue when the condition actually holds.
                fincept::python::PythonRunner::instance().run(
                    "yfinance_data.py", {"quote", symbol},
                    [cb, symbol, condition, threshold](const fincept::python::PythonResult& res) {
                        if (!res.success) {
                            cb(false, {}, res.error.isEmpty() ? QStringLiteral("Quote request failed") : res.error);
                            return;
                        }
                        const auto doc = QJsonDocument::fromJson(fincept::python::extract_json(res.output).trimmed().toUtf8());
                        const QJsonObject quote = doc.object();
                        const double price = quote.value("price").toDouble(0);
                        if (!doc.isObject() || price <= 0) {
                            cb(false, {}, QString("No price available for %1").arg(symbol));
                            return;
                        }

                        bool met = false;
                        if (condition == "below") {
                            met = price < threshold;
                        } else if (condition == "crosses") {
                            // Crossed since the previous close, in either direction.
                            const double prev = quote.value("previous_close").toDouble(0);
                            met = prev > 0 && ((prev < threshold && price >= threshold) ||
                                               (prev > threshold && price <= threshold));
                        } else {
                            met = price > threshold;
                        }

                        QJsonObject out;
                        out["symbol"] = symbol;
                        out["condition"] = condition;
                        out["price"] = threshold; // the configured threshold (unchanged output key)
                        out["current_price"] = price;
                        out["triggered"] = met;
                        if (!met) {
                            // Not met: nothing downstream should run. "_skipped" is the executor's
                            // marker for a node that produced no data for its consumers.
                            out["_skipped"] = true;
                            out["reason"] = QString("%1 is %2 (alert: %3 %4)")
                                                .arg(symbol)
                                                .arg(price)
                                                .arg(condition)
                                                .arg(threshold);
                        }
                        cb(true, out, {});
                    });
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
