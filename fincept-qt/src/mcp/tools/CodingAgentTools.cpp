// CodingAgentTools.cpp — Agent Workbench MCP tools.
//
//   coding_agent_status     detected CLIs + versions, MCP server state, run counts
//   coding_agent_run_start  queue a Claude Code / Codex run (destructive: spends
//                           the user's credits, can write files) → run_id at once
//   coding_agent_run_get    status, final answer and the last N transcript events
//   coding_agent_runs_list  recent runs, newest first
//
// Handlers run on an MCP worker thread; CodingAgentService is a GUI-thread
// QObject, so every handler hops to the GUI thread through
// AsyncDispatch::callback_to_promise(qApp, …) and never blocks the worker.
// Payloads are shaped at the source (limit / max_*_chars params) and every
// clip is marked in place (§M3/§M4).

#include "mcp/tools/CodingAgentTools.h"

#include "mcp/AsyncDispatch.h"
#include "mcp/ToolSchemaBuilder.h"
#include "mcp/server/McpHttpServer.h"
#include "services/coding_agents/CodingAgentService.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QJsonArray>
#include <QJsonObject>
#include <QObject>
#include <QTimer>

#include <algorithm>

namespace fincept::mcp::tools {

namespace coding_agent_tools_detail {

namespace ca = fincept::services::coding_agents;

QString iso_time(qint64 ms) {
    return ms > 0 ? QDateTime::fromMSecsSinceEpoch(ms).toString(Qt::ISODate) : QString();
}

/// Clip to `max_chars`, marking the cut so a model never mistakes it for the whole text (§M4).
QString clip_marked(const QString& s, int max_chars) {
    if (s.size() <= max_chars)
        return s;
    return s.left(max_chars) + QStringLiteral("\n… [truncated: %1 more characters]").arg(s.size() - max_chars);
}

QJsonObject cli_json(const ca::CliInfo& c) {
    QJsonObject o{{"installed", c.installed()},
                  {"path", QDir::toNativeSeparators(c.path)},
                  {"version", c.version},
                  {"install_guide", ca::CodingAgentService::install_guide_url(c.kind)}};
    if (c.via_cmd_shim)
        o.insert("via_npm_cmd_shim", true);
    if (c.probing)
        o.insert("version_probe", "running");
    if (!c.probe_error.isEmpty())
        o.insert("probe_error", c.probe_error);
    return o;
}

QJsonObject run_summary(const ca::CodingAgentRun& r, int queue_position) {
    QJsonObject o{{"run_id", r.id},
                  {"title", r.title},
                  {"agent", ca::agent_id(r.agent)},
                  {"status", ca::status_id(r.status)},
                  {"mode", ca::mode_id(r.mode)},
                  {"use_fincept_tools", r.use_fincept_tools},
                  {"workdir", QDir::toNativeSeparators(r.workdir)},
                  {"created_at", iso_time(r.created_at_ms)},
                  {"origin", r.origin}};
    if (!r.model.isEmpty())
        o.insert("model", r.model);
    if (r.started_at_ms > 0)
        o.insert("started_at", iso_time(r.started_at_ms));
    if (r.finished_at_ms > 0) {
        o.insert("finished_at", iso_time(r.finished_at_ms));
        if (r.started_at_ms > 0)
            o.insert("duration_s", static_cast<double>(r.finished_at_ms - r.started_at_ms) / 1000.0);
    }
    if (r.cost_usd.has_value())
        o.insert("cost_usd", *r.cost_usd);
    if (r.input_tokens > 0 || r.output_tokens > 0) {
        o.insert("input_tokens", static_cast<double>(r.input_tokens));
        o.insert("output_tokens", static_cast<double>(r.output_tokens));
    }
    if (!r.session_id.isEmpty())
        o.insert("session_id", r.session_id);
    if (!r.error.isEmpty())
        o.insert("error", clip_marked(r.error, 2000));
    if (!r.parent_run_id.isEmpty())
        o.insert("parent_run_id", r.parent_run_id);
    if (!r.schedule_id.isEmpty())
        o.insert("schedule_id", r.schedule_id);
    if (queue_position > 0)
        o.insert("queue_position", queue_position);
    return o;
}

ToolResult status_result() {
    auto& svc = ca::CodingAgentService::instance();
    auto& srv = mcp::McpHttpServer::instance();
    QJsonObject data{
        {"claude_code", cli_json(svc.cli_info(ca::AgentKind::Claude))},
        {"codex", cli_json(svc.cli_info(ca::AgentKind::Codex))},
        {"mcp_server", QJsonObject{{"running", srv.is_running()}, {"url", srv.url()}}},
        {"runs", QJsonObject{{"running", svc.running_count()},
                             {"queued", svc.queued_count()},
                             {"max_concurrent", svc.max_concurrent()}}},
        {"workspace_root", QDir::toNativeSeparators(ca::CodingAgentService::workspace_root())}};
    return ToolResult::ok_data(data);
}

bool any_probing(const ca::CodingAgentService& svc) {
    return svc.cli_info(ca::AgentKind::Claude).probing || svc.cli_info(ca::AgentKind::Codex).probing;
}

} // namespace coding_agent_tools_detail

using namespace coding_agent_tools_detail;

std::vector<ToolDef> get_coding_agent_tools() {
    std::vector<ToolDef> tools;

    // ── coding_agent_status ─────────────────────────────────────────────────
    {
        ToolDef t;
        t.name = "coding_agent_status";
        t.description = "Agent Workbench status: whether the Claude Code and OpenAI Codex CLIs are installed on this "
                        "computer (path, version), whether the terminal's MCP server is running, and how many agent "
                        "runs are running or queued.";
        t.category = "coding-agents";
        t.default_timeout_ms = 25000;
        t.async_handler = [](const QJsonObject&, ToolContext ctx, std::shared_ptr<QPromise<ToolResult>> promise) {
            AsyncDispatch::callback_to_promise(QCoreApplication::instance(), std::move(ctx), promise, [](auto resolve) {
                auto& svc = ca::CodingAgentService::instance();
                svc.initialize_deferred();
                const bool probed = svc.cli_info(ca::AgentKind::Claude).probed && svc.cli_info(ca::AgentKind::Codex).probed;
                if (!probed && !any_probing(svc))
                    svc.refresh_cli_detection();
                if (!any_probing(svc)) {
                    resolve(status_result());
                    return;
                }
                // Wait for the `--version` probes (async QProcess, ≤ 20 s each).
                auto* holder = new QObject(&svc);
                QObject::connect(&svc, &ca::CodingAgentService::cli_status_changed, holder, [resolve, holder]() {
                    if (any_probing(ca::CodingAgentService::instance()))
                        return;
                    resolve(status_result());
                    holder->deleteLater();
                });
                QTimer::singleShot(22000, holder, [resolve, holder]() {
                    resolve(status_result()); // report what is known; no-op if already resolved
                    holder->deleteLater();
                });
            });
        };
        tools.push_back(std::move(t));
    }

    // ── coding_agent_run_start ──────────────────────────────────────────────
    {
        ToolDef t;
        t.name = "coding_agent_run_start";
        t.description =
            "Start a Claude Code or OpenAI Codex agent run in the background on this computer, with live access to "
            "the terminal's data tools over MCP. Returns a run_id immediately — runs take minutes; follow them with "
            "coding_agent_run_get. Spends the user's Claude/OpenAI credits, and in workspace_edits mode the agent can "
            "write files in its working folder. Only use when the user asked for an agent run; never for trading.";
        t.category = "coding-agents";
        t.is_destructive = true; // spends money and can write files
        t.default_timeout_ms = 15000;
        t.input_schema =
            ToolSchemaBuilder()
                .string("agent", "Which CLI runs the task")
                .required()
                .enums({"claude", "codex"})
                .string("prompt", "The task for the agent, in plain language")
                .required()
                .length(1, ca::kAgentPromptMaxChars)
                .string("mode", "analysis = read-only (answer saved as RESULT.md); workspace_edits = may write files "
                                "in the run's working folder")
                .default_str("analysis")
                .enums({"analysis", "workspace_edits"})
                .string("model", "Model override, e.g. sonnet / opus (Claude) or a Codex model; omit for the CLI default")
                .boolean("use_fincept_tools", "Give the agent the terminal's live data tools over MCP")
                .default_bool(true)
                .number("max_budget_usd", "Claude only: stop the run once it has spent this many USD (0 = no cap)")
                .between(0, 1000)
                .string("title", "Short title for the run history (default: first line of the prompt)")
                .build();
        t.async_handler = [](const QJsonObject& args, ToolContext ctx, std::shared_ptr<QPromise<ToolResult>> promise) {
            const auto agent = ca::agent_from_id(args.value("agent").toString());
            if (!agent.has_value()) {
                // Resolve through the provider's single-winner guard (§M2b), never the promise directly.
                AsyncDispatch::callback_to_promise(nullptr, std::move(ctx), promise, [](auto resolve) {
                    resolve(ToolResult::fail("agent must be 'claude' or 'codex'"));
                });
                return;
            }
            ca::RunRequest req;
            req.agent = *agent;
            req.prompt = args.value("prompt").toString();
            req.mode = ca::mode_from_id(args.value("mode").toString("analysis"));
            req.model = args.value("model").toString();
            req.use_fincept_tools = args.value("use_fincept_tools").toBool(true);
            req.max_budget_usd = std::clamp(args.value("max_budget_usd").toDouble(0.0), 0.0, 1000.0);
            req.title = args.value("title").toString();
            req.origin = QStringLiteral("mcp");
            AsyncDispatch::callback_to_promise(QCoreApplication::instance(), std::move(ctx), promise, [req](auto resolve) {
                auto& svc = ca::CodingAgentService::instance();
                QString err;
                const QString run_id = svc.start_run(req, &err);
                if (run_id.isEmpty()) {
                    resolve(ToolResult::fail(err));
                    return;
                }
                const ca::CodingAgentRun* run = svc.find_run(run_id);
                QJsonObject data = run ? run_summary(*run, svc.queue_position(run_id)) : QJsonObject{{"run_id", run_id}};
                data.insert("next_step", "Poll coding_agent_run_get with this run_id (runs usually take 1-15 minutes); "
                                         "the user can also follow it live in the Agent Workbench screen.");
                resolve(ToolResult::ok(QStringLiteral("Agent run %1 started.").arg(run_id), data));
            });
        };
        tools.push_back(std::move(t));
    }

    // ── coding_agent_run_get ────────────────────────────────────────────────
    {
        ToolDef t;
        t.name = "coding_agent_run_get";
        t.description = "Status of an Agent Workbench run: summary (status, cost, tokens, working folder), the final "
                        "answer once finished, and the last N transcript events (tool calls, results, errors).";
        t.category = "coding-agents";
        t.default_timeout_ms = 15000;
        t.input_schema = ToolSchemaBuilder()
                             .string("run_id", "Run id returned by coding_agent_run_start or coding_agent_runs_list")
                             .required()
                             .integer("events", "How many of the most recent transcript events to include")
                             .default_int(20)
                             .between(0, 200)
                             .integer("max_event_chars", "Per-event body cap (characters)")
                             .default_int(1500)
                             .between(200, 20000)
                             .integer("max_result_chars", "Cap on the final answer (characters)")
                             .default_int(12000)
                             .between(1000, 100000)
                             .build();
        t.async_handler = [](const QJsonObject& args, ToolContext ctx, std::shared_ptr<QPromise<ToolResult>> promise) {
            const QString run_id = args.value("run_id").toString().trimmed();
            const int n_events = std::clamp(args.value("events").toInt(20), 0, 200);
            const int max_event_chars = std::clamp(args.value("max_event_chars").toInt(1500), 200, 20000);
            const int max_result_chars = std::clamp(args.value("max_result_chars").toInt(12000), 1000, 100000);
            AsyncDispatch::callback_to_promise(
                QCoreApplication::instance(), std::move(ctx), promise,
                [run_id, n_events, max_event_chars, max_result_chars](auto resolve) {
                    auto& svc = ca::CodingAgentService::instance();
                    svc.initialize_deferred();
                    if (!svc.find_run(run_id)) {
                        resolve(ToolResult::fail(QStringLiteral("No run with id '%1' (it may have been deleted, or "
                                                                "the run history is still loading).")
                                                     .arg(run_id)));
                        return;
                    }
                    auto* holder = new QObject(&svc);
                    svc.fetch_events(run_id, holder,
                                     [resolve, holder, run_id, n_events, max_event_chars,
                                      max_result_chars](const QVector<ca::AgentEvent>& events) {
                                         holder->deleteLater();
                                         auto& s = ca::CodingAgentService::instance();
                                         const ca::CodingAgentRun* run = s.find_run(run_id);
                                         if (!run) {
                                             resolve(ToolResult::fail(QStringLiteral("Run '%1' was deleted.").arg(run_id)));
                                             return;
                                         }
                                         QJsonObject data{{"run", run_summary(*run, s.queue_position(run_id))}};
                                         if (!run->final_text.isEmpty()) {
                                             data.insert("final_text", clip_marked(run->final_text, max_result_chars));
                                             if (run->final_text.size() > max_result_chars)
                                                 data.insert("final_text_note",
                                                             QStringLiteral("Full answer: RESULT.md in %1")
                                                                 .arg(QDir::toNativeSeparators(run->workdir)));
                                         }
                                         QJsonArray arr;
                                         const qsizetype from = qMax<qsizetype>(0, events.size() - n_events);
                                         for (qsizetype i = from; i < events.size(); ++i) {
                                             const ca::AgentEvent& e = events[i];
                                             QJsonObject eo{{"seq", e.seq},
                                                            {"at", iso_time(e.ts_ms)},
                                                            {"kind", ca::event_kind_id(e.kind)},
                                                            {"title", e.title}};
                                             if (!e.body.isEmpty())
                                                 eo.insert("body", clip_marked(e.body, max_event_chars));
                                             if (e.is_error)
                                                 eo.insert("is_error", true);
                                             arr.append(eo);
                                         }
                                         data.insert("events", arr);
                                         data.insert("events_total", static_cast<int>(events.size()));
                                         data.insert("events_returned", static_cast<int>(arr.size()));
                                         if (arr.size() < events.size())
                                             data.insert("events_note",
                                                         QStringLiteral("Showing the last %1 of %2 events — raise "
                                                                        "`events` (max 200) for more.")
                                                             .arg(arr.size())
                                                             .arg(events.size()));
                                         resolve(ToolResult::ok_data(data));
                                     });
                });
        };
        tools.push_back(std::move(t));
    }

    // ── coding_agent_runs_list ──────────────────────────────────────────────
    {
        ToolDef t;
        t.name = "coding_agent_runs_list";
        t.description = "List recent Agent Workbench runs (Claude Code / Codex), newest first, with status, cost and "
                        "timing. Filter by status or agent.";
        t.category = "coding-agents";
        t.default_timeout_ms = 10000;
        t.input_schema = ToolSchemaBuilder()
                             .integer("limit", "Maximum runs to return")
                             .default_int(20)
                             .between(1, 100)
                             .string("status", "Only runs in this state ('active' = queued or running)")
                             .enums({"active", "queued", "running", "succeeded", "failed", "cancelled", "interrupted"})
                             .string("agent", "Only runs of this CLI")
                             .enums({"claude", "codex"})
                             .build();
        t.async_handler = [](const QJsonObject& args, ToolContext ctx, std::shared_ptr<QPromise<ToolResult>> promise) {
            const int limit = std::clamp(args.value("limit").toInt(20), 1, 100);
            const QString status = args.value("status").toString();
            const QString agent = args.value("agent").toString();
            AsyncDispatch::callback_to_promise(QCoreApplication::instance(), std::move(ctx), promise,
                                               [limit, status, agent](auto resolve) {
                                                   auto& svc = ca::CodingAgentService::instance();
                                                   svc.initialize_deferred();
                                                   QJsonArray arr;
                                                   int matched = 0;
                                                   for (const auto& id : svc.run_ids()) {
                                                       const ca::CodingAgentRun* r = svc.find_run(id);
                                                       if (!r)
                                                           continue;
                                                       if (!agent.isEmpty() && ca::agent_id(r->agent) != agent)
                                                           continue;
                                                       if (status == QLatin1String("active")) {
                                                           if (ca::is_terminal_status(r->status))
                                                               continue;
                                                       } else if (!status.isEmpty() && ca::status_id(r->status) != status) {
                                                           continue;
                                                       }
                                                       ++matched;
                                                       if (arr.size() < limit)
                                                           arr.append(run_summary(*r, svc.queue_position(id)));
                                                   }
                                                   QString msg;
                                                   if (!svc.is_loaded())
                                                       msg = QStringLiteral("Run history is still loading — older runs "
                                                                            "may be missing; retry in a moment.");
                                                   else if (matched > arr.size())
                                                       msg = QStringLiteral("Showing %1 of %2 matching runs — raise "
                                                                            "`limit` (max 100) or filter.")
                                                                 .arg(arr.size())
                                                                 .arg(matched);
                                                   resolve(msg.isEmpty() ? ToolResult::ok_data(arr)
                                                                         : ToolResult::ok(msg, arr));
                                               });
        };
        tools.push_back(std::move(t));
    }

    return tools;
}

} // namespace fincept::mcp::tools
