#include "services/notifications/providers/TeamsProvider.h"
#include "network/http/HttpClient.h"

#include <QJsonArray>
#include <QJsonObject>

namespace fincept::notifications {

void TeamsProvider::load_fields(SettingsRepository& r, const QString& cat) {
    webhook_url_ = get_str(r, cat + ".webhook_url");
}

void TeamsProvider::save_fields(SettingsRepository& r, const QString& cat) {
    r.set(cat + ".webhook_url", webhook_url_, cat);
}

void TeamsProvider::send(const NotificationRequest& req,
                         std::function<void(bool, QString)> cb) {
    if (!is_configured()) { cb(false, "Not configured"); return; }

    const QString color = [&]() -> QString {
        switch (req.level) {
            case NotifLevel::Warning:  return "F59E0B";
            case NotifLevel::Alert:    return "F97316";
            case NotifLevel::Critical: return "EF4444";
            default:                   return "06B6D4";
        }
    }();

    // MS Teams Adaptive Card payload
    QJsonObject fact_title;
    fact_title["title"] = "Alert";
    fact_title["value"] = req.title;

    QJsonObject fact_time;
    fact_time["title"] = "Time";
    fact_time["value"] = req.timestamp.toString("yyyy-MM-dd hh:mm:ss");

    QJsonObject section;
    section["activityTitle"]    = req.title;
    section["activityText"]     = req.message;
    section["activitySubtitle"] = req.timestamp.toString("yyyy-MM-dd hh:mm:ss");

    QJsonArray sections;
    sections.append(section);

    QJsonObject body;
    body["@type"]      = "MessageCard";
    body["@context"]   = "http://schema.org/extensions";
    body["themeColor"] = color;
    body["summary"]    = req.title;
    body["title"]      = req.title;
    body["text"]       = req.message;
    body["sections"]   = sections;

    HttpClient::instance().post(webhook_url_, body, [cb](Result<QJsonDocument> res) {
        if (res.is_err()) { cb(false, QString::fromStdString(res.error())); return; }
        cb(true, {});
    });
}

} // namespace fincept::notifications
