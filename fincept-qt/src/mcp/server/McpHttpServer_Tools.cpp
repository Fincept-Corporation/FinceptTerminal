// McpHttpServer_Tools.cpp — the tool side of the external MCP server: catalogue
// snapshot (what tools/list advertises), the per-call policy, execution on a
// pool thread, and result shaping. See McpHttpServer_p.h for the file map.
//
// Policy for one tools/call (the INNER tool's, when the call is tool_invoke):
//   unknown / disabled                → JSON-RPC -32602
//   external-server tool via tool_invoke → refused (not re-exported)
//   destructive && !allow_actions     → isError, denied
//   live-trading || auth >= Verified  → ToolApprovalDialog (≤ 50 s), then run with
//                                       ScopedApprovedCall + destructive capability
//   destructive (actions allowed)     → run with destructive capability
//   read-only                         → run as an agent call without it
// Execution is always McpService::execute_tool(INTERNAL_SERVER_ID, …,
// allow_defer=true): a long tool returns a job receipt instead of blocking.

#include "core/logging/Logger.h"
#include "mcp/McpProvider.h"
#include "mcp/McpService.h"
#include "mcp/ResultStore.h"
#include "mcp/TerminalMcpBridge.h"
#include "mcp/server/CallApproval.h"
#include "mcp/server/EssentialTools.h"
#include "mcp/server/McpHttpServer_p.h"
#include "mcp/server/ToolApprovalDialog.h"

#include <QCryptographicHash>
#include <QDateTime>
#include <QJsonDocument>
#include <QMutexLocker>
#include <QRegularExpression>
#include <QSet>

#include <algorithm>

namespace fincept::mcp {

namespace mcp_http_detail {

namespace {

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

QJsonObject tool_entry(const CatalogTool& t) {
    QString description = t.description.trimmed();
    const bool approval = needs_approval(t);
    if (t.destructive && approval)
        description += QStringLiteral(" [Changes terminal state: runs only if the user allowed actions for external "
                                      "agents, and every call needs their approval in Fincept Terminal.]");
    else if (t.destructive)
        description += QStringLiteral(" [Changes terminal state: runs only if the user allowed actions for external "
                                      "agents.]");
    else if (approval)
        description += QStringLiteral(" [Every call needs the user's approval in Fincept Terminal.]");

    const QJsonObject annotations{
        {QStringLiteral("readOnlyHint"), !t.destructive},
        {QStringLiteral("destructiveHint"), t.destructive},
        {QStringLiteral("openWorldHint"), true},
    };
    return QJsonObject{
        {QStringLiteral("name"), t.advertised},
        {QStringLiteral("title"), make_title(t.name)},
        {QStringLiteral("description"), description},
        {QStringLiteral("inputSchema"), t.input_schema},
        {QStringLiteral("annotations"), annotations},
    };
}

CataloguePtr build_catalogue() {
    auto cat = std::make_shared<Catalogue>();
    auto& provider = McpProvider::instance();
    // Generation BEFORE the audit: a registration racing it leaves this snapshot
    // stale, so the next caller rebuilds instead of trusting a half-picture.
    cat->generation = provider.generation();
    const auto audit = provider.audit_all_tools(); // internal tools only — external servers are not re-exported
    for (const auto& info : audit) {
        if (!info.enabled || !info.has_handler)
            continue;
        CatalogTool t;
        t.name = info.name;
        t.advertised = make_advertised_name(info.name);
        t.description = info.description;
        t.category = info.category;
        t.input_schema = normalized_schema(info.input_schema);
        t.destructive = info.is_destructive;
        t.auth = info.auth_required;
        if (cat->by_advertised.contains(t.advertised))
            LOG_WARN(kTag, QStringLiteral("Advertised name '%1' collides — '%2' stays callable by its internal name only")
                               .arg(t.advertised, t.name));
        else
            cat->by_advertised.insert(t.advertised, t.name);
        cat->tools.insert(t.name, t);
    }

    QStringList names = cat->tools.keys();
    std::sort(names.begin(), names.end());
    for (const QString& n : names)
        cat->full_entries.append(tool_entry(cat->tools.value(n)));

    // Essentials: curated order; meta tools always (when registered), research
    // tools only while they stay read-only and approval-free.
    const QStringList meta = essential_meta_tool_names();
    QSet<QString> seen;
    for (const QString& n : essential_tool_names()) {
        if (seen.contains(n))
            continue;
        const auto it = cat->tools.constFind(n);
        if (it == cat->tools.constEnd())
            continue; // renamed, disabled or not in this build
        if (!meta.contains(n) && (it->destructive || needs_approval(*it)))
            continue;
        seen.insert(n);
        cat->essential_entries.append(tool_entry(*it));
    }
    cat->has_tool_invoke = cat->tools.contains(QString::fromLatin1(kToolInvoke));
    return cat;
}

QJsonObject text_block(const QString& text) {
    return QJsonObject{{QStringLiteral("type"), QStringLiteral("text")}, {QStringLiteral("text"), text}};
}

QString json_text(const QJsonValue& v) {
    if (v.isObject())
        return QString::fromUtf8(compact_json(v.toObject()));
    if (v.isArray())
        return QString::fromUtf8(compact_json(v.toArray()));
    // QJsonDocument only holds containers: serialise a scalar inside an array and unwrap it.
    const QByteArray wrapped = compact_json(QJsonArray{v});
    return QString::fromUtf8(wrapped.mid(1, wrapped.size() - 2));
}

/// MCP CallToolResult for a finished ToolResult. Oversized payloads are parked in
/// the ResultStore and replaced by a structure-preserving summary + result_id —
/// never truncated silently.
QJsonObject shape_tool_result(const QString& tool, const ToolResult& tr) {
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
        return error_call_result(text);
    }

