#include "services/notifications/providers/ProviderHttp.h"

#include <QJsonDocument>

#include <utility>

namespace fincept::notifications {

// Everything here runs inside an HttpClient callback: response_status() /
// response_header() describe the reply being delivered and are valid only there.

namespace {

/// The reply as far as an error result describes it.
DeliveryReply delivery_reply_from_error(const std::string& error) {
    DeliveryReply r;
    r.status = HttpClient::response_status();
    r.content_type = HttpClient::response_header("Content-Type");
    r.body_known = false;
    const int http_status = HttpClient::status_from_error(error);
    if (http_status > 0) {
        // An HTTP error answer ("HTTP_<status>[: <server sentence>]"): the client
        // keeps the server's sentence, not the body.
        r.status = http_status;
        r.client_message = HttpClient::message_from_error(error);
        return r;
    }
    if (r.status > 0)
        return r; // an answer whose body is not JSON (JSON path only): its status decides
    // No HTTP answer: transport failure ("Network error: …", URL scrubbed by
    // HttpClient) or a URL HttpClient refused to send.
    r.status = 0;
    r.client_message = QString::fromStdString(error);
    return r;
}

/// post_raw() / put_raw(): a 2xx body exactly as it arrived ("ok", "1", empty, JSON).
DeliveryReply delivery_reply_from_bytes(const Result<QByteArray>& res) {
    if (res.is_err())
        return delivery_reply_from_error(res.error());
    DeliveryReply r;
    r.status = HttpClient::response_status();
    r.content_type = HttpClient::response_header("Content-Type");
    r.body = res.value();
    if (r.status <= 0)
        r.status = 200; // a success always has a status; never call it "no answer"
    return r;
}

/// post_form(): HttpClient has no raw form-POST, so the JSON path. It reports a 2xx
/// that is not JSON as a parse error with response_status() still the 2xx (enough
/// to decide); the text of such a body is not available. Its only users (Twilio)
/// answer JSON.
DeliveryReply delivery_reply_from_json(DeliveryService service, const Result<QJsonDocument>& res) {
    if (res.is_err())
        return delivery_reply_from_error(res.error());
    DeliveryReply r;
    r.status = HttpClient::response_status();
    r.content_type = HttpClient::response_header("Content-Type");
    if (r.status <= 0)
        r.status = 200;
    if (delivery_reads_success_body(service) && !res.value().isNull())
        r.body = res.value().toJson(QJsonDocument::Compact);
    return r;
}

} // namespace

void send_delivery(const DeliveryRequest& request, std::function<void(bool, QString)> done) {
    const DeliveryService service = request.service;
    // No context object: the providers are not QObjects, and the callbacks hold
    // nothing but the caller's callback (NotificationService outlives every reply).
    auto& http = HttpClient::instance();

    if (request.verb == DeliveryRequest::Verb::PostForm) {
        http.post_form(
            request.url, request.form,
            [service, done = std::move(done)](Result<QJsonDocument> res) {
                const DeliveryOutcome o = classify_delivery(service, delivery_reply_from_json(service, res));
                done(o.delivered, o.reason);
            },
            nullptr, request.headers);
        return;
    }

    // JSON bodies: the raw forms hand any 2xx body over unparsed — nothing is
    // parsed on this thread, and a body that is not JSON is not an error.
    HttpClient::BytesCallback on_reply = [service, done = std::move(done)](Result<QByteArray> res) {
        const DeliveryOutcome o = classify_delivery(service, delivery_reply_from_bytes(res));
        done(o.delivered, o.reason);
    };
    if (request.verb == DeliveryRequest::Verb::PutJson)
        http.put_raw(request.url, request.json, std::move(on_reply), nullptr, request.headers);
    else
        http.post_raw(request.url, request.json, std::move(on_reply), nullptr, request.headers);
}

} // namespace fincept::notifications
