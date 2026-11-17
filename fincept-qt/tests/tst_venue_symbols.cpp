// Unit tests for src/core/instrument/VenueSymbols.{h,cpp} — the one venue / provider
// symbol table: Yahoo form both ways, TradingView codes, broker and crypto-exchange
// forms, asset class from syntax, the index / futures / crypto tables, and their
// consistency with core/market/MarketSessions. Qt Core only.
//
// Row-by-row cases come from tests/data/symbol_vectors.json (shared with the backend's
// Python port); this file adds table-wide properties: every covered venue round-trips,
// every Yahoo-less venue yields "", every MarketSessions suffix maps to the same venue.

#include "core/instrument/SymbolInput.h"
#include "core/instrument/VenueSymbols.h"
#include "core/market/MarketSessions.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QTest>

using namespace fincept::core::instrument;
namespace mkt = fincept::core::market;

namespace fincept::core::instrument::venue_symbols_detail {
QStringList table_problems();
} // namespace fincept::core::instrument::venue_symbols_detail

namespace {

QJsonObject tvs_load_vectors() {
    QString path = qEnvironmentVariable("FINCEPT_SYMBOL_VECTORS");
    if (path.isEmpty())
        path = QFINDTESTDATA("data/symbol_vectors.json");
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
        return {};
    return QJsonDocument::fromJson(f.readAll()).object();
}

const QJsonObject& tvs_vectors() {
    static const QJsonObject v = tvs_load_vectors();
    return v;
}

void tvs_rows(const char* section, const char* name_key) {
    QTest::addColumn<QJsonObject>("v");
    int i = 0;
    for (const auto& x : tvs_vectors().value(QLatin1String(section)).toArray()) {
        const QJsonObject v = x.toObject();
        QString n = QString::number(++i) + QLatin1Char(' ') + v.value(QLatin1String(name_key)).toString();
        for (const char* k : {"venue", "exchange", "broker", "class", "quote_type", "country", "style"})
            if (v.contains(QLatin1String(k)))
                n += QLatin1Char(' ') + v.value(QLatin1String(k)).toString();
        QTest::newRow(qPrintable(n)) << v;
    }
}

} // namespace

class TstVenueSymbols : public QObject {
    Q_OBJECT

  private slots:
    void initTestCase();

    void to_yahoo_vectors_data();
    void to_yahoo_vectors();
    void to_yahoo_tv_vectors_data();
    void to_yahoo_tv_vectors();
    void from_yahoo_vectors_data();
    void from_yahoo_vectors();
    void asset_class_vectors_data();
    void asset_class_vectors();
    void broker_vectors_data();
    void broker_vectors();
    void crypto_pair_vectors_data();
    void crypto_pair_vectors();
    void currency_vectors_data();
    void currency_vectors();
    void round_trip_vectors_data();
    void round_trip_vectors();

    void table_is_consistent();
    void marketsessions_suffixes_map_to_the_same_venue();
    void every_covered_venue_round_trips();
    void yahoo_less_venues_yield_nothing();
    void code_spaces_disambiguate();
    void reference_tables();
    void series_and_contracts();
    void broker_index_symbols_superset();
    void marketsessions_unchanged();
};

void TstVenueSymbols::initTestCase() {
    QVERIFY2(!tvs_vectors().isEmpty(), "tests/data/symbol_vectors.json not found (set FINCEPT_SYMBOL_VECTORS)");
    QCOMPARE(tvs_vectors().value("meta").toObject().value("schema").toInt(), 1);
}

// ── Shared vectors ──────────────────────────────────────────────────────────

void TstVenueSymbols::to_yahoo_vectors_data() {
    tvs_rows("to_yahoo", "code");
}

void TstVenueSymbols::to_yahoo_vectors() {
    QFETCH(QJsonObject, v);
    QCOMPARE(to_yahoo(v.value("code").toString(), v.value("venue").toString(),
                      asset_class_from_slug(v.value("class").toString()), v.value("country").toString()),
             v.value("expected").toString());
}

