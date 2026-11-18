// LlmFinceptAsync.cpp — Fincept-specific async LLM path.
//
// POST /research/llm/async to submit, then poll /research/llm/status/{id}. The
// submit endpoint takes a FLAT prompt (not an OpenAI messages array), so system
// prompt + tool catalog + history are synthesised into a single string here.
//
// ── Why this is a loop ────────────────────────────────────────────────────
// Tool calling on this path is MULTI-ROUND. It used to be single-shot: execute
// whatever tool the first reply named, then ask for a summary with tool calls
// forbidden. Operating the terminal takes several rounds (look → open → act →
// read) before there is anything worth summarising, so the loop is the fix.
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
// That `(2013)` + request_id envelope is MiniMax's. The cause is a FORMAT
// mismatch, not a flattening: the backend forwards `tools` untouched to an
// Anthropic-format Messages API (apiback research.py _run_llm_task), which
// expects {"name","description","input_schema"} items — the array we sent was
// OpenAI-shaped ({"type":"function","function":{"name",...}}), which has no
// top-level `name`. Tool-use blocks of the answer come back as OpenAI-style
// data.tool_calls (research.py _llm_output). finagent_core's FinceptChat uses
// the Anthropic shape on this same endpoint.
//
// Tools on this path still travel ONE way: injected into the prompt as a text
// catalog, invoked back as <tool_call> markup, executed here, and spliced into
// the transcript for the next round. Switching to structured `tools` (Anthropic
// shape) must be verified against the deployed endpoint first — a shape it
// rejects fails every Fincept chat, not just tool-using ones.
//
// Response-side structured tool_calls are still parsed if they ever appear;
// that costs nothing and is how we'd notice the backend gaining support.

#include "core/config/AppConfig.h"
#include "core/logging/Logger.h"
#include "mcp/McpProvider.h"
#include "mcp/McpService.h"
#include "mcp/ResultStore.h"
#include "services/llm/LlmContentExtractors.h"
#include "services/llm/LlmErrors.h"
#include "services/llm/LlmRequestPolicy.h"
#include "services/llm/LlmService.h"
#include "services/llm/LlmToolActivity.h"

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
// There is deliberately no round budget on this path. Operating the terminal
// takes several rounds per action (look, act, wait, read), and with the old
// tool-discovery catalogue every ceiling tried here (10, then 20) was hit before
// any real work — the model ran out of rounds and apologised. Work is bounded by
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

// The submit endpoint rate-limits to one request per second per user. A tool
// loop that answers fast can trip it between rounds; one short wait recovers
// that without failing a turn that already did real work.
constexpr int kSubmitRateLimitBackoffMs = 1500;

// Consecutive failed status polls (transport errors, 5xx) before the wait is
// abandoned. Without a cap a dead connection burned the whole 120 s poll budget
// before saying anything.
constexpr int kMaxConsecutivePollFailures = 8;

// Sum one successful round's usage into the turn total. Every round of the
// loop re-sends the whole prompt and is billed for it; returning only the last
// round's usage made a ten-round turn look ten times cheaper than it was.
void fincept_add_usage(LlmResponse& total, const LlmResponse& round) {
    total.prompt_tokens += round.prompt_tokens;
    total.completion_tokens += round.completion_tokens;
    total.total_tokens += round.total_tokens > 0 ? round.total_tokens : round.prompt_tokens + round.completion_tokens;
}
} // namespace

