// JobTools.cpp — Meta-tools for long-running jobs (job_*) and oversized
// results (result_fetch).
//
// These are the model's half of the two protocols introduced alongside
// JobRegistry and ResultStore:
//
//   Long-running work — a tool with `supports_async` that overruns its grace
//   window hands back `{job_id, status:"running"}` instead of blocking. The
//   model polls `job_status` (with `wait_ms`, so one call can cover the whole
//   run) and collects with `job_result`.
//
//   Oversized results — a tool result too large to splice into the transcript
//   is shrunk to a preview and parked in the ResultStore. The envelope carries
//   a `result_id` the model pages through with `result_fetch` if — and only if
//   — it actually needs the detail.
//
// Both are registered in Tier-0 (see McpService::tier_0_tool_names) because a
// model that has been handed a receipt must be able to redeem it without first
// running a `tool_list` search to discover how.

#include "mcp/tools/JobTools.h"

#include "core/logging/Logger.h"
#include "mcp/JobRegistry.h"
#include "mcp/ResultStore.h"
#include "mcp/ToolSchemaBuilder.h"

#include <QJsonArray>
#include <QJsonObject>

namespace fincept::mcp::tools {

static constexpr const char* TAG = "JobTools";

std::vector<ToolDef> get_job_tools() {
    std::vector<ToolDef> tools;

    // ── job_status ───────────────────────────────────────────────────────
    //
    // `wait_ms` is the token-efficiency lever. Without it a model polls in a
    // tight loop and each poll costs a full tool round (request + tool message
    // + the whole re-sent transcript). With it, a 90 s job costs ~2 polls
    // instead of ~30.
    {
        ToolDef t;
        t.name = "job_status";
        t.description = "Check a background job started by a long-running tool. "
                        "Pass wait_ms to block until the job finishes instead of polling repeatedly — "
                        "this is much cheaper than calling repeatedly in a loop. "
                        "Returns {job_id, tool, status, elapsed_ms, progress?, message?}. "
                        "When status is 'succeeded' or 'failed', call job_result to collect the payload.";
        t.category = "meta";
        t.input_schema = ToolSchemaBuilder()
                             .string("job_id", "Job id returned by the long-running tool")
                             .required()
                             .length(1, 64)
                             .integer("wait_ms", "Block up to this many ms waiting for the job to finish "
                                                 "(0 = return immediately). Prefer a large value — one "
                                                 "long wait costs far fewer tokens than many short polls.")
                             .default_int(0)
                             .between(0, JobRegistry::max_wait_ms())
                             .build();
        // Blocking for wait_ms is the whole point, so the handler's own budget
        // must comfortably exceed the longest permitted wait.
        t.default_timeout_ms = JobRegistry::max_wait_ms() + 5000;
        t.handler = [](const QJsonObject& args) -> ToolResult {
            const QString id = args["job_id"].toString();
            const int wait_ms = args["wait_ms"].toInt(0);

            auto snap = wait_ms > 0 ? JobRegistry::instance().wait_for(id, wait_ms)
                                    : JobRegistry::instance().status(id);
            if (!snap)
                return ToolResult::fail("Unknown job id: " + id +
                                        " (it may have expired — jobs are kept for 15 minutes)");

            QJsonObject out = snap->to_json();
            if (snap->is_finished() && !snap->result_collected)
                out["next"] = QStringLiteral("call job_result(job_id='%1') to collect the payload").arg(id);
            return ToolResult::ok_data(out);
        };
        tools.push_back(std::move(t));
    }

    // ── job_result ───────────────────────────────────────────────────────
    {
        ToolDef t;
        t.name = "job_result";
        t.description = "Collect the payload of a finished background job. "
                        "Fails while the job is still running — use job_status(wait_ms=…) first.";
        t.category = "meta";
        t.input_schema =
            ToolSchemaBuilder().string("job_id", "Job id to collect").required().length(1, 64).build();
        t.handler = [](const QJsonObject& args) -> ToolResult {
            const QString id = args["job_id"].toString();
            auto snap = JobRegistry::instance().status(id);
            if (!snap)
                return ToolResult::fail("Unknown job id: " + id +
                                        " (it may have expired — jobs are kept for 15 minutes)");
            if (!snap->is_finished())
                return ToolResult::fail(QString("Job %1 is still running (%2 ms elapsed). "
                                                "Call job_status(job_id='%1', wait_ms=20000) to wait for it.")
                                            .arg(id)
                                            .arg(snap->elapsed_ms()));

            auto result = JobRegistry::instance().take_result(id);
            if (!result)
                return ToolResult::fail("Job " + id + " finished but its result is no longer available");
            // Returned verbatim — the LLM boundary applies the size cap and
            // overflow-store handoff uniformly for every tool result, so
            // doing it again here would double-truncate.
            return *result;
        };
        tools.push_back(std::move(t));
    }

    // ── job_cancel ───────────────────────────────────────────────────────
    {
        ToolDef t;
        t.name = "job_cancel";
        t.description = "Request cancellation of a running background job. The job keeps reporting "
                        "'running' until the underlying tool actually unwinds — poll job_status to confirm.";
        t.category = "meta";
        t.input_schema =
            ToolSchemaBuilder().string("job_id", "Job id to cancel").required().length(1, 64).build();
        t.handler = [](const QJsonObject& args) -> ToolResult {
            const QString id = args["job_id"].toString();
            if (!JobRegistry::instance().request_cancel(id))
                return ToolResult::fail("Job " + id + " is unknown or already finished");
            return ToolResult::ok("Cancellation requested for " + id,
                                  QJsonObject{{"job_id", id}, {"status", "cancelling"}});
        };
        tools.push_back(std::move(t));
    }

    // ── job_list ─────────────────────────────────────────────────────────
    {
        ToolDef t;
        t.name = "job_list";
        t.description = "List background jobs. Defaults to running jobs only — useful to check whether "
                        "work you started earlier is still in flight before starting it again.";
        t.category = "meta";
        t.input_schema = ToolSchemaBuilder()
                             .boolean("include_finished", "Also list jobs that have already finished")
                             .default_bool(false)
                             .build();
        t.handler = [](const QJsonObject& args) -> ToolResult {
            const bool include_finished = args["include_finished"].toBool(false);
            QJsonArray rows;
            for (const auto& s : JobRegistry::instance().list(include_finished))
                rows.append(s.to_json());
            return ToolResult::ok_data(QJsonObject{{"count", rows.size()}, {"jobs", rows}});
        };
        tools.push_back(std::move(t));
    }

    // ── result_fetch ─────────────────────────────────────────────────────
    //
    // Deliberately byte-paged rather than "give me the whole thing": the point
    // of the overflow store is that the model pays for detail incrementally.
    {
        ToolDef t;
        t.name = "result_fetch";
        t.description = "Page through the full payload of a tool result that was too large to include "
                        "inline. Use the result_id from a truncated result envelope. "
                        "Returns a slice of the raw JSON text plus has_more — request only what you need, "
                        "each page costs tokens.";
        t.category = "meta";
        t.input_schema = ToolSchemaBuilder()
                             .string("result_id", "result_id from the truncated result envelope")
                             .required()
                             .length(1, 64)
                             .integer("offset", "Byte offset to start from")
                             .default_int(0)
                             .between(0, 100000000)
                             .integer("limit", "Bytes to return (max 20000)")
                             .default_int(6000)
                             .between(1, 20000)
                             .build();
        t.handler = [](const QJsonObject& args) -> ToolResult {
            const QString id = args["result_id"].toString();
            const auto page = ResultStore::instance().page(id, args["offset"].toInt(0), args["limit"].toInt(6000));
            if (!page.found)
                return ToolResult::fail("Unknown result_id: " + id +
                                        " (stored payloads expire after 15 minutes)");
            return ToolResult::ok_data(QJsonObject{
                {"result_id", id},
                {"tool", page.tool},
                {"offset", page.offset},
                {"returned", page.returned},
                {"total_bytes", page.total_bytes},
                {"has_more", page.has_more},
                {"next_offset", page.offset + page.returned},
                {"text", page.text},
            });
        };
        tools.push_back(std::move(t));
    }

    LOG_INFO(TAG, QString("Defined %1 job/result meta tools").arg(tools.size()));
    return tools;
}

} // namespace fincept::mcp::tools