void TstVenueSymbols::to_yahoo_tv_vectors_data() {
    tvs_rows("to_yahoo_tv", "symbol");
}

void TstVenueSymbols::to_yahoo_tv_vectors() {
    QFETCH(QJsonObject, v);
    QCOMPARE(to_yahoo_tv(v.value("symbol").toString(), v.value("exchange").toString(), v.value("country").toString()),
             v.value("expected").toString());
}

void TstVenueSymbols::from_yahoo_vectors_data() {
    tvs_rows("from_yahoo", "symbol");
}

void TstVenueSymbols::from_yahoo_vectors() {
    QFETCH(QJsonObject, v);
    const QString s = v.value("symbol").toString();
    const YahooParts p = from_yahoo(s);
    QCOMPARE(p.valid(), v.value("valid").toBool(true));
    if (v.contains("canonical"))
        QCOMPARE(p.symbol, v.value("canonical").toString());
    if (v.contains("code"))
        QCOMPARE(p.code, v.value("code").toString());
    if (v.contains("market"))
        QCOMPARE(p.market, v.value("market").toString());
    if (v.contains("venue"))
        QCOMPARE(p.venue, v.value("venue").toString());
    if (v.contains("class"))
        QCOMPARE(asset_class_slug(p.cls), v.value("class").toString());
    if (v.contains("id"))
        QCOMPARE(instrument_id_for(s), v.value("id").toString());
    if (v.contains("display"))
        QCOMPARE(display_symbol(s), v.value("display").toString());
}

void TstVenueSymbols::asset_class_vectors_data() {
    tvs_rows("asset_class_of", "symbol");
}

void TstVenueSymbols::asset_class_vectors() {
    QFETCH(QJsonObject, v);
    QCOMPARE(asset_class_slug(asset_class_of(v.value("symbol").toString(), v.value("quote_type").toString())),
             v.value("expected").toString());
}

void TstVenueSymbols::broker_vectors_data() {
    tvs_rows("broker", "symbol");
}

void TstVenueSymbols::broker_vectors() {
    QFETCH(QJsonObject, v);
    Instrument inst = instrument_skeleton(v.value("symbol").toString());
    if (v.contains("venue"))
        inst.venue = v.value("venue").toString();
    if (v.contains("provider"))
        inst.provider_symbols.insert(QStringLiteral("broker:") + v.value("broker").toString(),
                                     v.value("provider").toString());
    QString exchange = QStringLiteral("stale");
    QCOMPARE(to_broker_symbol(inst, v.value("broker").toString(), &exchange), v.value("expected").toString());
    QCOMPARE(exchange, v.value("exchange").toString());
}

void TstVenueSymbols::crypto_pair_vectors_data() {
    tvs_rows("crypto_pair", "symbol");
}

void TstVenueSymbols::crypto_pair_vectors() {
    QFETCH(QJsonObject, v);
    const auto style =
        v.value("style").toString() == QLatin1String("native") ? CryptoPairStyle::Native : CryptoPairStyle::Unified;
    QCOMPARE(crypto_pair(instrument_skeleton(v.value("symbol").toString()), v.value("exchange").toString(), style),
             v.value("expected").toString());
}

void TstVenueSymbols::currency_vectors_data() {
    tvs_rows("currency", "symbol");
}

void TstVenueSymbols::currency_vectors() {
    QFETCH(QJsonObject, v);
    // Pence / cents / agorot / fils listings keep their minor unit.
    QCOMPARE(instrument_skeleton(v.value("symbol").toString()).currency, v.value("expected").toString());
}

void TstVenueSymbols::round_trip_vectors_data() {
    QTest::addColumn<QString>("symbol");
    for (const auto& x : tvs_vectors().value("round_trip").toArray())
        QTest::newRow(qPrintable(x.toString())) << x.toString();
}

