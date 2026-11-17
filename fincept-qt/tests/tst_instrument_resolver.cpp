// Unit tests for the symbol system's resolver service (WP3):
//   services/instruments/InstrumentResolver.{h,cpp}   merge / de-dupe / ranking / exactness,
//                                                      stale requests, rate limits, offline,
//                                                      resolve_stored, lists, remote-row mapping
//   services/instruments/InstrumentResolverIndex.cpp  the InstrumentIndex-backed local source
//   services/instruments/InstrumentSync.{h,cpp}       the /symbols/changes loop (404, pages, 410)
// The local source is the REAL InstrumentIndex over a temp-file database (seeded with the
// bundled seed); the backend / Yahoo / validation sources are fakes answering
// asynchronously, so every case runs offline and deterministically.

#include "core/instrument/Instrument.h"
#include "core/instrument/InstrumentRank.h"
#include "core/instrument/SymbolInput.h"
#include "core/instrument/VenueSymbols.h"
#include "services/instruments/InstrumentIndex.h"
#include "services/instruments/InstrumentResolver.h"
#include "services/instruments/InstrumentSync.h"
#include "storage/repositories/SymbolMasterRepository.h"
#include "storage/sqlite/Database.h"
#include "storage/sqlite/migrations/MigrationRunner.h"

#include <QCoreApplication>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include <QTimer>

#include <functional>
#include <memory>

using namespace fincept::core::instrument;
using fincept::services::InstrumentIndex;
using fincept::services::InstrumentResolver;
using fincept::services::InstrumentSync;
using fincept::services::ResolveOptions;
using fincept::services::ResolveResult;
using fincept::services::ResolverRemoteReply;
using fincept::services::ResolverSources;

namespace {

/// A /market/search row as the live backend sends it.
QJsonObject tir_row(const char* exchange, const char* symbol, const char* name, const char* type,
                    const char* subtype = "common", const char* currency = "", const char* country = "") {
    return QJsonObject{{"exchange", exchange}, {"symbol", symbol}, {"name", name}, {"type", type},
                       {"subtype", subtype},   {"currency", currency}, {"country", country}};
}

/// A `yfinance_data.py search` row.
QJsonObject tir_yrow(const char* symbol, const char* name, const char* exchange, const char* type) {
    return QJsonObject{{"symbol", symbol}, {"name", name}, {"exchange", exchange}, {"type", type}, {"currency", "USD"}};
}

struct TirFake {
    QHash<QString, QJsonArray> backend;  ///< upper-cased query -> rows
    QHash<QString, QJsonArray> yahoo;    ///< upper-cased query -> rows
    QHash<QString, QJsonObject> validate; ///< upper-cased text -> confirmed Yahoo row
    bool backend_unreachable = false;
    bool yahoo_unreachable = false;
    int backend_delay_ms = 20;
    int yahoo_delay_ms = 20;
    QStringList backend_calls;
    QStringList yahoo_calls;
    QStringList validate_calls;
    int backend_in_flight = 0;
    int backend_max_in_flight = 0;
    QHash<QString, QStringList> recent;
};

ResolverSources tir_sources(const std::shared_ptr<TirFake>& f) {
    ResolverSources s = fincept::services::index_resolver_sources();
    s.debounce_ms = 30;
    s.backend_search = [f](const QString& q, int, QObject* ctx, ResolverSources::RemoteDone done) {
        f->backend_calls << q;
        f->backend_max_in_flight = std::max(f->backend_max_in_flight, ++f->backend_in_flight);
        QTimer::singleShot(f->backend_delay_ms, ctx, [f, q, done]() {
            --f->backend_in_flight;
            ResolverRemoteReply rep;
            if (f->backend_unreachable) {
                rep.unreachable = true;
                rep.error = QStringLiteral("HTTP_0: connection refused");
            } else {
                rep.ok = true;
                rep.rows = fincept::services::instruments_from_market_search(
                    QJsonDocument(QJsonObject{{"results", f->backend.value(q.toUpper())}}));
            }
            done(rep);
        });
    };
    s.yahoo_search = [f](const QString& q, int, QObject* ctx, ResolverSources::RemoteDone done) {
        f->yahoo_calls << q;
        QTimer::singleShot(f->yahoo_delay_ms, ctx, [f, q, done]() {
            ResolverRemoteReply rep;
            if (f->yahoo_unreachable) {
                rep.unreachable = true;
            } else {
                rep.ok = true;
                const QString isin = is_valid_isin(q.toUpper()) ? q.toUpper() : QString();
                rep.rows = fincept::services::instruments_from_yahoo_search(
                    QJsonObject{{"results", f->yahoo.value(q.toUpper())}}, isin);
            }
            done(rep);
        });
    };
    s.yahoo_validate = [f](const QString& q, QObject* ctx, ResolverSources::RemoteDone done) {
        f->validate_calls << q;
        QTimer::singleShot(10, ctx, [f, q, done]() {
            ResolverRemoteReply rep;
            if (f->yahoo_unreachable) {
                rep.unreachable = true;
            } else {
                rep.ok = true;
                const QJsonObject row = f->validate.value(q.toUpper());
                if (!row.isEmpty()) {
                    rep.rows << fincept::services::instrument_from_yahoo_row(row, 0);
                    rep.first_confirmed = true;
                }
            }
            done(rep);
        });
    };
    s.default_home_country = []() { return QString(); };
    s.load_recent = [f](const QString& key) { return f->recent.value(key); };
    s.save_recent = [f](const QString& key, const QStringList& v) { f->recent.insert(key, v); };
    return s;
}

/// Run one resolver call, collect every callback, return the final one (empty query on timeout).
ResolveResult tir_wait(const std::function<quint64(InstrumentResolver::Callback)>& start,
                       QVector<ResolveResult>* all = nullptr, int timeout_ms = 10000) {
    auto calls = std::make_shared<QVector<ResolveResult>>();
    start([calls](const ResolveResult& r) { calls->push_back(r); });
    if (!QTest::qWaitFor([calls]() { return !calls->isEmpty() && calls->last().final; }, timeout_ms))
        qWarning("resolver call timed out");
    if (all)
        *all = *calls;
    return calls->isEmpty() ? ResolveResult{} : calls->last();
}

QStringList tir_symbols(const ResolveResult& r, int n = 50) {
    QStringList out;
    for (const Instrument& i : r.matches.mid(0, n))
        out << (i.symbol.isEmpty() ? QStringLiteral("<") + i.id + QLatin1Char('>') : i.symbol);
    return out;
}

ResolveOptions tir_opts(const char* home = "", int limit = 12) {
    ResolveOptions o;
    o.home_country = QString::fromLatin1(home);
    o.limit = limit;
    return o;
}

} // namespace

