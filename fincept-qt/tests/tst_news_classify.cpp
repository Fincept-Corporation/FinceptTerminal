// Unit tests for the News tab's keyword classification (Qt Core only,
// header-only unit): src/services/news/NewsClassify.h — priority (FLASH /
// URGENT / BREAKING), market impact, event type and threat.
//
// Two sides: market-moving headlines that must be flagged, and ordinary stories
// full of generic trigger words ("emergency", "breaking", "probe", "fined",
// "settlement", "acquisition") that must stay ROUTINE / LOW.

#include "services/news/NewsClassify.h"

#include <QString>
#include <QTest>

using namespace fincept::services::news_classify;

namespace {

Result tnc_classify(const char* headline, const char* summary = "", int tier = 2, bool tickers = false,
                    double tone = 0.0) {
    Input in;
    in.headline = QString::fromUtf8(headline);
    in.summary = QString::fromUtf8(summary);
    in.tier = tier;
    in.has_tickers = tickers;
    in.tone = tone;
    in.tone_scored = tone != 0.0;
    return classify(in);
}

const char* tnc_prio(Prio p) {
    switch (p) {
        case Prio::Flash:
            return "FLASH";
        case Prio::Urgent:
            return "URGENT";
        case Prio::Breaking:
            return "BREAKING";
        case Prio::Routine:
            return "ROUTINE";
    }
    return "?";
}

const char* tnc_impact(Impact i) {
    return i == Impact::High ? "HIGH" : i == Impact::Medium ? "MEDIUM" : "LOW";
}

} // namespace

class TestNewsClassify : public QObject {
    Q_OBJECT

  private slots:
    void market_context_is_whole_words() {
        QVERIFY(market_terms(QStringLiteral("Fed raises rates")));
        QVERIFY(!market_terms(QStringLiteral("Fed up with potholes, residents act")));
        QVERIFY(!market_terms(QStringLiteral("Well-fed cattle fetch more at auction")));
        QVERIFY(market_terms(QStringLiteral("Treasury yields climb")));
        QVERIFY(market_terms(QStringLiteral("Firm raises $2bn")));
        QVERIFY(market_terms(QStringLiteral("Safaricom shares fell 3%")));
        QVERIFY(!market_terms(QStringLiteral("Minister shares road-safety plan")));
        QVERIFY(!market_terms(QStringLiteral("Euro 2028 draw announced")));
        QVERIFY(!market_terms(QStringLiteral("Back in stock: the toy of the year")));
    }

    void flags_market_moving_headlines_data() {
        QTest::addColumn<QString>("headline");
        QTest::addColumn<QString>("summary");
        QTest::addColumn<bool>("tickers");
        QTest::addColumn<double>("tone");
        QTest::addColumn<QString>("priority");
        QTest::addColumn<QString>("impact");
        QTest::addColumn<QString>("event");
        auto row = [](const char* name, const char* h, const char* s, bool t, double tone, const char* p,
                      const char* i, const char* e) {
            QTest::newRow(name) << QString::fromUtf8(h) << QString::fromUtf8(s) << t << tone << QString::fromLatin1(p)
                                << QString::fromLatin1(i) << QString::fromLatin1(e);
        };
        row("fed flash", "BREAKING: Fed cuts rates by 50 basis points in surprise move", "", false, 0.0, "FLASH",
            "HIGH", "RATES");
        row("fed emergency", "Fed holds emergency meeting as bank stocks slide", "", false, 0.0, "URGENT", "HIGH", "");
        row("opec emergency", "OPEC+ calls emergency meeting as oil prices slump", "", false, 0.0, "URGENT", "HIGH",
            "");
        row("market crash", "Stock market crash wipes $2 trillion off global shares", "", false, 0.0, "FLASH", "HIGH",
            "");
        row("circuit breakers", "ALERT: Circuit breakers halt trading on Nasdaq after 7% drop", "", false, 0.0,
            "FLASH", "HIGH", "");
        row("m&a", "Microsoft agrees to buy Activision for $69 billion", "", true, 0.0, "BREAKING", "HIGH", "M&A");
        row("earnings beat", "Apple beats estimates as iPhone revenue jumps", "", true, 0.3, "BREAKING", "MEDIUM",
            "EARNINGS");
        row("price target", "Goldman Sachs downgrades Tesla to sell, cuts price target", "", true, -0.2, "BREAKING",
            "MEDIUM", "RATING");
        row("cpi", "US CPI rises 0.4% in October, hotter than expected", "", false, 0.0, "BREAKING", "MEDIUM",
            "MACRO");
        row("bankruptcy", "Evergrande files for bankruptcy protection in New York", "", false, -0.6, "BREAKING",
            "HIGH", "CORPORATE");
        row("repo rate", "RBI keeps repo rate unchanged at 6.5%", "", false, 0.0, "BREAKING", "HIGH", "RATES");
        row("fined with context", "Tesla fined $2 billion by EU regulators", "", true, -0.4, "BREAKING", "MEDIUM",
            "LEGAL");
        row("tariffs", "US imposes new tariffs on Chinese steel imports", "", false, -0.3, "BREAKING", "MEDIUM",
            "POLICY");
        row("strong tone, wire", "Sensex plunges 1,200 points in worst session since 2020",
            "Investors dumped banking stocks.", false, -0.85, "ROUTINE", "HIGH", "");
    }