void TstVenueSymbols::round_trip_vectors() {
    QFETCH(QString, symbol);
    const YahooParts p = from_yahoo(symbol);
    QVERIFY(p.valid());
    QCOMPARE(p.symbol, symbol);
    QCOMPARE(to_yahoo(p.code, p.market, p.cls), symbol);
    QVERIFY(!instrument_id_for(symbol).isEmpty());
    const Instrument inst = instrument_skeleton(symbol);
    QCOMPARE(inst.symbol, symbol);
    QCOMPARE(inst.id, instrument_id_for(symbol));
    QCOMPARE(inst.asset_class, p.cls);
}

// ── Table-wide properties ───────────────────────────────────────────────────

void TstVenueSymbols::table_is_consistent() {
    const QStringList problems = venue_symbols_detail::table_problems();
    QVERIFY2(problems.isEmpty(), qPrintable(problems.join(QLatin1String("; "))));
    QSet<QString> keys;
    for (const VenueInfo& v : all_venue_infos()) {
        QVERIFY2(!keys.contains(v.key), qPrintable(v.key));
        keys.insert(v.key);
        QVERIFY2(venue_info(v.key) == &v, qPrintable(v.key));
        QVERIFY2(venue_info_for_code(v.key) == &v || v.role == VenueRole::Euronext, qPrintable(v.key));
        if (v.session_model)
            QVERIFY2(mkt::venue_by_code(v.venue) != nullptr, qPrintable(v.key));
        else if (!v.venue.isEmpty() && v.venue != QLatin1String("MCX") && v.venue != QLatin1String("NCDEX") &&
                 v.venue != QLatin1String("EUREX"))
            QVERIFY2(mkt::venue_by_code(v.venue) == nullptr || mkt::venue_by_code(v.venue)->id != v.venue,
                     qPrintable(v.key + QStringLiteral(": listing-only id collides with MarketSessions")));
        if (v.role == VenueRole::Listing && v.yahoo_covered)
            QVERIFY2(!v.suffixes.isEmpty(), qPrintable(v.key));
        if (v.role == VenueRole::UsListing)
            QVERIFY2(v.suffixes.isEmpty(), qPrintable(v.key));
    }
}

void TstVenueSymbols::marketsessions_suffixes_map_to_the_same_venue() {
    for (const mkt::Venue& mv : mkt::all_venues()) {
        for (const QString& s : mv.suffixes) {
            const VenueInfo* v = venue_info_for_suffix(s);
            QVERIFY2(v, qPrintable(s));
            QCOMPARE(v->venue, mv.id);
        }
        // Every MarketSessions venue code resolves to a table entry of that venue.
        const VenueInfo* v = venue_info_for_code(mv.id);
        QVERIFY2(v, qPrintable(mv.id));
        QCOMPARE(v->venue, mv.id);
    }
}

void TstVenueSymbols::every_covered_venue_round_trips() {
    for (const VenueInfo& v : all_venue_infos()) {
        if (!v.yahoo_covered || v.approximate ||
            (v.role != VenueRole::Listing && v.role != VenueRole::UsListing))
            continue;
        for (const char* code : {"ABC", "AB-C", "M&M"}) {
            const QString s = to_yahoo(QString::fromLatin1(code), v.key);
            QVERIFY2(!s.isEmpty(), qPrintable(v.key + QLatin1Char(' ') + QLatin1String(code)));
            const YahooParts p = from_yahoo(s);
            QVERIFY2(p.valid(), qPrintable(s));
            if (v.role == VenueRole::UsListing) {
                // Yahoo's bare form does not say which US exchange: NYSE by convention.
                QCOMPARE(p.market, QStringLiteral("NYSE"));
                QVERIFY(!p.venue_from_suffix);
            } else {
                QCOMPARE(p.market, v.key);
                QCOMPARE(p.venue, v.venue);
            }
            QCOMPARE(to_yahoo(p.code, p.market, p.cls), s);
            QCOMPARE(canonical_symbol(s), s);
        }
    }
    // Every table index, future and crypto asset round-trips too.
    for (const IndexInfo& x : all_indices()) {
        if (x.yahoo.isEmpty())
            continue;
        const YahooParts p = from_yahoo(x.yahoo);
        QCOMPARE(p.cls, AssetClass::Index);
        QCOMPARE(to_yahoo(p.code, p.market, p.cls), x.yahoo);
    }
    for (const FutureInfo& f : all_futures()) {
        const YahooParts p = from_yahoo(f.yahoo);
        QCOMPARE(p.cls, AssetClass::Future);
        QCOMPARE(to_yahoo(p.code, p.market, p.cls), f.yahoo);
    }
    for (const CryptoInfo& c : all_cryptos()) {
        const QString y = c.yahoo_base + QStringLiteral("-USD");
        const YahooParts p = from_yahoo(y);
        QCOMPARE(p.cls, AssetClass::Crypto);
        QCOMPARE(p.base, c.base);
        QCOMPARE(to_yahoo(p.code, p.market, p.cls), y);
    }
}

