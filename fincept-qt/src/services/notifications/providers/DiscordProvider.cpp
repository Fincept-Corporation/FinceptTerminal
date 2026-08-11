#include "services/notifications/providers/DiscordProvider.h"

#include "network/http/HttpClient.h"

#include <QJsonArray>
#include <QJsonObject>

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

    HttpClient::instance().post(webhook_url_, body, [cb](Result<QJsonDocument> res) {
        // Discord answers a successful webhook post with 204 No Content. That is
        // not an error path: QNetworkReply reports NoError for 2xx, so a real 204
        // arrives here as is_ok() with an empty document.
        //
        // There used to be a `err.find("204")` substring test here. Once HttpClient
        // began appending the server's message to the error string, that matched any
        // 4xx/5xx whose body merely contained "204" — a Retry-After value, a byte
        // count, a snowflake fragment — and reported a rejected webhook as delivered.
        // Never infer a status code by substring-searching an error message.
        if (res.is_err())
            cb(false, QString::fromStdString(res.error()));
        else
            cb(true, {});
    });
}

} // namespace fincept::notifications
