#pragma once
// DeliveryOutcome — the one decision "did this notification get delivered?" for
// every HTTP notification provider. Pure: Qt Core only, no network, no logging
// (the reason text is scrubbed with log_scrub(), defined in Logger.cpp), so
// tests/tst_notification_delivery.cpp covers each service's success and failure
// shapes without the app.
//
// The rule: an HTTP 2xx is delivered — whatever the body is (Slack and
// Mattermost answer the text "ok", Discord an empty 204, Teams "1" or an empty
// 202, a user's own webhook anything). A body is read only for the services
// whose API reports a failure inside a 2xx (Slack Web API / Workflow
// {"ok":false}, Telegram {"ok":false}, Pushover {"status":0}, PagerDuty
// {"status":"<not success>"}, Pushbullet {"error":…}, Twilio {"code":…}, the
// legacy Teams connector "Webhook message delivery failed …"). Anything else —
// no answer, 3xx, 4xx, 5xx — is not delivered, with one plain sentence why.

#include <QByteArray>
#include <QString>

namespace fincept::notifications {

/// Which service answered — decides which 2xx body conventions apply.
enum class DeliveryService {
    Generic,    ///< the user's own endpoint (Webhook provider, e-mail HTTP relay): any 2xx
    Slack,      ///< incoming webhook "ok" (text); Workflow / Web API JSON {"ok":false,"error":…}
    Mattermost, ///< incoming webhook "ok" (text)
    Discord,    ///< 204 empty, or 200 JSON message
    Teams,      ///< connector "1" (text) / Workflows 202 empty; 200 + "Webhook message delivery failed…"
    Gotify,     ///< 200 JSON message
    Ntfy,       ///< 200 JSON message
    Telegram,   ///< {"ok":true|false,"description":…}
    Pushover,   ///< {"status":1|0,"errors":[…]}
    Pushbullet, ///< push object, or {"error":{"message":…}}
    PagerDuty,  ///< 202 {"status":"success"}; {"status":"invalid event","message":…,"errors":[…]}
    Opsgenie,   ///< 202 {"result":…,"requestId":…}; errors are 4xx
    Twilio,     ///< 201 message resource; {"code":N,"message":…}
};

/// One HTTP answer as the provider received it.
struct DeliveryReply {
    /// HTTP status; 0 = no HTTP answer (could not connect, timed out, refused URL).
    int status = 0;
    /// Content-Type header as sent ("" when absent).
    QByteArray content_type;
    /// The body as received.
    QByteArray body;
    /// False when the body's text is not available — HttpClient reports a 4xx /
    /// 5xx only as its "HTTP_<status>: <sentence>" error, and its JSON path
    /// (post_form) a non-JSON 2xx only as "not JSON". `client_message` then stands
    /// in for the body.
    bool body_known = true;
    /// The HTTP client's own words: the transport failure for status 0
    /// ("Network error: …", URL already scrubbed by HttpClient), or the server's
    /// sentence it extracted from an error answer. Used only when the body says nothing.
    QString client_message;
};

struct DeliveryOutcome {
    bool delivered = false;
    /// Empty when delivered; otherwise one plain sentence, scrubbed (no URL path,
    /// token, key, e-mail address) and at most kMaxDeliveryReasonChars long.
    QString reason;
};

/// Longest reason handed to the UI / log.
inline constexpr int kMaxDeliveryReasonChars = 200;

/// delivered, or failed with the reason. Pure; any thread.
DeliveryOutcome classify_delivery(DeliveryService service, const DeliveryReply& reply);

/// True for the services whose 2xx body classify_delivery() reads (for every
/// other service the status alone decides, so the body need not be handed over).
bool delivery_reads_success_body(DeliveryService service);

/// The server's own explanation in an answer body (JSON error fields, or a short
/// plain-text body), scrubbed and shortened; empty when it carries none. HTML is
/// never returned. Pure; any thread. Exposed for the tests.
QString delivery_server_text(const QByteArray& content_type, const QByteArray& body);

} // namespace fincept::notifications
