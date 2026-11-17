// Unit tests for src/services/notifications/providers/DeliveryOutcome.{h,cpp}
// (pure, Qt Core only; the reason text is scrubbed with log_scrub() from Logger.cpp).
//
// classify_delivery() is the one decision every HTTP notification provider makes
// on its answer. The bug it fixes: a successful reply that is not JSON — Slack and
// Mattermost answer the text "ok", Discord an empty 204, Teams "1" or an empty 202
// — was reported as "Delivery failed" because the JSON parse error was taken for a
// failed delivery. The rule now: HTTP 2xx is delivered; a body is read only for the
// services that report a failure inside a 2xx; everything else fails with one plain,
// scrubbed sentence.
//
// MSVC: raw string literals are kept out of macro arguments (QVERIFY / QCOMPARE) —
// the preprocessor mis-splits one that contains a quote or an apostrophe.

#include "services/notifications/providers/DeliveryOutcome.h"

#include <QByteArray>
#include <QString>
#include <QTest>

using fincept::notifications::classify_delivery;
using fincept::notifications::delivery_reads_success_body;
using fincept::notifications::delivery_server_text;
using fincept::notifications::DeliveryOutcome;
using fincept::notifications::DeliveryReply;
using fincept::notifications::DeliveryService;
using fincept::notifications::kMaxDeliveryReasonChars;

namespace {

DeliveryReply answer(int status, const QByteArray& content_type, const QByteArray& body) {
    DeliveryReply r;
    r.status = status;
    r.content_type = content_type;
    r.body = body;
    return r;
}

DeliveryReply json(int status, const QByteArray& body) {
    return answer(status, "application/json; charset=utf-8", body);
}

DeliveryReply text(int status, const QByteArray& body) {
    return answer(status, "text/plain; charset=utf-8", body);
}

/// What today's HttpClient JSON path reports: the status, but no body text.
DeliveryReply unseen(int status, const QString& client_message = {}) {
    DeliveryReply r;
    r.status = status;
    r.body_known = false;
    r.client_message = client_message;
    return r;
}

DeliveryReply no_answer(const QString& client_message) {
    DeliveryReply r;
    r.status = 0;
    r.body_known = false;
    r.client_message = client_message;
    return r;
}

bool delivered(DeliveryService s, const DeliveryReply& r) {
    const DeliveryOutcome o = classify_delivery(s, r);
    return o.delivered && o.reason.isEmpty();
}

QString failure(DeliveryService s, const DeliveryReply& r) {
    const DeliveryOutcome o = classify_delivery(s, r);
    return o.delivered ? QStringLiteral("<delivered>") : o.reason;
}

} // namespace

class TstNotificationDelivery : public QObject {
    Q_OBJECT

  private slots:
    // Success shapes that are not JSON (the bug)
    void slack_and_mattermost_text_ok_is_delivered();
    void discord_empty_204_is_delivered();
    void teams_text_1_and_empty_202_are_delivered();
    void generic_webhook_any_2xx_body_is_delivered();
    void non_json_2xx_without_its_text_is_delivered_for_every_service();

    // Error-in-2xx conventions (kept per service)
    void slack_api_ok_false_is_a_failure();
    void telegram_ok_flag();
    void pushover_status_flag();
    void pagerduty_status_text();
    void pushbullet_error_object();
    void twilio_error_code();
    void teams_connector_failure_text_in_200();
    void opsgenie_202_is_delivered_with_or_without_request_id();
    void gotify_and_ntfy_json_success();

    // Non-2xx answers
    void rejected_answers_carry_the_server_sentence();
    void rejected_answer_with_unseen_body_uses_the_client_sentence();
    void rate_limit_server_error_and_redirect_heads();
    void html_error_page_is_never_shown();

    // No answer
    void transport_failure_reads_plainly();
    void timeout_reads_as_timed_out();
    void refused_url_keeps_the_client_text();

    // Privacy / size
    void reasons_are_scrubbed();
    void reasons_are_bounded();
    void server_text_extraction();
    void services_that_read_a_success_body();
};

// ── Success shapes that are not JSON ───────────────────────────────────────────

