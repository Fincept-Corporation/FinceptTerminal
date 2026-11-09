// McpHttpServer_Tools.cpp — the tool side of the external MCP server: catalogue
// snapshot (what tools/list advertises), the per-call policy, execution on a
// pool thread, and result shaping. See McpHttpServer_p.h for the file map.
//
// The catalogue is the terminal's ten core tools (src/mcp/core/CoreTools.h) —
// small enough that every client gets all of it, so there is no toolset choice
// and no discovery round-trip.
//
// Policy for one tools/call:
//   unknown / disabled                      → JSON-RPC -32602
//   ui-control / navigation, look-only mode → isError, denied
//   python run / status / cancel, scripts off → isError, denied
//   auth level >= Verified                  → ToolApprovalDialog (≤ 50 s), then run
//                                             inside a ScopedApprovedCall
//   everything else                         → run; the core tools themselves ask
//                                             the user before risky ACTIONS
// Execution is McpService::execute_tool(INTERNAL_SERVER_ID, …) under the
// caller's name (ScopedCallOrigin), so approval dialogs say who is asking.

#include "core/logging/Logger.h"
#include "mcp/McpProvider.h"
#include "mcp/McpService.h"
#include "mcp/ResultStore.h"
#include "mcp/TerminalMcpBridge.h"
#include "mcp/core/CoreTools.h"
#include "mcp/server/CallApproval.h"
#include "mcp/server/McpHttpServer_p.h"
#include "mcp/server/ToolApprovalDialog.h"

#include <QCryptographicHash>
#include <QDateTime>
#include <QFuture>
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
    if (needs_approval(t))
        description += QStringLiteral(" [Every call needs the user's approval in Fincept Terminal.]");

    const bool read_only = t.category == QLatin1String(core::kCategoryUiRead);
    // destructiveHint is what clients use to decide whether a call may run
    // without asking their own user. ui_act / ui_invoke can delete or cancel
    // things, ui_open / ui_workspace close panels and delete layouts, and python
    // runs scripts — so every non-read tool is honestly "may destroy", whatever
    // its ToolDef flag says (the core tools gate risk per ACTION, not per tool).
    const QJsonObject annotations{
        {QStringLiteral("readOnlyHint"), read_only},
        {QStringLiteral("destructiveHint"), !read_only || t.destructive},
        {QStringLiteral("idempotentHint"), read_only},
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

    // Core tools first, in their catalogue order; anything else after, sorted.
    QStringList order;
    for (const QString& n : core::core_tool_names()) {
        if (cat->tools.contains(n))
            order << n;
    }
    QStringList rest;
    for (auto it = cat->tools.constBegin(); it != cat->tools.constEnd(); ++it) {
        if (!order.contains(it.key()))
            rest << it.key();
    }
    rest.sort();
    order += rest;
    for (const QString& n : order)
        cat->entries.append(tool_entry(cat->tools.value(n)));
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

/// MCP CallToolResult for a finished ToolResult. Oversized text is parked in
/// the ResultStore and replaced by a structure-preserving summary + a result id
/// readable with ui_read — never truncated silently. Image blocks (screenshots)
/// are forwarded as they are; they are not counted against the text budget.
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
        return error_call_result(text);
    }

    QJsonObject envelope;
    const bool has_data = !tr.data.isNull() && !tr.data.isUndefined();
    if (has_data)
        envelope.insert(QStringLiteral("data"), tr.data);
    if (!tr.message.isEmpty())
        envelope.insert(QStringLiteral("message"), tr.message);

    QJsonArray content;
    QJsonObject result;
    const QByteArray serialized = compact_json(envelope);
    if (serialized.size() > budget_bytes && allow_park) {
        const QString result_id = ResultStore::instance().put(tool, envelope);
        QJsonValue shaped(envelope);
        const ShrinkStats st = shrink_json(shaped, std::max(1024, budget_bytes - 800));
        QJsonObject out = shaped.isObject() ? shaped.toObject() : QJsonObject{{QStringLiteral("data"), shaped}};
        LOG_INFO(kTag, QStringLiteral("Tool '%1' returned %2 KB — shaped for the external client, full payload parked "
                                      "as %3")
                           .arg(tool)
                           .arg(st.original_bytes / 1024)
                           .arg(result_id));
        const QString note = QStringLiteral("[Result truncated to fit the response budget (%1 → %2 bytes). Read the "
                                            "full payload with ui_read(ref=\"%3\", offset=0).]")
                                 .arg(st.original_bytes)
                                 .arg(st.final_bytes)
                                 .arg(result_id);
        if (out.value(QStringLiteral("message")).isString())
            content.append(text_block(out.value(QStringLiteral("message")).toString()));
        if (out.contains(QStringLiteral("data")))
            content.append(text_block(json_text(out.value(QStringLiteral("data")))));
        content.append(text_block(note));
    } else {
        if (!tr.message.isEmpty())
            content.append(text_block(tr.message));
        if (has_data) {
            content.append(text_block(tr.data.isString() ? tr.data.toString() : json_text(tr.data)));
            if (tr.data.isObject())
                result.insert(QStringLiteral("structuredContent"), tr.data.toObject());
        }
    }
    for (const QJsonValue& block : tr.content) {
        if (block.isObject())
            content.append(block);
    }
    if (content.isEmpty())
        content.append(text_block(QStringLiteral("OK")));
    result.insert(QStringLiteral("content"), content);
    result.insert(QStringLiteral("isError"), false);
    return result;
}

