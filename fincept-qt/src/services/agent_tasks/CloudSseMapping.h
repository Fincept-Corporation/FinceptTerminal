#pragma once
// CloudSseMapping.h — turns the cloud agent's text/event-stream into run events.
//
// Pure (Qt Core only, unit-tested): no network, no timers, no singletons.
//
//   SseParser          text/event-stream framing (event:, data:, comments,
//                      multi-line data, CR/LF), dispatching on a blank line.
//   TokenMeter         the turn's token count. Lite mode reports tokens per graph
//                      step and the total in "finish"; deep mode (windowed runner)
//                      reports a CUMULATIVE count per window in "step-finish" — so
//                      within a window the maximum counts, never the sum (plan B9).
//   apply_cloud_event  maps one SSE event of /chat/agent/stream (or /resume) to
//                      RunEvents plus the turn's state (text, question, billing…).
//
// The event vocabulary (apiback finagent agent_runner.py / windowed_runner.py,
// chat.py): session-meta, text-delta, thinking, planning, step-start, step-finish,
// window-start, window-done, llm-call-start, llm-call-end, tool-start, tool-end,
// tool-discover, delegation-start, delegation-end, interrupt, approval-required,
// proposal-decided, trade-executed, history-compressed, rate-limit-retry,
// follow-ups, critique, heartbeat, finish, error.

#include "services/agent_tasks/TaskRunTypes.h"

#include <QByteArray>
#include <QHash>
#include <QJsonObject>
#include <QString>
#include <QStringList>
#include <QVector>

namespace fincept::agent_tasks {

/// Incremental SSE framing. Feed it complete lines (terminator stripped or not).
class SseParser {
  public:
    struct Message {
        QString event;   ///< "message" when the block named none
        QByteArray data; ///< data: lines joined with '\n'
    };

    /// One line of the stream. Returns true (and fills `out`) when this line
    /// completes a message: a blank line after data, or an "event:" line that
    /// starts a new block while the previous one still holds data.
    bool feed_line(QByteArray line, Message* out);
    /// End of stream: the block still open, if it holds data.
    bool flush(Message* out);
    void reset();

  private:
    QString event_;
    QByteArray data_;
    bool has_data_ = false;
};

/// Tokens of one cloud turn (see the header comment).
class TokenMeter {
  public:
    void on_step_finish(const QJsonObject& data);
    void on_window_done(const QJsonObject& data);
    void on_finish(const QJsonObject& data);
    qint64 total() const;
    void reset();

  private:
    qint64 lite_sum_ = 0;
    qint64 finish_total_ = -1;
    QHash<int, qint64> window_max_; // 1-based window -> highest cumulative count seen
};

/// Everything one cloud turn has said so far.
struct CloudTurnTracker {
    int step = 0; ///< run step the events are filed under
    TokenMeter tokens;
    QString text; ///< text streamed in this turn
    int events_seen = 0;
    bool finished = false;    ///< "finish" arrived
    bool interrupted = false; ///< the turn ended on a question
    QString question;
    QString question_mode;
    bool resumable = false; ///< the paused run takes its answer through /chat/agent/stream/resume
    bool errored = false;   ///< "error" arrived
    QString error_message;
    QString error_code;
    bool credits_known = false;
    double credits_charged = 0;
    bool balance_known = false;
    double credit_balance = 0;
    QString session_uuid; ///< from session-meta
    QString new_title;    ///< from session-meta (auto-titled conversation)
    QStringList follow_ups;
    int tool_seq = 0;
    QHash<QString, QStringList> open_tools; ///< tool name -> FIFO of ref ids still running
    QStringList open_delegations;

    /// A new turn of the same run (an answer to a question): forget everything
    /// but `step`.
    void reset_for_new_turn();
};

struct CloudEventOutcome {
    QVector<RunEvent> events; ///< step / seq / ts left for TaskRunService
    QString text_delta;       ///< text to append to the reply (text-delta)
    bool tokens_changed = false;
    bool terminal = false;  ///< "finish" or "error": the turn is over
    bool heartbeat = false; ///< keep-alive only
};

/// Apply one SSE event to `t`. Unknown events become a verbose Note.
CloudEventOutcome apply_cloud_event(CloudTurnTracker& t, const QString& name, const QJsonObject& data);

/// data["verbose"] = true marks detail rows a transcript may leave out (model
/// calls, graph steps, token updates); the activity view shows them.
bool is_verbose_event(const RunEvent& e);

} // namespace fincept::agent_tasks