class TstInstrumentResolver : public QObject {
    Q_OBJECT

  private slots:
    void initTestCase();

    void map_market_search_rows();
    void map_yahoo_rows();
    void trading_meta();

    void owner_critical_resolve_data();
    void owner_critical_resolve();
    void search_merges_and_dedupes();
    void ambiguous_tickers_are_not_exact();
    void class_filter();
    void yahoo_fallback_and_learning();
    void validation_confirms_a_symbol();
    void stale_requests_are_dropped();
    void one_backend_request_in_flight_per_context();
    void cache_and_cancel();
    void offline_keeps_local_results();
    void invalid_and_empty_text();
    void resolve_stored_never_guesses();
    void resolve_many_keeps_order();
    void recent_picks();

    void sync_404_disables_quietly();
    void sync_applies_pages_through_the_index();
    void sync_410_refetches_from_start();

    void cleanupTestCase();

  private:
    QTemporaryDir dir_;
    std::shared_ptr<TirFake> fake_;
    std::unique_ptr<InstrumentResolver> resolver_;
};

void TstInstrumentResolver::initTestCase() {
    QCoreApplication::setOrganizationName(QStringLiteral("FinceptTests"));
    QCoreApplication::setApplicationName(QStringLiteral("tst_instrument_resolver"));
    fincept::register_migration_v060(); // the app registers every migration in main(); this suite needs v060 only
    QVERIFY(dir_.isValid());
    auto& database = fincept::Database::instance();
    const auto opened = database.open(dir_.filePath(QStringLiteral("fincept.db")));
    QVERIFY2(opened.is_ok(), opened.is_ok() ? "" : opened.error().c_str());
    InstrumentIndex& index = InstrumentIndex::instance();
    QSignalSpy seeded(&index, &InstrumentIndex::seed_applied);
    QSignalSpy failed(&index, &InstrumentIndex::index_error);
    index.start();
    if (!QTest::qWaitFor([&]() { return seeded.count() > 0 || failed.count() > 0; }, 30000))
        qWarning("the index neither seeded nor failed within 30 s");
    QVERIFY2(failed.isEmpty(), failed.isEmpty() ? "" : qPrintable(failed.first().first().toString()));
    QCOMPARE(seeded.count(), 1);

    fake_ = std::make_shared<TirFake>();
    auto& b = fake_->backend;
    b.insert("RELIANCE", QJsonArray{tir_row("NSE", "RELIANCE", "Reliance Industries Limited", "stock", "common", "INR", "IN"),
                                    tir_row("BSE", "RELIANCE", "Reliance Industries Limited", "stock", "common", "INR", "IN"),
                                    tir_row("NSE", "RELIANCE1!", "RELIANCE INDS FUTURES", "futures", "continuous", "INR", "IN"),
                                    tir_row("DSEBD", "RELIANCE1", "Reliance Insurance Mutual Fund", "fund", "closedend", "BDT", "BD")});
    b.insert("TCS", QJsonArray{tir_row("NSE", "TCS", "Tata Consultancy Services Limited", "stock", "common", "INR", "IN"),
                               tir_row("BSE", "TCS", "Tata Consultancy Services Limited", "stock", "common", "INR", "IN"),
                               tir_row("TSX", "TCS", "TECSYS Inc.", "stock", "common", "CAD", "CA")});
    b.insert("SAP", QJsonArray{tir_row("TSX", "SAP", "Saputo Inc.", "stock", "common", "CAD", "CA"),
                               tir_row("XETR", "SAP", "SAP SE", "stock", "common", "EUR", "DE"),
                               tir_row("NYSE", "SAP", "SAP SE", "dr", "", "USD", "US")});
    b.insert("BTC", QJsonArray{tir_row("SET", "BTC", "Brooker Group Public Company Limited", "stock", "common", "THB", "TH"),
                               tir_row("AMEX", "BTC", "Grayscale Bitcoin Mini Trust ETF", "fund", "etf", "USD", "US"),
                               tir_row("BINANCE", "BTCUSDT", "Bitcoin / TetherUS", "spot", "crypto", "USDT", "")});
    b.insert("AAPL", QJsonArray{tir_row("NASDAQ", "AAPL", "Apple Inc.", "stock", "common", "USD", "US"),
                                tir_row("BMV", "AAPL", "Apple Inc.", "stock", "common", "MXN", "MX"),
                                tir_row("BIVA", "AAPL", "Apple Inc.", "stock", "common", "MXN", "MX"),
                                tir_row("VIE", "AAPL", "Apple Inc.", "stock", "common", "EUR", "AT")});
    b.insert("PLUG", QJsonArray{tir_row("NASDAQ", "PLUG", "Plug Power Inc.", "stock", "common", "USD", "US")});
    b.insert("INF", QJsonArray{tir_row("NSE", "INFY", "Infosys Limited", "stock", "common", "INR", "IN")});
    b.insert("INFY", QJsonArray{tir_row("NSE", "INFY", "Infosys Limited", "stock", "common", "INR", "IN"),
                                tir_row("NYSE", "INFY", "Infosys Limited Sponsored ADR", "dr", "", "USD", "US")});
    fake_->yahoo.insert("PLUG POWER", QJsonArray{tir_yrow("PLUG", "Plug Power Inc.", "NCM", "EQUITY")});
    fake_->yahoo.insert("US5949181045", QJsonArray{tir_yrow("MSFT", "Microsoft Corporation", "NMS", "EQUITY")});
    fake_->validate.insert("ZZTOP.NS", tir_yrow("ZZTOP.NS", "ZZ Top Industries Ltd", "NSI", "EQUITY"));
    resolver_ = std::make_unique<InstrumentResolver>(tir_sources(fake_));
}

