// tst_backend_symbol_search — src/services/instruments/BackendSymbolSearch: the resolver's
// backend source asks GET /symbols/search first and falls back to GET /market/search
// (404 = not deployed, decided for the session; any other failure = this request only),
// over a FAKE asynchronous HTTP function. No network.
#include "services/instruments/BackendSymbolSearch.h"

#include <QCoreApplication>
#include <QJsonArray>
#include <QJsonObject>
#include <QTimer>
#include <QtTest>

using namespace fincept::services;
namespace ci = fincept::core::instrument;

namespace {

/// The TATAMOTORS answer of /symbols/search (shape of the WP8 examples, trimmed).
QJsonDocument symbols_reply() {
    QJsonObject tmpv{{"id", "XNSE:TMPV"},
                     {"symbol", "TMPV.NS"},
                     {"display", "TMPV"},
                     {"name", "Tata Motors Passenger Vehicles Limited"},
                     {"asset_class", "equity"},
                     {"quote_type", "EQUITY"},
                     {"venue", "NSE"},
                     {"mic", "XNSE"},
                     {"country", "IN"},
                     {"currency", "INR"},
                     {"isin", "INE155A01022"},
                     {"aliases", QJsonArray{"Tata Motors PV", "TELCO", "... 2 more"}},
                     {"former_symbols", QJsonArray{"TATAMOTORS"}},
                     {"primary_listing", true},
                     {"rank_weight", 10},
                     {"status", "active"},
                     {"match", QJsonObject{{"kind", "exact_former_symbol"}, {"text", "TATAMOTORS"}}}};
    QJsonObject bse{{"id", "XBOM:500570"},     {"symbol", "500570.BO"}, {"display", "500570"},
                    {"name", "Tata Motors PV"}, {"asset_class", "equity"}, {"venue", "BSE"},
                    {"mic", "XBOM"},            {"country", "IN"},       {"currency", "INR"}};
    return QJsonDocument(QJsonObject{{"query", "TATAMOTORS"},
                                     {"count", 2},
                                     {"version", "2026-11-16.1"},
                                     {"did_you_mean", QJsonValue()},
                                     {"results", QJsonArray{tmpv, bse}}});
}

QJsonDocument market_reply() {
    return QJsonDocument(QJsonObject{
        {"results", QJsonArray{QJsonObject{{"symbol", "AAPL"},
                                           {"name", "Apple Inc."},
                                           {"exchange", "NASDAQ"},
                                           {"country", "US"},
                                           {"type", "stock"}}}}});
}

/// Fake HTTP: answers by path prefix, always asynchronously (as HttpClient does).
struct FakeHttp {
    QStringList paths;
    QHash<QString, BackendSymbolSearch::HttpReply> by_prefix;

    BackendSymbolSearch::HttpGet get() {
        return [this](const QString& path, QObject* context, BackendSymbolSearch::HttpDone done) {
            paths << path;
            BackendSymbolSearch::HttpReply rep;
            for (auto it = by_prefix.cbegin(); it != by_prefix.cend(); ++it)
                if (path.startsWith(it.key()))
                    rep = it.value();
            QTimer::singleShot(0, context, [rep, done = std::move(done)]() { done(rep); });
        };
    }
};

BackendSymbolSearch::HttpReply reply(int status, const QJsonDocument& body = {}) {
    BackendSymbolSearch::HttpReply r;
    r.status = status;
    r.body = body;
    if (status < 200 || status >= 300)
        r.error = QStringLiteral("HTTP %1").arg(status);
    return r;
}

/// Runs one search and waits for its single answer.
ResolverRemoteReply run(BackendSymbolSearch& s, const QString& q, int limit, QObject* ctx, bool* sync = nullptr) {
    ResolverRemoteReply out;
    int calls = 0;
    s.search(q, limit, ctx, [&](const ResolverRemoteReply& r) {
        out = r;
        ++calls;
    });
    if (sync)
        *sync = calls > 0;
    if (!QTest::qWaitFor([&]() { return calls > 0; }, 2000))
        qWarning("no answer within 2 s");
    QTest::qWait(20); // a second (wrong) call would land here
    if (calls != 1)
        qFatal("done called %d times", calls);
    return out;
}

} // namespace

class TestBackendSymbolSearch : public QObject {
    Q_OBJECT
  private slots:
    void symbols_search_first_and_parsed() {
        FakeHttp http;
        http.by_prefix.insert(QStringLiteral("/symbols/search"), reply(200, symbols_reply()));
        BackendSymbolSearch s(http.get(), []() { return QStringLiteral("in"); });
        QObject ctx;
        bool sync = true;
        const ResolverRemoteReply r = run(s, QStringLiteral("TATAMOTORS"), 3, &ctx, &sync);
        QVERIFY(!sync); // never synchronously
        QCOMPARE(http.paths, QStringList{QStringLiteral("/symbols/search?q=TATAMOTORS&limit=3&country=IN")});
        QVERIFY(r.ok);
        QVERIFY(!r.unreachable);
        QCOMPARE(r.rows.size(), 2);
        QCOMPARE(r.rows.at(0).symbol, QStringLiteral("TMPV.NS"));
        QCOMPARE(r.rows.at(0).id, QStringLiteral("XNSE:TMPV"));
        QCOMPARE(r.rows.at(0).former_symbols, QStringList{QStringLiteral("TATAMOTORS")});
        QCOMPARE(r.rows.at(0).aliases, (QStringList{QStringLiteral("Tata Motors PV"), QStringLiteral("TELCO")}));
        QCOMPARE(r.rows.at(0).asset_class, ci::AssetClass::Equity);
        QCOMPARE(r.rows.at(1).symbol, QStringLiteral("500570.BO"));
        QCOMPARE(s.endpoint(), BackendSymbolSearch::Endpoint::SymbolsSearch);
    }

