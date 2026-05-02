#include "services/billing/TierService.h"

#include "core/logging/Logger.h"
#include "datahub/DataHub.h"
#include "services/billing/TierConfig.h"
#include "services/wallet/WalletTypes.h"

#include <QDateTime>
#include <QHash>
#include <QVariant>

namespace fincept::billing {

namespace {

constexpr const char* kTierFamilyPrefix = "billing:tier:";
constexpr const char* kVeFncptPrefix = "wallet:vefncpt:";

// Per-process cache of last-published tier per pubkey. Used by both
// `get_cached_tier` (synchronous gating) and the `tier_changed` signal
// emission (skip noise re-emits when the tier hasn't actually moved).
QHash<QString, fincept::wallet::TierStatus::Tier>& tier_cache() {
    static QHash<QString, fincept::wallet::TierStatus::Tier> c;
    return c;
}

} // namespace

TierService& TierService::instance() {
    static TierService inst;
    return inst;
}

TierService::TierService(QObject* parent) : QObject(parent) {}

TierService::~TierService() = default;

QStringList TierService::topic_patterns() const {
    return QStringList{QStringLiteral("billing:tier:*")};
}

QString TierService::pubkey_from_topic(const QString& topic) {
    return topic.mid(static_cast<int>(qstrlen(kTierFamilyPrefix)));
}

QString TierService::vefncpt_topic_for(const QString& pubkey) {
    return QString::fromLatin1(kVeFncptPrefix) + pubkey;
}

fincept::wallet::TierStatus::Tier
TierService::get_cached_tier(const QString& pubkey) const {
    return tier_cache().value(pubkey, fincept::wallet::TierStatus::Tier::Free);
}

void TierService::refresh(const QStringList& topics) {
    wire_vefncpt_listener();
    for (const auto& topic : topics) {
        const auto pubkey = pubkey_from_topic(topic);
        if (pubkey.isEmpty()) continue;
        publish_for(topic, pubkey);
    }
}

void TierService::publish_for(const QString& topic, const QString& pubkey) {
    auto& hub = fincept::datahub::DataHub::instance();
    const auto v = hub.peek(vefncpt_topic_for(pubkey));

    fincept::wallet::TierStatus s;
    s.pubkey_b58 = pubkey;
    s.decimals   = TierConfig::kDecimals;
    s.ts_ms      = QDateTime::currentMSecsSinceEpoch();

    quint64 weight_raw = 0;
    bool any_data = false;
    if (v.canConvert<fincept::wallet::VeFncptAggregate>()) {
        const auto agg = v.value<fincept::wallet::VeFncptAggregate>();
        bool ok = false;
        weight_raw = agg.total_weight_raw.toULongLong(&ok);
        if (!ok) weight_raw = 0;
        any_data = true;
        s.is_mock = agg.is_mock;
    }

    s.tier              = TierConfig::tier_from_weight(weight_raw);
    s.weight_raw        = QString::number(weight_raw);
    const auto next     = TierConfig::next_threshold_raw(s.tier);
    s.next_threshold_raw = next == 0 ? QString() : QString::number(next);

    // Update cache + emit on change. The cache is also the source for
    // synchronous gating in screens that haven't subscribed yet.
    auto& cache = tier_cache();
    const auto prev = cache.value(pubkey, fincept::wallet::TierStatus::Tier::Free);
    cache.insert(pubkey, s.tier);

    hub.publish(topic, QVariant::fromValue(s));

    if (prev != s.tier) {
        if (any_data) {
            // Only emit when we actually have a weight number to back the
            // claim — otherwise the very first publish (Free → Free) would
            // fire a noisy signal for every connected pubkey at startup.
            emit tier_changed(pubkey, static_cast<int>(s.tier));
            LOG_INFO("Tier", QStringLiteral("%1: %2 → %3")
                .arg(pubkey.left(12))
                .arg(TierConfig::label_for(prev))
                .arg(TierConfig::label_for(s.tier)));
        }
    }
}

void TierService::wire_vefncpt_listener() {
    if (listener_wired_) return;
    auto& hub = fincept::datahub::DataHub::instance();
    QObject::connect(&hub, &fincept::datahub::DataHub::topic_updated, this,
                     [this](const QString& topic, const QVariant& /*value*/) {
        if (!topic.startsWith(QLatin1String(kVeFncptPrefix))) return;
        const auto pubkey = topic.mid(qstrlen(kVeFncptPrefix));
        if (pubkey.isEmpty()) return;
        const auto out_topic = QString::fromLatin1(kTierFamilyPrefix) + pubkey;
        publish_for(out_topic, pubkey);
    });
    listener_wired_ = true;
    LOG_INFO("Tier", "veFNCPT listener wired");
}

} // namespace fincept::billing
