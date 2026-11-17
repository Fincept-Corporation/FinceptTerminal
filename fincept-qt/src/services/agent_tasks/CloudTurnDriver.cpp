// CloudTurnDriver.cpp — one detach-safe cloud agent turn (extracted from
// CloudTurnExecutor, whose behaviour it keeps), the network ICloudBackend, and
// CloudPlanExecutor's default constructor.

#include "services/agent_tasks/CloudTurnDriver.h"

#include "core/logging/Logger.h"
#include "screens/chat_mode/ChatModeService.h"
#include "services/agent_tasks/CloudAgentClient.h"
#include "services/agent_tasks/CloudPlanExecutor.h"
#include "services/agent_tasks/TaskRunService.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QJsonArray>
#include <QPointer>
#include <QTimer>

#include <iterator>
#include <memory>

namespace fincept::agent_tasks {

namespace cloud_turn_driver_detail {

constexpr const char* kCloudTurnDriverTag = "CloudTurn";

/// The strings came from CloudTurnExecutor: they keep its translation context.
QString ctd_tr(const char* text) {
    return QCoreApplication::translate("fincept::agent_tasks::CloudTurnExecutor", text);
}

qint64 ctd_now_ms() {
    return QDateTime::currentMSecsSinceEpoch();
}

/// Delay before recovery lookup n (1-based): quick at first, then every 15 s.
int ctd_lookup_delay_ms(int lookups) {
    static const int kDelays[] = {2000, 3000, 5000, 8000, 10000};
    const int i = lookups - 1;
    return (i >= 0 && i < int(std::size(kDelays))) ? kDelays[i] : 15000;
}

RunEvent ctd_event(RunEventKind kind, const QString& title, const QString& body = {}, const QJsonObject& data = {}) {
    RunEvent e;
    e.kind = kind;
    e.title = title;
    e.body = body;
    e.data = data;
    return e;
}

/// ICloudBackend over ChatModeService / TaskRunService / CloudTurnDriver.
class CtdNetworkBackend final : public ICloudBackend {
  public:
    ICloudTurn* create_turn(ICloudTurnListener* listener, const QString& run_id, QObject* parent) override {
        return new CloudTurnDriver(listener, run_id, parent);
    }

    void create_session(QObject* ctx, std::function<void(bool, QString, QString)> cb) override {
        QPointer<QObject> context(ctx);
        chat_mode::ChatModeService::instance().create_session(
            QString::fromLatin1(chat_mode::kChatDefaultSessionTitle),
            [context, cb = std::move(cb)](bool ok, chat_mode::ChatSession s, QString err) {
                if (!context || !cb)
                    return;
                cb(ok && !s.uuid.isEmpty(), s.uuid, err);
            });
    }

    const TaskRun* find_run(const QString& run_id) const override {
        return TaskRunService::instance().find_run(run_id);
    }

    QVector<RunStep> run_steps(const QString& run_id) const override {
        return TaskRunService::instance().steps(run_id);
    }

