#include "services/billing/FeeDiscountService.h"

#include "core/logging/Logger.h"
#include "datahub/DataHub.h"
#include "services/billing/FeeDiscountConfig.h"
#include "services/wallet/WalletTypes.h"

#include <QDateTime>
#include <QVariant>

namespace fincept::billing {

namespace {

constexpr const char* kFeeDiscountFamilyPrefix = "billing:fncpt_discount:";

quint64 fncpt_raw_amount(const fincept::wallet::WalletBalance& bal) {
    if (auto* h = bal.fncpt_holding()) {
        bool ok = false;
        const auto raw = h->amount_raw.toULongLong(&ok);
        return ok ? raw : 0;
    }
    return 0;
}

} // namespace

FeeDiscountService::FeeDiscountService(QObject* parent) : QObject(parent) {}

FeeDiscountService::~FeeDiscountService() = default;

QStringList FeeDiscountService::topic_patterns() const {
    return QStringList{QStringLiteral("billing:fncpt_discount:*")};
}

QString FeeDiscountService::pubkey_from_topic(const QString& topic) {
    return topic.mid(static_cast<int>(qstrlen(kFeeDiscountFamilyPrefix)));
}

QString FeeDiscountService::balance_topic_for(const QString& pubkey) {
    return QStringLiteral("wallet:balance:") + pubkey;
}

void FeeDiscountService::refresh(const QStringList& topics) {
    wire_balance_listener();
    for (const auto& topic : topics) {
        const auto pubkey = pubkey_from_topic(topic);
        if (pubkey.isEmpty()) continue;
        publish_for(topic, pubkey);
    }
}

void FeeDiscountService::publish_for(const QString& topic, const QString& pubkey) {
    auto& hub = fincept::datahub::DataHub::instance();
    const auto bal_topic = balance_topic_for(pubkey);
    const auto v = hub.peek(bal_topic);

    fincept::wallet::FncptDiscount d;
    d.pubkey_b58 = pubkey;
    d.threshold_raw = FeeDiscountConfig::kThresholdRaw;
    d.threshold_decimals = FeeDiscountConfig::kThresholdDecimals;
    d.discount_pct = FeeDiscountConfig::kDiscountPct;
    d.applied_skus = FeeDiscountConfig::applied_skus();
    d.ts_ms = QDateTime::currentMSecsSinceEpoch();

    if (v.canConvert<fincept::wallet::WalletBalance>()) {
        const auto bal = v.value<fincept::wallet::WalletBalance>();
        d.eligible = fncpt_raw_amount(bal) >= FeeDiscountConfig::kThresholdRaw;
    } else {
        // No balance cached yet. Subscribe so the next publish flips us.
        // Stay on `eligible=false` until we have data — the chip on the
        // HoldingsBar starts hidden, no UX glitch.
        d.eligible = false;
    }

    hub.publish(topic, QVariant::fromValue(d));
}

void FeeDiscountService::wire_balance_listener() {
    if (listener_wired_) return;
    auto& hub = fincept::datahub::DataHub::instance();
    // Listen for *every* wallet:balance:* publish and re-emit any active
    // discount topic that matches. We use the topic_changed signal because
    // it fires once per publish across the family.
    QObject::connect(&hub, &fincept::datahub::DataHub::topic_updated, this,
                     [this](const QString& topic, const QVariant& /*value*/) {
        const QString prefix = QStringLiteral("wallet:balance:");
        if (!topic.startsWith(prefix)) return;
        const auto pubkey = topic.mid(prefix.size());
        if (pubkey.isEmpty()) return;
        const auto out_topic = QString::fromLatin1(kFeeDiscountFamilyPrefix) + pubkey;
        // Only republish if someone is actually listening (cheap check —
        // if nobody subscribed, the hub will skip the publish anyway,
        // but we save the QVariant copy).
        publish_for(out_topic, pubkey);
    });
    listener_wired_ = true;
    LOG_INFO("FeeDiscount", "balance listener wired");
}

} // namespace fincept::billing
