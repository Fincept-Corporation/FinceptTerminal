#pragma once
// LlmRequestPolicy.h — internal header shared across LlmService TUs.
//
// `t_request_policy` is a thread_local override scoping per-request tool
// behaviour. Set by chat() / chat_streaming() workers via ToolPolicyGuard
// before invoking the request-builder helpers; restored on scope exit.
//
// Replaces the legacy pattern of mutating tools_enabled_ on the shared
// instance, which raced when the floating AiChatBubble and the AI Chat tab
// ran concurrently and the bubble's "restore" leaked tools=false back to
// the tab.
//
//   ToolPolicy::All           — attach the global tool catalog as-is.
//   ToolPolicy::NoNavigation  — attach tools but exclude `navigation` (floating
//                               bubble: model can call benign tools without
//                               redirecting the user's active screen).
//   ToolPolicy::None          — attach no tools at all (legacy use_tools=false).

#include "core/config/AppConfig.h"
#include "core/logging/Logger.h"
#include "mcp/McpProvider.h"
#include "mcp/McpService.h"
#include "mcp/McpTypes.h"
#include "mcp/ResultStore.h"
#include "mcp/TerminalMcpBridge.h"
#include "services/llm/LlmService.h"

#include <QElapsedTimer>
#include <QFuture>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSet>
#include <QString>
#include <QThread>

#include <QtConcurrent>

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <functional>
#include <utility>
#include <vector>