CallOutcome refused(const QString& message, const QString& summary, bool denied) {
    CallOutcome out;
    out.payload = error_call_result(message);
    out.denied = denied;
    out.summary = summary;
    return out;
}

bool client_cancelled(const CancelFlag& flag) {
    return flag && flag->load();
}

/// Run an internal tool with the client's cancel flag wired into its
/// ToolContext: McpProvider's cancellation watch then ends the call as soon as
/// the client gives up, and tools that poll ctx.is_cancelled (ui_wait) stop.
/// Blocks this pool thread, never the GUI thread. The auth gate inside
/// call_tool_async runs synchronously here, so the caller's ScopedCallFlags /
/// ScopedApprovedCall still apply.
ToolResult call_internal_tool(const QString& name, const QJsonObject& args, const CancelFlag& cancel) {
    ToolContext ctx; // timeout left at the default → the tool's own budget
    if (cancel)
        ctx.is_cancelled = [cancel]() { return cancel->load(); };
    QFuture<ToolResult> future = McpProvider::instance().call_tool_async(name, args, ctx);
    future.waitForFinished();
    if (future.resultCount() == 0)
        return ToolResult::fail(QStringLiteral("Tool '%1' produced no result").arg(name));
    ToolResult tr = future.result();
    if (!tr.success && tr.error == QLatin1String("cancelled"))
        tr.error = QStringLiteral("Cancelled — the client gave up on this call.");
    return tr;
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
    return tool.auth >= AuthLevel::Verified;
}

