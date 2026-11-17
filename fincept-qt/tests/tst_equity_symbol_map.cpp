// Unit tests for src/screens/equity_trading/EquitySymbolMap.h (header-only, Qt
// Core only): the Equity Trading screen's broker symbol -> Yahoo Finance ticker
// mapping and chart timeframe -> Yahoo history request table, used by the
// public-data fallback (EquityPublicFeed) and the Equity Research hand-off.
//
// A wrong mapping is silent in the app — Yahoo just answers "no data", or worse,
// quotes a different instrument — so every venue family is pinned here.

#include "screens/equity_trading/EquitySymbolMap.h"

#include <QString>
#include <QTest>

#include <utility>

using fincept::screens::equity::public_history_spec;
using fincept::screens::equity::public_yahoo_ticker;
using fincept::screens::equity::research_ticker_for;

namespace {
QString ticker(const char* symbol, const char* exchange) {
    return public_yahoo_ticker(QString::fromUtf8(symbol), QString::fromUtf8(exchange)).ticker;
}
} // namespace

class TstEquitySymbolMap : public QObject {
    Q_OBJECT

  private slots:
    void nse_bse_equities_data();
    void nse_bse_equities();
    void indian_indices_data();
    void indian_indices();
    void unknown_indian_index_is_unsupported();
    void us_tickers_data();
    void us_tickers();
    void other_markets_data();
    void other_markets();
    void fx_pairs_data();
    void fx_pairs();
    void already_yahoo_tickers_pass_through();
    void derivatives_exchanges_are_unsupported_data();
    void derivatives_exchanges_are_unsupported();
    void fno_contract_names_are_unsupported_on_cash_exchanges();
    void euronext_needs_a_market_suffix();
    void empty_symbol();
    void research_ticker_for_basics();
    void history_spec_table_data();
    void history_spec_table();
};

// ── NSE / BSE equities ───────────────────────────────────────────────────────

void TstEquitySymbolMap::nse_bse_equities_data() {
    QTest::addColumn<QString>("symbol");
    QTest::addColumn<QString>("exchange");
    QTest::addColumn<QString>("expected");

    QTest::newRow("NSE plain") << "RELIANCE" << "NSE" << "RELIANCE.NS";
    QTest::newRow("BSE plain") << "RELIANCE" << "BSE" << "RELIANCE.BO";
    QTest::newRow("-EQ series") << "RELIANCE-EQ" << "NSE" << "RELIANCE.NS";
    QTest::newRow("-BE series") << "RELIANCE-BE" << "NSE" << "RELIANCE.NS";
    QTest::newRow("-BL series on BSE") << "SBIN-BL" << "BSE" << "SBIN.BO";
    QTest::newRow("dash in the name") << "BAJAJ-AUTO" << "NSE" << "BAJAJ-AUTO.NS";
    QTest::newRow("dash in the name + -EQ") << "BAJAJ-AUTO-EQ" << "NSE" << "BAJAJ-AUTO.NS";
    QTest::newRow("ampersand") << "M&M" << "NSE" << "M&M.NS";
    QTest::newRow("ampersand + -EQ") << "M&M-EQ" << "NSE" << "M&M.NS";
    QTest::newRow("ampersand mid-name") << "L&TFH" << "BSE" << "L&TFH.BO";
    QTest::newRow("lower case + padding") << "  infy " << "nse" << "INFY.NS";
    QTest::newRow("Fyers prefix names the exchange") << "NSE:RELIANCE-EQ" << "" << "RELIANCE.NS";
    QTest::newRow("Fyers BSE prefix") << "BSE:TCS-EQ" << "" << "TCS.BO";
    // An explicit exchange wins over the prefix (pinned so a change is deliberate).
    QTest::newRow("explicit exchange wins") << "NSE:RELIANCE-EQ" << "BSE" << "RELIANCE.BO";
    QTest::newRow("ETF named like an index") << "NIFTYBEES" << "NSE" << "NIFTYBEES.NS";
}

