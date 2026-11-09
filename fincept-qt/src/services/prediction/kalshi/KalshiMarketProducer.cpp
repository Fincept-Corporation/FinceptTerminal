#include "services/prediction/kalshi/KalshiMarketProducer.h"

#include "core/logging/Logger.h"
#include "datahub/DataHub.h"
#include "datahub/DataHubMetaTypes.h"
#include "datahub/TopicPolicy.h"
#include "services/prediction/PredictionTypes.h"
#include "services/prediction/kalshi/KalshiRestClient.h"

#include <QHash>
#include <QSet>

namespace fincept::services::prediction::kalshi_ns {

// Named (not anonymous) inner namespace: src/services is unity-batched, and these names must
// not meet another file's file-scope helpers in the same batch.
namespace kalshi_market_producer_detail {

constexpr const char* kKalshiMarketTopicPrefix = "prediction:kalshi:market:";
constexpr const char* kKalshiMarketTag = "KalshiMarkets";
// URL length / response size bound per request; a dashboard asks for a handful.
constexpr int kKalshiMarketChunk = 100;

} // namespace kalshi_market_producer_detail

namespace kalshi_mkt_detail = kalshi_market_producer_detail;

KalshiMarketProducer& KalshiMarketProducer::instance() {
    static KalshiMarketProducer inst;
    return inst;
}

KalshiMarketProducer::KalshiMarketProducer(QObject* parent) : QObject(parent), rest_(new KalshiRestClient(this)) {}

QString KalshiMarketProducer::topic_for(const QString& ticker) {
    return QString::fromLatin1(kalshi_mkt_detail::kKalshiMarketTopicPrefix) + ticker;
}

QStringList KalshiMarketProducer::topic_patterns() const {
    return {QString::fromLatin1(kalshi_mkt_detail::kKalshiMarketTopicPrefix) + QLatin1Char('*')};
}

void KalshiMarketProducer::refresh(const QStringList& topics) {
    auto& hub = fincept::datahub::DataHub::instance();
    const QString prefix = QString::fromLatin1(kalshi_mkt_detail::kKalshiMarketTopicPrefix);

    QStringList tickers;
    tickers.reserve(topics.size());
    for (const QString& topic : topics) {
        if (!topic.startsWith(prefix))
            continue;
        const QString ticker = topic.mid(prefix.size());
        if (ticker.trimmed().isEmpty() || ticker.contains(QLatin1Char(','))) {
            hub.publish_error(topic, QStringLiteral("[INVALID] Not a Kalshi market ticker: ") + ticker);
            continue;
        }
        if (!tickers.contains(ticker))
            tickers.append(ticker);
    }

    for (qsizetype start = 0; start < tickers.size(); start += kalshi_mkt_detail::kKalshiMarketChunk) {
        const QStringList chunk = tickers.mid(start, kalshi_mkt_detail::kKalshiMarketChunk);
        LOG_DEBUG(kalshi_mkt_detail::kKalshiMarketTag, QString("GET /markets for %1 ticker(s)").arg(chunk.size()));
        rest_->fetch_markets_by_tickers(
            chunk,
            [chunk](const QVector<PredictionMarket>& markets) {
                auto& h = fincept::datahub::DataHub::instance();
                // Kalshi tickers are upper-case; match case-insensitively but publish under the topic
                // that was asked for.
                QHash<QString, QString> requested;
                for (const QString& t : chunk)
                    requested.insert(t.toUpper(), t);
                QSet<QString> answered;
                for (const auto& m : markets) {
                    const auto it = requested.constFind(m.key.market_id.toUpper());
                    if (it == requested.constEnd())
                        continue;
                    answered.insert(it.value());
                    h.publish(KalshiMarketProducer::topic_for(it.value()), QVariant::fromValue(m));
                }
                for (const QString& t : chunk) {
                    if (!answered.contains(t))
                        h.publish_error(KalshiMarketProducer::topic_for(t),
                                        QStringLiteral("[NOT_FOUND] Kalshi has no market ") + t);
                }
            },
            [chunk](const QString& message) {
                LOG_WARN(kalshi_mkt_detail::kKalshiMarketTag,
                         QString("GET /markets failed for %1 ticker(s): %2").arg(chunk.size()).arg(message));
                auto& h = fincept::datahub::DataHub::instance();
                for (const QString& t : chunk)
                    h.publish_error(KalshiMarketProducer::topic_for(t), QStringLiteral("[REQUEST_FAILED] ") + message);
            });
    }
}

void KalshiMarketProducer::ensure_registered_with_hub() {
    if (hub_registered_)
        return;
    auto& hub = fincept::datahub::DataHub::instance();
    hub.register_producer(this);

    fincept::datahub::TopicPolicy policy;
    policy.ttl_ms = 30 * 1000;             // prices move; Kalshi read limits are generous
    policy.min_interval_ms = 10 * 1000;    // also the retry cadence after a failed fetch
    policy.refresh_timeout_ms = 20 * 1000; // request transfer timeout is 15 s
    policy.push_only = false;
    hub.set_policy_pattern(QString::fromLatin1(kalshi_mkt_detail::kKalshiMarketTopicPrefix) + QLatin1Char('*'), policy);

    hub_registered_ = true;
    LOG_INFO(kalshi_mkt_detail::kKalshiMarketTag, "Registered with DataHub (prediction:kalshi:market:*)");
}

} // namespace fincept::services::prediction::kalshi_ns
