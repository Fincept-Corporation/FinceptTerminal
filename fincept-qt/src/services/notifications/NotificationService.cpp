#include "services/notifications/NotificationService.h"

#include "core/logging/Logger.h"
#include "storage/repositories/SettingsRepository.h"

// Provider includes
#include "services/notifications/providers/DiscordProvider.h"
#include "services/notifications/providers/EmailProvider.h"
#include "services/notifications/providers/GotifyProvider.h"
#include "services/notifications/providers/MattermostProvider.h"
#include "services/notifications/providers/NtfyProvider.h"
#include "services/notifications/providers/OpsgenieProvider.h"
#include "services/notifications/providers/PagerDutyProvider.h"
#include "services/notifications/providers/PushbulletProvider.h"
#include "services/notifications/providers/PushoverProvider.h"
#include "services/notifications/providers/SMSProvider.h"
#include "services/notifications/providers/SlackProvider.h"
#include "services/notifications/providers/TeamsProvider.h"
#include "services/notifications/providers/TelegramProvider.h"
#include "services/notifications/providers/WebhookProvider.h"
#include "services/notifications/providers/WhatsAppProvider.h"

#include <algorithm>

namespace fincept::notifications {

// ── Singleton ─────────────────────────────────────────────────────────────────

NotificationService& NotificationService::instance() {
    static NotificationService inst;
    return inst;
}

NotificationService::NotificationService() {
    register_providers();
}

void NotificationService::register_providers() {
    providers_.clear();
    providers_.emplace_back(std::make_unique<TelegramProvider>());
    providers_.emplace_back(std::make_unique<DiscordProvider>());
    providers_.emplace_back(std::make_unique<SlackProvider>());
    providers_.emplace_back(std::make_unique<EmailProvider>());
    providers_.emplace_back(std::make_unique<WhatsAppProvider>());
    providers_.emplace_back(std::make_unique<PushoverProvider>());
    providers_.emplace_back(std::make_unique<NtfyProvider>());
    providers_.emplace_back(std::make_unique<PushbulletProvider>());
    providers_.emplace_back(std::make_unique<GotifyProvider>());
    providers_.emplace_back(std::make_unique<MattermostProvider>());
    providers_.emplace_back(std::make_unique<TeamsProvider>());
    providers_.emplace_back(std::make_unique<WebhookProvider>());
    providers_.emplace_back(std::make_unique<PagerDutyProvider>());
    providers_.emplace_back(std::make_unique<OpsgenieProvider>());
    providers_.emplace_back(std::make_unique<SMSProvider>());

    for (auto& p : providers_)
        p->load_config();

    LOG_INFO("NotificationService", "Registered 15 notification providers");
}

// ── Send ──────────────────────────────────────────────────────────────────────

void NotificationService::send(const NotificationRequest& req) {
    // Read per-trigger filter from settings
    auto& repo = SettingsRepository::instance();
    auto get_bool = [&](const QString& key, bool def) -> bool {
        auto r = repo.get(key);
        return r.is_ok() ? (r.value() == "1") : def;
    };

    // Check trigger-level filter (in-app always on unless globally disabled)
    const bool inapp_on = get_bool("notifications.inapp", true);
    const bool price_on = get_bool("notifications.price_alerts", true);
    const bool orders_on = get_bool("notifications.order_fills", true);
    const bool news_on = get_bool("notifications.news_alerts", false);

    bool trigger_allowed = true;
    switch (req.trigger) {
        case NotifTrigger::PriceAlert:
            trigger_allowed = price_on;
            break;
        case NotifTrigger::OrderFill:
            trigger_allowed = orders_on;
            break;
        case NotifTrigger::NewsAlert:
            trigger_allowed = news_on;
            break;
        case NotifTrigger::Manual:
        case NotifTrigger::WorkflowNode:
            trigger_allowed = true;
            break;
    }

    if (!trigger_allowed) {
        LOG_DEBUG("NotificationService",
                  QString("Trigger suppressed by filter: %1").arg(req.title).toStdString().c_str());
        return;
    }

    // Always record in-app history
    NotificationRecord record;
    record.id = next_id_++;
    record.request = req;
    record.read = false;
    record.received_at = QDateTime::currentDateTime();
    history_.prepend(record);
    if (history_.size() > 200)
        history_.removeLast();

    if (inapp_on) {
        ++unread_;
        emit notification_received(record);
        emit unread_count_changed(unread_);
    }

    // Dispatch to external providers
    for (auto& p : providers_) {
        if (!p->is_enabled() || !p->is_configured())
            continue;
        const QString pid = p->provider_id();
        p->send(req, [pid](bool ok, const QString& err) {
            if (ok) {
                LOG_INFO("NotificationService", QString("Delivered via %1").arg(pid).toStdString().c_str());
            } else {
                LOG_WARN("NotificationService",
                         QString("Delivery failed via %1: %2").arg(pid, err).toStdString().c_str());
            }
        });
    }
}

void NotificationService::send_to(const QString& provider_id, const NotificationRequest& req,
                                  std::function<void(bool, QString)> cb) {
    auto* p = provider(provider_id);
    if (!p) {
        cb(false, QString("Provider '%1' not found").arg(provider_id));
        return;
    }
    p->send(req, std::move(cb));
}

// ── History ───────────────────────────────────────────────────────────────────

const QVector<NotificationRecord>& NotificationService::history() const {
    return history_;
}

int NotificationService::unread_count() const {
    return unread_;
}

void NotificationService::mark_read(int id) {
    for (auto& r : history_) {
        if (r.id == id && !r.read) {
            r.read = true;
            --unread_;
            if (unread_ < 0)
                unread_ = 0;
            emit unread_count_changed(unread_);
            return;
        }
    }
}

void NotificationService::mark_all_read() {
    for (auto& r : history_)
        r.read = true;
    unread_ = 0;
    emit unread_count_changed(0);
}

// ── Provider access ───────────────────────────────────────────────────────────

QVector<INotificationProvider*> NotificationService::providers() const {
    QVector<INotificationProvider*> out;
    out.reserve(static_cast<int>(providers_.size()));
    for (const auto& p : providers_)
        out.append(p.get());
    return out;
}

INotificationProvider* NotificationService::provider(const QString& id) const {
    for (const auto& p : providers_) {
        if (p->provider_id() == id)
            return p.get();
    }
    return nullptr;
}

void NotificationService::reload_all_configs() {
    for (auto& p : providers_)
        p->load_config();
    LOG_INFO("NotificationService", "All provider configs reloaded");
}

} // namespace fincept::notifications
