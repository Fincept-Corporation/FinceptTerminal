// src/screens/equity_research/EquityResearchFormat.h
//
// Number / currency formatting shared by the Equity Research tabs — a thin layer over
// the app's one formatter (ui/formatting/NumberFormat.h), so a market cap, a margin or a
// per-share price reads the same on every tab and follows the user's number locale and
// compact-number scheme (K/M/B or lakh/crore). Missing values render as "—", never as 0,
// "nan" or "inf".
//
// Listing: what a listing is QUOTED in vs what it REPORTS in. London prices are in pence
// (GBp), Johannesburg in cents (ZAc), Tel Aviv in agorot (ILA); Vodafone trades in GBp and
// reports in EUR; an ADR trades in USD and reports in TWD. Every comparison of a price with
// a statement-derived per-share figure goes through Listing: minor units are converted to
// the major unit first, and a different reporting currency is converted at the spot rate
// (FxRateService) — or not compared at all when no rate is known.
#pragma once

#include "services/equity/EquityResearchModels.h"
#include "services/markets/FxRateService.h"
#include "ui/formatting/NumberFormat.h"

#include <QCoreApplication>
#include <QString>

#include <cmath>
#include <limits>
#include <optional>

namespace fincept::screens::equity_fmt {

namespace uf = fincept::ui::formatting;

inline QString dash() {
    return uf::placeholder();
}

inline bool missing(double v) {
    return !std::isfinite(v);
}

/// Display symbol of a currency code ("$", "€", "p" for pence). Prefer money() / price(),
/// which place the symbol per locale and handle suffix symbols.
inline QString currency_symbol(const QString& code) {
    return code.isEmpty() ? QString() : uf::currency_symbol(code);
}

/// Abbreviated number in the user's scheme: 1.23T / 456.78B / 12.30M (or lakh / crore).
inline QString large(double v, int decimals = -1) {
    return missing(v) ? dash() : uf::compact(v, uf::Compact::FromSettings, QString(), decimals);
}

/// Grouped fixed-point number in the user's locale (1,234,567.89 / 1.234.567,89).
inline QString grouped(double v, int decimals = 2) {
    return missing(v) ? dash() : uf::number(v, decimals);
}

/// A fraction as a percentage: 0.1234 → "12.34%"; `signed_` adds "+" to positives.
inline QString pct(double fraction, int decimals = 2, bool signed_ = false) {
    return missing(fraction) ? dash() : uf::fraction_pct(fraction, decimals, signed_);
}

/// Percentage points: "+0.35 pp".
inline QString pp(double points, int decimals = 1) {
    return missing(points) ? dash() : uf::pp(points, decimals, true);
}

/// A multiple: "12.3x"; "—" for missing / non-positive values (a negative P/E is "no
/// earnings", not "cheap").
inline QString multiple(double v, int decimals = 1) {
    return (missing(v) || v <= 0.0) ? dash() : uf::ratio(v, decimals);
}

/// A per-share price in its own currency as quoted: "$182.40", "116.95p", "¥3,418",
/// "₹1,339.70". `decimals` = -1 picks them from the currency and magnitude.
inline QString price(double v, const QString& currency_code, int decimals = -1) {
    if (missing(v))
        return dash();
    uf::InstrumentFmt f;
    f.currency = currency_code;
    f.decimals = decimals;
    return uf::price_with_symbol(v, f);
}

/// An amount of money, abbreviated: "$3.95T", "€6.40B", "₹1.81 L Cr". Minor-unit codes
/// are taken as their major currency (market caps are reported in pounds, not pence).
inline QString money(double v, const QString& currency_code) {
    if (missing(v))
        return dash();
    const QString ccy = currency_code.isEmpty() ? QString() : uf::major_currency(currency_code);
    return uf::money(v, ccy, uf::Compact::FromSettings);
}

/// money() with a "+" on positives.
inline QString signed_money(double v, const QString& currency_code) {
    if (missing(v))
        return dash();
    const QString ccy = currency_code.isEmpty() ? QString() : uf::major_currency(currency_code);
    return uf::signed_money(v, ccy, uf::Compact::FromSettings);
}

/// What a listing is quoted in vs what its statements are reported in.
struct Listing {
    QString quote;     ///< trading currency as quoted ("GBp", "USD"); may be empty
    QString reporting; ///< statements currency ("EUR"); empty = same as the quote's major

