// src/services/agents/AgentService.cpp
//
// Singleton + state for the agent layer: instance accessor, cache, payload
// construction (build_api_keys / build_payload), Python-process invocation
// (run_python_light / run_python_stdin), and the DataHub publish surface
// (ensure_registered_with_hub / publish_agent_*). Higher-level entry points
// live in:
//   - AgentService_Discovery.cpp    — agent/tool/model discovery + configs
//   - AgentService_Execution.cpp    — run_agent, run_team, routing, multi
//   - AgentService_Workflows.cpp    — plans, portfolio analytics, etc.
//   - AgentService_Repositories.cpp — memory, sessions, paper trading
#include "services/agents/AgentService.h"

#include "auth/AuthManager.h"
#include "core/logging/Logger.h"
#include "datahub/DataHub.h"
#include "datahub/TopicPolicy.h"
#include "mcp/McpProvider.h"
#include "mcp/McpTypes.h"
#include "mcp/TerminalMcpBridge.h"
#include "python/PythonRunner.h"
#include "services/llm/LlmService.h"
#include "storage/cache/CacheManager.h"
#include "storage/repositories/AgentConfigRepository.h"
#include "storage/repositories/LlmConfigRepository.h"
#include "storage/repositories/LlmProfileRepository.h"
#include "storage/repositories/McpServerRepository.h"

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QJsonArray>
#include <QJsonDocument>
#include <QMetaObject>
#include <QPointer>
#include <QProcess>
#include <QSet>
#include <QTimer>
#include <QUuid>
#include <QVariant>

#include <atomic>
#include <memory>

