#include "services/economics/MacroCalendarService.h"

#include "auth/AuthManager.h"
#include "services/billing/PlanAccess.h"
#include "core/config/AppConfig.h"
#include "core/logging/Logger.h"
#include "datahub/DataHub.h"
#include "datahub/TopicPolicy.h"
#include "network/http/HttpClient.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QPointer>

#include <chrono>

namespace fincept::services {

namespace {
constexpr const char* kMacroCalTopic = "econ:fincept:upcoming_events";

// Path appended to AppConfig::api_base_url(). Requests go through the shared
// HttpClient, which attaches the signed-in user's session key (X-API-Key /
// X-Session-Token) to same-host api.fincept.in requests — so no endpoint key
// is baked into the binary.
constexpr const char* kMacroCalPath = "/macro/upcoming-events?limit=200";

// A refusal that waiting does not fix (no credits, signed out, not allowed):
// hours instead of the hub's 5-minute ladder cap, which re-asked ~12 times an
// hour for as long as the dashboard was open (~224k refused calls in five
// weeks). release_refusal() lifts it as soon as something changes.
constexpr std::chrono::milliseconds kMacroCalRefusedRetry = fincept::datahub::DataHub::kNegativeCacheRetry * 24;
// The server keeps an IP block for ~30 min.
constexpr std::chrono::milliseconds kMacroCalIpBlockRetry = fincept::datahub::DataHub::kNegativeCacheRetry * 2;
// macro.py MACRO_CALENDAR_CREDITS default: the LIST price of one uncached call.
constexpr double kMacroCalCreditCost = 1.0;
// The calendar's category in the server's plan table (billing::PlanAccess).
constexpr const char* kMacroCalPlanCategory = "macro";

/// What one uncached call costs THIS account: the plan's price of the list price
/// (0 when the plan includes macro data); the list price without a plan table.
double macro_cal_cost() {
    return fincept::billing::PlanAccess::instance().effective_cost(QString::fromLatin1(kMacroCalPlanCategory),
                                                                   kMacroCalCreditCost);
}

QJsonArray macro_cal_parse_events(const QJsonDocument& doc) {
    if (doc.isArray())
        return doc.array();
    if (!doc.isObject())
        return {};

    const auto root = doc.object();
    // {"success":true,"data":{"events":[...]}}
    if (root.contains(QStringLiteral("data")) && root.value(QStringLiteral("data")).isObject()) {
        const auto data = root.value(QStringLiteral("data")).toObject();
        if (data.contains(QStringLiteral("events")) && data.value(QStringLiteral("events")).isArray())
            return data.value(QStringLiteral("events")).toArray();
    }
    // {"data":[...]}
    if (root.contains(QStringLiteral("data")) && root.value(QStringLiteral("data")).isArray())
        return root.value(QStringLiteral("data")).toArray();
    // {"events":[...]}
    if (root.contains(QStringLiteral("events")) && root.value(QStringLiteral("events")).isArray())
        return root.value(QStringLiteral("events")).toArray();
    return {};
}
} // namespace

MacroCalendarService& MacroCalendarService::instance() {
    static MacroCalendarService s;
    return s;
}

MacroCalendarService::MacroCalendarService(QObject* parent) : QObject(parent) {}

void MacroCalendarService::ensure_registered_with_hub() {
    if (hub_registered_)
        return;
    auto& hub = fincept::datahub::DataHub::instance();
    hub.register_producer(this);

    fincept::datahub::TopicPolicy policy;
    // 15 min: each uncached call is billed (list price 1 credit; nothing when the
    // plan includes macro data) and the calendar is
    // scraped upstream on a slow cadence. A 5-minute TTL cost a dashboard left
    // open up to 12 credits an hour. The widget's ⟳ still fetches on demand.
    policy.ttl_ms = 15 * 60 * 1000;
    policy.min_interval_ms = 5 * 60 * 1000;
    policy.refresh_timeout_ms = 30 * 1000;
    hub.set_policy(QString::fromLatin1(kMacroCalTopic), policy);

    // What lifts a refusal backoff: a new sign-in / key, or credits again.
    auto& am = auth::AuthManager::instance();
    connect(&am, &auth::AuthManager::auth_state_changed, this, [this]() {
        const auto& s = auth::AuthManager::instance().session();
        if (refusal_.release_on_auth(s.authenticated, s.api_key))
            release_refusal("signed in");
    });
    connect(&am, &auth::AuthManager::credit_balance_changed, this, [this](double balance) {
        // Enough for one call at the price this account's plan pays (not the list price).
        if (refusal_.release_on_balance(balance, macro_cal_cost()))
            release_refusal("credits topped up");
    });
    // The plan table arrived or changed and the plan now includes macro data: an
    // "out of credits" pause no longer applies (the widget then loads by itself).
    connect(&fincept::billing::PlanAccess::instance(), &fincept::billing::PlanAccess::changed, this, [this]() {
        if (refusal_.kind == auth::RefusalLatch::Kind::Credits && macro_cal_cost() <= 0.0)
            release_refusal("plan includes macro data");
    });

    hub_registered_ = true;
    LOG_DEBUG("MacroCalendarService", "Registered with DataHub (econ:fincept:upcoming_events)");
}

QStringList MacroCalendarService::topic_patterns() const {
    return {QString::fromLatin1(kMacroCalTopic)};
}

void MacroCalendarService::publish_refused(const QString& message, auth::RefusalLatch::Kind kind) {
    auto& am = auth::AuthManager::instance();
    refusal_.refuse(kind, am.session().api_key, am.session().user_info.credit_balance);
    LOG_INFO("MacroCalendarService", logf("Economic calendar paused (not retried automatically)", {{"reason", message}}));
    fincept::datahub::DataHub::instance().publish_error(QString::fromLatin1(kMacroCalTopic), message,
                                                        kMacroCalRefusedRetry);
    // The header chip may still show credits the server says are gone (spent
    // through a client whose responses carry no balance): re-read the account
    // so the user sees why, and so a later top-up reads as a rise (RefusalLatch).
    // A pause happens at most once per refusal, so this costs one refresh.
    if (kind == auth::RefusalLatch::Kind::Credits)
        am.refresh_user_data(auth::AuthManager::Refresh::Now);
}

void MacroCalendarService::release_refusal(const char* why) {
    if (!refusal_.active())
        return;
    refusal_.clear();
    LOG_INFO("MacroCalendarService", QString("Calendar refusal lifted (%1)").arg(QLatin1String(why)));
    auto& hub = fincept::datahub::DataHub::instance();
    const QString topic = QString::fromLatin1(kMacroCalTopic);
    hub.clear_backoff(topic);
    // Showing it: fetch now (an event, not a timer). Nobody watching: the next
    // subscriber's cold start fetches.
    if (!hub.subscribers(topic).isEmpty())
        hub.request(topic, /*force=*/true);
}

void MacroCalendarService::refresh(const QStringList& topics) {
    // Single-topic producer — the hub may pass the topic list redundantly,
    // but there's only ever one fetch to do.
    const QString topic = QString::fromLatin1(kMacroCalTopic);
    if (!topics.contains(topic))
        return;

    // Account-only data: never ask without a key. The dashboard topics are
    // pre-warmed while the login screen is up (main.cpp), which used to send
    // this request unauthenticated — a 401 that also counts toward the
    // server's IP ban. A sign-in lifts this (release_refusal).
    const QString key = auth::AuthManager::instance().session().api_key;
    if (key.isEmpty()) {
        publish_refused(QStringLiteral("Sign in to Fincept to see the economic calendar."),
                        auth::RefusalLatch::Kind::Access);
        return;
    }
    if (HttpClient::instance().is_ip_blocked()) {
        fincept::datahub::DataHub::instance().publish_error(
            topic, QStringLiteral("The Fincept API is temporarily refusing this network — the calendar resumes when "
                                  "the block lifts."),
            kMacroCalIpBlockRetry);
        return;
    }

    // Route through the shared HttpClient so the signed-in user's session key
    // is attached automatically (same-host auth), matching the other Fincept
    // data services (Geopolitics/Maritime/Forum). Host comes from AppConfig so
    // staging / self-host overrides apply uniformly.
    const QString url = AppConfig::instance().api_base_url() + QString::fromLatin1(kMacroCalPath);

    QPointer<MacroCalendarService> self = this;
    HttpClient::instance().get(
        url,
        [self](Result<QJsonDocument> result) {
            if (!self)
                return;
            const QString topic = QString::fromLatin1(kMacroCalTopic);
            auto& hub = fincept::datahub::DataHub::instance();
            if (!result.is_ok()) {
                const std::string err = result.error();
                const int status = HttpClient::status_from_error(err);
                // The server's own explanation (HttpClient keeps it after the status).
                const QString server = HttpClient::message_from_error(err);
                // Consumers show this text, once, in place: say why the panel is
                // empty and what fixes it. Only a 401 is a sign-in problem. A 403
                // is an IP block (HttpClient adds how long it lasts), an
                // unverified account or a forbidden route; 402 is the credit
                // balance or an expired pass; 429 a plan limit.
                if (status == 401) {
                    self->publish_refused(QStringLiteral("Sign in to Fincept to load the economic calendar."),
                                          auth::RefusalLatch::Kind::Access);
                } else if (status == 402) {
                    // The price this account's plan pays (the list price without a plan table;
                    // a refusal while the plan claims "included" names the list price). On Free
                    // the message also says what Pro changes for macro data (included).
                    const double plan_cost = macro_cal_cost();
                    const double cost = plan_cost > 0.0 ? plan_cost : kMacroCalCreditCost;
                    self->publish_refused(
                        fincept::billing::PlanAccess::instance().with_upgrade_note(
                            tr("Out of Fincept credits: the economic calendar needs %1 credit(s) per update. Top "
                               "up or upgrade in Plans & Pricing, then press ⟳ — it stays paused until then.")
                                .arg(QString::number(cost, 'g', 3)),
                            QString::fromLatin1(kMacroCalPlanCategory)),
                        auth::RefusalLatch::Kind::Credits);
                } else if (status == 403 && HttpClient::instance().is_ip_blocked()) {
                    hub.publish_error(topic, server.isEmpty() ? QStringLiteral("Fincept API refused (IP block)") : server,
                                      kMacroCalIpBlockRetry);
                } else if (status == 403) {
                    self->publish_refused(server.isEmpty()
                                              ? QStringLiteral("This account cannot load the economic calendar (HTTP 403).")
                                              : server,
                                          auth::RefusalLatch::Kind::Access);
                } else {
                    QString msg;
                    if (status == 0)
                        msg = QStringLiteral("Fincept API unreachable — %1").arg(server);
                    else if (!server.isEmpty())
                        msg = server;
                    else
                        msg = QStringLiteral("Fincept API error (HTTP %1)").arg(status);
                    // DEBUG: the HTTP layer and DataHub already record this failure.
                    LOG_DEBUG("MacroCalendarService", logf("Economic calendar request failed", {{"error", msg}}));
                    hub.publish_error(topic, msg); // transient: the hub's ladder retries
                }
                return;
            }
            // {success:false, message} on a 200 is a failure, not "no events this week".
            const QJsonObject root = result.value().object();
            if (root.contains(QStringLiteral("success")) && !root.value(QStringLiteral("success")).toBool()) {
                QString msg = HttpClient::error_text_from_body(root);
                if (msg.isEmpty())
                    msg = QStringLiteral("Calendar request failed");
                LOG_DEBUG("MacroCalendarService", logf("Economic calendar request failed", {{"error", msg}}));
                hub.publish_error(topic, msg);
                return;
            }
            self->refusal_.clear();
            const QJsonArray events = macro_cal_parse_events(result.value());
            hub.publish(topic, QVariant::fromValue(events));
        },
        this);
}

} // namespace fincept::services