void TstInstrumentResolver::cleanupTestCase() {
    resolver_.reset();
    fincept::SymbolMasterRepository::set_forbidden_thread(nullptr);
    fincept::Database::instance().close();
}

// ── Mapping ─────────────────────────────────────────────────────────────────

void TstInstrumentResolver::map_market_search_rows() {
    using fincept::services::instrument_from_market_search_row;
    const Instrument rel = instrument_from_market_search_row(
        tir_row("NSE", "RELIANCE", "Reliance Industries Limited", "stock", "common", "INR", "IN"), 0);
    QCOMPARE(rel.symbol, QStringLiteral("RELIANCE.NS"));
    QCOMPARE(rel.id, QStringLiteral("XNSE:RELIANCE"));
    QCOMPARE(rel.currency, QStringLiteral("INR"));
    QCOMPARE(rel.asset_class, AssetClass::Equity);
    QCOMPARE(rel.provider_symbols.value("tv"), QStringLiteral("NSE:RELIANCE"));
    QCOMPARE(rel.rank_weight, 5);

    const Instrument nifty = instrument_from_market_search_row(tir_row("NSE", "NIFTY", "Nifty 50 Index", "index", "main"), 3);
    QCOMPARE(nifty.symbol, QStringLiteral("^NSEI"));
    QCOMPARE(nifty.asset_class, AssetClass::Index);
    QCOMPARE(nifty.rank_weight, 4);

    const Instrument btc = instrument_from_market_search_row(
        tir_row("BINANCE", "BTCUSDT", "Bitcoin / TetherUS", "spot", "crypto", "USDT", "GLOBAL"), 0);
    QCOMPARE(btc.symbol, QStringLiteral("BTC-USD"));
    QCOMPARE(btc.asset_class, AssetClass::Crypto);
    QVERIFY(btc.country.isEmpty());

    const Instrument brk = instrument_from_market_search_row(
        tir_row("NYSE", "BRK.B", "Berkshire Hathaway Inc. Class B", "stock", "common", "USD", "US"), 0);
    QCOMPARE(brk.symbol, QStringLiteral("BRK-B"));
    QCOMPARE(brk.venue, QStringLiteral("NYSE")); // filled from the row's exchange (bare US form has none)
    QCOMPARE(brk.currency, QStringLiteral("USD"));

    const Instrument biva = instrument_from_market_search_row(tir_row("BIVA", "AAPL", "Apple Inc.", "stock"), 0);
    QCOMPARE(biva.symbol, QStringLiteral("AAPL.MX"));

    const Instrument etf = instrument_from_market_search_row(tir_row("AMEX", "SPY", "SPDR S&P 500 ETF", "fund", "etf", "USD", "US"), 0);
    QCOMPARE(etf.symbol, QStringLiteral("SPY"));
    QCOMPARE(etf.asset_class, AssetClass::Etf);

    // No Yahoo symbol: kept, not quotable, with an id of its own and the row's facts.
    const Instrument fut = instrument_from_market_search_row(
        tir_row("NSE", "RELIANCE1!", "RELIANCE INDS FUTURES", "futures", "continuous", "INR", "IN"), 0);
    QVERIFY(fut.symbol.isEmpty());
    QVERIFY(!fut.id.isEmpty());
    QCOMPARE(fut.display, QStringLiteral("RELIANCE1!"));
    QCOMPARE(fut.asset_class, AssetClass::Future);
    QCOMPARE(fut.currency, QStringLiteral("INR"));
    QVERIFY(!fincept::services::is_quotable(fut));

    const Instrument unknown = instrument_from_market_search_row(tir_row("NOSUCHX", "XYZ", "Xyz Co", "stock", "common", "XXX", "ZZ"), 0);
    QVERIFY(unknown.symbol.isEmpty()); // never the bare code (that would be a US listing)
    QCOMPARE(unknown.id, QStringLiteral("TV.NOSUCHX:XYZ"));

    QVERIFY(!instrument_from_market_search_row(tir_row("ECONOMICS", "USGDP", "US GDP", "economic"), 0).is_valid());

    // Several venues mapping to one listing collapse in the resolver, not here.
    const QVector<Instrument> rows = fincept::services::instruments_from_market_search(
        QJsonDocument(QJsonObject{{"results", fake_->backend.value("AAPL")}}));
    QCOMPARE(rows.size(), 4);
}

