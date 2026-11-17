#pragma once
// CursorStreamParser.h — Cursor's agent CLI `agent -p … --output-format stream-json`
// (cursor.com/docs/cli/reference/output-format):
//   system   {subtype: init, session_id, model, apiKeySource, cwd, permissionMode}
//   user     {message{role, content[{type: text, text}]}}          — the prompt echo
//   assistant{message{role, content[{type: text, text}]}, session_id} — one complete
//            message between tool calls (no --stream-partial-output is requested)
//   tool_call{subtype: started | completed, call_id,
//             tool_call{readToolCall | writeToolCall | …: {args, result?}} or
//             tool_call{function{name, arguments}}}
//   result   {subtype: success, is_error, duration_ms, result, session_id}
// Thinking is suppressed in print mode, there is no usage or cost, and on an
// error the stream may end WITHOUT a result event (the error goes to stderr) —
// success therefore means exit code 0 and a result.

#include "services/harness/IHarnessAdapter.h"
#include "services/harness/parsers/StreamParserSupport.h"

#include <QHash>
#include <QString>

namespace fincept::harness {

class CursorStreamParser final : public IStreamParser {
  public:
    QVector<services::coding_agents::AgentEvent> parse_line(const QString& line) override;
    const services::coding_agents::StreamFacts& facts() const override { return facts_; }

  private:
    void on_tool_call(const QJsonObject& o, const QString& raw, QVector<services::coding_agents::AgentEvent>& out);
    void on_result(const QJsonObject& o, const QString& raw, QVector<services::coding_agents::AgentEvent>& out);

    services::coding_agents::StreamFacts facts_;
    QHash<QString, QString> tool_names_; // call_id -> display name
};

} // namespace fincept::harness
