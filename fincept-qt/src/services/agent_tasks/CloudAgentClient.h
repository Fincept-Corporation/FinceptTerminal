#pragma once
// CloudAgentClient.h — one streamed cloud agent turn: POST /chat/agent/stream (a
// new message) or /chat/agent/stream/resume (the answer to a question the run
// paused on), read as text/event-stream.
//
// One instance per run turn, GUI thread, its own QNetworkReply — so several runs
// (and windows) never share or abort each other's connection.
//
// Watchdogs (every network path ends in closed() with a plain-language message):
//   * no response headers within kResponseTimeoutMs -> closed(false, 0, "no_response", …)
//   * no data at all for kStallTimeoutMs after that (the server sends a heartbeat
//     every 30 s while it works)              -> closed(false, status, "stalled", …)
// closed() codes: "" (clean end of stream), "stalled", "no_response",
// "connection_lost" (dropped after the server accepted the turn), "network" (no
// answer from the server at all), "resume_rejected" (a resume refused before
// anything ran — send the answer as an ordinary message), "insufficient_credits"
// (402), or the server's X-Error-Code of a refusal ("agent_busy", …).
//
// detach() drops the connection without emitting anything. The server keeps
// working on a detached (or lost) turn, charges it and stores the answer in the
// conversation; CloudTurnExecutor recovers it from there.

#include "services/agent_tasks/CloudSseMapping.h"

#include <QByteArray>
#include <QJsonObject>
#include <QObject>
#include <QPointer>
#include <QString>

class QNetworkReply;
class QTimer;

namespace fincept::agent_tasks {

class CloudAgentClient : public QObject {
    Q_OBJECT
  public:
    explicit CloudAgentClient(QObject* parent = nullptr);
    ~CloudAgentClient() override;

    static constexpr int kResponseTimeoutMs = 60 * 1000;
    static constexpr int kStallTimeoutMs = 150 * 1000;

    void start_turn(const QString& session_uuid, const QString& message, const QString& mode, int profile_id);
    void resume_turn(const QString& session_uuid, const QString& answer, const QString& mode);
    void detach();

    /// A request is on the wire.
    bool is_open() const { return reply_ != nullptr; }
    /// The server answered 2xx: it took the turn (charged it, stored the message).
    bool accepted() const { return accepted_; }

  signals:
    void sse_event(const QString& name, const QJsonObject& data);
    void closed(bool clean, int http_status, const QString& code, const QString& message);
    /// WP1: the server accepted the turn (HTTP 2xx headers arrived).
    void response_started(int http_status);

  private:
    void send(const QString& path, const QByteArray& body, bool allow_replay, bool is_resume);
    void on_meta_data();
    void on_ready_read();
    void on_finished();
    void on_watchdog(const QString& code);
    void dispatch(const SseParser::Message& m);
    /// Emits closed() once and releases the reply.
    void close_with(bool clean, int status, const QString& code, const QString& message);
    void release_reply(bool abort);

    QPointer<QNetworkReply> reply_;
    SseParser parser_;
    QTimer* response_timer_ = nullptr;
    QTimer* stall_timer_ = nullptr;
    QString path_;
    QByteArray body_;
    bool replay_allowed_ = false;
    bool is_resume_ = false;
    bool accepted_ = false;
    bool closed_ = true;
    int status_ = 0;
};

} // namespace fincept::agent_tasks
