#pragma once
// EquitySymbolMap — broker symbol -> Yahoo Finance ticker, and chart timeframe ->
// Yahoo history request. Pure functions, header-only, Qt Core only, so they are
// unit-tested without the screen (tests/tst_equity_symbol_map.cpp).
//
//   research_ticker_for   the screen's NSE->.NS / BSE->.BO hand-off to Equity Research
//   public_yahoo_ticker   the public-data fallback's mapping (EquityPublicFeed::yahoo_symbol)
//   public_history_spec   timeframe -> (period, interval) (EquityPublicFeed::history_spec)
//
// EquityTypes.h includes this header, so research_ticker_for stays where its
// callers already find it.

#include <QCoreApplication>
#include <QHash>
#include <QRegularExpression>
#include <QSet>
#include <QString>
#include <QStringList>

namespace fincept::screens::equity {

// Yahoo-style ticker for a broker symbol — what Equity Research (yfinance) expects:
// "RELIANCE" on NSE -> "RELIANCE.NS". Empty when the instrument has no equity
// counterpart (option / future / commodity contracts), so callers hide the action.
inline QString research_ticker_for(const QString& symbol, const QString& exchange) {
    static const QHash<QString, QString> kSuffix = {
        {"NSE", ".NS"}, {"BSE", ".BO"}, {"HKEX", ".HK"}, {"TSE", ".T"},   {"KRX", ".KS"}, {"SGX", ".SI"},
        {"ASX", ".AX"}, {"LSE", ".L"},  {"TSX", ".TO"},  {"XETR", ".DE"}, {"SIX", ".SW"},
    };
    static const QStringList kNoEquity = {"NFO", "BFO", "MCX", "CDS", "BCD", "NCDEX"};
    // Contract names end in strike+CE/PE or FUT (e.g. NIFTY2660923200CE, RELIANCE26JULFUT).
    static const QRegularExpression kDerivativeTail(QStringLiteral("(\\d(CE|PE)|FUT)$"));

    const QString sym = symbol.trimmed().toUpper();
    const QString ex = exchange.trimmed().toUpper();
    if (sym.isEmpty() || kNoEquity.contains(ex) || kDerivativeTail.match(sym).hasMatch())
        return {};
    const auto it = kSuffix.constFind(ex);
    if (it == kSuffix.constEnd() || sym.contains(QLatin1Char('.')))
        return sym;
    return sym + it.value();
}

struct PublicYahooTicker {
    QString ticker;             // e.g. "RELIANCE.NS", "^NSEI", "BRK-B"
    QString unsupported_reason; // set when Yahoo has no public equivalent
    bool ok() const { return !ticker.isEmpty(); }
};

// Chart timeframe ("1m".."1w") -> the Yahoo history request that serves it.
struct PublicHistorySpec {
    QString period;
    QString interval;
    qint64 bar_ms = 0;
    bool intraday = false;
};

namespace symbol_map_detail {

inline bool eqsm_is_us_exchange(const QString& ex) {
    static const QSet<QString> kUs = {QStringLiteral("NYSE"), QStringLiteral("NASDAQ"), QStringLiteral("AMEX"),
                                      QStringLiteral("ARCA"), QStringLiteral("BATS"),   QStringLiteral("CBOE"),
                                      QStringLiteral("XNYS"), QStringLiteral("XNAS"),   QStringLiteral("OTC")};
    return kUs.contains(ex);
}

// Exchanges whose instruments are exchange-traded derivatives / commodity
// contracts (and MCX's commodity indices) — Yahoo Finance carries none of them.
inline bool eqsm_is_contract_venue(const QString& ex) {
    static const QSet<QString> kVenues = {QStringLiteral("NFO"),   QStringLiteral("BFO"), QStringLiteral("MCX"),
                                          QStringLiteral("CDS"),   QStringLiteral("NCDEX"), QStringLiteral("BCD"),
                                          QStringLiteral("MCX_INDEX")};
    return kVenues.contains(ex);
}

// The instrument master's own exchange for Indian indices (InstrumentTypes.h:
// Upstox, Dhan, Kotak, 5paisa, Motilal, Groww, XTS, Noren all normalise to these).
inline bool eqsm_is_india_index_venue(const QString& ex) {
    return ex == QLatin1String("NSE_INDEX") || ex == QLatin1String("BSE_INDEX");
}

// Broker index names -> Yahoo index tickers. Keys are the symbol with spaces,
// dashes and underscores removed ("NIFTY 50" -> "NIFTY50").
inline QString eqsm_index_ticker(const QString& sym, const QString& ex) {
    static const QHash<QString, QString> kIndia = {
        {QStringLiteral("NIFTY"), QStringLiteral("^NSEI")},
        {QStringLiteral("NIFTY50"), QStringLiteral("^NSEI")},
        {QStringLiteral("BANKNIFTY"), QStringLiteral("^NSEBANK")},
        {QStringLiteral("NIFTYBANK"), QStringLiteral("^NSEBANK")},
        {QStringLiteral("FINNIFTY"), QStringLiteral("NIFTY_FIN_SERVICE.NS")},
        {QStringLiteral("NIFTYFINSERVICE"), QStringLiteral("NIFTY_FIN_SERVICE.NS")},
        {QStringLiteral("MIDCPNIFTY"), QStringLiteral("NIFTY_MID_SELECT.NS")},
        {QStringLiteral("NIFTYMIDSELECT"), QStringLiteral("NIFTY_MID_SELECT.NS")},
        {QStringLiteral("NIFTYNEXT50"), QStringLiteral("^NSMIDCP")},
        {QStringLiteral("NIFTYIT"), QStringLiteral("^CNXIT")},
        {QStringLiteral("INDIAVIX"), QStringLiteral("^INDIAVIX")},
        {QStringLiteral("SENSEX"), QStringLiteral("^BSESN")},
        {QStringLiteral("BSESENSEX"), QStringLiteral("^BSESN")},
    };
    static const QHash<QString, QString> kUs = {
        {QStringLiteral("SPX"), QStringLiteral("^GSPC")}, {QStringLiteral("NDX"), QStringLiteral("^NDX")},
        {QStringLiteral("DJI"), QStringLiteral("^DJI")},  {QStringLiteral("DJIA"), QStringLiteral("^DJI")},
        {QStringLiteral("VIX"), QStringLiteral("^VIX")},  {QStringLiteral("RUT"), QStringLiteral("^RUT")},
    };
    QString key = sym;
    if (key.endsWith(QLatin1String("-INDEX"))) // Fyers: NSE:NIFTY50-INDEX, BSE:SENSEX-INDEX
        key.chop(6);
    key.remove(QLatin1Char(' '));
    key.remove(QLatin1Char('-'));
    key.remove(QLatin1Char('_'));
    const bool any = ex.isEmpty() || ex == QLatin1String("INDICES");
    if (any || ex == QLatin1String("NSE") || ex == QLatin1String("BSE") || eqsm_is_india_index_venue(ex)) {
        const QString t = kIndia.value(key);
        if (!t.isEmpty())
            return t;
    }
    if (any || eqsm_is_us_exchange(ex))
        return kUs.value(key);
    return {};
}

} // namespace symbol_map_detail

// Broker symbol + exchange -> Yahoo ticker. Builds on research_ticker_for() and
// adds indices, broker series suffixes (-EQ), US class shares and FX pairs.
inline PublicYahooTicker public_yahoo_ticker(const QString& symbol, const QString& exchange) {
    using namespace symbol_map_detail;
    QString sym = symbol.trimmed().toUpper();
    QString ex = exchange.trimmed().toUpper();

    // "NSE:RELIANCE-EQ" (Fyers-style) — the prefix names the exchange.
    if (sym.contains(QLatin1Char(':'))) {
        if (ex.isEmpty())
            ex = sym.section(QLatin1Char(':'), 0, 0);
        sym = sym.section(QLatin1Char(':'), -1);
    }
    if (sym.isEmpty())
        return {{}, QCoreApplication::translate("EquityPublicFeed", "No symbol selected.")};

    // Already a Yahoo ticker: index (^NSEI), FX / futures (EURUSD=X, GC=F). Shared
    // watchlists can hold these.
    if (sym.startsWith(QLatin1Char('^')) || sym.contains(QLatin1Char('=')))
        return {sym, {}};

    if (ex == QLatin1String("XETRA"))
        ex = QStringLiteral("XETR"); // research_ticker_for's key for Frankfurt (.DE)

    const QString index = eqsm_index_ticker(sym, ex);
    if (!index.isEmpty())
        return {index, {}};

    // An index the table above does not know. Falling through used to hand the
    // bare name ("NIFTY AUTO", "NIFTYAUTO-INDEX.NS") to Yahoo as if it were a ticker.
    if (eqsm_is_india_index_venue(ex) || sym.endsWith(QLatin1String("-INDEX"))) {
        return {{},
                QCoreApplication::translate("EquityPublicFeed",
                                            "%1 is an index with no known Yahoo Finance ticker - connect a broker "
                                            "in ACCOUNTS to see it.")
                    .arg(sym)};
    }

    if (eqsm_is_contract_venue(ex)) {
        return {{},
                QCoreApplication::translate(
                    "EquityPublicFeed",
                    "%1 trades on %2. Yahoo Finance has no public data for exchange-traded derivatives or "
                    "commodity contracts - connect a broker in ACCOUNTS to see it.")
                    .arg(sym, ex)};
    }

    if (ex == QLatin1String("FOREX")) {
        QString pair = sym;
        pair.remove(QLatin1Char('/'));
        static const QRegularExpression kPair(QStringLiteral("^[A-Z]{6}$"));
        if (kPair.match(pair).hasMatch())
            return {pair + QStringLiteral("=X"), {}};
        return {{},
                QCoreApplication::translate("EquityPublicFeed", "%1 is not a currency pair Yahoo Finance quotes.")
                    .arg(sym)};
    }

    if (ex == QLatin1String("NSE") || ex == QLatin1String("BSE")) {
        // Broker series suffixes (Angel / Fyers / Shoonya spell RELIANCE as
        // RELIANCE-EQ). "BAJAJ-AUTO" keeps its dash — only known series go.
        static const QRegularExpression kSeries(QStringLiteral("-(EQ|BE|BZ|BL|SM|ST)$"));
        sym.remove(kSeries);
    }

    if (eqsm_is_us_exchange(ex)) {
        // Class shares: exchanges print BRK.B / BRK/B, Yahoo wants BRK-B.
        sym.replace(QLatin1Char('/'), QLatin1Char('-'));
        static const QRegularExpression kClass(QStringLiteral("^([A-Z]+)\\.([A-Z])$"));
        const auto m = kClass.match(sym);
        if (m.hasMatch())
            sym = m.captured(1) + QLatin1Char('-') + m.captured(2);
        return {sym, {}};
    }

    if (ex == QLatin1String("EURONEXT") && !sym.contains(QLatin1Char('.'))) {
        return {{},
                QCoreApplication::translate(
                    "EquityPublicFeed",
                    "%1 on Euronext: Yahoo Finance needs the market suffix (for example AIR.PA or ASML.AS).")
                    .arg(sym)};
    }

    // NSE -> .NS, BSE -> .BO, LSE -> .L, TSX -> .TO, XETR -> .DE, ... ; an already
    // suffixed ticker is kept; an F&O contract name comes back empty.
    const QString ticker = research_ticker_for(sym, ex);
    if (ticker.isEmpty()) {
        return {{},
                QCoreApplication::translate(
                    "EquityPublicFeed",
                    "%1 is a derivative contract. Yahoo Finance has no public data for it - connect a broker "
                    "in ACCOUNTS to see it.")
                    .arg(sym)};
    }
    return {ticker, {}};
}

inline PublicHistorySpec public_history_spec(const QString& tf) {
    // Yahoo serves 1m bars for the last ~7 days, 5m/15m for ~60 days and 60m for
    // ~730 days; every period below sits inside those windows.
    constexpr qint64 kMin = 60LL * 1000;
    if (tf == QLatin1String("1m"))
        return {QStringLiteral("5d"), QStringLiteral("1m"), kMin, true};
    if (tf == QLatin1String("5m"))
        return {QStringLiteral("1mo"), QStringLiteral("5m"), 5 * kMin, true};
    if (tf == QLatin1String("15m"))
        return {QStringLiteral("1mo"), QStringLiteral("15m"), 15 * kMin, true};
    if (tf == QLatin1String("1h"))
        return {QStringLiteral("3mo"), QStringLiteral("60m"), 60 * kMin, true};
    if (tf == QLatin1String("1w"))
        return {QStringLiteral("5y"), QStringLiteral("1wk"), 7 * 24 * 60 * kMin, false};
    if (tf == QLatin1String("1d"))
        return {QStringLiteral("2y"), QStringLiteral("1d"), 24 * 60 * kMin, false};
    return {QStringLiteral("1y"), QStringLiteral("1d"), 24 * 60 * kMin, false};
}

} // namespace fincept::screens::equity