    void flags_market_moving_headlines() {
        QFETCH(QString, headline);
        QFETCH(QString, summary);
        QFETCH(bool, tickers);
        QFETCH(double, tone);
        QFETCH(QString, priority);
        QFETCH(QString, impact);
        QFETCH(QString, event);
        const Result r = tnc_classify(headline.toUtf8().constData(), summary.toUtf8().constData(), 2, tickers, tone);
        QCOMPARE(QString::fromLatin1(tnc_prio(r.priority)), priority);
        QCOMPARE(QString::fromLatin1(tnc_impact(r.impact)), impact);
        QCOMPARE(r.event_type, event);
    }

    void ordinary_stories_stay_routine_data() {
        QTest::addColumn<QString>("headline");
        QTest::addColumn<QString>("summary");
        QTest::addColumn<double>("tone");
        auto row = [](const char* name, const char* h, const char* s, double tone) {
            QTest::newRow(name) << QString::fromUtf8(h) << QString::fromUtf8(s) << tone;
        };
        // The QA example: tier-2 business feed, strongly negative tone, "emergency",
        // "fined", a GDP figure in the summary.
        row("kenya road safety", "Kenya declares road-safety emergency as crash deaths rise",
            "Road accidents cost the economy about 5% of GDP a year; police said motorists breaking the rules will "
            "be fined and an urgent probe has begun.",
            -0.85);
        row("record-breaking", "Record-breaking heatwave grips southern Europe", "", -0.3);
        row("breaking as a verb", "Breaking the cycle of poverty in rural Nairobi", "", 0.4);
        row("police probe", "Police probe deadly bus crash on Mombasa highway", "", -0.8);
        row("land acquisition", "Government compensates families over land acquisition for new bypass", "", 0.1);
        row("settlement", "Israeli settlement expansion draws UN criticism", "", -0.5);
        row("charged", "Man charged with assault after nightclub brawl", "", -0.7);
        row("hospital emergency", "Hospital emergency department waiting times hit record", "", -0.5);
        row("flood alert", "Weather service issues flood alert for coastal towns", "", -0.4);
        row("fed up", "Fed up with potholes, residents fill them in themselves", "", -0.2);
        row("storm downgraded", "Storm downgraded to tropical depression", "", 0.1);
        row("urgent appeal", "URGENT appeal for blood donors after highway pile-up", "", -0.6);
        row("pitch invasion", "Football club fined after pitch invasion", "", -0.4);
        row("circuit breaker fire", "Faulty circuit breaker blamed for apartment fire", "", -0.6);
        row("shares a plan", "Minister shares emergency plan for drought-hit farmers", "", -0.3);
    }

