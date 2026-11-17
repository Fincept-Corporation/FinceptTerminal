#pragma once
// CloudTurnDriver.h — the app's ICloudTurn (CloudTurnPort.h): one cloud agent turn
// over its own CloudAgentClient, detach-safe. Shared by CloudTurnExecutor (one
// reply per run) and CloudPlanExecutor (plan, step and synthesis turns). GUI
// thread; one driver per run at a time; owners call release(), never delete.
//
//   start()   streams /chat/agent/stream, or /…/resume for an answer, falling back
//             to an ordinary message when the server says nothing is waiting.
//             With check_stored_first it first looks for an answer the server
//             stored while the terminal was closed, and sends only if none.
//   detach()  the server cannot be stopped: the stream is dropped, then the answer
//             the server stores is recovered (it is still charged). Again while
//             recovering: stop waiting (outcome Stopped).
//
// Recovery (detach, lost connection, 150 s without data, a stream that ended
// without an answer): GET /chat/sessions/{uuid} until an assistant message follows
// the message this turn sent, then adopt it. A message the server never received
// fails at once ("nothing was charged"); recovery gives up
// kCloudRecoveryDeadlineMs after the turn started.
//
// This file also provides the network ICloudBackend and the default constructor
// of CloudPlanExecutor (which creates it), so CloudPlanExecutor.cpp itself stays
// Qt-Core-only and links into unit tests with a fake backend.

#include "screens/chat_mode/ChatModeTypes.h"
#include "services/agent_tasks/CloudSseMapping.h"
#include "services/agent_tasks/CloudTurnPort.h"

#include <QJsonObject>
#include <QObject>
#include <QString>
#include <QVector>

class QTimer;

namespace fincept::agent_tasks {

class CloudAgentClient;

class CloudTurnDriver final : public QObject, public ICloudTurn {
  public:
    CloudTurnDriver(ICloudTurnListener* listener, const QString& run_id, QObject* parent = nullptr);
    ~CloudTurnDriver() override;

    void start(const CloudTurnRequest& request) override;
    void detach() override;
    void release() override;
    bool recovering() const override { return recovering_; }
    bool is_open() const override;

    /// Index of the assistant message that answers the user message `sent`, -1
    /// when none yet; `sent_found` says whether `sent` is in the conversation.
    static int find_answer(const QVector<chat_mode::ChatMessage>& messages, const QString& sent, bool* sent_found);

  private:
    void reset_turn();
    void send(bool as_answer);
    void on_sse(const QString& name, const QJsonObject& data);
    void on_closed(bool clean, int http_status, const QString& code, const QString& message);
    void report_usage();
    void complete();
    void fail(const QString& message, const QString& code, bool error_reported);
    void stop(const QString& text, const QString& message);
    void begin_recovery(const QString& reason, bool by_user);
    void schedule_lookup(int delay_ms);
    void lookup();
    void adopt(const chat_mode::ChatMessage& message);

    ICloudTurnListener* listener_;
    QString run_id_;
    CloudTurnRequest request_;
    CloudAgentClient* client_ = nullptr; ///< child
    QTimer* lookup_timer_ = nullptr;     ///< child
    CloudTurnTracker tracker_;
    qint64 reported_tokens_ = 0;
    qint64 turn_started_ms_ = 0;
    bool accepted_ = false;  ///< the server answered 2xx: it holds (and charged) the message
    bool done_ = false;      ///< the outcome of this start() was reported
    bool recovering_ = false; ///< reading the stored answer from the conversation
    bool detached_by_user_ = false;
    bool lookup_in_flight_ = false;
    int lookups_ = 0;
    bool resend_if_unanswered_ = false; ///< check_stored_first: send when nothing was stored
    bool released_ = false;
};

} // namespace fincept::agent_tasks
