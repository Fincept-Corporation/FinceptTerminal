#include "services/workflow/nodes/NotificationNodes.h"
#include "services/workflow/NodeRegistry.h"

namespace fincept::workflow {

void register_notification_nodes(NodeRegistry& registry)
{
    auto make_notif = [&](const QString& id, const QString& name, const QString& desc,
                          QVector<ParamDef> extra_params) {
        QVector<ParamDef> params = {
            { "message", "Message", "string", "", {}, "Notification message", true },
        };
        params.append(extra_params);
        registry.register_type({
            .type_id = id, .display_name = name, .category = "Notifications",
            .description = desc, .icon_text = "@", .accent_color = "#ca8a04", .version = 1,
            .inputs  = {{ "input_0", "Data In", PortDirection::Input, ConnectionType::Main }},
            .outputs = {{ "output_main", "Main", PortDirection::Output, ConnectionType::Main }},
            .parameters = params,
            .execute = [](const QJsonObject& params, const QVector<QJsonValue>& inputs,
                          std::function<void(bool, QJsonValue, QString)> cb) {
                auto data = inputs.isEmpty() ? QJsonValue{} : inputs[0];
                Q_UNUSED(params);
                cb(true, data, {}); // Placeholder
            },
        });
    };

    make_notif("notify.email", "Email", "Send an email notification",
        {{ "to", "To", "string", "", {}, "Email address", true },
         { "subject", "Subject", "string", "", {}, "Email subject" }});

    make_notif("notify.slack", "Slack", "Send a Slack message",
        {{ "channel", "Channel", "string", "#alerts", {}, "Slack channel" },
         { "webhook_url", "Webhook URL", "string", "", {}, "Slack webhook URL", true }});

    make_notif("notify.discord", "Discord", "Send a Discord message",
        {{ "webhook_url", "Webhook URL", "string", "", {}, "Discord webhook URL", true }});

    make_notif("notify.telegram", "Telegram", "Send a Telegram message",
        {{ "chat_id", "Chat ID", "string", "", {}, "Telegram chat ID", true },
         { "bot_token", "Bot Token", "string", "", {}, "Telegram bot token", true }});

    make_notif("notify.sms", "SMS", "Send an SMS notification",
        {{ "phone", "Phone", "string", "", {}, "Phone number", true }});

    make_notif("notify.webhook", "Webhook", "Send data to a webhook URL",
        {{ "url", "URL", "string", "", {}, "Webhook URL", true },
         { "method", "Method", "select", "POST", {"POST","PUT","PATCH"}, "" }});
}

} // namespace fincept::workflow
