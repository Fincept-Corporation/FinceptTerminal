#pragma once
// ProviderHttp — how every HTTP notification provider sends its request and
// turns the answer into (delivered, reason). The decision itself is
// classify_delivery() (DeliveryOutcome.h, pure); this file only feeds it what
// HttpClient reports: the HTTP status, the Content-Type and the body.

#include "network/http/HttpClient.h"
#include "services/notifications/providers/DeliveryOutcome.h"

#include <QJsonObject>
#include <QMap>
#include <QString>

#include <functional>

namespace fincept::notifications {

/// One provider request.
struct DeliveryRequest {
    enum class Verb {
        PostJson, ///< JSON body (most providers)
        PutJson,  ///< JSON body, PUT (Webhook provider set to PUT)
        PostForm, ///< application/x-www-form-urlencoded (Twilio SMS / WhatsApp)
    };
    DeliveryService service = DeliveryService::Generic;
    Verb verb = Verb::PostJson;
    QString url;
    QJsonObject json;            ///< PostJson / PutJson
    QMap<QString, QString> form; ///< PostForm
    HttpClient::Headers headers; ///< per-request credentials (never in the URL)
};

/// Sends `request` and calls `done(delivered, reason)` once, on HttpClient's
/// thread (the providers are not QObjects). `reason` is empty when delivered,
/// otherwise one plain, scrubbed sentence (no URL path, token or key).
void send_delivery(const DeliveryRequest& request, std::function<void(bool, QString)> done);

} // namespace fincept::notifications