void TstNotificationDelivery::slack_and_mattermost_text_ok_is_delivered() {
    QVERIFY(delivered(DeliveryService::Slack, text(200, "ok")));
    QVERIFY(delivered(DeliveryService::Mattermost, text(200, "ok")));
    QVERIFY(delivered(DeliveryService::Mattermost, answer(200, "", "ok")));
}

void TstNotificationDelivery::discord_empty_204_is_delivered() {
    QVERIFY(delivered(DeliveryService::Discord, answer(204, "", "")));
    QVERIFY(delivered(DeliveryService::Discord, unseen(204)));
    // ?wait=true answers the created message as JSON.
    const QByteArray message = R"({"id":"1234","type":0,"content":""})";
    QVERIFY(delivered(DeliveryService::Discord, json(200, message)));
}

void TstNotificationDelivery::teams_text_1_and_empty_202_are_delivered() {
    QVERIFY(delivered(DeliveryService::Teams, text(200, "1")));
    QVERIFY(delivered(DeliveryService::Teams, answer(202, "", "")));
    QVERIFY(delivered(DeliveryService::Teams, unseen(200)));
}

void TstNotificationDelivery::generic_webhook_any_2xx_body_is_delivered() {
    QVERIFY(delivered(DeliveryService::Generic, answer(200, "", "")));
    QVERIFY(delivered(DeliveryService::Generic, answer(201, "text/html", "<html><body>Thanks</body></html>")));
    QVERIFY(delivered(DeliveryService::Generic, text(202, "accepted")));
    const QByteArray received = R"({"received":true})";
    QVERIFY(delivered(DeliveryService::Generic, json(200, received)));
    // No documented convention for a user's own endpoint: its JSON is not second-guessed.
    const QByteArray success_false = R"({"success":false})";
    QVERIFY(delivered(DeliveryService::Generic, json(200, success_false)));
}

void TstNotificationDelivery::non_json_2xx_without_its_text_is_delivered_for_every_service() {
    const DeliveryService all[] = {
        DeliveryService::Generic,  DeliveryService::Slack,      DeliveryService::Mattermost,
        DeliveryService::Discord,  DeliveryService::Teams,      DeliveryService::Gotify,
        DeliveryService::Ntfy,     DeliveryService::Telegram,   DeliveryService::Pushover,
        DeliveryService::Pushbullet, DeliveryService::PagerDuty, DeliveryService::Opsgenie,
        DeliveryService::Twilio,
    };
    for (DeliveryService s : all) {
        const QByteArray which = QByteArray::number(static_cast<int>(s));
        QVERIFY2(delivered(s, unseen(200)), which.constData());
        QVERIFY2(delivered(s, unseen(204)), which.constData());
    }
}

// ── Error-in-2xx conventions ───────────────────────────────────────────────────

void TstNotificationDelivery::slack_api_ok_false_is_a_failure() {
    const QByteArray ok_true = R"({"ok":true})";
    const QByteArray ok_false = R"({"ok":false,"error":"channel_not_found"})";
    QVERIFY(delivered(DeliveryService::Slack, json(200, ok_true)));
    QCOMPARE(failure(DeliveryService::Slack, json(200, ok_false)), QStringLiteral("Not delivered: channel_not_found"));
    // Another service's "ok":false means nothing there (Mattermost has no such convention).
    QVERIFY(delivered(DeliveryService::Mattermost, json(200, ok_false)));
}

void TstNotificationDelivery::telegram_ok_flag() {
    const QByteArray sent = R"({"ok":true,"result":{"message_id":42,"chat":{"id":1},"text":"hi"}})";
    const QByteArray refused = R"({"ok":false,"error_code":400,"description":"Bad Request: chat not found"})";
    const QByteArray bare = R"({"ok":false})";
    QVERIFY(delivered(DeliveryService::Telegram, json(200, sent)));
    QCOMPARE(failure(DeliveryService::Telegram, json(200, refused)),
             QStringLiteral("Not delivered: Bad Request: chat not found"));
    // Telegram mirrors the error in the HTTP status as well.
    QCOMPARE(failure(DeliveryService::Telegram, json(400, refused)),
             QStringLiteral("Rejected (HTTP 400): Bad Request: chat not found"));
    QCOMPARE(failure(DeliveryService::Telegram, json(200, bare)),
             QStringLiteral("The service reported that the message was not delivered"));
}

