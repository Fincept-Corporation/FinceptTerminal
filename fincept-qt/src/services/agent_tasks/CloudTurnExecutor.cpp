// CloudTurnExecutor.cpp — one cloud reply per run, detach-safe: a lost, stalled or
// detached stream recovers the answer the server stores in the conversation. The
// turn is driven by CloudTurnDriver; this file maps its outcome onto the run.

#include "services/agent_tasks/CloudTurnExecutor.h"

#include "core/logging/Logger.h"
#include "screens/chat_mode/ChatModeService.h"
#include "services/agent_tasks/CloudTurnDriver.h"
#include "services/agent_tasks/TaskRunService.h"

#include <QPointer>
#include <QTimer>

namespace fincept::agent_tasks {

namespace cloud_turn_executor_detail {

constexpr const char* kCloudTurnTag = "CloudTurn";
constexpr int kCloudTurnSummaryChars = 500;

QString ctx_summary(const QString& text) {
    const QString t = text.trimmed();
    return t.size() > kCloudTurnSummaryChars ? t.left(kCloudTurnSummaryChars - 1) + QChar(0x2026) : t;
}

RunEvent ctx_event(RunEventKind kind, const QString& title, const QString& body = {}, const QJsonObject& data = {}) {
    RunEvent e;
    e.kind = kind;
    e.title = title;
    e.body = body;
    e.data = data;
    return e;
}

} // namespace cloud_turn_executor_detail

using namespace cloud_turn_executor_detail;
using chat_mode::ChatModeService;
using chat_mode::ChatSession;

// The public limits and the driver's must not drift apart.
static_assert(CloudTurnExecutor::kRecoveryDeadlineMs == kCloudRecoveryDeadlineMs);
static_assert(CloudTurnExecutor::kUnacceptedLookups == kCloudUnacceptedLookups);

struct CloudTurnExecutor::Turn {
    QString run_id;
    CloudTurnRequest request;          ///< what the turn sends (session, text, mode, profile, as_answer)
    CloudTurnDriver* driver = nullptr; ///< child of the executor; release(), never delete
    bool done = false;                 ///< finished() / waiting() reported for this turn
    bool waiting = false;              ///< parked on a question (WaitingInput)
};

CloudTurnExecutor::CloudTurnExecutor(QObject* parent) : IRunExecutor(parent) {}

CloudTurnExecutor::~CloudTurnExecutor() {
    for (auto& [id, t] : turns_) {
        if (t->driver)
            t->driver->release(); // deleted with this object (child); nothing is emitted
    }
    turns_.clear();
}

QString CloudTurnExecutor::engine_id() const {
    return QString::fromLatin1(kExecutorCloudTurn);
}

bool CloudTurnExecutor::can_pause(const QString&) const {
    return false; // one reply cannot pause half-way
}

void CloudTurnExecutor::request_pause(const QString&) {}

CloudTurnExecutor::Turn* CloudTurnExecutor::turn(const QString& run_id) const {
    const auto it = turns_.find(run_id);
    return it == turns_.end() ? nullptr : it->second.get();
}

CloudTurnExecutor::Turn* CloudTurnExecutor::ensure_turn(const QString& run_id) {
    if (Turn* t = turn(run_id))
        return t;
    auto t = std::make_unique<Turn>();
    t->run_id = run_id;
    t->driver = new CloudTurnDriver(static_cast<ICloudTurnListener*>(this), run_id, this);
    Turn* raw = t.get();
    turns_.emplace(run_id, std::move(t));
    return raw;
}

void CloudTurnExecutor::drop_turn(const QString& run_id) {
    const auto it = turns_.find(run_id);
    if (it == turns_.end())
        return;
    if (it->second->driver)
        it->second->driver->release();
    turns_.erase(it);
}

// ── Start / answer ───────────────────────────────────────────────────────────

void CloudTurnExecutor::start(const TaskRun& run, const QVector<RunStep>& steps) {
    drop_turn(run.id);
    Turn* t = ensure_turn(run.id);
    t->request.session_uuid = run.session_uuid;
    t->request.mode = run.mode == QLatin1String("deep") ? QStringLiteral("deep") : QStringLiteral("lite");
    t->request.profile_id = run.profile_id;
    t->request.text = run.prompt;
    t->request.as_answer = run.context.value(QLatin1String(kCtxAnswerToPause)).toBool();
    t->request.step = 0;
    const QString run_id = run.id;

    // A resume of a run this executor already started once (an app quit caught it).
    const bool resumed = !steps.isEmpty() && steps.first().attempts > 0;
    if (steps.isEmpty()) {
        RunStep s;
        s.run_id = run_id;
        s.title = tr("Reply");
        s.instruction = run.prompt.left(4000);
        emit plan_ready(run_id, {s});
    }
    emit step_changed(run_id, 0, StepStatus::Running, {});

    if (!(t = turn(run_id)))
        return; // cancelled from a slot meanwhile
    if (!t->request.session_uuid.isEmpty()) {
        begin(t, resumed);
        return;
    }
    // A run without a conversation (NEW RUN / schedule): create one. The default
    // title lets the server auto-title it from the first message.
    QPointer<CloudTurnExecutor> self(this);
    ChatModeService::instance().create_session(
        QString::fromLatin1(chat_mode::kChatDefaultSessionTitle), [self, run_id](bool ok, ChatSession s, QString err) {
            if (!self)
                return;
            Turn* t = self->turn(run_id);
            if (!t || t->done)
                return;
            if (!ok || s.uuid.isEmpty()) {
                self->fail(t, tr("Could not create a conversation for this run: %1").arg(err), QStringLiteral("session_create"),
                           {}, true);
                return;
            }
            t->request.session_uuid = s.uuid;
            QJsonObject d;
            d["session_uuid"] = s.uuid;
            d["verbose"] = true;
            emit self->event(run_id, ctx_event(RunEventKind::Status, tr("Conversation created"), {}, d));
            if ((t = self->turn(run_id)))
                self->begin(t, false);
        });
}

void CloudTurnExecutor::begin(Turn* t, bool resend_after_check) {
    // resend_after_check: resuming a run the app quit caught — the driver adopts a
    // reply the server stored meanwhile, otherwise sends the message again.
    CloudTurnRequest request = t->request;
    request.check_stored_first = resend_after_check;
    t->driver->start(request);
}

void CloudTurnExecutor::answer(const QString& run_id, const QString& text) {
    Turn* t = turn(run_id);
    if (!t) {
        // Paused before a restart: rebuild what the turn needs from the run.
        const TaskRun* run = TaskRunService::instance().find_run(run_id);
        if (!run) {
            emit finished(run_id, RunStatus::Failed, {}, tr("This run no longer exists."));
            return;
        }
        t = ensure_turn(run_id);
        t->request.session_uuid = run->session_uuid;
        t->request.mode = run->mode == QLatin1String("deep") ? QStringLiteral("deep") : QStringLiteral("lite");
        t->request.profile_id = run->profile_id;
        t->request.step = 0;
    }
    t->request.text = text;
    t->request.as_answer = true;
    t->waiting = false;
    emit step_changed(run_id, 0, StepStatus::Running, {});
    if (!(t = turn(run_id)))
        return;
    t->done = false;
    begin(t, false);
}

// ── What the turn reports ────────────────────────────────────────────────────

void CloudTurnExecutor::cloud_turn_event(const QString& run_id, const RunEvent& e) {
    emit event(run_id, e);
}

void CloudTurnExecutor::cloud_turn_text(const QString& run_id, int step, const QString& chunk) {
    emit text_delta(run_id, step, chunk);
}

void CloudTurnExecutor::cloud_turn_usage(const QString& run_id, double credits, int tokens) {
    emit usage(run_id, credits, tokens);
}

void CloudTurnExecutor::cloud_turn_outcome(const QString& run_id, const CloudTurnOutcome& outcome) {
    Turn* t = turn(run_id);
    if (!t || t->done)
        return;
    switch (outcome.kind) {
        case CloudTurnOutcome::Kind::Answered:
            complete(t, outcome);
            return;
        case CloudTurnOutcome::Kind::Failed:
            fail(t, outcome.error, outcome.code, outcome.text, /*with_error_event=*/!outcome.error_reported);
            return;
        case CloudTurnOutcome::Kind::Stopped:
            // Stop pressed again while recovering, or stopped before the server
            // received the message.
            t->done = true;
            emit finished(run_id, RunStatus::Cancelled, outcome.text, outcome.error);
            QTimer::singleShot(0, this, [this, run_id]() { drop_turn(run_id); });
            return;
    }
}

void CloudTurnExecutor::cloud_turn_closed(const QString& run_id) {
    // The stream closed after the result (deep mode sends a self-check after
    // "finish"): nothing more to wait for unless the run parked on a question.
    Turn* t = turn(run_id);
    if (!t || t->waiting)
        return;
    QTimer::singleShot(0, this, [this, run_id]() {
        Turn* tt = turn(run_id);
        if (tt && tt->done && !tt->waiting && !tt->driver->recovering())
            drop_turn(run_id);
    });
}

void CloudTurnExecutor::complete(Turn* t, const CloudTurnOutcome& o) {
    if (t->done)
        return;
    t->done = true;
    const QString run_id = t->run_id;
    if (o.question && o.resumable && !o.adopted) {
        // The run is paused on the server, waiting for the user's answer.
        t->waiting = true;
        QJsonObject payload;
        payload["question"] = o.question_text;
        payload["resumable"] = true;
        if (!o.question_mode.isEmpty())
            payload["mode"] = o.question_mode;
        LOG_INFO(kCloudTurnTag, QStringLiteral("Run %1 waits for the user's answer").arg(run_id.left(8)));
        emit waiting(run_id, RunStatus::WaitingInput, payload);
        return;
    }
    // An answer adopted from the conversation (a stored question included) ends
    // the run: the next message answers that question.
    emit step_changed(run_id, 0, StepStatus::Done, ctx_summary(o.text));
    emit finished(run_id, RunStatus::Completed, o.text, {});
    if (o.adopted)
        QTimer::singleShot(0, this, [this, run_id]() { drop_turn(run_id); });
}

void CloudTurnExecutor::fail(Turn* t, const QString& message, const QString& code, const QString& partial,
                             bool with_error_event) {
    if (t->done)
        return;
    t->done = true;
    const QString run_id = t->run_id;
    const QString text = cloud_turn_failure_text(message, code);
    if (with_error_event) {
        RunEvent e = ctx_event(RunEventKind::Error, tr("The reply failed"), text);
        e.is_error = true;
        if (!code.isEmpty())
            e.data["code"] = code;
        emit event(run_id, e);
    }
    emit step_changed(run_id, 0, StepStatus::Failed, ctx_summary(text));
    emit finished(run_id, RunStatus::Failed, partial, text);
    QTimer::singleShot(0, this, [this, run_id]() {
        Turn* tt = turn(run_id);
        if (tt && tt->done && !tt->waiting)
            drop_turn(run_id);
    });
}

// ── Detach ───────────────────────────────────────────────────────────────────

void CloudTurnExecutor::cancel(const QString& run_id) {
    Turn* t = turn(run_id);
    if (!t || t->done) {
        // Nothing in flight (a waiting or already finished turn): forget it; the
        // service ends the run itself. A run the service still counts as running
        // without any state here must not hang: report it cancelled.
        if (t)
            QTimer::singleShot(0, this, [this, run_id]() { drop_turn(run_id); });
        const TaskRun* run = TaskRunService::instance().find_run(run_id);
        if (!t && run && holds_slot(run->status))
            emit finished(run_id, RunStatus::Cancelled, {}, {});
        return;
    }
    // The server cannot be stopped: the driver detaches and recovers the stored
    // answer; pressed again while recovering, it stops waiting (outcome Stopped).
    t->driver->detach();
}

} // namespace fincept::agent_tasks
