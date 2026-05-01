#include "services/wallet/BuybackBurnService.h"

#include "core/logging/Logger.h"
#include "datahub/DataHub.h"
#include "storage/secure/SecureStorage.h"

#include <QDateTime>
#include <QJsonArray>
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

// Topic constants — terminal-wide (no <pubkey> segment).
constexpr const char* kTopicBuybackEpoch  = "treasury:buyback_epoch";
constexpr const char* kTopicBurnTotal     = "treasury:burn_total";
constexpr const char* kTopicSupplyHistory = "treasury:supply_history";

// SecureStorage keys — endpoint is shared across treasury producers.
constexpr const char* kKeyEndpoint = "fincept.treasury_endpoint";

QString trim_trailing_slash(QString s) {
    while (s.endsWith('/')) s.chop(1);
    return s;
}

} // namespace

BuybackBurnService::BuybackBurnService(QObject* parent) : QObject(parent) {
    nam_ = new QNetworkAccessManager(this);
}

BuybackBurnService::~BuybackBurnService() = default;

QStringList BuybackBurnService::topic_patterns() const {
    return QStringList{
        QString::fromLatin1(kTopicBuybackEpoch),
        QString::fromLatin1(kTopicBurnTotal),
        QString::fromLatin1(kTopicSupplyHistory),
    };
}

QString BuybackBurnService::resolve_endpoint() const {
    auto r = SecureStorage::instance().retrieve(QString::fromLatin1(kKeyEndpoint));
    if (r.is_ok()) return trim_trailing_slash(r.value().trimmed());
    return {};
}

void BuybackBurnService::refresh(const QStringList& topics) {
    const auto endpoint = resolve_endpoint();
    const bool mock = endpoint.isEmpty();
    if (mock) {
        // Log once per process — refresh fires every 30 s in mock mode and
        // we don't want 2 INFO lines/min indefinitely while the worker is
        // un-deployed. Subsequent refreshes use LOG_DEBUG.
        static std::atomic<bool> once{false};
        if (!once.exchange(true)) {
            LOG_INFO("BuybackBurn",
                     "no fincept.treasury_endpoint configured — serving mock data");
        } else {
            LOG_DEBUG("BuybackBurn", "mock-mode refresh");
        }
    }
    for (const auto& topic : topics) {
        if (topic == QLatin1String(kTopicBuybackEpoch)) {
            mock ? publish_mock_buyback_epoch() : refresh_buyback_epoch(endpoint);
        } else if (topic == QLatin1String(kTopicBurnTotal)) {
            mock ? publish_mock_burn_total() : refresh_burn_total(endpoint);
        } else if (topic == QLatin1String(kTopicSupplyHistory)) {
            mock ? publish_mock_supply_history() : refresh_supply_history(endpoint);
        }
        // Unknown topic — silently ignore. The hub only routes patterns
        // we registered for, so this branch is defensive.
    }
}

// ── HTTP fetch paths ───────────────────────────────────────────────────────

void BuybackBurnService::refresh_buyback_epoch(const QString& endpoint) {
    QNetworkRequest req(QUrl(endpoint + QStringLiteral("/buyback/current")));
    req.setHeader(QNetworkRequest::UserAgentHeader,
                  QStringLiteral("FinceptTerminal/BuybackBurnService"));

    auto* reply = nam_->get(req);
    QPointer<BuybackBurnService> self = this;
    QObject::connect(reply, &QNetworkReply::finished, this, [self, reply]() {
        reply->deleteLater();
        if (!self) return;
        auto& hub = fincept::datahub::DataHub::instance();
        if (reply->error() != QNetworkReply::NoError) {
            hub.publish_error(QString::fromLatin1(kTopicBuybackEpoch),
                              reply->errorString());
            return;
        }
        const auto doc = QJsonDocument::fromJson(reply->readAll());
        if (!doc.isObject()) {
            hub.publish_error(QString::fromLatin1(kTopicBuybackEpoch),
                              QStringLiteral("malformed JSON"));
            return;
        }
        const auto j = doc.object();

        BuybackEpoch e;
        e.epoch_no            = j.value(QStringLiteral("epoch_no")).toInt();
        e.start_ts_ms         = static_cast<qint64>(
            j.value(QStringLiteral("start_ts_ms")).toDouble());
        e.end_ts_ms           = static_cast<qint64>(
            j.value(QStringLiteral("end_ts_ms")).toDouble());
        e.revenue_total_usd   = j.value(QStringLiteral("revenue_total_usd")).toDouble();
        e.revenue_subs_usd    = j.value(QStringLiteral("revenue_subs_usd")).toDouble();
        e.revenue_predmkt_usd = j.value(QStringLiteral("revenue_predmkt_usd")).toDouble();
        e.revenue_misc_usd    = j.value(QStringLiteral("revenue_misc_usd")).toDouble();
        e.buyback_usd         = j.value(QStringLiteral("buyback_usd")).toDouble();
        e.staker_yield_usd    = j.value(QStringLiteral("staker_yield_usd")).toDouble();
        e.treasury_topup_usd  = j.value(QStringLiteral("treasury_topup_usd")).toDouble();
        e.fncpt_bought_raw    = j.value(QStringLiteral("fncpt_bought_raw")).toString();
        e.fncpt_burned_raw    = j.value(QStringLiteral("fncpt_burned_raw")).toString();
        e.fncpt_decimals      = j.value(QStringLiteral("fncpt_decimals")).toInt(6);
        e.avg_buy_price_usd   = j.value(QStringLiteral("avg_buy_price_usd")).toDouble();
        e.burn_signature      = j.value(QStringLiteral("burn_signature")).toString();
        e.ts_ms               = QDateTime::currentMSecsSinceEpoch();
        e.is_mock             = false;
        hub.publish(QString::fromLatin1(kTopicBuybackEpoch), QVariant::fromValue(e));
    });
}

