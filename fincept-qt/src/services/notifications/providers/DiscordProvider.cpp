#include "services/notifications/providers/DiscordProvider.h"

#include "services/notifications/providers/ProviderHttp.h"

#include <QJsonArray>
#include <QJsonObject>

#include <utility>

namespace fincept::notifications {

void DiscordProvider::load_fields(SettingsRepository& r, const QString& cat) {
    webhook_url_ = get_secret(r, cat + ".webhook_url");
}

void DiscordProvider::save_fields(SettingsRepository& r, const QString& cat) {
    set_secret(r, cat + ".webhook_url", webhook_url_, cat);
}

void DiscordProvider::send(const NotificationRequest& req, std::function<void(bool, QString)> cb) {
    if (!is_configured()) {
        cb(false, "Not configured");
        return;
    }

    // Embed accent shown inside Discord, not by the terminal: fixed colours,
    // independent of the terminal theme.
    const int color = [&]() -> int {
        switch (req.level) {
            case NotifLevel::Warning:
                return 0xF59E0B; // amber
            case NotifLevel::Alert:
                return 0xF97316; // orange
            case NotifLevel::Critical:
                return 0xEF4444; // red
            default:
                return 0x06B6D4; // cyan
        }
    }();

    QJsonObject embed;
    embed["title"] = req.title;
    embed["description"] = req.message;
    embed["color"] = color;
    embed["timestamp"] = req.timestamp.toUTC().toString(Qt::ISODate);

    QJsonArray embeds;
    embeds.append(embed);

    QJsonObject body;
    body["embeds"] = embeds;

    // Discord answers a successful webhook post with an empty 204 No Content. The
    // delivery decision is made on the HTTP status (send_delivery /
    // classify_delivery), never by searching an error message for "204" (that
    // once matched any 4xx/5xx whose body merely contained "204" and reported a
    // rejected webhook as delivered).
    DeliveryRequest request;
    request.service = DeliveryService::Discord;
    request.url = webhook_url_;
    request.json = body;
    send_delivery(request, std::move(cb));
}

} // namespace fincept::notifications