    QJsonObject envelope;
    const bool has_data = !tr.data.isNull() && !tr.data.isUndefined();
    if (has_data)
        envelope.insert(QStringLiteral("data"), tr.data);
    if (!tr.message.isEmpty())
        envelope.insert(QStringLiteral("message"), tr.message);

    // result_fetch pages a parked payload; re-parking its page would only loop.
    const QByteArray serialized = compact_json(envelope);
    if (serialized.size() > kResultBudgetBytes && tool != QLatin1String(kResultFetch)) {
        const QString result_id = ResultStore::instance().put(tool, envelope);
        QJsonValue shaped(envelope);
        const ShrinkStats st = shrink_json(shaped, std::max(1024, kResultBudgetBytes - 800));
        QJsonObject out = shaped.isObject() ? shaped.toObject() : QJsonObject{{QStringLiteral("data"), shaped}};
        out.insert(QStringLiteral("truncated"), true);
        out.insert(QStringLiteral("result_id"), result_id);
        out.insert(QStringLiteral("full_bytes"), st.original_bytes);
        out.insert(QStringLiteral("note"),
                   QStringLiteral("Result truncated to fit the response budget (%1 → %2 bytes). Page the full payload "
                                  "with result_fetch(result_id='%3', offset=0, limit=6000).")
                       .arg(st.original_bytes)
                       .arg(st.final_bytes)
                       .arg(result_id));
        LOG_INFO(kTag, QStringLiteral("Tool '%1' returned %2 KB — shaped for the external client, full payload parked "
                                      "as %3")
                           .arg(tool)
                           .arg(st.original_bytes / 1024)
                           .arg(result_id));
        return QJsonObject{
            {QStringLiteral("content"), QJsonArray{text_block(QString::fromUtf8(compact_json(out)))}},
            {QStringLiteral("structuredContent"), out},
            {QStringLiteral("isError"), false},
        };
    }

