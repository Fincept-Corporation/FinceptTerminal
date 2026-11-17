#pragma once
// AcpUpdateParser.h — NDJSON where each line carries ONE Agent Client Protocol
// session update (agentclientprotocol.com/protocol/schema), e.g. Grok Build's
// `--output-format streaming-json`. Generic: every ACP harness can reuse it.
//
// Accepted line shapes (whichever a harness prints):
//   {"jsonrpc":"2.0","method":"session/update","params":{"sessionId":…,"update":{…}}}
//   {"sessionId":…,"update":{…}}                       — the bare notification params
//   {"sessionUpdate":"…",…}                             — a bare SessionUpdate
//   {"jsonrpc":"2.0","id":…,"result":{"stopReason":…,"usage":{…}}}  — the prompt response
//   {"jsonrpc":"2.0","id":…,"error":{"code":…,"message":…}}
//   {"stopReason":…}                                    — a bare PromptResponse
//
// Updates → events:
//   agent_message_chunk / agent_thought_chunk  → partial Text / Thinking events that
//       accumulate per message (messageId when given), then ONE complete event when
//       another content-bearing update arrives, the turn stops, or finish() runs
//   user_message_chunk                         → nothing (the prompt echoed back)
//   tool_call                                  → ToolCall (ref_id = toolCallId)
//   tool_call_update completed | failed        → ToolResult with the same ref_id;
//       an edit / delete / move tool, or "diff" content, also yields a FileChange
//   plan                                       → Status "Plan"
//   notice                                     → Status, or Error for severity "error"
//   usage_update                               → cost in facts (when in USD), no event
//   available_commands_update, config_option_update, session_info_update → nothing
//   current_mode_update                        → a terse status
//   stopReason                                 → saw_result; result_is_error unless "end_turn"
//   anything else                              → a status event with the raw summary
// Metadata updates (commands, modes, config, usage, session info) do not end a
// message being streamed; every other update does.

#include "services/harness/IHarnessAdapter.h"
#include "services/harness/parsers/StreamParserSupport.h"

#include <QHash>
#include <QJsonObject>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QVector>

namespace fincept::harness {

class AcpUpdateParser final : public IStreamParser {
  public:
    QVector<services::coding_agents::AgentEvent> parse_line(const QString& line) override;
    QVector<services::coding_agents::AgentEvent> finish() override;
    const services::coding_agents::StreamFacts& facts() const override { return facts_; }

  private:
    struct ToolState {
        QString title;
        QString kind;           // ACP ToolKind: read | edit | delete | move | search | execute | …
        QStringList locations;  // ToolCallLocation paths
        QStringList diff_paths; // "diff" content paths
        QStringList new_files;  // "diff" content without oldText
        bool announced = false; // ToolCall emitted
        bool finished = false;  // ToolResult emitted
    };

    void note_session(const QJsonObject& o);
    void on_update(const QJsonObject& update, const QString& raw, QVector<services::coding_agents::AgentEvent>& out);
    void on_chunk(services::coding_agents::EventKind kind, const QJsonObject& update,
                  QVector<services::coding_agents::AgentEvent>& out);
    void on_tool(const QJsonObject& update, bool is_new, const QString& raw,
                 QVector<services::coding_agents::AgentEvent>& out);
    void on_response(const QJsonObject& o, const QString& raw, QVector<services::coding_agents::AgentEvent>& out);
    void on_stop(const QString& reason, const QJsonObject& usage, const QString& raw,
                 QVector<services::coding_agents::AgentEvent>& out);
    void take_usage(const QJsonObject& usage, const QString& raw, QVector<services::coding_agents::AgentEvent>& out);
    void flush(QVector<services::coding_agents::AgentEvent>& out);

    services::coding_agents::StreamFacts facts_;
    stream_support::DeltaAccumulator acc_;
    QHash<QString, ToolState> tools_;
    int message_counter_ = 0;
};

} // namespace fincept::harness
