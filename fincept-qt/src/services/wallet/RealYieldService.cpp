#include "services/wallet/RealYieldService.h"

#include "core/logging/Logger.h"
#include "datahub/DataHub.h"
#include "storage/secure/SecureStorage.h"

#include <QDateTime>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPointer>
#include <QUrl>
#include <QVariant>

#include <atomic>

namespace fincept::wallet {

namespace {

constexpr const char* kYieldFamily   = "wallet:yield:";
constexpr const char* kTopicRevenue  = "treasury:revenue";

// Symbol names suffixed with _yield to avoid unity-build collisions with the
// matching constants/helper in BuybackBurnService.cpp's anonymous namespace.
constexpr const char* kKeyEndpoint_yield   = "fincept.yield_endpoint";

constexpr qint64 kWeekMs = 7LL * 24 * 60 * 60 * 1000;

QString trim_trailing_slash_yield(QString s) {
    while (s.endsWith('/')) s.chop(1);
    return s;
}

} // namespace

RealYieldService& RealYieldService::instance() {
    static RealYieldService inst;
    return inst;
}

RealYieldService::RealYieldService(QObject* parent) : QObject(parent) {
    nam_ = new QNetworkAccessManager(this);
}

RealYieldService::~RealYieldService() = default;

QStringList RealYieldService::topic_patterns() const {
    return QStringList{
        QStringLiteral("wallet:yield:*"),
        QString::fromLatin1(kTopicRevenue),
    };
}

QString RealYieldService::resolve_endpoint() const {
    auto r = SecureStorage::instance().retrieve(QString::fromLatin1(kKeyEndpoint_yield));
    if (r.is_ok()) return trim_trailing_slash_yield(r.value().trimmed());
    return {};
}

QString RealYieldService::pubkey_from_yield_topic(const QString& topic) {
    return topic.mid(static_cast<int>(qstrlen(kYieldFamily)));
}

void RealYieldService::refresh(const QStringList& topics) {
    const auto endpoint = resolve_endpoint();
    const bool mock = endpoint.isEmpty();
    if (mock) {
        static std::atomic<bool> once{false};
        if (!once.exchange(true)) {
            LOG_INFO("RealYield",
                     "no fincept.yield_endpoint configured — serving mock yield");
        } else {
            LOG_DEBUG("RealYield", "mock-mode refresh");
        }
    }

    for (const auto& topic : topics) {
        if (topic.startsWith(QLatin1String(kYieldFamily))) {
            const auto pk = pubkey_from_yield_topic(topic);
            if (pk.isEmpty()) continue;
            mock ? publish_mock_yield(topic, pk)
                 : refresh_yield_real(endpoint, topic, pk);
        } else if (topic == QLatin1String(kTopicRevenue)) {
            mock ? publish_mock_revenue() : refresh_revenue_real(endpoint);
        }
    }
}

// ── Real fetch paths ───────────────────────────────────────────────────────

void RealYieldService::refresh_yield_real(const QString& endpoint,
                                          const QString& topic,
                                          const QString& pubkey) {
    QNetworkRequest req(QUrl(endpoint + QStringLiteral("/yield/") + pubkey));
    req.setHeader(QNetworkRequest::UserAgentHeader,
                  QStringLiteral("FinceptTerminal/RealYieldService"));
    auto* reply = nam_->get(req);
    QPointer<RealYieldService> self = this;
    QObject::connect(reply, &QNetworkReply::finished, this,
                     [self, reply, topic, pubkey]() {
        reply->deleteLater();
        if (!self) return;
        auto& hub = fincept::datahub::DataHub::instance();
        if (reply->error() != QNetworkReply::NoError) {
            hub.publish_error(topic, reply->errorString());
            return;
        }
        const auto doc = QJsonDocument::fromJson(reply->readAll());
        if (!doc.isObject()) {
            hub.publish_error(topic, QStringLiteral("malformed JSON"));
            return;
        }
        const auto j = doc.object();
        YieldSnapshot s;
        s.pubkey_b58         = pubkey;
        s.lifetime_usdc      = j.value(QStringLiteral("lifetime_usdc")).toDouble();
        s.last_period_usdc   = j.value(QStringLiteral("last_period_usdc")).toDouble();
        s.last_period_end_ts = static_cast<qint64>(
            j.value(QStringLiteral("last_period_end_ts")).toDouble());
        s.ts_ms              = QDateTime::currentMSecsSinceEpoch();
        s.is_mock            = false;
        hub.publish(topic, QVariant::fromValue(s));
    });
}

void RealYieldService::refresh_revenue_real(const QString& endpoint) {
    QNetworkRequest req(QUrl(endpoint + QStringLiteral("/revenue/current")));
    req.setHeader(QNetworkRequest::UserAgentHeader,
                  QStringLiteral("FinceptTerminal/RealYieldService"));
    auto* reply = nam_->get(req);
    QPointer<RealYieldService> self = this;
    QObject::connect(reply, &QNetworkReply::finished, this,
                     [self, reply]() {
        reply->deleteLater();
        if (!self) return;
        auto& hub = fincept::datahub::DataHub::instance();
        if (reply->error() != QNetworkReply::NoError) {
            hub.publish_error(QString::fromLatin1(kTopicRevenue), reply->errorString());
            return;
        }
        const auto doc = QJsonDocument::fromJson(reply->readAll());
        if (!doc.isObject()) {
            hub.publish_error(QString::fromLatin1(kTopicRevenue),
                              QStringLiteral("malformed JSON"));
            return;
        }
        const auto j = doc.object();
        TreasuryRevenue r;
        r.period_start_ts = static_cast<qint64>(
            j.value(QStringLiteral("period_start_ts")).toDouble());
        r.period_end_ts   = static_cast<qint64>(
            j.value(QStringLiteral("period_end_ts")).toDouble());
        r.total_usd       = j.value(QStringLiteral("total_usd")).toDouble();
        r.ts_ms           = QDateTime::currentMSecsSinceEpoch();
        r.is_mock         = false;
        hub.publish(QString::fromLatin1(kTopicRevenue), QVariant::fromValue(r));
    });
}

// ── Mock paths ─────────────────────────────────────────────────────────────
//
// Numbers chosen for internal consistency with the Phase 5 mock buyback
// epoch ($42,310 weekly revenue, 25 % staker share = $10,577.50 distributed).
// The single "demo wallet" is assumed to own 100 % of veFNCPT weight, so
// `lifetime_usdc` matches the sum of `LockPosition.lifetime_yield_usdc`
// from the StakingService mock (1,240 + 42 + 14 = 1,296). `last_period_usdc`
// matches the 1-yr position's most recent share — a tractable demo number.

void RealYieldService::publish_mock_yield(const QString& topic, const QString& pubkey) {
    YieldSnapshot s;
    s.pubkey_b58         = pubkey;
    s.lifetime_usdc      = 1'296.0;       // sum of mock LockPosition.lifetime_yield_usdc
    s.last_period_usdc   = 42.0;
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    // Most recent epoch ended now; aligns with the buyback mock's end_ts.
    s.last_period_end_ts = now;
    s.ts_ms              = now;
    s.is_mock            = true;
    fincept::datahub::DataHub::instance().publish(topic, QVariant::fromValue(s));
}

void RealYieldService::publish_mock_revenue() {
    TreasuryRevenue r;
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    r.period_start_ts = now - kWeekMs;
    r.period_end_ts   = now;
    r.total_usd       = 42'310.0;          // matches Phase 5 mock buyback epoch
    r.ts_ms           = now;
    r.is_mock         = true;
    fincept::datahub::DataHub::instance().publish(
        QString::fromLatin1(kTopicRevenue), QVariant::fromValue(r));
}

} // namespace fincept::wallet
