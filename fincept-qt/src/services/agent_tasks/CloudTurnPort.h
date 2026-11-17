#pragma once
// CloudTurnPort.h — the narrow seam between a cloud executor and "one cloud agent
// turn". Header-only, Qt Core only, no moc (plain interfaces), so executors that
// talk to the cloud through it are unit-testable with a fake turn.
//
//   ICloudTurn          one turn: send a message (or the answer to a question),
//                       stream it, and on a lost / stalled / detached stream recover
//                       the answer the server stores in the conversation.
//                       The app's implementation is CloudTurnDriver (shared by
//                       CloudTurnExecutor and CloudPlanExecutor).
//   ICloudTurnListener  what a turn reports, always with the run id.
//   ICloudBackend       what CloudPlanExecutor needs besides turns (conversation
//                       creation, the run record after a restart, logging).
//
// Re-entrancy: a listener callback may lead (through TaskRunService's slots) to
// detach() or release() of the very turn that is calling. A turn therefore checks
// after every callback whether it was released and stops if so; a released turn
// never calls its listener again. Owners never delete a turn directly: they call
// release(), which drops the stream silently and deletes the object later.

#include "services/agent_tasks/TaskRunTypes.h"

#include <QCoreApplication>
#include <QObject>
#include <QString>
#include <QVector>

#include <functional>
#include <memory>

namespace fincept::agent_tasks {

/// The server ends a turn after ~5 min (15 for a background task); recovery
/// gives up this long after the turn started.
inline constexpr int kCloudRecoveryDeadlineMs = 10 * 60 * 1000;
/// A turn the server never acknowledged is given up after this many failed lookups.
inline constexpr int kCloudUnacceptedLookups = 2;

/// What one turn sends.
struct CloudTurnRequest {
    QString session_uuid;
    QString text;          ///< the message, or the answer when as_answer
    QString mode;          ///< "lite" | "deep"
    int profile_id = 0;    ///< investment profile (0 = none)
    bool as_answer = false; ///< /chat/agent/stream/resume (falls back to a new message when nothing waits)
    /// Resuming after an app quit: adopt the stored answer to `text` if the server
    /// finished it meanwhile, otherwise send `text` again.
    bool check_stored_first = false;
    int step = 0; ///< run step the turn's streamed text is filed under (text callbacks)
};

/// How a turn ended (reported once per start()).
struct CloudTurnOutcome {
    enum class Kind {
        Answered, ///< the agent answered (streamed, or recovered from the conversation)
        Failed,   ///< the turn failed (error, refusal, recovery gave up)
        Stopped,  ///< the user stopped it (stopped waiting, or detached before delivery)
    };
    Kind kind = Kind::Failed;
    /// Answered: the reply (a stored question stripped from it; the question when
    /// the reply holds nothing else). Failed / Stopped: the text streamed so far.
    QString text;
    bool question = false;  ///< the turn ended on a question
    bool resumable = false; ///< ... that /chat/agent/stream/resume answers
    QString question_text;
    QString question_mode;
    bool adopted = false;   ///< recovered from the conversation (the stream's end never arrived)
    QString message_uuid;   ///< the stored assistant message, when adopted
    QString error;          ///< Failed / Stopped: plain-language reason
    QString code;           ///< Failed: machine-readable reason ("insufficient_credits", "recovery_timeout"…)
    bool error_reported = false; ///< an Error event for it was already emitted by the turn
};

class ICloudTurnListener {
  public:
    virtual void cloud_turn_event(const QString& run_id, const RunEvent& e) = 0;
    virtual void cloud_turn_text(const QString& run_id, int step, const QString& chunk) = 0;
    /// Amounts added since the previous call (IRunExecutor::usage() convention).
    virtual void cloud_turn_usage(const QString& run_id, double credits, int tokens) = 0;
    virtual void cloud_turn_outcome(const QString& run_id, const CloudTurnOutcome& outcome) = 0;
    /// The stream of a turn whose outcome was already reported has closed:
    /// nothing more comes from it (deep mode sends a self-check after "finish").
    virtual void cloud_turn_closed(const QString& run_id) = 0;

  protected:
    ~ICloudTurnListener() = default;
};

class ICloudTurn {
  public:
    virtual ~ICloudTurn() = default;
    /// Send `request` (or first look for its stored answer); every start() ends in
    /// exactly one cloud_turn_outcome(). A turn may be started again after that.
    virtual void start(const CloudTurnRequest& request) = 0;
    /// The user stopped following: drop the stream and recover the stored answer
    /// (the server cannot be stopped). Called again while recovering, or before the
    /// server received the message: give up — outcome Stopped.
    virtual void detach() = 0;
    /// Drop everything silently (no more callbacks) and delete the turn later.
    virtual void release() = 0;
    /// Reading the stored answer from the conversation.
    virtual bool recovering() const = 0;
    /// The stream is still open (possibly after the outcome was reported).
    virtual bool is_open() const = 0;
};

class ICloudBackend {
  public:
    virtual ~ICloudBackend() = default;
    /// A new turn reporting to `listener` for `run_id`, parented to `parent`.
    virtual ICloudTurn* create_turn(ICloudTurnListener* listener, const QString& run_id, QObject* parent) = 0;
    /// Create a conversation; `cb` runs on the GUI thread unless `ctx` is gone.
    virtual void create_session(QObject* ctx, std::function<void(bool ok, QString uuid, QString error)> cb) = 0;
    /// The run as TaskRunService holds it (answer() after a restart), or null.
    virtual const TaskRun* find_run(const QString& run_id) const = 0;
    virtual QVector<RunStep> run_steps(const QString& run_id) const = 0;
    virtual void log_info(const QString& message) = 0;
};

/// The plain-language failure text of a cloud turn (shared by the cloud executors).
inline QString cloud_turn_failure_text(const QString& message, const QString& code) {
    if (code == QLatin1String("not_waiting")) {
        // A resumed run whose pause was gone once the stream had started (a rare race):
        // the server stored the answer as a message and refunded the turn.
        return QCoreApplication::translate(
            "fincept::agent_tasks::CloudTurnExecutor",
            "The agent was no longer waiting for an answer. Your reply is saved in the conversation but was not "
            "answered; send it again as a new message if you still want an answer.");
    }
    const QString m = message.trimmed();
    return m.isEmpty() ? QCoreApplication::translate("fincept::agent_tasks::CloudTurnExecutor", "The reply failed.") : m;
}

} // namespace fincept::agent_tasks