namespace fincept::services {

namespace {
// How long run_python_light() waits for PythonRunner's async interpreter
// detection before giving up and failing the call.
constexpr int kPythonReadyGraceMs = 30000;

// Default excludes applied to every agent: UI-driving tools (navigation /
// system / settings), the recursive chat tools (ai-chat), and the tool
// discovery meta tools. Non-negotiable — per-agent config may only ADD to
// them. TerminalMcpBridge enforces the same set at dispatch.
const QStringList& default_tool_excludes() {
    static const QStringList kCats = {QStringLiteral("navigation"), QStringLiteral("system"),
                                      QStringLiteral("settings"), QStringLiteral("ai-chat"), QStringLiteral("meta")};
    return kCats;
}

/// Resolve the effective tool filter for one agent from its config, merging the
/// non-negotiable defaults with the agent's own `tool_filter` block.
///
/// Pulled out of build_payload so the SAME resolved filter can be handed to the
/// bridge as a run scope. It used to be inline and reachable only when the
/// catalog was being built, which meant the filter shaped what the agent was
/// TOLD about and nothing else.
///
/// Supported `tool_filter` keys:
///   categories[]            — whitelist (empty = all enabled)
///   exclude_categories[]    — blacklist, ON TOP of the defaults
///   name_patterns[]         — regex include on tool name
///   exclude_name_patterns[] — regex exclude on tool name
///   max_tools (int)         — hard cap on catalog size
mcp::ToolFilter resolve_tool_filter(const QJsonObject& config) {
    mcp::ToolFilter filter;
    filter.exclude_categories = default_tool_excludes();

    const QJsonObject tf = config.value("tool_filter").toObject();
    if (tf.isEmpty())
        return filter;

    if (tf.contains("categories")) {
        filter.categories.clear();
        for (const auto& v : tf["categories"].toArray())
            filter.categories.append(v.toString());
    }
    if (tf.contains("exclude_categories")) {
        // User excludes are ADDITIVE on top of defaults — defaults are
        // non-negotiable (UI tools are never safe for agents).
        for (const auto& v : tf["exclude_categories"].toArray()) {
            const QString cat = v.toString().trimmed();
            if (!cat.isEmpty() && !filter.exclude_categories.contains(cat))
                filter.exclude_categories.append(cat);
        }
    }
    if (tf.contains("name_patterns")) {
        for (const auto& v : tf["name_patterns"].toArray()) {
            const QString p = v.toString().trimmed();
            if (!p.isEmpty())
                filter.name_patterns.append(p);
        }
    }
    if (tf.contains("exclude_name_patterns")) {
        for (const auto& v : tf["exclude_name_patterns"].toArray()) {
            const QString p = v.toString().trimmed();
            if (!p.isEmpty())
                filter.exclude_name_patterns.append(p);
        }
    }
    filter.max_tools = tf.value("max_tools").toInt(0);
    return filter;
}

// McpServer.args is stored as a JSON array (current) or a space-separated
// string (legacy) — same two formats McpManager accepts.
QJsonArray agent_svc_mcp_args(const QString& raw) {
    QJsonArray out;
    if (raw.trimmed().startsWith(QLatin1Char('['))) {
        const QJsonDocument doc = QJsonDocument::fromJson(raw.toUtf8());
        if (doc.isArray()) {
            for (const auto& v : doc.array())
                out.append(v.toString());
            return out;
        }
    }
    for (const QString& part : raw.split(QLatin1Char(' '), Qt::SkipEmptyParts))
        out.append(part);
    return out;
}

// McpServer.env: a JSON object (current) or legacy "KEY=VAL KEY2=VAL2".
QJsonObject agent_svc_mcp_env(const QString& raw) {
    QJsonObject out;
    if (raw.trimmed().startsWith(QLatin1Char('{'))) {
        const QJsonDocument doc = QJsonDocument::fromJson(raw.toUtf8());
        if (doc.isObject()) {
            const QJsonObject o = doc.object();
            for (auto it = o.constBegin(); it != o.constEnd(); ++it)
                out.insert(it.key(), it.value().toString());
            return out;
        }
    }
    for (const QString& pair : raw.split(QLatin1Char(' '), Qt::SkipEmptyParts)) {
        const qsizetype eq = pair.indexOf(QLatin1Char('='));
        if (eq > 0)
            out.insert(pair.left(eq), pair.mid(eq + 1));
    }
    return out;
}

/// Fill in what a caller left out of an agent config before it is shipped to Python.
///
///  1. A saved (DB) agent named only by `agent_id` — the AI Chat selector, the
///     MCP run tools, routing results. Python resolves `agent_id` against its own
///     card registry, which knows nothing about agents created in the CREATE tab,
///     so those agents silently ran as the generic assistant (no instructions,
///     tools or feature flags). The saved config is merged UNDER whatever the
///     caller supplied, and the model is re-resolved from the agent's profile
///     assignment — the persisted `model` snapshot is deliberately ignored (it
///     goes stale, and new saves no longer carry an API key).
///  2. `mcp_server_ids` (the CREATE tab's MCP SERVERS selection) → the full
///     `mcp_servers` definitions core_agent._connect_mcp_servers expects. Done
///     here rather than at save time so server env secrets are never written
///     into the agent config.
QJsonObject agent_svc_expand_config(const QJsonObject& config) {
    QJsonObject out = config;

    const QString agent_id = out.value(QStringLiteral("agent_id")).toString();
    if (!agent_id.isEmpty() && !out.contains(QStringLiteral("instructions"))) {
        const auto saved = AgentConfigRepository::instance().get(agent_id);
        if (saved.is_ok()) {
            const QJsonObject saved_cfg = QJsonDocument::fromJson(saved.value().config_json.toUtf8()).object();
            for (auto it = saved_cfg.constBegin(); it != saved_cfg.constEnd(); ++it) {
                // `model`: stale snapshot (re-resolved below). terminal_mcp_* /
                // terminal_tools: run-scoped bridge wiring that build_payload() mints
                // itself — a stored value must never pre-empt (or smuggle in) a token.
                if (it.key() == QLatin1String("model") || it.key() == QLatin1String("terminal_tools") ||
                    it.key().startsWith(QLatin1String("terminal_mcp_")))
                    continue;
                if (!out.contains(it.key()))
                    out.insert(it.key(), it.value());
            }
            if (out.value(QStringLiteral("model")).toObject().value(QStringLiteral("provider")).toString().isEmpty()) {
                const ResolvedLlmProfile resolved =
                    ai_chat::LlmService::instance().resolve_profile(QStringLiteral("agent"), agent_id);
                if (!resolved.provider.isEmpty())
                    out.insert(QStringLiteral("model"), ai_chat::LlmService::profile_to_json(resolved));
            }
        }
    }

    const QJsonArray server_ids = out.value(QStringLiteral("mcp_server_ids")).toArray();
    if (!server_ids.isEmpty() && !out.contains(QStringLiteral("mcp_servers"))) {
        QSet<QString> wanted;
        for (const auto& v : server_ids)
            wanted.insert(v.toString());
        const auto servers = McpServerRepository::instance().list_all();
        if (servers.is_ok()) {
            QJsonArray defs;
            for (const auto& s : servers.value()) {
                if (!s.enabled || !wanted.contains(s.id))
                    continue;
                QJsonObject d;
                d.insert(QStringLiteral("id"), s.id);
                d.insert(QStringLiteral("name"), s.name);
                d.insert(QStringLiteral("command"), s.command);
                d.insert(QStringLiteral("args"), agent_svc_mcp_args(s.args));
                d.insert(QStringLiteral("env"), agent_svc_mcp_env(s.env));
                d.insert(QStringLiteral("transport"), QStringLiteral("stdio"));
                defs.append(d);
            }
            if (!defs.isEmpty())
                out.insert(QStringLiteral("mcp_servers"), defs);
        }
    }
    return out;
}

/// The bridge run-scope token build_payload() minted for this payload ("" when
/// the bridge is off or the caller supplied its own). Streaming runners pass it
/// to TerminalMcpBridge::end_run() when their subprocess exits.
QString agent_svc_run_token(const QJsonObject& payload) {
    return payload.value(QStringLiteral("config")).toObject().value(QStringLiteral("terminal_mcp_token")).toString();
}
} // namespace

// ── Singleton ────────────────────────────────────────────────────────────────

AgentService& AgentService::instance() {
    static AgentService inst;
    return inst;
}

AgentService::AgentService(QObject* parent) : QObject(parent) {
    // Phase 1/2 wiring — start the local HTTP bridge that exposes internal
    // MCP tools to the Python finagent subprocess, and install the auth
    // checker that gates agent-originated tool calls. Both are idempotent;
    // failure to bind the port is logged but not fatal — agents simply lose
    // terminal-tool access.
    connect(&mcp::TerminalMcpBridge::instance(), &mcp::TerminalMcpBridge::bridge_error, this,
            [](const QString& msg) { LOG_ERROR("AgentService", "Terminal MCP bridge error: " + msg); });
    if (mcp::TerminalMcpBridge::instance().start()) {
        LOG_INFO("AgentService", "Terminal MCP bridge started: " + mcp::TerminalMcpBridge::instance().endpoint());
    } else {
        LOG_WARN("AgentService", "Terminal MCP bridge failed to start — agents will run "
                                 "without internal tool access");
    }

    // Auth checker — process-wide. Distinguishes agent-originated calls
    // (via TerminalMcpBridge::is_call_in_progress) from chat-path calls so
    // chat behaviour is unchanged. Rules:
    //   - AuthLevel >= Verified  → always deny (no path can prove the
    //     gate non-interactively)
    //   - is_destructive + agent → deny (agents can't show a confirm modal;
    //     opt-in via per-agent config is Phase 5 work)
    //   - is_destructive + chat  → allow (subject to the fail-closed
    //     destructive capability gate McpProvider::check_authorization
    //     applies BEFORE this checker runs)
    //   - everything else        → allow
    //
    // ── The confirmation-modal seam ──────────────────────────────────────────
    //
    // The `required >= AuthLevel::Verified` line below is the SINGLE line to
    // change when the confirmation modal lands. It denies unconditionally, so
    // the 28 tools declared AuthLevel::ExplicitConfirm are refused on every
    // path — including for a user who has explicitly granted destructive
    // capability in Settings → Security. That is deliberate and stays as-is:
    // "ask the user" cannot be honoured while there is nobody to ask, and
    // failing closed is the only safe reading of a tool that asked to be
    // confirmed.
    //
    // What it currently blocks, so whoever builds the modal knows the blast
    // radius:
    //   - Correctly blocked, and must STAY blocked without an explicit
    //     per-call confirmation — real money and live credentials:
    //       live-trading (6): live_place_order, live_smart_order,
    //         live_cancel_order, live_cancel_all_orders, live_close_position,
    //         live_close_all_positions
    //       profile     (1): profile_get_api_key   (returns live key material)
    //       mcp-servers (6): install_mcp_server_from_marketplace,
    //         add_mcp_server, remove_mcp_server, start_mcp_server,
    //         restart_mcp_server, call_external_mcp_tool
    //         (each spawns or drives an arbitrary local child process)
    //   - Over-blocked pending the modal — destructive but recoverable, and
    //     arguably covered by the Settings capability grant alone:
    //       workspace (7): apply_layout, delete_layout, apply_layout_template,
    //         restore_last_workspace, delete_workspace_snapshot,
    //         restore_workspace_snapshot, close_window
    //       dashboard (3): load_dashboard_layout, apply_dashboard_template,
    //         clear_dashboard_layout
    //       agents    (3): delete_agent_config, delete_workflow,
    //         agent_paper_execute_trade
    //       excel     (1): delete_excel_sheet
    //       file_manager (1): download_managed_file
    //
    // When the modal exists, replace the line with a call that prompts and
    // returns the user's verdict. Do NOT relax it to `> Verified` or drop the
    // ExplicitConfirm level — the 13 tools in the first group depend on it.
    mcp::McpProvider::instance().set_auth_checker([](mcp::AuthLevel required, bool is_destructive) -> bool {
        if (required >= mcp::AuthLevel::Verified)
            return false;
        if (is_destructive && mcp::TerminalMcpBridge::is_call_in_progress() &&
            !mcp::TerminalMcpBridge::is_destructive_allowed())
            return false;
        return true;
    });
}

// ── Cache helpers ────────────────────────────────────────────────────────────

void AgentService::clear_cache() {
    fincept::CacheManager::instance().clear_category("agents");
    LOG_INFO("AgentService", "Cache cleared");
}

QVector<AgentInfo> AgentService::cached_agents() const {
    const QVariant cv = fincept::CacheManager::instance().get("agents:list");
    if (cv.isNull())
        return {};
    const QJsonObject root = QJsonDocument::fromJson(cv.toString().toUtf8()).object();
    QVector<AgentInfo> agents;
    for (const auto& v : root["agents"].toArray()) {
        const QJsonObject o = v.toObject();
        AgentInfo info;
        info.id = o["id"].toString();
        info.name = o["name"].toString();
        info.description = o["description"].toString();
        info.category = o["category"].toString();
        info.provider = o["provider"].toString();
        info.version = o["version"].toString();
        info.config = o["config"].toObject();
        for (const auto& c : o["capabilities"].toArray())
            info.capabilities.append(c.toString());
        agents.append(info);
    }
    return agents;
}

int AgentService::cached_agent_count() const {
    const QVariant cv = fincept::CacheManager::instance().get("agents:list");
    if (cv.isNull())
        return 0;
    return QJsonDocument::fromJson(cv.toString().toUtf8()).object()["agents"].toArray().size();
}

// ── API key builder ──────────────────────────────────────────────────────────

QJsonObject AgentService::build_api_keys() const {
    // Python's _get_api_key() looks up keys by env-var name (e.g. "ANTHROPIC_API_KEY"),
    // not by lowercase provider name. Send both forms so the lookup always succeeds.
    static const QMap<QString, QString> kEnvVarNames = {
        {"anthropic", "ANTHROPIC_API_KEY"},
        {"openai", "OPENAI_API_KEY"},
        {"google", "GOOGLE_API_KEY"},
        {"gemini", "GOOGLE_API_KEY"},
        {"groq", "GROQ_API_KEY"},
        {"deepseek", "DEEPSEEK_API_KEY"},
        {"financial_datasets", "FINANCIAL_DATASETS_API_KEY"},
        {"tavily", "TAVILY_API_KEY"},
        {"mistral", "MISTRAL_API_KEY"},
        {"cohere", "COHERE_API_KEY"},
        {"xai", "XAI_API_KEY"},
        {"kimi", "MOONSHOT_API_KEY"},
        {"moonshot", "MOONSHOT_API_KEY"},
    };

    QJsonObject keys;
    auto providers = LlmConfigRepository::instance().list_providers();
    if (providers.is_ok()) {
        for (const auto& p : providers.value()) {
            if (p.api_key.isEmpty())
                continue;
            const QString lower = p.provider.toLower();
            keys[lower] = p.api_key; // lowercase form
            const QString env_name = kEnvVarNames.value(lower);
            if (!env_name.isEmpty())
                keys[env_name] = p.api_key; // env-var form Python expects
            // Provider aliases: send canonical form so Python ModelsRegistry resolves correctly
            // e.g. "gemini" -> also set "google" so key lookup works after alias normalisation
            static const QMap<QString, QString> kProviderAliases = {
                {"gemini", "google"},
                {"claude", "anthropic"},
            };
            const QString alias = kProviderAliases.value(lower);
            if (!alias.isEmpty())
                keys[alias] = p.api_key;
            // Also ship base_url so super_agent/_llm_classify can use the right
            // endpoint (e.g. MiniMax OpenAI-compat behind "anthropic" provider).
            if (!p.base_url.isEmpty()) {
                keys[lower + "_base_url"] = p.base_url;
                keys[lower.toUpper() + "_BASE_URL"] = p.base_url;
            }
        }
    }

    // Always include Fincept session API key (from login) so agents can use
    // the fincept provider even if user hasn't manually configured it in Settings.
    if (!keys.contains("fincept")) {
        const auto& session = auth::AuthManager::instance().session();
        if (!session.api_key.isEmpty()) {
            keys["fincept"] = session.api_key;
            keys["FINCEPT_API_KEY"] = session.api_key;
        }
    }

    return keys;
}

// ── Payload builder ──────────────────────────────────────────────────────────

QJsonObject AgentService::build_payload(const QString& action, const QJsonObject& params,
                                        const QJsonObject& config_in) const {
    // Saved agents named only by id + MCP server selections become full configs
    // here, so every caller (chat, MCP tools, routing, tests) behaves the same.
    const QJsonObject config = agent_svc_expand_config(config_in);
    QJsonObject payload;
    payload["action"] = action;
    payload["api_keys"] = build_api_keys();

    // Inject the resolved LLM config as active_llm so Python can build the exact
    // model instance without re-resolving credentials.
    // Priority: config["model"] (per-agent resolved profile) > global active LLM.
    // This means: if the caller already embedded a resolved profile in config["model"]
    // (as build_config_from_editor() now does), Python gets the right per-agent creds.
    {
        if (config.contains("model") && !config["model"].toObject()["provider"].toString().isEmpty()) {
            // Use the per-agent resolved profile already embedded in the config
            payload["active_llm"] = config["model"].toObject();
        } else {
            auto& llm = ai_chat::LlmService::instance();
            if (llm.is_configured()) {
                QJsonObject active_llm;
                active_llm["provider"] = llm.active_provider();
                active_llm["model_id"] = llm.active_model();
                active_llm["api_key"] = llm.active_api_key();
                active_llm["base_url"] = llm.active_base_url();
                active_llm["temperature"] = llm.active_temperature();
                active_llm["max_tokens"] = llm.active_max_tokens();
                payload["active_llm"] = active_llm;
            }
        }
    }

    // Resolve user_id for per-persona SQLite isolation on the Python side.
    // Priority: params["user_id"] (caller override) > config["user_id"] > session-derived.
    // Session: user_info.id > 0 → QString::number(id); id == 0 (guest/unauth) → "guest".
    QJsonObject enriched_params = params;
    if (!enriched_params.contains("user_id") || enriched_params["user_id"].toString().isEmpty()) {
        QString uid;
        if (config.contains("user_id") && !config["user_id"].toString().isEmpty()) {
            uid = config["user_id"].toString();
        } else {
            const auto& session = auth::AuthManager::instance().session();
            uid = session.user_info.id > 0 ? QString::number(session.user_info.id) : QStringLiteral("guest");
        }
        enriched_params["user_id"] = uid;
    }

    // Phase 3 — inject the local MCP bridge endpoint + filtered tool catalog
    // into the config the Python toolkit reads. Skip if the caller already set
    // these (lets explicit overrides win) or if the bridge is not running.
    QJsonObject enriched_config = config;
    auto& bridge = mcp::TerminalMcpBridge::instance();
    // Honour an explicit opt-out from the agent config: if the agent author
    // set `terminal_tools_enabled=false`, skip all bridge wiring and let the
    // agent run with only its declared `tools` (yfinance/duckduckgo/etc.).
    const bool terminal_tools_enabled = enriched_config.value("terminal_tools_enabled").toBool(true);

    if (bridge.is_active() && terminal_tools_enabled) {
        if (!enriched_config.contains("terminal_mcp_endpoint"))
            enriched_config["terminal_mcp_endpoint"] = bridge.endpoint();

        // Resolve the per-agent filter ONCE, up front — before the catalog is
        // built and regardless of whether it is built at all. It is both the
        // catalog shape AND the dispatch policy; a caller that pre-supplied
        // `terminal_tools` must still be held to it.
        const mcp::ToolFilter filter = resolve_tool_filter(enriched_config);
        const bool include_external = enriched_config.value("include_external_mcp").toBool(true);

        if (!enriched_config.contains("terminal_mcp_token")) {
            // Run-scoped token, not the process token: it carries `filter` to
            // the bridge so an agent that names a tool its own config excluded
            // is refused at dispatch, not merely omitted from its catalog.
            // Retired by run_python_stdin when the agent process exits.
            enriched_config["terminal_mcp_token"] = bridge.begin_run(filter, include_external);
        }
        // Capability token — only injected when the agent config opts in.
        // Without this header on each request the bridge will block any
        // `is_destructive=true` tool call even if the agent's LLM tries one.
        if (enriched_config.value("allow_destructive_tools").toBool(false) &&
            !enriched_config.contains("terminal_mcp_destructive_token")) {
            enriched_config["terminal_mcp_destructive_token"] = bridge.destructive_token();
        }
        if (!enriched_config.contains("terminal_tools"))
            enriched_config["terminal_tools"] = bridge.tool_definitions(filter, include_external);

        // Dry-run mode is opt-in and read by the Python TerminalToolkit. When
        // true, the toolkit short-circuits each call and returns a synthetic
        // result without crossing the bridge — useful for testing prompts /
        // agent loops without touching real state. We just propagate the
        // flag; nothing on the C++ side changes.
        // (No-op here — `tools_dry_run` already lives in enriched_config if
        // the agent set it; CreateAgentPanel writes the key at save time.)
    }

    if (!enriched_params.isEmpty())
        payload["params"] = enriched_params;
    if (!enriched_config.isEmpty())
        payload["config"] = enriched_config;
    return payload;
}

QJsonObject AgentService::model_config_for_profile(const QString& profile_id, const QString& context_type) const {
    ResolvedLlmProfile resolved;
    if (!profile_id.isEmpty()) {
        const auto pr = LlmProfileRepository::instance().get_profile(profile_id);
        if (pr.is_ok()) {
            const LlmProfile& p = pr.value();
            resolved.profile_id = p.id;
            resolved.profile_name = p.name;
            resolved.provider = p.provider;
            resolved.model_id = p.model_id;
            resolved.api_key = p.api_key;
            resolved.base_url = p.base_url;
            resolved.temperature = p.temperature;
            resolved.max_tokens = p.max_tokens;
            resolved.system_prompt = p.system_prompt;
        }
    }
    if (resolved.provider.isEmpty() && !context_type.isEmpty())
        resolved = ai_chat::LlmService::instance().resolve_profile(context_type, {});
    if (resolved.provider.isEmpty())
        return {};
    return ai_chat::LlmService::profile_to_json(resolved);
}

// ── Python lightweight runner ────────────────────────────────────────────────

void AgentService::run_python_light(const QString& action, const QJsonObject& params,
                                    std::function<void(bool, QJsonObject)> on_result) {
    // SECURITY: this used to hand the payload to PythonRunner::run() as a
    // command-line argument. That payload is not "light" — build_payload()
    // embeds every configured LLM provider API key, the Fincept session key,
    // the MCP bridge token, the destructive-capability token, and the whole
    // terminal_tools catalog. As argv it is readable by any process running as
    // the same user (Win32_Process.CommandLine via WMI on Windows, no
    // elevation; /proc/<pid>/cmdline on Linux) and it is captured by crash
    // dumps and EDR telemetry. Worse, the catalog pushes it past PythonRunner's
    // 8 KB argv-spill threshold, so it was written to a temp file in
    // QStandardPaths::TempLocation with default (0644 in /tmp) permissions.
    //
    // stdin has neither problem: it is never visible outside the pipe and never
    // touches disk. run_python_stdin() already does exactly this for the other
    // ~40 actions, so the light path just delegates to it.
    auto& py = python::PythonRunner::instance();
    if (py.is_available()) {
        run_python_stdin(action, params, {}, std::move(on_result));
        return;
    }

    // Interpreter detection can still be in flight on a cold install, and
    // discover_agents() fires from main.cpp at startup. PythonRunner::run()
    // used to queue the request until detection finished; replicate that by
    // waiting for python_ready, with a bounded fallback so the caller always
    // gets an answer even when no interpreter is ever found (python_ready is
    // not emitted in that case).
    auto fired = std::make_shared<std::atomic_bool>(false);
    QPointer<AgentService> self = this;
    auto resume = [self, action, params, on_result, fired](bool ready) {
        if (fired->exchange(true))
            return;
        if (!self)
            return;
        if (!ready) {
            LOG_WARN("AgentService", QString("%1 skipped — no Python interpreter available").arg(action));
            on_result(false, QJsonObject{{"error", "Python not available"}});
            return;
        }
        self->run_python_stdin(action, params, {}, on_result);
    };
    connect(&py, &python::PythonRunner::python_ready, this, [resume]() { resume(true); },
            Qt::SingleShotConnection);
    QTimer::singleShot(kPythonReadyGraceMs, this, [resume]() { resume(false); });
}

// ── Python stdin runner (for large payloads) ─────────────────────────────────

void AgentService::run_python_stdin(const QString& action, const QJsonObject& params, const QJsonObject& config,
                                    std::function<void(bool, QJsonObject)> on_result) {
    auto& py = python::PythonRunner::instance();
    if (!py.is_available()) {
        // Deferred, never synchronous: callers (run_agent, run_team, …) hand the
        // request id back to their panel only AFTER this returns, and the panel's
        // request-id guard drops a result that arrives earlier — the UI then sat
        // on "Executing..." forever. Cold-start interpreter detection makes this
        // reachable whenever a run is started in the first seconds.
        QMetaObject::invokeMethod(
            this, [on_result]() { on_result(false, QJsonObject{{"error", "Python not available"}}); },
            Qt::QueuedConnection);
        return;
    }

    QJsonObject payload = build_payload(action, params, config);
    QByteArray payload_bytes = QJsonDocument(payload).toJson(QJsonDocument::Compact);

    // Bridge run scope opened by build_payload — retire it the moment the agent
    // process is gone so its token stops authenticating. Empty (or a
    // caller-supplied token, which the bridge never registered) makes end_run a
    // no-op, so this is safe for every action including the non-agent ones.
    const QString run_token = agent_svc_run_token(payload);

    QString python_path = py.python_path();
    QString script_path = py.scripts_dir() + "/agents/finagent_core/main.py";

    // Spawn QProcess directly for stdin writing (P4 exception like ExchangeService)
    auto* proc = new QProcess(this);
    // Share the standard Python env + cwd + Windows console suppression with
    // PythonRunner so every finagent spawn sees the same FINCEPT_DATA_DIR,
    // FINAGENT_DATA_DIR, and PYTHONPATH.
    proc->setProcessEnvironment(py.build_python_env());
    proc->setWorkingDirectory(py.scripts_dir());
#ifdef _WIN32
    proc->setCreateProcessArgumentsModifier([](QProcess::CreateProcessArguments* cpa) {
        cpa->flags |= 0x08000000; // CREATE_NO_WINDOW
    });
#endif
    QPointer<AgentService> self = this;
    auto timer = std::make_shared<QElapsedTimer>();
    timer->start();
    // A crashed child raises BOTH errorOccurred(Crashed) and finished(); without a
    // shared "already answered" flag on_result fired twice (two error toasts /
    // two signals for one request).
    auto settled = std::make_shared<bool>(false);

    connect(proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this,
            [self, proc, action, on_result, timer, run_token, settled](int exit_code, QProcess::ExitStatus) {
                mcp::TerminalMcpBridge::instance().end_run(run_token);
                int elapsed = timer->elapsed();

                QString stdout_str = QString::fromUtf8(proc->readAllStandardOutput());
                QString stderr_str = QString::fromUtf8(proc->readAllStandardError());
                proc->deleteLater();

                if (!self || *settled)
                    return;
                *settled = true;

                // Extract JSON from output
                QString json_str = python::extract_json(stdout_str);
                if (json_str.isEmpty() && exit_code == 0) {
                    json_str = stdout_str.trimmed();
                }

                QJsonDocument doc = QJsonDocument::fromJson(json_str.toUtf8());
                if (exit_code != 0 || doc.isNull()) {
                    LOG_ERROR("AgentService", QString("%1 failed (exit=%2, %3ms): %4")
                                                  .arg(action)
                                                  .arg(exit_code)
                                                  .arg(elapsed)
                                                  .arg(stderr_str.left(200)));
                    QString err = stderr_str.isEmpty() ? "Agent execution failed" : stderr_str.left(500);
                    on_result(false, QJsonObject{{"error", err}});
                    return;
                }

                LOG_INFO("AgentService", QString("%1 completed in %2ms").arg(action).arg(elapsed));
                QJsonObject result = doc.object();
                result["execution_time_ms"] = elapsed;
                on_result(true, result);
            });

    connect(proc, &QProcess::errorOccurred, this,
            [self, proc, action, on_result, run_token, settled](QProcess::ProcessError e) {
                // Only a failed spawn ends without a finished() signal. A crash is
                // reported by finished() (with the child's stderr, which is a far
                // better message than "Process crashed"); write/read errors mean
                // the child is already on its way out.
                if (e != QProcess::FailedToStart) {
                    LOG_WARN("AgentService",
                             QString("%1 process error %2: %3").arg(action).arg(int(e)).arg(proc->errorString()));
                    return;
                }
                mcp::TerminalMcpBridge::instance().end_run(run_token);
                QString err = proc->errorString();
                proc->deleteLater();
                if (!self || *settled)
                    return;
                *settled = true;
                LOG_ERROR("AgentService", QString("%1 process error: %2").arg(action, err));
                on_result(false, QJsonObject{{"error", "Process error: " + err}});
            });

    LOG_INFO("AgentService", QString("Running %1 via stdin (%2 bytes)").arg(action).arg(payload_bytes.size()));
    proc->start(python_path, {script_path, "--stdin"});
    proc->write(payload_bytes);
    proc->closeWriteChannel();
}

// ── Agent discovery ──────────────────────────────────────────────────────────

QString AgentService::make_cache_key(const QString& action, const QJsonObject& params) const {
    return action + "|" + QString::fromUtf8(QJsonDocument(params).toJson(QJsonDocument::Compact));
}

bool AgentService::get_cached_response(const QString& key, QJsonObject& out) const {
    const QVariant cv = fincept::CacheManager::instance().get("agents:resp:" + key);
    if (cv.isNull())
        return false;
    out = QJsonDocument::fromJson(cv.toString().toUtf8()).object();
    return true;
}

void AgentService::set_cached_response(const QString& key, const QJsonObject& data) {
    fincept::CacheManager::instance().put(
        "agents:resp:" + key, QVariant(QString::fromUtf8(QJsonDocument(data).toJson(QJsonDocument::Compact))),
        kResponseCacheTtlSec, "agents");
}

void AgentService::clear_response_cache() {
    fincept::CacheManager::instance().remove_prefix("agents:resp:");
    LOG_INFO("AgentService", "Response cache cleared");
}

QJsonObject AgentService::get_cache_stats() const {
    QJsonObject stats;
    stats["agents_cached"] = cached_agent_count();
    stats["total_entries"] = fincept::CacheManager::instance().entry_count();
    return stats;
}

// ── DataHub integration (Phase 9) ─────────────────────────────────────────────

QStringList AgentService::topic_patterns() const {
    // AgentService is push-only: it owns these families but never services a
    // pull-through `refresh()`. Patterns are declared so set_policy_pattern()
    // entries bind to this producer for introspection/stats.
    return {
        QStringLiteral("agent:output:*"),  QStringLiteral("agent:stream:*"), QStringLiteral("agent:status:*"),
        QStringLiteral("agent:routing:*"), QStringLiteral("agent:error:*"),  QStringLiteral("task:event:*"),
    };
}

void AgentService::refresh(const QStringList& /*topics*/) {
    // Push-only — no pull semantics. Run outputs materialise when a caller
    // triggers run_agent/run_team/run_workflow.
}

int AgentService::max_requests_per_sec() const {
    return 0; // push-only, no outbound rate cap
}

void AgentService::ensure_registered_with_hub() {
    if (hub_registered_)
        return;
    auto& hub = fincept::datahub::DataHub::instance();
    hub.register_producer(this);

    // Output: short-lived per-run topic, retired on completion.
    fincept::datahub::TopicPolicy output_policy;
    output_policy.push_only = true;
    output_policy.ttl_ms = 10 * 60 * 1000; // safety net if retire_topic is missed
    hub.set_policy_pattern(QStringLiteral("agent:output:*"), output_policy);

    // Stream: token firehose — coalesce at 50ms so subscribers don't drown.
    fincept::datahub::TopicPolicy stream_policy;
    stream_policy.push_only = true;
    stream_policy.coalesce_within_ms = 50;
    stream_policy.ttl_ms = 5 * 60 * 1000;
    hub.set_policy_pattern(QStringLiteral("agent:stream:*"), stream_policy);

    // Status: thinking/tool narration — same shape as stream.
    fincept::datahub::TopicPolicy status_policy;
    status_policy.push_only = true;
    status_policy.coalesce_within_ms = 100;
    status_policy.ttl_ms = 5 * 60 * 1000;
    hub.set_policy_pattern(QStringLiteral("agent:status:*"), status_policy);

    // Routing: one-shot decision per run.
    fincept::datahub::TopicPolicy routing_policy;
    routing_policy.push_only = true;
    routing_policy.ttl_ms = 10 * 60 * 1000;
    hub.set_policy_pattern(QStringLiteral("agent:routing:*"), routing_policy);

    // Error: rare but important — keep briefly for late subscribers.
    fincept::datahub::TopicPolicy error_policy;
    error_policy.push_only = true;
    error_policy.ttl_ms = 2 * 60 * 1000;
    hub.set_policy_pattern(QStringLiteral("agent:error:*"), error_policy);

    // Agentic Mode: per-task event stream. One topic per task; retired when
    // a terminal event (done/error/cancelled) lands. Coalesce step_end bursts
    // so high-frequency updates don't overwhelm subscribers.
    fincept::datahub::TopicPolicy task_event_policy;
    task_event_policy.push_only = true;
    task_event_policy.coalesce_within_ms = 50;
    task_event_policy.ttl_ms = 10 * 60 * 1000;
    hub.set_policy_pattern(QStringLiteral("task:event:*"), task_event_policy);

    hub_registered_ = true;
    LOG_INFO("AgentService", "Registered with DataHub (agent:*)");
}

void AgentService::publish_agent_result(const AgentExecutionResult& r, bool final) {
    if (!hub_registered_ || r.request_id.isEmpty())
        return;
    QJsonObject obj{
        {"request_id", r.request_id},
        {"success", r.success},
        {"response", r.response},
        {"error", r.error},
        {"execution_time_ms", r.execution_time_ms},
        {"final", final},
    };
    const QString topic = QStringLiteral("agent:output:") + r.request_id;
    fincept::datahub::DataHub::instance().publish(topic, QVariant(obj));
    if (final) {
        // Disposable per-run topic — drop cached state to bound hub memory.
        // Subscribers still pinned via owner remain attached.
        fincept::datahub::DataHub::instance().retire_topic(topic);
    }
}

void AgentService::publish_agent_token(const QString& run_id, const QString& token) {
    if (!hub_registered_ || run_id.isEmpty())
        return;
    QJsonObject obj{{"request_id", run_id}, {"token", token}};
    fincept::datahub::DataHub::instance().publish(QStringLiteral("agent:stream:") + run_id, QVariant(obj));
}

void AgentService::publish_agent_status(const QString& run_id, const QString& status) {
    if (!hub_registered_ || run_id.isEmpty())
        return;
    QJsonObject obj{{"request_id", run_id}, {"status", status}};
    fincept::datahub::DataHub::instance().publish(QStringLiteral("agent:status:") + run_id, QVariant(obj));
}

void AgentService::publish_routing_result(const RoutingResult& r) {
    if (!hub_registered_ || r.request_id.isEmpty())
        return;
    QJsonObject obj{
        {"request_id", r.request_id}, {"success", r.success},       {"agent_id", r.agent_id},
        {"intent", r.intent},         {"confidence", r.confidence},
    };
    fincept::datahub::DataHub::instance().publish(QStringLiteral("agent:routing:") + r.request_id, QVariant(obj));
}

void AgentService::publish_agent_error(const QString& context, const QString& message) {
    if (!hub_registered_)
        return;
    QJsonObject obj{{"context", context}, {"message", message}};
    fincept::datahub::DataHub::instance().publish(QStringLiteral("agent:error:") + context, QVariant(obj));
}

void AgentService::publish_task_event(const QString& task_id, const QJsonObject& event) {
    if (task_id.isEmpty())
        return;
    emit task_event(task_id, event);
    if (!hub_registered_)
        return;
    const QString topic = QStringLiteral("task:event:") + task_id;
    fincept::datahub::DataHub::instance().publish(topic, QVariant(event));
    const QString kind = event.value(QStringLiteral("kind")).toString();
    if (kind == QStringLiteral("done") || kind == QStringLiteral("error") || kind == QStringLiteral("cancelled")) {
        // Disposable per-task topic — drop cached state to bound hub memory.
        fincept::datahub::DataHub::instance().retire_topic(topic);
    }
}

} // namespace fincept::services
