// src/app/StartupPrewarm.h — header-only, zero app sources — and the start-up
// pre-warm in src/app/main.cpp that uses it.
//
// Owner's rule: nothing that costs credits runs automatically. The pre-warm asks
// producers for data before anyone looked at it, so it may only name topics
// declared free. A failure here means a billed topic (the economic calendar,
// geopolitics events, vessel data, …) is on its way back into the start-up
// requests, or main.cpp stopped filtering them: load it on the user's action
// instead (a LOAD / refresh button), never at start-up.

#include "app/StartupPrewarm.h"

#include <QFile>
#include <QRegularExpression>
#include <QString>
#include <QStringList>
#include <QTest>

#ifndef FINCEPT_SOURCE_DIR
#    error "FINCEPT_SOURCE_DIR (fincept-qt/) must be defined by tests/CMakeLists.txt"
#endif

using namespace fincept::startup_prewarm;

class TstStartupPrewarm : public QObject {
    Q_OBJECT

  private slots:
    void matches_like_the_hub();
    void quotes_and_news_are_free();
    void template_topics_are_free();
    void billed_topics_are_never_free();
    void undeclared_topics_are_not_free();
    void free_patterns_do_not_reach_billed_ones();
    void free_only_drops_what_is_not_free();
    void main_cpp_filters_the_prewarm();
};

namespace {

/// Topics of producers that call a billed Fincept route (one per billed pattern).
QStringList tsp_billed_samples() {
    return {QStringLiteral("econ:fincept:upcoming_events"), QStringLiteral("geopolitics:events"),
            QStringLiteral("geopolitics:countries"),        QStringLiteral("geopolitics:categories"),
            QStringLiteral("geopolitics:cities"),           QStringLiteral("maritime:vessel:9344745"),
            QStringLiteral("maritime:vessels:area"),        QStringLiteral("maritime:history:9344745")};
}

/// A topic a pattern matches: the pattern itself, or its prefix + "x".
QString tsp_sample_of(const QString& pattern) {
    return pattern.endsWith(QLatin1Char('*')) ? pattern.left(pattern.size() - 1) + QStringLiteral("x") : pattern;
}

const char* const kTspAppendPattern = "topics\\.append\\(\\s*QStringLiteral\\(\"([^\"]+)\"\\)";

} // namespace

void TstStartupPrewarm::matches_like_the_hub() {
    QVERIFY(topic_matches(QStringLiteral("market:quote:AAPL"), QStringLiteral("market:quote:*")));
    QVERIFY(topic_matches(QStringLiteral("market:quote:"), QStringLiteral("market:quote:*")));
    QVERIFY(!topic_matches(QStringLiteral("market:quote"), QStringLiteral("market:quote:*")));
    QVERIFY(!topic_matches(QStringLiteral("market:history:AAPL"), QStringLiteral("market:quote:*")));
    QVERIFY(topic_matches(QStringLiteral("news:general"), QStringLiteral("news:general")));
    QVERIFY(!topic_matches(QStringLiteral("news:general2"), QStringLiteral("news:general")));
    QVERIFY(!topic_matches(QStringLiteral("news:genera"), QStringLiteral("news:general")));
}

void TstStartupPrewarm::quotes_and_news_are_free() {
    for (const char* sym : {"^GSPC", "AAPL", "EURUSD=X", "BTC-USD", "GC=F"})
        QVERIFY2(is_free_topic(QStringLiteral("market:quote:") + QLatin1String(sym)), sym);
    QVERIFY(is_free_topic(QStringLiteral("news:general")));
}

void TstStartupPrewarm::template_topics_are_free() {
    const QStringList topics = default_template_topics();
    QVERIFY(!topics.isEmpty());
    for (const QString& t : topics)
        QVERIFY2(is_free_topic(t), qPrintable(t));
    QVERIFY(!topics.contains(QStringLiteral("econ:fincept:upcoming_events")));
}

void TstStartupPrewarm::billed_topics_are_never_free() {
    for (const QString& t : tsp_billed_samples()) {
        QVERIFY2(is_billed_topic(t), qPrintable(t));
        QVERIFY2(!is_free_topic(t), qPrintable(t));
    }
    // Every billed pattern is exercised by a sample above.
    for (const QString& p : billed_topic_patterns()) {
        bool covered = false;
        for (const QString& t : tsp_billed_samples())
            covered = covered || topic_matches(t, p);
        QVERIFY2(covered, qPrintable(p));
    }
}