void TstInstrumentResolver::map_yahoo_rows() {
    using fincept::services::instrument_from_yahoo_row;
    // The script defaults a missing currency to USD: the listing's own currency wins.
    const Instrument rel = instrument_from_yahoo_row(tir_yrow("RELIANCE.NS", "Reliance Industries Limited", "NSI", "EQUITY"), 0);
    QCOMPARE(rel.currency, QStringLiteral("INR"));
    QCOMPARE(rel.venue, QStringLiteral("NSE"));
    const Instrument aapl = instrument_from_yahoo_row(tir_yrow("AAPL", "Apple Inc.", "NMS", "EQUITY"), 0);
    QCOMPARE(aapl.venue, QStringLiteral("NASDAQ"));
    QCOMPARE(aapl.id, QStringLiteral("US:AAPL"));
    const Instrument spy = instrument_from_yahoo_row(tir_yrow("SPY", "SPDR S&P 500 ETF Trust", "PCX", "ETF"), 0);
    QCOMPARE(spy.asset_class, AssetClass::Etf);
    QVERIFY(!instrument_from_yahoo_row(tir_yrow("US0378331005", "Apple", "", "EQUITY"), 0).is_valid()); // never an ISIN
    const QVector<Instrument> isin_rows = fincept::services::instruments_from_yahoo_search(
        QJsonObject{{"results", QJsonArray{tir_yrow("MSFT", "Microsoft", "NMS", "EQUITY")}}}, "US5949181045");
    QCOMPARE(isin_rows.value(0).isin, QStringLiteral("US5949181045"));
}

void TstInstrumentResolver::trading_meta() {
    using fincept::services::trading_meta_for;
    const auto rel = trading_meta_for(instrument_skeleton("RELIANCE.NS"));
    QCOMPARE(rel.venue, QStringLiteral("NSE"));
    QCOMPARE(rel.time_zone, QStringLiteral("Asia/Kolkata"));
    QVERIFY(rel.lot_known);
    QCOMPARE(rel.lot_size, 1.0);
    QVERIFY(!rel.tick_known);
    const auto toyota = trading_meta_for(instrument_skeleton("7203.T"));
    QCOMPARE(toyota.lot_size, 100.0);
    QVERIFY(toyota.lot_known);
    const auto aapl = trading_meta_for(instrument_skeleton("AAPL"));
    QCOMPARE(aapl.time_zone, QStringLiteral("America/New_York"));
    const auto hk = trading_meta_for(instrument_skeleton("0700.HK"));
    QVERIFY(!hk.lot_known); // board lot differs per stock
    const auto idx = trading_meta_for(instrument_skeleton("^NSEI"));
    QVERIFY(!idx.lot_known);
    QCOMPARE(idx.time_zone, QStringLiteral("Asia/Kolkata"));
    const auto btc = trading_meta_for(instrument_skeleton("BTC-USD"));
    QVERIFY(!btc.time_zone.isEmpty());
    QVERIFY(!btc.lot_known);
}

// ── Owner-critical answers ──────────────────────────────────────────────────

void TstInstrumentResolver::owner_critical_resolve_data() {
    QTest::addColumn<QString>("text");
    QTest::addColumn<QString>("home");
    QTest::addColumn<QString>("first");
    QTest::addColumn<bool>("exact");
    QTest::addColumn<QString>("note_part");
    QTest::newRow("BTC -> coin") << "BTC" << "US" << "BTC-USD" << true << "";
    QTest::newRow("btc-usd") << "btc-usd" << "" << "BTC-USD" << true << "";
    QTest::newRow("BTC/USDT") << "BTC/USDT" << "" << "BTC-USD" << true << "";
    QTest::newRow("apple -> AAPL") << "apple" << "US" << "AAPL" << true << ""; // seed alias "Apple"
    QTest::newRow("apple inc") << "apple inc" << "US" << "AAPL" << false << "";
    QTest::newRow("AAPL") << "AAPL" << "IN" << "AAPL" << true << "";
    QTest::newRow("RELIANCE home IN") << "RELIANCE" << "IN" << "RELIANCE.NS" << true << "";
    QTest::newRow("NSE:RELIANCE") << "NSE:RELIANCE" << "" << "RELIANCE.NS" << true << "";
    QTest::newRow("reliance.ns") << "reliance.ns" << "" << "RELIANCE.NS" << true << "";
    QTest::newRow("TATAMOTORS renamed") << "TATAMOTORS" << "IN" << "TMPV.NS" << true << "TATAMOTORS is now TMPV";
    QTest::newRow("relaince typo") << "relaince" << "IN" << "RELIANCE.NS" << false << "did you mean";
    QTest::newRow("ISIN seeded") << "US0378331005" << "" << "AAPL" << true << "";
    QTest::newRow("ISIN via Yahoo") << "US5949181045" << "" << "MSFT" << true << "";
    QTest::newRow("700") << "700" << "" << "0700.HK" << true << "";
    QTest::newRow("0700.HK") << "0700.HK" << "" << "0700.HK" << true << "";
    QTest::newRow("EURUSD") << "EURUSD" << "" << "EURUSD=X" << true << "";
    QTest::newRow("USD/INR") << "USD/INR" << "" << "INR=X" << true << "";
    QTest::newRow("^NSEI") << "^NSEI" << "" << "^NSEI" << true << "";
    QTest::newRow("nifty 50") << "nifty 50" << "IN" << "^NSEI" << true << "";
    QTest::newRow("gold") << "gold" << "US" << "GC=F" << true << "";
    QTest::newRow("GC=F") << "GC=F" << "" << "GC=F" << true << "";
}

