#include "services/wallet/StakingService.h"

#include "core/logging/Logger.h"
#include "datahub/DataHub.h"

#include <QCoreApplication>

#include <atomic>

namespace fincept::wallet {

namespace {

constexpr const char* kLocksFamily = "wallet:locks:";
constexpr const char* kVeFncptFamily = "wallet:vefncpt:";

constexpr qint64 kSecondsPerMonth = 30LL * 24 * 60 * 60;
constexpr qint64 kSecondsPerYear = 365LL * 24 * 60 * 60;

} // namespace

StakingService& StakingService::instance() {
    static StakingService inst;
    return inst;
}

StakingService::StakingService(QObject* parent) : QObject(parent) {}

StakingService::~StakingService() = default;

QStringList StakingService::topic_patterns() const {
    return QStringList{
        QStringLiteral("wallet:locks:*"),
        QStringLiteral("wallet:vefncpt:*"),
    };
}

double StakingService::multiplier_for(Duration d) noexcept {
    switch (d) {
        case Duration::ThreeMonths:
            return 0.0625;
        case Duration::SixMonths:
            return 0.125;
        case Duration::OneYear:
            return 0.25;
        case Duration::TwoYears:
            return 0.5;
        case Duration::FourYears:
            return 1.0;
    }
    return 0.0;
}

QString StakingService::label_for(Duration d) {
    switch (d) {
        case Duration::ThreeMonths:
            return QStringLiteral("3 MO");
        case Duration::SixMonths:
            return QStringLiteral("6 MO");
        case Duration::OneYear:
            return QStringLiteral("1 YR");
        case Duration::TwoYears:
            return QStringLiteral("2 YR");
        case Duration::FourYears:
            return QStringLiteral("4 YR");
    }
    return {};
}

qint64 StakingService::seconds_for(Duration d) noexcept {
    switch (d) {
        case Duration::ThreeMonths:
            return kSecondsPerMonth * 3;
        case Duration::SixMonths:
            return kSecondsPerMonth * 6;
        case Duration::OneYear:
            return kSecondsPerYear;
        case Duration::TwoYears:
            return kSecondsPerYear * 2;
        case Duration::FourYears:
            return kSecondsPerYear * 4;
    }
    return 0;
}

bool StakingService::staking_live() const {
    // Flip only together with a real position decoder (getProgramAccounts +
    // Anchor/Borsh deserialisation of fincept_lock accounts) and a real
    // build_lock_tx(). Until then every staking number would be invented.
    return false;
}

QString StakingService::not_live_reason() {
    return QCoreApplication::translate("StakingService",
                                       "On-chain staking is not live yet — the fincept_lock program is not deployed, "
                                       "so lock positions can't be read or created in this build.");
}

void StakingService::refresh(const QStringList& topics) {
    // Never publish data: there is no source for it. An explicit error keeps
    // any consumer (hub introspection, a future panel) from mistaking silence
    // for "this wallet has no locks".
    static std::atomic<bool> once{false};
    if (!once.exchange(true))
        LOG_INFO("Staking", "lock topics requested — staking is not live in this build, reporting unavailable");

    auto& hub = fincept::datahub::DataHub::instance();
    const QString reason = not_live_reason();
    for (const auto& topic : topics) {
        if (topic.startsWith(QLatin1String(kLocksFamily)) || topic.startsWith(QLatin1String(kVeFncptFamily)))
            hub.publish_error(topic, reason);
    }
}

void StakingService::build_lock_tx(const QString& /*pubkey*/, const QString& /*amount_raw*/, Duration /*duration*/,
                                   std::function<void(Result<QString>)> callback) {
    // Real implementation (when the program ships): a versioned tx carrying the
    // Borsh-encoded `lock` instruction and the position-NFT mint PDA.
    callback(Result<QString>::err(not_live_reason().toStdString()));
}

} // namespace fincept::wallet
