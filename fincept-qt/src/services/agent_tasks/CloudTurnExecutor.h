#pragma once
// CloudTurnExecutor.h — one cloud agent reply is one run (today's chat, made
// detach-safe). engine_id() "cloud_turn".
//
//   start()   creates the conversation if the run has none, then streams one
//             /chat/agent/stream turn through its own CloudAgentClient. A run
//             whose context has "answer_to_pause" starts with /…/resume instead
//             (the conversation's stored question), falling back to an ordinary
//             message when the server says nothing is waiting.
//   answer()  continues a run that paused on a question (WaitingInput) through
//             /chat/agent/stream/resume — same fallback.
//   cancel()  the server cannot be stopped: the run DETACHES (the stream is
//             dropped), then recovers the answer the server stores — the reply
//             is still charged. Sends to that conversation stay blocked until the
//             answer is adopted or the recovery gives up. Cancel again while
//             recovering stops waiting.
//
// Recovery (detach, lost connection, 150 s without data, a stream that ended
// without an answer): GET /chat/sessions/{uuid} until an assistant message
// follows the message this turn sent, then adopt it as the run's result. A
// message the server never received fails at once ("nothing was charged");
// recovery gives up kRecoveryDeadlineMs after the turn started.
//
// The turn itself (stream, fallbacks, recovery) is driven by CloudTurnDriver,
// shared with CloudPlanExecutor; this class maps its outcome onto the run.

#include "services/agent_tasks/CloudTurnPort.h"
#include "services/agent_tasks/IRunExecutor.h"

#include <QString>

#include <map>
#include <memory>

namespace fincept::agent_tasks {

class CloudTurnExecutor : public IRunExecutor, private ICloudTurnListener {
    Q_OBJECT
  public:
    explicit CloudTurnExecutor(QObject* parent = nullptr);
    ~CloudTurnExecutor() override;

    /// The server ends a turn after ~5 min (15 for a background task); allow margin.
    static constexpr int kRecoveryDeadlineMs = 10 * 60 * 1000;
    /// A turn the server never acknowledged is given up after this many failed lookups.
    static constexpr int kUnacceptedLookups = 2;

    QString engine_id() const override;
    void start(const TaskRun& run, const QVector<RunStep>& steps) override;
    void answer(const QString& run_id, const QString& text) override;
    void request_pause(const QString& run_id) override;
    void cancel(const QString& run_id) override;
    bool can_pause(const QString& run_id) const override;

  private:
    struct Turn; // per-run state (CloudTurnExecutor.cpp)

    Turn* turn(const QString& run_id) const;
    Turn* ensure_turn(const QString& run_id);
    void drop_turn(const QString& run_id);

    void begin(Turn* t, bool resend_after_check);
    void complete(Turn* t, const CloudTurnOutcome& outcome);
    void fail(Turn* t, const QString& message, const QString& code, const QString& partial, bool with_error_event);

    // ICloudTurnListener: what the run's CloudTurnDriver reports.
    void cloud_turn_event(const QString& run_id, const RunEvent& e) override;
    void cloud_turn_text(const QString& run_id, int step, const QString& chunk) override;
    void cloud_turn_usage(const QString& run_id, double credits, int tokens) override;
    void cloud_turn_outcome(const QString& run_id, const CloudTurnOutcome& outcome) override;
    void cloud_turn_closed(const QString& run_id) override;

    std::map<QString, std::unique_ptr<Turn>> turns_;
};

} // namespace fincept::agent_tasks