    QJsonArray content;
    if (!tr.message.isEmpty())
        content.append(text_block(tr.message));
    QJsonObject result;
    if (has_data) {
        content.append(text_block(tr.data.isString() ? tr.data.toString() : json_text(tr.data)));
        if (tr.data.isObject())
            result.insert(QStringLiteral("structuredContent"), tr.data.toObject());
    }
    if (content.isEmpty())
        content.append(text_block(QStringLiteral("OK")));
    result.insert(QStringLiteral("content"), content);
    result.insert(QStringLiteral("isError"), false);
    return result;
}

const QString& actions_disabled_message() {
    static const QString m = QStringLiteral(
        "This tool changes state in the terminal (orders, files, settings). The user has not allowed external agents "
        "to run actions. Ask them to enable it in Fincept Terminal → Agent Workbench → Connect, or perform this step "
        "in the terminal.");
    return m;
}

CallOutcome refused(const QString& message, const QString& summary, bool denied) {
    CallOutcome out;
    out.payload = error_call_result(message);
    out.denied = denied;
    out.summary = summary;
    return out;
}

} // namespace

// ── Shared helpers ───────────────────────────────────────────────────────────

qint64 now_ms() {
    return QDateTime::currentMSecsSinceEpoch();
}

QByteArray compact_json(const QJsonObject& object) {
    return QJsonDocument(object).toJson(QJsonDocument::Compact);
}

QByteArray compact_json(const QJsonArray& array) {
    return QJsonDocument(array).toJson(QJsonDocument::Compact);
}

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

QJsonObject error_call_result(const QString& message) {
    return QJsonObject{{QStringLiteral("content"), QJsonArray{text_block(message)}}, {QStringLiteral("isError"), true}};
}

bool needs_approval(const CatalogTool& tool) {
    // >= Verified, not just ExplicitConfirm: the AuthChecker AgentService installs
    // lets every tool at that level run only inside a user-approved call, so
    // asking is the only way such a tool can run at all.
    return tool.category == QLatin1String(kLiveTradingCategory) || tool.auth >= AuthLevel::Verified;
}

QString resolve_tool_name(const Catalogue& catalogue, const QString& called) {
    if (called.isEmpty())
        return {};
    const auto adv = catalogue.by_advertised.constFind(called);
    if (adv != catalogue.by_advertised.constEnd())
        return adv.value();
    if (catalogue.tools.contains(called))
        return called;
    return {};
}

CataloguePtr current_catalogue(WorkerShared& shared) {
    const quint64 generation = McpProvider::instance().generation();
    {
        QMutexLocker lock(&shared.catalogue_mutex);
        if (shared.catalogue && shared.catalogue->generation == generation)
            return shared.catalogue;
    }
    // Built outside the lock: audit_all_tools() takes the provider's own mutex.
    CataloguePtr fresh = build_catalogue();
    QMutexLocker lock(&shared.catalogue_mutex);
    if (!shared.catalogue || shared.catalogue->generation <= fresh->generation)
        shared.catalogue = fresh;
    return shared.catalogue;
}

// ── One tools/call (pool thread) ─────────────────────────────────────────────

CallOutcome run_tool_call(WorkerShared& shared, const CallJob& job) {
    const CataloguePtr cat = current_catalogue(shared);
    const QString internal = resolve_tool_name(*cat, job.called_name);
    if (internal.isEmpty()) {
        CallOutcome out;
        out.rpc_error = true;
        out.payload = rpc_error_body(
            -32602, QStringLiteral("Unknown tool: %1. Use tool_list to search the catalogue.").arg(job.called_name));
        out.summary = QStringLiteral("unknown tool");
        return out;
    }

    // The tool whose policy applies: for tool_invoke, the INNER tool — tool_invoke
    // itself is a non-destructive dispatcher, and the inner call happens inside it
    // on this same thread, under the flags set below.
    const CatalogTool* gate = &cat->tools.constFind(internal).value();
    QJsonObject call_args = job.args;
    QJsonObject preview_args = job.args;
    QString label; // "→ inner: " prefix for the activity summary
    if (internal == QLatin1String(kToolInvoke)) {
        const QString inner = job.args.value(QStringLiteral("tool")).toString().trimmed();
        const QString server_id = job.args.value(QStringLiteral("server_id")).toString().trimmed();
        if (!inner.isEmpty()) { // empty → tool_invoke's own validation answers
            const QString aliased = resolve_tool_name(*cat, inner);
            const QString inner_name = aliased.isEmpty() ? inner : aliased;
            const auto target = McpService::instance().resolve_tool_target(inner_name, server_id);
            if (!target.error.isEmpty())
                return refused(target.error, QStringLiteral("→ %1: %2").arg(inner, clip_text(target.error, 60)), false);
            if (target.server_id != QLatin1String(INTERNAL_SERVER_ID))
                return refused(QStringLiteral("'%1' belongs to the external MCP server '%2' configured in Fincept "
                                              "Terminal. External agents can only reach Fincept Terminal's built-in "
                                              "tools — connect to that server directly instead.")
                                   .arg(inner, target.server_id),
                               QStringLiteral("→ %1: denied — external server tool").arg(inner), true);
            const auto it = cat->tools.constFind(inner_name);
            if (it == cat->tools.constEnd())
                return refused(QStringLiteral("Unknown tool: %1. Use tool_list to search the catalogue.").arg(inner),
                               QStringLiteral("→ %1: unknown tool").arg(inner), false);
            gate = &it.value();
            call_args.insert(QStringLiteral("tool"), gate->name); // tool_invoke resolves bare internal names
            preview_args = job.args.value(QStringLiteral("arguments")).toObject();
            label = QStringLiteral("→ %1: ").arg(gate->name);
        }
    }

    if (gate->destructive && !shared.allow_actions.load())
        return refused(actions_disabled_message(), label + QStringLiteral("denied — actions not allowed"), true);

    bool approved = false;
    if (needs_approval(*gate)) {
        if (!shared.accepting.load())
            return refused(QStringLiteral("Fincept Terminal's MCP server is stopping; the call was not run."),
                           label + QStringLiteral("denied — server stopping"), true);
        ToolApprovalRequest req;
        req.client = job.client;
        req.tool = gate->name;
        req.description = gate->description;
        req.args = preview_args;
        req.destructive = gate->destructive;
        req.live_trading = gate->category == QLatin1String(kLiveTradingCategory);
        switch (ToolApprovalDialog::request_approval(req)) {
            case ToolApprovalVerdict::Approved:
                approved = true;
                break;
            case ToolApprovalVerdict::Denied:
                return refused(QStringLiteral("Denied by the user in Fincept Terminal"),
                               label + QStringLiteral("denied by the user"), true);
            case ToolApprovalVerdict::TimedOut:
                return refused(
                    QStringLiteral("Approval timed out — ask the user to approve it in Fincept Terminal and retry"),
                    label + QStringLiteral("approval timed out"), true);
            case ToolApprovalVerdict::Unavailable:
                return refused(QStringLiteral("Fincept Terminal could not ask the user to approve this call (no "
                                              "terminal window is open), so it was not run. Ask the user to open "
                                              "Fincept Terminal and retry."),
                               label + QStringLiteral("denied — no terminal window to ask"), true);
        }
        if (!shared.accepting.load())
            return refused(QStringLiteral("Fincept Terminal's MCP server stopped; the call was not run."),
                           label + QStringLiteral("denied — server stopped"), true);
    }

    ToolResult tr;
    {
        // Agent-call context for McpProvider's gate (auth runs synchronously on
        // this thread): destructive capability only where the policy granted it.
        TerminalMcpBridge::ScopedCallFlags flags(/*call_in_progress=*/true,
                                                 /*destructive_allowed=*/approved || gate->destructive);
        const QString server_id = QString::fromLatin1(INTERNAL_SERVER_ID);
        if (approved) {
            ScopedApprovedCall approval_marker; // the user approved exactly this call
            tr = McpService::instance().execute_tool(server_id, internal, call_args, /*allow_defer=*/true);
        } else {
            tr = McpService::instance().execute_tool(server_id, internal, call_args, /*allow_defer=*/true);
        }
    }

    CallOutcome out;
    out.payload = shape_tool_result(internal, tr);
    out.success = tr.success;
    if (tr.success) {
        const QJsonObject data = tr.data.toObject();
        const QString job_id = data.value(QStringLiteral("job_id")).toString();
        const bool running = data.value(QStringLiteral("status")).toString() == QLatin1String("running");
        out.summary = label + (running && !job_id.isEmpty()
                                   ? QStringLiteral("started background job %1").arg(job_id)
                                   : (approved ? QStringLiteral("ok (approved by the user)") : QStringLiteral("ok")));
    } else {
        out.summary = label + QStringLiteral("error: ") + clip_text(tr.error, kSummaryMaxChars);
    }
    return out;
}

} // namespace mcp_http_detail

} // namespace fincept::mcp