void BuybackBurnService::refresh_burn_total(const QString& endpoint) {
    QNetworkRequest req(QUrl(endpoint + QStringLiteral("/burn/total")));
    auto* reply = nam_->get(req);
    QPointer<BuybackBurnService> self = this;
    QObject::connect(reply, &QNetworkReply::finished, this, [self, reply]() {
        reply->deleteLater();
        if (!self) return;
        auto& hub = fincept::datahub::DataHub::instance();
        if (reply->error() != QNetworkReply::NoError) {
            hub.publish_error(QString::fromLatin1(kTopicBurnTotal),
                              reply->errorString());
            return;
        }
        const auto doc = QJsonDocument::fromJson(reply->readAll());
        if (!doc.isObject()) {
            hub.publish_error(QString::fromLatin1(kTopicBurnTotal),
                              QStringLiteral("malformed JSON"));
            return;
        }
        const auto j = doc.object();

        BurnTotal t;
        t.total_burned_raw      = j.value(QStringLiteral("total_burned_raw")).toString();
        t.supply_remaining_raw  = j.value(QStringLiteral("supply_remaining_raw")).toString();
        t.decimals              = j.value(QStringLiteral("decimals")).toInt(6);
        t.spent_on_buyback_usd  = j.value(QStringLiteral("spent_on_buyback_usd")).toDouble();
        t.ts_ms                 = QDateTime::currentMSecsSinceEpoch();
        t.is_mock               = false;
        hub.publish(QString::fromLatin1(kTopicBurnTotal), QVariant::fromValue(t));
    });
}

void BuybackBurnService::refresh_supply_history(const QString& endpoint) {
    QNetworkRequest req(QUrl(endpoint + QStringLiteral("/supply/history")));
    auto* reply = nam_->get(req);
    QPointer<BuybackBurnService> self = this;
    QObject::connect(reply, &QNetworkReply::finished, this, [self, reply]() {
        reply->deleteLater();
        if (!self) return;
        auto& hub = fincept::datahub::DataHub::instance();
        if (reply->error() != QNetworkReply::NoError) {
            hub.publish_error(QString::fromLatin1(kTopicSupplyHistory),
                              reply->errorString());
            return;
        }
        const auto doc = QJsonDocument::fromJson(reply->readAll());
        if (!doc.isObject()) {
            hub.publish_error(QString::fromLatin1(kTopicSupplyHistory),
                              QStringLiteral("malformed JSON"));
            return;
        }
        // Expected shape: { "points": [ { ts_ms, total_raw, circulating_raw, burned_raw, decimals }, ... ] }
        const auto arr = doc.object().value(QStringLiteral("points")).toArray();
        QVector<SupplyHistoryPoint> points;
        points.reserve(arr.size());
        for (const auto& v : arr) {
            const auto p = v.toObject();
            SupplyHistoryPoint sp;
            sp.ts_ms = static_cast<qint64>(p.value(QStringLiteral("ts_ms")).toDouble());
            sp.total_raw       = p.value(QStringLiteral("total_raw")).toString();
            sp.circulating_raw = p.value(QStringLiteral("circulating_raw")).toString();
            sp.burned_raw      = p.value(QStringLiteral("burned_raw")).toString();
            sp.decimals        = p.value(QStringLiteral("decimals")).toInt(6);
            points.push_back(sp);
        }
        hub.publish(QString::fromLatin1(kTopicSupplyHistory),
                    QVariant::fromValue(points));
    });
}