void TstEquitySymbolMap::nse_bse_equities() {
    QFETCH(QString, symbol);
    QFETCH(QString, exchange);
    QFETCH(QString, expected);
    const auto y = public_yahoo_ticker(symbol, exchange);
    QVERIFY2(y.ok(), qPrintable(y.unsupported_reason));
    QCOMPARE(y.ticker, expected);
}

// ── Indian indices ───────────────────────────────────────────────────────────

void TstEquitySymbolMap::indian_indices_data() {
    QTest::addColumn<QString>("symbol");
    QTest::addColumn<QString>("exchange");
    QTest::addColumn<QString>("expected");

    QTest::newRow("NIFTY 50 / NSE") << "NIFTY 50" << "NSE" << "^NSEI";
    QTest::newRow("NIFTY / NSE") << "NIFTY" << "NSE" << "^NSEI";
    QTest::newRow("Nifty 50 mixed case") << "Nifty 50" << "NSE" << "^NSEI";
    QTest::newRow("NIFTY_50 underscore") << "NIFTY_50" << "INDICES" << "^NSEI";
    QTest::newRow("no exchange") << "NIFTY 50" << "" << "^NSEI";
    QTest::newRow("BANKNIFTY") << "BANKNIFTY" << "NSE" << "^NSEBANK";
    QTest::newRow("NIFTY BANK") << "NIFTY BANK" << "NSE" << "^NSEBANK";
    QTest::newRow("FINNIFTY") << "FINNIFTY" << "NSE" << "NIFTY_FIN_SERVICE.NS";
    QTest::newRow("NIFTY FIN SERVICE") << "NIFTY FIN SERVICE" << "NSE" << "NIFTY_FIN_SERVICE.NS";
    QTest::newRow("MIDCPNIFTY") << "MIDCPNIFTY" << "NSE" << "NIFTY_MID_SELECT.NS";
    QTest::newRow("NIFTY MID SELECT") << "NIFTY MID SELECT" << "NSE" << "NIFTY_MID_SELECT.NS";
    QTest::newRow("NIFTY NEXT 50") << "NIFTY NEXT 50" << "NSE" << "^NSMIDCP";
    QTest::newRow("NIFTY IT") << "NIFTY IT" << "NSE" << "^CNXIT";
    QTest::newRow("INDIA VIX") << "INDIA VIX" << "NSE" << "^INDIAVIX";
    QTest::newRow("SENSEX / BSE") << "SENSEX" << "BSE" << "^BSESN";
    QTest::newRow("BSE SENSEX") << "BSE SENSEX" << "BSE" << "^BSESN";
    // The instrument master's own index exchanges (Upstox, Dhan, Kotak, 5paisa, ...).
    // These used to fall through to the bare name ("NIFTY 50") as a Yahoo ticker.
    QTest::newRow("NSE_INDEX venue") << "NIFTY 50" << "NSE_INDEX" << "^NSEI";
    QTest::newRow("NSE_INDEX bank") << "Nifty Bank" << "NSE_INDEX" << "^NSEBANK";
    QTest::newRow("BSE_INDEX venue") << "SENSEX" << "BSE_INDEX" << "^BSESN";
    QTest::newRow("NSE_INDEX vix") << "INDIA VIX" << "NSE_INDEX" << "^INDIAVIX";
    // Fyers spells indices with an -INDEX suffix (used to become "NIFTY50-INDEX.NS").
    QTest::newRow("Fyers NIFTY50-INDEX") << "NSE:NIFTY50-INDEX" << "" << "^NSEI";
    QTest::newRow("Fyers NIFTYBANK-INDEX") << "NSE:NIFTYBANK-INDEX" << "" << "^NSEBANK";
    QTest::newRow("Fyers SENSEX-INDEX") << "BSE:SENSEX-INDEX" << "" << "^BSESN";
    QTest::newRow("Fyers FINNIFTY-INDEX") << "NSE:FINNIFTY-INDEX" << "" << "NIFTY_FIN_SERVICE.NS";
}