void TstInstrumentResolver::owner_critical_resolve() {
    QFETCH(QString, text);
    QFETCH(QString, home);
    QFETCH(QString, first);
    QFETCH(bool, exact);
    QFETCH(QString, note_part);
    ResolveOptions o = tir_opts(qPrintable(home));
    QObject ctx;
    const ResolveResult r =
        tir_wait([&](auto cb) { return resolver_->resolve(text, o, &ctx, cb); });
    QVERIFY2(r.final, qPrintable(text + QStringLiteral(": no final answer")));
    QVERIFY2(r.error.isEmpty(), qPrintable(r.error));
    QVERIFY2(!r.matches.isEmpty() && r.matches.first().symbol == first,
             qPrintable(text + QStringLiteral(" -> ") + tir_symbols(r, 6).join(',') + QStringLiteral(" | ") + r.note));
    QCOMPARE(r.exact, exact);
    QCOMPARE(r.ranked.size(), r.matches.size());
    if (!note_part.isEmpty())
        QVERIFY2(r.note.contains(note_part, Qt::CaseInsensitive), qPrintable(r.note));
    // Every match carries what a caller needs without another lookup.
    const Instrument& m = r.matches.first();
    QVERIFY(!m.id.isEmpty());
    QVERIFY(!m.display.isEmpty());
    QVERIFY(m.asset_class != AssetClass::Unknown);
    QVERIFY2(!m.currency.isEmpty(), qPrintable(m.symbol));
}

void TstInstrumentResolver::search_merges_and_dedupes() {
    QObject ctx;
    QVector<ResolveResult> calls;
    const ResolveResult r = tir_wait([&](auto cb) { return resolver_->search("AAPL", tir_opts("US"), &ctx, cb); }, &calls);
    QVERIFY(calls.size() >= 2);
    QVERIFY(!calls.first().final); // local first, remote still to come
    QVERIFY(r.from_remote);
    QCOMPARE(r.matches.first().symbol, QStringLiteral("AAPL"));
    QCOMPARE(tir_symbols(r).count(QStringLiteral("AAPL.MX")), 1); // BMV + BIVA rows are one listing
    QCOMPARE(tir_symbols(r).count(QStringLiteral("AAPL")), 1);    // seed + backend copy merged
    const Instrument& aapl = r.matches.first();
    QCOMPARE(aapl.name, QStringLiteral("Apple Inc."));            // curated name kept
    QCOMPARE(aapl.rank_weight, 10);                               // curated weight kept
    QCOMPARE(aapl.provider_symbols.value("tv"), QStringLiteral("NASDAQ:AAPL")); // remote fills
    QCOMPARE(aapl.venue, QStringLiteral("NASDAQ"));

    // A remote-only instrument arrives with its facts.
    QObject ctx2;
    const ResolveResult plug = tir_wait([&](auto cb) { return resolver_->search("PLUG", tir_opts("US"), &ctx2, cb); });
    QCOMPARE(plug.matches.value(0).symbol, QStringLiteral("PLUG"));
    QCOMPARE(plug.matches.value(0).currency, QStringLiteral("USD"));
    QCOMPARE(plug.matches.value(0).venue, QStringLiteral("NASDAQ"));
    QVERIFY(plug.from_remote);

    // Rows without a Yahoo symbol stay listed (not quotable); dated / continuous futures are filtered.
    QObject ctx3;
    const ResolveResult rel = tir_wait([&](auto cb) { return resolver_->search("RELIANCE", tir_opts("IN"), &ctx3, cb); });
    QCOMPARE(rel.matches.value(0).symbol, QStringLiteral("RELIANCE.NS"));
    QCOMPARE(rel.matches.value(1).symbol, QStringLiteral("RELIANCE.BO"));
    bool has_unquoted = false;
    for (const Instrument& i : rel.matches) {
        QVERIFY2(i.asset_class != AssetClass::Future, qPrintable(i.id));
        if (i.symbol.isEmpty())
            has_unquoted = true;
    }
    QVERIFY(has_unquoted); // the Dhaka fund row: no quote data, but shown
}

void TstInstrumentResolver::ambiguous_tickers_are_not_exact() {
    QObject ctx;
    const ResolveResult tcs = tir_wait([&](auto cb) { return resolver_->resolve("TCS", tir_opts("IN"), &ctx, cb); });
    QCOMPARE(tcs.matches.value(0).symbol, QStringLiteral("TCS.NS"));
    QVERIFY(!tcs.exact); // TECSYS on the TSX is another company
    QVERIFY2(tcs.note.contains(QStringLiteral("more than one")), qPrintable(tcs.note));
    QVERIFY(tir_symbols(tcs).contains(QStringLiteral("TCS.TO")));

    const ResolveResult sap = tir_wait([&](auto cb) { return resolver_->resolve("SAP", tir_opts("US"), &ctx, cb); });
    QCOMPARE(tir_symbols(sap, 3), (QStringList{"SAP", "SAP.DE", "SAP.TO"}));
    QVERIFY(!sap.exact); // Saputo

    const ResolveResult infy = tir_wait([&](auto cb) { return resolver_->resolve("INFY", tir_opts("IN"), &ctx, cb); });
    QCOMPARE(infy.matches.value(0).symbol, QStringLiteral("INFY.NS"));
    QVERIFY(infy.exact); // the NYSE ADR is the same issuer
}

void TstInstrumentResolver::class_filter() {
    QObject ctx;
    ResolveOptions o = tir_opts("US");
    o.ctx.allowed_classes = {AssetClass::Equity, AssetClass::Etf};
    const ResolveResult r = tir_wait([&](auto cb) { return resolver_->search("BTC", o, &ctx, cb); });
    QVERIFY(!r.matches.isEmpty());
    for (const Instrument& i : r.matches)
        QVERIFY2(i.asset_class == AssetClass::Equity || i.asset_class == AssetClass::Etf, qPrintable(i.symbol));
    QCOMPARE(r.matches.first().symbol, QStringLiteral("BTC"));

    ResolveOptions fx = tir_opts();
    fx.ctx.allowed_classes = {AssetClass::Equity};
    const ResolveResult none = tir_wait([&](auto cb) { return resolver_->resolve("EURUSD", fx, &ctx, cb); });
    QVERIFY(none.matches.isEmpty());
    QVERIFY2(none.note.contains(QStringLiteral("instrument types")), qPrintable(none.note));
}

