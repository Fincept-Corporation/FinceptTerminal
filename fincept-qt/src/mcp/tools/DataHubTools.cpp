// DataHubTools.cpp — DataHub introspection MCP tools.
//
// Exposes the in-process DataHub pub/sub layer to LLM tool callers so
// models can inspect live topic state, subscriber counts, and last-value
// snapshots during debugging / agent workflows.

#include "mcp/tools/DataHubTools.h"

#include "core/logging/Logger.h"
#include "datahub/DataHub.h"
#include "mcp/ToolSchemaBuilder.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QElapsedTimer>
#include <QJsonArray>
#include <QJsonDocument>
#include <QPointer>
#include <QPromise>
#include <QTimer>
#include <QVariantList>

#include <memory>

namespace fincept::mcp::tools {

static constexpr const char* TAG = "DataHubTools";


static QJsonValue variant_to_json(const QVariant& v) {
    // Try the direct QVariant -> QJsonValue path first; falls back to
    // string representation for types Qt can't natively serialise
    // (custom structs registered via Q_DECLARE_METATYPE with no JSON
    // converter). Bounded to avoid unbounded LLM context bloat.
    auto j = QJsonValue::fromVariant(v);
    if (j.isNull() && v.isValid()) {
        const QString s = v.toString();
        return s.left(4096);
    }
    if (j.isString()) {
        const QString s = j.toString();
        if (s.size() > 4096) return QJsonValue(s.left(4096) + "...[truncated]");
    }
    return j;
}


std::vector<ToolDef> get_datahub_tools() {
    std::vector<ToolDef> tools;

    // ── datahub_list_topics ─────────────────────────────────────────────
    {
        ToolDef t;
        t.name = "datahub_list_topics";
        t.description = "List every active DataHub topic with subscriber counts and "
                        "last-publish age. Useful to see what live data the terminal "
                        "is currently streaming.";
        t.category = "datahub";
        t.handler = [](const QJsonObject&) -> ToolResult {
            const auto snap = fincept::datahub::DataHub::instance().stats();
            const qint64 now = QDateTime::currentMSecsSinceEpoch();
            QJsonArray arr;
            for (const auto& s : snap) {
                QJsonObject e;
                e["topic"] = s.topic;
                e["subscribers"] = s.subscriber_count;
                e["publishes"] = s.total_publishes;
                e["push_only"] = s.push_only;
                e["in_flight"] = s.in_flight;
                e["age_ms"] = s.last_publish_ms > 0 ? static_cast<qint64>(now - s.last_publish_ms) : -1;
                arr.append(e);
            }
            return ToolResult::ok_data(arr);
        };
        tools.push_back(std::move(t));
    }

    // ── datahub_peek ────────────────────────────────────────────────────
    {
        ToolDef t;
        t.name = "datahub_peek";
        t.description = "Read the current cached value for a DataHub topic without "
                        "triggering a refresh. Returns {value, age_ms} — check age_ms "
                        "before trusting the value for fresh decisions. Returns null "
                        "if the topic is unknown or has never been published.";
        t.category = "datahub";
        t.input_schema.properties = QJsonObject{
            {"topic", QJsonObject{{"type", "string"}, {"description", "Topic name (e.g. market:quote:AAPL)"}}}};
        t.input_schema.required = {"topic"};
        t.handler = [](const QJsonObject& args) -> ToolResult {
            const QString topic = args["topic"].toString().trimmed();
            if (topic.isEmpty()) return ToolResult::fail("Missing 'topic'");
            auto& hub = fincept::datahub::DataHub::instance();
            // peek_raw: MCP diagnostic tool shows stale data rather than
            // nothing — callers use stats() / last_publish_ms to judge freshness.
            const QVariant v = hub.peek_raw(topic);
            QJsonObject out;
            out["topic"] = topic;
            if (!v.isValid()) {
                out["value"] = QJsonValue::Null;
                out["age_ms"] = -1;
                return ToolResult::ok("Topic unknown or unset", out);
            }
            qint64 age = -1;
            for (const auto& s : hub.stats()) {
                if (s.topic == topic) {
                    age = QDateTime::currentMSecsSinceEpoch() - s.last_publish_ms;
                    break;
                }
            }
            out["value"] = variant_to_json(v);
            out["age_ms"] = age;
            return ToolResult::ok_data(out);
        };
        tools.push_back(std::move(t));
    }

    // ── datahub_request ─────────────────────────────────────────────────
    {
        ToolDef t;
        t.name = "datahub_request";
        t.description = "Ask the DataHub to refresh a topic now. Returns immediately — "
                        "the refresh runs asynchronously and subscribers are notified "
                        "when the producer publishes. Use 'force' to bypass the topic's "
                        "min_interval rate gate.";
        t.category = "datahub";
        t.input_schema.properties = QJsonObject{
            {"topic", QJsonObject{{"type", "string"}, {"description", "Topic name to refresh"}}},
            {"force", QJsonObject{{"type", "boolean"}, {"description", "Bypass min_interval_ms (default false)"}}}};
        t.input_schema.required = {"topic"};
        t.handler = [](const QJsonObject& args) -> ToolResult {
            const QString topic = args["topic"].toString().trimmed();
            if (topic.isEmpty()) return ToolResult::fail("Missing 'topic'");
            const bool force = args["force"].toBool(false);
            fincept::datahub::DataHub::instance().request(topic, force);
            return ToolResult::ok(QString("Refresh requested for %1 (force=%2)")
                                      .arg(topic, force ? "true" : "false"),
                                  QJsonObject{{"topic", topic}, {"force", force}});
        };
        tools.push_back(std::move(t));
    }

    // ── datahub_subscribe_briefly ───────────────────────────────────────
    // Phase 4 streaming exemplar (replaces the previous nested-QEventLoop
    // hack). Async handler: subscribes a heap-allocated owner to the topic
    // on the DataHub's thread, collects samples until duration elapses or
    // cancellation fires, then resolves the promise with the accumulated
    // payload. No worker-thread blocking, no nested event loops.
    {
        ToolDef t;
        t.name = "datahub_subscribe_briefly";
        t.description = "Subscribe to a topic for a short window and return every value "
                        "delivered during that window. Duration clamped to 100-30000 ms. "
                        "Useful for sampling a volatile topic (e.g. a live ticker) to "
                        "reason about its recent behaviour.";
        t.category = "datahub";
        t.input_schema = ToolSchemaBuilder()
            .string("topic", "Topic name (e.g. market:quote:AAPL)")
                .required().length(1, 256)
            .integer("duration_ms", "Window length in milliseconds")
                .required().between(100, 30000)
            .build();
        // Generous timeout = max duration + slack. Provider's watchdog kicks
        // in only if the unsubscribe-and-resolve dance gets stuck.
        t.default_timeout_ms = 35000;
        t.async_handler = [](const QJsonObject& args, ToolContext ctx,
                              std::shared_ptr<QPromise<ToolResult>> promise) {
            const QString topic = args["topic"].toString().trimmed();
            int duration = std::clamp(args["duration_ms"].toInt(), 100, 30000);

            // Marshal the entire setup onto qApp's thread so:
            //   • DataHub::subscribe runs on the hub's thread (consistent
            //     with how the rest of the codebase uses it)
            //   • the owner QObject's affinity is qApp — its deleteLater
            //     posts to a thread that actually has a running event loop
            //     (the worker thread that called the handler may have no
            //     event loop and would leak the owner)
            QMetaObject::invokeMethod(qApp, [topic, duration, ctx, promise]() {
                auto* owner = new QObject;
                owner->setObjectName("mcp.subscribe_briefly:" + topic);

                auto collected = std::make_shared<QJsonArray>();
                auto clock = std::make_shared<QElapsedTimer>();
                clock->start();

                fincept::datahub::DataHub::instance().subscribe(
                    owner, topic, [collected, clock](const QVariant& v) {
                        QJsonObject e;
                        e["t_ms"] = static_cast<qint64>(clock->elapsed());
                        e["value"] = variant_to_json(v);
                        collected->append(e);
                    });

                QTimer::singleShot(duration, qApp, [topic, duration, collected, owner, promise, ctx]() {
                    fincept::datahub::DataHub::instance().unsubscribe(owner);
                    owner->deleteLater();

                    if (promise->future().isFinished()) return; // watchdog already fired
                    QJsonObject out;
                    out["topic"] = topic;
                    out["duration_ms"] = duration;
                    out["count"] = collected->size();
                    out["samples"] = *collected;
                    if (ctx.cancelled()) {
                        promise->addResult(ToolResult::fail("cancelled"));
                    } else {
                        promise->addResult(ToolResult::ok_data(out));
                    }
                    promise->finish();
                });
            }, Qt::QueuedConnection);
        };
        tools.push_back(std::move(t));
    }

    LOG_INFO(TAG, QString("Defined %1 DataHub introspection tools").arg(tools.size()));
    return tools;
}

} // namespace fincept::mcp::tools
