// CloudAgentClient.cpp — one streamed /chat/agent/stream (or …/resume) turn.

#include "services/agent_tasks/CloudAgentClient.h"

#include "core/logging/Logger.h"
#include "network/http/HttpClient.h"
#include "screens/chat_mode/ChatModeService.h"

#include <QJsonDocument>
#include <QJsonValue>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTimer>

namespace fincept::agent_tasks {

namespace cloud_agent_client_detail {
constexpr const char* kCloudClientTag = "CloudAgentClient";
constexpr int kCloudClientRawPreviewChars = 2000;
} // namespace cloud_agent_client_detail

using namespace cloud_agent_client_detail;
using chat_mode::ChatModeService;

CloudAgentClient::CloudAgentClient(QObject* parent) : QObject(parent) {
    response_timer_ = new QTimer(this);
    response_timer_->setSingleShot(true);
    response_timer_->setInterval(kResponseTimeoutMs);
    connect(response_timer_, &QTimer::timeout, this, [this]() { on_watchdog(QStringLiteral("no_response")); });

    stall_timer_ = new QTimer(this);
    stall_timer_->setSingleShot(true);
    stall_timer_->setInterval(kStallTimeoutMs);
    connect(stall_timer_, &QTimer::timeout, this, [this]() { on_watchdog(QStringLiteral("stalled")); });
}

CloudAgentClient::~CloudAgentClient() {
    release_reply(/*abort=*/true);
}

void CloudAgentClient::start_turn(const QString& session_uuid, const QString& message, const QString& mode,
                                  int profile_id) {
    QJsonObject body;
    body["message"] = message;
    body["session_id"] = session_uuid;
    body["mode"] = mode == QLatin1String("deep") ? QStringLiteral("deep") : QStringLiteral("lite");
    // Never true: trade proposals stay explicit cards the user decides.
    body["auto_approve"] = false;
    // The backend attaches a profile only when it is the caller's own
    // (chat.py _start_agent_turn); null means "no profile".
    body["profile_id"] = profile_id > 0 ? QJsonValue(profile_id) : QJsonValue(QJsonValue::Null);
    body["source"] = QJsonValue(QJsonValue::Null);
    // This client shows the agent's questions and answers them through
    // /chat/agent/stream/resume; without the opt-in the agent never pauses to ask.
    body["interactive"] = true;
    send(QStringLiteral("/chat/agent/stream"), QJsonDocument(body).toJson(QJsonDocument::Compact), true, false);
}

void CloudAgentClient::resume_turn(const QString& session_uuid, const QString& answer, const QString& mode) {
    // {session_id, feedback, mode?, auto_approve?} (apiback chat.py AgentResumeRequest);
    // the run continues in the mode that paused it, `mode` is informational.
    QJsonObject body;
    body["session_id"] = session_uuid;
    body["feedback"] = answer;
    body["mode"] = mode == QLatin1String("deep") ? QStringLiteral("deep") : QStringLiteral("lite");
    body["auto_approve"] = false;
    send(QStringLiteral("/chat/agent/stream/resume"), QJsonDocument(body).toJson(QJsonDocument::Compact), true, true);
}

void CloudAgentClient::detach() {
    if (closed_ && !reply_)
        return;
    closed_ = true;
    response_timer_->stop();
    stall_timer_->stop();
    release_reply(/*abort=*/true);
    parser_.reset();
    LOG_DEBUG(kCloudClientTag, QStringLiteral("Detached from %1 (the server keeps working on this turn)").arg(path_));
}

void CloudAgentClient::send(const QString& path, const QByteArray& body, bool allow_replay, bool is_resume) {
    release_reply(/*abort=*/true);
    path_ = path;
    body_ = body;
    replay_allowed_ = allow_replay;
    is_resume_ = is_resume;
    accepted_ = false;
    closed_ = false;
    status_ = 0;
    parser_.reset();

    auto& svc = ChatModeService::instance();
    QNetworkRequest req = svc.build_request(path);
    req.setRawHeader("Accept", "text/event-stream");
    req.setAttribute(QNetworkRequest::CacheLoadControlAttribute, QNetworkRequest::AlwaysNetwork);
    req.setTransferTimeout(0); // the response / stall watchdogs below decide
    QNetworkReply* r = svc.stream_network()->post(req, body);
    reply_ = r;
    connect(r, &QNetworkReply::metaDataChanged, this, &CloudAgentClient::on_meta_data);
    connect(r, &QNetworkReply::readyRead, this, &CloudAgentClient::on_ready_read);
    connect(r, &QNetworkReply::finished, this, &CloudAgentClient::on_finished);
    stall_timer_->stop();
    response_timer_->start();
    LOG_DEBUG(kCloudClientTag, QStringLiteral("POST %1 (%2 bytes)").arg(path).arg(body.size()));
}

void CloudAgentClient::on_meta_data() {
    if (!reply_ || closed_)
        return;
    const int status = reply_->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if (status <= 0)
        return;
    status_ = status;
    response_timer_->stop();
    stall_timer_->start();
    if (status >= 200 && status < 300 && !accepted_) {
        accepted_ = true;
        emit response_started(status);
    }
}

void CloudAgentClient::on_ready_read() {
    QNetworkReply* r = reply_;
    if (!r || closed_)
        return;
    if (status_ == 0)
        on_meta_data();
    stall_timer_->start(); // any data keeps the stream alive
    // An error answer (401/402/4xx/5xx) carries a JSON body, not events: leave it
    // unread so on_finished() can show the server's explanation.
    if (status_ >= 300)
        return;
    QPointer<CloudAgentClient> self(this);
    while (self && reply_ == r && !closed_ && r->canReadLine()) {
        SseParser::Message m;
        if (parser_.feed_line(r->readLine(), &m))
            dispatch(m);
    }
}

void CloudAgentClient::dispatch(const SseParser::Message& m) {
    QJsonParseError pe;
    const QJsonDocument doc = QJsonDocument::fromJson(m.data, &pe);
    QJsonObject data;
    if (pe.error == QJsonParseError::NoError && doc.isObject())
        data = doc.object();
    else
        data.insert(QStringLiteral("raw"), QString::fromUtf8(m.data.left(kCloudClientRawPreviewChars)));
    emit sse_event(m.event, data);
}

void CloudAgentClient::on_finished() {
    QNetworkReply* r = reply_;
    if (!r || closed_)
        return;
    response_timer_->stop();
    stall_timer_->stop();
    const int status = r->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const auto net_err = r->error();
    LOG_DEBUG(kCloudClientTag, QStringLiteral("%1 finished — HTTP %2, net_err=%3").arg(path_).arg(status).arg(net_err));

    if (net_err == QNetworkReply::NoError) {
        // Lines that arrived with the final chunk, then a block the server did not
        // terminate with a blank line.
        QPointer<CloudAgentClient> self(this);
        while (self && reply_ == r && !closed_ && r->canReadLine()) {
            SseParser::Message m;
            if (parser_.feed_line(r->readLine(), &m))
                dispatch(m);
        }
        if (!self || reply_ != r || closed_)
            return;
        const QByteArray rest = r->readAll();
        SseParser::Message m;
        if (!rest.isEmpty() && parser_.feed_line(rest, &m))
            dispatch(m);
        if (!self || reply_ != r || closed_)
            return;
        if (parser_.flush(&m))
            dispatch(m);
        if (!self || reply_ != r || closed_)
            return;
        close_with(true, status, {}, {});
        return;
    }

    const QByteArray raw = r->readAll();
    auto& svc = ChatModeService::instance();
    bool ip_blocked = false;
    if (svc.report_auth_failure(r, status, raw, &ip_blocked) && replay_allowed_) {
        // Refused only for the session token, before anything ran or was charged:
        // post the same turn once more with the current credentials.
        LOG_INFO(kCloudClientTag, "Re-sending the agent turn without the expired session token");
        release_reply(/*abort=*/false);
        send(path_, body_, /*allow_replay=*/false, is_resume_);
        return;
    }
    const QJsonObject err_body = QJsonDocument::fromJson(raw).object();
    const QString code = ChatModeService::reply_error_code(r);
    const int retry_after = ChatModeService::reply_retry_after(r);

    if (status >= 200 && status < 300) {
        close_with(false, status, QStringLiteral("connection_lost"),
                   tr("The connection to the Fincept server was lost (%1).")
                       .arg(ChatModeService::network_error_name(net_err)));
        return;
    }
    if (status == 0) {
        close_with(false, 0, QStringLiteral("network"),
                   ChatModeService::reply_error_text(0, {}, net_err, false, retry_after));
        return;
    }
    // A resume refused before anything was charged or stored because nothing is
    // waiting (409 not_waiting; a 409 without a code from servers that send none),
    // or that the server cannot route (older deployment): the answer goes out as an
    // ordinary message. (409 resume_in_progress and 400 invalid_request are errors.)
    if (is_resume_ && (code == QLatin1String("not_waiting") || (code.isEmpty() && status == 409) ||
                       ChatModeService::reply_route_missing(status, err_body, code))) {
        const QString server = status == 409 ? HttpClient::error_text_from_body(err_body) : QString();
        close_with(false, status, QStringLiteral("resume_rejected"), server);
        return;
    }
    const QString text = ChatModeService::reply_error_text(status, err_body, net_err, ip_blocked, retry_after);
    close_with(false, status, status == 402 ? QStringLiteral("insufficient_credits") : code, text);
}

void CloudAgentClient::on_watchdog(const QString& code) {
    if (!reply_ || closed_)
        return;
    const QString message = code == QLatin1String("no_response")
                                ? tr("The Fincept server did not answer within %1 s.").arg(kResponseTimeoutMs / 1000)
                                : tr("No data from the Fincept server for %1 s.").arg(kStallTimeoutMs / 1000);
    LOG_WARN(kCloudClientTag, QStringLiteral("Agent stream stopped by the watchdog path=%1 reason=%2").arg(path_, code));
    close_with(false, status_, code, message);
}

void CloudAgentClient::close_with(bool clean, int status, const QString& code, const QString& message) {
    if (closed_)
        return;
    closed_ = true;
    response_timer_->stop();
    stall_timer_->stop();
    release_reply(/*abort=*/!clean);
    if (!clean)
        LOG_WARN(kCloudClientTag, QStringLiteral("%1 closed (HTTP %2, %3): %4").arg(path_).arg(status).arg(code, message));
    emit closed(clean, status, code, message);
}

void CloudAgentClient::release_reply(bool abort) {
    QNetworkReply* r = reply_;
    reply_ = nullptr;
    if (!r)
        return;
    // abort() emits finished() synchronously: disconnect first.
    disconnect(r, nullptr, this, nullptr);
    if (abort && r->isRunning())
        r->abort();
    r->deleteLater();
}

} // namespace fincept::agent_tasks
