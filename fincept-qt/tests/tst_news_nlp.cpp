// Unit tests for the News tab's pure text / analytics logic (Qt Core only):
//   src/services/news/NewsLanguage.{h,cpp}      language identification
//   src/services/news/NewsTickerTagger.{h,cpp}  validated ticker tagging
//   src/services/news/NewsAnalytics.{h,cpp}     velocity, tokens, baselines, buckets, trending
//
// Every timestamp is explicit, so the suite does not depend on the clock.

#include "services/news/NewsAnalytics.h"
#include "services/news/NewsLanguage.h"
#include "services/news/NewsTickerTagger.h"

#include <QString>
#include <QTest>

#include <cmath>

using namespace fincept::services;

namespace {

QStringList tnn_symbols(const QVector<news_tags::TickerTag>& tags) {
    QStringList out;
    for (const auto& t : tags)
        out.append(t.symbol);
    return out;
}

} // namespace

class TestNewsNlp : public QObject {
    Q_OBJECT

  private slots:
    // ── Language ────────────────────────────────────────────────────────────
    void language_latin_data() {
        QTest::addColumn<QString>("text");
        QTest::addColumn<QString>("expected");
        QTest::newRow("en") << QStringLiteral("Fed holds rates steady as inflation cools and stocks rise") << "en";
        QTest::newRow("fr") << QString::fromUtf8("La BCE maintient ses taux directeurs malgré la hausse des prix")
                            << "fr";
        QTest::newRow("de") << QString::fromUtf8("Dax schließt im Plus, Anleger setzen auf sinkende Zinsen") << "de";
        QTest::newRow("es") << QString::fromUtf8("El Banco de España eleva su previsión de crecimiento para la economía")
                            << "es";
        QTest::newRow("pt") << QString::fromUtf8("Ibovespa fecha em alta com ações da Petrobras e do Itaú") << "pt";
        QTest::newRow("it") << QString::fromUtf8("Borsa di Milano chiude in rialzo, bene le banche e il lusso") << "it";
        QTest::newRow("nl") << QStringLiteral("De Nederlandse economie groeit sneller dan verwacht door export") << "nl";
    }
    void language_latin() {
        QFETCH(QString, text);
        QFETCH(QString, expected);
        QCOMPARE(news_lang::detect(text).code, expected);
    }

    void language_scripts() {
        QCOMPARE(news_lang::detect(QString::fromUtf8("英伟达风波引发芯片出口限制担忧")).code, QStringLiteral("zh"));
        QCOMPARE(news_lang::detect(QString::fromUtf8("トヨタ、純利益が過去最高に")).code, QStringLiteral("ja"));
        QCOMPARE(news_lang::detect(QString::fromUtf8("삼성전자 3분기 영업이익 증가")).code, QStringLiteral("ko"));
        QCOMPARE(news_lang::detect(QString::fromUtf8("Центробанк сохранил ключевую ставку")).code, QStringLiteral("ru"));
        QCOMPARE(news_lang::detect(QString::fromUtf8("أرامكو تعلن نتائج الربع الثالث")).code, QStringLiteral("ar"));
        QCOMPARE(news_lang::detect(QString::fromUtf8("सेंसेक्स में तेजी, निफ्टी नए शिखर पर")).code, QStringLiteral("hi"));
    }

    void language_prior() {
        // Names only — no evidence: the feed's language decides.
        const auto g = news_lang::detect(QStringLiteral("Nvidia, Apple, Microsoft"), QStringLiteral("de"));
        QCOMPARE(g.code, QStringLiteral("de"));
        QVERIFY(g.from_prior);
        // Clear evidence overrides a wrong prior.
        QCOMPARE(news_lang::detect(QStringLiteral("The central bank raised rates for the third time this year"),
                                   QStringLiteral("fr"))
                     .code,
                 QStringLiteral("en"));
        // One shared or borrowed word does not relabel the feed's language
        // (real headlines that used to flip: "will" read as English, "AI" as Italian).
        QCOMPARE(news_lang::detect(QString::fromUtf8("Autohändler: VW-Chef Blume will 2030 Rekordrendite erreichen"),
                                   QStringLiteral("de"))
                     .code,
                 QStringLiteral("de"));
        QCOMPARE(news_lang::detect(QStringLiteral("DeepSeek Won't Sink U.S. AI Titans"), QStringLiteral("en")).code,
                 QStringLiteral("en"));
        // No prior, ASCII, weak evidence: English with low confidence.
        const auto weak = news_lang::detect(QStringLiteral("Nvidia Q3 Beat"));
        QCOMPARE(weak.code, QStringLiteral("en"));
        QVERIFY(weak.confidence < 0.5);
        QVERIFY(news_lang::sentiment_supported(QStringLiteral("en")));
        QVERIFY(!news_lang::sentiment_supported(QStringLiteral("fr")));
    }

