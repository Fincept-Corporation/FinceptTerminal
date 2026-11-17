// McpHttpServer_Tools.cpp — the terminal's backend for McpServerEngine: the
// catalogue snapshot (what tools/list advertises), bearer-token checks, the
// per-call policy, execution and result shaping. See McpHttpServer_p.h.
//
// The catalogue is the terminal's core tools (src/mcp/core/CoreTools.h) — small
// enough that every client gets all of it, so there is no toolset choice and no
// discovery round-trip.
//
// Policy for one tools/call (the engine already resolved the name, applied the
// run token's scope and the concurrency limits):
//   ui-control / navigation, look-only mode → isError, denied
//   python run / status / cancel, scripts off → isError, denied
//   auth level >= Verified                  → ToolApprovalDialog (≤ 50 s), then run
//                                             inside a ScopedApprovedCall
//   everything else                         → run; the core tools themselves ask
//                                             the user before risky ACTIONS
// Execution: McpProvider::call_tool_async under the caller's name
// (ScopedCallOrigin) on a pool thread; the pool thread returns as soon as the
// tool is STARTED and the result is shaped in a continuation, so no thread is
// parked on a tool and a slow tool cannot starve the others. The client's
// cancel flag and progress callback travel in the ToolContext.

#include "core/logging/Logger.h"
#include "mcp/McpProvider.h"
#include "mcp/ResultStore.h"
#include "mcp/TerminalMcpBridge.h"
#include "mcp/core/CoreTools.h"
#include "mcp/server/CallApproval.h"
#include "mcp/server/McpHttpServer_p.h"
#include "mcp/server/ToolApprovalDialog.h"

#include "auth/ConstantTime.h"

#include <QCryptographicHash>
#include <QDateTime>
#include <QFuture>
#include <QJsonDocument>
#include <QMutexLocker>
#include <QRegularExpression>
#include <QThreadPool>

#include <algorithm>
#include <exception>
#include <optional>