    void log_info(const QString& message) override { LOG_INFO("CloudPlan", message); }
};

} // namespace cloud_turn_driver_detail

using namespace cloud_turn_driver_detail;
using chat_mode::ChatMessage;
using chat_mode::ChatModeService;
using chat_mode::ChatSession;

// ── CloudPlanExecutor's default constructor (the app's) ─────────────────────

CloudPlanExecutor::CloudPlanExecutor(QObject* parent)
    : CloudPlanExecutor(std::make_unique<CtdNetworkBackend>(), parent) {}

// ── Lifetime ─────────────────────────────────────────────────────────────────

CloudTurnDriver::CloudTurnDriver(ICloudTurnListener* listener, const QString& run_id, QObject* parent)
    : QObject(parent), listener_(listener), run_id_(run_id) {}

CloudTurnDriver::~CloudTurnDriver() {
    if (client_)
        client_->detach(); // deleted with this object (child); nothing is emitted
}

void CloudTurnDriver::release() {
    if (released_)
        return;
    released_ = true;
    if (lookup_timer_)
        lookup_timer_->stop();
    if (client_)
        client_->detach();
    deleteLater();
}

bool CloudTurnDriver::is_open() const {
    return client_ && client_->is_open();
}

// ── Start ────────────────────────────────────────────────────────────────────

void CloudTurnDriver::start(const CloudTurnRequest& request) {
    if (released_)
        return;
    request_ = request;
    request_.mode = request.mode == QLatin1String("deep") ? QStringLiteral("deep") : QStringLiteral("lite");
    if (request.check_stored_first) {
        // Resuming a run the app quit caught: the server may have finished (and
        // stored) the reply meanwhile — adopt it; otherwise send the message again.
        reset_turn();
        resend_if_unanswered_ = true;
        turn_started_ms_ = ctd_now_ms();
        begin_recovery(ctd_tr("Checking the conversation for an answer stored while the terminal was closed"), false);
        return;
    }
    send(request_.as_answer);
}

void CloudTurnDriver::reset_turn() {
    tracker_.reset_for_new_turn();
    tracker_.step = request_.step;
    reported_tokens_ = 0;
    accepted_ = false;
    done_ = false;
    recovering_ = false;
    detached_by_user_ = false;
    lookups_ = 0;
    lookup_in_flight_ = false;
    resend_if_unanswered_ = false;
    turn_started_ms_ = ctd_now_ms();
    if (lookup_timer_)
        lookup_timer_->stop();
}

void CloudTurnDriver::send(bool as_answer) {
    reset_turn();
    if (client_) {
        client_->detach();
        client_->deleteLater();
    }
    client_ = new CloudAgentClient(this);
    connect(client_, &CloudAgentClient::sse_event, this,
            [this](const QString& name, const QJsonObject& data) { on_sse(name, data); });
    connect(client_, &CloudAgentClient::closed, this,
            [this](bool clean, int status, const QString& code, const QString& message) {
                on_closed(clean, status, code, message);
            });
    connect(client_, &CloudAgentClient::response_started, this, [this](int) { accepted_ = true; });

    QJsonObject d;
    d["mode"] = request_.mode;
    d["verbose"] = true;
    if (as_answer)
        d["answer"] = true;
    // What goes out is fixed before the listener runs (it may start this turn anew).
    CloudAgentClient* client = client_;
    const QString session = request_.session_uuid;
    const QString text = request_.text;
    const QString mode = request_.mode;
    const int profile = request_.profile_id;
    listener_->cloud_turn_event(run_id_, ctd_event(RunEventKind::Status,
                                                   as_answer ? ctd_tr("Sending your answer to the agent")
                                                             : ctd_tr("Sending to the cloud agent"),
                                                   {}, d));
    if (released_ || client != client_)
        return;
    LOG_INFO(kCloudTurnDriverTag, QStringLiteral("Run %1: %2 (session %3, %4)")
                                      .arg(run_id_.left(8), as_answer ? QStringLiteral("resume") : QStringLiteral("new turn"),
                                           session.left(8), mode));
    if (as_answer)
        client->resume_turn(session, text, mode);
    else
        client->start_turn(session, text, mode, profile);
}

// ── Stream ───────────────────────────────────────────────────────────────────

void CloudTurnDriver::report_usage() {
    const qint64 total = tracker_.tokens.total();
    const qint64 delta = total - reported_tokens_;
    if (delta <= 0)
        return;
    reported_tokens_ = total;
    listener_->cloud_turn_usage(run_id_, 0, static_cast<int>(delta));
}

void CloudTurnDriver::on_sse(const QString& name, const QJsonObject& data) {
    if (released_)
        return;
    const CloudEventOutcome out = apply_cloud_event(tracker_, name, data);
    for (const RunEvent& e : out.events) {
        listener_->cloud_turn_event(run_id_, e);
        if (released_)
            return; // the owner dropped the run
    }
    if (!out.text_delta.isEmpty()) {
        listener_->cloud_turn_text(run_id_, tracker_.step, out.text_delta);
        if (released_)
            return;
    }
    if (out.tokens_changed) {
        report_usage();
        if (released_)
            return;
    }
    if (!out.terminal || done_)
        return;
    if (tracker_.errored) {
        fail(tracker_.error_message, tracker_.error_code, /*error_reported=*/true);
        return;
    }
    // "finish": the turn's billing, then the result.
    if (tracker_.credits_known && tracker_.credits_charged > 0) {
        listener_->cloud_turn_usage(run_id_, tracker_.credits_charged, 0);
        if (released_)
            return;
    }
    if (tracker_.balance_known)
        ChatModeService::instance().report_credit_balance(tracker_.credit_balance);
    else
        ChatModeService::instance().refresh_credit_balance(1500); // older servers / enterprise accounts
    complete();
}

void CloudTurnDriver::on_closed(bool clean, int http_status, const QString& code, const QString& message) {
    if (released_)
        return;
    if (done_) {
        // The stream closed after the result (deep mode sends a self-check after
        // "finish"): nothing more comes from this turn.
        listener_->cloud_turn_closed(run_id_);
        return;
    }
    if (recovering_)
        return;

    if (code == QLatin1String("resume_rejected")) {
        // Nothing was waiting for an answer (or the server has no resume route):
        // refused before anything was charged or stored. Send it as a new message.
        QJsonObject d;
        if (!message.isEmpty())
            d["server_message"] = message;
        listener_->cloud_turn_event(run_id_, ctd_event(RunEventKind::Note,
                                                       ctd_tr("The agent was no longer waiting for an answer, so your "
                                                              "reply was sent as a new message."),
                                                       {}, d));
        if (released_)
            return;
        send(false);
        return;
    }
    if (http_status >= 300) {
        // Refused before anything ran (402, 429, 503 …): the server's reason.
        RunEvent e = ctd_event(RunEventKind::Error, ctd_tr("The server refused this message"), message);
        e.is_error = true;
        e.data["http_status"] = http_status;
        if (!code.isEmpty())
            e.data["code"] = code;
        listener_->cloud_turn_event(run_id_, e);
        if (released_)
            return;
        fail(message, code, /*error_reported=*/true);
        return;
    }
    if (clean) {
        // Older servers end a lite turn that stopped on a question without "finish".
        if (!tracker_.text.trimmed().isEmpty() || tracker_.interrupted) {
            ChatModeService::instance().refresh_credit_balance(1500);
            complete();
            return;
        }
        begin_recovery(ctd_tr("The stream ended without an answer — checking the conversation"), false);
        return;
    }
    // connection_lost / stalled / no_response / network
    begin_recovery(message.isEmpty() ? ctd_tr("The connection to the agent was lost") : message, false);
}

// ── Outcomes ─────────────────────────────────────────────────────────────────

void CloudTurnDriver::complete() {
    if (done_)
        return;
    done_ = true;
    recovering_ = false;
    if (lookup_timer_)
        lookup_timer_->stop();
    CloudTurnOutcome o;
    o.kind = CloudTurnOutcome::Kind::Answered;
    o.question = tracker_.interrupted;
    o.resumable = tracker_.interrupted && tracker_.resumable;
    o.question_text = tracker_.question;
    o.question_mode = tracker_.question_mode;
    // An older server's question (not resumable) ends the turn: the next message answers it.
    o.text = tracker_.text.trimmed().isEmpty() && tracker_.interrupted ? tracker_.question : tracker_.text;
    listener_->cloud_turn_outcome(run_id_, o);
}

void CloudTurnDriver::fail(const QString& message, const QString& code, bool error_reported) {
    if (done_)
        return;
    done_ = true;
    recovering_ = false;
    if (lookup_timer_)
        lookup_timer_->stop();
    CloudTurnOutcome o;
    o.kind = CloudTurnOutcome::Kind::Failed;
    o.text = tracker_.text;
    o.error = message;
    o.code = code;
    o.error_reported = error_reported;
    listener_->cloud_turn_outcome(run_id_, o);
}

void CloudTurnDriver::stop(const QString& text, const QString& message) {
    if (done_)
        return;
    done_ = true;
    recovering_ = false;
    if (lookup_timer_)
        lookup_timer_->stop();
    CloudTurnOutcome o;
    o.kind = CloudTurnOutcome::Kind::Stopped;
    o.text = text;
    o.error = message;
    listener_->cloud_turn_outcome(run_id_, o);
}

// ── Detach and recovery ──────────────────────────────────────────────────────

void CloudTurnDriver::detach() {
    if (released_ || done_)
        return;
    if (recovering_) {
        // Stop pressed again while waiting for the stored answer: stop waiting.
        stop(tracker_.text, ctd_tr("Stopped waiting for the stored answer. The server may still store it: reopen the "
                                   "conversation later to see it."));
        return;
    }
    begin_recovery(ctd_tr("Detached — you stopped following this reply"), true);
}

void CloudTurnDriver::begin_recovery(const QString& reason, bool by_user) {
    if (client_)
        client_->detach();
    recovering_ = true;
    detached_by_user_ = detached_by_user_ || by_user;
    lookups_ = 0;
    QJsonObject d;
    d["recovering"] = true;
    d["detached"] = detached_by_user_;
    const QString body =
        by_user ? ctd_tr("The server cannot be stopped: it finishes this reply, charges it and stores it in the "
                         "conversation. The terminal picks the stored answer up; this conversation takes new messages "
                         "once it has (press Stop again to stop waiting).")
                : ctd_tr("The server keeps working on this reply and stores it when done; the terminal picks it up from "
                         "the conversation.");
    LOG_INFO(kCloudTurnDriverTag, QStringLiteral("Run %1 recovering: %2").arg(run_id_.left(8), reason));
    listener_->cloud_turn_event(run_id_, ctd_event(RunEventKind::Status, reason, body, d));
    if (released_)
        return;
    schedule_lookup(by_user ? 2000 : 1000);
}

void CloudTurnDriver::schedule_lookup(int delay_ms) {
    if (!lookup_timer_) {
        lookup_timer_ = new QTimer(this);
        lookup_timer_->setSingleShot(true);
        connect(lookup_timer_, &QTimer::timeout, this, [this]() { lookup(); });
    }
    lookup_timer_->start(delay_ms);
}

void CloudTurnDriver::lookup() {
    if (released_ || done_ || !recovering_ || lookup_in_flight_)
        return;
    if (request_.session_uuid.isEmpty()) {
        fail(ctd_tr("This run has no conversation to recover its answer from."), QStringLiteral("no_session"), false);
        return;
    }
    if (ctd_now_ms() - turn_started_ms_ > kCloudRecoveryDeadlineMs) {
        fail(ctd_tr("No stored answer arrived within %1 minutes. The server may have failed to finish this reply: open "
                    "the conversation later to check, or send the message again.")
                 .arg(kCloudRecoveryDeadlineMs / 60000),
             QStringLiteral("recovery_timeout"), false);
        return;
    }
    lookup_in_flight_ = true;
    ++lookups_;
    QPointer<CloudTurnDriver> self(this);
    ChatModeService::instance().load_session(
        request_.session_uuid, [self](bool ok, ChatSession, QVector<ChatMessage> messages, QString err, int status) {
            if (!self || self->released_)
                return;
            CloudTurnDriver* d = self.data();
            d->lookup_in_flight_ = false;
            if (d->done_ || !d->recovering_)
                return;
            if (!ok) {
                if (status == 404) {
                    d->fail(ctd_tr("This conversation no longer exists on the server, so its answer cannot be "
                                   "recovered."),
                            QStringLiteral("session_gone"), false);
                    return;
                }
                LOG_WARN(kCloudTurnDriverTag, QStringLiteral("Run %1: recovery lookup %2 failed: %3")
                                                  .arg(d->run_id_.left(8))
                                                  .arg(d->lookups_)
                                                  .arg(err));
                d->schedule_lookup(ctd_lookup_delay_ms(d->lookups_));
                return;
            }
            bool sent_found = false;
            const int idx = find_answer(messages, d->request_.text, &sent_found);
            if (idx >= 0) {
                d->adopt(messages.at(idx));
                return;
            }
            if (d->resend_if_unanswered_) {
                d->resend_if_unanswered_ = false;
                d->listener_->cloud_turn_event(d->run_id_, ctd_event(RunEventKind::Note,
                                                                     ctd_tr("No stored answer — sending the message again")));
                if (!self || d->released_)
                    return;
                d->send(false);
                return;
            }
            if (!sent_found && !d->accepted_ && d->lookups_ >= kCloudUnacceptedLookups) {
                if (d->detached_by_user_)
                    d->stop({}, ctd_tr("Stopped before the server received the message; nothing was charged."));
                else
                    d->fail(ctd_tr("Your message did not reach the Fincept server, so nothing was charged and no "
                                   "answer is coming. Send it again."),
                            QStringLiteral("not_delivered"), false);
                return;
            }
            if (d->lookups_ % 6 == 0) {
                QJsonObject data;
                data["verbose"] = true;
                data["recovering"] = true;
                d->listener_->cloud_turn_event(d->run_id_, ctd_event(RunEventKind::Status,
                                                                     ctd_tr("Still waiting for the stored answer (%1 s)")
                                                                         .arg((ctd_now_ms() - d->turn_started_ms_) / 1000),
                                                                     {}, data));
                if (!self || d->released_)
                    return;
            }
            d->schedule_lookup(ctd_lookup_delay_ms(d->lookups_));
        });
}

int CloudTurnDriver::find_answer(const QVector<ChatMessage>& messages, const QString& sent, bool* sent_found) {
    const QString needle = sent.trimmed();
    int user_idx = -1;
    for (int i = static_cast<int>(messages.size()) - 1; i >= 0; --i) {
        if (messages.at(i).role == QLatin1String("user") && messages.at(i).content.trimmed() == needle) {
            user_idx = i;
            break;
        }
    }
    if (sent_found)
        *sent_found = user_idx >= 0;
    if (user_idx < 0)
        return -1;
    for (int i = user_idx + 1; i < messages.size(); ++i) {
        const ChatMessage& m = messages.at(i);
        if (m.role != QLatin1String("assistant"))
            continue;
        // A trade decision recorded through the proposals route is its own message.
        if (m.metadata.contains(QStringLiteral("proposal_decision")))
            continue;
        return i;
    }
    return -1;
}

void CloudTurnDriver::adopt(const ChatMessage& message) {
    if (done_)
        return;
    done_ = true;
    recovering_ = false;
    if (lookup_timer_)
        lookup_timer_->stop();
    // The server also appends a question to `content` (for clients that read only
    // that); the Question event carries it, so don't repeat it in the answer.
    QString content = message.content;
    const QString question = message.interrupt_question();
    if (!question.isEmpty()) {
        const QString trimmed = content.trimmed();
        if (trimmed == question)
            content.clear();
        else if (trimmed.endsWith(question))
            content = trimmed.left(trimmed.size() - question.size()).trimmed();
    }
    LOG_INFO(kCloudTurnDriverTag,
             QStringLiteral("Run %1 adopted the stored answer (%2 chars)").arg(run_id_.left(8)).arg(content.size()));
    QJsonObject d;
    d["message_uuid"] = message.uuid;
    d["recovering"] = false;
    listener_->cloud_turn_event(run_id_, ctd_event(RunEventKind::Note, ctd_tr("Recovered the answer the server stored"), {}, d));
    if (released_)
        return;
    // The usage the stored message carries: its tokens (tokens_used), never its
    // bill — the server charged the turn but only the lost stream's "finish" said
    // how much. The turn is counted, and its credits are marked as not reported.
    {
        const qint64 stored_tokens = qMax(0, message.tokens_used);
        const qint64 extra = stored_tokens - reported_tokens_;
        if (extra > 0) {
            reported_tokens_ = stored_tokens;
            listener_->cloud_turn_usage(run_id_, 0, static_cast<int>(extra));
            if (released_)
                return;
        }
        QJsonObject u;
        u["turn_end"] = true;
        u["credits_unreported"] = true;
        u["tokens_total"] = static_cast<double>(qMax(stored_tokens, reported_tokens_));
        u["recovered"] = true;
        listener_->cloud_turn_event(run_id_,
                                    ctd_event(RunEventKind::Usage,
                                              stored_tokens > 0 ? ctd_tr("Recovered reply · %1 tokens · credits not reported "
                                                                         "by the server")
                                                                      .arg(stored_tokens)
                                                                : ctd_tr("Recovered reply · credits not reported by the server"),
                                              {}, u));
        if (released_)
            return;
    }
    if (!question.isEmpty()) {
        QJsonObject q;
        q["resumable"] = true; // the stored question is answered through /resume (with the message fallback)
        q["stored"] = true;
        listener_->cloud_turn_event(run_id_, ctd_event(RunEventKind::Question, ctd_tr("The agent asks"), question, q));
        if (released_)
            return;
    }
    for (const QJsonValue& v : message.proposals()) {
        const QJsonObject p = v.toObject();
        const QString status = p.value(QStringLiteral("status")).toString();
        if (!status.isEmpty() && status != QLatin1String("pending"))
            continue;
        RunEvent e = ctd_event(RunEventKind::Approval, ctd_tr("Trade approval"), p.value(QStringLiteral("notes")).toString(), p);
        e.ref_id = p.value(QStringLiteral("proposal_id")).toString();
        listener_->cloud_turn_event(run_id_, e);
        if (released_)
            return;
    }
    ChatModeService::instance().refresh_credit_balance(0); // the stream's balance never arrived
    CloudTurnOutcome o;
    o.kind = CloudTurnOutcome::Kind::Answered;
    o.adopted = true;
    o.message_uuid = message.uuid;
    o.question = !question.isEmpty();
    o.resumable = o.question;
    o.question_text = question;
    o.text = content.trimmed().isEmpty() && !question.isEmpty() ? question : content;
    listener_->cloud_turn_outcome(run_id_, o);
}

} // namespace fincept::agent_tasks
