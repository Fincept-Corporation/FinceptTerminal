#include "services/notifications/providers/TelegramProvider.h"

#include "network/http/HttpClient.h"

#include <QJsonObject>

namespace fincept::notifications {

void TelegramProvider::load_fields(SettingsRepository& r, const QString& cat) {
    bot_token_ = get_secret(r, cat + ".bot_token");
    chat_id_ = get_str(r, cat + ".chat_id");
}

void TelegramProvider::save_fields(SettingsRepository& r, const QString& cat) {
    set_secret(r, cat + ".bot_token", bot_token_, cat);
    r.set(cat + ".chat_id", chat_id_, cat);
}

void TelegramProvider::send(const NotificationRequest& req, std::function<void(bool, QString)> cb) {
    if (!is_configured()) {
        cb(false, "Not configured");
        return;
    }

    const QString url = QString("https://api.telegram.org/bot%1/sendMessage").arg(bot_token_);

    const QString level_tag = [&]() -> QString {
        switch (req.level) {
            case NotifLevel::Warning:
                return QStringLiteral(u"\u26A0\uFE0F "); // warning sign
            case NotifLevel::Alert:
                return QStringLiteral(u"\U0001F514 "); // bell
            case NotifLevel::Critical:
                return QStringLiteral(u"\U0001F6A8 "); // rotating light
            default:
                return QStringLiteral(u"\u2139\uFE0F "); // information
        }
    }();

    QJsonObject body;
    body["chat_id"] = chat_id_;
    body["parse_mode"] = "HTML";
    // parse_mode=HTML: a raw '<', '>' or '&' in the title/message (an alert such as
    // "AAPL < 150" or "R&D") makes Telegram reject the whole message with
    // "can't parse entities" — escape the user-controlled parts.
    body["text"] = QString("<b>%1%2</b>\n%3\n<i>%4</i>")
                       .arg(level_tag, req.title.toHtmlEscaped(), req.message.toHtmlEscaped(),
                            req.timestamp.toString("yyyy-MM-dd hh:mm:ss"));

    HttpClient::instance().post(url, body, [cb](Result<QJsonDocument> res) {
        if (res.is_err()) {
            cb(false, QString::fromStdString(res.error()));
            return;
        }
        const bool ok = res.value().object().value("ok").toBool();
        cb(ok, ok ? QString{} : res.value().object().value("description").toString());
    });
}

} // namespace fincept::notifications
