// TerminalMcpBridge.cpp — Local HTTP/1.1 bridge for the Python finagent toolkit.
//
// Why hand-rolled HTTP framing: Qt 6.8.3 in this build does not include the
// QHttpServer module (only Qt6::Network and Qt6::WebSockets are present).
// The contract is small and we control both ends, so a tiny parser is
// cheaper than adding a Qt module dependency.

#include "mcp/TerminalMcpBridge.h"

#include "auth/ConstantTime.h"
#include "auth/LoopbackGuard.h"
#include "core/logging/Logger.h"
#include "mcp/McpManager.h"
#include "mcp/McpProvider.h"
#include "mcp/McpService.h"
#include "mcp/ResultStore.h"
#include "mcp/server/EssentialTools.h"

#include <QDateTime>
#include <QFutureWatcher>
#include <QHostAddress>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPointer>
#include <QPromise>
#include <QRegularExpression>
#include <QSet>
#include <QTcpServer>
#include <QTcpSocket>
#include <QThreadPool>
#include <QUuid>
#include <QtConcurrent/QtConcurrent>

#include <algorithm>
#include <memory>
#include <optional>

namespace fincept::mcp {

static constexpr const char* kBridgeTag = "TerminalMcpBridge";

// HTTP/1.1 framing limits. Localhost-only with a known peer (the agent
// subprocess), so generous but bounded — anything past these is a bug or an
// unwelcome process probing the port.
static constexpr int kBridgeMaxHeaderBytes = 16 * 1024;
static constexpr int kBridgeMaxBodyBytes = 4 * 1024 * 1024; // 4 MB

// Run-scope bounds. Every runner retires its scope when the agent process
// exits; these only catch a scope that was leaked. A leaked scope is fail-safe
// (the run's policy simply stays enforced for a token nobody holds any more)
// but must not accumulate for the session. Agentic tasks can legitimately run
// for hours, so the age bound is generous.
static constexpr qint64 kRunScopeMaxLifetimeMs = 24LL * 60 * 60 * 1000; // 24 h
static constexpr int kMaxLiveRunScopes = 256;

// Dispatch pool width. Each slot runs only the synchronous part of a call
// (auth, validation, sync handlers, async kick-off) — async tools resolve via a
// continuation and never park a thread — so four is ample for a handful of
// concurrent agent runs.
static constexpr int kBridgeDispatchThreads = 4;

// Response budget. Agent tool results are spliced into the agent's own LLM
// transcript, which re-sends them every round; a multi-MB filing or price
// history drowns the context. Over the budget, the payload is shaped
// structurally (valid JSON, marked in place) and the full copy parked in the
// ResultStore for result_fetch.
static constexpr int kBridgeResultBudgetBytes = 24 * 1024;

// HTTP timeout hint for external tools (the MCP wire carries no budget).
static constexpr int kBridgeExternalToolTimeoutMs = 120000;

// Per-thread flags: set by the bridge across the synchronous portion of
// McpProvider::call_tool_async (where auth runs). The McpProvider auth
// checker reads these to distinguish agent-originated calls from chat-path
// calls without changing the AuthChecker signature.
static thread_local bool tls_call_in_progress = false;
static thread_local bool tls_destructive_allowed = false;

bool TerminalMcpBridge::is_call_in_progress() {
    return tls_call_in_progress;
}

bool TerminalMcpBridge::is_destructive_allowed() {
    return tls_destructive_allowed;
}

namespace {

// Categories an agent may never dispatch, regardless of what its catalog says.
// Mirrors the default `exclude_categories` AgentService applies to every agent
// filter — UI-driving tools ("navigation"/"system"/"settings"), recursive chat
// tools ("ai-chat"), and the meta tools. Excludes a specific agent adds on top
// of these are enforced per run, via the run scope opened by begin_run().
const QStringList& never_dispatchable_categories() {
    static const QStringList kCats = {QStringLiteral("navigation"), QStringLiteral("system"),
                                      QStringLiteral("settings"), QStringLiteral("ai-chat"), QStringLiteral("meta")};
    return kCats;
}

// The meta tools an Essentials run is built around: discovery plus the generic
// dispatcher. Exempt from the "meta" category exclusion for those runs only —
// without them an essentials agent could not reach anything outside its small
// catalogue. job_* stay excluded: the bridge never defers (no job protocol on
// the Python side), so there is never a job for an agent to poll.
const QSet<QString>& essentials_meta_tools() {
    static const QSet<QString> kTools = {QStringLiteral("tool_list"), QStringLiteral("tool_describe"),
                                         QStringLiteral("tool_invoke")};
    return kTools;
}

// result_fetch pages a payload this bridge parked when it shaped an oversized
// response, so every run must be able to reach it, whatever its filter says.
bool bridge_always_dispatchable(const QString& server_id, const QString& tool_name) {
    return server_id == QLatin1String(INTERNAL_SERVER_ID) && tool_name == QLatin1String("result_fetch");
}

// True when `filter` would have kept this tool OUT of the catalog, in which
// case dispatching it must be refused too. Predicates mirror McpService's
// apply_tool_filter exactly (category include/exclude, then name regex
// include/exclude) so the catalog an agent sees and the calls the bridge
// honours cannot disagree.
//
// `filter.max_tools` is deliberately NOT applied: it caps how large a catalog
// the agent is shown, not which tools are permitted. Enforcing a count here
// would make a call's fate depend on how many other tools happened to sort
// ahead of it — a different rule wearing the same name.
//
// `category` is empty for tools on external MCP servers; that is the same value
// apply_tool_filter sees for them, so a whitelist filter excludes them there and
// here alike. `skip_category_excludes` lets the essentials meta tools through
// the "meta" exclusion while still honouring the agent's name patterns.
bool filter_excludes(const ToolFilter& filter, const QString& tool_name, const QString& category,
                     bool skip_category_excludes, QString* reason) {
    auto deny = [reason](const QString& why) {
        if (reason)
            *reason = why;
        return true;
    };

    if (!skip_category_excludes) {
        if (!filter.categories.isEmpty() && (category.isEmpty() || !filter.categories.contains(category)))
            return deny(QStringLiteral("category '%1' is not in this agent's allowed categories").arg(category));

        if (!filter.exclude_categories.isEmpty() && filter.exclude_categories.contains(category))
            return deny(QStringLiteral("category '%1' is excluded by this agent's configuration").arg(category));
    }

    if (!filter.name_patterns.isEmpty()) {
        bool any_match = false;
        for (const auto& p : filter.name_patterns) {
            if (QRegularExpression(p).match(tool_name).hasMatch()) {
                any_match = true;
                break;
            }
        }
        if (!any_match)
            return deny(QStringLiteral("tool name does not match this agent's allowed name patterns"));
    }

    for (const auto& p : filter.exclude_name_patterns) {
        if (QRegularExpression(p).match(tool_name).hasMatch())
            return deny(QStringLiteral("tool name is excluded by this agent's configuration"));
    }

    return false;
}

// The single dispatch policy. Used for every POST /tool, for the tool a
// tool_invoke call is about to reach, and to vet every catalog entry — so the
// catalog, direct calls and indirect calls can never disagree.
// Returns the refusal reason, or nullopt when the call may proceed.
std::optional<QString> bridge_dispatch_refusal(const ToolFilter& filter, bool include_external,
                                               AgentToolCatalog catalog, const QString& server_id,
                                               const QString& tool_name, const QString& category) {
    if (bridge_always_dispatchable(server_id, tool_name))
        return std::nullopt;

    const bool internal = server_id == QLatin1String(INTERNAL_SERVER_ID);
    if (!internal && !include_external)
        return QStringLiteral("external MCP tools are disabled for this agent");

    const bool essentials_meta =
        internal && catalog == AgentToolCatalog::Essentials && essentials_meta_tools().contains(tool_name);

    // 1. Process-wide, non-negotiable.
    if (internal && !essentials_meta && never_dispatchable_categories().contains(category))
        return QStringLiteral("category '%1' is not dispatchable over the agent bridge").arg(category);

    // 2. The run's own filter (the agent author's includes/excludes).
    QString why;
    if (filter_excludes(filter, tool_name, internal ? category : QString(), essentials_meta, &why))
        return why;
    return std::nullopt;
}

struct CallFlagGuard {
    explicit CallFlagGuard(bool destructive_ok) {
        tls_call_in_progress = true;
        tls_destructive_allowed = destructive_ok;
    }
    ~CallFlagGuard() {
        tls_call_in_progress = false;
        tls_destructive_allowed = false;
    }
    CallFlagGuard(const CallFlagGuard&) = delete;
    CallFlagGuard& operator=(const CallFlagGuard&) = delete;
};

// Shape an oversized response to the bridge budget. Mirrors the chat loop's
// fit_llm_payload (services/llm/LlmRequestPolicy.h) with a larger budget: the
// full envelope is parked in the ResultStore, the response keeps a
// structurally-valid preview, and the trim is announced in place — an agent
// that read a shortened array as complete would reason off a truncated dataset.
void bridge_shape_payload(const QString& tool_name, QJsonObject& payload) {
    const QByteArray compact = QJsonDocument(payload).toJson(QJsonDocument::Compact);
    if (compact.size() <= kBridgeResultBudgetBytes)
        return;

    const QString result_id = ResultStore::instance().put(tool_name, payload);
    QJsonValue shaped = QJsonValue(payload);
    const ShrinkStats st = shrink_json(shaped, std::max(1024, kBridgeResultBudgetBytes - 800));

    payload = shaped.isObject() ? shaped.toObject() : QJsonObject{{"data", shaped}};
    payload["truncated"] = true;
    payload["result_id"] = result_id;
    payload["full_bytes"] = st.original_bytes;
    payload["note"] = QStringLiteral("Result truncated to fit the agent context (%1 -> %2 bytes); arrays and long "
                                     "strings are marked where they were cut. Page the full payload with "
                                     "result_fetch(result_id='%3', offset=0, limit=6000).")
                          .arg(st.original_bytes)
                          .arg(st.final_bytes)
                          .arg(result_id);

    LOG_INFO(kBridgeTag, QString("Tool '%1' returned %2 KB to an agent — shaped to %3 B, full payload parked as %4")
                             .arg(tool_name)
                             .arg(st.original_bytes / 1024)
                             .arg(st.final_bytes)
                             .arg(result_id));
}

} // namespace

// Public RAII helper — re-establishes the (thread_local) gating flags on the
// pool thread that runs an external tool, then restores the prior values.
TerminalMcpBridge::ScopedCallFlags::ScopedCallFlags(bool call_in_progress, bool destructive_allowed)
    : prev_in_progress_(tls_call_in_progress), prev_destructive_(tls_destructive_allowed) {
    tls_call_in_progress = call_in_progress;
    tls_destructive_allowed = destructive_allowed;
}

TerminalMcpBridge::ScopedCallFlags::~ScopedCallFlags() {
    tls_call_in_progress = prev_in_progress_;
    tls_destructive_allowed = prev_destructive_;
}

TerminalMcpBridge& TerminalMcpBridge::instance() {
    static TerminalMcpBridge s;
    return s;
}

TerminalMcpBridge::TerminalMcpBridge(QObject* parent) : QObject(parent) {}

// ── Lifecycle ────────────────────────────────────────────────────────────────

bool TerminalMcpBridge::start() {
    if (active_)
        return true;

    server_ = new QTcpServer(this);
    connect(server_, &QTcpServer::newConnection, this, &TerminalMcpBridge::on_new_connection);

    if (!server_->listen(QHostAddress::LocalHost, 0)) {
        LOG_ERROR(kBridgeTag, "Failed to bind 127.0.0.1: " + server_->errorString());
        emit bridge_error(QStringLiteral("Failed to bind terminal MCP bridge: ") + server_->errorString());
        server_->deleteLater();
        server_ = nullptr;
        return false;
    }

    destructive_token_ = QUuid::createUuid().toString(QUuid::WithoutBraces);
    active_ = true;
    LOG_INFO(kBridgeTag, QString("Listening on %1").arg(endpoint()));
    return true;
}

void TerminalMcpBridge::stop() {
    if (!active_)
        return;
    active_ = false;

    for (auto it = states_.constBegin(); it != states_.constEnd(); ++it) {
        if (it.key()) {
            it.key()->disconnectFromHost();
            it.key()->deleteLater();
        }
    }
    states_.clear();

    {
        // Run scopes die with the listener — a restart mints fresh tokens.
        QMutexLocker lock(&runs_mutex_);
        runs_.clear();
    }

    // Drop calls still queued for the pool; ones already running finish on
    // their own and find the socket gone.
    if (dispatch_pool_)
        dispatch_pool_->clear();

    if (server_) {
        server_->close();
        server_->deleteLater();
        server_ = nullptr;
    }
    LOG_INFO(kBridgeTag, "Stopped");
}

QString TerminalMcpBridge::endpoint() const {
    if (!active_ || !server_)
        return {};
    return QString("http://127.0.0.1:%1").arg(server_->serverPort());
}

// ── Run scopes ───────────────────────────────────────────────────────────────

QString TerminalMcpBridge::begin_run(const ToolFilter& filter, bool include_external, AgentToolCatalog catalog) {
    if (!active_)
        return {}; // nothing listening — nothing to scope

    RunScope scope;
    scope.filter = filter;
    scope.include_external = include_external;
    scope.catalog = catalog;
    scope.started_ms = QDateTime::currentMSecsSinceEpoch();

    const QString run_token = QUuid::createUuid().toString(QUuid::WithoutBraces);
    {
        QMutexLocker lock(&runs_mutex_);
        sweep_runs_locked();
        runs_.insert(run_token, scope);
    }
    return run_token;
}

void TerminalMcpBridge::end_run(const QString& run_token) {
    if (run_token.isEmpty())
        return;
    QMutexLocker lock(&runs_mutex_);
    runs_.remove(run_token);
}

void TerminalMcpBridge::sweep_runs_locked() {
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    int aged_out = 0;
    for (auto it = runs_.begin(); it != runs_.end();) {
        if (now - it.value().started_ms > kRunScopeMaxLifetimeMs) {
            it = runs_.erase(it);
            ++aged_out;
        } else {
            ++it;
        }
    }

    // Hard ceiling as a second backstop, oldest first.
    while (runs_.size() >= kMaxLiveRunScopes) {
        auto oldest = runs_.begin();
        for (auto it = runs_.begin(); it != runs_.end(); ++it) {
            if (it.value().started_ms < oldest.value().started_ms)
                oldest = it;
        }
        runs_.erase(oldest);
        ++aged_out;
    }

    if (aged_out > 0)
        LOG_DEBUG(kBridgeTag, QString("Swept %1 stale agent run scope(s); %2 live").arg(aged_out).arg(runs_.size()));
}

std::optional<TerminalMcpBridge::RunScope> TerminalMcpBridge::run_scope(const QString& run_token) const {
    if (run_token.isEmpty())
        return std::nullopt;
    QMutexLocker lock(&runs_mutex_);
    const auto it = runs_.constFind(run_token);
    if (it == runs_.constEnd())
        return std::nullopt;
    return it.value();
}

bool TerminalMcpBridge::authenticate(const QString& supplied, QString* run_token_out) const {
    if (run_token_out)
        run_token_out->clear();
    if (supplied.isEmpty())
        return false;

    // Constant-time throughout: `supplied` is attacker-controlled, and a
    // short-circuiting compare leaks how many leading bytes were right, which
    // turns a 128-bit guess into a byte-at-a-time search.
    QMutexLocker lock(&runs_mutex_);
    for (auto it = runs_.constBegin(); it != runs_.constEnd(); ++it) {
        if (auth::constant_time_equals(supplied, it.key())) {
            if (run_token_out)
                *run_token_out = it.key();
            return true;
        }
    }
    return false;
}

// ── Tool catalog serialisation ───────────────────────────────────────────────

QJsonArray TerminalMcpBridge::tool_definitions(const ToolFilter& filter, bool include_external,
                                               AgentToolCatalog catalog) const {
    auto& svc = McpService::instance();
    auto& provider = McpProvider::instance();

    std::vector<UnifiedTool> tools;
    if (catalog == AgentToolCatalog::Essentials) {
        // Curated set (meta tools first, then research tools by priority).
        // Names that are not registered in this build are skipped — the list
        // is hand-maintained and a rename must never break an agent.
        for (const QString& name : essential_tool_names()) {
            if (auto t = provider.find_tool(name))
                tools.push_back(*t);
        }
    } else {
        // Goes through McpService so external MCP servers (configured via the
        // MCP Servers tab) are visible to agents too. The user's max_tools is
        // the only cap (0 = every matching tool), read-only tools first.
        tools = svc.get_agent_tools(filter);
    }

    QJsonArray out;
    for (const auto& t : tools) {
        // Honour the per-agent opt-out for externals — UnifiedTool::is_internal
        // is set true for the Fincept catalog and false for everything else.
        if (!include_external && !t.is_internal)
            continue;
        // Never advertise what dispatch would refuse.
        if (bridge_dispatch_refusal(filter, include_external, catalog, t.server_id, t.name, t.category))
            continue;
        QJsonObject schema = t.input_schema;
        if (schema.isEmpty()) {
            schema["type"] = "object";
            schema["properties"] = QJsonObject();
        }
        QJsonObject entry;
        entry["name"] = t.name;
        entry["description"] = t.description;
        // camelCase per MCP wire spec — what TerminalToolkit reads.
        entry["inputSchema"] = schema;
        // The toolkit echoes serverId back on POST /tool; without it an external
        // tool would be routed to the internal registry and fail as unknown.
        entry["serverId"] = t.server_id;
        // The tool's own budget — the toolkit sizes its HTTP timeout from it.
        entry["timeoutMs"] = t.is_internal ? provider.effective_timeout_ms(t.name) : kBridgeExternalToolTimeoutMs;
        out.append(entry);
    }
    return out;
}

// ── Connection handling ─────────────────────────────────────────────────────

void TerminalMcpBridge::on_new_connection() {
    while (server_ && server_->hasPendingConnections()) {
        QTcpSocket* sock = server_->nextPendingConnection();
        states_.insert(sock, RequestState{});
        connect(sock, &QTcpSocket::readyRead, this, &TerminalMcpBridge::on_ready_read);
        connect(sock, &QTcpSocket::disconnected, this, &TerminalMcpBridge::on_disconnected);
    }
}

void TerminalMcpBridge::on_disconnected() {
    auto* sock = qobject_cast<QTcpSocket*>(sender());
    if (!sock)
        return;
    states_.remove(sock);
    sock->deleteLater();
}

void TerminalMcpBridge::on_ready_read() {
    auto* sock = qobject_cast<QTcpSocket*>(sender());
    if (!sock)
        return;
    auto it = states_.find(sock);
    if (it == states_.end())
        return;

    RequestState& st = it.value();
    st.buffer.append(sock->readAll());

    if (!st.headers_parsed) {
        const int header_end = st.buffer.indexOf("\r\n\r\n");
        if (header_end < 0) {
            if (st.buffer.size() > kBridgeMaxHeaderBytes) {
                write_error(sock, 431, "Request header too large");
            }
            return; // wait for more
        }
        if (!parse_headers(st)) {
            write_error(sock, 400, "Malformed request");
            return;
        }
        if (st.content_length > kBridgeMaxBodyBytes) {
            write_error(sock, 413, "Payload too large");
            return;
        }
    }

    // Body fully arrived?
    const int header_end = st.buffer.indexOf("\r\n\r\n");
    const int body_start = header_end + 4;
    const int body_have = st.buffer.size() - body_start;
    if (body_have < st.content_length)
        return; // wait for more bytes

    try_dispatch(sock);
}

bool TerminalMcpBridge::parse_headers(RequestState& st) {
    const int header_end = st.buffer.indexOf("\r\n\r\n");
    if (header_end < 0)
        return false;
    const QByteArray head = st.buffer.left(header_end);
    const QList<QByteArray> lines = head.split('\n');
    if (lines.isEmpty())
        return false;

    // Request line: METHOD SP PATH SP HTTP/1.1
    const QByteArray req_line = lines.front().trimmed();
    const QList<QByteArray> parts = req_line.split(' ');
    if (parts.size() < 2)
        return false;
    st.method = QString::fromLatin1(parts[0]);
    st.path = QString::fromLatin1(parts[1]);

    for (int i = 1; i < lines.size(); ++i) {
        const QByteArray line = lines[i].trimmed();
        if (line.isEmpty())
            continue;
        const int colon = line.indexOf(':');
        if (colon <= 0)
            continue;
        const QString key = QString::fromLatin1(line.left(colon).trimmed()).toLower();
        const QString val = QString::fromLatin1(line.mid(colon + 1).trimmed());
        st.headers.insert(key, val);
    }
    st.content_length = st.headers.value("content-length", "0").toInt();
    st.headers_parsed = true;
    return true;
}

// ── Request routing ─────────────────────────────────────────────────────────

void TerminalMcpBridge::try_dispatch(QTcpSocket* sock) {
    auto it = states_.find(sock);
    if (it == states_.end())
        return;
    RequestState& st = it.value();

    // ── Origin validation — must come before authn, and before any work ──────
    //
    // Binding 127.0.0.1 keeps other hosts out; it does not keep out the user's
    // own browser. Any page they have open can POST at this port, and a page
    // that resolves a hostname it controls to 127.0.0.1 (DNS rebinding) can talk
    // to us as same-origin. The custom X-MCP-Token header forces a CORS
    // preflight that this server never answers, which happens to stop a browser
    // today — but that is a side effect of the header, not a decision, and it
    // stops nothing that isn't a browser. Check Host and the fetch metadata
    // explicitly, like the other loopback listeners do.
    //
    // allow_cross_site_navigation=false: unlike the OAuth / wallet callbacks
    // this endpoint is a machine-to-machine JSON API. It has no legitimate
    // top-level navigation, so a navigate-mode request is never ours. The
    // Python agent uses urllib and sends no Sec-Fetch-* at all, which the guard
    // passes through to the token check below.
    const quint16 bound_port = server_ ? static_cast<quint16>(server_->serverPort()) : 0;
    const auto origin_check = auth::check_loopback_request(st.buffer, bound_port,
                                                           /*allow_cross_site_navigation=*/false);
    if (!origin_check.allowed) {
        LOG_WARN(kBridgeTag, QString("Rejecting request — %1 (path=%2)").arg(origin_check.reason, st.path));
        write_error(sock, 403, "Request rejected by origin policy");
        return;
    }

    // Authn — constant-time against every live run token.
    const QString supplied = st.headers.value("x-mcp-token");
    if (!authenticate(supplied, &st.run_token)) {
        LOG_WARN(kBridgeTag, QString("Rejecting request — token mismatch (path=%1)").arg(st.path));
        write_error(sock, 401, "Invalid or missing X-MCP-Token");
        return;
    }

    // Strip query string for path matching.
    const QString path_only = st.path.section('?', 0, 0);

    if (st.method == "POST" && path_only == "/tool") {
        const int header_end = st.buffer.indexOf("\r\n\r\n");
        const QByteArray body = st.buffer.mid(header_end + 4, st.content_length);
        const QJsonDocument doc = QJsonDocument::fromJson(body);
        if (!doc.isObject()) {
            write_error(sock, 400, "Invalid JSON body");
            return;
        }
        handle_post_tool(sock, doc.object());
        return;
    }

    if (st.method == "GET" && path_only == "/tools") {
        handle_get_tools(sock);
        return;
    }

    write_error(sock, 404, QString("Unknown route: %1 %2").arg(st.method, path_only));
}

// ── POST /tool ──────────────────────────────────────────────────────────────

void TerminalMcpBridge::handle_post_tool(QTcpSocket* sock, const QJsonObject& body) {
    const QString call_id = body.value("id").toString();
    const QString tool_name = body.value("tool").toString();
    const QJsonObject args = body.value("args").toObject();
    // Optional — the toolkit sets serverId (from the catalog entry) for tools on
    // external MCP servers. Absent means the internal Fincept catalog.
    QString server_id = body.value("serverId").toString().trimmed();
    if (server_id.isEmpty())
        server_id = QString::fromLatin1(INTERNAL_SERVER_ID);

    if (tool_name.isEmpty()) {
        write_error(sock, 400, "Missing 'tool' field");
        return;
    }

    LOG_INFO(kBridgeTag, QString("Tool call %1: %2 on %3").arg(call_id, tool_name, server_id));

    auto it_state = states_.find(sock);
    const RequestState* req = (it_state != states_.end()) ? &it_state.value() : nullptr;

    auto refuse = [this, sock, call_id, &tool_name](const QString& why) {
        LOG_WARN(kBridgeTag, QString("Refusing tool '%1' — %2").arg(tool_name, why));
        QJsonObject refusal =
            ToolResult::fail(QString("Tool '%1' is not available to this agent (%2).").arg(tool_name, why)).to_json();
        if (!call_id.isEmpty())
            refusal["id"] = call_id;
        write_json_response(sock, 200, refusal);
    };

    // ── Tool boundary, enforced at DISPATCH ──────────────────────────────────
    //
    // The catalog an agent was given is a hint; an agent that knows a tool's
    // name (stale catalog, guess, injected instruction) can POST it anyway. So
    // every call is held to the run's policy (bridge_dispatch_refusal): the
    // process-wide never-dispatchable categories, then the run's own filter.
    // Every authenticated request carries a run scope; one that has vanished
    // means the agent run already ended, so the call is refused rather than
    // run without a policy.
    const auto scope = run_scope(req ? req->run_token : QString());
    if (!scope) {
        refuse(QStringLiteral("the agent run this token belonged to has ended"));
        return;
    }

    QString category; // empty for tools on external MCP servers
    if (server_id == QLatin1String(INTERNAL_SERVER_ID)) {
        if (const auto info = McpProvider::instance().find_tool(tool_name))
            category = info->category;
    }
    if (const auto why = bridge_dispatch_refusal(scope->filter, scope->include_external, scope->catalog, server_id,
                                                 tool_name, category)) {
        refuse(*why);
        return;
    }

    // tool_invoke reaches another tool. The INNER tool must satisfy the same
    // policy as a direct call, or tool_invoke would be a way around the run's
    // filter and the never-dispatchable categories. (Unresolvable targets are
    // left to tool_invoke itself, which reports them to the agent.)
    if (server_id == QLatin1String(INTERNAL_SERVER_ID) && tool_name == QLatin1String("tool_invoke")) {
        const QString inner = args.value("tool").toString().trimmed();
        const auto target = McpService::instance().resolve_tool_target(inner, args.value("server_id").toString());
        if (target.error.isEmpty()) {
            if (const auto why = bridge_dispatch_refusal(scope->filter, scope->include_external, scope->catalog,
                                                         target.server_id, inner, target.category)) {
                refuse(QStringLiteral("tool_invoke target '%1': %2").arg(inner, *why));
                return;
            }
        }
    }

    QPointer<TerminalMcpBridge> self = this;
    QPointer<QTcpSocket> sock_guard = sock;

    // The auth checker (installed by AgentService) inspects
    // is_call_in_progress() and is_destructive_allowed() to apply
    // agent-specific gating. Constant-time compare — the header is
    // attacker-controlled and the token is a capability.
    const QString destructive_hdr = req ? req->headers.value("x-mcp-allow-destructive") : QString();
    const bool destructive_ok =
        !destructive_token_.isEmpty() && auth::constant_time_equals(destructive_hdr, destructive_token_);

    // Dispatch OFF the GUI thread, on the bridge's own pool.
    //
    // AgentService is constructed on the GUI thread, so this QTcpServer and
    // every accepted socket are GUI-thread objects: readyRead → on_ready_read →
    // try_dispatch → here, with no thread hop. Sync-shape handlers that ran
    // inline here used to take tools/ThreadHelper.h's run_async_wait branch
    // that ends in a nested event loop, re-entering handle_post_tool for a
    // second concurrent call — unbounded and holding the UI hostage.
    //
    // Only the synchronous part runs on the pool thread (auth, validation, sync
    // handlers, async kick-off). An async tool resolves later through a
    // continuation, so a 5-minute analytics call no longer parks a pool thread
    // in waitForFinished() for five minutes.
    //
    // CallFlagGuard is thread_local, so it MUST be established inside the
    // worker: without it the auth checker sees a chat-path call and the
    // destructive gate silently opens.
    if (!dispatch_pool_) {
        // Intentionally never deleted: QThreadPool's destructor waits for running
        // tasks, and a tool still blocked at shutdown (its watchdog lives on the
        // GUI event loop, which is gone by then) would stall process exit.
        dispatch_pool_ = new QThreadPool;
        dispatch_pool_->setObjectName(QStringLiteral("mcp-bridge-dispatch"));
        dispatch_pool_->setMaxThreadCount(kBridgeDispatchThreads);
    }

    auto promise = std::make_shared<QPromise<ToolResult>>();
    promise->start();
    QFuture<ToolResult> future = promise->future();
    auto settled = std::make_shared<std::atomic<bool>>(false);
    auto settle = [promise, settled](ToolResult r) {
        bool expected = false;
        if (!settled->compare_exchange_strong(expected, true))
            return;
        promise->addResult(std::move(r));
        promise->finish();
    };
    (void)QtConcurrent::run(dispatch_pool_, [settle, tool_name, args, server_id, destructive_ok]() {
        QFuture<ToolResult> f;
        {
            CallFlagGuard guard(destructive_ok);
            if (server_id == QLatin1String(INTERNAL_SERVER_ID)) {
                f = McpProvider::instance().call_tool_async(tool_name, args);
            } else {
                // External — McpService::execute_openai_function_async handles the
                // QtConcurrent::run wrapping for blocking JSON-RPC clients (and
                // carries these flags across its own hop). Build the wire-form
                // name it expects.
                const QString fn = server_id + "__" + McpProvider::encode_tool_name_for_wire(tool_name);
                f = McpService::instance().execute_openai_function_async(fn, args);
            }
        }
        f.then([settle](QFuture<ToolResult> done) {
            settle(done.resultCount() > 0 ? done.result() : ToolResult::fail("Tool produced no result"));
        });
    });

    // Resolve via QFutureWatcher so the socket write lands back on this
    // (GUI) thread when the tool completes.
    auto* watcher = new QFutureWatcher<ToolResult>(this);
    connect(watcher, &QFutureWatcher<ToolResult>::finished, this, [self, sock_guard, tool_name, call_id, watcher]() {
        const auto fut = watcher->future();
        ToolResult result = (fut.resultCount() > 0) ? fut.result() : ToolResult::fail("Tool produced no result");
        watcher->deleteLater();

        if (!self || !sock_guard) {
            return; // bridge or peer gone
        }

        QJsonObject payload = result.to_json();
        bridge_shape_payload(tool_name, payload);
        if (!call_id.isEmpty())
            payload["id"] = call_id;
        self->write_json_response(sock_guard, 200, payload);
        emit self->tool_called(tool_name, result.success);
    });
    watcher->setFuture(future);
}

// ── GET /tools ──────────────────────────────────────────────────────────────

void TerminalMcpBridge::handle_get_tools(QTcpSocket* sock) {
    // A caller gets exactly the catalog its own run policy produces, so a
    // dynamic refresh can never widen what dispatch will honour.
    const auto it = states_.constFind(sock);
    const auto scope = run_scope(it != states_.constEnd() ? it.value().run_token : QString());
    if (!scope) {
        write_error(sock, 401, "The agent run this token belonged to has ended");
        return;
    }
    QJsonObject payload;
    payload["tools"] = tool_definitions(scope->filter, scope->include_external, scope->catalog);
    write_json_response(sock, 200, payload);
}

// ── HTTP response helpers ───────────────────────────────────────────────────

static const char* bridge_status_text(int code) {
    switch (code) {
        case 200:
            return "OK";
        case 400:
            return "Bad Request";
        case 401:
            return "Unauthorized";
        case 403:
            return "Forbidden";
        case 404:
            return "Not Found";
        case 413:
            return "Payload Too Large";
        case 431:
            return "Request Header Fields Too Large";
        case 500:
            return "Internal Server Error";
        default:
            return "Unknown";
    }
}

void TerminalMcpBridge::write_json_response(QTcpSocket* sock, int status, const QJsonObject& body) {
    if (!sock || sock->state() != QAbstractSocket::ConnectedState)
        return;

    const QByteArray body_bytes = QJsonDocument(body).toJson(QJsonDocument::Compact);

    QByteArray response;
    response.reserve(body_bytes.size() + 256);
    response += "HTTP/1.1 " + QByteArray::number(status) + ' ' + bridge_status_text(status) + "\r\n";
    response += "Content-Type: application/json\r\n";
    response += "Content-Length: " + QByteArray::number(body_bytes.size()) + "\r\n";
    response += "Connection: close\r\n";
    response += "\r\n";
    response += body_bytes;

    sock->write(response);
    sock->flush();
    sock->disconnectFromHost();
}

void TerminalMcpBridge::write_error(QTcpSocket* sock, int status, const QString& message) {
    QJsonObject body;
    body["success"] = false;
    body["error"] = message;
    write_json_response(sock, status, body);
}

} // namespace fincept::mcp
