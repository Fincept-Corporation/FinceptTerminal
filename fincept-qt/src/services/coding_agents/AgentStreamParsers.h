#pragma once
// AgentStreamParsers.h — turn the machine-readable stdout of the print-mode
// agent CLIs into transcript events (the other harness parsers live in
// src/services/harness/parsers).
//
//   ClaudeStreamParser — `claude -p --output-format stream-json --verbose`
//                        (also Qwen Code `qwen -p -o stream-json`)
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
    qint64 input_tokens = 0;  // all input tokens, cached ones included
    qint64 output_tokens = 0;
    qint64 cached_tokens = 0; // input tokens read from the provider's cache (Claude cache_read, Codex cached_input)
    QString final_text;      // Claude: result.result; Codex: last agent_message
    bool saw_result = false; // Claude: a `result` arrived; Codex: a turn completed or failed
    bool result_is_error = false;
    QString result_error;    // error from `result` / `turn.failed`
    QString last_error;      // most recent non-fatal error text (Codex `error`, API errors)
    QString stop_reason;     // why the turn ended, when the harness says so ("end_turn", "max_turns", …)
    int num_turns = 0;
    int tool_calls = 0;
    QStringList mcp_problems; // MCP servers that did not connect (Claude init)
    /// Rate limiting reported in the stream (Claude rate_limit_event): status
    /// ("allowed", "allowed_warning", "rejected", …) and when the window resets
    /// (epoch ms; 0 = not reported). Feeds HarnessRegistry::report_failure().
    QString rate_limit_status;
    qint64 rate_limit_resets_at_ms = 0;
};

/// Also reads Qwen Code's `qwen -p … -o stream-json` (same event shapes; its
/// init message is system/session_start).
///
/// With `--include-partial-messages`, `stream_event` lines (Anthropic stream
/// events) become partial Text / Thinking events (AgentEvent::partial, ref_id =
/// "<message id>:<block index>"); the `assistant` message that follows stays the
/// one complete event and carries the same ref_id. Edit / Write / MultiEdit /
/// NotebookEdit calls that succeed also yield a FileChange event.
class ClaudeStreamParser {
  public:
    QVector<AgentEvent> parse_line(const QString& line);
    /// The process has exited: streamed text whose complete `assistant` message
    /// never arrived becomes a complete event.
    QVector<AgentEvent> finish();
    const StreamFacts& facts() const { return facts_; }

  private:
    void on_system(const QJsonObject& o, const QString& raw, QVector<AgentEvent>& out);
    void on_assistant(const QJsonObject& o, QVector<AgentEvent>& out);
    void on_user(const QJsonObject& o, QVector<AgentEvent>& out);
    void on_result(const QJsonObject& o, const QString& raw, QVector<AgentEvent>& out);
    void on_stream_event(const QJsonObject& o, QVector<AgentEvent>& out);
    void on_rate_limit(const QJsonObject& o, const QString& raw, QVector<AgentEvent>& out);
    void append_partial(int index, EventKind kind, const QString& delta, QVector<AgentEvent>& out);
    /// ref_id of the first streamed block of `kind` not yet completed (empty = none).
    QString take_partial_key(EventKind kind, const QString& message_id);
    /// Streamed blocks that never got their `assistant` message → complete events.
    void flush_partials(QVector<AgentEvent>& out);

    /// One content block streamed through stream_event deltas.
    struct PartialBlock {
        EventKind kind = EventKind::Text;
        QString key;           // ref_id of its partial events and of the complete event
        QString text;          // the block so far
        bool stopped = false;  // outgrew one event body: no further partials
        bool consumed = false; // its complete event was emitted
    };
    struct PendingEdit {
        QString kind; // "write" | "update"
        QString path;
    };

    StreamFacts facts_;
    QHash<QString, QString> tool_names_; // tool_use_id -> tool name
    QVector<PartialBlock> partial_blocks_; // the current API message's streamed blocks, in order
    QHash<int, int> partial_by_index_;     // stream content_block index -> partial_blocks_ position
    QString partial_message_id_;           // message.id from message_start (may be empty)
    QString partial_message_key_;          // message.id, or "msg-<n>" when it has none
    int partial_messages_ = 0;
    QHash<QString, PendingEdit> pending_edits_; // tool_use_id -> the file an edit tool changes
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