    void ordinary_stories_stay_routine() {
        QFETCH(QString, headline);
        QFETCH(QString, summary);
        QFETCH(double, tone);
        const Result r = tnc_classify(headline.toUtf8().constData(), summary.toUtf8().constData(), 2, false, tone);
        QCOMPARE(QString::fromLatin1(tnc_prio(r.priority)), QStringLiteral("ROUTINE"));
        QCOMPARE(QString::fromLatin1(tnc_impact(r.impact)), QStringLiteral("LOW"));
        QCOMPARE(r.event_type, QString());
    }

    void threat_words_are_whole_words() {
        QCOMPARE(threat(QStringLiteral("patriot fans celebrate title win")).level, Threat::Info);
        QCOMPARE(threat(QStringLiteral("hurricanes beat rangers 4-2 in overtime")).level, Threat::Info);
        QCOMPARE(threat(QStringLiteral("football club fined after pitch invasion")).level, Threat::Info);
        QCOMPARE(threat(QStringLiteral("embargoed until 09:00 gmt: quarterly survey")).level, Threat::Info);
        QCOMPARE(threat(QStringLiteral("faulty circuit breaker blamed for apartment fire")).level, Threat::Info);
        QCOMPARE(threat(QStringLiteral("riots erupt in the capital")).level, Threat::Medium);
        QCOMPARE(threat(QStringLiteral("magnitude 7.1 earthquake strikes off japan")).level, Threat::High);
        QCOMPARE(threat(QStringLiteral("stock market crash wipes out gains")).level, Threat::Critical);
        QCOMPARE(threat(QStringLiteral("circuit breakers halt trading on nasdaq")).level, Threat::Critical);
    }

    void one_company_crash_is_not_a_market_crash() {
        // Seen on screen: a single stock's "shares crash" lit the FLASH banner.
        Result r = tnc_classify("Tata Motors PV shares crash: What led to auto stock 8% ramming, Nifty laggard; "
                                "down 27% in 2026 - Buy, sell or hold?",
                                "", 2, true, -0.5);
        QVERIFY(r.priority != Prio::Flash);
        QVERIFY(r.priority != Prio::Urgent);
        r = tnc_classify("Asian markets crash as yen surges on surprise BOJ hike");
        QCOMPARE(r.priority, Prio::Flash);
        r = tnc_classify("Global stocks collapse after Lehman-style failure");
        QCOMPARE(r.priority, Prio::Flash);
    }

    void hypothetical_crashes_and_emergencies_are_not_flagged() {
        // Seen on screen: an evergreen investing piece lit the FLASH banner.
        const char* headlines[] = {
            "If the Stock Market Crashes, Warren Buffett Says This is the Best Move Investors Can Make",
            "Will the stock market crash in 2027?",
            "3 Stocks to Buy Before the Market Crashes",
            "IMF warns of market crash as debt piles up",
            "Could the Fed hold an emergency meeting this week?",
            "How to prepare your portfolio for a market crash",
        };
        for (const char* h : headlines) {
            const Result r = tnc_classify(h, "", 2, false, 0.1);
            QVERIFY2(r.priority != Prio::Flash && r.priority != Prio::Urgent, h);
        }
        // The publisher's own marker still counts.
        QCOMPARE(tnc_classify("BREAKING: Fed could hold emergency meeting, sources say").priority, Prio::Flash);
    }

    void tier_four_is_never_high() {
        const Result r = tnc_classify("BREAKING: Fed cuts rates by 50 basis points", "", 4);
        QCOMPARE(r.priority, Prio::Flash);
        QCOMPARE(r.impact, Impact::Medium);
    }

    void summary_events_need_a_market_headline() {
        // A market headline: the event named in the summary counts.
        Result r = tnc_classify("Stocks slip ahead of key data", "Investors await Friday's GDP figures.");
        QCOMPARE(r.event_type, QStringLiteral("MACRO"));
        QCOMPARE(r.impact, Impact::Medium);
        QCOMPARE(r.priority, Prio::Routine); // not in the headline: not BREAKING
        // A non-market headline: it does not.
        r = tnc_classify("Nairobi traffic deaths up sharply", "The toll is put at 5% of GDP.");
        QCOMPARE(r.event_type, QString());
        QCOMPARE(r.impact, Impact::Low);
    }
};

QTEST_GUILESS_MAIN(TestNewsClassify)
#include "tst_news_classify.moc"
