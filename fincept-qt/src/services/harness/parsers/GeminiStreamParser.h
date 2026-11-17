#pragma once
// GeminiStreamParser.h — Google Gemini CLI `gemini -p … -o stream-json`.
//
// Event types and fields exactly as gemini-cli defines them
// (packages/core/src/output/types.ts, JsonStreamEventType):
//   init        {session_id, model}
//   message     {role: user | assistant, content, delta?}
//   tool_use    {tool_name, tool_id, parameters}
//   tool_result {tool_id, status: success | error, output?, error?{type, message}}
//   error       {severity: warning | error, message}
//   result      {status: success | error, error?{type, message},
//                stats?{total_tokens, input_tokens, output_tokens, cached, input,
//                       duration_ms, tool_calls, models}}
// Assistant messages with delta:true are streamed as partial Text events and
// completed when another event arrives (or in finish()); a non-delta assistant
// message is complete on its own. The prompt echo (role user) is not shown.

#include "services/harness/IHarnessAdapter.h"
#include "services/harness/parsers/StreamParserSupport.h"

#include <QHash>
#include <QString>

namespace fincept::harness {

class GeminiStreamParser final : public IStreamParser {
  public:
    QVector<services::coding_agents::AgentEvent> parse_line(const QString& line) override;
    QVector<services::coding_agents::AgentEvent> finish() override;
    const services::coding_agents::StreamFacts& facts() const override { return facts_; }

  private:
    void on_message(const QJsonObject& o, const QString& raw, QVector<services::coding_agents::AgentEvent>& out);
    void on_result(const QJsonObject& o, const QString& raw, QVector<services::coding_agents::AgentEvent>& out);
    void flush(QVector<services::coding_agents::AgentEvent>& out);

    services::coding_agents::StreamFacts facts_;
    stream_support::DeltaAccumulator acc_;
    QHash<QString, QString> tool_names_; // tool_id -> tool_name
    int message_counter_ = 0;
};

} // namespace fincept::harness