void TstEquitySymbolMap::indian_indices() {
    QFETCH(QString, symbol);
    QFETCH(QString, exchange);
    QFETCH(QString, expected);
    const auto y = public_yahoo_ticker(symbol, exchange);
    QVERIFY2(y.ok(), qPrintable(y.unsupported_reason));
    QCOMPARE(y.ticker, expected);
}

void TstEquitySymbolMap::unknown_indian_index_is_unsupported() {
    // Never hand an index's display name to Yahoo as a ticker.
    for (const auto& [sym, ex] : {std::pair{"NIFTY AUTO", "NSE_INDEX"}, std::pair{"BANKEX", "BSE_INDEX"},
                                  std::pair{"NSE:NIFTYAUTO-INDEX", ""}}) {
        const auto y = public_yahoo_ticker(QString::fromUtf8(sym), QString::fromUtf8(ex));
        QVERIFY2(!y.ok(), qPrintable(QString::fromUtf8(sym) + QStringLiteral(" -> ") + y.ticker));
        QVERIFY(y.unsupported_reason.contains(QLatin1String("index")));
    }
}

// ── US ───────────────────────────────────────────────────────────────────────

void TstEquitySymbolMap::us_tickers_data() {
    QTest::addColumn<QString>("symbol");
    QTest::addColumn<QString>("exchange");
    QTest::addColumn<QString>("expected");

    QTest::newRow("NASDAQ") << "AAPL" << "NASDAQ" << "AAPL";
    QTest::newRow("NYSE") << "JPM" << "NYSE" << "JPM";
    QTest::newRow("ARCA ETF") << "SPY" << "ARCA" << "SPY";
    QTest::newRow("XNAS mic") << "MSFT" << "XNAS" << "MSFT";
    QTest::newRow("lower case") << "nvda" << "nasdaq" << "NVDA";
    QTest::newRow("class share dot") << "BRK.B" << "NYSE" << "BRK-B";
    QTest::newRow("class share slash") << "BRK/B" << "NYSE" << "BRK-B";
    QTest::newRow("class share lower") << "brk.b" << "nyse" << "BRK-B";
    QTest::newRow("class share BF.B") << "BF.B" << "NYSE" << "BF-B";
    QTest::newRow("Yahoo form kept") << "BRK-B" << "NYSE" << "BRK-B";
    // Only a single-letter class is rewritten (pinned).
    QTest::newRow("multi-letter suffix kept") << "ABC.WS" << "NYSE" << "ABC.WS";
    QTest::newRow("S&P 500 index") << "SPX" << "CBOE" << "^GSPC";
    QTest::newRow("VIX index") << "VIX" << "CBOE" << "^VIX";
    QTest::newRow("Dow index, no exchange") << "DJI" << "" << "^DJI";
}

void TstEquitySymbolMap::us_tickers() {
    QFETCH(QString, symbol);
    QFETCH(QString, exchange);
    QFETCH(QString, expected);
    const auto y = public_yahoo_ticker(symbol, exchange);
    QVERIFY2(y.ok(), qPrintable(y.unsupported_reason));
    QCOMPARE(y.ticker, expected);
}

// ── LSE / TSX / XETRA and other suffixed markets ─────────────────────────────

void TstEquitySymbolMap::other_markets_data() {
    QTest::addColumn<QString>("symbol");
    QTest::addColumn<QString>("exchange");
    QTest::addColumn<QString>("expected");

    QTest::newRow("LSE") << "VOD" << "LSE" << "VOD.L";
    QTest::newRow("LSE already suffixed") << "VOD.L" << "LSE" << "VOD.L";
    QTest::newRow("TSX") << "SHOP" << "TSX" << "SHOP.TO";
    QTest::newRow("XETRA") << "SAP" << "XETRA" << "SAP.DE";
    QTest::newRow("XETR mic") << "SAP" << "XETR" << "SAP.DE";
    QTest::newRow("HKEX") << "0700" << "HKEX" << "0700.HK";
    QTest::newRow("ASX") << "BHP" << "ASX" << "BHP.AX";
    QTest::newRow("Euronext with suffix") << "AIR.PA" << "EURONEXT" << "AIR.PA";
}

