// LlmFinceptAsync.cpp — Fincept-specific async LLM path.
//
// POST /research/llm/async to submit, then poll /research/llm/status/{id}. The
// submit endpoint takes a FLAT prompt (not an OpenAI messages array), so system
// prompt + tool catalog + history are synthesised into a single string here.
//
// ── Why this is a loop ────────────────────────────────────────────────────
// Tool calling on this path is MULTI-ROUND. It used to be single-shot: execute
// whatever tool the first reply named, then ask for a summary with tool calls
// forbidden. With Tool RAG on (the default) the model only ever sees the Tier-0
// tools, so round one is ALWAYS a discovery call — tool_list. The turn then
// ended there and the user got the raw tool_list JSON presented as the answer.
// A discovery chain needs three rounds (tool_list → tool_describe → the real
// call) before there is anything worth summarising, so the loop is the fix.
//
// Each round appends the assistant turn plus its tool results to the prompt
// transcript and re-submits. Follow-ups stay on /research/llm/async: the old
// code hopped to the sync /research/chat endpoint, which is a different surface
// with its own model default (an OpenRouter slug), so the Fincept model id we
// send was not valid there — that request failing is what left the raw JSON in
// the chat bubble.
//
// ── Why `tools` is NOT sent ───────────────────────────────────────────────
// The published AsyncLLMRequest model carries `tools`/`tool_choice` fields, so
// sending a standard OpenAI tool array looks right. It isn't. The backend
// rejects it upstream:
//
//   Fincept async task failed: LLM service error:
//   {"type":"error","error":{"type":"invalid_request_error",
//    "message":"invalid params, function name is empty (2013)"}}
//
// That `(2013)` + request_id envelope is MiniMax's, and the array we send is
// well-formed OpenAI ({"type":"function","function":{"name",...}}) — the exact
// shape that works when talking to the minimax provider directly. So the
// backend re-wraps or flattens `tools` on the way through and loses
// function.name. The endpoint's own documented client contract is
// `{ prompt, temperature, max_tokens }`, its `tools` field has no item schema
// published, and its documented response is a plain `data.response` string with
// no tool_calls — i.e. the field is not a supported surface here.
//
// Tools on this path therefore travel ONE way: injected into the prompt as a
// text catalog, invoked back as <tool_call> markup, executed here, and spliced
// into the transcript for the next round. Do not re-add `tools` to the submit
// body without first probing the endpoint for an item shape it actually
// accepts — it breaks every Fincept chat, not just tool-using ones.
//
// Response-side structured tool_calls are still parsed if they ever appear;
// that costs nothing and is how we'd notice the backend gaining support.

#include "core/config/AppConfig.h"
#include "core/logging/Logger.h"
#include "mcp/McpProvider.h"
#include "mcp/McpService.h"
#include "mcp/ResultStore.h"
#include "services/llm/LlmContentExtractors.h"
#include "services/llm/LlmRequestPolicy.h"
#include "services/llm/LlmService.h"

#include <QByteArray>
#include <QElapsedTimer>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QList>
#include <QJsonObject>
#include <QJsonValue>
#include <QRegularExpression>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QThread>
#include <QVariant>

#include <algorithm>
#include <vector>

