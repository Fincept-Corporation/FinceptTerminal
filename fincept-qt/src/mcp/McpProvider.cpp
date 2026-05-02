// McpProvider.cpp — Internal tool registry and executor (Qt port)

#include "mcp/McpProvider.h"

#include "core/logging/Logger.h"
#include "mcp/SchemaValidator.h"

#include <QCoreApplication>
#include <QFutureWatcher>
#include <QPromise>
#include <QRegularExpression>
#include <QSet>
#include <QThread>
#include <QTimer>

#include <atomic>
#include <memory>

namespace fincept::mcp {

static constexpr const char* TAG = "McpProvider";

McpProvider& McpProvider::instance() {
    static McpProvider s;
    return s;
}

// ============================================================================
// Registration
// ============================================================================

void McpProvider::register_tool(ToolDef tool) {
    QMutexLocker lock(&mutex_);
    QString name = tool.name;
    tools_.insert(name, std::move(tool));
    ++generation_;
}

void McpProvider::register_tools(std::vector<ToolDef> tools) {
    QMutexLocker lock(&mutex_);
    for (auto& t : tools) {
        QString name = t.name;
        tools_.insert(name, std::move(t));
    }
    ++generation_;
}

void McpProvider::unregister_tool(const QString& name) {
    QMutexLocker lock(&mutex_);
    tools_.remove(name);
    ++generation_;
}

// ============================================================================
// Enable / Disable
// ============================================================================

void McpProvider::set_tool_enabled(const QString& name, bool enabled) {
    QMutexLocker lock(&mutex_);
    if (enabled)
        disabled_tools_.remove(name);
    else
        disabled_tools_.insert(name);
    ++generation_;
}

bool McpProvider::is_tool_enabled(const QString& name) const {
    QMutexLocker lock(&mutex_);
    return !disabled_tools_.contains(name);
}

// ============================================================================
// Discovery
// ============================================================================

std::vector<UnifiedTool> McpProvider::list_tools() const {
    QMutexLocker lock(&mutex_);
    std::vector<UnifiedTool> result;
    result.reserve(static_cast<std::size_t>(tools_.size()));

    for (auto it = tools_.cbegin(); it != tools_.cend(); ++it) {
        if (disabled_tools_.contains(it.key()))
            continue;
        const auto& t = it.value();
        result.push_back({QString(INTERNAL_SERVER_ID), QString(INTERNAL_SERVER_NAME), t.name, t.description,
                          t.input_schema.to_json(), true, t.category, t.is_destructive});
    }
    return result;
}

std::vector<UnifiedTool> McpProvider::list_all_tools() const {
    QMutexLocker lock(&mutex_);
    std::vector<UnifiedTool> result;
    result.reserve(static_cast<std::size_t>(tools_.size()));

    for (auto it = tools_.cbegin(); it != tools_.cend(); ++it) {
        const auto& t = it.value();
        result.push_back({QString(INTERNAL_SERVER_ID), QString(INTERNAL_SERVER_NAME), t.name, t.description,
                          t.input_schema.to_json(), true, t.category, t.is_destructive});
    }
    return result;
}

std::size_t McpProvider::tool_count() const {
    QMutexLocker lock(&mutex_);
    std::size_t count = 0;
    for (auto it = tools_.cbegin(); it != tools_.cend(); ++it) {
        if (!disabled_tools_.contains(it.key()))
            ++count;
    }
    return count;
}

bool McpProvider::has_tool(const QString& name) const {
    QMutexLocker lock(&mutex_);
    return tools_.contains(name);
}

// ============================================================================
// Execution
// ============================================================================

ToolResult McpProvider::call_tool(const QString& name, const QJsonObject& args) {
    // Sync entry point. If the registered tool is async, we still want to
    // give legacy callers (LlmService, TerminalToolBridge, workflow nodes)
    // a blocking ToolResult — so we dispatch async and wait. The caller is
    // already on a background thread (every existing call site is) so
    // blocking here is safe; .result() drains the QFuture.
    auto future = call_tool_async(name, args);
    future.waitForFinished();
    if (future.resultCount() == 0)
        return ToolResult::fail("Tool '" + name + "' produced no result");
    return future.result();
}

QFuture<ToolResult> McpProvider::call_tool_async(const QString& name, const QJsonObject& args, ToolContext ctx) {
    ToolHandler sync_handler;
    AsyncToolHandler async_handler;
    ToolSchema schema;
    int default_timeout_ms = 30000;
    AuthLevel auth_required = AuthLevel::None;
    bool is_destructive = false;

    {
        QMutexLocker lock(&mutex_);

        auto fail_now = [](const QString& msg) {
            QPromise<ToolResult> p;
            p.start();
            p.addResult(ToolResult::fail(msg));
            p.finish();
            return p.future();
        };

        // Phase 6.10: try canonical name first, then any registered alias.
        // Aliases let us migrate names without breaking saved chats / Finagent
        // workflows that reference the old name.
        QString resolved = name;
        if (!tools_.contains(resolved)) {
            for (auto it = tools_.constBegin(); it != tools_.constEnd(); ++it) {
                if (it.value().legacy_aliases.contains(name)) {
                    LOG_INFO(TAG, QString("Tool called by legacy name '%1' — canonical '%2'")
                                      .arg(name, it.key()));
                    resolved = it.key();
                    break;
                }
            }
        }

        if (!tools_.contains(resolved))
            return fail_now("Tool not found: " + name);
        if (disabled_tools_.contains(resolved))
            return fail_now("Tool is disabled: " + resolved);

        const auto& def = tools_[resolved];
        sync_handler = def.handler;
        async_handler = def.async_handler;
        schema = def.input_schema;
        default_timeout_ms = def.default_timeout_ms;
        auth_required = def.auth_required;
        is_destructive = def.is_destructive;

        if (!async_handler && !sync_handler)
            return fail_now("Tool '" + resolved + "' has no handler");
    }

    // Phase 6.3: authorization gate. We don't import AuthManager here to
    // avoid pulling auth headers into McpTypes.h consumers — instead we
    // expose a hook that the app installs at startup.
    //
    // No-checker semantics:
    //   • AuthLevel <= Authenticated and is_destructive flag → log + pass
    //     (the flag is a hint for the Phase 6.12 modal; not a hard gate
    //     until that UI lands)
    //   • AuthLevel >= Verified → fail closed (genuine privilege escalation
    //     that must not happen unauthenticated)
    if (auth_required != AuthLevel::None || is_destructive) {
        AuthChecker checker;
        {
            QMutexLocker lock(&mutex_);
            checker = auth_checker_;
        }
        if (checker) {
            if (!checker(auth_required, is_destructive)) {
                LOG_WARN(TAG, QString("Tool '%1' blocked: auth_required=%2 is_destructive=%3")
                                  .arg(name, auth_level_str(auth_required))
                                  .arg(is_destructive ? "true" : "false"));
                QPromise<ToolResult> p;
                p.start();
                p.addResult(ToolResult::fail(QString("Tool '%1' requires %2 auth")
                                                 .arg(name, auth_level_str(auth_required))));
                p.finish();
                return p.future();
            }
        } else if (auth_required >= AuthLevel::Verified) {
            // Fail-closed: Verified/Subscribed/ExplicitConfirm cannot be
            // safely evaluated without a checker. Refuse the call.
            LOG_WARN(TAG, QString("Tool '%1' blocked: no AuthChecker registered (required=%2)")
                              .arg(name, auth_level_str(auth_required)));
            QPromise<ToolResult> p;
            p.start();
            p.addResult(ToolResult::fail("Tool requires user confirmation but no authorisation hook is installed"));
            p.finish();
            return p.future();
        } else if (is_destructive) {
            // Advisory log only — the modal that prompts on this flag is
            // Phase 6.12 work. Tools tagged Authenticated+destructive still
            // run today; once the modal ships, install the checker and the
            // upper branch starts gating them.
            LOG_INFO(TAG, QString("Tool '%1' is destructive (flag is advisory; install McpProvider::set_auth_checker to gate)")
                              .arg(name));
        }
    }

    // Phase 3: validate + inject defaults before invoking the handler.
    QJsonObject normalized = args;
    auto vr = validate_args(schema, normalized);
    if (vr.is_err()) {
        LOG_WARN(TAG, QString("Tool '%1' rejected: %2").arg(name, QString::fromStdString(vr.error())));
        QPromise<ToolResult> p;
        p.start();
        p.addResult(ToolResult::fail(QString::fromStdString(vr.error())));
        p.finish();
        return p.future();
    }

    // Per-call timeout overrides ToolDef::default_timeout_ms when supplied
    // (Phase 4 framework; Phase 6 wires _meta.timeout_ms here).
    if (ctx.timeout_ms == 30000) // ToolContext default — not explicitly set
        ctx.timeout_ms = default_timeout_ms;

    // Async preferred; fall back to sync wrapped in an immediately-resolved
    // future so call_tool() works uniformly for legacy handlers.
    if (async_handler) {
        auto promise = std::make_shared<QPromise<ToolResult>>();
        promise->start();

        // Cancellation flag — set by the timeout timer or by a caller-supplied
        // hook. Wired into ctx.is_cancelled below.
        auto cancelled = std::make_shared<std::atomic<bool>>(false);
        if (!ctx.is_cancelled) {
            ctx.is_cancelled = [cancelled]() { return cancelled->load(); };
        } else {
            // Compose: original hook OR our internal flag.
            auto orig = ctx.is_cancelled;
            ctx.is_cancelled = [orig, cancelled]() { return cancelled->load() || orig(); };
        }

        // Arm a one-shot timeout. We post the timer onto the QApplication
        // thread so it ticks even if the caller returns immediately.
        auto* watchdog = new QTimer;
        watchdog->setSingleShot(true);
        watchdog->moveToThread(qApp->thread());
        QObject::connect(watchdog, &QTimer::timeout, watchdog, [promise, cancelled, watchdog, name]() {
            if (!promise->future().isFinished()) {
                cancelled->store(true);
                LOG_WARN(TAG, QString("Tool '%1' timed out").arg(name));
                promise->addResult(ToolResult::fail("Tool '" + name + "' timed out"));
                promise->finish();
            }
            watchdog->deleteLater();
        });
        QMetaObject::invokeMethod(watchdog, [watchdog, ms = ctx.timeout_ms]() {
            watchdog->start(ms);
        }, Qt::QueuedConnection);

        try {
            LOG_DEBUG(TAG, "Calling async tool: " + name);
            async_handler(normalized, ctx, promise);
        } catch (const std::exception& e) {
            LOG_ERROR(TAG, QString("Async tool '%1' threw: %2").arg(name, e.what()));
            if (!promise->future().isFinished()) {
                promise->addResult(ToolResult::fail(QString("Tool execution error: ") + e.what()));
                promise->finish();
            }
        } catch (...) {
            LOG_ERROR(TAG, QString("Async tool '%1' threw unknown exception").arg(name));
            if (!promise->future().isFinished()) {
                promise->addResult(ToolResult::fail("Unknown error during tool execution"));
                promise->finish();
            }
        }
        return promise->future();
    }

    // Legacy sync path — invoke immediately, wrap in a resolved future.
    QPromise<ToolResult> p;
    p.start();
    try {
        LOG_DEBUG(TAG, "Calling sync tool: " + name);
        p.addResult(sync_handler(normalized));
    } catch (const std::exception& e) {
        LOG_ERROR(TAG, QString("Tool '%1' threw exception: %2").arg(name, e.what()));
        p.addResult(ToolResult::fail(QString("Tool execution error: ") + e.what()));
    } catch (...) {
        LOG_ERROR(TAG, QString("Tool '%1' threw unknown exception").arg(name));
        p.addResult(ToolResult::fail("Unknown error during tool execution"));
    }
    p.finish();
    return p.future();
}

// ============================================================================
// LLM Integration
// ============================================================================

QJsonArray McpProvider::format_tools_for_openai() const {
    auto tools = list_tools();
    QJsonArray result;

    for (const auto& tool : tools) {
        QJsonObject schema = tool.input_schema;
        if (schema.isEmpty()) {
            schema["type"] = "object";
            schema["properties"] = QJsonObject();
        }

        QJsonObject fn;
        fn["name"] = QString(INTERNAL_SERVER_ID) + "__" + encode_tool_name_for_wire(tool.name);
        fn["description"] = tool.description;
        fn["parameters"] = schema;

        QJsonObject entry;
        entry["type"] = "function";
        entry["function"] = fn;
        result.append(entry);
    }

    return result;
}

QPair<QString, QString> McpProvider::parse_openai_function_name(const QString& fn_name) {
    int pos = fn_name.indexOf("__");
    if (pos <= 0 || pos >= fn_name.length() - 2)
        return {"", ""};
    return {fn_name.left(pos), decode_tool_name_from_wire(fn_name.mid(pos + 2))};
}

// Tightest common-subset regex for tool function names across every supported
// provider (see header for provenance). Each candidate emitted by
// encode_tool_name_for_wire MUST match this regex; non-matching names are
// logged so they can be renamed in source.
static const QRegularExpression& common_subset_regex() {
    static const QRegularExpression re(QStringLiteral("^[a-zA-Z][a-zA-Z0-9_-]{0,63}$"));
    return re;
}

QString McpProvider::encode_tool_name_for_wire(const QString& tool_name) {
    // Step 1: replace '.' with "-dot-". Internal tool names never contain
    // hyphens (verified across all 239 tools), so the round-trip is unique.
    QString out = tool_name;
    out.replace(QLatin1Char('.'), QStringLiteral("-dot-"));

    // Step 2: validate against the common subset. We intentionally do not
    // attempt to repair non-conforming names — any tool whose name fails this
    // check needs to be renamed in source, not silently mangled at the wire
    // boundary (mangled names break the model's ability to call the tool by
    // the catalog-advertised string). Log once per unique offender.
    if (!common_subset_regex().match(out).hasMatch()) {
        static QSet<QString> warned;
        if (!warned.contains(out)) {
            warned.insert(out);
            LOG_WARN("McpProvider",
                     QString("Tool name '%1' does not match the provider-safe regex "
                             "^[a-zA-Z][a-zA-Z0-9_-]{0,63}$ — Kimi/Anthropic/OpenAI will "
                             "reject the request. Rename the tool at source.")
                         .arg(out));
        }
    }
    return out;
}

QString McpProvider::decode_tool_name_from_wire(const QString& wire_name) {
    QString out = wire_name;
    out.replace(QStringLiteral("-dot-"), QStringLiteral("."));
    return out;
}

// ============================================================================
// Lifecycle
// ============================================================================

void McpProvider::clear() {
    QMutexLocker lock(&mutex_);
    tools_.clear();
    disabled_tools_.clear();
    ++generation_;
}

// ============================================================================
// Authorization (Phase 6.3)
// ============================================================================

void McpProvider::set_auth_checker(AuthChecker checker) {
    QMutexLocker lock(&mutex_);
    auth_checker_ = std::move(checker);
}

// ============================================================================
// Generation Counter
// ============================================================================

quint64 McpProvider::generation() const {
    QMutexLocker lock(&mutex_);
    return generation_;
}

} // namespace fincept::mcp