namespace fincept::mcp {

namespace mcp_http_detail {

namespace {

using server::AuthInfo;
using server::CallDone;
using server::CallOutcome;
using server::CallRequest;
using server::CataloguePtr;

bool is_advertised_name_char(QChar c) {
    const char16_t u = c.unicode();
    return (u >= 'a' && u <= 'z') || (u >= 'A' && u <= 'Z') || (u >= '0' && u <= '9') || u == '_' || u == '-';
}

bool is_valid_advertised_name(const QString& name) {
    if (name.isEmpty() || name.size() > kMaxAdvertisedNameLen)
        return false;
    return std::all_of(name.cbegin(), name.cend(), is_advertised_name_char);
}

QString make_title(const QString& name) {
    static const QRegularExpression separators(QStringLiteral("[_.\\-]+"));
    QStringList words = name.split(separators, Qt::SkipEmptyParts);
    for (QString& w : words)
        w[0] = w[0].toUpper();
    return words.join(QLatin1Char(' '));
}

QJsonObject normalized_schema(QJsonObject schema) {
    if (schema.value(QStringLiteral("type")).toString() != QLatin1String("object"))
        schema.insert(QStringLiteral("type"), QStringLiteral("object"));
    if (!schema.value(QStringLiteral("properties")).isObject())
        schema.insert(QStringLiteral("properties"), QJsonObject{});
    const QJsonValue required = schema.value(QStringLiteral("required"));
    if (schema.contains(QStringLiteral("required")) && (!required.isArray() || required.toArray().isEmpty()))
        schema.remove(QStringLiteral("required"));
    return schema;
}

/// Tools at AuthLevel >= Verified (ExplicitConfirm in practice) need the user's
/// per-call approval in ToolApprovalDialog. (The core tools ask per ACTION
/// themselves; this covers any tool that declares the level.)
bool needs_approval(AuthLevel auth) {
    // >= Verified, not just ExplicitConfirm: the AuthChecker AgentService installs
    // lets every tool at that level run only inside a user-approved call, so
    // asking is the only way such a tool can run at all.
    return auth >= AuthLevel::Verified;
}

/// Operating or rearranging the UI (categories ui-control and navigation) —
/// refused while allow_ui_control is off.
bool is_ui_control(const QString& category) {
    return category == QLatin1String(core::kCategoryUiControl) || category == QLatin1String(core::kCategoryNavigation);
}

struct ToolPolicyFacts {
    bool destructive = false;
    AuthLevel auth = AuthLevel::None;
    QString description;
};

QJsonObject tool_entry(const McpProvider::ToolAuditInfo& info, const QString& advertised) {
    QString description = info.description.trimmed();
    if (needs_approval(info.auth_required))
        description += QStringLiteral(" [Every call needs the user's approval in Fincept Terminal.]");
    const bool read_only = info.category == QLatin1String(core::kCategoryUiRead);
    // destructiveHint is what clients use to decide whether a call may run
    // without asking their own user (Codex `default_tools_approval_mode =
    // "writes"` prompts for every tool not marked read-only). ui_act / ui_invoke
    // can delete or cancel things, ui_open / ui_workspace close panels and delete
    // layouts, and python runs scripts — so every non-read tool is honestly "may
    // destroy", whatever its ToolDef flag says (the core tools gate risk per
    // ACTION, not per tool).
    const QJsonObject annotations{
        {QStringLiteral("readOnlyHint"), read_only},
        {QStringLiteral("destructiveHint"), !read_only || info.is_destructive},
        {QStringLiteral("idempotentHint"), read_only},
        {QStringLiteral("openWorldHint"), true},
    };
    return QJsonObject{
        {QStringLiteral("name"), advertised},
        {QStringLiteral("title"), make_title(info.name)},
        {QStringLiteral("description"), description},
        {QStringLiteral("inputSchema"), normalized_schema(info.input_schema)},
        {QStringLiteral("annotations"), annotations},
    };
}

/// Catalogue + the per-tool policy facts the engine does not carry.
struct TerminalCatalogue : server::Catalogue {
    QHash<QString, ToolPolicyFacts> policy; // by internal name
};

std::shared_ptr<const TerminalCatalogue> build_catalogue() {
    auto cat = std::make_shared<TerminalCatalogue>();
    auto& provider = McpProvider::instance();
    // Generation BEFORE the audit: a registration racing it leaves this snapshot
    // stale, so the next caller rebuilds instead of trusting a half-picture.
    cat->generation = provider.generation();
    const auto audit = provider.audit_all_tools(); // internal tools only — external servers are not re-exported
    QHash<QString, McpProvider::ToolAuditInfo> usable;
    for (const auto& info : audit) {
        if (info.enabled && info.has_handler)
            usable.insert(info.name, info);
    }
    // Core tools first, in their catalogue order; anything else after, sorted.
    QStringList order;
    for (const QString& n : core::core_tool_names()) {
        if (usable.contains(n))
            order << n;
    }
    QStringList rest;
    for (auto it = usable.cbegin(); it != usable.cend(); ++it) {
        if (!order.contains(it.key()))
            rest << it.key();
    }
    rest.sort();
    order += rest;
    for (const QString& n : order) {
        const McpProvider::ToolAuditInfo& info = usable[n];
        const QString advertised = make_advertised_name(info.name);
        if (cat->tools.contains(advertised)) {
            LOG_WARN(kTag, logf("Advertised MCP tool name collides; the tool stays callable by its internal name only",
                                {{"advertised", advertised}, {"tool", info.name}}));
            continue;
        }
        server::ToolMeta meta;
        meta.internal = info.name;
        meta.advertised = advertised;
        meta.category = info.category;
        meta.timeout_ms = info.default_timeout_ms;
        meta.input_schema = normalized_schema(info.input_schema);
        cat->tools.insert(advertised, meta);
        cat->internal_to_advertised.insert(info.name, advertised);
        cat->policy.insert(info.name, ToolPolicyFacts{info.is_destructive, info.auth_required, info.description});
        cat->entries.append(tool_entry(info, advertised));
    }
    return cat;
}

QString json_text(const QJsonValue& v) {
    return QString::fromUtf8(wire::compact_json_value(v));
}

/// MCP CallToolResult for a finished ToolResult. Oversized text is parked in
/// the ResultStore and replaced by a structure-preserving summary + a result id
/// readable with ui_read — never truncated silently. Image blocks (screenshots)
/// are forwarded as they are; they are not counted against the text budget.
/// No structuredContent: no tool declares an outputSchema, and the text block
/// already carries the JSON (see McpClientProfile.cpp for what each client does
/// with a copy).
QJsonObject shape_tool_result(const QString& tool, const ToolResult& tr, int budget_bytes, bool allow_park) {
    if (!tr.success) {
        QString text = tr.error.trimmed();
        if (text.isEmpty())
            text = QStringLiteral("Tool '%1' failed without an error message.").arg(tool);
        const QString extra = tr.message.trimmed();
        if (!extra.isEmpty() && extra != text)
            text += QLatin1Char('\n') + extra;
        if (text.size() > kMaxErrorTextChars)
            text = text.left(kMaxErrorTextChars) +
                   QStringLiteral("\n… [error text clipped: %1 more characters]").arg(text.size() - kMaxErrorTextChars);
        return wire::error_call_result(text);
    }

    QJsonObject envelope;
    const bool has_data = !tr.data.isNull() && !tr.data.isUndefined();
    if (has_data)
        envelope.insert(QStringLiteral("data"), tr.data);
    if (!tr.message.isEmpty())
        envelope.insert(QStringLiteral("message"), tr.message);

    QJsonArray content;
    const QByteArray serialized = wire::compact_json(envelope);
    if (serialized.size() > budget_bytes && allow_park) {
        const QString result_id = ResultStore::instance().put(tool, envelope);
        QJsonValue shaped(envelope);
        const ShrinkStats st = shrink_json(shaped, std::max(1024, budget_bytes - 800));
        QJsonObject out = shaped.isObject() ? shaped.toObject() : QJsonObject{{QStringLiteral("data"), shaped}};
        LOG_INFO(kTag, logf("Large tool result shaped for an external client",
                            {{"tool", tool}, {"kb", st.original_bytes / 1024}, {"result_id", result_id}}));
        const QString note = QStringLiteral("[Result truncated to fit the response budget (%1 → %2 bytes). Read the "
                                            "full payload with ui_read(ref=\"%3\", offset=0).]")
                                 .arg(st.original_bytes)
                                 .arg(st.final_bytes)
                                 .arg(result_id);
        if (out.value(QStringLiteral("message")).isString())
            content.append(wire::text_block(out.value(QStringLiteral("message")).toString()));
        if (out.contains(QStringLiteral("data")))
            content.append(wire::text_block(json_text(out.value(QStringLiteral("data")))));
        content.append(wire::text_block(note));
    } else {
        if (!tr.message.isEmpty())
            content.append(wire::text_block(tr.message));
        if (has_data)
            content.append(wire::text_block(tr.data.isString() ? tr.data.toString() : json_text(tr.data)));
    }
    for (const QJsonValue& block : tr.content) {
        if (block.isObject())
            content.append(block);
    }
    if (content.isEmpty())
        content.append(wire::text_block(QStringLiteral("OK")));
    return QJsonObject{{QStringLiteral("content"), content}, {QStringLiteral("isError"), false}};
}

CallOutcome refused(const QString& message, const QString& summary, bool denied) {
    CallOutcome out;
    out.payload = wire::error_call_result(message);
    out.denied = denied;
    out.summary = summary;
    return out;
}

class TerminalBackend final : public server::McpServerBackend {
  public:
    explicit TerminalBackend(std::shared_ptr<BackendShared> shared) : shared_(std::move(shared)) {}

