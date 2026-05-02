#include "services/wallet/StakingService.h"

#include "core/logging/Logger.h"
#include "datahub/DataHub.h"
#include "services/wallet/SolanaRpcClient.h"
#include "storage/secure/SecureStorage.h"

#include <QDateTime>
#include <QPointer>
#include <QVariant>

#include <atomic>
#include <cmath>

namespace fincept::wallet {

namespace {

constexpr const char* kLocksFamily   = "wallet:locks:";
constexpr const char* kVeFncptFamily = "wallet:vefncpt:";
constexpr const char* kKeyProgramId  = "fincept.lock_program_id";

constexpr qint64 kSecondsPerMonth = 30LL * 24 * 60 * 60;
constexpr qint64 kSecondsPerYear  = 365LL * 24 * 60 * 60;

/// Demo locks for plan §3.2. Returned for any pubkey in mock mode so the
/// UI is reviewable without an Anchor program. Numbers are anchored to
/// the now() so the "X days remaining" text feels live.
QVector<LockPosition> mock_positions() {
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    QVector<LockPosition> out;
    out.reserve(3);

    auto make = [now](const QString& id_seed, const QString& amount,
                       qint64 duration_sec, double lifetime_yield) {
        LockPosition p;
        p.position_id = QStringLiteral("MockLock") + id_seed;
        p.amount_raw = amount;
        p.decimals = 6;
        // Pretend the lock was created 1/3 of the way through its duration.
        p.lock_start_ts = now - (duration_sec / 3) * 1000;
        p.unlock_ts = p.lock_start_ts + duration_sec * 1000;
        p.duration_secs = duration_sec;
        // Weight = amount × multiplier-for-duration. Multipliers below.
        const double mult = (duration_sec >= kSecondsPerYear * 4) ? 1.0
                          : (duration_sec >= kSecondsPerYear * 2) ? 0.5
                          : (duration_sec >= kSecondsPerYear)     ? 0.25
                          : (duration_sec >= kSecondsPerMonth * 6) ? 0.125
                          : 0.0625;
        const auto amt_units = amount.toULongLong();
        p.weight_raw = QString::number(static_cast<quint64>(amt_units * mult));
        p.lifetime_yield_usdc = lifetime_yield;
        p.is_mock = true;
        return p;
    };

    out.push_back(make(QStringLiteral("4yr"),
                       QStringLiteral("2000000000"), // 2,000 @ 6dp
                       kSecondsPerYear * 4, 1240.0));
    out.push_back(make(QStringLiteral("1yr"),
                       QStringLiteral("1000000000"), // 1,000 @ 6dp
                       kSecondsPerYear, 42.0));
    out.push_back(make(QStringLiteral("6mo"),
                       QStringLiteral("500000000"),  // 500 @ 6dp
                       kSecondsPerMonth * 6, 14.0));
    return out;
}

} // namespace

StakingService& StakingService::instance() {
    static StakingService inst;
    return inst;
}

StakingService::StakingService(QObject* parent) : QObject(parent) {
    rpc_ = new SolanaRpcClient(this);
}

StakingService::~StakingService() = default;

QStringList StakingService::topic_patterns() const {
    return QStringList{
        QStringLiteral("wallet:locks:*"),
        QStringLiteral("wallet:vefncpt:*"),
    };
}

double StakingService::multiplier_for(Duration d) noexcept {
    switch (d) {
        case Duration::ThreeMonths: return 0.0625;
        case Duration::SixMonths:   return 0.125;
        case Duration::OneYear:     return 0.25;
        case Duration::TwoYears:    return 0.5;
        case Duration::FourYears:   return 1.0;
    }
    return 0.0;
}

QString StakingService::label_for(Duration d) {
    switch (d) {
        case Duration::ThreeMonths: return QStringLiteral("3 MO");
        case Duration::SixMonths:   return QStringLiteral("6 MO");
        case Duration::OneYear:     return QStringLiteral("1 YR");
        case Duration::TwoYears:    return QStringLiteral("2 YR");
        case Duration::FourYears:   return QStringLiteral("4 YR");
    }
    return {};
}

qint64 StakingService::seconds_for(Duration d) noexcept {
    switch (d) {
        case Duration::ThreeMonths: return kSecondsPerMonth * 3;
        case Duration::SixMonths:   return kSecondsPerMonth * 6;
        case Duration::OneYear:     return kSecondsPerYear;
        case Duration::TwoYears:    return kSecondsPerYear * 2;
        case Duration::FourYears:   return kSecondsPerYear * 4;
    }
    return 0;
}

QString StakingService::pubkey_from_locks_topic(const QString& topic) {
    return topic.mid(static_cast<int>(qstrlen(kLocksFamily)));
}

QString StakingService::pubkey_from_vefncpt_topic(const QString& topic) {
    return topic.mid(static_cast<int>(qstrlen(kVeFncptFamily)));
}

QString StakingService::resolve_program_id() const {
    auto r = SecureStorage::instance().retrieve(QString::fromLatin1(kKeyProgramId));
    if (r.is_ok()) return r.value().trimmed();
    return {};
}

bool StakingService::program_is_configured() const {
    return !resolve_program_id().isEmpty();
}

void StakingService::refresh(const QStringList& topics) {
    const auto program_id = resolve_program_id();
    const bool mock = program_id.isEmpty();
    if (mock) {
        // Log once per process — refresh fires every 30 s in mock mode.
        static std::atomic<bool> once{false};
        if (!once.exchange(true)) {
            LOG_INFO("Staking",
                     "no fincept.lock_program_id configured — serving mock locks");
        } else {
            LOG_DEBUG("Staking", "mock-mode refresh");
        }
    }

    for (const auto& topic : topics) {
        if (topic.startsWith(QLatin1String(kLocksFamily))) {
            const auto pk = pubkey_from_locks_topic(topic);
            if (pk.isEmpty()) continue;
            mock ? publish_mock_locks(topic, pk)
                 : refresh_locks_real(topic, pk, program_id);
        } else if (topic.startsWith(QLatin1String(kVeFncptFamily))) {
            const auto pk = pubkey_from_vefncpt_topic(topic);
            if (pk.isEmpty()) continue;
            mock ? publish_mock_vefncpt(topic, pk)
                 : refresh_vefncpt_real(topic, pk, program_id);
        }
    }
}

// ── Real path stubs ────────────────────────────────────────────────────────
//
// Real implementation requires:
//   - SolanaRpcClient::get_program_accounts (not yet wrapped)
//   - Anchor account discriminator + Borsh deserialiser for `LockPosition`
//   - PDA derivation: position_pda = find_program_address(["lock", owner, idx])
// All of which depend on the program being deployed. Until then, these
// methods publish_error so the UI shows a clear "program not deployed" state
// rather than silent empty rows.

void StakingService::refresh_locks_real(const QString& topic,
                                        const QString& /*pubkey*/,
                                        const QString& /*program_id*/) {
    auto& hub = fincept::datahub::DataHub::instance();
    hub.publish_error(topic,
        QStringLiteral("real fetch not implemented yet — Anchor program "
                       "deployment + Borsh deserialiser pending"));
}

void StakingService::refresh_vefncpt_real(const QString& topic,
                                          const QString& /*pubkey*/,
                                          const QString& /*program_id*/) {
    auto& hub = fincept::datahub::DataHub::instance();
    hub.publish_error(topic,
        QStringLiteral("real fetch not implemented yet — Anchor program "
                       "deployment + Borsh deserialiser pending"));
}

// ── Mock path ──────────────────────────────────────────────────────────────

void StakingService::publish_mock_locks(const QString& topic, const QString& pubkey) {
    auto& hub = fincept::datahub::DataHub::instance();
    auto positions = mock_positions();
    hub.publish(topic, QVariant::fromValue(positions));
    emit locks_published(pubkey);
}

void StakingService::publish_mock_vefncpt(const QString& topic, const QString& pubkey) {
    const auto positions = mock_positions();

    // Sum weights at full precision in atomic units.
    quint64 total_weight = 0;
    for (const auto& p : positions) {
        bool ok = false;
        total_weight += p.weight_raw.toULongLong(&ok);
        // toULongLong silently returns 0 on failure — fine for mock data.
    }

    VeFncptAggregate agg;
    agg.pubkey_b58 = pubkey;
    agg.total_weight_raw = QString::number(total_weight);
    agg.decimals = 6;
    agg.position_count = positions.size();
    // Projected next-period yield: at $42k weekly revenue × 25 % staker
    // share × the user's share of total weight. For the mock, assume the
    // user owns 100 % of the global weight (single-user demo) — projects
    // ~$10,500 per week. Real path divides by the global weight reported
    // by the program.
    agg.projected_next_period_yield_usdc = 42'310.0 * 0.25;
    agg.ts_ms = QDateTime::currentMSecsSinceEpoch();
    agg.is_mock = true;
    fincept::datahub::DataHub::instance().publish(topic, QVariant::fromValue(agg));
}

// ── Tx-building helpers ────────────────────────────────────────────────────
//
// In mock mode these always fail with a clear message. The LOCK / EXTEND /
// WITHDRAW buttons in the UI catch the error and surface it in the panel's
// error strip rather than opening a confirm dialog.

void StakingService::build_lock_tx(const QString& /*pubkey*/,
                                   const QString& /*amount_raw*/,
                                   Duration /*duration*/,
                                   std::function<void(Result<QString>)> callback) {
    if (!program_is_configured()) {
        callback(Result<QString>::err(
            "fincept_lock Anchor program not deployed yet. Configure "
            "SecureStorage `fincept.lock_program_id` once the program "
            "ships to enable real locks."));
        return;
    }
    // Real implementation: build a versioned tx with the `lock` instruction.
    // Requires the Borsh-encoded args and the position-NFT mint PDA.
    callback(Result<QString>::err("real build_lock_tx not yet implemented"));
}

void StakingService::build_extend_tx(const QString& /*pubkey*/,
                                     const QString& /*position_id*/,
                                     Duration /*new_duration*/,
                                     std::function<void(Result<QString>)> callback) {
    if (!program_is_configured()) {
        callback(Result<QString>::err(
            "fincept_lock Anchor program not deployed yet."));
        return;
    }
    callback(Result<QString>::err("real build_extend_tx not yet implemented"));
}

void StakingService::build_withdraw_tx(const QString& /*pubkey*/,
                                       const QString& /*position_id*/,
                                       std::function<void(Result<QString>)> callback) {
    if (!program_is_configured()) {
        callback(Result<QString>::err(
            "fincept_lock Anchor program not deployed yet."));
        return;
    }
    callback(Result<QString>::err("real build_withdraw_tx not yet implemented"));
}

} // namespace fincept::wallet
