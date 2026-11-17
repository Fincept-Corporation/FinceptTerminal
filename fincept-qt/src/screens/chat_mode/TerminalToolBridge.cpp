#include "screens/chat_mode/TerminalToolBridge.h"

#include "auth/AuthManager.h"
#include "core/logging/Logger.h"
#include "mcp/McpProvider.h"
#include "mcp/TerminalMcpBridge.h"
#include "mcp/server/CallApproval.h"
#include "network/http/HttpClient.h"
#include "screens/chat_mode/ChatModeService.h"
#include "services/agent_tasks/TaskRunService.h"

#include <algorithm>
#include <optional>

#include <QCoreApplication>
#include <QDateTime>
#include <QFuture>
#include <QFutureWatcher>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPointer>
#include <QtConcurrent>

namespace fincept::chat_mode {

namespace tool_bridge_detail {

QString tool_bridge_preview(const QJsonObject& result) {
    QString text = result.value(QStringLiteral("success")).toBool() ? result.value(QStringLiteral("message")).toString()
                                                                      : result.value(QStringLiteral("error")).toString();
    if (text.isEmpty() && result.contains(QStringLiteral("data")))
        text = QString::fromUtf8(QJsonDocument(QJsonObject{{QStringLiteral("data"), result.value(QStringLiteral("data"))}})
                                     .toJson(QJsonDocument::Compact));
    text = text.simplified();
    return text.size() > 300 ? text.left(299) + QChar(0x2026) : text;
}

QString tool_bridge_args_preview(const QJsonObject& args) {
    QStringList parts;
    for (auto it = args.constBegin(); it != args.constEnd() && parts.size() < 3; ++it) {
        QString v = it.value().toString();
        if (!it.value().isString()) // [value] -> value
            v = QString::fromUtf8(QJsonDocument(QJsonArray{it.value()}).toJson(QJsonDocument::Compact)).mid(1).chopped(1);
        parts << QStringLiteral("%1=%2").arg(it.key(), v.left(80));
    }
    return parts.join(QStringLiteral(", "));
}

} // namespace tool_bridge_detail

using namespace tool_bridge_detail;

// The cloud agent (Finagent, chat mode) may look at the terminal — the ui-read
// tools: overview, snapshot, read, wait, screenshot — but never drive it.
// Operating the user's screens ("ui-control": ui_act / ui_invoke) and opening
// or rearranging them ("navigation": ui_open / ui_workspace) stay with agents
// running on this machine, where risky steps are approved in the terminal.
const QStringList TerminalToolBridge::EXCLUDED_CATEGORIES = {"navigation", "ui-control"};

// No whole category is blocked beyond the exclusions above.
const QStringList TerminalToolBridge::BLOCKED_CATEGORIES = {};

// The bundled Python scripts run local code with the user's stored API keys
// (and the exchange/ ones can place orders): never on a remote agent's say-so.
const QStringList TerminalToolBridge::BLOCKED_TOOLS = {"python"};

bool TerminalToolBridge::is_remote_blocked(const QString& name, const QString& category) {
    // EXCLUDED_CATEGORIES was previously applied ONLY while building the
    // catalog in register_tools(). Catalog-time filtering is cosmetic: the
    // backend hands us a tool NAME to run, so anything the model already knows
    // the name of (a stale catalog, a guess, an injected instruction) executed
    // regardless. Folding the exclusion in here makes both the catalog and the
    // dispatch path (execute_call) enforce one list.
    // "ai-chat" is excluded by register_tools() as a recursion guard (the cloud
    // agent driving the chat LLM that is driving it); mirror that at dispatch.
    return category == QLatin1String("ai-chat") || category == QLatin1String("ai_chat") ||
           EXCLUDED_CATEGORIES.contains(category) || BLOCKED_CATEGORIES.contains(category) ||
           BLOCKED_TOOLS.contains(name);
}

TerminalToolBridge& TerminalToolBridge::instance() {
    // Heap-allocated with the application as parent: it is deleted while the
    // event loop machinery still exists, not at static destruction.
    static QPointer<TerminalToolBridge> s;
    if (!s)
        s = new TerminalToolBridge(QCoreApplication::instance());
    return *s;
}

TerminalToolBridge::TerminalToolBridge(QObject* parent) : QObject(parent) {
    poll_timer_ = new QTimer(this);
    poll_timer_->setInterval(kTickMs);
    connect(poll_timer_, &QTimer::timeout, this, &TerminalToolBridge::on_poll_tick);

    stop_timer_ = new QTimer(this);
    stop_timer_->setSingleShot(true);
    stop_timer_->setInterval(static_cast<int>(kActiveTailMs));
    connect(stop_timer_, &QTimer::timeout, this, [this]() {
        if (!wanted())
            stop();
    });

    // A cloud run changes how often a tool call can arrive — and a run keeps the
    // bridge alive with Chat Mode hidden or the window locked. (Server background
    // tasks are gone: long tasks are runs.)
    auto& runs = agent_tasks::TaskRunService::instance();
    connect(&runs, &agent_tasks::TaskRunService::run_added, this, [this](const QString&) { on_runs_changed(); });
    connect(&runs, &agent_tasks::TaskRunService::run_updated, this, [this](const QString&) { on_runs_changed(); });
    connect(&runs, &agent_tasks::TaskRunService::run_finished, this, [this](const QString&) { on_runs_changed(); });

    // A changed auth state (session token dropped after a 401, re-login, key
    // re-validated) or a lifted IP block is the moment a backed-off bridge may try
    // again right away.
    connect(&auth::AuthManager::instance(), &auth::AuthManager::auth_state_changed, this,
            &TerminalToolBridge::reset_backoff);
    connect(&HttpClient::instance(), &HttpClient::ip_block_changed, this, [this](bool blocked, const QString&) {
        if (blocked) {
            LOG_WARN("TerminalToolBridge", "Fincept API is blocking this IP — tool polling paused");
        } else {
            LOG_INFO("TerminalToolBridge", "IP block lifted — tool polling resumes");
            reset_backoff();
        }
    });
}

void TerminalToolBridge::set_view_visible(bool visible) {
    view_visible_ = std::max(0, view_visible_ + (visible ? 1 : -1));
    update_activation();
}

QStringList TerminalToolBridge::calling_runs() const {
    auto& svc = agent_tasks::TaskRunService::instance();
    QStringList out;
    for (const QString& id : svc.active_run_ids(QString::fromLatin1(agent_tasks::kEngineCloud))) {
        const agent_tasks::TaskRun* r = svc.find_run(id);
        if (r && agent_tasks::holds_slot(r->status))
            out << id;
    }
    return out;
}

bool TerminalToolBridge::wanted() const {
    return view_visible_ > 0 || runs_active_;
}

void TerminalToolBridge::update_activation() {
    if (wanted()) {
        stop_timer_->stop();
        if (!active_)
            start();
    } else if (active_ && !stop_timer_->isActive()) {
        stop_timer_->start(); // calls can trail the end of a run
    }
}

void TerminalToolBridge::on_runs_changed() {
    const bool runs = !calling_runs().isEmpty();
    const bool was = runs_active_;
    runs_active_ = runs;
    const bool now = runs_active_;
    update_activation();
    if (now != was)
        on_agent_activity(now);
}

void TerminalToolBridge::start() {
    if (active_)
        return;
    active_ = true;
    LOG_INFO("TerminalToolBridge", QString("Starting tool bridge (%1 view(s), cloud run %2)")
                                       .arg(view_visible_)
                                       .arg(runs_active_ ? "active" : "idle"));
    // Always register afresh on entry: the backend drops a registration 300 s after
    // it was made, so one from an earlier visit to chat mode has usually expired
    // even though the tool generation is unchanged.
    last_register_ok_ms_ = 0;
    last_poll_ms_ = 0; // poll right away
    offered_count_ = 0;
    rejected_.clear();
    confirmed_ = false;
    visibility_known_ = false;
    agent_visible_ = false;
    bridge_unavailable_ = false;
    reregister_requested_ = false;
    register_error_.clear();
    reset_backoff();
    refresh_status();
    poll_timer_->start();
    emit active_changed(true);
    on_poll_tick();
}

void TerminalToolBridge::stop() {
    if (!active_)
        return;
    active_ = false;
    poll_timer_->stop();
    stop_timer_->stop();
    confirmed_ = false;
    LOG_INFO("TerminalToolBridge", "Stopped tool bridge (no Chat Mode view, run or task left)");
    emit active_changed(false);
}

void TerminalToolBridge::reset_backoff() {
    poll_failures_ = 0;
    poll_not_before_ms_ = 0;
    register_retry_not_before_ms_ = 0;
}

void TerminalToolBridge::on_agent_activity(bool active) {
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    if (active) {
        if (active_)
            on_poll_tick(); // a call may come any moment: switch to the fast cadence now
    } else {
        // Calls can still trail the end of a run (a tool started late, a slow close).
        fast_until_ms_ = std::max(fast_until_ms_, now + kActiveTailMs);
    }
}

bool TerminalToolBridge::wants_fast_poll(qint64 now) const {
    // While the server's bridge is down no call can be handed out: keep to the idle
    // cadence, which is enough to notice it is back.
    if (bridge_unavailable_)
        return false;
    return runs_active_ || now < fast_until_ms_;
}

static bool tool_bridge_same_status(const ToolBridgeStatus& a, const ToolBridgeStatus& b) {
    if (a.state != b.state || a.offered != b.offered || a.detail != b.detail || a.rejected.size() != b.rejected.size())
        return false;
    for (int i = 0; i < a.rejected.size(); ++i) {
        if (a.rejected[i].name != b.rejected[i].name || a.rejected[i].reason != b.rejected[i].reason)
            return false;
    }
    return true;
}

void TerminalToolBridge::set_status(ToolBridgeStatus status) {
    if (tool_bridge_same_status(status, status_))
        return;
    status_ = std::move(status);
    emit tools_status_changed(status_);
}

void TerminalToolBridge::refresh_status() {
    using State = ToolBridgeStatus::State;
    ToolBridgeStatus s;
    s.offered = offered_count_;
    s.rejected = rejected_;
    if (bridge_unavailable_) {
        s.state = State::Unavailable;
        s.detail = register_error_;
    } else if (last_register_ok_ms_ == 0) {
        // Never registered in this visit: nothing to show until a registration is
        // attempted, and "not registered" (with the reason) once one has failed.
        s.state = register_error_.isEmpty() ? State::Off : State::NotRegistered;
        s.detail = register_error_;
    } else if (reregister_requested_) {
        s.state = State::NotRegistered;
        s.detail = register_error_.isEmpty()
                       ? tr("The server no longer holds the registration; registering again.")
                       : register_error_;
    } else if (visibility_known_) {
        if (offered_count_ <= 0) {
            s.state = State::NotRegistered;
            s.detail = tr("The server accepted none of the tools.");
        } else {
            // Fresh on every poll: the agent service can go away and come back.
            s.state = agent_visible_ ? State::Usable : State::AgentUnreachable;
        }
    } else if (offered_count_ > 0) {
        // Older server: it cannot say whether the agent sees the registration, so the
        // tools are advertised only once the agent has called one.
        s.state = confirmed_ ? State::Usable : State::Unconfirmed;
    } else {
        s.state = State::Off;
    }
    set_status(std::move(s));
}

// ── Register tools ───────────────────────────────────────────────────────────

void TerminalToolBridge::register_tools() {
    auto& provider = mcp::McpProvider::instance();
    const quint64 gen = provider.generation();

    // A registration is already on the wire — its result decides whether another is needed.
    if (register_in_flight_)
        return;

    auto all_tools = provider.list_tools();
    QJsonArray tools_json;
    int blocked = 0;

    for (const auto& tool : all_tools) {
        // Phase 6.5: filter by ToolDef.category, not by name-prefix.
        // Pre-Phase-6 the check was tool.name.startsWith("navigation_") etc.
        // — but tool names don't follow that prefix convention, so the
        // filter never matched and every navigation/system/settings tool
        // got uploaded to the cloud. UnifiedTool.category (populated by
        // McpProvider::list_tools) is the source of truth.
        if (EXCLUDED_CATEGORIES.contains(tool.category))
            continue;
        // Recursive AiChat tools — skip by category, not by name prefix.
        if (tool.category == "ai-chat" || tool.category == "ai_chat")
            continue;
        // Fail closed on real-money / arbitrary-code tools — the cloud agent
        // has no equivalent of the local agent's destructive opt-in token.
        if (is_remote_blocked(tool.name, tool.category)) {
            ++blocked;
            continue;
        }

        // The backend's TerminalToolDef (apiback chat.py) takes {name, description,
        // category?, input_schema?}; the agent's model is Anthropic, which requires
        // input_schema to be a JSON Schema OBJECT with a properties map. Internal tools
        // already serialise that way; external MCP servers' schemas may omit either.
        QJsonObject schema = tool.input_schema;
        if (schema.value(QStringLiteral("type")).toString() != QLatin1String("object"))
            schema[QStringLiteral("type")] = QStringLiteral("object");
        if (!schema.value(QStringLiteral("properties")).isObject())
            schema[QStringLiteral("properties")] = QJsonObject();

        QJsonObject t;
        t["name"] = tool.name;
        t["description"] = tool.description;
        if (!tool.category.isEmpty())
            t["category"] = tool.category;
        t["input_schema"] = schema;
        tools_json.append(t);
    }

    const int count = tools_json.size();
    LOG_INFO("TerminalToolBridge",
             QString("Registering %1 tools (gen %2, %3 withheld as unsafe for remote execution)")
                 .arg(count)
                 .arg(gen)
                 .arg(blocked));

    register_in_flight_ = true;
    QPointer<TerminalToolBridge> self = this;
    ChatModeService::instance().register_terminal_tools(
        tools_json, "4.0.0", count,
        [self, gen](bool ok, ToolRegistration reg, QString err, bool unavailable, int retry_after_s) {
            if (!self)
                return;
            self->register_in_flight_ = false;
            if (!ok) {
                LOG_DEBUG("TerminalToolBridge", "Registration failed: " + err); // ChatModeService logs the failure
                // Not signed in / backend down: the tick would re-register every 3 s for as
                // long as chat mode is open. Back off — 30 s, or the server's Retry-After
                // when it is longer.
                self->register_retry_not_before_ms_ =
                    QDateTime::currentMSecsSinceEpoch() + std::max<qint64>(30000, qint64(retry_after_s) * 1000);
                // 503 terminal_bridge_unavailable: the server's bridge is down — wait for a
                // poll to say `available: true` instead of retrying the registration.
                if (unavailable)
                    self->bridge_unavailable_ = true;
                const bool new_streak = self->register_error_ != err;
                self->register_error_ = err;
                // Older servers: whatever the backend held may lapse now — stop claiming the
                // tools work. Newer ones say on the next poll whether it still exists.
                if (!self->visibility_known_)
                    self->confirmed_ = false;
                self->refresh_status();
                if (new_streak) // not every 30 s retry: the header label shows the lasting state
                    emit self->bridge_error(tr("The terminal's tools could not be registered with the cloud agent: %1")
                                                .arg(err));
                return;
            }
            self->last_gen_ = gen;
            self->last_register_ok_ms_ = QDateTime::currentMSecsSinceEpoch();
            self->register_error_.clear();
            self->reregister_requested_ = false;
            self->bridge_unavailable_ = false;
            self->offered_count_ = reg.registered;
            self->rejected_ = reg.rejected;
            if (reg.visibility_known) {
                self->visibility_known_ = true;
                self->agent_visible_ = reg.agent_visible;
            }
            LOG_INFO("TerminalToolBridge",
                     QString("Registered %1 tools, %2 rejected — %3")
                         .arg(reg.registered)
                         .arg(reg.rejected.size())
                         .arg(!reg.visibility_known ? QStringLiteral("the server cannot say whether the agent sees them")
                              : reg.agent_visible   ? QStringLiteral("visible to the agent")
                                                    : QStringLiteral("the agent service is not running")));
            for (const auto& r : std::as_const(reg.rejected))
                LOG_WARN("TerminalToolBridge", QString("Tool not accepted: %1 (%2)").arg(r.name, r.reason));
            self->refresh_status();
        });
}

// ── Poll for pending calls ───────────────────────────────────────────────────

void TerminalToolBridge::on_poll_tick() {
    if (!active_)
        return;

    // Signed out: there is no key to send, and a request without one only 401s.
    const auto& auth = auth::AuthManager::instance();
    if (!auth.is_authenticated() || auth.session().api_key.isEmpty())
        return;
    // The Fincept API is refusing this IP (403 IP_BLOCKED, after too many 401s from
    // it): every request would be refused too. Wait for the block to lift.
    if (HttpClient::instance().is_ip_blocked())
        return;

    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    auto& svc = ChatModeService::instance();

    // Register when the tools changed or the server says the registration is gone.
    // Servers that report `registered` keep it alive through the polls themselves;
    // older ones need a renewal before their TTL drops it.
    const quint64 gen = mcp::McpProvider::instance().generation();
    const bool expiring = last_register_ok_ms_ == 0 ||
                          (!server_tracks_registration_ && now - last_register_ok_ms_ >= kReregisterIntervalMs) ||
                          (reregister_requested_ && now - last_register_ok_ms_ >= kLapseReregisterMinMs);
    // Not while the server's bridge is down: a poll saying `available: true` comes first.
    if ((gen != last_gen_ || expiring) && now >= register_retry_not_before_ms_ && !bridge_unavailable_)
        register_tools();

    // One poll at a time: the timer ticks every 3 s but a poll can take up to its 15 s
    // timeout, and overlapping polls returned the same pending call more than once.
    // After failures, wait out the backoff (see below). Otherwise poll when due: fast
    // while an agent run can call a tool, slow (registration keep-alive) when idle.
    if (poll_in_flight_ || now < poll_not_before_ms_)
        return;
    const qint64 interval = wants_fast_poll(now) ? kFastPollMs : kIdlePollMs;
    // 250 ms of slack: the tick timer and the interval are the same 3 s.
    if (last_poll_ms_ != 0 && now - last_poll_ms_ < interval - 250)
        return;
    poll_in_flight_ = true;
    last_poll_ms_ = now;

    QPointer<TerminalToolBridge> self = this;
    svc.poll_pending_calls([self](bool ok, PendingToolCalls pending, QString err, int retry_after_s) {
        if (!self)
            return;
        self->poll_in_flight_ = false;
        if (!self->active_)
            return;
        const qint64 done_ms = QDateTime::currentMSecsSinceEpoch();
        if (!ok) {
            // Back off exponentially (6 s, 12 s, … up to 5 min), and never sooner than the
            // server's Retry-After. A 401 here has already been reported to AuthManager by
            // ChatModeService; repeating it every 3 s only feeds the server's IP ban.
            // auth_state_changed / an unblocked IP resets the backoff (reset_backoff()).
            ++self->poll_failures_;
            const qint64 delay =
                std::max<qint64>(std::min<qint64>(kFastPollMs << std::min(self->poll_failures_, 7), kMaxPollBackoffMs),
                                 qint64(retry_after_s) * 1000);
            self->poll_not_before_ms_ = done_ms + delay;
            LOG_DEBUG("TerminalToolBridge", QString("Poll failed (%1 in a row, next in %2 s): %3")
                                                .arg(self->poll_failures_)
                                                .arg(delay / 1000)
                                                .arg(err));
            return;
        }
        self->poll_failures_ = 0;
        self->poll_not_before_ms_ = 0;

        if (pending.available_known) {
            // available: false = the server's bridge (its store) is down: `registered:
            // false` then says nothing about the registration — do not register, wait.
            if (pending.available == self->bridge_unavailable_)
                LOG_INFO("TerminalToolBridge", pending.available
                                                   ? QStringLiteral("The server's tool bridge is available again")
                                                   : QStringLiteral("The server's tool bridge is unavailable — waiting"));
            self->bridge_unavailable_ = !pending.available;
        }
        if (self->bridge_unavailable_) {
            self->reregister_requested_ = false;
        } else if (pending.registered_known) {
            self->server_tracks_registration_ = true;
            if (!pending.registered && !self->reregister_requested_ && self->last_register_ok_ms_ != 0)
                LOG_INFO("TerminalToolBridge", "The server no longer holds the tool registration — registering again");
            // Before our own registration has answered, "not registered" is expected
            // (a registration that never succeeded is retried by the tick anyway).
            self->reregister_requested_ = !pending.registered && self->last_register_ok_ms_ != 0;
        }
        if (pending.visibility_known) {
            self->visibility_known_ = true;
            self->agent_visible_ = pending.agent_visible;
        }
        if (!pending.calls.isEmpty()) {
            // A delivered call proves the agent sees the registration (older servers'
            // only proof) — and that a run is in progress: more calls may follow.
            if (!self->confirmed_)
                LOG_INFO("TerminalToolBridge", "The cloud agent called a terminal tool — registration confirmed live");
            self->confirmed_ = true;
            if (self->visibility_known_)
                self->agent_visible_ = true;
            self->fast_until_ms_ = std::max(self->fast_until_ms_, done_ms + kCallTailMs);
        }
        self->refresh_status();
        if (self->reregister_requested_)
            QTimer::singleShot(0, self, &TerminalToolBridge::on_poll_tick); // re-register without waiting a tick
        for (const auto& v : std::as_const(pending.calls)) {
            const QJsonObject call = v.toObject();
            self->execute_call(call["call_id"].toString(), call["tool_name"].toString(), call["arguments"].toObject());
        }
    });
}

// ── Execute a tool call locally ──────────────────────────────────────────────

void TerminalToolBridge::execute_call(const QString& call_id, const QString& tool_name, const QJsonObject& arguments) {
    // Already running (the previous poll handed it out and its result isn't posted yet).
    if (!call_id.isEmpty() && in_flight_calls_.contains(call_id)) {
        LOG_DEBUG("TerminalToolBridge", QString("Call %1 already executing — ignoring duplicate").arg(call_id));
        return;
    }
    LOG_INFO("TerminalToolBridge", QString("Cloud agent tool call tool=%1 call=%2").arg(tool_name, call_id));

    // Phase 5.10: centralised __ parsing. Was previously hardcoded as
    // mid(18) which silently breaks if the server-id prefix ever changes.
    auto [server_id, parsed_name] = mcp::McpProvider::parse_openai_function_name(tool_name);
    const QString local_name = parsed_name.isEmpty() ? tool_name : parsed_name;

    // Defence in depth: the backend can hand back any tool name — a stale
    // catalog from an earlier generation, or one the model invented. Re-check
    // the deny list at execution time so a blocked tool can never run even if
    // it was somehow registered.
    // The server does not say which run a call belongs to: attribute it to the
    // cloud runs that are going now (usually exactly one).
    auto& runs = agent_tasks::TaskRunService::instance();
    const QStringList run_ids = calling_runs();
    bool paper = false;
    QString origin = QStringLiteral("Fincept agent (cloud)");
    for (const QString& id : run_ids) {
        if (const agent_tasks::TaskRun* r = runs.find_run(id)) {
            paper = paper || r->is_unattended();
            if (run_ids.size() == 1)
                origin = QStringLiteral("Fincept agent: %1").arg(r->title.left(60));
        }
    }
    if (run_ids.size() > 1)
        origin = QStringLiteral("Fincept agent (%1 runs)").arg(run_ids.size());

    const auto tool_info = mcp::McpProvider::instance().find_tool(local_name);
    const QString tool_category = tool_info ? tool_info->category : QString();
    if (is_remote_blocked(local_name, tool_category)) {
        LOG_WARN("TerminalToolBridge",
                 QString("Refused remote tool '%1' (category '%2') — real-money / arbitrary-code / credential / "
                         "UI-only tools are not available to the cloud agent")
                     .arg(local_name, tool_category));
        const auto refusal = mcp::ToolResult::fail(
            QString("Tool '%1' is not available to the remote agent. It moves real funds, executes local code, "
                    "reads stored credentials, or drives the local UI, and must be run by the user from the "
                    "terminal itself.")
                .arg(local_name));
        ChatModeService::instance().submit_tool_result(call_id, refusal.to_json(), [](ToolResultDelivery, QString) {});
        QJsonObject d;
        d["refused"] = true;
        d["category"] = tool_category;
        record_call_event(run_ids, true, call_id, local_name,
                          tr("Refused: the cloud agent may not run this tool (real funds, local code, stored "
                             "credentials or driving the screen)."),
                          false, d);
        emit bridge_error(tr("Blocked unsafe remote tool call: %1").arg(local_name));
        return;
    }
    record_call_event(run_ids, false, call_id, local_name, tool_bridge_args_preview(arguments), true, {});
    // Audit trail for every mutating tool the remote agent runs. Not a gate —
    // just a record, so a surprising state change can be traced back.
    if (tool_info && tool_info->is_destructive)
        LOG_WARN("TerminalToolBridge",
                 QString("Remote agent executing destructive tool '%1' (call %2)").arg(local_name, call_id));

    // Phase 4: dispatch via call_tool_async so sync handlers don't block
    // a worker thread for the duration of long-running tools, and so
    // multiple in-flight tool calls fan out concurrently. A QFutureWatcher
    // delivers the result back to this bridge's thread when complete.
    QPointer<TerminalToolBridge> self = this;

    // Mark this as an AGENT-originated call so the process-wide AuthChecker
    // installed by AgentService actually applies. That checker denies
    // `is_destructive` tools only when TerminalMcpBridge::is_call_in_progress()
    // is true — a thread_local flag previously set ONLY by TerminalMcpBridge.
    // This bridge (the cloud-agent poll path) never set it, so every tool it
    // dispatched was classified as a user-driven chat call and skipped the
    // destructive gate entirely. The deny list above is defence in depth; this
    // is the actual gate. `destructive_allowed=false` because a poll-driven
    // remote agent can never show a confirmation modal.
    //
    // Scoped around call_tool_async specifically: McpProvider runs
    // check_authorization() synchronously in that function's prologue
    // (McpProvider.cpp) before dispatching to a pool thread, so the flag is
    // read on THIS thread while the guard is alive.
    if (!call_id.isEmpty())
        in_flight_calls_.insert(call_id);

    QFuture<mcp::ToolResult> future;
    {
        // Who is calling (approval prompts, activity logs) and — while a scheduled
        // run is going — paper only: money actions are refused, not prompted.
        mcp::ScopedCallOrigin origin_scope(origin);
        // Stable ids for agent attribution: the run only when exactly one is going.
        mcp::ScopedCallClient client_scope(QString::fromLatin1(mcp::ScopedCallClient::kFinceptAgent),
                                           run_ids.size() == 1 ? run_ids.first() : QString());
        std::optional<mcp::ScopedPaperRun> paper_scope;
        if (paper)
            paper_scope.emplace();
        mcp::TerminalMcpBridge::ScopedCallFlags gate(/*call_in_progress=*/true, /*destructive_allowed=*/false);
        future = mcp::McpProvider::instance().call_tool_async(local_name, arguments);
    }

    auto* watcher = new QFutureWatcher<mcp::ToolResult>(this);
    QObject::connect(watcher, &QFutureWatcher<mcp::ToolResult>::finished, this,
                     [self, call_id, local_name, watcher, run_ids]() {
        // resultCount() lives on QFuture, not QFutureWatcher —
        // use future() to reach it.
        const auto fut = watcher->future();
        mcp::ToolResult result =
            (fut.resultCount() > 0) ? fut.result() : mcp::ToolResult::fail("Tool produced no result");
        watcher->deleteLater();

        if (!self)
            return;
        self->in_flight_calls_.remove(call_id);

        // The run(s) see the call's outcome (the full result when it is small enough
        // for a result card, a preview otherwise).
        const QJsonObject result_json = result.to_json();
        {
            QJsonObject d;
            const QByteArray compact = QJsonDocument(result_json).toJson(QJsonDocument::Compact);
            if (compact.size() <= kMaxResultBytesInEvent)
                d["result"] = result_json;
            else
                d["result_bytes"] = static_cast<double>(compact.size());
            self->record_call_event(run_ids, true, call_id, local_name, tool_bridge_preview(result_json),
                                    result.success, d);
        }

        // The result is posted even when the bridge stopped meanwhile: the tool has
        // already RUN, and dropping its result left the remote agent waiting on a call
        // it can no longer get an answer for.
        //
        // Capture result by value into the submit callback —
        // avoids the init-capture pattern (which Clang on
        // Windows was rejecting for nested-lambda scoping).
        ChatModeService::instance().submit_tool_result(
            call_id, result_json, [self, local_name, call_id, result, result_json](ToolResultDelivery delivery, QString err) {
                switch (delivery) {
                    case ToolResultDelivery::Delivered:
                        break;
                    case ToolResultDelivery::AlreadyDelivered: // 409: the agent has its answer
                        LOG_DEBUG("TerminalToolBridge",
                                  QString("Result for %1 (call %2) was already delivered").arg(local_name, call_id));
                        break;
                    case ToolResultDelivery::Expired: // 404: the agent stopped waiting — nothing to do
                        LOG_INFO("TerminalToolBridge",
                                 QString("Call %1 (%2) expired before its result arrived; result dropped")
                                     .arg(call_id, local_name));
                        break;
                    case ToolResultDelivery::Failed:
                        LOG_WARN("TerminalToolBridge",
                                 QString("Failed to submit result for %1: %2").arg(local_name, err));
                        break;
                }
                if (self)
                    emit self->tool_executed(local_name, result.success, result_json);
            });
    });
    watcher->setFuture(future);
}

void TerminalToolBridge::record_call_event(const QStringList& run_ids, bool result, const QString& call_id,
                                           const QString& tool, const QString& body, bool success,
                                           const QJsonObject& data) {
    if (run_ids.isEmpty())
        return;
    agent_tasks::RunEvent ev;
    ev.kind = result ? agent_tasks::RunEventKind::ToolResult : agent_tasks::RunEventKind::ToolCall;
    ev.title = tr("Terminal tool: %1").arg(tool);
    ev.body = body;
    ev.is_error = result && !success;
    ev.ref_id = QStringLiteral("terminal-%1").arg(call_id);
    ev.data = data;
    ev.data["source"] = QStringLiteral("terminal");
    ev.data["tool"] = tool;
    if (result)
        ev.data["success"] = success;
    if (run_ids.size() > 1)
        ev.data["ambiguous"] = true; // the server does not say which run asked
    auto& svc = agent_tasks::TaskRunService::instance();
    for (const QString& id : run_ids)
        svc.record_event(id, ev);
}

} // namespace fincept::chat_mode