    // ── Tickers ─────────────────────────────────────────────────────────────
    void tickers_explicit_forms() {
        const auto dict = news_tags::SymbolDictionary::builtin();
        auto syms = tnn_symbols(news_tags::tag(QStringLiteral("Reliance (NSE:RELIANCE) and $AAPL rally"), {}, dict));
        QVERIFY(syms.contains(QStringLiteral("RELIANCE.NS")));
        QVERIFY(syms.contains(QStringLiteral("AAPL")));

        syms = tnn_symbols(news_tags::tag(QStringLiteral("Toyota 7203.T and Tencent 0700.HK lead Asia"), {}, dict));
        QVERIFY(syms.contains(QStringLiteral("7203.T")));
        QVERIFY(syms.contains(QStringLiteral("0700.HK")));

        // HKG: 700 is padded to Yahoo's four-digit form.
        syms = tnn_symbols(news_tags::tag(QStringLiteral("Tencent (HKG: 700) buys back shares"), {}, dict));
        QVERIFY(syms.contains(QStringLiteral("0700.HK")));

        // Exchange-qualified class shares come out in Yahoo's form (the private suffix table
        // tagged "BRK.B", which Yahoo does not quote).
        syms = tnn_symbols(news_tags::tag(QStringLiteral("Berkshire (NYSE: BRK.B) adds to its stake"), {}, dict));
        QVERIFY(syms.contains(QStringLiteral("BRK-B")));
    }

    // The exchange -> Yahoo mapping now comes from the shared venue table
    // (core::instrument::to_yahoo); these are the prefixes the tagger's pattern accepts.
    void tickers_exchange_codes_data() {
        QTest::addColumn<QString>("exchange");
        QTest::addColumn<QString>("code");
        QTest::addColumn<QString>("expected");
        QTest::newRow("nse") << "NSE" << "RELIANCE" << "RELIANCE.NS";
        QTest::newRow("bse") << "BSE" << "500325" << "500325.BO";
        QTest::newRow("nasdaq-mixed-case") << "Nasdaq" << "AAPL" << "AAPL";
        QTest::newRow("nyse-class-share") << "NYSE" << "BRK.B" << "BRK-B";
        QTest::newRow("hkg-padded") << "HKG" << "700" << "0700.HK";
        QTest::newRow("tyo") << "TYO" << "7203" << "7203.T";
        QTest::newRow("tse-is-tokyo") << "TSE" << "7203" << "7203.T";
        QTest::newRow("tpe-padded") << "TPE" << "50" << "0050.TW";
        QTest::newRow("krx-padded") << "KRX" << "5930" << "005930.KS";
        QTest::newRow("kosdaq") << "KOSDAQ" << "035720" << "035720.KQ";
        QTest::newRow("sha-padded") << "SHA" << "600519" << "600519.SS";
        QTest::newRow("lon") << "LON" << "VOD" << "VOD.L";
        QTest::newRow("etr") << "ETR" << "SAP" << "SAP.DE";
        QTest::newRow("epa") << "EPA" << "MC" << "MC.PA";
        QTest::newRow("bit") << "BIT" << "ENI" << "ENI.MI";
        QTest::newRow("tsx") << "TSX" << "RY" << "RY.TO";
        QTest::newRow("asx") << "ASX" << "BHP" << "BHP.AX";
        QTest::newRow("klse") << "KLSE" << "1155" << "1155.KL";
        QTest::newRow("bovespa") << "BOVESPA" << "PETR4" << "PETR4.SA";
        QTest::newRow("tadawul") << "TADAWUL" << "2222" << "2222.SR";
        QTest::newRow("already-suffixed") << "LSE" << "VOD.L" << "VOD.L";
        QTest::newRow("unknown-venue") << "XYZQ" << "ABC" << "";
        QTest::newRow("empty-code") << "NSE" << "" << "";
    }
    void tickers_exchange_codes() {
        QFETCH(QString, exchange);
        QFETCH(QString, code);
        QFETCH(QString, expected);
        QCOMPARE(news_tags::yahoo_for_exchange_code(exchange, code), expected);
    }