namespace fincept::ai_chat::detail {

inline thread_local LlmService::ToolPolicy t_request_policy = LlmService::ToolPolicy::All;

// Optional progress emitter — when set, the tool loop pushes per-round status
// ("Using tool X...") through this so the chat bubble paints in real-time
// during the otherwise-silent multi-second tool execution phase. Set by
// do_streaming_request before falling back into the tool loop, cleared on exit.
using ProgressEmitter = std::function<void(const QString&)>;
inline thread_local ProgressEmitter t_progress_emitter;

struct ProgressEmitterGuard {
    ProgressEmitter prev;
    explicit ProgressEmitterGuard(ProgressEmitter p) : prev(std::move(t_progress_emitter)) {
        t_progress_emitter = std::move(p);
    }
    ~ProgressEmitterGuard() { t_progress_emitter = std::move(prev); }
    ProgressEmitterGuard(const ProgressEmitterGuard&) = delete;
    ProgressEmitterGuard& operator=(const ProgressEmitterGuard&) = delete;
};

inline void emit_progress(const QString& text) {
    if (t_progress_emitter)
        t_progress_emitter(text);
}

/// Human-readable one-line label for a tool call.
///
/// The core tools get plain-English wording plus the argument that tells one
/// call from the next — without it, six ui_act calls all render as "ui_act".
inline QString tool_progress_label(const QString& display_name, const QJsonObject& args) {
    auto arg = [&args](const char* key) { return args.value(QLatin1String(key)).toString().trimmed().left(40); };
    auto with = [](const QString& label, const QString& detail) {
        return detail.isEmpty() ? label : label + QStringLiteral(" · ") + detail;
    };
    if (display_name == QLatin1String("ui_overview"))
        return QStringLiteral("Looking at the terminal");
    if (display_name == QLatin1String("ui_open"))
        return with(QStringLiteral("Opening"),
                    arg("screen") + (arg("symbol").isEmpty() ? QString() : QLatin1Char(' ') + arg("symbol")));
    if (display_name == QLatin1String("ui_snapshot"))
        return with(QStringLiteral("Reading the screen"), arg("query").isEmpty() ? arg("target") : arg("query"));
    if (display_name == QLatin1String("ui_act")) {
        if (args.value(QStringLiteral("steps")).isArray())
            return QStringLiteral("Working the screen · %1 steps")
                .arg(args.value(QStringLiteral("steps")).toArray().size());
        return with(arg("action").isEmpty() ? QStringLiteral("Acting") : arg("action"),
                    arg("name").isEmpty() ? arg("ref") : arg("name"));
    }
    if (display_name == QLatin1String("ui_read"))
        return with(QStringLiteral("Reading"), arg("ref"));
    if (display_name == QLatin1String("ui_wait"))
        return with(QStringLiteral("Waiting"),
                    arg("until") + (arg("text").isEmpty() ? QString() : QLatin1Char(' ') + arg("text")));
    if (display_name == QLatin1String("ui_screenshot"))
        return with(QStringLiteral("Screenshot"), arg("target"));
    if (display_name == QLatin1String("ui_workspace"))
        return with(QStringLiteral("Arranging"), arg("action"));
    if (display_name == QLatin1String("ui_invoke"))
        return with(QStringLiteral("Running command"), arg("action"));
    if (display_name == QLatin1String("python")) {
        // A job id leads for status / cancel: the chat's Tools card tracks the job live from it.
        const QString detail = !arg("job_id").isEmpty()   ? arg("job_id")
                               : !arg("script").isEmpty() ? arg("script")
                                                          : arg("query");
        return with(QStringLiteral("Python ") + arg("action"), detail);
    }

    // Real tools: name plus the first short STRING argument as context
    // ("get_news · MARKETS"). Positional indices are deliberately excluded — a
    // run of set_excel_cell calls rendered as "· 0 / · 1 / · 3 / · 1 …", which
    // tells the reader nothing and defeats run-collapsing downstream. Numbers in
    // general make poor labels, so only strings qualify.
    static const QSet<QString> kPositionalKeys = {
        QStringLiteral("sheet_index"), QStringLiteral("row"),   QStringLiteral("col"),
        QStringLiteral("column"),      QStringLiteral("index"), QStringLiteral("limit"),
        QStringLiteral("offset"),      QStringLiteral("top_k"), QStringLiteral("count"),
    };
    for (auto it = args.constBegin(); it != args.constEnd(); ++it) {
        if (!it.value().isString() || kPositionalKeys.contains(it.key().toLower()))
            continue;
        const QString v = it.value().toString().trimmed();
        if (v.isEmpty() || v.size() > 40 || v.contains(QLatin1Char('\n')))
            continue;
        return display_name + QStringLiteral(" · ") + v;
    }
    return display_name;
}

/// Emit one tool-progress line on the dedicated tool channel.
///
/// Every call is emitted; collapsing a run of the same tool into "name ×N" is
/// the consumer's job, because only the consumer can rewrite a line it already
/// rendered. Suppressing here would have cost the count entirely.
inline void emit_tool_progress(const QString& display_name, const QJsonObject& args) {
    if (!t_progress_emitter)
        return;
    t_progress_emitter(tool_stream_prefix() + tool_progress_label(display_name, args) + QStringLiteral("\n"));
}

// Active chat session id for the in-flight LLM request. Set by the chat screen
// before calling chat_streaming(), read by MCP tools (e.g. report_session_context)
// to scope per-chat state — for example, "did this chat session already start a
// report?" so the model knows whether to continue or start fresh.
inline thread_local QString t_chat_session_id;

struct ChatSessionGuard {
    QString prev;
    explicit ChatSessionGuard(QString id) : prev(std::move(t_chat_session_id)) { t_chat_session_id = std::move(id); }
    ~ChatSessionGuard() { t_chat_session_id = std::move(prev); }
    ChatSessionGuard(const ChatSessionGuard&) = delete;
    ChatSessionGuard& operator=(const ChatSessionGuard&) = delete;
};

// ── Cooperative cancellation ─────────────────────────────────────────────────
//
// A turn can run for minutes (reasoning model + multi-round tool loop) and the
// request threads were uninterruptible: the only way out was to wait for the
// 10-minute loop deadline. LlmService::cancel_active_request() bumps a
// generation counter; every worker thread records the value when its request
// starts (CancelScope) and polls it at the places that can block — the SSE read
// loop, each blocking POST, the Fincept poll loop and the top of every tool
// round. A request that has not armed a scope (any other thread) is never
// reported as cancelled, so a stray call can't abort unrelated work.
inline std::atomic<quint64> g_llm_cancel_generation{0};
inline thread_local bool t_cancel_armed = false;
inline thread_local quint64 t_cancel_baseline = 0;

inline bool cancel_requested() {
    return t_cancel_armed && g_llm_cancel_generation.load(std::memory_order_acquire) != t_cancel_baseline;
}

struct CancelScope {
    bool prev_armed;
    quint64 prev_baseline;
    CancelScope() : prev_armed(t_cancel_armed), prev_baseline(t_cancel_baseline) {
        t_cancel_armed = true;
        t_cancel_baseline = g_llm_cancel_generation.load(std::memory_order_acquire);
    }
    ~CancelScope() {
        t_cancel_armed = prev_armed;
        t_cancel_baseline = prev_baseline;
    }
    CancelScope(const CancelScope&) = delete;
    CancelScope& operator=(const CancelScope&) = delete;
};

/// Sleep `ms` in short slices so a cancel lands within ~100 ms. Returns false
/// if the request was cancelled while waiting.
inline bool cancellable_sleep(int ms) {
    int left = ms;
    while (left > 0) {
        if (cancel_requested())
            return false;
        const int slice = std::min(left, 100);
        QThread::msleep(static_cast<unsigned long>(slice));
        left -= slice;
    }
    return !cancel_requested();
}

struct ToolPolicyGuard {
    LlmService::ToolPolicy prev;
    explicit ToolPolicyGuard(LlmService::ToolPolicy p) : prev(t_request_policy) { t_request_policy = p; }
    ~ToolPolicyGuard() { t_request_policy = prev; }
    ToolPolicyGuard(const ToolPolicyGuard&) = delete;
    ToolPolicyGuard& operator=(const ToolPolicyGuard&) = delete;
};

inline bool effective_tools_enabled(bool global_tools_enabled) {
    return global_tools_enabled && t_request_policy != LlmService::ToolPolicy::None;
}

inline bool should_hide_navigation() {
    return t_request_policy == LlmService::ToolPolicy::NoNavigation;
}

inline mcp::ToolFilter apply_request_policy(const mcp::ToolFilter& base) {
    mcp::ToolFilter out = base;
    if (should_hide_navigation() && !out.exclude_categories.contains(QStringLiteral("navigation")))
        out.exclude_categories.append(QStringLiteral("navigation"));
    return out;
}

// True for OpenAI-family chat model ids (gpt-*, chatgpt-*, o1/o3/o4 reasoning
// series). Used by pass-through aggregators (AIHubMix) that forward parameters
// unchanged: these models reject the legacy `max_tokens` field and require
// `max_completion_tokens` (o-series / gpt-5 return a 400 otherwise), whereas
// non-OpenAI models (claude-*, gemini-*, deepseek-*, qwen-*, …) keep max_tokens.
// `model_lower` must already be lower-cased by the caller.
inline bool is_openai_family_model(const QString& model_lower) {
    return model_lower.startsWith(QLatin1String("gpt-")) || model_lower.startsWith(QLatin1String("chatgpt")) ||
           model_lower.startsWith(QLatin1String("o1")) || model_lower.startsWith(QLatin1String("o3")) ||
           model_lower.startsWith(QLatin1String("o4"));
}

// ── Tool-result size discipline ───────────────────────────────────────────
//
// Tool results used to be spliced into the transcript verbatim, with no cap on
// any of the four provider paths (OpenAI first round, OpenAI tool loop,
// Anthropic tool loop, Gemini tool loop). Because every round re-posts the
// whole message array, one oversized result is re-billed as input on every
// remaining round of the turn — up to 200 of them (the `max_tool_rounds`
// clamp). A single unshaped `edgar_get_filing` could dominate a turn's cost.
//
// The cap is a budget, not a guillotine: whatever doesn't fit is parked in the
// ResultStore and the envelope tells the model how to page it back with
// `ui_read(ref=<result id>)`. Detail is available on demand instead of by default.

/// Byte ceiling for a single tool result in the transcript. 8 KB carries a
/// realistic table or article list intact; beyond that the overflow store
/// takes over. Overridable via `mcp/tool_result_max_chars`.
inline int tool_result_budget_bytes() {
    return std::clamp(AppConfig::instance().get("mcp/tool_result_max_chars", QVariant(8000)).toInt(), 512, 200000);
}

/// Shrink `payload` to the budget if needed, parking the original in the
/// ResultStore and annotating the envelope with `result_id` + retrieval note.
/// No-op when the payload already fits.
inline void fit_llm_payload(const QString& tool_name, QJsonObject& payload) {
    const QByteArray compact = QJsonDocument(payload).toJson(QJsonDocument::Compact);
    const int budget = tool_result_budget_bytes();
    if (compact.size() <= budget)
        return;

    const QString result_id = mcp::ResultStore::instance().put(tool_name, payload);

    // Shrink the whole envelope rather than just `data`: some tools carry their
    // bulk in `message` instead, and shaping only one leaves the other free to
    // blow the budget. Reserve headroom for the keys added below.
    QJsonValue shaped = QJsonValue(payload);
    const mcp::ShrinkStats st = mcp::shrink_json(shaped, std::max(512, budget - 600));

    payload = shaped.isObject() ? shaped.toObject() : QJsonObject{{"data", shaped}};
    payload["truncated"] = true;
    payload["result_id"] = result_id;
    payload["full_bytes"] = st.original_bytes;
    payload["note"] = QStringLiteral("Result truncated to fit context (%1 → %2 bytes). Page the full payload "
                                     "with ui_read(ref='%3', offset=0).")
                          .arg(st.original_bytes)
                          .arg(st.final_bytes)
                          .arg(result_id);

    LOG_INFO("LlmService", QString("Tool '%1' returned %2 KB — shaped to %3 B, full payload parked as %4")
                               .arg(tool_name)
                               .arg(st.original_bytes / 1024)
                               .arg(st.final_bytes)
                               .arg(result_id));
}

/// Envelope for a tool result, shaped to the transcript budget.
inline QJsonObject shape_tool_result_for_llm(const QString& tool_name, const mcp::ToolResult& tr) {
    QJsonObject envelope = tr.to_json();
    fit_llm_payload(tool_name, envelope);
    return envelope;
}

/// Same, serialised — for the providers whose tool messages take a string.
inline QString encode_tool_result_for_llm(const QString& tool_name, const mcp::ToolResult& tr) {
    return QString::fromUtf8(QJsonDocument(shape_tool_result_for_llm(tool_name, tr)).toJson(QJsonDocument::Compact));
}

// —— Tool-loop budget ———
//
// `max_tool_rounds` bounds how many times the loop iterates, which is not the
// same as bounding how long it runs. One round can start a 300 s background
// job and long-poll it for 30 s, so the default 40 rounds is, in wall-clock
// terms, unbounded — and the user watching a chat bubble has no way to tell a
// working turn from a wedged one. The two budgets answer different questions
// and a loop needs both: rounds cap the token spend, the deadline caps the
// wait.
//
// Exhausting either is a normal outcome, not an error, but it must be VISIBLE.
// The loop used to stop and hand back whatever it had, so a turn that ran out
// of budget mid-task was indistinguishable from one that finished — the same
// failure shape as a silently truncated tool result (see §M4).

/// Wall-clock ceiling for one turn's tool loop, in ms. 0 disables it.
/// Ten minutes is well past any legitimate interactive turn while still short
/// enough that a stuck loop surfaces during the session rather than after it.
inline int tool_loop_deadline_ms() {
    return std::max(0, AppConfig::instance().get("mcp/tool_loop_deadline_ms", QVariant(600000)).toInt());
}

class ToolLoopBudget {
  public:
    explicit ToolLoopBudget(int max_rounds) : max_rounds_(max_rounds), deadline_ms_(tool_loop_deadline_ms()) {
        timer_.start();
    }