void TstInstrumentResolver::yahoo_fallback_and_learning() {
    QObject ctx;
    const int before = fake_->yahoo_calls.size();
    const ResolveResult r = tir_wait([&](auto cb) { return resolver_->resolve("Plug Power", tir_opts("US"), &ctx, cb); });
    QCOMPARE(fake_->yahoo_calls.size(), before + 1);
    QCOMPARE(r.matches.value(0).symbol, QStringLiteral("PLUG"));
    QVERIFY(r.from_remote);
    // Learned: the next local-only search finds it (the index writes after its 1.5 s coalescing).
    ResolveOptions local = tir_opts("US");
    local.allow_remote = false;
    bool found = false;
    for (int i = 0; i < 20 && !found; ++i) {
        QTest::qWait(300);
        const ResolveResult l = tir_wait([&](auto cb) { return resolver_->resolve("PLUG", local, &ctx, cb); });
        found = l.matches.value(0).symbol == QLatin1String("PLUG") && l.exact;
    }
    QVERIFY(found);
}

void TstInstrumentResolver::validation_confirms_a_symbol() {
    QObject ctx;
    const ResolveResult r = tir_wait([&](auto cb) { return resolver_->resolve("zztop.ns", tir_opts(), &ctx, cb); });
    QVERIFY(fake_->validate_calls.contains(QStringLiteral("ZZTOP.NS")));
    QCOMPARE(r.matches.value(0).symbol, QStringLiteral("ZZTOP.NS"));
    QVERIFY(r.exact);
    QCOMPARE(r.matches.value(0).currency, QStringLiteral("INR"));
    // Something nobody knows: no match, a plain note, no error.
    const ResolveResult none = tir_wait([&](auto cb) { return resolver_->resolve("QQQZZZ.NS", tir_opts(), &ctx, cb); });
    QVERIFY(none.matches.isEmpty());
    QVERIFY(none.error.isEmpty());
    QVERIFY(!none.note.isEmpty());
}

void TstInstrumentResolver::stale_requests_are_dropped() {
    auto f = std::make_shared<TirFake>(*fake_);
    f->backend_calls.clear();
    InstrumentResolver r(tir_sources(f)); // fresh: no cached replies
    QObject ctx;
    QStringList delivered;
    for (const char* t : {"R", "RE", "REL", "RELIANCE"}) {
        const QString text = QString::fromLatin1(t);
        r.search(text, tir_opts("IN"), &ctx, [&delivered](const ResolveResult& res) {
            delivered << res.query + (res.final ? QStringLiteral("/final") : QStringLiteral("/local"));
        });
    }
    QTRY_VERIFY_WITH_TIMEOUT(delivered.contains(QStringLiteral("RELIANCE/final")), 10000);
    QTest::qWait(300);
    for (const QString& d : delivered)
        QVERIFY2(d.startsWith(QStringLiteral("RELIANCE/")), qPrintable(delivered.join(' ')));
    // Debounced: the backend saw only the last text.
    QCOMPARE(f->backend_calls, QStringList{QStringLiteral("RELIANCE")});

    // A destroyed context never gets its callback.
    bool called = false;
    auto* gone = new QObject;
    r.search("SAP", tir_opts(), gone, [&called](const ResolveResult&) { called = true; });
    delete gone;
    QTest::qWait(400);
    QVERIFY(!called);
}

void TstInstrumentResolver::one_backend_request_in_flight_per_context() {
    auto f = std::make_shared<TirFake>(*fake_);
    f->backend_calls.clear();
    f->backend_delay_ms = 400;
    f->backend_max_in_flight = 0;
    InstrumentResolver r(tir_sources(f));
    QObject ctx;
    QStringList finals;
    auto cb = [&finals](const ResolveResult& res) {
        if (res.final)
            finals << res.query;
    };
    r.search("INF", tir_opts("IN"), &ctx, cb);
    QTRY_VERIFY_WITH_TIMEOUT(f->backend_calls.size() == 1, 5000); // "INF" is in flight
    r.search("INFY", tir_opts("IN"), &ctx, cb);
    QTRY_VERIFY_WITH_TIMEOUT(finals.contains(QStringLiteral("INFY")), 10000);
    QCOMPARE(f->backend_calls, (QStringList{"INF", "INFY"}));
    QCOMPARE(f->backend_max_in_flight, 1);
    QVERIFY(!finals.contains(QStringLiteral("INF")));
}

void TstInstrumentResolver::cache_and_cancel() {
    auto f = std::make_shared<TirFake>(*fake_);
    f->backend_calls.clear();
    InstrumentResolver r(tir_sources(f));
    QObject a;
    QObject b;
    tir_wait([&](auto cb) { return r.search("PLUG", tir_opts("US"), &a, cb); });
    tir_wait([&](auto cb) { return r.search("PLUG", tir_opts("US"), &b, cb); });
    QCOMPARE(f->backend_calls.count(QStringLiteral("PLUG")), 1); // second answer from the cache

    bool called = false;
    const quint64 id = r.resolve("TCS", tir_opts("IN"), &a, [&called](const ResolveResult&) { called = true; });
    r.cancel(id);
    QTest::qWait(500);
    QVERIFY(!called);
}