void TstEquitySymbolMap::other_markets() {
    QFETCH(QString, symbol);
    QFETCH(QString, exchange);
    QFETCH(QString, expected);
    const auto y = public_yahoo_ticker(symbol, exchange);
    QVERIFY2(y.ok(), qPrintable(y.unsupported_reason));
    QCOMPARE(y.ticker, expected);
}

// ── FX ───────────────────────────────────────────────────────────────────────

void TstEquitySymbolMap::fx_pairs_data() {
    QTest::addColumn<QString>("symbol");
    QTest::addColumn<QString>("expected"); // empty = unsupported

    QTest::newRow("EURUSD") << "EURUSD" << "EURUSD=X";
    QTest::newRow("EUR/USD") << "EUR/USD" << "EURUSD=X";
    QTest::newRow("lower case") << "usdinr" << "USDINR=X";
    QTest::newRow("not six letters") << "EURO" << "";
    QTest::newRow("digits") << "EUR123" << "";
}

void TstEquitySymbolMap::fx_pairs() {
    QFETCH(QString, symbol);
    QFETCH(QString, expected);
    const auto y = public_yahoo_ticker(symbol, QStringLiteral("FOREX"));
    QCOMPARE(y.ticker, expected);
    if (expected.isEmpty())
        QVERIFY(y.unsupported_reason.contains(QLatin1String("currency pair")));
}

void TstEquitySymbolMap::already_yahoo_tickers_pass_through() {
    QCOMPARE(ticker("^NSEI", "NSE"), QStringLiteral("^NSEI"));
    QCOMPARE(ticker("^GSPC", ""), QStringLiteral("^GSPC"));
    QCOMPARE(ticker("EURUSD=X", ""), QStringLiteral("EURUSD=X"));
    QCOMPARE(ticker("GC=F", "MCX"), QStringLiteral("GC=F")); // even on a contract venue
}

// ── Unsupported ──────────────────────────────────────────────────────────────

void TstEquitySymbolMap::derivatives_exchanges_are_unsupported_data() {
    QTest::addColumn<QString>("symbol");
    QTest::addColumn<QString>("exchange");

    QTest::newRow("NFO option") << "NIFTY2660923200CE" << "NFO";
    QTest::newRow("NFO future") << "RELIANCE26JULFUT" << "NFO";
    QTest::newRow("BFO") << "SENSEX26JUL80000PE" << "BFO";
    QTest::newRow("MCX") << "CRUDEOIL26JULFUT" << "MCX";
    QTest::newRow("MCX lower case") << "GOLDM26AUGFUT" << "mcx";
    QTest::newRow("CDS") << "USDINR26JULFUT" << "CDS";
    QTest::newRow("NCDEX") << "GUARSEED26JULFUT" << "NCDEX";
    QTest::newRow("BCD") << "EURINR26JULFUT" << "BCD";
    QTest::newRow("MCX_INDEX") << "MCXCOMPDEX" << "MCX_INDEX";
    // A name that looks like an equity is still a contract on these venues.
    QTest::newRow("equity-looking name on NFO") << "RELIANCE" << "NFO";
}

void TstEquitySymbolMap::derivatives_exchanges_are_unsupported() {
    QFETCH(QString, symbol);
    QFETCH(QString, exchange);
    const auto y = public_yahoo_ticker(symbol, exchange);
    QVERIFY2(!y.ok(), qPrintable(y.ticker));
    QVERIFY(y.unsupported_reason.contains(exchange.trimmed().toUpper()));
    QVERIFY(y.unsupported_reason.contains(QLatin1String("ACCOUNTS")));
}

void TstEquitySymbolMap::fno_contract_names_are_unsupported_on_cash_exchanges() {
    // A contract that reached the watchlist under NSE / BSE (some brokers do that).
    for (const char* sym : {"NIFTY2660923200CE", "BANKNIFTY26JUL50000PE", "RELIANCE26JULFUT"}) {
        const auto y = public_yahoo_ticker(QString::fromLatin1(sym), QStringLiteral("NSE"));
        QVERIFY2(!y.ok(), sym);
        QVERIFY(y.unsupported_reason.contains(QLatin1String("derivative contract")));
    }
}