void TstNotificationDelivery::pushover_status_flag() {
    const QByteArray sent = R"({"status":1,"request":"5042853c"})";
    const QByteArray bad_user = R"({"user":"invalid","errors":["user identifier is invalid"],"status":0})";
    const QByteArray bad_token = R"({"token":"invalid","errors":["application token is invalid"],"status":0})";
    QVERIFY(delivered(DeliveryService::Pushover, json(200, sent)));
    QCOMPARE(failure(DeliveryService::Pushover, json(200, bad_user)),
             QStringLiteral("Not delivered: user identifier is invalid"));
    QCOMPARE(failure(DeliveryService::Pushover, json(400, bad_token)),
             QStringLiteral("Rejected (HTTP 400): application token is invalid"));
}

void TstNotificationDelivery::pagerduty_status_text() {
    const QByteArray sent = R"({"status":"success","message":"Event processed","dedup_key":"x"})";
    const QByteArray invalid = R"({"status":"invalid event","message":"Event object is invalid"})";
    const QByteArray invalid_detail = R"({"status":"invalid event","message":"Event object is invalid",)"
                                      R"j("errors":["Length of routing_key is incorrect (should be 32 characters)"]})j";
    QVERIFY(delivered(DeliveryService::PagerDuty, json(202, sent)));
    QCOMPARE(failure(DeliveryService::PagerDuty, json(202, invalid)),
             QStringLiteral("Not delivered: Event object is invalid"));
    QCOMPARE(failure(DeliveryService::PagerDuty, json(400, invalid_detail)),
             QStringLiteral("Rejected (HTTP 400): Event object is invalid - Length of routing_key is incorrect "
                            "(should be 32 characters)"));
}

void TstNotificationDelivery::pushbullet_error_object() {
    const QByteArray push = R"({"active":true,"iden":"ujpah72o0","type":"note","title":"t","body":"b"})";
    const QByteArray err = R"({"error":{"code":"invalid_access_token","type":"invalid_request",)"
                           R"j("message":"Access token is missing or invalid.","cat":"~(=^.^)"}})j";
    QVERIFY(delivered(DeliveryService::Pushbullet, json(200, push)));
    QCOMPARE(failure(DeliveryService::Pushbullet, json(200, err)),
             QStringLiteral("Not delivered: Access token is missing or invalid."));
    QCOMPARE(failure(DeliveryService::Pushbullet, json(401, err)),
             QStringLiteral("Rejected (HTTP 401): Access token is missing or invalid."));
}

void TstNotificationDelivery::twilio_error_code() {
    // A message resource: "error_code" / "error_message" are null, there is no "code".
    const QByteArray resource = R"({"sid":"SM1","status":"queued","error_code":null,"error_message":null,)"
                                R"("to":"+15558675310","body":"x"})";
    const QByteArray err = R"({"code":21211,"message":"The To number 123 is not a valid phone number.",)"
                           R"("more_info":"https://www.twilio.com/docs/errors/21211","status":400})";
    QVERIFY(delivered(DeliveryService::Twilio, json(201, resource)));
    QCOMPARE(failure(DeliveryService::Twilio, json(400, err)),
             QStringLiteral("Rejected (HTTP 400): The To number 123 is not a valid phone number."));
    QCOMPARE(failure(DeliveryService::Twilio, json(200, err)),
             QStringLiteral("Not delivered: The To number 123 is not a valid phone number."));
}

void TstNotificationDelivery::teams_connector_failure_text_in_200() {
    const QByteArray failed = "Webhook message delivery failed with error: Microsoft Teams endpoint returned HTTP "
                              "error 429 with ContextId tcid=0,server=msgapi-production-eus-azsc2-4-17,cv=deadbeef.0.";
    const QString r = failure(DeliveryService::Teams, text(200, failed));
    QVERIFY2(r.startsWith(QStringLiteral("Not delivered: Webhook message delivery failed with error: Microsoft "
                                         "Teams endpoint returned HTTP error 429")),
             qPrintable(r));
    QCOMPARE(failure(DeliveryService::Teams, text(400, "Summary or Text is required.")),
             QStringLiteral("Rejected (HTTP 400): Summary or Text is required."));
    // The same text from a user's own endpoint is not Teams' convention.
    QVERIFY(delivered(DeliveryService::Generic, text(200, "Webhook message delivery failed")));
}

