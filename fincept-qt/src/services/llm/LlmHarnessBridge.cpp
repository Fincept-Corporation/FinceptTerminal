// LlmHarnessBridge.cpp — LlmService's harness route ("harness:<id>"): one
// request answered by an agent harness through HarnessChatBackend, its
// channels mapped onto the chat's in-band chunk protocol so AI Chat's Thinking
// and Tools cards, the Quick Chat bubble and Quick Ask work unchanged:
//   answer text   → plain chunks
//   thinking      → think_stream_prefix() + text
//   narration     → "\x01__TOOL_CALL_CLEAR__" when the agent moves on to tools
//   tool call     → tool_stream_prefix() + label + "\n"           (start line)
//   tool result   → tool_stream_prefix() + encode_tool_done() + "\n" (completion record)

#include "core/logging/Logger.h"
#include "services/harness/HarnessChatBackend.h"
#include "services/llm/LlmRequestPolicy.h"
#include "services/llm/LlmService.h"
#include "services/llm/LlmToolActivity.h"
#include "services/llm/ModelRouter.h"
#include "services/llm/ProviderCatalog.h"

#include <QElapsedTimer>
#include <QHash>
#include <QJsonDocument>
#include <QJsonObject>

#include <limits>
#include <memory>

namespace fincept::ai_chat {

namespace llm_harness_bridge_detail {

/// Quick answers need no 10-minute budget: the arena and the tool picker are
/// one JSON reply; the chat features keep the plan's 10 minutes (tool loops).
int lhb_timeout_ms(const QString& feature) {
    if (feature == QLatin1String(kFeatureToolPicker))
        return 2 * 60 * 1000;
    if (feature == QLatin1String(kFeatureQuickAsk))
        return 5 * 60 * 1000;
    return 10 * 60 * 1000;
}

struct LhbToolStart {
    QString label;
    QElapsedTimer timer;
};

} // namespace llm_harness_bridge_detail

using namespace llm_harness_bridge_detail;

LlmResponse LlmService::do_harness_request(const QString& user_message, const std::vector<ConversationMessage>& history,
                                           const StreamCallback& on_chunk) {
    harness::HarnessChatRequest req;
    req.harness_id = ProviderCatalog::harness_id(provider_);
    req.model = model_;
    req.effort = harness_effort_;
    req.system_prompt = system_prompt_;
    req.user_message = user_message;
    for (const ConversationMessage& m : history)
        req.history.append(harness::ChatTurn{m.role, m.content});
    req.chat_session_id = detail::t_chat_session_id;
    req.feature = route_feature_;
    // The request's ToolPolicy (thread-local, set by chat / chat_streaming); a
    // feature that needs no tools (Quick Ask, the tool picker's JSON) gets none.
    const FeatureSpec* spec = feature_spec(route_feature_);
    if (!detail::effective_tools_enabled(tools_enabled_) || (spec && !spec->needs_tools))
        req.tools = harness::ChatTools::None;
    else
        req.tools = detail::should_hide_navigation() ? harness::ChatTools::NoNavigation : harness::ChatTools::All;
    req.require_api_key_source = harness_require_key_source_;
    req.timeout_ms = lhb_timeout_ms(route_feature_);

    // ── Channels → chunks ──
    auto tools = std::make_shared<QHash<QString, LhbToolStart>>();
    harness::HarnessChatSink sink;
    sink.text = [&on_chunk](const QString& delta) {
        if (on_chunk)
            on_chunk(delta, false);
    };
    sink.thinking = [&on_chunk](const QString& delta) {
        if (on_chunk)
            on_chunk(think_stream_prefix() + delta, false);
    };
    sink.text_reset = [&on_chunk]() {
        if (on_chunk)
            on_chunk(QStringLiteral("\x01__TOOL_CALL_CLEAR__"), false);
    };
    sink.tool_call = [&on_chunk, tools](const QString& id, const QString& tool, const QString& args_json) {
        const QJsonObject args = QJsonDocument::fromJson(args_json.toUtf8()).object();
        LhbToolStart start;
        start.label = detail::tool_progress_label(tool, args);
        start.timer.start();
        const QString label = start.label;
        if (!id.isEmpty())
            tools->insert(id, start);
        if (on_chunk)
            on_chunk(tool_stream_prefix() + label + QStringLiteral("\n"), false);
    };
    sink.tool_result = [&on_chunk, tools](const QString& id, const QString& tool, bool ok, const QString& body) {
        ToolDoneRecord r;
        const auto it = tools->constFind(id);
        r.label = it != tools->cend() ? it->label : tool;
        r.elapsed_ms = it != tools->cend() ? it->timer.elapsed() : -1;
        r.outcome = ok ? ToolDoneRecord::Outcome::Ok : ToolDoneRecord::Outcome::Failed;
        if (!ok)
            r.detail = body.left(240);
        if (on_chunk)
            on_chunk(tool_stream_prefix() + encode_tool_done(r) + QStringLiteral("\n"), false);
    };
    sink.cancelled = []() { return detail::cancel_requested(); };

    const harness::HarnessChatResult hr = harness::HarnessChatBackend::complete(req, sink);

    LlmResponse resp;
    resp.success = hr.success;
    resp.cancelled = hr.cancelled;
    resp.content = hr.content;
    resp.error = hr.error;
    resp.prompt_tokens = static_cast<int>(qMin<qint64>(hr.input_tokens, std::numeric_limits<int>::max()));
    resp.completion_tokens = static_cast<int>(qMin<qint64>(hr.output_tokens, std::numeric_limits<int>::max()));
    resp.total_tokens = resp.prompt_tokens + resp.completion_tokens;
    resp.model = hr.model;
    resp.cost_usd = hr.cost_usd.value_or(-1.0);
    resp.harness_failure = hr.failure;
    resp.harness_output_started = hr.output_started;
    resp.harness_subscription = hr.subscription_detected;
    return resp;
}

} // namespace fincept::ai_chat