    void tickers_aliases() {
        const auto dict = news_tags::SymbolDictionary::builtin();
        auto syms = tnn_symbols(news_tags::tag(QStringLiteral("Samsung Electronics profit jumps on chip demand"), {}, dict));
        QCOMPARE(syms.value(0), QStringLiteral("005930.KS"));
        syms = tnn_symbols(news_tags::tag(QString::fromUtf8("腾讯第三季度营收增长"), {}, dict));
        QCOMPARE(syms.value(0), QStringLiteral("0700.HK"));
        syms = tnn_symbols(news_tags::tag(QString::fromUtf8("贵州茅台股价下跌"), {}, dict));
        QCOMPARE(syms, QStringList{QStringLiteral("600519.SS")});
        // Ambiguous alias without finance context: no tag.
        syms = tnn_symbols(news_tags::tag(QStringLiteral("Travellers face new visa rules"), {}, dict));
        QVERIFY(!syms.contains(QStringLiteral("V")));
        syms = tnn_symbols(news_tags::tag(QStringLiteral("Visa shares rise after earnings beat"), {}, dict));
        QVERIFY(syms.contains(QStringLiteral("V")));
        // Hyphenated compounds and the longest alias win.
        syms = tnn_symbols(news_tags::tag(QStringLiteral("VW-Chef Blume will Rekordrendite erreichen"), {}, dict));
        QCOMPARE(syms.value(0), QStringLiteral("VOW3.DE"));
        syms = tnn_symbols(news_tags::tag(QStringLiteral("Siemens Energy and GE Vernova rally"), {}, dict));
        QVERIFY(syms.contains(QStringLiteral("ENR.DE")));
        QVERIFY(syms.contains(QStringLiteral("GEV")));
        QVERIFY(!syms.contains(QStringLiteral("SIE.DE")));
    }

    void tickers_no_guessing() {
        const auto dict = news_tags::SymbolDictionary::builtin();
        QStringList unverified;
        const auto tags =
            news_tags::tag(QStringLiteral("CEO of XYZQ says GDP and CPI data matter for the FED"), {}, dict, &unverified);
        QVERIFY(tags.isEmpty());
        QVERIFY(unverified.contains(QStringLiteral("XYZQ")));
        QVERIFY(!unverified.contains(QStringLiteral("GDP")));
        // Abbreviations and legal forms are not listings.
        QVERIFY(news_tags::tag(QStringLiteral("U.S. officials meet as Acme B.V. restructures"), {}, dict).isEmpty());
        // A shouted headline does not turn every word into a symbol.
        QVERIFY(news_tags::tag(QStringLiteral("MARKETS RALLY AS INFLATION COOLS ACROSS EUROPE"), {}, dict).isEmpty());
    }

    void tickers_user_symbols() {
        auto dict = news_tags::SymbolDictionary::builtin();
        dict.add_symbol(QStringLiteral("SAN.MC"), QStringLiteral("Banco Santander S.A."), true);
        dict.add_symbol(QStringLiteral("ZS"), QStringLiteral("Zscaler, Inc."), true);
        auto syms = tnn_symbols(news_tags::tag(QStringLiteral("ZS jumps as Zscaler guidance tops estimates"), {}, dict));
        QCOMPARE(syms.value(0), QStringLiteral("ZS"));
        QCOMPARE(dict.resolve(QStringLiteral("SAN")), QStringLiteral("SAN.MC"));
        QCOMPARE(news_tags::normalize_company_name(QStringLiteral("HSBC Holdings plc")), QStringLiteral("HSBC"));
    }

