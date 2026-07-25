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
#include "mcp/McpTypes.h"
#include "mcp/ResultStore.h"
#include "services/llm/LlmService.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSet>
#include <QString>

#include <algorithm>
#include <functional>

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

// ── Tool RAG activation ──────────────────────────────────────────────────
//
// When Tool RAG is on, the model is only sent the ~7 Tier-0 tools each turn and
// discovers the rest via tool_list / tool_describe. Those meta-tools hand back
// the discovered tool as *text* — but a structured function-calling model
// (Kimi, OpenAI, Groq, …) can only emit a tool_call for a function actually
// DECLARED in the request's `tools` array. Without re-declaring what it found,
// the model stalls right after tool_describe ("I don't have that tool loaded"),
// which is exactly the failure mode that breaks agentic actions like
// create_portfolio.
//
// This mirrors Anthropic's Tool Search Tool: the discovered tool definition
// must be injected into the active tool set. We accumulate the bare names the
// model surfaces (via tool_list) or commits to (via tool_describe) across the
// turn, and feed them back into format_tools_for_openai so they become real,
// callable functions on the next round.
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
// `result_fetch`. Detail is available on demand instead of by default.

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
    payload["note"] = QStringLiteral("Result truncated to fit context (%1 → %2 bytes). Call "
                                     "result_fetch(result_id='%3', offset=0, limit=6000) to page the full payload.")
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

// Ceiling on how many discovered tools stay declared at once.
//
// Every activated tool re-enters the `tools` array on every subsequent round,
// at full schema size. Left unbounded (the previous behaviour), a turn that
// makes eight tool_list calls has ~40 extra schemas riding along by mid-turn —
// which is roughly the whole catalogue Tier-0 exists to avoid sending, so the
// ~85% saving quietly evaporates exactly when the turn is longest.
//
// 24 is comfortably above what any single turn genuinely juggles (top_k is
// capped at 20 and defaults to 5) while keeping the tail bounded.
inline constexpr int kMaxActivatedTools = 24;

/// Insertion-ordered, capped set of tools the model has discovered this turn.
/// Evicts least-recently-activated first, so a tool the model keeps coming back
/// to survives while stale candidates from an early search age out.
class ActivationTracker {
  public:
    ActivationTracker() = default;
    explicit ActivationTracker(const QSet<QString>& seed) {
        for (const auto& n : seed)
            add(n);
    }

    void add(const QString& name) {
        if (name.isEmpty())
            return;
        if (set_.contains(name)) {
            order_.removeOne(name); // re-activation refreshes recency
            order_.append(name);
            return;
        }
        set_.insert(name);
        order_.append(name);
        while (order_.size() > kMaxActivatedTools)
            set_.remove(order_.takeFirst());
    }

    const QSet<QString>& names() const { return set_; }
    int size() const { return set_.size(); }

  private:
    QStringList order_; // least-recently-activated at the front
    QSet<QString> set_;
};

inline void note_tool_activations(const QString& bare_tool_name, const QJsonObject& args, const mcp::ToolResult& result,
                                  ActivationTracker& activated) {
    if (!result.success)
        return;
    if (bare_tool_name == QLatin1String("tool_list")) {
        // Activate every candidate the search surfaced — the model may pick any.
        const QJsonArray rows = result.data.toObject().value("tools").toArray();
        for (const auto& row : rows)
            activated.add(row.toObject().value("name").toString());
    } else if (bare_tool_name == QLatin1String("tool_describe")) {
        // The model committed to one tool — prefer the canonical name echoed in
        // the result, fall back to the name it asked about. Adding it last also
        // makes it the most-recently-used entry, so it outlives the rest of the
        // search batch it came from.
        QString name = result.data.toObject().value("name").toString();
        if (name.isEmpty())
            name = args.value("name").toString();
        activated.add(name);
    }
}

} // namespace fincept::ai_chat::detail
