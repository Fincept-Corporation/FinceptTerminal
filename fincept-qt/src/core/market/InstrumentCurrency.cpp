// InstrumentCurrency.cpp — see InstrumentCurrency.h. Qt Core only.

#include "core/market/InstrumentCurrency.h"

#include "core/market/MarketSessions.h"

#include <QHash>

namespace fincept::core::market {

namespace {

// Yahoo suffixes whose currency is not (or not only) the venue table's: listing
// suffixes without a session model, and books that trade several currencies.
// An empty value means "cannot be told from the symbol".
const QHash<QString, QString>& ic_extra_suffix_currency() {
    static const QHash<QString, QString> k = {
        {QStringLiteral("IL"), QString()}, // LSE International Order Book (mostly USD, not all)
        {QStringLiteral("AT"), QStringLiteral("EUR")}, // Athens
        {QStringLiteral("PR"), QStringLiteral("CZK")}, // Prague
        {QStringLiteral("BD"), QStringLiteral("HUF")}, // Budapest
        {QStringLiteral("CA"), QStringLiteral("EGP")}, // Cairo
        {QStringLiteral("QA"), QStringLiteral("QAR")}, // Qatar
        {QStringLiteral("RG"), QStringLiteral("EUR")}, // Riga
        {QStringLiteral("VS"), QStringLiteral("EUR")}, // Vilnius
        {QStringLiteral("TL"), QStringLiteral("EUR")}, // Tallinn
        {QStringLiteral("SN"), QStringLiteral("CLP")}, // Santiago
        {QStringLiteral("BA"), QStringLiteral("ARS")}, // Buenos Aires
        {QStringLiteral("PS"), QStringLiteral("PHP")}, // Philippines
    };
    return k;
}

bool ic_all_letters(const QString& s) {
    for (const QChar c : s)
        if (c < QLatin1Char('A') || c > QLatin1Char('Z'))
            return false;
    return !s.isEmpty();
}

// USD stablecoins quoted as their own currency (converted at "<CODE>-USD").
// Same list as the crypto rows of ui/formatting/CurrencyTable.cpp.
bool ic_is_stablecoin(const QString& s) {
    return s == QLatin1String("USDT") || s == QLatin1String("USDC") || s == QLatin1String("DAI") ||
           s == QLatin1String("FDUSD") || s == QLatin1String("TUSD") || s == QLatin1String("BUSD");
}

} // namespace

QString currency_for_symbol(const QString& symbol, const QString& exchange_hint) {
    QString s = symbol.trimmed().toUpper();
    QString hint = exchange_hint.trimmed();
    const qsizetype colon = s.indexOf(QLatin1Char(':'));
    if (colon > 0) { // "NSE:RELIANCE-EQ": the prefix names the exchange
        if (hint.isEmpty())
            hint = s.left(colon);
        s = s.mid(colon + 1);
    }
    // Index levels are not money; futures are quoted in dollars or cents per contract spec.
    if (s.isEmpty() || s.startsWith(QLatin1Char('^')) || s.endsWith(QLatin1String("=F")))
        return {};
    if (s.endsWith(QLatin1String("=X"))) {
        // A pair is priced in its quote currency; Yahoo's "JPY=X" is USD/JPY.
        const QString pair = s.chopped(2);
        if (pair.size() == 6 && ic_all_letters(pair))
            return pair.right(3);
        if (pair.size() == 3 && ic_all_letters(pair))
            return pair;
        return {};
    }

    const qsizetype dot = s.lastIndexOf(QLatin1Char('.'));
    if (dot >= 0) {
        const QString suffix = s.mid(dot + 1);
        const auto& extra = ic_extra_suffix_currency();
        const auto it = extra.constFind(suffix);
        if (it != extra.constEnd())
            return it.value();
        if (const Venue* v = venue_for_suffix(suffix))
            return v->currency;
        return {}; // unknown suffix (or a class share like BRK.B)
    }

    // Crypto pairs: BTC-USD, ETH-EUR, SOL-USDT (a stablecoin is its own
    // currency, not USD: it converts at its own rate). Share classes (BRK-B)
    // fall through.
    const qsizetype dash = s.lastIndexOf(QLatin1Char('-'));
    if (dash > 0) {
        const QString quote = s.mid(dash + 1);
        if (ic_is_stablecoin(quote))
            return quote;
        if (quote.size() == 3 && quote.at(0).isLetter())
            return quote;
    }

    if (!hint.isEmpty()) {
        const Venue* v = venue_by_code(hint);
        return (v && !v->currency.isEmpty()) ? v->currency : QString();
    }
    return QStringLiteral("USD"); // bare ticker = US listing
}

double minor_unit_divisor(const QString& ccy) {
    const QString c = ccy.trimmed();
    // Case matters: "GBp" is pence, "GBP" is pounds.
    if (c == QLatin1String("GBp") || c == QLatin1String("GBX") || c == QLatin1String("GBx") ||
        c == QLatin1String("ZAc") || c == QLatin1String("ZAC") || c == QLatin1String("ILA") ||
        c == QLatin1String("ILa") || c == QLatin1String("USX") || c == QLatin1String("USc"))
        return 100.0;
    if (c == QLatin1String("KWF"))
        return 1000.0;
    return 1.0;
}

bool is_minor_unit(const QString& ccy) {
    return minor_unit_divisor(ccy) != 1.0;
}

QString major_currency(const QString& ccy) {
    const QString c = ccy.trimmed();
    if (c.isEmpty())
        return {};
    if (c == QLatin1String("GBp") || c == QLatin1String("GBX") || c == QLatin1String("GBx"))
        return QStringLiteral("GBP");
    if (c == QLatin1String("ZAc") || c == QLatin1String("ZAC"))
        return QStringLiteral("ZAR");
    if (c == QLatin1String("ILA") || c == QLatin1String("ILa"))
        return QStringLiteral("ILS");
    if (c == QLatin1String("USX") || c == QLatin1String("USc"))
        return QStringLiteral("USD");
    if (c == QLatin1String("KWF"))
        return QStringLiteral("KWD");
    return c.toUpper();
}

double to_major_units(double amount, const QString& ccy) {
    return amount / minor_unit_divisor(ccy);
}

} // namespace fincept::core::market