    /// Check at the top of every round, before issuing the next request.
    bool exhausted() const { return reason() != Reason::None; }

    void note_round() { ++rounds_; }
    int rounds_used() const { return rounds_; }
    qint64 elapsed_ms() const { return timer_.elapsed(); }

    /// One line naming which budget ran out and what it cost, for the log and
    /// for the model's final-summary prompt. Empty while the loop is healthy.
    QString exhaustion_note() const {
        switch (reason()) {
            case Reason::Rounds:
                return QStringLiteral("tool-call budget spent: %1 of %1 rounds used in %2 s")
                    .arg(rounds_)
                    .arg(elapsed_ms() / 1000);
            case Reason::Deadline:
                return QStringLiteral("time budget spent: %1 s elapsed (limit %2 s) after %3 round(s)")
                    .arg(elapsed_ms() / 1000)
                    .arg(deadline_ms_ / 1000)
                    .arg(rounds_);
            case Reason::None:
                break;
        }
        return {};
    }

  private:
    enum class Reason { None, Rounds, Deadline };

    Reason reason() const {
        if (max_rounds_ > 0 && rounds_ >= max_rounds_)
            return Reason::Rounds;
        if (deadline_ms_ > 0 && timer_.elapsed() >= deadline_ms_)
            return Reason::Deadline;
        return Reason::None;
    }

