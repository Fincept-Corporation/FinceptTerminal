// Unit tests for src/ui/navigation/CommandMatch.h
//
// The command bar's screen matching. The defect these pin: typing a screen's
// full title with a space ("alt investments") found nothing — only the
// one-word aliases did — and "equity res…" was swallowed by the dock-verb
// parser instead of finding Equity Research.

#include "ui/navigation/CommandMatch.h"

#include <QTest>

namespace cm = fincept::ui::command_match;

namespace {
struct Screen {
    QString name;
    QString id;
    QStringList aliases;
    QStringList keywords;
    QString description;
};

const Screen kAlt{QStringLiteral("Alt. Investments"),
                  QStringLiteral("alt_investments"),
                  {QStringLiteral("altinv"), QStringLiteral("alt"), QStringLiteral("alternatives")},
                  {QStringLiteral("alternative"), QStringLiteral("investments"), QStringLiteral("hedge")},
                  QStringLiteral("Alternative investment analysis")};
const Screen kEquityResearch{QStringLiteral("Equity Research"),
                             QStringLiteral("equity_research"),
                             {QStringLiteral("research"), QStringLiteral("er")},
                             {QStringLiteral("fundamentals")},
                             QStringLiteral("Company research")};
const Screen kAsia{QStringLiteral("Asia Markets"),
                   QStringLiteral("asia_markets"),
                   {QStringLiteral("asia"), QStringLiteral("apac"), QStringLiteral("asian")},
                   {QStringLiteral("asia")},
                   QStringLiteral("Asian market data")};
const Screen kMa{QStringLiteral("M&A Analytics"),
                 QStringLiteral("ma_analytics"),
                 {QStringLiteral("ma"), QStringLiteral("mna"), QStringLiteral("mergers")},
                 {QStringLiteral("ma"), QStringLiteral("mergers"), QStringLiteral("acquisitions")},
                 QStringLiteral("Mergers and acquisitions")};

int score(const QString& q, const Screen& s) {
    return cm::score(q, s.name, s.id, s.aliases, s.keywords, s.description);
}
} // namespace

class TstCommandMatch : public QObject {
    Q_OBJECT

  private slots:
    void words_split_on_punctuation();
    void full_title_with_spaces_matches();
    void title_typed_any_way_matches();
    void aliases_rank_as_before();
    void partial_title_in_order();
    void any_order_words_match_below_title_threshold();
    void keywords_and_description_still_match();
    void no_match_and_empty();
    void ordering_prefers_title_over_keyword();
};

void TstCommandMatch::words_split_on_punctuation() {
    QCOMPARE(cm::words(QStringLiteral("M&A Analytics")),
             (QStringList{QStringLiteral("m"), QStringLiteral("a"), QStringLiteral("analytics")}));
    QCOMPARE(cm::words(QStringLiteral("  Alt.  Investments ")),
             (QStringList{QStringLiteral("alt"), QStringLiteral("investments")}));
    QCOMPARE(cm::words(QStringLiteral("alt_investments")),
             (QStringList{QStringLiteral("alt"), QStringLiteral("investments")}));
    QVERIFY(cm::words(QStringLiteral("&& -")).isEmpty());
}

void TstCommandMatch::full_title_with_spaces_matches() {
    QCOMPARE(score(QStringLiteral("alt investments"), kAlt), 95);
    QCOMPARE(score(QStringLiteral("Alt. Investments"), kAlt), 95);
    QCOMPARE(score(QStringLiteral("  ALT   INVESTMENTS  "), kAlt), 95);
    QCOMPARE(score(QStringLiteral("equity research"), kEquityResearch), 95);
    QVERIFY(score(QStringLiteral("alt investments"), kAlt) >= cm::kTitleMatch);
}

void TstCommandMatch::title_typed_any_way_matches() {
    QCOMPARE(score(QStringLiteral("altinvestments"), kAlt), 95); // no space
    QCOMPARE(score(QStringLiteral("alt inv"), kAlt), 90);        // start of the title
    QCOMPARE(score(QStringLiteral("m&a"), kMa), 90);
    QCOMPARE(score(QStringLiteral("m a analytics"), kMa), 95);
}

void TstCommandMatch::aliases_rank_as_before() {
    QCOMPARE(score(QStringLiteral("alt"), kAlt), 100);
    QCOMPARE(score(QStringLiteral("ALTINV"), kAlt), 100);
    QCOMPARE(score(QStringLiteral("alternatives"), kAlt), 100);
    QVERIFY(score(QStringLiteral("altern"), kAlt) >= 80); // alias prefix
}

void TstCommandMatch::partial_title_in_order() {
    // "equity res" is a title being typed, not "<equity> <dock verb>".
    QVERIFY(score(QStringLiteral("equity res"), kEquityResearch) >= cm::kTitleMatch);
    QVERIFY(score(QStringLiteral("equity r"), kEquityResearch) >= cm::kTitleMatch);
    QVERIFY(cm::prefixes_in_order({QStringLiteral("eq"), QStringLiteral("res")},
                                  {QStringLiteral("equity"), QStringLiteral("research")}));
    QVERIFY(!cm::prefixes_in_order({QStringLiteral("res"), QStringLiteral("eq")},
                                   {QStringLiteral("equity"), QStringLiteral("research")}));
}

void TstCommandMatch::any_order_words_match_below_title_threshold() {
    const int s = score(QStringLiteral("investments alt"), kAlt);
    QCOMPARE(s, 75);
    // "markets a" must stay the start of the dock command "markets add".
    QVERIFY(score(QStringLiteral("markets a"), kAsia) < cm::kTitleMatch);
    QVERIFY(score(QStringLiteral("markets a"), kAsia) > 0);
}

void TstCommandMatch::keywords_and_description_still_match() {
    QCOMPARE(score(QStringLiteral("hedge"), kAlt), 45);       // keyword word
    QCOMPARE(score(QStringLiteral("hedge alt"), kAlt), 45);   // keyword + title word
    QCOMPARE(score(QStringLiteral("analysis"), kAlt), 45);    // description word
    QCOMPARE(score(QStringLiteral("ment analy"), kAlt), 40);  // description substring
    QCOMPARE(score(QStringLiteral("ernat"), kAlt), 60);       // inside an alias
}

void TstCommandMatch::no_match_and_empty() {
    QCOMPARE(score(QStringLiteral("xyz"), kAlt), 0);
    QCOMPARE(score(QStringLiteral("alt xyz"), kAlt), 0);
    QCOMPARE(score(QString(), kAlt), 0);
    QCOMPARE(score(QStringLiteral("   "), kAlt), 0);
    QCOMPARE(score(QStringLiteral("&&"), kAlt), 0);
}

void TstCommandMatch::ordering_prefers_title_over_keyword() {
    // "investments" is a title word of Alt. Investments and only a description
    // word of nothing else here.
    QVERIFY(score(QStringLiteral("investments"), kAlt) > score(QStringLiteral("investments"), kMa));
    QVERIFY(score(QStringLiteral("research"), kEquityResearch) > score(QStringLiteral("research"), kAlt));
}

QTEST_GUILESS_MAIN(TstCommandMatch)
#include "tst_command_match.moc"