void TstVenueSymbols::yahoo_less_venues_yield_nothing() {
    for (const VenueInfo& v : all_venue_infos()) {
        if (v.yahoo_covered)
            continue;
        for (const char* code : {"ABC", "RELIANCE", "GOLD", "1234"})
            QVERIFY2(to_yahoo(QString::fromLatin1(code), v.key).isEmpty(), qPrintable(v.key));
    }
    // Unknown venues never give the bare ticker (which would quote the US company).
    for (const char* venue : {"BOGUS", "XXXX", "MCX", "NFO", "PSE", "ADX", "BER", "NGM_NORDIC"})
        QVERIFY2(to_yahoo(QStringLiteral("AAPL"), QString::fromLatin1(venue)).isEmpty(), venue);
    QVERIFY(to_yahoo_tv(QStringLiteral("AAPL"), QStringLiteral("NGM")).isEmpty());
}

void TstVenueSymbols::code_spaces_disambiguate() {
    // "NGM": Nasdaq Global Market for Yahoo / MarketSessions, Nordic Growth Market for TradingView.
    QCOMPARE(venue_info_for_code(QStringLiteral("NGM"))->venue, QStringLiteral("NASDAQ"));
    QCOMPARE(venue_info_for_code(QStringLiteral("NGM"), CodeSpace::Yahoo)->venue, QStringLiteral("NASDAQ"));
    QCOMPARE(venue_info_for_code(QStringLiteral("NGM"), CodeSpace::TradingView)->key, QStringLiteral("NGM_NORDIC"));
    // "CSE": Canada for TradingView / MarketSessions, Colombo in Yahoo's quote.exchange.
    QCOMPARE(venue_info_for_code(QStringLiteral("CSE"))->key, QStringLiteral("CSE"));
    QCOMPARE(venue_info_for_code(QStringLiteral("CSE"), CodeSpace::Yahoo)->key, QStringLiteral("CSE_LK"));
    // KOSDAQ is its own listing market (.KQ) inside the KRX venue.
    QCOMPARE(venue_info_for_code(QStringLiteral("KOSDAQ"))->suffixes.value(0), QStringLiteral("KQ"));
    QCOMPARE(venue_info_for_code(QStringLiteral("KOE"), CodeSpace::Yahoo)->key, QStringLiteral("KOSDAQ"));
    QCOMPARE(venue_info_for_code(QStringLiteral("KRX"))->key, QStringLiteral("KOSPI"));
    // Euronext needs the country.
    QCOMPARE(venue_info_for_code(QStringLiteral("EURONEXT"))->role, VenueRole::Euronext);
    QCOMPARE(venue_info_for_code(QStringLiteral("EURONEXT"), CodeSpace::Any, QStringLiteral("NL"))->key,
             QStringLiteral("AMSTERDAM"));
    QCOMPARE(venue_info_for_code(QStringLiteral("euronext"), CodeSpace::TradingView, QStringLiteral("pt"))->key,
             QStringLiteral("LISBON"));
    // Codes from MarketSessions, Yahoo and brokers.
    QCOMPARE(venue_id_for_code(QStringLiteral("XETR")), QStringLiteral("XETRA"));
    QCOMPARE(venue_id_for_code(QStringLiteral(" nsi ")), QStringLiteral("NSE"));
    QCOMPARE(venue_id_for_code(QStringLiteral("UK")), QStringLiteral("LSE"));
    QCOMPARE(venue_info_for_code(QStringLiteral("NFO"))->role, VenueRole::Derivatives);
    QCOMPARE(venue_info_for_code(QStringLiteral("NSE_INDEX"))->role, VenueRole::IndexNamespace);
    QCOMPARE(venue_info_for_code(QStringLiteral("OANDA"))->role, VenueRole::Fx);
    QCOMPARE(venue_info_for_code(QStringLiteral("BINANCE"))->role, VenueRole::Crypto);
    QCOMPARE(venue_info_for_code(QStringLiteral("COMEX"))->role, VenueRole::Futures);
    QVERIFY(!venue_info_for_code(QStringLiteral("NOSUCH")));
    QVERIFY(!venue_info_for_code(QString()));
    // Suffix lookup ignores a leading dot and case; approximations never own a suffix.
    QCOMPARE(venue_info_for_suffix(QStringLiteral(".ns"))->key, QStringLiteral("NSE"));
    QCOMPARE(venue_info_for_suffix(QStringLiteral("DE"))->key, QStringLiteral("XETRA"));
    QCOMPARE(venue_info_for_suffix(QStringLiteral("IL"))->key, QStringLiteral("LSE_IOB"));
    QVERIFY(!venue_info_for_suffix(QStringLiteral("B")));
}

