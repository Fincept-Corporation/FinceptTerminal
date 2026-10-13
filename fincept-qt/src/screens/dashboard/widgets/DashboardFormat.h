#pragma once
// Number formatting shared by the dashboard tiles.
//
// The tiles used to format every price as QString::number(v, 'f', 2) with a
// hard-coded "$": EURUSD showed 1.08 (pips lost), DOGE 0.12, the Dow 42512.34
// with no grouping, and an INR / index / FX quote carried a dollar sign. The
// quote feed (QuoteData) has no currency field, so prices are shown as plain
// numbers — the convention on professional terminals.

#include <QLocale>
#include <QString>

#include <cmath>

namespace fincept::screens::widgets::dash_fmt {

/// Grouped "1,234.56" formatting independent of the OS locale.
inline QString grouped(double v, int decimals) {
    static const QLocale kLocale(QLocale::English, QLocale::UnitedStates);
    return kLocale.toString(v, 'f', decimals);
}

/// Decimals a price needs: FX rates (Yahoo "=X") keep pips, sub-unit prices
/// keep 4-6 significant decimals, everything else 2.
inline int price_decimals(double v, const QString& symbol = {}) {
    const double a = std::abs(v);
    if (symbol.endsWith(QLatin1String("=X")))
        return a < 10.0 ? 4 : 3; // EURUSD 1.0834, USDJPY 151.235
    if (a == 0.0 || a >= 1.0)
        return 2;
    if (a >= 0.01)
        return 4;
    return 6;
}

/// A price: grouped, adaptive decimals, no currency sign. "--" when not set.
inline QString price(double v, const QString& symbol = {}) {
    if (!std::isfinite(v) || v <= 0.0)
        return QStringLiteral("--");
    return grouped(v, price_decimals(v, symbol));
}

/// A signed absolute change in the price's own decimals ("+1,234.50").
inline QString signed_change(double change, double ref_price, const QString& symbol = {}) {
    if (!std::isfinite(change))
        return QStringLiteral("--");
    const QString body = grouped(std::abs(change), price_decimals(ref_price, symbol));
    return (change > 0 ? QStringLiteral("+") : change < 0 ? QStringLiteral("-") : QString()) + body;
}

/// A signed percent ("+1.23%"). `pct` is already in percent units.
inline QString signed_pct(double pct, int decimals = 2) {
    if (!std::isfinite(pct))
        return QStringLiteral("--");
    return (pct > 0 ? QStringLiteral("+") : QString()) + QString::number(pct, 'f', decimals) + QLatin1Char('%');
}

/// A difference of two percentages, in percentage points ("+0.35 pp").
inline QString signed_pp(double pp, int decimals = 2) {
    if (!std::isfinite(pp))
        return QStringLiteral("--");
    return (pp > 0 ? QStringLiteral("+") : QString()) + QString::number(pp, 'f', decimals) + QStringLiteral(" pp");
}

/// A share / contract quantity: integers without decimals, fractional
/// quantities (fractional shares, crypto) with up to 4, trailing zeros trimmed.
inline QString quantity(double q) {
    if (!std::isfinite(q))
        return QStringLiteral("--");
    if (std::abs(q - std::round(q)) < 1e-9)
        return grouped(std::round(q), 0);
    QString s = QString::number(q, 'f', 4);
    while (s.endsWith(QLatin1Char('0')))
        s.chop(1);
    if (s.endsWith(QLatin1Char('.')))
        s.chop(1);
    return s;
}

/// Compact volume ("12.3M"); "--" for none.
inline QString compact_volume(double v) {
    if (!std::isfinite(v) || v <= 0.0)
        return QStringLiteral("--");
    if (v >= 1e9)
        return QString::number(v / 1e9, 'f', 2) + QLatin1Char('B');
    if (v >= 1e6)
        return QString::number(v / 1e6, 'f', 2) + QLatin1Char('M');
    if (v >= 1e3)
        return QString::number(v / 1e3, 'f', 1) + QLatin1Char('K');
    return QString::number(static_cast<qint64>(v));
}

} // namespace fincept::screens::widgets::dash_fmt