    void not_found_switches_to_market_search_for_the_session() {
        FakeHttp http;
        http.by_prefix.insert(QStringLiteral("/symbols/search"), reply(404));
        http.by_prefix.insert(QStringLiteral("/market/search"), reply(200, market_reply()));
        BackendSymbolSearch s(http.get());
        QObject ctx;
        const ResolverRemoteReply first = run(s, QStringLiteral("AAPL"), 5, &ctx);
        QVERIFY(first.ok);
        QCOMPARE(first.rows.size(), 1);
        QCOMPARE(first.rows.at(0).symbol, QStringLiteral("AAPL"));
        QCOMPARE(http.paths, (QStringList{QStringLiteral("/symbols/search?q=AAPL&limit=5"),
                                          QStringLiteral("/market/search?q=AAPL&limit=5")}));
        QCOMPARE(s.endpoint(), BackendSymbolSearch::Endpoint::MarketSearchOnly);

        http.paths.clear();
        const ResolverRemoteReply second = run(s, QStringLiteral("MSFT"), 5, &ctx);
        QVERIFY(second.ok);
        QCOMPARE(http.paths, QStringList{QStringLiteral("/market/search?q=MSFT&limit=5")});
    }

    void server_error_falls_back_for_that_request_only() {
        FakeHttp http;
        http.by_prefix.insert(QStringLiteral("/symbols/search"), reply(503));
        http.by_prefix.insert(QStringLiteral("/market/search"), reply(200, market_reply()));
        BackendSymbolSearch s(http.get());
        QObject ctx;
        const ResolverRemoteReply r = run(s, QStringLiteral("AAPL"), 5, &ctx);
        QVERIFY(r.ok);
        QCOMPARE(r.rows.size(), 1);
        QCOMPARE(s.endpoint(), BackendSymbolSearch::Endpoint::Unknown);

        http.paths.clear();
        http.by_prefix.insert(QStringLiteral("/symbols/search"), reply(200, symbols_reply()));
        const ResolverRemoteReply again = run(s, QStringLiteral("TATAMOTORS"), 5, &ctx);
        QVERIFY(again.ok);
        QCOMPARE(http.paths, QStringList{QStringLiteral("/symbols/search?q=TATAMOTORS&limit=5")});
        QCOMPARE(s.endpoint(), BackendSymbolSearch::Endpoint::SymbolsSearch);
    }

    void both_down_is_unreachable_not_no_match() {
        FakeHttp http;
        http.by_prefix.insert(QStringLiteral("/symbols/search"), reply(0));
        http.by_prefix.insert(QStringLiteral("/market/search"), reply(0));
        BackendSymbolSearch s(http.get());
        QObject ctx;
        const ResolverRemoteReply r = run(s, QStringLiteral("AAPL"), 5, &ctx);
        QVERIFY(!r.ok);
        QVERIFY(r.unreachable);
        QVERIFY(!r.error.isEmpty());
        QCOMPARE(http.paths.size(), 2);
    }

    void empty_answer_is_ok_with_no_rows() {
        FakeHttp http;
        http.by_prefix.insert(QStringLiteral("/symbols/search"),
                              reply(200, QJsonDocument(QJsonObject{{"results", QJsonArray{}}})));
        BackendSymbolSearch s(http.get());
        QObject ctx;
        const ResolverRemoteReply r = run(s, QStringLiteral("zzzz"), 5, &ctx);
        QVERIFY(r.ok);
        QVERIFY(!r.unreachable);
        QVERIFY(r.rows.isEmpty());
        QCOMPARE(http.paths.size(), 1); // no fallback for a plain "no match"
    }

    void paths_encode_and_clamp() {
        QCOMPARE(BackendSymbolSearch::symbols_search_path(QStringLiteral("S&P 500"), 0, QString()),
                 QStringLiteral("/symbols/search?q=S%26P%20500&limit=1"));
        QCOMPARE(BackendSymbolSearch::symbols_search_path(QStringLiteral("x"), 99, QStringLiteral("EU-area")),
                 QStringLiteral("/symbols/search?q=x&limit=50")); // only a 2-letter country is sent
        QCOMPARE(BackendSymbolSearch::market_search_path(QStringLiteral("BRK.B"), 12),
                 QStringLiteral("/market/search?q=BRK.B&limit=12"));
    }

    void bare_array_and_junk_rows() {
        const QJsonDocument doc(QJsonArray{QJsonObject{{"symbol", "AAPL"}, {"asset_class", "equity"}}, QJsonValue(),
                                           QJsonObject{}});
        const QVector<ci::Instrument> rows = instruments_from_symbols_search(doc);
        QCOMPARE(rows.size(), 1);
        QCOMPARE(rows.at(0).symbol, QStringLiteral("AAPL"));
        QVERIFY(!rows.at(0).id.isEmpty()); // filled by complete_instrument()
        QVERIFY(instruments_from_symbols_search(QJsonDocument()).isEmpty());
    }
};

QTEST_GUILESS_MAIN(TestBackendSymbolSearch)
#include "tst_backend_symbol_search.moc"