void TstInstrumentResolver::offline_keeps_local_results() {
    auto f = std::make_shared<TirFake>(*fake_);
    f->backend_calls.clear();
    f->backend_unreachable = true;
    f->yahoo_unreachable = true;
    InstrumentResolver r(tir_sources(f));
    QObject ctx;
    const ResolveResult rel = tir_wait([&](auto cb) { return r.search("RELIANCE", tir_opts("IN"), &ctx, cb); });
    QVERIFY(rel.offline);
    QVERIFY(rel.error.isEmpty());
    QCOMPARE(rel.matches.value(0).symbol, QStringLiteral("RELIANCE.NS"));
    QVERIFY2(rel.note.contains(QStringLiteral("unavailable")), qPrintable(rel.note));
    // Backed off: the next request does not knock again.
    const int calls = f->backend_calls.size();
    const ResolveResult tcs = tir_wait([&](auto cb) { return r.search("TCS", tir_opts("IN"), &ctx, cb); });
    QCOMPARE(f->backend_calls.size(), calls);
    QVERIFY(tcs.offline);
    QCOMPARE(tcs.matches.value(0).symbol, QStringLiteral("TCS.NS"));
    // resolve() still answers an exact local hit.
    const ResolveResult aapl = tir_wait([&](auto cb) { return r.resolve("AAPL", tir_opts("US"), &ctx, cb); });
    QVERIFY(aapl.exact);
}

void TstInstrumentResolver::invalid_and_empty_text() {
    QObject ctx;
    const ResolveResult junk =
        tir_wait([&](auto cb) { return resolver_->resolve(QString(120, QLatin1Char('A')), tir_opts(), &ctx, cb); });
    QVERIFY(!junk.error.isEmpty());
    QVERIFY(junk.matches.isEmpty());
    const ResolveResult bad_isin = tir_wait([&](auto cb) { return resolver_->resolve("US0378331006", tir_opts(), &ctx, cb); });
    QVERIFY2(bad_isin.error.contains(QStringLiteral("check digit")), qPrintable(bad_isin.error));
    const ResolveResult empty = tir_wait([&](auto cb) { return resolver_->search("   ", tir_opts(), &ctx, cb); });
    QVERIFY(empty.final);
    QVERIFY(empty.matches.isEmpty());
    QVERIFY(empty.error.isEmpty());
}

void TstInstrumentResolver::resolve_stored_never_guesses() {
    QObject ctx;
    const int b = fake_->backend_calls.size();
    const int y = fake_->yahoo_calls.size();
    auto stored = [&](const char* sym, const char* hint) {
        return tir_wait([&](auto cb) { return resolver_->resolve_stored(sym, hint, &ctx, cb); });
    };
    const ResolveResult tm = stored("TATAMOTORS.NS", "");
    QCOMPARE(tir_symbols(tm), (QStringList{"TATAMOTORS.NS", "TMPV.NS"}));
    QCOMPARE(tm.matches.first().status, InstrumentStatus::Renamed);
    QVERIFY(tm.exact);
    QVERIFY2(tm.note.contains(QStringLiteral("now TMPV")), qPrintable(tm.note));

    const ResolveResult rel = stored("RELIANCE", "NSE");
    QCOMPARE(tir_symbols(rel), QStringList{"RELIANCE.NS"});
    QVERIFY(rel.exact);
    QCOMPARE(rel.matches.first().name, QStringLiteral("Reliance Industries Limited"));

    const ResolveResult brk = stored(" brk.b ", "");
    QCOMPARE(brk.matches.value(0).symbol, QStringLiteral("BRK-B"));

    // A bare code with no exchange stays what it means on Yahoo (US) and is flagged.
    const ResolveResult bare = stored("RELIANCE", "");
    QCOMPARE(tir_symbols(bare), QStringList{"RELIANCE"});
    QVERIFY(!bare.exact);
    QVERIFY2(bare.note.contains(QStringLiteral("no exchange")), qPrintable(bare.note));

    const ResolveResult unknown_listing = stored("NOSUCHCO.NS", "");
    QCOMPARE(unknown_listing.matches.value(0).id, QStringLiteral("XNSE:NOSUCHCO"));
    QVERIFY(unknown_listing.exact);

    const ResolveResult name = stored("tata motors", "");
    QVERIFY(name.matches.isEmpty());
    QVERIFY(!name.note.isEmpty());

    const ResolveResult isin = stored("US0378331005", "");
    QCOMPARE(isin.matches.value(0).symbol, QStringLiteral("AAPL"));
    QVERIFY(isin.exact);

    const ResolveResult fx = stored("eurusd=x", "");
    QCOMPARE(fx.matches.value(0).symbol, QStringLiteral("EURUSD=X"));

    QCOMPARE(fake_->backend_calls.size(), b); // local only: no network
    QCOMPARE(fake_->yahoo_calls.size(), y);
}

void TstInstrumentResolver::resolve_many_keeps_order() {
    QObject ctx;
    QVector<ResolveResult> got;
    bool done = false;
    resolver_->resolve_many({"AAPL", "RELIANCE.NS", "QQQZZZ.NS", "TCS", "BTC"}, tir_opts("IN"), &ctx,
                            [&](const QVector<ResolveResult>& r) {
                                got = r;
                                done = true;
                            });
    QTRY_VERIFY_WITH_TIMEOUT(done, 20000);
    QCOMPARE(got.size(), 5);
    QCOMPARE(got.at(0).matches.value(0).symbol, QStringLiteral("AAPL"));
    QVERIFY(got.at(0).exact);
    QCOMPARE(got.at(1).matches.value(0).symbol, QStringLiteral("RELIANCE.NS"));
    QVERIFY(got.at(1).exact);
    QVERIFY(got.at(2).matches.isEmpty());
    QVERIFY(!got.at(3).exact);
    QCOMPARE(got.at(4).matches.value(0).symbol, QStringLiteral("BTC-USD"));
}

void TstInstrumentResolver::recent_picks() {
    auto f = std::make_shared<TirFake>();
    InstrumentResolver r(tir_sources(f));
    r.remember_pick("wl", instrument_skeleton("AAPL"));
    r.remember_pick("wl", instrument_skeleton("RELIANCE.NS"));
    r.remember_pick("wl", instrument_skeleton("AAPL"));
    const QVector<Instrument> rec = r.recent_picks("wl");
    QCOMPARE(rec.size(), 2);
    QCOMPARE(rec.at(0).symbol, QStringLiteral("AAPL"));
    QCOMPARE(rec.at(1).symbol, QStringLiteral("RELIANCE.NS"));
    QVERIFY(r.recent_picks("other").isEmpty());
}