    AuthInfo authenticate(const QByteArray& bearer) const override {
        AuthInfo a;
        if (bearer.isEmpty() || bearer.size() > 512)
            return a;
        const QString supplied = QString::fromLatin1(bearer);
        QString expected;
        QByteArray fingerprint;
        {
            QMutexLocker lock(&shared_->auth_mutex);
            expected = shared_->auth_token;
            fingerprint = shared_->auth_fingerprint;
        }
        if (!expected.isEmpty() && auth::constant_time_equals(supplied, expected)) {
            a.ok = true;
            a.fingerprint = fingerprint;
            return a;
        }
        // Or a live run-scoped token of an in-app harness call (its policy is
        // enforced per message by the engine and per call below).
        if (const auto policy = run_token_table().check(supplied, QDateTime::currentMSecsSinceEpoch())) {
            a.ok = true;
            a.run_token = true;
            a.policy = *policy;
            a.fingerprint = QCryptographicHash::hash(bearer, QCryptographicHash::Sha256).toHex().left(16);
        }
        return a;
    }

    CataloguePtr catalogue() override { return current_catalogue(*shared_); }

    QJsonObject server_info() const override {
        QString version;
        {
            QMutexLocker lock(&shared_->facts_mutex);
            version = shared_->app_version;
        }
        // "edition": both Fincept builds answer as "fincept-terminal"; the edition
        // id says which one this is (also in GET /health, checked by the stdio
        // bridge and the self-check before they send a token).
        return QJsonObject{{QStringLiteral("name"), QStringLiteral("fincept-terminal")},
                           {QStringLiteral("title"), QStringLiteral("Fincept Terminal (%1 edition)")
                                                         .arg(QString::fromLatin1(edition::kId))},
                           {QStringLiteral("version"), version},
                           {QStringLiteral("edition"), QString::fromLatin1(edition::kId)}};
    }