    // ── Analytics ───────────────────────────────────────────────────────────
    void velocity() {
        const qint64 now = 1'800'000'000;
        using news_analytics::Velocity;
        // Four in the last two hours, one in the six before: rising.
        QCOMPARE(news_analytics::cluster_velocity({now - 600, now - 1800, now - 3600, now - 5400, now - 4 * 3600}, now),
                 Velocity::Rising);
        // Old articles only (older than 2h, within 8h): falling.
        QCOMPARE(news_analytics::cluster_velocity({now - 3 * 3600, now - 4 * 3600, now - 5 * 3600}, now),
                 Velocity::Falling);
        // One recent, nothing before: not enough to call it rising.
        QCOMPARE(news_analytics::cluster_velocity({now - 600}, now), Velocity::Stable);
        // Articles older than 8 h are ignored entirely.
        QCOMPARE(news_analytics::cluster_velocity({now - 600, now - 900, now - 20 * 3600, now - 21 * 3600}, now),
                 Velocity::Rising);
    }

    void cjk_tokens_cluster() {
        const auto a = news_analytics::headline_tokens(QString::fromUtf8("英伟达股价大跌 芯片出口限制"));
        const auto b = news_analytics::headline_tokens(QString::fromUtf8("芯片出口限制引发英伟达股价大跌"));
        QVERIFY(news_analytics::jaccard(a, b) > 0.5);
        const auto e1 = news_analytics::headline_tokens(QStringLiteral("Fed cuts rates by 25 basis points"));
        QVERIFY(e1.contains(QStringLiteral("cuts")));
        QVERIFY(!e1.contains(QStringLiteral("by")));
        QCOMPARE(news_analytics::duplicate_key(QStringLiteral("Fed Cuts Rates — Live!")),
                 QStringLiteral("fed cuts rates live"));
    }

    void baselines() {
        QVector<int> counts;
        for (int i = 0; i < 24; ++i)
            counts.append(i % 2 == 0 ? 4 : 6);
        const auto b = news_analytics::baseline_of(counts);
        QCOMPARE(b.samples, 24);
        QCOMPARE(b.mean, 5.0);
        QCOMPARE(b.stddev, 1.0);
        QCOMPARE(news_analytics::zscore(9, b), 4.0);
        QVERIFY(std::isnan(news_analytics::zscore(9, news_analytics::baseline_of(counts.mid(0, 10)))));
    }

    void buckets_and_trending() {
        const qint64 t0 = 1'800'000'000 - (1'800'000'000 % 3600);
        QVector<news_analytics::Sample> samples = {{t0 + 10, 0.5, true, 1},
                                                   {t0 + 20, -0.3, true, -1},
                                                   {t0 + 3700, 0.0, false, 0}};
        const auto pts = news_analytics::bucketize(samples, t0, t0 + 2 * 3600, 3600);
        QCOMPARE(pts.size(), 2);
        QCOMPARE(pts[0].count, 2);
        QCOMPARE(pts[0].scored, 2);
        QVERIFY(std::abs(pts[0].mean_score - 0.1) < 1e-9);
        QCOMPARE(pts[1].count, 1);
        QVERIFY(std::isnan(pts[1].mean_score));

        const qint64 now = t0 + 7200;
        QVector<news_analytics::TrendInput> in = {
            {now - 100, {"AAPL"}, 0.4, true},       {now - 200, {"AAPL"}, 0.2, true},
            {now - 300, {"AAPL", "MSFT"}, 0.0, false}, {now - 4000, {"MSFT"}, 0.0, false},
            {now - 4100, {"MSFT"}, 0.0, false},     {now - 400, {"MSFT"}, -0.5, true},
        };
        const auto rows = news_analytics::trending(in, now, 3600, 10, 2);
        QCOMPARE(rows.size(), 2);
        QCOMPARE(rows[0].key, QStringLiteral("AAPL"));
        QCOMPARE(rows[0].current, 3);
        QCOMPARE(rows[0].previous, 0);
        QVERIFY(std::abs(rows[0].mean_score - 0.3) < 1e-9);
        QCOMPARE(rows[1].key, QStringLiteral("MSFT"));
        QCOMPARE(rows[1].previous, 2);
        QCOMPARE(news_analytics::change_pct(rows[1].current, rows[1].previous), 0.0);
        QVERIFY(std::isnan(news_analytics::change_pct(3, 0)));
    }
};

QTEST_GUILESS_MAIN(TestNewsNlp)
#include "tst_news_nlp.moc"