void TstNotificationDelivery::opsgenie_202_is_delivered_with_or_without_request_id() {
    const QByteArray accepted = R"({"result":"Request will be processed","took":0.302,"requestId":"43a29c5c"})";
    const QByteArray accepted_no_id = R"({"result":"Request will be processed"})";
    const QByteArray bad_key = R"({"message":"Key format is not valid!","took":0.001,"requestId":"a1"})";
    QVERIFY(delivered(DeliveryService::Opsgenie, json(202, accepted)));
    QVERIFY(delivered(DeliveryService::Opsgenie, json(202, accepted_no_id)));
    QCOMPARE(failure(DeliveryService::Opsgenie, json(422, bad_key)),
             QStringLiteral("Rejected (HTTP 422): Key format is not valid!"));
}

void TstNotificationDelivery::gotify_and_ntfy_json_success() {
    const QByteArray gotify_sent = R"({"id":25,"appid":5,"message":"m","title":"t","priority":5})";
    const QByteArray gotify_401 = R"({"error":"Unauthorized","errorCode":401,"errorDescription":"you need to )"
                                  R"(provide a valid access token or user credentials to access this api"})";
    const QByteArray ntfy_sent = R"({"id":"sPs71M8A2T","time":1643935928,"event":"message","topic":"t"})";
    const QByteArray ntfy_403 = R"({"code":40301,"http":403,"error":"forbidden"})";
    QVERIFY(delivered(DeliveryService::Gotify, json(200, gotify_sent)));
    QCOMPARE(failure(DeliveryService::Gotify, json(401, gotify_401)),
             QStringLiteral("Rejected (HTTP 401): you need to provide a valid access token or user credentials to "
                            "access this api"));
    QVERIFY(delivered(DeliveryService::Ntfy, json(200, ntfy_sent)));
    QCOMPARE(failure(DeliveryService::Ntfy, json(403, ntfy_403)), QStringLiteral("Rejected (HTTP 403): forbidden"));
}

// ── Non-2xx answers ────────────────────────────────────────────────────────────

void TstNotificationDelivery::rejected_answers_carry_the_server_sentence() {
    const QByteArray discord_404 = R"({"message":"Unknown Webhook","code":10015})";
    const QByteArray mattermost_400 = R"({"id":"web.incoming_webhook.invalid.app_error","message":"Invalid webhook.",)"
                                      R"("detailed_error":"","status_code":400})";
    QCOMPARE(failure(DeliveryService::Slack, text(404, "no_service")), QStringLiteral("Rejected (HTTP 404): no_service"));
    QCOMPARE(failure(DeliveryService::Slack, text(400, "invalid_payload")),
             QStringLiteral("Rejected (HTTP 400): invalid_payload"));
    QCOMPARE(failure(DeliveryService::Discord, json(404, discord_404)),
             QStringLiteral("Rejected (HTTP 404): Unknown Webhook"));
    QCOMPARE(failure(DeliveryService::Mattermost, json(400, mattermost_400)),
             QStringLiteral("Rejected (HTTP 400): Invalid webhook."));
    QCOMPARE(failure(DeliveryService::Generic, answer(404, "", "")), QStringLiteral("Rejected (HTTP 404)"));
}

void TstNotificationDelivery::rejected_answer_with_unseen_body_uses_the_client_sentence() {
    QCOMPARE(failure(DeliveryService::Discord, unseen(404, QStringLiteral("Unknown Webhook"))),
             QStringLiteral("Rejected (HTTP 404): Unknown Webhook"));
    QCOMPARE(failure(DeliveryService::Telegram, unseen(401)), QStringLiteral("Rejected (HTTP 401)"));
    // A known body that says nothing falls back to the client's sentence too.
    DeliveryReply r = answer(400, "", "");
    r.client_message = QStringLiteral("Bad payload");
    QCOMPARE(failure(DeliveryService::Generic, r), QStringLiteral("Rejected (HTTP 400): Bad payload"));
}