// Build a compact tool catalog string for injection into the system prompt.
// This lets models that don't support structured tool_calls still emit
// text-based tool invocations that extract_text_tool_calls can detect. The
// internal catalogue is ten core tools, so every tool is listed inline with its
// parameters (external servers' tools follow, capped).
static QString build_tool_catalog_for_prompt(const mcp::ToolFilter& filter) {
    auto all_tools = mcp::McpService::instance().get_all_tools(filter);
    if (all_tools.empty())
        return {};

    QString catalog;
    catalog += "You have access to tools. To use one, emit a <tool_call> block:\n";
    catalog += "<tool_call>{\"name\": \"TOOL_NAME\", \"arguments\": {\"param\": \"value\"}}</tool_call>\n";
    catalog += "Emit tool_call blocks ALONE, with no commentary around them — you will be called again with the "
               "results and can then continue. You may emit SEVERAL blocks in one turn when the calls are "
               "independent (it is faster). Never announce what you are about to do (\"I'll look for…\", \"Let me "
               "check…\"): a reply without a tool_call block is your FINAL answer to the user, so either act or "
               "answer. Only call a tool when the request needs live data or an action in the terminal; answer "
               "greetings and general questions directly in plain text.\n"
               "Arguments are JSON: a parameter marked \"list\" is a JSON array (\"args\": [\"quote\", \"AAPL\"]), "
               "never one string; parameters marked * are required; leave out the ones you do not need.\n"
               "Data recipe: python(action=\"search\", query=\"…\") finds a script, python(action=\"info\", "
               "script=\"…\") shows its commands, python(action=\"run\", script=\"…\", args=[…]) runs it. If a call "
               "FAILS, read the reason, fix the arguments and call again; if a script has nothing, try another "
               "script or read the open screen — do not stop at the first obstacle.\n"
               "Shortcuts that need no search (script, args): quotes for several symbols — yfinance_data "
               "[\"batch_quotes\", \"^GSPC\", \"^DJI\", \"^IXIC\", \"AAPL\"]; one quote — yfinance_data [\"quote\", "
               "\"AAPL\"]; price history — yfinance_data [\"historical_period\", \"AAPL\", \"6mo\", \"1d\"]; company "
               "news — yfinance_data [\"news\", \"AAPL\", \"10\"]; today's US movers — screener_data [\"predefined\", "
               "\"day_gainers\", \"15\"] (also day_losers, most_actives); company profile / fundamentals — "
               "yfinance_data [\"info\", \"AAPL\"]. Index symbols: ^GSPC (S&P 500), ^DJI (Dow), ^IXIC (Nasdaq), "
               "^RUT (Russell 2000), ^VIX. Use these first for market questions; search only for anything else.\n"
               "Work on the screen that owns the task. Decide the screen first: one company = "
               "equity_research; market overview and movers = markets; holdings and risk = portfolio; orders = "
               "equity_trading or crypto_trading; option chains = fno; stock filters = screener; macro series = "
               "economics; strategy tests = backtesting; headlines = news (ui_open() with no arguments lists "
               "every screen and the tasks that belong on it). Open it with the "
               "symbol when there is one, then read or operate that screen — do not do a portfolio task in "
               "Markets or a screening task in Equity Research. A request to open, show or pull up a stock, "
               "company or ticker is ui_open(screen=\"equity_research\", symbol=\"AAPL\"): one call, then say it "
               "is open. A question that only needs figures needs no screen: use the data shortcuts.\n"
               "You operate Fincept Terminal like a person: ui_overview shows what is open, ui_open opens a "
               "screen (optionally on a symbol), ui_snapshot lists a panel's controls with refs, ui_act works "
               "them, ui_read reads tables and texts in full, ui_wait waits for data to load. The server "
               "prefix (fincept-terminal__) is optional.\n\n";
    catalog += "Available tools:\n";
    int count = 0;
    for (const auto& tool : all_tools) {
        catalog += "- " + mcp::McpProvider::encode_tool_name_for_wire(tool.name) + ": " + tool.description;
        QJsonObject props = tool.input_schema["properties"].toObject();
        if (!props.isEmpty()) {
            // name:type, "*" = required, choices for enums, element type for lists. Names
            // alone made the model guess shapes (`args` sent as a string, not a list).
            QStringList required;
            for (const auto& rv : tool.input_schema["required"].toArray())
                required.append(rv.toString());
            QStringList params;
            for (auto it = props.constBegin(); it != props.constEnd(); ++it) {
                const QJsonObject spec = it.value().toObject();
                QString type = spec.value(QStringLiteral("type")).toString();
                if (type == QLatin1String("array")) {
                    const QString item = spec.value(QStringLiteral("items")).toObject().value(QStringLiteral("type")).toString();
                    type = QStringLiteral("list of ") + (item.isEmpty() ? QStringLiteral("values") : item + QLatin1Char('s'));
                }
                QString one = it.key() + (required.contains(it.key()) ? QStringLiteral("*") : QString());
                if (!type.isEmpty())
                    one += QLatin1Char(':') + type;
                const QJsonArray choices = spec.value(QStringLiteral("enum")).toArray();
                if (!choices.isEmpty() && choices.size() <= 8) {
                    QStringList names;
                    for (const auto& c : choices)
                        names.append(c.toString());
                    one += QLatin1Char('[') + names.join(QLatin1Char('|')) + QLatin1Char(']');
                }
                params.append(one);
            }
            catalog += " (params: " + params.join(", ") + ")";
        }
        catalog += "\n";
        ++count;
        if (count >= 40) {
            catalog += "... and " + QString::number(all_tools.size() - 40) + " more tools available.\n";
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

// Turn a backend failure string into one readable sentence for the chat.
//
// Upstream failures arrive with the provider's raw envelope attached — e.g.
//   LLM service error: {"type":"error","error":{"type":"invalid_request_error",
//                       "message":"invalid params, function name is empty (2013)"}}
// or, from the async worker, a Python dict repr. That used to be shown to the
// user verbatim. Keep the lead-in, replace the blob with its message, and leave
// the raw text to the log (the caller logs it).
static QString fincept_humanize_error(const QString& raw) {
    QString s = raw.trimmed();
    const qsizetype open = s.indexOf(QLatin1Char('{'));
    const qsizetype close = s.lastIndexOf(QLatin1Char('}'));
    if (open >= 0 && close > open) {
        QString lead = s.left(open).trimmed();
        while (lead.endsWith(QLatin1Char(':')) || lead.endsWith(QLatin1Char('-')))
            lead.chop(1);
        lead = lead.trimmed();
        QString inner;
        const QJsonDocument doc = QJsonDocument::fromJson(s.mid(open, close - open + 1).toUtf8());
        if (doc.isObject())
            inner = fincept_extract_error(doc.object());
        if (inner.isEmpty() || inner.startsWith(QLatin1Char('{'))) {
            // Python repr: 'message': "..." or 'detail': '...'
            static const QRegularExpression rx_kv(
                QStringLiteral("['\"](?:message|detail|reason)['\"]\\s*:\\s*(?:'([^']*)'|\"([^\"]*)\")"));
            const auto m = rx_kv.match(s);
            inner = m.hasMatch() ? (m.captured(1).isEmpty() ? m.captured(2) : m.captured(1)) : QString();
        }
        if (!inner.isEmpty())
            s = lead.isEmpty() ? inner : lead + QStringLiteral(": ") + inner;
        else
            s = (lead.isEmpty() ? QStringLiteral("The server returned an unreadable error") : lead) +
                QStringLiteral(" (details in the log)");
    }
    if (s.size() > 300)
        s = s.left(299) + QChar(0x2026);
    return s;
}

// This endpoint is submit-and-poll: nothing arrives until a round is complete, so
// without help the chat shows a silent block for the whole turn and then the full
// answer at once. Two things make the turn readable while it runs:
//
//  - what the model wrote around its tool calls ("Checking the index constituents
//    first…") and the loop's own status go to the Thinking channel as they happen;
//  - the final answer is handed to the chat in small pieces over a fraction of a
//    second, so it reads as a stream like every other provider's. The service still
//    returns the complete text; the chat replaces the streamed copy with it.
static void fincept_emit_thinking(const QString& text) {
    const QString t = text.trimmed();
    if (!t.isEmpty())
        detail::emit_progress(think_stream_prefix() + t + QStringLiteral("\n\n"));
}

static QString fincept_strip_tool_markup(QString content) {
    static const QRegularExpression rx_block(
        QStringLiteral("<\\s*(?:\\w+\\s*:\\s*)?tool_call\\s*>[\\s\\S]*?(?:<\\s*/\\s*(?:\\w+\\s*:\\s*)?tool_call\\s*>|$)"),
        QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression rx_invoke(QStringLiteral("<\\s*invoke\\b[\\s\\S]*?(?:<\\s*/\\s*invoke\\s*>|$)"),
                                              QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression rx_fence(QStringLiteral("```tool_call[\\s\\S]*?(?:```|$)"));
    content.remove(rx_block);
    content.remove(rx_invoke);
    content.remove(rx_fence);
    return content.trimmed();
}

static void fincept_stream_answer(const QString& answer) {
    const QString text = answer.trimmed();
    if (text.isEmpty())
        return;
    // Word-sized pieces; the whole reveal is capped near 0.9 s however long the answer is.
    constexpr int kPiece = 28;
    const int pieces = std::max<int>(1, static_cast<int>((text.size() + kPiece - 1) / kPiece));
    const int delay_ms = std::clamp(900 / pieces, 0, 14);
    qsizetype pos = 0;
    while (pos < text.size()) {
        if (detail::cancel_requested())
            return;
        qsizetype end = std::min<qsizetype>(text.size(), pos + kPiece);
        // Do not cut inside a word (or a surrogate pair): extend to the next space.
        while (end < text.size() && !text.at(end).isSpace() && end - pos < kPiece * 2)
            ++end;
        detail::emit_progress(text.mid(pos, end - pos));
        pos = end;
        if (delay_ms > 0 && pos < text.size())
            QThread::msleep(static_cast<unsigned long>(delay_ms));
    }
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

static void fincept_emit_thinking(const QString& text);

LlmResponse LlmService::fincept_submit_poll(const QString& prompt, QJsonArray* out_tool_calls) {
    LlmResponse resp;
    if (out_tool_calls)
        *out_tool_calls = {};

    // {prompt, max_tokens} only — the endpoint's documented contract. No
    // `tools` (see the header note), no temperature (backend default).
    QJsonObject submit_body;
    submit_body["prompt"] = prompt;
    submit_body["max_tokens"] = resolved_max_tokens();
    // Reasoning effort chosen in the chat (default medium). A server that predates the
    // field ignores it.
    if (!detail::t_reasoning_effort.isEmpty())
        submit_body["effort"] = detail::t_reasoning_effort;
    // The server adds its fixed description of the terminal as the model's system prompt.
    submit_body["context"] = QStringLiteral("terminal");

    auto hdr = get_headers();
    const QString fincept_base = fincept::AppConfig::instance().api_base_url();
    const QString async_url = fincept_base + "/research/llm/async";
    const QString status_base = fincept_base + "/research/llm/status/";

    LOG_DEBUG(kLlmFinceptTag, logf("Fincept async submit", {{"url", log_url(async_url)},
                                                            {"api_key_set", !api_key_.isEmpty()},
                                                            {"prompt_chars", prompt.length()}}));

    QByteArray json_data = QJsonDocument(submit_body).toJson(QJsonDocument::Compact);
    auto submit = eventloop_request("POST", async_url, json_data, hdr, 30000);
    if (!submit.success && !submit.cancelled && submit.status == 429) {
        const int backoff = std::clamp(submit.retry_after_s * 1000, kSubmitRateLimitBackoffMs, 10000);
        LOG_WARN(kLlmFinceptTag, QString("Fincept async submit rate-limited — retrying once in %1 ms").arg(backoff));
        if (!detail::cancellable_sleep(backoff)) {
            resp.cancelled = true;
            resp.error = "Request cancelled";
            return resp;
        }
        submit = eventloop_request("POST", async_url, json_data, hdr, 30000);
    }
    if (!submit.success) {
        if (submit.cancelled) {
            resp.cancelled = true;
            resp.error = "Request cancelled";
            return resp;
        }
        // A refused key/session or an empty credit balance is an expected answer (policy B.1):
        // INFO; anything else failed the user's request: ERROR. The body only at DEBUG.
        if (classify_fincept_failure(submit.status, submit.error + QLatin1Char(' ') +
                                                        QString::fromUtf8(submit.body)) != LlmErrorKind::Other)
            LOG_INFO(kLlmFinceptTag, logf("LLM submit refused", {{"provider", "fincept"},
                                                                 {"status", submit.status},
                                                                 {"body_bytes", submit.body.size()},
                                                                 {"error", submit.error}}));
        else
            LOG_ERROR(kLlmFinceptTag, logf("LLM submit failed", {{"provider", "fincept"},
                                                                 {"status", submit.status},
                                                                 {"body_bytes", submit.body.size()},
                                                                 {"error", submit.error}}));
        LOG_DEBUG(kLlmFinceptTag, logf("LLM submit failure body", {{"body", QString::fromUtf8(submit.body.left(200))}}));
        if (submit.status == 401 || submit.status == 403) {
            // This client's QNAM bypasses HttpClient: report the refusal so a
            // stale session recovers (or an IP block pauses polling), and show
            // the server's reason. Not retryable — the same request would fail.
            resp.error = handle_fincept_refusal(submit, hdr.value(QStringLiteral("X-Session-Token")));
            return resp;
        }
        const LlmErrorKind kind =
            classify_fincept_failure(submit.status, submit.error + QLatin1Char(' ') + QString::fromUtf8(submit.body));
        if (kind != LlmErrorKind::Other) {
            // Not retryable: resubmitting cannot buy credits or fix a key.
            resp.error = make_fincept_error(kind, QString::fromUtf8(submit.body) + QLatin1Char(' ') + submit.error);
            return resp;
        }
        resp.error = "Fincept AI request failed: " + fincept_humanize_error(submit.error);
        // 5xx / gateway / transport trouble may clear on a resubmit; a 4xx will not.
        resp.retryable = submit.status == 0 || submit.status >= 500;
        return resp;
    }

    auto submit_doc = QJsonDocument::fromJson(submit.body);
    if (submit_doc.isNull()) {
        resp.error = QStringLiteral("The Fincept server sent an unreadable reply (details in the log).");
        LOG_ERROR(kLlmFinceptTag, logf("LLM submit answer is not valid JSON", {{"provider", "fincept"},
                                                                                {"status", submit.status},
                                                                                {"body_bytes", submit.body.size()}}));
        LOG_DEBUG(kLlmFinceptTag, logf("LLM submit answer body", {{"body", QString::fromUtf8(submit.body.left(200))}}));
        return resp;
    }

    // Response can nest task_id at top level or inside data
    QJsonObject sj = submit_doc.object();
    QString task_id = sj["task_id"].toString();
    if (task_id.isEmpty())
        task_id = sj["data"].toObject()["task_id"].toString();
    if (task_id.isEmpty()) {
        resp.error = QStringLiteral("The Fincept server did not accept the request (no task id; details in the log).");
        LOG_ERROR(kLlmFinceptTag, logf("LLM submit answer has no task id", {{"provider", "fincept"},
                                                                             {"status", submit.status},
                                                                             {"body_bytes", submit.body.size()}}));
        LOG_DEBUG(kLlmFinceptTag, logf("LLM submit answer body", {{"body", QString::fromUtf8(submit.body.left(200))}}));
        return resp;
    }
    LOG_DEBUG(kLlmFinceptTag, logf("Fincept async task submitted", {{"run", task_id}}));

    const QString poll_url = status_base + task_id;
    QElapsedTimer clock;
    clock.start();
    int poll_failures = 0;
    for (int i = 0; clock.elapsed() < kPollBudgetMs; ++i) {
        // Sliced sleep so a user's Stop lands within ~100 ms instead of after the poll gap.
        if (!detail::cancellable_sleep(i < kFastPollCount ? kFastPollMs : kSlowPollMs)) {
            resp.cancelled = true;
            resp.error = "Request cancelled";
            return resp;
        }

        auto poll = eventloop_request("GET", poll_url, {}, hdr, 15000);
        if (!poll.success) {
            if (poll.cancelled) {
                resp.cancelled = true;
                resp.error = "Request cancelled";
                return resp;
            }
            LOG_WARN(kLlmFinceptTag, QString("Fincept async poll failed (HTTP %1): %2").arg(poll.status).arg(poll.error));
            // A rejected key/session cannot recover by polling again (the old loop
            // kept polling for the full two minutes and then reported a timeout).
            if (poll.status == 401 || poll.status == 403) {
                resp.error = handle_fincept_refusal(poll, hdr.value(QStringLiteral("X-Session-Token")));
                return resp;
            }
            const LlmErrorKind kind = classify_fincept_failure(poll.status, QString());
            if (kind != LlmErrorKind::Other) {
                resp.error = make_fincept_error(kind, QString::fromUtf8(poll.body));
                return resp;
            }
            if (poll.status == 404) {
                // The backend keeps tasks for 30 min and loses them on restart.
                resp.error = QStringLiteral("The Fincept server lost this request before it finished (task expired). "
                                            "Please try again.");
                resp.retryable = true;
                return resp;
            }
            if (++poll_failures >= kMaxConsecutivePollFailures) {
                resp.error = "Lost contact with the Fincept server while waiting for the answer: " +
                             fincept_humanize_error(poll.error);
                resp.retryable = true;
                return resp;
            }
            continue;
        }
        poll_failures = 0;

        auto poll_doc = QJsonDocument::fromJson(poll.body);
        if (poll_doc.isNull()) {
            LOG_WARN_RL(kLlmFinceptTag, QStringLiteral("fincept-poll-json"), 60000,
                        logf("Research poll answer is not valid JSON", {{"run", task_id},
                                                                        {"attempt", i + 1},
                                                                        {"bytes", poll.body.size()}}));
            continue;
        }

        QJsonObject pj = poll_doc.object();
        QString status = pj["status"].toString();
        QJsonObject data_obj = pj["data"].toObject();
        if (status.isEmpty())
            status = data_obj["status"].toString();

        LOG_DEBUG(kLlmFinceptTag, logf("Fincept async poll", {{"run", task_id},
                                                              {"attempt", i + 1},
                                                              {"status", status},
                                                              {"duration_ms", clock.elapsed()}}));

        if (status == "failed") {
            const QString err = fincept_extract_error(pj);
            // Always log the raw body when the reason can't be named. Without
            // it "unknown error" is a dead end — there is no way to tell a
            // silent backend from a reason we simply failed to look for.
            if (err.isEmpty()) {
                LOG_ERROR(kLlmFinceptTag, logf("LLM task failed", {{"provider", "fincept"},
                                                                   {"run", task_id},
                                                                   {"reason", "no_reason_field"},
                                                                   {"body_bytes", poll.body.size()}}));
                LOG_DEBUG(kLlmFinceptTag, logf("LLM task failure body", {{"body", QString::fromUtf8(poll.body.left(200))}}));
            } else if (classify_fincept_failure(0, err) != LlmErrorKind::Other) {
                // Out of credits / key refused: an expected answer, not a fault (policy B.1).
                LOG_INFO(kLlmFinceptTag, logf("LLM task refused", {{"provider", "fincept"},
                                                                   {"run", task_id},
                                                                   {"error", err.left(200)}}));
            } else {
                LOG_ERROR(kLlmFinceptTag, logf("LLM task failed", {{"provider", "fincept"},
                                                                   {"run", task_id},
                                                                   {"error", err.left(200)}}));
            }
            // The worker reports a failed credit check as text ("402: {...}").
            // Classify it before anything else: it is final, and resubmitting
            // it (the old behaviour) only doubled the wait for the same refusal.
            const LlmErrorKind kind = classify_fincept_failure(0, err);
            if (kind != LlmErrorKind::Other) {
                resp.error = make_fincept_error(kind, err);
                return resp;
            }
            resp.error = err.isEmpty() ? QStringLiteral("The Fincept AI service reported a failure but gave no reason "
                                                        "(details in the log). Please try again.")
                                       : "The Fincept AI service could not complete this request: " +
                                             fincept_humanize_error(err);
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
            resp.error = QStringLiteral("The Fincept AI service returned an empty answer. Please try again.");
            LOG_WARN(kLlmFinceptTag, "Fincept async task completed with empty response field");
            return resp;
        }

        resp.content = strip_think_blocks(response);
        resp.success = true;

        // The model's reasoning for this call, when the server returns it: shown live in
        // the chat's Thinking section.
        {
            QString thinking = inner["thinking"].toString();
            if (thinking.isEmpty())
                thinking = data_obj["thinking"].toString();
            fincept_emit_thinking(thinking);
        }

        // GET /research/llm/status returns {status, data: {response, usage, ...}}:
        // usage sits beside response in data_obj. The nested shape is kept as a
        // fallback, matching the response lookup above.
        QJsonObject usage = data_obj["usage"].toObject();
        if (usage.isEmpty())
            usage = inner["usage"].toObject();
        if (!usage.isEmpty()) {
            resp.prompt_tokens = usage["input_tokens"].toInt();
            resp.completion_tokens = usage["output_tokens"].toInt();
            resp.total_tokens = usage["total_tokens"].toInt();
        }
        return resp;
    }

    resp.error = QStringLiteral("The Fincept AI service did not answer within %1 s. Please try again.")
                     .arg(kPollBudgetMs / 1000);
    LOG_ERROR(kLlmFinceptTag, "Fincept async timed out waiting for task " + task_id);
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

    // Results are memoised per (tool, args) for the turn. A flat prompt gives the
    // model no structural cue that a call already happened, and it will re-issue
    // the identical call indefinitely — one observed turn spent 4 of its 10 rounds
    // re-issuing one identical call and ran out of budget before doing anything
    // else. Re-serving the cached result with an
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

    // Outcome tally across the turn. Used to catch the worst failure mode there
    // is: every call rejected, and the model reporting the task as done anyway.
    int real_calls = 0;
    int real_failures = 0;
    bool challenged_fabrication = false;

    // Replies that only announced a next step ("I'll look for…") and were sent back.
    int intent_nudges = 0;
    constexpr int kMaxIntentNudges = 3;

    // Token usage summed over every successful round (see fincept_add_usage).
    LlmResponse usage;
    const auto with_turn_usage = [&usage](LlmResponse r) {
        r.prompt_tokens = usage.prompt_tokens;
        r.completion_tokens = usage.completion_tokens;
        r.total_tokens = usage.total_tokens;
        return r;
    };
    // Success of each memoised call, so a served-from-cache repeat reports the
    // same outcome on the chat's tool card as the original call did.
    QHash<QString, bool> call_cache_ok;

    LlmResponse resp;
    // Wall-clock ceiling for the whole tool phase (mcp/tool_loop_deadline_ms, 0 = off).
    // This loop deliberately has no ROUND budget (see kFinceptHardStop), which left it
    // with no budget at all: 200 rounds x a poll of up to 2 minutes each, with the one
    // LLM worker thread — and every request queued behind it — held the whole time.
    // Exhausting it is a normal outcome: it falls through to the plain-text close-out
    // turn below, exactly like the no-progress stop (CLAUDE.md M7).
    const qint64 tool_deadline_ms = detail::tool_loop_deadline_ms();
    for (int round = 0; round < kFinceptHardStop; ++round) {
        if (detail::cancel_requested()) {
            resp = LlmResponse{};
            resp.cancelled = true;
            resp.error = "Request cancelled";
            return resp;
        }
        if (round > 0 && tool_deadline_ms > 0 && turn_clock.elapsed() >= tool_deadline_ms) {
            LOG_WARN(kLlmFinceptTag, QString("Fincept: tool phase hit the %1 s time budget after %2 round(s) — "
                                             "requesting close-out")
                                         .arg(tool_deadline_ms / 1000)
                                         .arg(round));
            break;
        }
        QJsonArray structured_calls;
        const QString prompt = compose_prompt(round_blocks.isEmpty() ? QString() : kContinueHint);
        // Said before the model is called, so the turn shows life the instant it is sent
        // (and again at every step) instead of a silent block until the first reply lands.
        fincept_emit_thinking(round == 0 ? QStringLiteral("Request sent. Working out how to answer…")
                                         : QStringLiteral("Step %1: reading the results and deciding what comes next…")
                                               .arg(round + 1));
        resp = fincept_submit_poll(prompt, &structured_calls);
        // One retry on a transient upstream failure. An observed task ran 62 s
        // and then reported failure with no reason on a 5 KB prompt — nothing
        // about the request to fix, so resubmitting is the only useful response.
        // The endpoint documents that failed tasks are not charged.
        if (!resp.success && resp.retryable) {
            LOG_WARN(kLlmFinceptTag, logf("LLM task failed upstream, resubmitting once",
                                          {{"provider", "fincept"}, {"round", round}, {"error", resp.error}}));
            resp = fincept_submit_poll(prompt, &structured_calls);
        }
        if (!resp.success)
            return resp;
        fincept_add_usage(usage, resp);

        if (!tools_on)
            return with_turn_usage(resp);

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

            // "I'll look for a script that…" and nothing else: the model narrated its next
            // step instead of taking it. Returned as the answer, the user gets a promise and
            // a dead chat (the very first run of the redesigned tab did exactly this). Send
            // it back — bounded, so a model that cannot act still ends with its own words.
            if (intent_nudges < kMaxIntentNudges && looks_like_unfinished_intent(resp.content) &&
                (tool_deadline_ms <= 0 || turn_clock.elapsed() < tool_deadline_ms)) {
                ++intent_nudges;
                LOG_INFO(kLlmFinceptTag, logf("Model announced a step without taking it, asking it to act",
                                              {{"provider", "fincept"}, {"round", round}, {"attempt", intent_nudges}}));
                fincept_emit_thinking(resp.content);
                round_blocks.append("\n\nAssistant: " + resp.content.trimmed() +
                                    "\n\nSystem: That was an announcement, not an answer, and nothing was done. Do it "
                                    "now: emit the <tool_call> block(s) for that step immediately, with no other text. "
                                    "If you already have everything you need, write the complete final answer for the "
                                    "user instead (with the actual figures). Do not describe what you will do.\n");
                continue;
            }

            // Plain text — the answer.
            LOG_DEBUG(kLlmFinceptTag, QString("Fincept: finished after %1 round(s), %2 chars (%3/%4 real tool calls "
                                              "failed)")
                                         .arg(round + 1)
                                         .arg(resp.content.length())
                                         .arg(real_failures)
                                         .arg(real_calls));
            fincept_stream_answer(resp.content);
            return with_turn_usage(resp);
        }

        if (round == kFinceptHardStop - 1) {
            LOG_WARN(kLlmFinceptTag, QString("Fincept: hit the %1-round runaway backstop — forcing a summary turn")
                                         .arg(kFinceptHardStop));
            break;
        }

        // What the model wrote around its tool calls is its reasoning for this step: shown
        // live in the chat's Thinking section instead of being dropped.
        fincept_emit_thinking(fincept_strip_tool_markup(resp.content));

        // ── Execute and splice the results back into the transcript ──
        QString block = "\n\nAssistant: " + resp.content.trimmed() + "\n\nTool results:\n";
        int fresh_calls = 0;
        for (const auto& tc : calls) {
            const int sep = tc.name.indexOf(QStringLiteral("__"));
            QString display = (sep >= 0) ? tc.name.mid(sep + 2) : tc.name;
            display.replace(QStringLiteral("-dot-"), QStringLiteral("."));

            const QString args_json =
                QString::fromUtf8(QJsonDocument(tc.args).toJson(QJsonDocument::Compact));
            const QString cache_key = display + QLatin1Char('|') + args_json;

            detail::emit_tool_progress(display, tc.args);

            // Only pure lookups are memoised. The terminal's tools observe and
            // operate live screens: a second ui_snapshot after a click must see
            // the new state, never the cached old one.
            const QString action = tc.args.value(QStringLiteral("action")).toString();
            const bool cacheable = display == QLatin1String("python") &&
                                   (action == QLatin1String("search") || action == QLatin1String("info"));
            const auto cached = cacheable ? call_cache.constFind(cache_key) : call_cache.constEnd();
            if (cached != call_cache.constEnd()) {
                LOG_WARN(kLlmFinceptTag, QString("Fincept tool round %1: %2 repeated verbatim — serving cached result")
                                             .arg(round)
                                             .arg(display));
                block += "- " + display + " -> (ALREADY CALLED this turn with the same arguments; the result is "
                                          "unchanged and repeated here. Do not call it again.) " +
                         cached.value() + "\n";
                ToolDoneRecord repeat;
                repeat.outcome = call_cache_ok.value(cache_key, true) ? ToolDoneRecord::Outcome::Ok
                                                                      : ToolDoneRecord::Outcome::Failed;
                repeat.elapsed_ms = 0;
                repeat.label = detail::tool_progress_label(display, tc.args);
                repeat.detail = QStringLiteral("repeated call — reused this turn's earlier result");
                detail::emit_progress(tool_stream_prefix() + encode_tool_done(repeat) + QStringLiteral("\n"));
                continue;
            }
            ++fresh_calls;

            LOG_DEBUG(kLlmFinceptTag, logf("LLM tool call started",
                                           {{"tool", display}, {"round", round}, {"args_bytes", args_json.size()}}));

            QElapsedTimer call_clock;
            call_clock.start();
            auto tr = mcp::McpService::instance().execute_openai_function(tc.name, tc.args);
            emit_tool_done(display, tc.args, tr, call_clock.elapsed()); // logs "LLM tool call finished"
            const QString rendered = fincept_render_tool_result(tr);
            if (cacheable) {
                call_cache.insert(cache_key, rendered);
                call_cache_ok.insert(cache_key, tr.success);
            }
            block += "- " + display + " -> " + rendered + "\n";

            ++real_calls;
            if (!tr.success)
                ++real_failures;
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

    LOG_DEBUG(kLlmFinceptTag, QString("Fincept: tool phase ended after %1 round block(s), %2 ms — requesting close-out")
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
        LOG_WARN(kLlmFinceptTag, logf("LLM close-out turn failed upstream, resubmitting once",
                                      {{"provider", "fincept"}, {"error", final_turn.error}}));
        final_turn = fincept_submit_poll(final_prompt, nullptr);
    }
    if (final_turn.success && !final_turn.content.isEmpty()) {
        fincept_add_usage(usage, final_turn);
        fincept_stream_answer(final_turn.content);
        return with_turn_usage(final_turn);
    }
    if (final_turn.cancelled)
        return final_turn; // Stop pressed during the close-out — keep the cancelled flag

    // Nothing usable — surface the failure instead of leaking tool markup.
    LlmResponse err;
    err.error = final_turn.error.isEmpty() ? QStringLiteral("Fincept LLM stopped without producing an answer.")
                                           : final_turn.error;
    return err;
}

} // namespace fincept::ai_chat
