#include "services/notifications/providers/DeliveryOutcome.h"

#include "core/logging/LogRedact.h"

#include <QChar>
#include <QCoreApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QJsonValue>
#include <QLatin1String>

#include <initializer_list>

namespace fincept::notifications {

namespace {

/// Longest server explanation folded into a reason (the head adds ~30 chars).
constexpr int kDeliveryServerTextChars = 160;

QString delivery_tr(const char* text) {
    return QCoreApplication::translate("NotificationDelivery", text);
}

/// One readable line: whitespace collapsed, secrets scrubbed, cut at `max_chars`.
QString delivery_clean(const QString& text, int max_chars) {
    QString s = fincept::log_scrub(text.simplified());
    if (s.size() > max_chars) {
        s.truncate(max_chars - 1);
        s += QChar(0x2026); // …
    }
    return s;
}

bool delivery_looks_like_json(const QByteArray& content_type, const QByteArray& body) {
    if (content_type.toLower().contains("json"))
        return true;
    const QByteArray t = body.trimmed();
    return t.startsWith('{') || t.startsWith('[');
}

/// The body as a JSON object; `is_object` false when it is not one.
QJsonObject delivery_json_object(const QByteArray& content_type, const QByteArray& body, bool* is_object) {
    *is_object = false;
    if (body.trimmed().isEmpty() || !delivery_looks_like_json(content_type, body))
        return {};
    QJsonParseError err;
    const QJsonDocument doc = QJsonDocument::fromJson(body, &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject())
        return {};
    *is_object = true;
    return doc.object();
}

/// A readable string held by `v`: the string itself, an object's message /
/// description / detail / error, or the first such entry of an array.
QString delivery_text_of(const QJsonValue& v) {
    if (v.isString())
        return v.toString().trimmed();
    if (v.isObject()) {
        const QJsonObject o = v.toObject();
        for (const char* key : {"message", "description", "detail", "error"}) {
            const QJsonValue inner = o.value(QLatin1String(key));
            if (inner.isString() && !inner.toString().trimmed().isEmpty())
                return inner.toString().trimmed();
        }
        return {};
    }
    if (v.isArray()) {
        const QJsonArray arr = v.toArray();
        for (const QJsonValue& entry : arr) {
            const QString t = delivery_text_of(entry);
            if (!t.isEmpty())
                return t;
        }
    }
    return {};
}

/// The explanation in a JSON error object, across the services' spellings:
/// Telegram "description", Gotify "errorDescription", most "message", Slack /
/// ntfy "error" (Pushbullet: an object), Pushover / PagerDuty "errors" (array).
QString delivery_json_error_text(const QJsonObject& o) {
    QString primary;
    for (const char* key : {"description", "errorDescription", "message", "error", "error_message", "detail"}) {
        primary = delivery_text_of(o.value(QLatin1String(key)));
        if (!primary.isEmpty())
            break;
    }
    const QString detail = delivery_text_of(o.value(QLatin1String("errors")));
    if (primary.isEmpty())
        return detail;
    if (!detail.isEmpty() && !primary.contains(detail))
        return primary + QStringLiteral(" - ") + detail;
    return primary;
}

/// First line of a plain-text body; empty for an empty or HTML body.
QString delivery_plain_text(const QByteArray& content_type, const QByteArray& body) {
    const QByteArray t = body.trimmed();
    if (t.isEmpty() || content_type.toLower().contains("html") || t.startsWith('<'))
        return {};
    QString s = QString::fromUtf8(t.left(1024));
    const qsizetype nl = s.indexOf(QLatin1Char('\n'));
    if (nl >= 0)
        s.truncate(nl);
    return s.trimmed();
}

DeliveryOutcome delivery_failed(const QString& reason) {
    return DeliveryOutcome{false, delivery_clean(reason, kMaxDeliveryReasonChars)};
}

/// A 2xx whose body says the message was not delivered.
DeliveryOutcome delivery_failed_in_2xx(const QString& server_text) {
    const QString said = delivery_clean(server_text, kDeliveryServerTextChars);
    if (said.isEmpty())
        return delivery_failed(delivery_tr("The service reported that the message was not delivered"));
    return delivery_failed(delivery_tr("Not delivered: %1").arg(said));
}

/// No HTTP answer at all: HttpClient's transport text ("Network error: <Qt reason>",
/// URL already scrubbed) or its own refusal ("Invalid URL: …").
DeliveryOutcome delivery_no_answer(const QString& client_message) {
    const QString msg = client_message.trimmed();
    const QLatin1String network_prefix("Network error:");
    if (msg.isEmpty())
        return delivery_failed(delivery_tr("Could not reach the service"));
    if (!msg.startsWith(network_prefix))
        return delivery_failed(msg);
    QString detail = msg.mid(network_prefix.size()).trimmed();
    // A transfer timeout aborts the reply: Qt says only "Operation canceled".
    if (detail.compare(QLatin1String("Operation canceled"), Qt::CaseInsensitive) == 0)
        detail = delivery_tr("the request timed out");
    if (detail.isEmpty())
        return delivery_failed(delivery_tr("Could not reach the service"));
    return delivery_failed(delivery_tr("Could not reach the service: %1")
                               .arg(delivery_clean(detail, kDeliveryServerTextChars)));
}

} // namespace

QString delivery_server_text(const QByteArray& content_type, const QByteArray& body) {
    bool is_object = false;
    const QJsonObject o = delivery_json_object(content_type, body, &is_object);
    QString text;
    if (is_object) {
        text = delivery_json_error_text(o);
    } else if (delivery_looks_like_json(content_type, body)) {
        const QJsonDocument doc = QJsonDocument::fromJson(body);
        if (doc.isArray())
            text = delivery_text_of(QJsonValue(doc.array()));
        // JSON-typed but unparsable: say nothing rather than echo a fragment.
    } else {
        text = delivery_plain_text(content_type, body);
    }
    return delivery_clean(text, kDeliveryServerTextChars);
}

DeliveryOutcome classify_delivery(DeliveryService service, const DeliveryReply& reply) {
    const int status = reply.status;
    if (status <= 0)
        return delivery_no_answer(reply.client_message);

    if (status < 200 || status >= 300) {
        QString says = reply.body_known ? delivery_server_text(reply.content_type, reply.body) : QString();
        if (says.isEmpty())
            says = delivery_clean(reply.client_message, kDeliveryServerTextChars);
        QString head;
        if (status == 429)
            head = delivery_tr("Rate limited (HTTP 429)");
        else if (status >= 500)
            head = delivery_tr("Service error (HTTP %1)").arg(status);
        else if (status < 400) // 1xx / 3xx: never a delivery
            head = delivery_tr("Unexpected answer (HTTP %1)").arg(status);
        else
            head = delivery_tr("Rejected (HTTP %1)").arg(status);
        return delivery_failed(says.isEmpty() ? head : head + QStringLiteral(": ") + says);
    }

    // 2xx: delivered, unless this service's API reports a failure in the body.
    // A body the client could not hand over (a non-JSON 2xx on post_form's JSON
    // path) can carry none of the JSON conventions below.
    if (!reply.body_known || !delivery_reads_success_body(service))
        return DeliveryOutcome{true, {}};

    bool is_object = false;
    const QJsonObject o = delivery_json_object(reply.content_type, reply.body, &is_object);
    switch (service) {
        case DeliveryService::Slack:    // Web API / Workflow webhooks: {"ok":false,"error":"…"}
        case DeliveryService::Telegram: // {"ok":false,"error_code":N,"description":"…"}
            if (is_object && o.contains(QLatin1String("ok")) && !o.value(QLatin1String("ok")).toBool(true))
                return delivery_failed_in_2xx(delivery_json_error_text(o));
            break;
        case DeliveryService::Pushover: { // {"status":1} on success, any other value is an error
            const QJsonValue s = o.value(QLatin1String("status"));
            const bool bad = s.isDouble() ? s.toInt() != 1 : (s.isString() && s.toString().trimmed() != QLatin1String("1"));
            if (is_object && bad)
                return delivery_failed_in_2xx(delivery_json_error_text(o));
            break;
        }
        case DeliveryService::PagerDuty: { // {"status":"success"}
            const QJsonValue s = o.value(QLatin1String("status"));
            if (is_object && s.isString() && s.toString().compare(QLatin1String("success"), Qt::CaseInsensitive) != 0)
                return delivery_failed_in_2xx(delivery_json_error_text(o));
            break;
        }
        case DeliveryService::Pushbullet: // {"error":{"message":"…"}}
            if (is_object && o.contains(QLatin1String("error")))
                return delivery_failed_in_2xx(delivery_json_error_text(o));
            break;
        case DeliveryService::Twilio: // errors carry a numeric "code"; a message resource never does
            if (is_object && o.contains(QLatin1String("code")) && !o.value(QLatin1String("code")).isNull())
                return delivery_failed_in_2xx(delivery_json_error_text(o));
            break;
        case DeliveryService::Teams: { // legacy connector: HTTP 200 + "Webhook message delivery failed with error: …"
            if (is_object)
                break;
            const QString text = delivery_plain_text(reply.content_type, reply.body);
            if (text.contains(QLatin1String("delivery failed"), Qt::CaseInsensitive) ||
                text.contains(QLatin1String("returned HTTP error"), Qt::CaseInsensitive))
                return delivery_failed_in_2xx(text);
            break;
        }
        case DeliveryService::Generic:
        case DeliveryService::Mattermost:
        case DeliveryService::Discord:
        case DeliveryService::Gotify:
        case DeliveryService::Ntfy:
        case DeliveryService::Opsgenie: // 202 = accepted; its errors are 4xx (and carry "requestId" too)
            break;
    }
    return DeliveryOutcome{true, {}};
}

bool delivery_reads_success_body(DeliveryService service) {
    switch (service) {
        case DeliveryService::Slack:
        case DeliveryService::Telegram:
        case DeliveryService::Pushover:
        case DeliveryService::PagerDuty:
        case DeliveryService::Pushbullet:
        case DeliveryService::Twilio:
        case DeliveryService::Teams:
            return true;
        case DeliveryService::Generic:
        case DeliveryService::Mattermost:
        case DeliveryService::Discord:
        case DeliveryService::Gotify:
        case DeliveryService::Ntfy:
        case DeliveryService::Opsgenie:
            return false;
    }
    return false;
}

} // namespace fincept::notifications