void TstNotificationDelivery::rate_limit_server_error_and_redirect_heads() {
    const QByteArray limited = R"({"message":"You are being rate limited.","retry_after":0.6,"global":false})";
    const QByteArray ok_true = R"({"ok":true})";
    QCOMPARE(failure(DeliveryService::Discord, json(429, limited)),
             QStringLiteral("Rate limited (HTTP 429): You are being rate limited."));
    QCOMPARE(failure(DeliveryService::Generic, unseen(503)), QStringLiteral("Service error (HTTP 503)"));
    QCOMPARE(failure(DeliveryService::Generic, unseen(302)), QStringLiteral("Unexpected answer (HTTP 302)"));
    QCOMPARE(failure(DeliveryService::Generic, json(301, ok_true)), QStringLiteral("Unexpected answer (HTTP 301)"));
}

void TstNotificationDelivery::html_error_page_is_never_shown() {
    QCOMPARE(failure(DeliveryService::Generic,
                     answer(502, "text/html", "<html><head><title>502 Bad Gateway</title></head></html>")),
             QStringLiteral("Service error (HTTP 502)"));
    QCOMPARE(failure(DeliveryService::Teams, answer(500, "", "<!DOCTYPE html><html>oops</html>")),
             QStringLiteral("Service error (HTTP 500)"));
}

// ── No answer ──────────────────────────────────────────────────────────────────

void TstNotificationDelivery::transport_failure_reads_plainly() {
    QCOMPARE(failure(DeliveryService::Telegram,
                     no_answer(QStringLiteral("Network error: Host api.telegram.org not found"))),
             QStringLiteral("Could not reach the service: Host api.telegram.org not found"));
    QCOMPARE(failure(DeliveryService::Generic, no_answer({})), QStringLiteral("Could not reach the service"));
    QCOMPARE(failure(DeliveryService::Generic, no_answer(QStringLiteral("Network error:"))),
             QStringLiteral("Could not reach the service"));
}

void TstNotificationDelivery::timeout_reads_as_timed_out() {
    QCOMPARE(failure(DeliveryService::Slack, no_answer(QStringLiteral("Network error: Operation canceled"))),
             QStringLiteral("Could not reach the service: the request timed out"));
}

void TstNotificationDelivery::refused_url_keeps_the_client_text() {
    const QString refused =
        QStringLiteral("Invalid URL: expected http:// or https:// (or an API path starting with a slash)");
    QCOMPARE(failure(DeliveryService::Generic, no_answer(refused)), refused);
}

// ── Privacy / size ─────────────────────────────────────────────────────────────

void TstNotificationDelivery::reasons_are_scrubbed() {
    // A webhook URL is the secret itself; a Telegram URL carries the bot token.
    const QString hook = QStringLiteral("https://hooks.slack.com/services/T0001/B0002/Xq9ZbLkP2mN7rT4vW8yA1cD3");
    const QString tg_secret = QStringLiteral("AAHdqTcvCH1vGWJxfSeofSAs0K5PALDsaw");
    const QString tg =
        QStringLiteral("https://api.telegram.org/bot123456789:") + tg_secret + QStringLiteral("/sendMessage");

    const QString r1 = failure(DeliveryService::Slack, no_answer(QStringLiteral("Network error: Error transferring ") +
                                                                 hook + QStringLiteral(" - server replied: Not Found")));
    QVERIFY2(!r1.contains(QStringLiteral("/services/")) && !r1.contains(QStringLiteral("Xq9ZbLkP2mN7rT4vW8yA1cD3")),
             qPrintable(r1));
    QVERIFY2(r1.startsWith(QStringLiteral("Could not reach the service: Error transferring https://hooks.slack.com")),
             qPrintable(r1));

    const QString r2 = failure(DeliveryService::Telegram, unseen(404, QStringLiteral("Not Found: ") + tg));
    QVERIFY2(!r2.contains(tg_secret) && !r2.contains(QStringLiteral("sendMessage")), qPrintable(r2));

    const QByteArray tg_body = QByteArray(R"({"ok":false,"error_code":401,"description":"Unauthorized token )") +
                               tg_secret.toUtf8() + QByteArray(R"("})");
    const QString r3 = failure(DeliveryService::Telegram, json(401, tg_body));
    QVERIFY2(!r3.contains(tg_secret), qPrintable(r3));
    QVERIFY2(r3.startsWith(QStringLiteral("Rejected (HTTP 401): Unauthorized")), qPrintable(r3));

    const QByteArray relay_body = R"({"message":"mailbox alice@example.com unknown, api_key=sk-abc123def456"})";
    const QString r4 = failure(DeliveryService::Generic, json(400, relay_body));
    QVERIFY2(!r4.contains(QStringLiteral("alice@example.com")) && !r4.contains(QStringLiteral("sk-abc123def456")),
             qPrintable(r4));
}

