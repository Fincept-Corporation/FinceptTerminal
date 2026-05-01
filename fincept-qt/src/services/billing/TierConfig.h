#pragma once

#include "services/wallet/WalletTypes.h"

#include <QString>
#include <QStringList>

namespace fincept::billing {

/// Tier thresholds + cross-screen gating config (Phase 3 §3.6).
///
/// Header-only — bumped to a config file by Phase 4 if thresholds need to
/// vary by environment. For now they're pinned and the derivation is pure.
///
/// Atomic units (decimals = 6 to match $FNCPT). veFNCPT weight = locked
/// $FNCPT × duration multiplier, so the thresholds compare apples to
/// apples — 100 veFNCPT means either 100 $FNCPT locked for 4 yrs (1.0×)
/// or 1,600 $FNCPT locked for 3 mo (0.0625×), etc.
struct TierConfig {
    static constexpr int kDecimals = 6;

    static constexpr quint64 kBronzeThresholdRaw  = 100ULL * 1'000'000ULL;     ///< 100 veFNCPT
    static constexpr quint64 kSilverThresholdRaw  = 1'000ULL * 1'000'000ULL;   ///< 1,000 veFNCPT
    static constexpr quint64 kGoldThresholdRaw    = 10'000ULL * 1'000'000ULL;  ///< 10,000 veFNCPT

    /// Tier from atomic weight. Pure function — used by both `TierService`
    /// (when publishing the topic) and any caller that needs a synchronous
    /// view (cross-screen gating in `AI Quant Lab`, `Alpha Arena`).
    static fincept::wallet::TierStatus::Tier tier_from_weight(quint64 weight_raw) noexcept {
        using Tier = fincept::wallet::TierStatus::Tier;
        if (weight_raw >= kGoldThresholdRaw)   return Tier::Gold;
        if (weight_raw >= kSilverThresholdRaw) return Tier::Silver;
        if (weight_raw >= kBronzeThresholdRaw) return Tier::Bronze;
        return Tier::Free;
    }

    /// Raw amount of veFNCPT required to reach the next tier from `current`.
    /// Returns 0 at Gold (no next tier).
    static quint64 next_threshold_raw(fincept::wallet::TierStatus::Tier current) noexcept {
        using Tier = fincept::wallet::TierStatus::Tier;
        switch (current) {
            case Tier::Free:   return kBronzeThresholdRaw;
            case Tier::Bronze: return kSilverThresholdRaw;
            case Tier::Silver: return kGoldThresholdRaw;
            case Tier::Gold:   return 0;
        }
        return 0;
    }

    /// User-facing label.
    static QString label_for(fincept::wallet::TierStatus::Tier t) {
        using Tier = fincept::wallet::TierStatus::Tier;
        switch (t) {
            case Tier::Free:   return QStringLiteral("FREE");
            case Tier::Bronze: return QStringLiteral("BRONZE");
            case Tier::Silver: return QStringLiteral("SILVER");
            case Tier::Gold:   return QStringLiteral("GOLD");
        }
        return {};
    }

    /// Stable feature IDs that each tier unlocks. Consumed by gating
    /// checks elsewhere in the terminal (AI Quant Lab, Alpha Arena, paid
    /// screens). The exhaustive list below is the **plan §3.6 mapping**
    /// — keep it tight; new gates require an explicit decision.
    static QStringList features_unlocked_by(fincept::wallet::TierStatus::Tier t) {
        using Tier = fincept::wallet::TierStatus::Tier;
        switch (t) {
            case Tier::Free:
                return {};
            case Tier::Bronze:
                return {QStringLiteral("api-quota-basic")};
            case Tier::Silver:
                return {QStringLiteral("api-quota-basic"),
                        QStringLiteral("premium-screens"),
                        QStringLiteral("ai-quant-lab")};
            case Tier::Gold:
                return {QStringLiteral("api-quota-basic"),
                        QStringLiteral("premium-screens"),
                        QStringLiteral("ai-quant-lab"),
                        QStringLiteral("alpha-arena"),
                        QStringLiteral("all-agents")};
        }
        return {};
    }
};

} // namespace fincept::billing
