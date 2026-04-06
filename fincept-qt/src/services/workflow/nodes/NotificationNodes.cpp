#include "services/workflow/nodes/NotificationNodes.h"

#include "services/notifications/NotificationService.h"
#include "services/workflow/NodeRegistry.h"

namespace fincept::workflow {

// Helper: dispatch a notification via NotificationService and pipe input through.
static auto make_execute(const QString& provider_id) {
    return [provider_id](const QJsonObject& params,
                         const QVector<QJsonValue>& inputs,
                         std::function<void(bool, QJsonValue, QString)> cb) {
        using namespace fincept::notifications;

        NotificationRequest req;
        req.title   = params.value("title").toString("Workflow Alert");
        req.message = params.value("message").toString();
        req.trigger = NotifTrigger::WorkflowNode;

        const QJsonValue pass_through = inputs.isEmpty() ? QJsonValue{} : inputs[0];

        NotificationService::instance().send_to(provider_id, req,
            [cb, pass_through](bool ok, const QString& err) {
                cb(ok, pass_through, err);
            });
    };
}

void register_notification_nodes(NodeRegistry& registry) {
    auto make_notif = [&](const QString& id,
                          const QString& name,
                          const QString& desc,
                          QVector<ParamDef> extra_params) {
        QVector<ParamDef> params = {
            {"message", "Message", "string", "", {}, "Notification message", true},
            {"title",   "Title",   "string", "", {}, "Notification title"},
        };
        params.append(extra_params);

        // Map workflow node IDs to NotificationService provider IDs
        const QString pid = [&]() -> QString {
            if (id == "notify.email")    return "email";
            if (id == "notify.slack")    return "slack";
            if (id == "notify.discord")  return "discord";
            if (id == "notify.telegram") return "telegram";
            if (id == "notify.sms")      return "sms";
            if (id == "notify.webhook")  return "webhook";
            return id;
        }();

        registry.register_type({
            .type_id      = id,
            .display_name = name,
            .category     = "Notifications",
            .description  = desc,
            .icon_text    = "@",
            .accent_color = "#ca8a04",
            .version      = 1,
            .inputs       = {{"input_0", "Data In", PortDirection::Input, ConnectionType::Main}},
            .outputs      = {{"output_main", "Main", PortDirection::Output, ConnectionType::Main}},
            .parameters   = params,
            .execute      = make_execute(pid),
        });
    };

    make_notif("notify.email", "Email", "Send an email notification",
               {{"to", "To", "string", "", {}, "Override To address (optional)"}});

    make_notif("notify.slack", "Slack", "Send a Slack message",
               {{"channel", "Channel", "string", "#alerts", {}, "Override channel (optional)"}});

    make_notif("notify.discord", "Discord", "Send a Discord message", {});

    make_notif("notify.telegram", "Telegram", "Send a Telegram message", {});

    make_notif("notify.sms", "SMS", "Send an SMS notification",
               {{"phone", "Phone", "string", "", {}, "Override To number (optional)"}});

    make_notif("notify.webhook", "Webhook", "Send data to a webhook URL",
               {{"url",    "URL",    "string", "", {}, "Override webhook URL (optional)"},
                {"method", "Method", "select", "POST", {"POST", "PUT", "PATCH"}, ""}});
}

} // namespace fincept::workflow