bool is_ui_control(const CatalogTool& tool) {
    return tool.category == QLatin1String(core::kCategoryUiControl) ||
           tool.category == QLatin1String(core::kCategoryNavigation);
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
            -32602,
            QStringLiteral("Unknown tool: %1. tools/list shows the ten tools this server has.").arg(job.called_name));
        out.summary = QStringLiteral("unknown tool");
        return out;
    }
    const CatalogTool& gate = cat->tools.constFind(internal).value();

    // Queued behind other calls while the client already gave up (or hung up).
    if (client_cancelled(job.cancel))
        return refused(QStringLiteral("Cancelled by the client before it started; nothing was done."),
                       QStringLiteral("cancelled before start"), false);

    // ── The user's permissions for external agents ──
    if (is_ui_control(gate) && !shared.allow_ui_control.load())
        return refused(
            QStringLiteral("The user has set external agents to look-only in Fincept Terminal (Settings › AI "
                           "Agents (MCP)): %1 cannot open, arrange or operate screens. Use ui_overview, "
                           "ui_snapshot, ui_read and ui_screenshot, or ask the user to allow UI control.")
                .arg(internal),
            QStringLiteral("denied — look-only mode"), true);
    if (internal == QLatin1String("python") && !shared.allow_scripts.load()) {
        const QString action = job.args.value(QStringLiteral("action")).toString();
        if (action != QLatin1String("search") && action != QLatin1String("info"))
            return refused(
                QStringLiteral("The user has not allowed external agents to run the terminal's Python scripts "
                               "(Settings › AI Agents (MCP)). python(action=\"search\"/\"info\") still "
                               "works."),
                QStringLiteral("denied — scripts not allowed"), true);
    }
    if (gate.destructive && !shared.allow_ui_control.load())
        return refused(QStringLiteral("This tool changes terminal state and external agents are look-only."),
                       QStringLiteral("denied — look-only mode"), true);

    bool approved = false;
    if (needs_approval(gate)) {
        if (!shared.accepting.load())
            return refused(QStringLiteral("Fincept Terminal's MCP server is stopping; the call was not run."),
                           QStringLiteral("denied — server stopping"), true);
        ToolApprovalRequest req;
        req.client = job.client;
        req.tool = gate.name;
        req.description = gate.description;
        req.args = job.args;
        req.destructive = gate.destructive;
        switch (ToolApprovalDialog::request_approval(req)) {
            case ToolApprovalVerdict::Approved:
                approved = true;
                break;
            case ToolApprovalVerdict::Denied:
                return refused(QStringLiteral("Denied by the user in Fincept Terminal"),
                               QStringLiteral("denied by the user"), true);
            case ToolApprovalVerdict::TimedOut:
                return refused(
                    QStringLiteral("Approval timed out — ask the user to approve it in Fincept Terminal and retry"),
                    QStringLiteral("approval timed out"), true);
            case ToolApprovalVerdict::Unavailable:
                return refused(QStringLiteral("Fincept Terminal could not ask the user to approve this call (no "
                                              "terminal window is open), so it was not run."),
                               QStringLiteral("denied — no terminal window to ask"), true);
        }
        if (!shared.accepting.load())
            return refused(QStringLiteral("Fincept Terminal's MCP server stopped; the call was not run."),
                           QStringLiteral("denied — server stopped"), true);
    }

    ToolResult tr;
    {
        // Agent-call context for McpProvider's gate (auth runs synchronously on
        // this thread), and the caller's name for any approval a tool asks for.
        TerminalMcpBridge::ScopedCallFlags flags(/*call_in_progress=*/true,
                                                 /*destructive_allowed=*/approved || gate.destructive);
        ScopedCallOrigin origin(job.client);
        if (approved) {
            ScopedApprovedCall approval_marker; // the user approved exactly this call
            tr = call_internal_tool(internal, job.args, job.cancel);
        } else {
            tr = call_internal_tool(internal, job.args, job.cancel);
        }
    }

    // A stored result is read back page by page; re-parking its page would loop.
    const bool allow_park = !(internal == QLatin1String("ui_read") &&
                              job.args.value(QStringLiteral("ref")).toString().startsWith(QLatin1String("res_")));
    CallOutcome out;
    out.payload = shape_tool_result(internal, tr, shared.result_budget_bytes.load(), allow_park);
    out.success = tr.success;
    if (tr.success)
        out.summary = !approved                            ? QStringLiteral("ok")
                      : ToolApprovalDialog::auto_approve() ? QStringLiteral("ok (auto mode — not asked)")
                                                           : QStringLiteral("ok (approved by the user)");
    else
        out.summary = QStringLiteral("error: ") + clip_text(tr.error, kSummaryMaxChars);
    return out;
}

} // namespace mcp_http_detail

} // namespace fincept::mcp