void TstStartupPrewarm::undeclared_topics_are_not_free() {
    // Free only by declaration: a topic nobody has checked is not pre-warmed.
    for (const char* t : {"econ:fred:GDP", "dbnomics:series:x", "market:history:AAPL", "news:symbol:AAPL",
                          "maritime:health", "geopolitics:hdx:conflicts", "", "market:quote"})
        QVERIFY2(!is_free_topic(QString::fromLatin1(t)), t);
}

void TstStartupPrewarm::free_patterns_do_not_reach_billed_ones() {
    // A free pattern widened over a billed family ("econ:*", "geopolitics:*",
    // "maritime:*") is a declaration error even though is_free_topic() still says no.
    for (const QString& free : free_topic_patterns()) {
        for (const QString& billed : billed_topic_patterns()) {
            QVERIFY2(!topic_matches(tsp_sample_of(billed), free), qPrintable(free + QStringLiteral(" covers ") + billed));
        }
        for (const QString& t : tsp_billed_samples())
            QVERIFY2(!topic_matches(t, free), qPrintable(free + QStringLiteral(" covers ") + t));
    }
}

void TstStartupPrewarm::free_only_drops_what_is_not_free() {
    const QStringList in{QStringLiteral("market:quote:AAPL"), QStringLiteral("econ:fincept:upcoming_events"),
                         QStringLiteral("news:general"), QStringLiteral("geopolitics:events"),
                         QStringLiteral("something:new")};
    QStringList dropped;
    const QStringList out = free_only(in, &dropped);
    QCOMPARE(out, (QStringList{QStringLiteral("market:quote:AAPL"), QStringLiteral("news:general")}));
    QCOMPARE(dropped, (QStringList{QStringLiteral("econ:fincept:upcoming_events"), QStringLiteral("geopolitics:events"),
                                   QStringLiteral("something:new")}));
    QCOMPARE(free_only(in), out); // `dropped` is optional
    QVERIFY(free_only({}).isEmpty());
}

void TstStartupPrewarm::main_cpp_filters_the_prewarm() {
    QFile f(QStringLiteral(FINCEPT_SOURCE_DIR) + QStringLiteral("/src/app/main.cpp"));
    QVERIFY2(f.open(QIODevice::ReadOnly), qPrintable(f.fileName()));
    const QString src = QString::fromUtf8(f.readAll());

    // The pre-warm block: from its heading to the request it sends.
    const qsizetype begin = src.indexOf(QStringLiteral("Pre-warm the dashboard topics"));
    QVERIFY2(begin >= 0, "the pre-warm block heading moved: update this test with it");
    const qsizetype request = src.indexOf(QStringLiteral("hub.request(topics"), begin);
    QVERIFY2(request > begin, "the pre-warm request moved: update this test with it");
    const QString block = src.mid(begin, request - begin);

    // Everything it requests passes free_only() first ...
    QVERIFY2(block.contains(QStringLiteral("startup_prewarm::free_only(topics")),
             "main.cpp must pass the pre-warm list through startup_prewarm::free_only()");
    QVERIFY(block.contains(QStringLiteral("startup_prewarm::default_template_topics()")));
    // ... and no literal topic it appends is billed or undeclared.
    QVERIFY(!block.contains(QStringLiteral("econ:fincept")));
    const QRegularExpression append_re(QString::fromLatin1(kTspAppendPattern));
    auto it = append_re.globalMatch(block);
    while (it.hasNext()) {
        const QString topic = it.next().captured(1);
        QVERIFY2(is_free_topic(topic), qPrintable(QStringLiteral("main.cpp pre-warms ") + topic));
    }
    // The quote prefix it builds topics from is a free family.
    QVERIFY(block.contains(QStringLiteral("QStringLiteral(\"market:quote:\") + s")));
    QVERIFY(is_free_topic(QStringLiteral("market:quote:SPY")));
}

QTEST_GUILESS_MAIN(TstStartupPrewarm)
#include "tst_startup_prewarm.moc"