// ── Mock paths (used when no endpoint is configured) ───────────────────────
//
// Numbers below are illustrative — chosen to make the dashboard look
// plausible in screenshots and demos. They should NEVER be presented as
// real data: every payload sets `is_mock=true` and the UI surfaces the
// state explicitly.

void BuybackBurnService::publish_mock_buyback_epoch() {
    BuybackEpoch e;
    e.epoch_no            = 47;
    const auto now = QDateTime::currentMSecsSinceEpoch();
    constexpr qint64 kWeekMs = 7LL * 24 * 60 * 60 * 1000;
    e.start_ts_ms         = now - kWeekMs;
    e.end_ts_ms           = now;
    e.revenue_total_usd   = 42'310.0;
    e.revenue_subs_usd    = 32'150.0;
    e.revenue_predmkt_usd = 8'120.0;
    e.revenue_misc_usd    = 2'040.0;
    // 50/25/25 split per plan §5.4 — keeps the demo numbers internally
    // consistent: 21,155 + 10,577.50 + 10,577.50 ≈ 42,310 (rounded).
    e.buyback_usd         = 21'155.0;
    e.staker_yield_usd    = 10'577.5;
    e.treasury_topup_usd  = 10'577.5;
    e.fncpt_bought_raw    = QStringLiteral("82400000000000"); // 82.4M @ 6 decimals
    e.fncpt_burned_raw    = QStringLiteral("82400000000000");
    e.fncpt_decimals      = 6;
    e.avg_buy_price_usd   = 0.000257;
    e.burn_signature      = QStringLiteral("5kj3wMockBurnSignaturePlaceholderXXXXXXXXXXX");
    e.ts_ms               = now;
    e.is_mock             = true;
    fincept::datahub::DataHub::instance().publish(
        QString::fromLatin1(kTopicBuybackEpoch), QVariant::fromValue(e));
}

void BuybackBurnService::publish_mock_burn_total() {
    BurnTotal t;
    t.total_burned_raw     = QStringLiteral("1420000000000000"); // 1.42 B
    t.supply_remaining_raw = QStringLiteral("8580000000000000"); // 8.58 B
    t.decimals             = 6;
    t.spent_on_buyback_usd = 384'201.0;
    t.ts_ms                = QDateTime::currentMSecsSinceEpoch();
    t.is_mock              = true;
    fincept::datahub::DataHub::instance().publish(
        QString::fromLatin1(kTopicBurnTotal), QVariant::fromValue(t));
}

void BuybackBurnService::publish_mock_supply_history() {
    // 12 monthly samples, ending now. Burn grows roughly linearly; circulating
    // shrinks accordingly. Total stays flat (10B). Numbers are deliberately
    // smooth so the chart doesn't lie about precision in mock mode.
    QVector<SupplyHistoryPoint> points;
    constexpr qint64 kMonthMs = 30LL * 24 * 60 * 60 * 1000;
    const auto now = QDateTime::currentMSecsSinceEpoch();
    constexpr qint64 kTotal = 10'000'000'000'000'000LL;     // 10 B @ 6 decimals
    constexpr qint64 kStartBurn = 200'000'000'000'000LL;    // 200 M start
    constexpr qint64 kEndBurn   = 1'420'000'000'000'000LL;  // 1.42 B end
    for (int i = 11; i >= 0; --i) {
        SupplyHistoryPoint sp;
        sp.ts_ms = now - (i * kMonthMs);
        const double t = static_cast<double>(11 - i) / 11.0; // 0..1
        const qint64 burned = kStartBurn +
            static_cast<qint64>((kEndBurn - kStartBurn) * t);
        sp.total_raw       = QString::number(kTotal);
        sp.burned_raw      = QString::number(burned);
        sp.circulating_raw = QString::number(kTotal - burned);
        sp.decimals        = 6;
        points.push_back(sp);
    }
    fincept::datahub::DataHub::instance().publish(
        QString::fromLatin1(kTopicSupplyHistory), QVariant::fromValue(points));
}

} // namespace fincept::wallet