void TstEquitySymbolMap::euronext_needs_a_market_suffix() {
    const auto y = public_yahoo_ticker(QStringLiteral("AIR"), QStringLiteral("EURONEXT"));
    QVERIFY(!y.ok());
    QVERIFY(y.unsupported_reason.contains(QLatin1String("AIR.PA")));
}

void TstEquitySymbolMap::empty_symbol() {
    for (const char* s : {"", "   ", "NSE:"}) {
        const auto y = public_yahoo_ticker(QString::fromLatin1(s), QStringLiteral("NSE"));
        QVERIFY(!y.ok());
        QVERIFY(!y.unsupported_reason.isEmpty());
    }
}

void TstEquitySymbolMap::research_ticker_for_basics() {
    QCOMPARE(research_ticker_for(QStringLiteral("RELIANCE"), QStringLiteral("NSE")), QStringLiteral("RELIANCE.NS"));
    QCOMPARE(research_ticker_for(QStringLiteral("tcs"), QStringLiteral(" bse ")), QStringLiteral("TCS.BO"));
    QCOMPARE(research_ticker_for(QStringLiteral("AAPL"), QStringLiteral("NASDAQ")), QStringLiteral("AAPL"));
    QCOMPARE(research_ticker_for(QStringLiteral("VOD.L"), QStringLiteral("LSE")), QStringLiteral("VOD.L"));
    QVERIFY(research_ticker_for(QStringLiteral("NIFTY2660923200CE"), QStringLiteral("NFO")).isEmpty());
    QVERIFY(research_ticker_for(QStringLiteral("RELIANCE26JULFUT"), QStringLiteral("NSE")).isEmpty());
    QVERIFY(research_ticker_for(QString(), QStringLiteral("NSE")).isEmpty());
}

// ── Chart timeframe -> Yahoo history request ─────────────────────────────────

void TstEquitySymbolMap::history_spec_table_data() {
    QTest::addColumn<QString>("timeframe");
    QTest::addColumn<QString>("period");
    QTest::addColumn<QString>("interval");
    QTest::addColumn<qint64>("bar_ms");
    QTest::addColumn<bool>("intraday");

    constexpr qint64 kMin = 60LL * 1000;
    // Yahoo serves 1m bars for ~7 days, 5m/15m for ~60 days, 60m for ~730 days:
    // every intraday period must stay inside its window or Yahoo returns nothing.
    QTest::newRow("1m") << "1m" << "5d" << "1m" << kMin << true;
    QTest::newRow("5m") << "5m" << "1mo" << "5m" << 5 * kMin << true;
    QTest::newRow("15m") << "15m" << "1mo" << "15m" << 15 * kMin << true;
    QTest::newRow("1h") << "1h" << "3mo" << "60m" << 60 * kMin << true;
    QTest::newRow("1d") << "1d" << "2y" << "1d" << 24 * 60 * kMin << false;
    QTest::newRow("1w") << "1w" << "5y" << "1wk" << 7 * 24 * 60 * kMin << false;
    // Anything else falls back to a year of daily bars.
    QTest::newRow("unknown 30m") << "30m" << "1y" << "1d" << 24 * 60 * kMin << false;
    QTest::newRow("empty") << "" << "1y" << "1d" << 24 * 60 * kMin << false;
}

void TstEquitySymbolMap::history_spec_table() {
    QFETCH(QString, timeframe);
    QFETCH(QString, period);
    QFETCH(QString, interval);
    QFETCH(qint64, bar_ms);
    QFETCH(bool, intraday);
    const auto spec = public_history_spec(timeframe);
    QCOMPARE(spec.period, period);
    QCOMPARE(spec.interval, interval);
    QCOMPARE(spec.bar_ms, bar_ms);
    QCOMPARE(spec.intraday, intraday);
}

QTEST_GUILESS_MAIN(TstEquitySymbolMap)
#include "tst_equity_symbol_map.moc"