void TstVenueSymbols::reference_tables() {
    QSet<QString> seen;
    for (const IndexInfo& x : all_indices()) {
        QVERIFY2(!x.name.isEmpty(), qPrintable(x.yahoo));
        QVERIFY2(!x.country.isEmpty(), qPrintable(x.name));
        if (x.yahoo.isEmpty())
            continue;
        QVERIFY2(!seen.contains(x.yahoo), qPrintable(x.yahoo));
        seen.insert(x.yahoo);
        QCOMPARE(index_for_symbol(x.yahoo), &x);
        QCOMPARE(asset_class_of(x.yahoo), AssetClass::Index);
        QCOMPARE(canonical_symbol(x.yahoo), x.yahoo);
        for (const QString& a : x.aliases) {
            bool found = false;
            for (const AliasTarget& t : lookup_alias(a))
                found = found || t.yahoo == x.yahoo;
            QVERIFY2(found, qPrintable(a));
        }
    }
    for (const FutureInfo& f : all_futures()) {
        QCOMPARE(f.yahoo, f.root + QStringLiteral("=F"));
        QCOMPARE(future_for_root(f.root), &f);
        QCOMPARE(to_yahoo(f.root, QStringLiteral("CME")), f.yahoo);
    }
    for (const CryptoInfo& c : all_cryptos()) {
        QCOMPARE(crypto_for_base(c.base), &c);
        QCOMPARE(crypto_for_base(c.yahoo_base), &c);
        QCOMPARE(to_yahoo(c.base + QStringLiteral("USDT"), QStringLiteral("BINANCE")),
                 c.yahoo_base + QStringLiteral("-USD"));
        QCOMPARE(to_yahoo(c.base + QStringLiteral("/USD"), QStringLiteral("KRAKEN")),
                 c.yahoo_base + QStringLiteral("-USD"));
    }
    QVERIFY(is_iso_currency(QStringLiteral("USD")));
    QVERIFY(is_iso_currency(QStringLiteral("CNH")));
    QVERIFY(!is_iso_currency(QStringLiteral("XAU")));
    QVERIFY(!is_iso_currency(QStringLiteral("BTC")));
    QVERIFY(!is_iso_currency(QStringLiteral("usd")));
    QVERIFY(is_stablecoin(QStringLiteral("USDT")));
    QVERIFY(!is_stablecoin(QStringLiteral("USD")));
    QCOMPARE(crypto_quote_currencies().value(0), QStringLiteral("FDUSD"));
    QCOMPARE(alias_key(QStringLiteral("S&P 500")), QStringLiteral("SP500"));
    QCOMPARE(alias_key(QStringLiteral("Nifty_50")), QStringLiteral("NIFTY50"));
    QCOMPARE(alias_key(QStringLiteral("^nsei")), QStringLiteral("NSEI"));
    // "CRUDE" names both oils, WTI first; text-only aliases are flagged.
    const QVector<AliasTarget> crude = lookup_alias(QStringLiteral("crude"));
    QCOMPARE(crude.size(), 2);
    QCOMPARE(crude.at(0).yahoo, QStringLiteral("CL=F"));
    QCOMPARE(crude.at(1).yahoo, QStringLiteral("BZ=F"));
    QVERIFY(lookup_alias(QStringLiteral("DOW")).value(0).text_only);
    QVERIFY(!lookup_alias(QStringLiteral("DJIA")).value(0).text_only);
    QVERIFY(lookup_alias(QStringLiteral("DJT")).isEmpty()); // ^DJT's Yahoo base is not a free-text alias
    QVERIFY(lookup_alias(QStringLiteral("BTC")).isEmpty()); // bare crypto bases go through crypto_for_base
}

