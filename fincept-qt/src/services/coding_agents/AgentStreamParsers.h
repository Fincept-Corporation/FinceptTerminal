#pragma once
// AgentStreamParsers.h — turn the machine-readable stdout of the two supported
// agent CLIs into transcript events.
//
//   ClaudeStreamParser — `claude -p --output-format stream-json --verbose`
//   CodexJsonParser    — `codex exec --json`
//
// Each stdout LINE is one JSON document. parse_line() returns 0..n events and
// updates facts() (session id, cost, tokens, final answer). Both parsers are
// defensive by contract: an unknown event type or field is rendered as a
// generic status event, a malformed line becomes a status event carrying the
// raw text, and nothing ever throws or asserts. No widgets, no I/O — QtCore only.

#include "services/coding_agents/CodingAgentTypes.h"

#include <QByteArray>
#include <QHash>
#include <QJsonObject>
#include <QList>
#include <QString>
#include <QVector>

#include <optional>

namespace fincept::services::coding_agents {

/// Splits a byte stream into complete lines and keeps the trailing partial
/// line until the next chunk (a JSON event is often split across reads).
class LineBuffer {
  public:
    /// Append bytes; returns every complete line (without "\n" / "\r\n").
    QList<QByteArray> feed(const QByteArray& chunk);
    /// The unterminated remainder — call once when the process has exited.
    QByteArray take_rest();
    bool empty() const { return buf_.isEmpty(); }

    /// A partial line larger than this is emitted as-is rather than buffered
    /// without bound (a runaway writer must not exhaust memory).
    static constexpr qsizetype kMaxLineBytes = 32 * 1024 * 1024;

  private:
    QByteArray buf_;
};

/// What a parser has learned from the stream so far.
struct StreamFacts {
    QString session_id; // Claude session_id / Codex thread_id
    QString model;
    std::optional<double> cost_usd;
    qint64 input_tokens = 0;
    qint64 output_tokens = 0;
    QString final_text;      // Claude: result.result; Codex: last agent_message
    bool saw_result = false; // Claude: a `result` arrived; Codex: a turn completed or failed
    bool result_is_error = false;
    QString result_error;    // error from `result` / `turn.failed`
    QString last_error;      // most recent non-fatal error text (Codex `error`, API errors)
    int num_turns = 0;
    int tool_calls = 0;
    QStringList mcp_problems; // MCP servers that did not connect (Claude init)
};

class ClaudeStreamParser {
  public:
    QVector<AgentEvent> parse_line(const QString& line);
    const StreamFacts& facts() const { return facts_; }

  private:
    void on_system(const QJsonObject& o, const QString& raw, QVector<AgentEvent>& out);
    void on_assistant(const QJsonObject& o, QVector<AgentEvent>& out);
    void on_user(const QJsonObject& o, QVector<AgentEvent>& out);
    void on_result(const QJsonObject& o, const QString& raw, QVector<AgentEvent>& out);

    StreamFacts facts_;
    QHash<QString, QString> tool_names_; // tool_use_id -> tool name
};

class CodexJsonParser {
  public:
    QVector<AgentEvent> parse_line(const QString& line);
    const StreamFacts& facts() const { return facts_; }

  private:
    void on_item(const QString& phase, const QJsonObject& item, QVector<AgentEvent>& out);

    StreamFacts facts_;
};

/// "mcp__fincept__quote_get" → "fincept.quote_get"; other names unchanged.
QString display_tool_name(const QString& name);

} // namespace fincept::services::coding_agents
