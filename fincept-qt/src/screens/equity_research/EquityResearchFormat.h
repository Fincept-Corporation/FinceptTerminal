// src/screens/equity_research/EquityResearchFormat.h
//
// Number/currency formatting shared by the Equity Research tabs, so a market cap, a
// margin or a per-share price reads the same on every tab. Missing values render as
// "—", never as 0, "nan" or "inf".
#pragma once

#include <QHash>
#include <QLocale>
#include <QString>

#include <cmath>

namespace fincept::screens::equity_fmt {

inline QString dash() {
    return QStringLiteral("—");
}

inline bool missing(double v) {
    return !std::isfinite(v);
}

/// "$", "€", "₹", … for an ISO currency code; unknown codes print as "XYZ ".
inline QString currency_symbol(const QString& code) {
    static const QHash<QString, QString> kSymbols = {
        {"USD", "$"},          {"EUR", "€"},    {"GBP", "£"},  {"GBp", "GBp "},   {"JPY", "¥"},
        {"CNY", "CN¥"},   {"INR", "₹"},    {"KRW", "₩"},  {"RUB", "₽"}, {"BRL", "R$"},
        {"TRY", "₺"},     {"CHF", "CHF "},      {"CAD", "C$"},      {"AUD", "A$"},     {"NZD", "NZ$"},
        {"HKD", "HK$"},        {"SGD", "S$"},        {"TWD", "NT$"},     {"MXN", "MX$"},    {"ZAR", "R"},
        {"SEK", "SEK "},       {"NOK", "NOK "},      {"DKK", "DKK "},    {"PLN", "zł"}, {"THB", "฿"},
        {"IDR", "Rp"},         {"MYR", "RM"},        {"PHP", "₱"},  {"CZK", "Kč"}, {"HUF", "Ft"},
        {"ILS", "₪"},     {"CLP", "CL$"},       {"COP", "COL$"},    {"PEN", "S/."},    {"ARS", "AR$"},
        {"EGP", "E£"},    {"NGN", "₦"},    {"PKR", "Rs"},      {"BDT", "৳"}, {"VND", "₫"},
        {"SAR", "SAR "},       {"QAR", "QAR "},      {"AED", "AED "},    {"KWD", "KWD "},   {"ILA", "ILA "},
    };
    if (code.isEmpty())
        return QStringLiteral("$");
    // "GBp" (pence) is case-sensitive on purpose: LSE prices quote in pence, statements in GBP.
    const auto exact = kSymbols.constFind(code);
    if (exact != kSymbols.constEnd())
        return exact.value();
    const auto it = kSymbols.constFind(code.toUpper());
    return it != kSymbols.constEnd() ? it.value() : code.toUpper() + QLatin1Char(' ');
}

/// 1.23T / 456.78B / 12.30M / 4.5K / 950 — sign kept, "—" for a missing value.
inline QString large(double v, int decimals = 2) {
    if (missing(v))
        return dash();
    const double a = std::fabs(v);
    QString s;
    if (a >= 1e12)
        s = QString::number(a / 1e12, 'f', decimals) + QLatin1Char('T');
    else if (a >= 1e9)
        s = QString::number(a / 1e9, 'f', decimals) + QLatin1Char('B');
    else if (a >= 1e6)
        s = QString::number(a / 1e6, 'f', decimals) + QLatin1Char('M');
    else if (a >= 1e3)
        s = QString::number(a / 1e3, 'f', 1) + QLatin1Char('K');
    else
        s = QString::number(a, 'f', a < 10.0 && a != std::floor(a) ? 2 : 0);
    return v < 0 ? QLatin1Char('-') + s : s;
}

/// Grouped fixed-point number (1,234,567.89).
inline QString grouped(double v, int decimals = 2) {
    if (missing(v))
        return dash();
    return QLocale(QLocale::English).toString(v, 'f', decimals);
}

/// A fraction as a percentage: 0.1234 → "12.34%"; `signed_` adds "+" to positives.
inline QString pct(double fraction, int decimals = 2, bool signed_ = false) {
    if (missing(fraction))
        return dash();
    const double p = fraction * 100.0;
    const QString body = QString::number(p, 'f', decimals) + QLatin1Char('%');
    return (signed_ && p > 0.0) ? QLatin1Char('+') + body : body;
}

/// Per-share money with the currency symbol: "$182.40", "₹2,945.10".
inline QString price(double v, const QString& currency_code, int decimals = 2) {
    if (missing(v))
        return dash();
    return currency_symbol(currency_code) + grouped(v, decimals);
}

} // namespace fincept::screens::equity_fmt