    QString instructions() const override { return server_instructions(); }

    bool content_request(const QString& method, const QJsonObject& params, QJsonObject* result, int* error_code,
                         QString* error_message) override {
        return serve_content(*shared_, method, params, result, error_code, error_message);
    }

    void call_tool(const CallRequest& request, CallDone done) override {
        std::shared_ptr<BackendShared> state = shared_;
        QThreadPool* pool = state->ensure_pool();
        pool->start([state, request, done]() {
            try {
                start_on_pool(state, request, done);
            } catch (const std::exception& e) {
                LOG_ERROR(kTag, logf("MCP tool call threw", {{"tool", request.tool.internal},
                                                             {"client", request.client_label},
                                                             {"error", QString::fromUtf8(e.what()).left(200)}}));
                done(refused(QStringLiteral("Internal error while running the tool: %1").arg(QString::fromUtf8(e.what())),
                             QStringLiteral("internal error"), false));
            } catch (...) {
                LOG_ERROR(kTag, logf("MCP tool call threw", {{"tool", request.tool.internal},
                                                             {"client", request.client_label},
                                                             {"error", "unknown exception"}}));
                done(refused(QStringLiteral("Internal error while running the tool."), QStringLiteral("internal error"),
                             false));
            }
        });
    }

  private:
    /// Pool thread: policy, approval (the one place a pool thread blocks), then
    /// START the tool and return; the result arrives through the continuation.
    static void start_on_pool(const std::shared_ptr<BackendShared>& state, const CallRequest& req, const CallDone& done) {
        if (!state->accepting.load()) {
            done(refused(QStringLiteral("Fincept Terminal's MCP server is stopping; the call was not run."),
                         QStringLiteral("denied — server stopping"), true));
            return;
        }
        if (req.cancel && req.cancel->load()) {
            done(refused(QStringLiteral("Cancelled by the client before it started; nothing was done."),
                         QStringLiteral("cancelled before start"), false));
            return;
        }
        const auto base = current_catalogue(*state);
        const auto* cat = static_cast<const TerminalCatalogue*>(base.get()); // every catalogue here is built by build_catalogue()
        const QString& internal = req.tool.internal;
        const ToolPolicyFacts facts = cat ? cat->policy.value(internal) : ToolPolicyFacts{};

        // ── The user's permissions for external agents ──
        if (is_ui_control(req.tool.category) && !state->allow_ui_control.load()) {
            done(refused(QStringLiteral("The user has set external agents to look-only in Fincept Terminal (Settings › AI "
                                        "Agents (MCP)): %1 cannot open, arrange or operate screens. Use ui_overview, "
                                        "ui_snapshot, ui_read and ui_screenshot, or ask the user to allow UI control.")
                             .arg(internal),
                         QStringLiteral("denied — look-only mode"), true));
            return;
        }
        if (internal == QLatin1String("python") && !state->allow_scripts.load()) {
            const QString action = req.args.value(QStringLiteral("action")).toString();
            if (action != QLatin1String("search") && action != QLatin1String("info")) {
                done(refused(QStringLiteral("The user has not allowed external agents to run the terminal's Python "
                                            "scripts (Settings › AI Agents (MCP)). python(action=\"search\"/\"info\") "
                                            "still works."),
                             QStringLiteral("denied — scripts not allowed"), true));
                return;
            }
        }
        if (facts.destructive && !state->allow_ui_control.load()) {
            done(refused(QStringLiteral("This tool changes terminal state and external agents are look-only."),
                         QStringLiteral("denied — look-only mode"), true));
            return;
        }

        bool approved = false;
        if (needs_approval(facts.auth)) {
            ToolApprovalRequest ar;
            ar.client = req.client_label;
            ar.tool = internal;
            ar.description = facts.description;
            ar.args = req.args;
            ar.destructive = facts.destructive;
            ar.kind = QStringLiteral("generic"); // a tool that declares the confirm level (no finer kind known)
            ar.client_id = req.client_id;
            ar.run_id = req.run_id;
            switch (ToolApprovalDialog::request_approval(ar)) {
                case ToolApprovalVerdict::Approved:
                    approved = true;
                    break;
                case ToolApprovalVerdict::Denied:
                    done(refused(QStringLiteral("Denied by the user in Fincept Terminal"),
                                 QStringLiteral("denied by the user"), true));
                    return;
                case ToolApprovalVerdict::TimedOut:
                    done(refused(QStringLiteral("Approval timed out — ask the user to approve it in Fincept Terminal "
                                                "and retry"),
                                 QStringLiteral("approval timed out"), true));
                    return;
                case ToolApprovalVerdict::Unavailable:
                    done(refused(QStringLiteral("Fincept Terminal could not ask the user to approve this call (no "
                                                "terminal window is open), so it was not run."),
                                 QStringLiteral("denied — no terminal window to ask"), true));
                    return;
            }
            if (!state->accepting.load() || (req.cancel && req.cancel->load())) {
                done(refused(QStringLiteral("The call was cancelled while waiting for approval; it was not run."),
                             QStringLiteral("cancelled"), false));
                return;
            }
        }

        // The client's cancel flag and progress callback reach the tool.
        ToolContext ctx; // timeout left at the default → the tool's own budget
        const server::CancelFlag cancel = req.cancel;
        if (cancel)
            ctx.is_cancelled = [cancel]() { return cancel->load(); };
        if (req.progress) {
            const auto progress = req.progress;
            ctx.on_progress = [progress](double fraction, const QString& message) {
                progress(std::clamp(fraction, 0.0, 1.0), 1.0, message);
            };
        }

        QFuture<ToolResult> future;
        {
            // Agent-call context for McpProvider's gate (auth runs synchronously on
            // this thread), the caller's name for any approval a tool asks for, and
            // a paper-only run token's guarantee.
            TerminalMcpBridge::ScopedCallFlags flags(/*call_in_progress=*/true,
                                                     /*destructive_allowed=*/approved || facts.destructive);
            ScopedCallOrigin origin(req.client_label);
            ScopedCallClient client(req.client_id, req.run_id); // stable ids for agent attribution
            std::optional<ScopedPaperRun> paper;
            if (req.paper_only)
                paper.emplace();
            std::optional<ScopedApprovedCall> approval_marker; // the user approved exactly this call
            if (approved)
                approval_marker.emplace();
            future = McpProvider::instance().call_tool_async(internal, req.args, ctx);
        }

        // A stored result is read back page by page; re-parking its page would loop.
        const bool allow_park = !(internal == QLatin1String("ui_read") &&
                                  req.args.value(QStringLiteral("ref")).toString().startsWith(QLatin1String("res_")));
        const int budget = req.result_budget_bytes > 0 ? req.result_budget_bytes : state->result_budget_bytes.load();
        const QString tool_name = internal;
        future
            .then(state->ensure_pool(),
                  [done, approved, allow_park, budget, tool_name](QFuture<ToolResult> f) {
                      ToolResult tr = f.resultCount() > 0
                                          ? f.result()
                                          : ToolResult::fail(QStringLiteral("Tool '%1' produced no result").arg(tool_name));
                      if (!tr.success && tr.error == QLatin1String("cancelled"))
                          tr.error = QStringLiteral("Cancelled — the client gave up on this call.");
                      CallOutcome out;
                      out.payload = shape_tool_result(tool_name, tr, budget, allow_park);
                      out.success = tr.success;
                      if (tr.success)
                          out.summary = !approved                            ? QStringLiteral("ok")
                                        : ToolApprovalDialog::auto_approve() ? QStringLiteral("ok (auto mode — not asked)")
                                                                             : QStringLiteral("ok (approved by the user)");
                      else
                          out.summary = QStringLiteral("error: ") + clip_text(tr.error, kSummaryMaxChars);
                      done(out);
                  })
            .onCanceled([done, tool_name]() {
                // A handler that dropped its promise without an answer.
                done(refused(QStringLiteral("Tool '%1' ended without a result.").arg(tool_name),
                             QStringLiteral("no result"), false));
            });
    }

