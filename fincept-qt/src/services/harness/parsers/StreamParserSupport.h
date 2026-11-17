#pragma once
// StreamParserSupport.h — helpers shared by the harness stream parsers in
// src/services/harness/parsers (ACP session updates, Gemini CLI, Cursor, plain
// text): event construction with the transcript limits applied, defensive JSON
// reading, and DeltaAccumulator, which assembles one streamed message from
// text deltas and reports it as partial events followed by one complete event.
//
// QtCore only. Never throws; every helper degrades to an empty value.

#include "services/coding_agents/AgentStreamParsers.h"
#include "services/coding_agents/CodingAgentTypes.h"

#include <QJsonObject>
#include <QJsonValue>
#include <QString>
#include <QStringList>
#include <QVector>

#include <initializer_list>
#include <optional>

namespace fincept::harness::stream_support {

/// One transcript event; title simplified and capped, body and raw clipped to
/// the transcript limits (kAgentEventBodyMaxBytes / kAgentEventRawMaxBytes).
services::coding_agents::AgentEvent make_event(services::coding_agents::EventKind kind, const QString& title,
                                               const QString& body = {}, bool is_error = false,
                                               const QString& ref_id = {}, const QString& raw = {});

/// Compact JSON of an object / array; the text of a scalar; empty for null / undefined.
QString compact_json(const QJsonValue& v);
/// Not null / undefined / an empty string, array or object.
bool has_json_value(const QJsonValue& v);
/// Readable text of a content block, an array of blocks, or an MCP CallToolResult
/// ({content:[…]}): text blocks verbatim, images and resources as short markers.
QString content_text(const QJsonValue& content);
/// Compact JSON of `o` without `skip_keys`, clipped to `max_bytes`.
QString json_summary(const QJsonObject& o, const QStringList& skip_keys, int max_bytes);
/// "1,234,567".
QString grouped_number(qint64 v);
/// The first of `keys` that holds a number, as an integer; 0 when none does.
qint64 json_count(const QJsonObject& o, std::initializer_list<const char*> keys);
/// The first of `keys` that holds a non-empty string.
QString json_string(const QJsonObject& o, std::initializer_list<const char*> keys);
/// `trimmed` parsed as one JSON object. False for anything else (not JSON, an array, a scalar).
bool parse_json_object(const QString& trimmed, QJsonObject& out);
/// A stdout line that reads like an error message (used for non-JSON lines).
bool looks_like_error(const QString& line);
/// A non-JSON line on a structured stream: a status event, or an error event
/// (and facts.last_error) when it reads like one. Never dropped.
void plain_output_line(const QString& trimmed, services::coding_agents::StreamFacts& facts,
                       QVector<services::coding_agents::AgentEvent>& out);

/// One message assembled from streamed deltas (Text or Thinking).
///
/// append() returns the partial event to show (body = the message so far,
/// ref_id = the message key) and finish_message() the one complete event that
/// replaces it. Partials stop once the message outgrows one event body (the
/// complete event still carries the clipped text), so a very long message costs
/// O(n) overall; the text itself is capped at kMaxMessageChars.
class DeltaAccumulator {
  public:
    static constexpr qsizetype kMaxMessageChars = 8 * 1024 * 1024;

    bool active() const { return active_; }
    services::coding_agents::EventKind kind() const { return kind_; }
    const QString& key() const { return key_; }
    const QString& message_id() const { return message_id_; }
    const QString& text() const { return text_; }

    /// Begin a message (a previous one must have been finished or cleared).
    void start(services::coding_agents::EventKind kind, const QString& key, const QString& title,
               const QString& message_id = {});
    /// Append a delta; returns the partial event, or nullopt when nothing is
    /// worth showing yet (whitespace only) or partials have stopped (too long).
    std::optional<services::coding_agents::AgentEvent> append(const QString& delta);
    /// End the message: its complete event (nullopt for an empty / whitespace
    /// message). `full_text` receives the whole text (before event clipping).
    std::optional<services::coding_agents::AgentEvent> finish_message(QString* full_text = nullptr);
    void clear();

  private:
    bool active_ = false;
    bool partials_stopped_ = false;
    bool truncated_ = false;
    services::coding_agents::EventKind kind_ = services::coding_agents::EventKind::Text;
    QString key_;
    QString title_;
    QString message_id_;
    QString text_;
};

} // namespace fincept::harness::stream_support