void TstVenueSymbols::series_and_contracts() {
    // The one series set (union of the five in the code base).
    for (const char* s : {"-EQ", "-BE", "-BZ", "-BL", "-SM", "-ST", "-MF", "-SG"})
        QCOMPARE(strip_series_suffix(QStringLiteral("RELIANCE") + QLatin1String(s)), QStringLiteral("RELIANCE"));
    QCOMPARE(strip_series_suffix(QStringLiteral("bajaj-auto-eq")), QStringLiteral("BAJAJ-AUTO"));
    QCOMPARE(strip_series_suffix(QStringLiteral("BAJAJ-AUTO")), QStringLiteral("BAJAJ-AUTO"));
    QCOMPARE(strip_series_suffix(QStringLiteral("MCDOWELL-N")), QStringLiteral("MCDOWELL-N"));
    QCOMPARE(strip_series_suffix(QStringLiteral("-EQ")), QStringLiteral("-EQ"));
    for (const char* c : {"NIFTY2660923200CE", "BANKNIFTY26JUL50000PE", "RELIANCE26JULFUT", "CRUDEOIL26JULFUT",
                          "SENSEX26JUL80000PE", "NIFTY26JUN2524000CE"})
        QVERIFY2(is_exchange_contract_name(QString::fromLatin1(c)), c);
    for (const char* c : {"RELIANCE", "NIFTYBEES", "PRICE", "SPACE", "INFY", "AAPL250117C00150000"})
        QVERIFY2(!is_exchange_contract_name(QString::fromLatin1(c)), c);
}