    std::shared_ptr<BackendShared> shared_;
};

} // namespace

// ── Shared helpers ───────────────────────────────────────────────────────────

QString clip_text(const QString& text, int max_chars) {
    const QString flat = text.simplified();
    if (flat.size() <= max_chars)
        return flat;
    return flat.left(std::max(0, max_chars - 1)) + QChar(0x2026);
}

QString make_advertised_name(const QString& internal_name) {
    if (is_valid_advertised_name(internal_name))
        return internal_name;
    QString clean;
    clean.reserve(internal_name.size());
    for (const QChar c : internal_name)
        clean.append(is_advertised_name_char(c) ? c : QLatin1Char('_'));
    const QString hash = QString::fromLatin1(
        QCryptographicHash::hash(internal_name.toUtf8(), QCryptographicHash::Sha256).toHex().left(8));
    return clean.left(kAliasPrefixLen) + QLatin1Char('_') + hash;
}

server::CataloguePtr current_catalogue(BackendShared& shared) {
    const quint64 generation = McpProvider::instance().generation();
    {
        QMutexLocker lock(&shared.catalogue_mutex);
        if (shared.catalogue && shared.catalogue->generation == generation)
            return shared.catalogue;
    }
    // Built outside the lock: audit_all_tools() takes the provider's own mutex.
    server::CataloguePtr fresh = build_catalogue();
    QMutexLocker lock(&shared.catalogue_mutex);
    if (!shared.catalogue || shared.catalogue->generation <= fresh->generation)
        shared.catalogue = fresh;
    return shared.catalogue;
}

std::shared_ptr<server::McpServerBackend> make_terminal_backend(std::shared_ptr<BackendShared> shared) {
    return std::make_shared<TerminalBackend>(std::move(shared));
}

} // namespace mcp_http_detail

} // namespace fincept::mcp