namespace fincept::ai_chat {

namespace {
constexpr const char* kLlmFinceptTag = "LlmService";

// Runaway backstop, NOT a budget.
//
// There is deliberately no round budget on this path. Tool RAG costs ~3 rounds
// per action before any real work happens, so every ceiling tried here (10, then
// 20) was hit during discovery — the model spent the turn learning what tools
// exist and ran out before writing anything, then apologised. Work is bounded by
// whether the model is still making progress (see kMaxNoProgressRounds), not by
// a count. This number only exists so a pathological loop can't run forever; it
// matches the [1,200] clamp on max_tool_rounds used by the structured providers.
constexpr int kFinceptHardStop = 200;

// Consecutive rounds in which every tool call was a verbatim repeat already
// served from cache — i.e. the model learned nothing new. That, not elapsed
// rounds, is the real signal that a turn is stuck.
constexpr int kMaxNoProgressRounds = 3;

// HARD server limit on the whole `prompt` field:
//
//   HTTP 422: body -> prompt: Value error, Prompt too long (max 50000 characters)
//
// The whole prompt is re-sent every round, so an unbounded turn otherwise grows
// an unbounded request — a 10-round turn already reached ~34 KB. Everything the
// composer emits (system prompt + tool catalog + chat history + round
// transcript + trailing instruction) has to fit under this together, so the
// round budget is derived from what the fixed parts leave over, never fixed
// independently. A previous 120000 constant here was above the server cap and
// 422'd on long turns.
constexpr int kServerMaxPromptChars = 50000;

// Headroom for the trailing instruction and the elision notices, and so a
// rounding slip can't land exactly on the cap.
constexpr int kPromptSafetyMargin = 2000;

// Wall-clock budget for polling a single submitted task.
constexpr qint64 kPollBudgetMs = 120000;

// Poll fast at first — most tasks finish in a few seconds and a flat 3 s floor
// added a guaranteed 3 s to every single round of the loop.
constexpr int kFastPollMs = 1000;
constexpr int kFastPollCount = 6;
constexpr int kSlowPollMs = 3000;

// Tool results are spliced into a prompt string that is re-sent whole on every
// later round, so an unshaped result is re-billed for the rest of the turn.
// Mirrors the structured path's transcript discipline (see fit_llm_payload).
constexpr int kToolResultBudgetBytes = 4000;
} // namespace

// Build a compact tool catalog string for injection into the system prompt.
// This allows models that don't support structured tool_calls to still emit
// text-based tool invocations that extract_text_tool_calls can detect.
//
// Two modes:
//   • Tool RAG ON + default filter: emit only the Tier-0 tools + explicit
//     instructions to use tool_list for everything else. Same disclosure
//     model as structured providers — keeps the catalog small and forces
//     deliberate discovery.
//   • Tool RAG OFF or explicit filter: legacy behaviour — list up to 60
//     filtered tools inline.
static QString build_tool_catalog_for_prompt(const mcp::ToolFilter& filter) {
    auto all_tools = mcp::McpService::instance().get_all_tools(filter);
    if (all_tools.empty())
        return {};

    const bool default_filter = filter.categories.isEmpty() && filter.exclude_categories.isEmpty() &&
                                filter.name_patterns.isEmpty() && filter.exclude_name_patterns.isEmpty() &&
                                filter.max_tools == 0;
    const bool use_rag =
        default_filter && fincept::AppConfig::instance().get("mcp/use_tool_rag", QVariant(true)).toBool();

    QString catalog;
    catalog += "You have access to tools. To use one, emit a <tool_call> block:\n";
    catalog += "<tool_call>{\"name\": \"TOOL_NAME\", \"arguments\": {\"param\": \"value\"}}</tool_call>\n";
    catalog += "Emit the block ALONE, with no commentary around it — you will be called again with the "
               "result and can then continue. Only call a tool when the request needs live data or an "
               "action in the terminal; answer greetings and general questions directly in plain text.\n\n";

    if (use_rag) {
        // Mirror McpService::tier_0_tool_names() (kept in sync manually — small
        // list, low churn).
        static const QSet<QString> kTier0 = {
            "tool_list", "tool_describe", "navigate_to_tab", "list_tabs", "get_current_tab", "get_auth_status",
        };
        catalog += "Always-available tools:\n";
        for (const auto& tool : all_tools) {
            if (!kTier0.contains(tool.name))
                continue;
            QString fn_name = tool.server_id + "__" + mcp::McpProvider::encode_tool_name_for_wire(tool.name);
            catalog += "- " + fn_name + ": " + tool.description + "\n";
        }
        // The three-step loop is spelled out with a worked example on purpose.
        // The previous wording ended with "then invoke the tool", which never
        // said HOW — and a model that has just discovered a tool has no reason
        // to assume the mechanism is the same <tool_call> block it used for
        // discovery. One observed turn burned 17 rounds searching for an
        // execute/invoke/run tool (tool_list "call execute ma_sec_dcf_inputs
        // ticker AAPL") while never once calling ma_sec_dcf_inputs itself.
        catalog += QStringLiteral(
            "\nThe two tools above are the only ones listed here, but they are NOT the only ones you can call.\n"
            "You can call ANY tool name that tool_list returns, immediately, using the same <tool_call> block.\n"
            "There is no separate execute/invoke/run step and no registration step — emitting the block IS "
            "calling the tool.\n"
            "\n"
            "Worked example — the full three steps:\n"
            "  1. <tool_call>{\"name\": \"tool_list\", \"arguments\": {\"query\": \"SEC DCF inputs for a "
            "company\"}}</tool_call>\n"
            "     -> returns e.g. a tool named \"ma_sec_dcf_inputs\"\n"
            "  2. <tool_call>{\"name\": \"tool_describe\", \"arguments\": {\"name\": "
            "\"ma_sec_dcf_inputs\"}}</tool_call>\n"
            "     -> returns its input schema, e.g. {\"ticker\": \"string\"}\n"
            "  3. <tool_call>{\"name\": \"ma_sec_dcf_inputs\", \"arguments\": {\"ticker\": \"AAPL\"}}</tool_call>\n"
            "     -> THIS is the call that returns the data. Step 3 is the point of steps 1 and 2.\n"
            "\n"
            "Do not call tool_list again looking for a way to run a tool you have already found — you already "
            "have it; go straight to step 3. The server prefix (fincept-terminal__) is optional; a bare tool "
            "name works. For requests with several intents, run the loop once per intent.\n");
        return catalog;
    }

    // ── Legacy mode ──
    catalog += "Available tools:\n";
    int count = 0;
    for (const auto& tool : all_tools) {
        QString fn_name = tool.server_id + "__" + mcp::McpProvider::encode_tool_name_for_wire(tool.name);
        catalog += "- " + fn_name + ": " + tool.description;
        QJsonObject props = tool.input_schema["properties"].toObject();
        if (!props.isEmpty()) {
            QStringList params;
            for (auto it = props.constBegin(); it != props.constEnd(); ++it)
                params.append(it.key());
            catalog += " (params: " + params.join(", ") + ")";
        }
        catalog += "\n";
        ++count;
        if (count >= 60) {
            catalog += "... and " + QString::number(all_tools.size() - 60) + " more tools available.\n";
            break;
        }
    }
    return catalog;
}

// Dig a human-readable failure reason out of a status payload.
//
// The old code checked exactly two spots — `error` at the top level and under
// `data` — and fell back to the literal string "unknown error" while logging
// nothing. A backend that reports its reason anywhere else (nested one level
// deeper, or under `message`/`detail`, or as an object rather than a string)
// was therefore indistinguishable from a backend that said nothing at all,
// which makes "is this us or the server?" unanswerable from the logs.
static QString fincept_extract_error(const QJsonObject& root) {
    static const char* kKeys[] = {"error", "message", "detail", "reason", "error_message"};

    // Breadth-first over the payload and its nested `data` objects.
    QList<QJsonObject> scopes{root};
    if (root["data"].isObject()) {
        const QJsonObject d = root["data"].toObject();
        scopes.append(d);
        if (d["data"].isObject())
            scopes.append(d["data"].toObject());
    }

    for (const QJsonObject& scope : scopes) {
        for (const char* key : kKeys) {
            const QJsonValue v = scope.value(QLatin1String(key));
            if (v.isString() && !v.toString().trimmed().isEmpty())
                return v.toString().trimmed();
            // Some gateways nest {"error": {"message": "..."}}.
            if (v.isObject()) {
                const QJsonObject o = v.toObject();
                for (const char* inner : kKeys) {
                    const QJsonValue iv = o.value(QLatin1String(inner));
                    if (iv.isString() && !iv.toString().trimmed().isEmpty())
                        return iv.toString().trimmed();
                }
                return QString::fromUtf8(QJsonDocument(o).toJson(QJsonDocument::Compact));
            }
        }
    }
    return {};
}

// Render one tool result for splicing back into the prompt transcript.
static QString fincept_render_tool_result(const mcp::ToolResult& tr) {
    // Failures are marked unmissably. A model that skims a JSON envelope for
    // "success":false will happily report the action as done — one observed turn
    // had set_excel_sheet_data denied by an auth gate and still told the user
    // "the sheet was created successfully with all the DCF model data", reading
    // the following recalculate_excel_sheet -> OK as confirmation.
    if (!tr.success) {
        const QString why = !tr.error.isEmpty() ? tr.error : (!tr.message.isEmpty() ? tr.message : QStringLiteral("unknown error"));
        return QStringLiteral("FAILED - this action did NOT happen. Reason: ") + why;
    }
    if (!tr.message.isEmpty())
        return tr.message;
    QJsonValue payload = (!tr.data.isNull() && !tr.data.isUndefined()) ? tr.data : QJsonValue(tr.to_json());
    mcp::shrink_json(payload, kToolResultBudgetBytes);
    if (payload.isObject())
        return QString::fromUtf8(QJsonDocument(payload.toObject()).toJson(QJsonDocument::Compact));
    if (payload.isArray())
        return QString::fromUtf8(QJsonDocument(payload.toArray()).toJson(QJsonDocument::Compact));
    return payload.toVariant().toString();
}

LlmResponse LlmService::fincept_submit_poll(const QString& prompt, QJsonArray* out_tool_calls) {
    LlmResponse resp;
    if (out_tool_calls)
        *out_tool_calls = {};

    // {prompt, max_tokens} only — the endpoint's documented contract. No
    // `tools` (see the header note), no temperature (backend default).
    QJsonObject submit_body;
    submit_body["prompt"] = prompt;
    submit_body["max_tokens"] = resolved_max_tokens();

    auto hdr = get_headers();
    const QString fincept_base = fincept::AppConfig::instance().api_base_url();
    const QString async_url = fincept_base + "/research/llm/async";
    const QString status_base = fincept_base + "/research/llm/status/";

    LOG_INFO(kLlmFinceptTag, QString("Fincept async: submitting to %1 (api_key=%2, prompt_len=%3)")
                                 .arg(async_url)
                                 .arg(api_key_.isEmpty() ? QStringLiteral("EMPTY")
                                                         : QStringLiteral("len=%1").arg(api_key_.size()))
                                 .arg(prompt.length()));

    QByteArray json_data = QJsonDocument(submit_body).toJson(QJsonDocument::Compact);
    auto submit = eventloop_request("POST", async_url, json_data, hdr, 30000);
    if (!submit.success) {
        resp.error = "Fincept async submit failed: " + submit.error;
        LOG_ERROR(kLlmFinceptTag, resp.error);
        return resp;
    }

    auto submit_doc = QJsonDocument::fromJson(submit.body);
    if (submit_doc.isNull()) {
        resp.error = "Fincept async: failed to parse submit response";
        LOG_ERROR(kLlmFinceptTag, resp.error);
        return resp;
    }

    // Response can nest task_id at top level or inside data
    QJsonObject sj = submit_doc.object();
    QString task_id = sj["task_id"].toString();
    if (task_id.isEmpty())
        task_id = sj["data"].toObject()["task_id"].toString();
    if (task_id.isEmpty()) {
        resp.error = "Fincept async: no task_id in submit response";
        LOG_ERROR(kLlmFinceptTag, resp.error);
        return resp;
    }
    LOG_INFO(kLlmFinceptTag, "Fincept async task_id: " + task_id);

    const QString poll_url = status_base + task_id;
    QElapsedTimer clock;
    clock.start();
    for (int i = 0; clock.elapsed() < kPollBudgetMs; ++i) {
        QThread::msleep(static_cast<unsigned long>(i < kFastPollCount ? kFastPollMs : kSlowPollMs));

        auto poll = eventloop_request("GET", poll_url, {}, hdr, 15000);
        if (!poll.success) {
            LOG_WARN(kLlmFinceptTag, "Fincept async poll failed: " + poll.error);
            continue;
        }

        auto poll_doc = QJsonDocument::fromJson(poll.body);
        if (poll_doc.isNull())
            continue;

        QJsonObject pj = poll_doc.object();
        QString status = pj["status"].toString();
        QJsonObject data_obj = pj["data"].toObject();
        if (status.isEmpty())
            status = data_obj["status"].toString();

        LOG_INFO(kLlmFinceptTag, QString("Fincept async poll %1 status=%2 (%3 ms)").arg(i + 1).arg(status).arg(
                                     clock.elapsed()));

        if (status == "failed") {
            const QString err = fincept_extract_error(pj);
            // Always log the raw body when the reason can't be named. Without
            // it "unknown error" is a dead end — there is no way to tell a
            // silent backend from a reason we simply failed to look for.
            if (err.isEmpty()) {
                LOG_ERROR(kLlmFinceptTag, QStringLiteral("Fincept async task failed with no reason field. Raw "
                                                         "status body: ") +
                                              QString::fromUtf8(poll.body.left(1200)));
            }
            resp.error = "Fincept async task failed: " +
                         (err.isEmpty() ? QStringLiteral("the server reported a failure but gave no reason "
                                                         "(see log for the raw response)")
                                        : err);
            LOG_ERROR(kLlmFinceptTag, resp.error);
            resp.retryable = true; // upstream/model failure, not a bad request
            return resp;
        }
        if (status != "completed")
            continue;

        // data.data.response, or data.response on the flatter shape.
        const QJsonObject inner = data_obj["data"].toObject();
        QString response = inner["response"].toString();
        if (response.isEmpty())
            response = data_obj["response"].toString();

        // Structured tool_calls, if this backend produces them. Checked in the
        // same places the text response is found — the payload shape is not
        // published, so accept any of them rather than guess one.
        if (out_tool_calls) {
            for (const QJsonObject& scope : {inner, data_obj, pj}) {
                const QJsonArray tcs = scope["tool_calls"].toArray();
                if (!tcs.isEmpty()) {
                    *out_tool_calls = tcs;
                    break;
                }
            }
        }

        if (response.isEmpty() && (!out_tool_calls || out_tool_calls->isEmpty())) {
            resp.error = "Fincept async completed but response is empty";
            LOG_WARN(kLlmFinceptTag, "Fincept async task completed with empty response field");
            return resp;
        }

        resp.content = strip_think_blocks(response);
        resp.success = true;

        const QJsonObject usage = inner["usage"].toObject();
        if (!usage.isEmpty()) {
            resp.prompt_tokens = usage["input_tokens"].toInt();
            resp.completion_tokens = usage["output_tokens"].toInt();
            resp.total_tokens = usage["total_tokens"].toInt();
        }
        return resp;
    }

    resp.error = "Fincept async timed out waiting for response";
    LOG_ERROR(kLlmFinceptTag, resp.error);
    return resp;
}

LlmResponse LlmService::fincept_async_request(const QString& user_message,
                                              const std::vector<ConversationMessage>& history) {
    const bool tools_on = detail::effective_tools_enabled(tools_enabled_);
    const mcp::ToolFilter filter = detail::apply_request_policy(tool_filter_);

    // ── Prompt parts, kept separate so each can be trimmed independently ──
    // Priority when the 50 000-char cap bites, least valuable dropped first:
    //   1. oldest prior-conversation turns   (context from before this request)
    //   2. oldest tool-round blocks          (this turn's earlier findings)
    // The instructions, the tool catalog and the user's actual question are
    // never dropped — without them the model cannot act at all.
    QString head;
    if (!system_prompt_.isEmpty())
        head += system_prompt_ + "\n\n";
    if (tools_on) {
        const QString tool_catalog = build_tool_catalog_for_prompt(filter);
        if (!tool_catalog.isEmpty())
            head += tool_catalog + "\n";
    }

    QStringList history_blocks;
    for (const auto& m : history) {
        if (m.role == "user")
            history_blocks.append("User: " + m.content + "\n\n");
        else if (m.role == "assistant")
            history_blocks.append("Assistant: " + m.content + "\n\n");
    }
    const QString ask = "User: " + user_message;

    // One block per round so the oldest can be dropped when the prompt no
    // longer fits. Kept out of the fixed prefix so the trailing "continue"
    // instruction is re-composed each round rather than accumulating a stale
    // copy inside the transcript on every iteration.
    QStringList round_blocks;
    bool rounds_elided = false;
    bool history_dropped = false;

    static const QString kRoundsElidedNote =
        QStringLiteral("\n\n[Earlier tool rounds were dropped to fit the context. Their results are no longer "
                       "available — if you need one again, call the tool again.]\n");
    static const QString kHistoryElidedNote =
        QStringLiteral("[Earlier messages in this conversation were dropped to fit the context.]\n\n");

    auto total_size = [](const QStringList& l) {
        qsizetype n = 0;
        for (const auto& s : l)
            n += s.size();
        return n;
    };

    // Assemble head + history + ask + rounds + tail so the whole thing fits
    // under the server cap, dropping the least valuable parts first.
    auto compose_prompt = [&](const QString& tail) -> QString {
        const qsizetype budget = kServerMaxPromptChars - kPromptSafetyMargin;
        const qsizetype fixed = head.size() + ask.size() + tail.size();

        qsizetype avail = budget - fixed;
        if (avail < 0) {
            // Degenerate: the instructions plus the question alone exceed the
            // cap. Nothing to trim that wouldn't break the request outright —
            // send it and let the server rule, but say so in the log.
            LOG_ERROR(kLlmFinceptTag, QString("Fincept: fixed prompt parts are %1 chars, over the %2 cap — the "
                                              "system prompt or tool catalog is too large")
                                          .arg(fixed)
                                          .arg(kServerMaxPromptChars));
            avail = 0;
        }

        // Round blocks earn their space first — they are this turn's work.
        while (total_size(round_blocks) > avail && round_blocks.size() > 1) {
            round_blocks.removeFirst();
            if (!rounds_elided) {
                rounds_elided = true;
                LOG_WARN(kLlmFinceptTag, "Fincept: prompt at the server cap — dropping oldest tool rounds");
            }
        }
        // A single round can still exceed the budget on its own (several large
        // tool results in one turn). Truncate rather than 422 — and mark it, so
        // the model doesn't read a cut-off result as the whole answer.
        if (round_blocks.size() == 1 && round_blocks.first().size() > avail && avail > 200) {
            round_blocks[0] = round_blocks.first().left(avail - 120) +
                              QStringLiteral("\n[... tool output truncated to fit the context ...]\n");
            LOG_WARN(kLlmFinceptTag, "Fincept: single round block exceeded the prompt budget — truncated");
        }
        qsizetype remaining = avail - total_size(round_blocks) - (rounds_elided ? kRoundsElidedNote.size() : 0);

        // Whatever is left goes to prior conversation, oldest dropped first.
        while (total_size(history_blocks) > remaining && !history_blocks.isEmpty()) {
            history_blocks.removeFirst();
            if (!history_dropped) {
                history_dropped = true;
                LOG_WARN(kLlmFinceptTag, "Fincept: prompt at the server cap — dropping oldest conversation turns");
            }
            remaining = avail - total_size(round_blocks) - (rounds_elided ? kRoundsElidedNote.size() : 0) -
                        kHistoryElidedNote.size();
        }

        QString out = head;
        if (history_dropped)
            out += kHistoryElidedNote;
        out += history_blocks.join(QString());
        out += ask;
        if (rounds_elided)
            out += kRoundsElidedNote;
        out += round_blocks.join(QString());
        out += tail;

        if (out.size() > kServerMaxPromptChars) {
            LOG_ERROR(kLlmFinceptTag,
                      QString("Fincept: composed prompt is %1 chars, still over the cap").arg(out.size()));
        }
        return out;
    };

    // No ActivationTracker here, unlike the structured providers: nothing is
    // re-declared to the backend because nothing is declared to begin with.
    // tool_describe's schema lands in the transcript as text, which is what the
    // model reads before emitting the real call.
    //
    // Results are memoised per (tool, args) for the turn. A flat prompt gives the
    // model no structural cue that a call already happened, and it will re-issue
    // the identical call indefinitely — one observed turn spent 4 of its 10 rounds
    // on the same tool_describe{"name":"set_excel_sheet_data"} and ran out of
    // budget before writing anything. Re-serving the cached result with an
    // explicit "you already called this" note breaks the cycle without wasting
    // a round.
    QHash<QString, QString> call_cache;

    static const QString kContinueHint =
        QStringLiteral("\nContinue. If you still need data or an action, emit the next <tool_call> block and nothing "
                       "else. Do NOT repeat a call already listed above — its result is there; use it. There is no "
                       "limit on how many tools you may call, so gather everything you need and COMPLETE the task "
                       "rather than stopping to report progress. A result marked FAILED means that action did NOT "
                       "happen: fix it and retry, or say plainly that it failed — never describe failed work as "
                       "done. Only when the work is actually done, write the final answer in plain text with no "
                       "markup.\n\nAssistant:");

    QElapsedTimer turn_clock;
    turn_clock.start();

    // Rounds spent asking the model to re-emit malformed markup. Bounded so a
    // model that simply cannot produce a clean block still terminates with text.
    int malformed_rounds = 0;
    constexpr int kMaxMalformedRounds = 2;

    // Consecutive rounds that produced no new information (every call a cache
    // hit). This is the stop condition — not a round count.
    int no_progress_rounds = 0;

    // Tools the model has fetched a schema for, and how many consecutive rounds
    // it has spent searching/describing without calling any of them.
    QStringList described_tools;
    int discovery_only_rounds = 0;

    // Outcome tally across the turn, excluding the discovery meta-tools. Used to
    // catch the worst failure mode there is: every real call rejected, and the
    // model reporting the task as done anyway.
    int real_calls = 0;
    int real_failures = 0;
    bool challenged_fabrication = false;

    LlmResponse resp;
    for (int round = 0; round < kFinceptHardStop; ++round) {
        QJsonArray structured_calls;
        const QString prompt = compose_prompt(round_blocks.isEmpty() ? QString() : kContinueHint);
        resp = fincept_submit_poll(prompt, &structured_calls);
        // One retry on a transient upstream failure. An observed task ran 62 s
        // and then reported failure with no reason on a 5 KB prompt — nothing
        // about the request to fix, so resubmitting is the only useful response.
        // The endpoint documents that failed tasks are not charged.
        if (!resp.success && resp.retryable) {
            LOG_WARN(kLlmFinceptTag, "Fincept: task failed upstream — resubmitting once");
            resp = fincept_submit_poll(prompt, &structured_calls);
        }
        if (!resp.success)
            return resp;

        if (!tools_on)
            return resp;

        // ── Collect this round's tool calls ──
        std::vector<TextToolCall> calls;
        for (const auto& tc_val : structured_calls) {
            const QJsonObject tc = tc_val.toObject();
            const QJsonObject fn = tc["function"].toObject();
            const QString name = fn["name"].toString();
            if (name.isEmpty())
                continue;
            QJsonValue raw_args = fn["arguments"];
            QJsonObject args = raw_args.isObject()
                                   ? raw_args.toObject()
                                   : QJsonDocument::fromJson(raw_args.toString("{}").toUtf8()).object();
            calls.push_back({name, args});
        }
        if (calls.empty())
            calls = extract_text_tool_calls(resp.content);

        if (calls.empty()) {
            // Markup we recognised but could not parse. Returning it verbatim
            // dumps raw <tool_call> JSON into the chat bubble dressed up as an
            // answer — the exact failure this loop exists to prevent. Show the
            // model its own malformed output and ask for one clean block.
            if (has_text_tool_markup(resp.content) && ++malformed_rounds <= kMaxMalformedRounds) {
                LOG_WARN(kLlmFinceptTag, QString("Fincept: round %1 emitted unparsable tool markup (attempt %2/%3) — "
                                                 "asking the model to re-issue it")
                                             .arg(round)
                                             .arg(malformed_rounds)
                                             .arg(kMaxMalformedRounds));
                round_blocks.append("\n\nAssistant: " + resp.content.trimmed() +
                                    "\n\nSystem: That tool call could not be parsed. Emit EXACTLY ONE tool call, as a "
                                    "single JSON object on one line, wrapped in <tool_call> and </tool_call>, with no "
                                    "other text:\n<tool_call>{\"name\": \"TOOL_NAME\", \"arguments\": {}}</tool_call>\n"
                                    "If you need two tools, call the first one now and the second one next turn.\n");
                continue;
            }

            // Retries exhausted and the markup is still unparsable. Scrub it —
            // half a JSON blob is noise to the user either way.
            if (has_text_tool_markup(resp.content)) {
                LOG_WARN(kLlmFinceptTag, "Fincept: giving up on unparsable tool markup — stripping it from the answer");
                static const QRegularExpression rx_block("<\\s*(?:\\w+\\s*:\\s*)?tool_call\\s*>[\\s\\S]*?(?:<\\s*/"
                                                         "\\s*(?:\\w+\\s*:\\s*)?tool_call\\s*>|$)",
                                                         QRegularExpression::CaseInsensitiveOption);
                resp.content = resp.content.remove(rx_block).trimmed();
                if (resp.content.isEmpty()) {
                    resp.success = false;
                    resp.error = QStringLiteral("The model kept emitting malformed tool calls. Please try again.");
                    return resp;
                }
            }

            // Every real tool call was rejected, yet the model is answering.
            // That is the shape of a fabricated report — one turn had
            // create_workbook/add_tab (nonexistent) and 15 get_excel_cell calls
            // all fail, then described a finished workbook down to the cell
            // references and a per-share valuation. Send it back once with the
            // tally; if it insists, the answer stands but the log records it.
            if (real_calls > 0 && real_failures == real_calls && !challenged_fabrication) {
                challenged_fabrication = true;
                LOG_WARN(kLlmFinceptTag, QString("Fincept: all %1 real tool call(s) failed but the model produced an "
                                                 "answer — challenging it")
                                             .arg(real_calls));
                round_blocks.append("\n\nAssistant: " + resp.content.trimmed() +
                                    "\n\nSystem: STOP. Every one of the " + QString::number(real_calls) +
                                    " tool calls you made this turn FAILED. Nothing was created, written, read or "
                                    "recalculated. Any file, workbook, tab, cell value or computed figure you just "
                                    "described does not exist. Rewrite your reply: state plainly that the task was "
                                    "not completed, list the calls that failed and why, and do not report any "
                                    "number you did not receive from a successful tool result.\n");
                continue;
            }

            // Plain text — the answer.
            LOG_INFO(kLlmFinceptTag, QString("Fincept: finished after %1 round(s), %2 chars (%3/%4 real tool calls "
                                             "failed)")
                                         .arg(round + 1)
                                         .arg(resp.content.length())
                                         .arg(real_failures)
                                         .arg(real_calls));
            return resp;
        }

        if (round == kFinceptHardStop - 1) {
            LOG_WARN(kLlmFinceptTag, QString("Fincept: hit the %1-round runaway backstop — forcing a summary turn")
                                         .arg(kFinceptHardStop));
            break;
        }

        // ── Execute and splice the results back into the transcript ──
        QString block = "\n\nAssistant: " + resp.content.trimmed() + "\n\nTool results:\n";
        int fresh_calls = 0;
        bool only_discovery = true; // this round did nothing but search/describe
        for (const auto& tc : calls) {
            const int sep = tc.name.indexOf(QStringLiteral("__"));
            QString display = (sep >= 0) ? tc.name.mid(sep + 2) : tc.name;
            display.replace(QStringLiteral("-dot-"), QStringLiteral("."));

            const QString args_json =
                QString::fromUtf8(QJsonDocument(tc.args).toJson(QJsonDocument::Compact));
            const QString cache_key = display + QLatin1Char('|') + args_json;

            detail::emit_tool_progress(display, tc.args);

            const auto cached = call_cache.constFind(cache_key);
            if (cached != call_cache.constEnd()) {
                LOG_WARN(kLlmFinceptTag, QString("Fincept tool round %1: %2 repeated verbatim — serving cached result")
                                             .arg(round)
                                             .arg(display));
                block += "- " + display + " -> (ALREADY CALLED this turn with the same arguments; the result is "
                                          "unchanged and repeated here. Do not call it again.) " +
                         cached.value() + "\n";
                continue;
            }
            ++fresh_calls;

            LOG_INFO(kLlmFinceptTag, QString("Fincept tool round %1: executing %2 args=%3")
                                         .arg(round)
                                         .arg(display, args_json.left(200)));

            // allow_defer stays false: this path has no job_* protocol wired in,
            // so a receipt would be mistaken for the result (see §M5).
            auto tr = mcp::McpService::instance().execute_openai_function(tc.name, tc.args);
            LOG_INFO(kLlmFinceptTag, QString("Fincept tool round %1: %2 -> %3 (err=%4)")
                                         .arg(round)
                                         .arg(display, tr.success ? "OK" : "FAIL", tr.error.left(120)));
            const QString rendered = fincept_render_tool_result(tr);
            call_cache.insert(cache_key, rendered);
            block += "- " + display + " -> " + rendered + "\n";

            // Remember which real tools the model has already looked up, and
            // whether it ever got past looking things up this round.
            if (display == QLatin1String("tool_describe")) {
                const QString looked_up = tc.args.value(QStringLiteral("name")).toString();
                if (tr.success && !looked_up.isEmpty() && !described_tools.contains(looked_up))
                    described_tools.append(looked_up);
            } else if (display != QLatin1String("tool_list")) {
                only_discovery = false;
                ++real_calls;
                if (!tr.success)
                    ++real_failures;
            }
        }

        // Break the search-forever loop. tool_list with a fresh query always
        // returns something, so the no-progress check never fires — a turn can
        // spend every round hunting for a way to "execute" a tool it already
        // holds the schema for. Name the tools it already has and tell it to
        // call one.
        if (only_discovery && !described_tools.isEmpty()) {
            if (++discovery_only_rounds >= 2) {
                block += "\nSystem: STOP SEARCHING. You already have the schema for: " +
                         described_tools.join(QStringLiteral(", ")) +
                         ". There is no separate execute/invoke/run tool — emitting a <tool_call> block IS how you "
                         "call it. Your next message must be exactly one <tool_call> block naming one of those "
                         "tools, e.g. <tool_call>{\"name\": \"" +
                         described_tools.last() +
                         "\", \"arguments\": {...}}</tool_call>, with the arguments its schema requires.\n";
                LOG_WARN(kLlmFinceptTag, QString("Fincept: %1 discovery-only rounds — nudging the model to call %2")
                                             .arg(discovery_only_rounds)
                                             .arg(described_tools.last()));
            }
        } else {
            discovery_only_rounds = 0;
        }

        round_blocks.append(block);

        // Progress check. A round in which every call was already cached taught
        // the model nothing, so repeating it will teach it nothing either. This
        // — not a round count — is what ends a stuck turn.
        if (fresh_calls == 0) {
            if (++no_progress_rounds >= kMaxNoProgressRounds) {
                LOG_WARN(kLlmFinceptTag, QString("Fincept: %1 consecutive rounds with no new tool results — stopping")
                                             .arg(no_progress_rounds));
                break;
            }
        } else {
            no_progress_rounds = 0;
        }
    }

    LOG_INFO(kLlmFinceptTag, QString("Fincept: tool phase ended after %1 round block(s), %2 ms — requesting close-out")
                                 .arg(round_blocks.size())
                                 .arg(turn_clock.elapsed()));

    // Stopped with a tool call still pending. Ask for a close-out in plain text
    // rather than leaving raw markup in the chat bubble. Passed as the tail so
    // it survives trimming — dropping the instruction would defeat the turn.
    const QString final_prompt = compose_prompt(QStringLiteral(
        "\n\nStop calling tools now and reply in plain text with no tool calls or markup: state what you completed "
        "and anything still incomplete.\n\nAssistant:"));
    LlmResponse final_turn = fincept_submit_poll(final_prompt, nullptr);
    if (!final_turn.success && final_turn.retryable) {
        LOG_WARN(kLlmFinceptTag, "Fincept: close-out turn failed upstream — resubmitting once");
        final_turn = fincept_submit_poll(final_prompt, nullptr);
    }
    if (final_turn.success && !final_turn.content.isEmpty())
        return final_turn;

    // Nothing usable — surface the failure instead of leaking tool markup.
    LlmResponse err;
    err.error = final_turn.error.isEmpty() ? QStringLiteral("Fincept LLM stopped without producing an answer.")
                                           : final_turn.error;
    return err;
}

} // namespace fincept::ai_chat