void TstNotificationDelivery::reasons_are_bounded() {
    const QByteArray long_text(5000, 'x');
    const QString r1 = failure(DeliveryService::Generic, text(400, "error " + long_text));
    QVERIFY(r1.size() <= kMaxDeliveryReasonChars);
    QVERIFY(r1.startsWith(QStringLiteral("Rejected (HTTP 400): error ")));
    const QByteArray slack_long = QByteArray(R"({"ok":false,"error":")") + long_text + QByteArray(R"("})");
    const QString r2 = failure(DeliveryService::Slack, json(200, slack_long));
    QVERIFY(r2.size() <= kMaxDeliveryReasonChars);
    QVERIFY(r2.startsWith(QStringLiteral("Not delivered: xxx")));
    const QString r3 =
        failure(DeliveryService::Generic, no_answer(QStringLiteral("Network error: ") + QString::fromLatin1(long_text)));
    QVERIFY(r3.size() <= kMaxDeliveryReasonChars);
    // Only the first line of a text body; whitespace collapsed.
    QCOMPARE(failure(DeliveryService::Generic, text(400, "  first   line \nsecond line")),
             QStringLiteral("Rejected (HTTP 400): first line"));
}

void TstNotificationDelivery::server_text_extraction() {
    const QByteArray detail = R"({"detail":"Not authenticated"})";
    const QByteArray nested = R"({"error":{"message":"nested"}})";
    const QByteArray array = R"(["first problem","second"])";
    const QByteArray no_text = R"({"code":5,"took":1})";
    const QByteArray sniffed = R"({"message":"sniffed without a content type"})";
    QCOMPARE(delivery_server_text("application/json", detail), QStringLiteral("Not authenticated"));
    QCOMPARE(delivery_server_text("application/json", nested), QStringLiteral("nested"));
    QCOMPARE(delivery_server_text("application/json", array), QStringLiteral("first problem"));
    QCOMPARE(delivery_server_text("application/json", no_text), QString());
    QCOMPARE(delivery_server_text("application/json", "{broken"), QString());
    QCOMPARE(delivery_server_text("", sniffed), QStringLiteral("sniffed without a content type"));
    QCOMPARE(delivery_server_text("text/html", "Bad Gateway"), QString());
    QCOMPARE(delivery_server_text("", ""), QString());
}

void TstNotificationDelivery::services_that_read_a_success_body() {
    QVERIFY(delivery_reads_success_body(DeliveryService::Slack));
    QVERIFY(delivery_reads_success_body(DeliveryService::Telegram));
    QVERIFY(delivery_reads_success_body(DeliveryService::Pushover));
    QVERIFY(delivery_reads_success_body(DeliveryService::PagerDuty));
    QVERIFY(delivery_reads_success_body(DeliveryService::Pushbullet));
    QVERIFY(delivery_reads_success_body(DeliveryService::Twilio));
    QVERIFY(delivery_reads_success_body(DeliveryService::Teams));
    QVERIFY(!delivery_reads_success_body(DeliveryService::Generic));
    QVERIFY(!delivery_reads_success_body(DeliveryService::Mattermost));
    QVERIFY(!delivery_reads_success_body(DeliveryService::Discord));
    QVERIFY(!delivery_reads_success_body(DeliveryService::Gotify));
    QVERIFY(!delivery_reads_success_body(DeliveryService::Ntfy));
    QVERIFY(!delivery_reads_success_body(DeliveryService::Opsgenie));
}

QTEST_GUILESS_MAIN(TstNotificationDelivery)
#include "tst_notification_delivery.moc"
