#include "services/billing/TierService.h"

#include "core/logging/Logger.h"
#include "datahub/DataHub.h"
#include "services/billing/TierConfig.h"
#include "services/wallet/WalletTypes.h"

#include <QDateTime>
#include <QVariant>

namespace fincept::billing {

namespace {

constexpr const char* kTierFamilyPrefix = "billing:tier:";
constexpr const char* kVeFncptPrefix = "wallet:vefncpt:";

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

void TierService::refresh(const QStringList& topics) {
    wire_vefncpt_listener();
    for (const auto& topic : topics) {
        const auto pubkey = pubkey_from_topic(topic);
        if (pubkey.isEmpty())
            continue;
        publish_for(topic, pubkey);
    }
}

void TierService::publish_for(const QString& topic, const QString& pubkey) {
    auto& hub = fincept::datahub::DataHub::instance();
    // peek() drops the value once the veFNCPT topic's TTL ages out, which would
    // publish Free for a wallet that actually holds a tier. Fall back to the
    // last-known-good aggregate instead of regressing the tier.
    auto v = hub.peek(vefncpt_topic_for(pubkey));
    if (!v.isValid())
        v = hub.peek_raw(vefncpt_topic_for(pubkey));

    fincept::wallet::TierStatus s;
    s.pubkey_b58 = pubkey;
    s.decimals = TierConfig::kDecimals;
    s.ts_ms = QDateTime::currentMSecsSinceEpoch();

    quint64 weight_raw = 0;
    if (v.canConvert<fincept::wallet::VeFncptAggregate>()) {
        const auto agg = v.value<fincept::wallet::VeFncptAggregate>();
        bool ok = false;
        weight_raw = agg.total_weight_raw.toULongLong(&ok);
        if (!ok)
            weight_raw = 0;
    }

    s.tier = TierConfig::tier_from_weight(weight_raw);
    s.weight_raw = QString::number(weight_raw);
    const auto next = TierConfig::next_threshold_raw(s.tier);
    s.next_threshold_raw = next == 0 ? QString() : QString::number(next);

    hub.publish(topic, QVariant::fromValue(s));
}

void TierService::wire_vefncpt_listener() {
    if (listener_wired_)
        return;
    auto& hub = fincept::datahub::DataHub::instance();
    QObject::connect(&hub, &fincept::datahub::DataHub::topic_updated, this,
                     [this](const QString& topic, const QVariant& /*value*/) {
                         if (!topic.startsWith(QLatin1String(kVeFncptPrefix)))
                             return;
                         const auto pubkey = topic.mid(qstrlen(kVeFncptPrefix));
                         if (pubkey.isEmpty())
                             return;
                         const auto out_topic = QString::fromLatin1(kTierFamilyPrefix) + pubkey;
                         publish_for(out_topic, pubkey);
                     });
    listener_wired_ = true;
    LOG_INFO("Tier", "veFNCPT listener wired");
}

} // namespace fincept::billing