    QString quote_major() const { return quote.isEmpty() ? QString() : uf::major_currency(quote); }
    QString reporting_major() const {
        return reporting.isEmpty() ? quote_major() : uf::major_currency(reporting);
    }
    /// Quoted in a minor unit (pence, cents, agorot).
    bool minor_quote() const { return !quote.isEmpty() && uf::minor_unit_divisor(quote) > 1; }
    /// Quote and statements share a major currency (GBp vs GBP counts as the same).
    bool same_currency() const { return reporting_major().isEmpty() || reporting_major() == quote_major(); }
    /// A quoted price in the major unit: 116.95 GBp → 1.1695 (GBP).
    double to_major(double quoted) const { return quote.isEmpty() ? quoted : uf::to_major_units(quoted, quote); }
    /// A quoted price (pence) → the reporting currency, at the spot rate when the
    /// currencies differ. NaN when no rate is known yet.
    double quote_to_reporting(double quoted) const {
        if (missing(quoted))
            return quoted;
        const double major = to_major(quoted);
        if (same_currency())
            return major;
        const auto r = services::FxRateService::instance().rate(quote_major(), reporting_major());
        return r ? major * *r : std::numeric_limits<double>::quiet_NaN();
    }
    /// An amount in the reporting currency → quoted units (pence for GBp). NaN without a rate.
    double reporting_to_quote(double amount) const {
        if (missing(amount))
            return amount;
        double major = amount;
        if (!same_currency()) {
            const auto r = services::FxRateService::instance().rate(reporting_major(), quote_major());
            if (!r)
                return std::numeric_limits<double>::quiet_NaN();
            major = amount * *r;
        }
        return quote.isEmpty() ? major : major * uf::minor_unit_divisor(quote);
    }
    /// Spot rate reporting → quote major, when the currencies differ (nullopt when same or unknown).
    std::optional<double> fx_reporting_to_quote() const {
        if (same_currency())
            return std::nullopt;
        return services::FxRateService::instance().rate(reporting_major(), quote_major());
    }
    /// Subscribe the FX legs this listing needs (FxRateService::rates_changed follows).
    void ensure_fx() const {
        if (!same_currency() && !quote_major().isEmpty())
            services::FxRateService::instance().ensure({{reporting_major(), quote_major()}});
    }
    /// "Quoted in GBp (pence) · reports in EUR" — empty when nothing is known.
    QString describe() const {
        if (quote.isEmpty())
            return {};
        const char* ctx = "EquityResearchFormat";
        QString q = minor_quote()
                        ? QCoreApplication::translate(ctx, "Quoted in %1 (1/%2 %3)")
                              .arg(quote)
                              .arg(uf::minor_unit_divisor(quote))
                              .arg(quote_major())
                        : QCoreApplication::translate(ctx, "Quoted in %1").arg(quote);
        if (!reporting.isEmpty())
            q += QStringLiteral(" · ") + QCoreApplication::translate(ctx, "reports in %1").arg(reporting_major());
        return q;
    }
};

inline Listing listing_of(const services::equity::StockInfo& info) {
    return Listing{info.currency, info.financial_currency};
}

/// Analyst consensus from Yahoo's recommendation key — the one mapping the header bar's REC
/// and the Overview / Analysis badges share.
struct Recommendation {
    QString text;  ///< translated label; empty when the key is missing / unknown ("none")
    int tone = 0;  ///< +1 buy side, 0 hold, -1 sell side
};

inline Recommendation recommendation(const QString& key) {
    // Labels live in the EquityResearchScreen translation context (where they always were).
    static const struct {
        const char* key;
        const char* label;
        int tone;
    } kMap[] = {
        {"strong_buy", QT_TRANSLATE_NOOP("EquityResearchScreen", "STRONG BUY"), 1},
        {"buy", QT_TRANSLATE_NOOP("EquityResearchScreen", "BUY"), 1},
        {"outperform", QT_TRANSLATE_NOOP("EquityResearchScreen", "OUTPERFORM"), 1},
        {"hold", QT_TRANSLATE_NOOP("EquityResearchScreen", "HOLD"), 0},
        {"neutral", QT_TRANSLATE_NOOP("EquityResearchScreen", "NEUTRAL"), 0},
        {"underperform", QT_TRANSLATE_NOOP("EquityResearchScreen", "UNDERPERFORM"), -1},
        {"sell", QT_TRANSLATE_NOOP("EquityResearchScreen", "SELL"), -1},
        {"strong_sell", QT_TRANSLATE_NOOP("EquityResearchScreen", "STRONG SELL"), -1},
    };
    const QString k = key.trimmed().toLower();
    for (const auto& m : kMap) {
        if (k == QLatin1String(m.key))
            return {QCoreApplication::translate("EquityResearchScreen", m.label), m.tone};
    }
    return {};
}

} // namespace fincept::screens::equity_fmt