void TstVenueSymbols::broker_index_symbols_superset() {
    // Every entry of trading/instruments/InstrumentNormalize.cpp's two index maps, and the
    // Angel-only copy in InstrumentService.cpp ("BSE SENSEX" now SENSEX everywhere).
    const struct {
        const char* raw;
        const char* expected;
    } rows[] = {
        {"NIFTY 50", "NIFTY"},
        {"NIFTY BANK", "BANKNIFTY"},
        {"NIFTY FIN SERVICE", "FINNIFTY"},
        {"NIFTY MID SELECT", "MIDCPNIFTY"},
        {"NIFTY MIDCAP SELECT", "MIDCPNIFTY"},
        {"NIFTY NEXT 50", "NIFTYNXT50"},
        {"INDIA VIX", "INDIAVIX"},
        {"NIFTY100 LIQ 15", "NIFTY100LIQ15"},
        {"NIFTY MIDCAP 50", "NIFTYMIDCAP50"},
        {"NIFTY MIDCAP 100", "NIFTYMIDCAP100"},
        {"NIFTY SMALLCAP 50", "NIFTYSMALLCAP50"},
        {"NIFTY SMALLCAP 100", "NIFTYSMALLCAP100"},
        {"NIFTY 100", "NIFTY100"},
        {"NIFTY 200", "NIFTY200"},
        {"NIFTY 500", "NIFTY500"},
        {"NIFTY AUTO", "NIFTYAUTO"},
        {"NIFTY ENERGY", "NIFTYENERGY"},
        {"NIFTY FMCG", "NIFTYFMCG"},
        {"NIFTY IT", "NIFTYIT"},
        {"NIFTY MEDIA", "NIFTYMEDIA"},
        {"NIFTY METAL", "NIFTYMETAL"},
        {"NIFTY PHARMA", "NIFTYPHARMA"},
        {"NIFTY PSU BANK", "NIFTYPSUBANK"},
        {"NIFTY REALTY", "NIFTYREALTY"},
        {"NIFTY INFRA", "NIFTYINFRA"},
        {"NIFTY SERV SECTOR", "NIFTYSERVSECTOR"},
        {"NIFTY COMMODITIES", "NIFTYCOMMODITIES"},
        {"NIFTY CONSUMPTION", "NIFTYCONSUMPTION"},
        {"NIFTY CPSE", "NIFTYCPSE"},
        {"NIFTY GROWSECT 15", "NIFTYGROWSECT15"},
        {"NIFTY100 QUALTY30", "NIFTY100QUALTY30"},
        {"NIFTY50 VALUE 20", "NIFTY50VALUE20"},
        {"NIFTY ALPHA 50", "NIFTYALPHA50"},
        {"NIFTY ALPHA LOW-VOL", "NIFTYALPHALOWVOL"},
        {"NIFTY200 QUALTY30", "NIFTY200QUALTY30"},
        {"SENSEX", "SENSEX"},
        {"BSE SENSEX", "SENSEX"},
        {"S&P BSE SENSEX", "SENSEX"},
        {"SENSEX 50", "SENSEX50"},
        {"SNSX50", "SENSEX50"},
        {"BANKEX", "BANKEX"},
        {"BSE BANKEX", "BANKEX"},
        {"NIFTY50", "NIFTY"},
        {"NIFTYBANK", "BANKNIFTY"},
        {"NIFTYFINSERVICE", "FINNIFTY"},
        {"NIFTYNEXT50", "NIFTYNXT50"},
        {"NIFTYMIDSELECT", "MIDCPNIFTY"},
        {"NIFTYMIDCAPSELECT", "MIDCPNIFTY"},
        {"NIFTYMCAP50", "NIFTYMIDCAP50"},
        {"BSESENSEX", "SENSEX"},
        {"BSEBANKEX", "BANKEX"},
        {"Nifty Bank", "BANKNIFTY"},
        {"NIFTY TOTAL MKT", "NIFTYTOTALMKT"}, // unknown: upper case, spaces / dashes removed
        {"S&P SOMETHING-NEW", "SOMETHINGNEW"},
    };
    for (const auto& r : rows)
        QCOMPARE(broker_index_symbol(QString::fromUtf8(r.raw)), QString::fromLatin1(r.expected));
}

void TstVenueSymbols::marketsessions_unchanged() {
    // This package adds no data to MarketSessions; its contract still holds.
    QVERIFY(!mkt::venue_by_code(QStringLiteral("MCX")));
    QCOMPARE(mkt::venue_for_symbol(QStringLiteral("RELIANCE.NS"))->id, QStringLiteral("NSE"));
    QCOMPARE(mkt::venue_for_symbol(QStringLiteral("AAPL"))->id, QStringLiteral("NYSE"));
    QVERIFY(!mkt::venue_by_code(QStringLiteral("ATHEX"))); // listing-only ids live here, not there
}

QTEST_GUILESS_MAIN(TstVenueSymbols)
#include "tst_venue_symbols.moc"