// ── Sync ────────────────────────────────────────────────────────────────────

void TstInstrumentResolver::sync_404_disables_quietly() {
    int gets = 0;
    InstrumentSync::Ops ops;
    ops.first_delay_ms = 0;
    ops.get = [&gets](const QString&, QObject* ctx, InstrumentSync::HttpDone done) {
        ++gets;
        QTimer::singleShot(0, ctx, [done]() {
            InstrumentSync::HttpReply rep;
            rep.status = 404;
            rep.error = QStringLiteral("HTTP_404: Not Found");
            done(rep);
        });
    };
    ops.read_cursor = [](QObject* ctx, InstrumentSync::CursorDone done) {
        QTimer::singleShot(0, ctx, [done]() { done(QString()); });
    };
    ops.apply_delta = [](const QJsonObject&, QObject*, InstrumentSync::DeltaDone done) { done(false, "unexpected", false); };
    InstrumentSync sync(ops);
    QSignalSpy off(&sync, &InstrumentSync::endpoint_unavailable);
    sync.start();
    QTRY_COMPARE_WITH_TIMEOUT(off.count(), 1, 5000);
    QVERIFY(!sync.is_enabled());
    sync.sync_now();
    sync.start();
    QTest::qWait(200);
    QCOMPARE(gets, 1);
    QCOMPARE(off.count(), 1);
}

void TstInstrumentResolver::sync_applies_pages_through_the_index() {
    QStringList paths;
    Instrument newco = instrument_skeleton("NEWCO.NS");
    newco.name = QStringLiteral("New Company Limited");
    InstrumentSync::Ops ops;
    ops.first_delay_ms = 0;
    ops.get = [&paths, newco](const QString& path, QObject* ctx, InstrumentSync::HttpDone done) {
        paths << path;
        QTimer::singleShot(0, ctx, [path, newco, done]() {
            InstrumentSync::HttpReply rep;
            rep.status = 200;
            if (!path.contains(QStringLiteral("since=")))
                rep.body = QJsonDocument(QJsonObject{
                    {"next", "7"},
                    {"has_more", true},
                    {"changes", QJsonArray{QJsonObject{{"op", "upsert"}, {"instrument", newco.to_json()}}}}});
            else
                rep.body = QJsonDocument(QJsonObject{{"since", "7"}, {"next", "9"}, {"has_more", false}, {"changes", QJsonArray{}}});
            done(rep);
        });
    };
    ops.read_cursor = [](QObject* ctx, InstrumentSync::CursorDone done) {
        InstrumentIndex::instance().read_sync_cursor(ctx, std::move(done));
    };
    ops.apply_delta = [](const QJsonObject& d, QObject* ctx, InstrumentSync::DeltaDone done) {
        InstrumentIndex::instance().apply_master_delta(d, ctx, std::move(done));
    };
    InstrumentSync sync(ops);
    QSignalSpy finished(&sync, &InstrumentSync::sync_finished);
    sync.start();
    QTRY_COMPARE_WITH_TIMEOUT(finished.count(), 1, 10000);
    QCOMPARE(finished.first().at(0).toInt(), 2);
    QCOMPARE(finished.first().at(1).toString(), QStringLiteral("9"));
    QCOMPARE(paths.size(), 2);
    QVERIFY2(paths.at(0).startsWith(QStringLiteral("/symbols/changes?limit=")), qPrintable(paths.at(0)));
    QVERIFY2(paths.at(1).contains(QStringLiteral("since=7")), qPrintable(paths.at(1)));
    QVERIFY(sync.is_enabled());

    QObject ctx;
    const ResolveResult r = tir_wait([&](auto cb) { return resolver_->resolve_stored("NEWCO.NS", "", &ctx, cb); });
    QCOMPARE(r.matches.value(0).name, QStringLiteral("New Company Limited"));
}

void TstInstrumentResolver::sync_410_refetches_from_start() {
    QStringList paths;
    InstrumentSync::Ops ops;
    ops.first_delay_ms = 0;
    ops.get = [&paths](const QString& path, QObject* ctx, InstrumentSync::HttpDone done) {
        paths << path;
        QTimer::singleShot(0, ctx, [path, done]() {
            InstrumentSync::HttpReply rep;
            if (path.contains(QStringLiteral("since="))) {
                rep.status = 410;
            } else {
                rep.status = 200;
                rep.body = QJsonDocument(QJsonObject{{"next", "1"}, {"changes", QJsonArray{}}});
            }
            done(rep);
        });
    };
    ops.read_cursor = [](QObject* ctx, InstrumentSync::CursorDone done) {
        QTimer::singleShot(0, ctx, [done]() { done(QStringLiteral("99")); });
    };
    ops.apply_delta = [](const QJsonObject& d, QObject* ctx, InstrumentSync::DeltaDone done) {
        QTimer::singleShot(0, ctx, [d, done]() { done(true, d.value("next").toString(), false); });
    };
    InstrumentSync sync(ops);
    QSignalSpy finished(&sync, &InstrumentSync::sync_finished);
    sync.start();
    QTRY_COMPARE_WITH_TIMEOUT(finished.count(), 1, 5000);
    QCOMPARE(paths.size(), 2);
    QVERIFY(paths.at(0).contains(QStringLiteral("since=99")));
    QVERIFY(!paths.at(1).contains(QStringLiteral("since=")));
    QCOMPARE(finished.first().at(1).toString(), QStringLiteral("1"));
}

QTEST_GUILESS_MAIN(TstInstrumentResolver)
#include "tst_instrument_resolver.moc"