    QElapsedTimer timer_;
    int max_rounds_ = 0;
    int deadline_ms_ = 0;
    int rounds_ = 0;
};

// —— Parallel tool execution ———
//
// A model that asks for six quotes emits six tool_calls in ONE assistant turn,
// and every loop here executed them one after another — six sequential round
// trips through Python or HTTP for work with no dependency between the calls.
// The async dispatcher built to fix that (McpService::execute_openai_function_async,
// "Phase 5 will join these with QtFuture::whenAll") had no callers at all.
//
// Two things stop this from being a plain std::transform over a thread pool:
//
// 1. ORDER IS OBSERVABLE. `is_destructive` tools mutate terminal state, and a
//    round of [write_cell, read_cell] means something different if the read
//    lands first. So a destructive call is a BARRIER: consecutive read-only
//    calls fan out together, a destructive call runs alone, and the model's
//    relative ordering is preserved exactly. Rounds that are all reads — the
//    overwhelmingly common case — parallelise completely; a round with writes
//    degrades gracefully toward sequential rather than reordering anything.
//
// 2. TOOLS READ THREAD-LOCAL STATE. The destructive-tool gate reads
//    TerminalMcpBridge's thread_local flags, and four report-builder tools read
//    `t_chat_session_id`. Both default to empty/false on a pool thread, which
//    would silently reclassify an agent call as a chat call and detach report
//    tools from their session. Every worker re-establishes both.
//
// Results come back index-aligned with the input, so callers append tool
// messages in the model's original order whatever order they finished in.

/// One tool call from a round, provider-agnostic.
struct PendingToolCall {
    QString wire_name;  // "<server>__<tool>", as the model emitted it
    QString display;    // bare name, for progress lines and activation tracking
    QJsonObject args;
    QString call_id;    // tool_call_id / tool_use_id; opaque here, echoed by the caller
};

/// Max tool calls executed concurrently within one round. 1 restores the old
/// strictly-sequential behaviour and is the kill switch.
///
/// The default is deliberately low. PythonRunner caps itself at 3 concurrent
/// processes, so a wider fan-out on Python-backed tools just moves the queue
/// rather than shortening it, while still multiplying peak memory.
inline int tool_fanout() {
    return std::clamp(AppConfig::instance().get("mcp/tool_fanout", QVariant(4)).toInt(), 1, 16);
}

/// True if the tool mutates terminal state.
///
/// Resolved through `McpProvider::find_tool`, an O(1) snapshot lookup. The
/// obvious alternative — scanning `McpService::get_all_tools()` — returns the
/// whole catalogue BY VALUE, so a six-call round would copy 926 tool records,
/// each carrying a serialised JSON schema, six times over, to answer six
/// booleans.
///
/// `find_tool` only knows internal tools, and everything it does not know is
/// treated as destructive. That is the right default twice over: an
/// unrecognised name is more likely a stale alias than a proven read, and
/// external MCP tools are ALREADY gated destructive-by-default in
/// `McpService::execute_tool` because the MCP wire carries no destructiveness
/// metadata. Serialising them here matches the security posture they are
/// executed under. The cost is that a round of external calls does not fan
/// out; correctness first.
inline bool tool_is_destructive(const QString& wire_name) {
    const QString bare = mcp::McpProvider::parse_openai_function_name(wire_name).second;
    if (bare.isEmpty())
        return true;
    const auto t = mcp::McpProvider::instance().find_tool(bare);
    if (!t)
        return true;
    // Tools that change what the screen shows (ui_act, ui_invoke, ui_open,
    // ui_workspace) are barriers too: two of them racing would interleave
    // their steps on the GUI thread in an order the model never asked for.
    return t->is_destructive || t->category == QLatin1String("ui-control") ||
           t->category == QLatin1String("navigation");
}

/// Split a round into execution batches, preserving the model's ordering.
///
/// Each batch is a half-open `[begin, end)` range over the round's calls, and
/// batches run strictly in order. A batch with more than one element is safe to
/// run concurrently; a destructive call always lands in a batch of its own, so
/// nothing can overtake it and it can overtake nothing.
///
/// Pure and separated from execution because the ordering rule is the part that
/// can be wrong in a way no build error catches: get it subtly off and a write
/// silently overtakes a read, in production, occasionally. This shape is
/// exhaustively checked by the tool self-test.
inline std::vector<std::pair<std::size_t, std::size_t>> plan_tool_batches(const std::vector<bool>& destructive,
                                                                         int fanout) {
    std::vector<std::pair<std::size_t, std::size_t>> batches;
    const std::size_t n = destructive.size();
    std::size_t i = 0;
    while (i < n) {
        if (fanout <= 1 || destructive[i]) {
            batches.emplace_back(i, i + 1); // barrier, or fan-out disabled
            ++i;
            continue;
        }
        std::size_t end = i;
        while (end < n && static_cast<int>(end - i) < fanout && !destructive[end])
            ++end;
        batches.emplace_back(i, end);
        i = end;
    }
    return batches;
}

/// Execute one round's tool calls, honouring the ordering rules above.
/// Returns results index-aligned with `calls`.
inline std::vector<mcp::ToolResult> execute_tool_calls(const std::vector<PendingToolCall>& calls) {
    std::vector<mcp::ToolResult> out(calls.size());
    if (calls.empty())
        return out;

    std::vector<bool> destructive;
    destructive.reserve(calls.size());
    for (const auto& c : calls)
        destructive.push_back(tool_is_destructive(c.wire_name));
    const auto batches = plan_tool_batches(destructive, tool_fanout());

    // Capture the thread_local context ONCE, on the loop thread, and hand it to
    // every worker. Reading it inside the worker would read the pool thread's
    // defaults, which is the bug this exists to prevent.
    const bool call_in_progress = mcp::TerminalMcpBridge::is_call_in_progress();
    const bool destructive_allowed = mcp::TerminalMcpBridge::is_destructive_allowed();
    const QString session_id = t_chat_session_id;

    for (const auto& [begin, end] : batches) {
        if (end - begin == 1) {
            // In-thread: a single call gains nothing from a pool hop, and this
            // keeps the common one-call round byte-for-byte what it was.
            out[begin] = mcp::McpService::instance().execute_openai_function(calls[begin].wire_name, calls[begin].args);
            continue;
        }
        std::vector<QFuture<mcp::ToolResult>> futures;
        futures.reserve(end - begin);
        for (std::size_t k = begin; k < end; ++k) {
            futures.push_back(QtConcurrent::run(
                [call = calls[k], call_in_progress, destructive_allowed, session_id]() -> mcp::ToolResult {
                    mcp::TerminalMcpBridge::ScopedCallFlags flags(call_in_progress, destructive_allowed);
                    ChatSessionGuard session(session_id);
                    return mcp::McpService::instance().execute_openai_function(call.wire_name, call.args);
                }));
        }
        for (std::size_t k = begin; k < end; ++k) {
            auto& f = futures[k - begin];
            f.waitForFinished();
            out[k] = f.resultCount() > 0 ? f.result() : mcp::ToolResult::fail("Tool produced no result");
        }
    }
    return out;
}

} // namespace fincept::ai_chat::detail
