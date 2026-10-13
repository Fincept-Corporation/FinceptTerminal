#pragma once

#include <QString>
#include <QStringList>

namespace fincept::billing {

/// Static configuration for the PLANNED $FNCPT fee discount (Phase 2 §2C).
///
/// Header-only by design — these are compile-time constants that both
/// `FeeDiscountService` and `FeeDiscountPanel` read.
///
/// Eligibility is computed and shown, but no checkout / billing path applies
/// the discount yet (neither the terminal nor the backend keys on these SKU
/// ids), so every user-facing surface must present it as planned.
struct FeeDiscountConfig {
    /// Eligibility threshold in raw $FNCPT atomic units (decimals = 6).
    /// 1,000 $FNCPT = 1,000 × 10^6 = 1,000,000,000 raw.
    static constexpr quint64 kThresholdRaw = 1000ULL * 1000000ULL;

    /// $FNCPT mint decimals — pump.fun standard. Used to convert raw → UI
    /// when rendering "1,000 $FNCPT" instead of the atomic count.
    static constexpr int kThresholdDecimals = 6;

    /// Planned discount percentage on the SKUs below.
    static constexpr int kDiscountPct = 30;

    /// SKUs the discount is planned for. The strings are stable IDs the
    /// billing pipeline will eventually key on (nothing does yet); the panel
    /// renders human labels via `display_label()`.
    static QStringList applied_skus() {
        return QStringList{
            QStringLiteral("ai-report"),
            QStringLiteral("deep-backtest"),
            QStringLiteral("premium-screen"),
        };
    }

    /// Human-readable label for a SKU id. Unknown ids fall back to the
    /// raw id (so a future SKU added on the backend at least renders).
    static QString display_label(const QString& sku) {
        if (sku == QStringLiteral("ai-report"))
            return QStringLiteral("AI Reports");
        if (sku == QStringLiteral("deep-backtest"))
            return QStringLiteral("Deep Backtests");
        if (sku == QStringLiteral("premium-screen"))
            return QStringLiteral("Premium Screens");
        return sku;
    }

    /// Indicative reference price for the savings preview on the
    /// FeeDiscountPanel. Not a real billed amount — purely a UX hint.
    /// The savings = (reference × kDiscountPct / 100).
    static constexpr double kReferencePriceUsd = 9.99;
};

} // namespace fincept::billing
