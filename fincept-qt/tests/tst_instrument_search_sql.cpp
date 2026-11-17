// Unit tests for src/trading/instruments/InstrumentSearchSql.h — header-only,
// zero app sources.
//
// InstrumentRepository::search / search_all (the SQLite fallback for a broker
// whose catalogue is not in memory) used to pass the user's text straight into
// LIKE '%…%': '%' and '_' acted as wildcards, and even a one-letter query read
// every instrument row because a leading '%' cannot use an index. User wildcards
// are now escaped (ESCAPE '\') and short queries become a range on the symbol
// index.

#include "trading/instruments/InstrumentSearchSql.h"

#include <QStringList>
#include <QTest>

namespace iss = fincept::trading::instrument_search_sql;

class TstInstrumentSearchSql : public QObject {
    Q_OBJECT

  private slots:
    void wildcards_are_escaped() {
        QCOMPARE(iss::escape_like("RELIANCE"), QString("RELIANCE"));
        QCOMPARE(iss::escape_like("A_B"), QString("A\\_B"));
        QCOMPARE(iss::escape_like("50%"), QString("50\\%"));
        QCOMPARE(iss::escape_like("C\\D"), QString("C\\\\D")); // the escape character itself
        QCOMPARE(iss::escape_like("%_\\"), QString("\\%\\_\\\\"));
        QCOMPARE(iss::escape_like(QString()), QString());
    }

    void patterns() {
        QCOMPARE(iss::contains_pattern(" re_l "), QString("%RE\\_L%"));
        QCOMPARE(iss::contains_pattern("%"), QString("%\\%%")); // a lone '%' no longer matches every row
        QCOMPARE(iss::starts_with_pattern("nif"), QString("NIF%"));
        QCOMPARE(iss::starts_with_pattern("m&m_"), QString("M&M\\_%"));
    }

    void short_queries_are_prefix_only() {
        QVERIFY(iss::prefix_only("N"));
        QVERIFY(iss::prefix_only("RE"));
        QVERIFY(iss::prefix_only("  RE  "));
        QVERIFY(iss::prefix_only(""));
        QVERIFY(!iss::prefix_only("REL"));
        QVERIFY(!iss::prefix_only("NIFTY"));
        QCOMPARE(iss::kMinSubstringChars, 3);
    }

    void prefix_range_bounds() {
        QString lo, hi;
        QVERIFY(iss::prefix_range("NIF", lo, hi));
        QCOMPARE(lo, QString("NIF"));
        QCOMPARE(hi, QString("NIG"));

        QVERIFY(iss::prefix_range("Z", lo, hi));
        QCOMPARE(hi, QString("["));

        QVERIFY(iss::prefix_range("A~", lo, hi));
        QCOMPARE(hi, QString("A") + QChar(0x7F));

        // The increment must not produce a lone surrogate.
        QVERIFY(iss::prefix_range(QString(QChar(0xD7FF)), lo, hi));
        QCOMPARE(hi, QString(QChar(0xE000)));

        QVERIFY(!iss::prefix_range(QString(), lo, hi));
        QVERIFY(!iss::prefix_range(QString(QChar(0xFFFF)), lo, hi));
        QVERIFY(!iss::prefix_range(QString::fromUcs4(U"A\U0001F600"), lo, hi)); // surrogate pair
    }

    void prefix_range_selects_exactly_the_prefix() {
        // QString::compare is UTF-16 code-unit order, which for BMP text without
        // surrogates is the code-point (= SQLite BINARY / UTF-8 byte) order.
        const QStringList symbols = {"NI",          "NIE",        "NIEZZZ", "NIF",     "NIFTY",   "NIFTY24DECFUT",
                                     "NIFA",        "NIFZZZZZZ",  "NIG",    "NIGA",    "N",       "",
                                     "NIFé",   "NIF�",  "NIFTY~", "BANKNIFTY", "nifty", "NIF "};
        for (const QString& prefix : {QString("NIF"), QString("N"), QString("NIFTY")}) {
            QString lo, hi;
            QVERIFY(iss::prefix_range(prefix, lo, hi));
            for (const QString& s : symbols) {
                const bool in_range = s.compare(lo) >= 0 && s.compare(hi) < 0;
                QVERIFY2(in_range == s.startsWith(prefix), qPrintable(prefix + " / " + s));
            }
        }
    }
};

QTEST_GUILESS_MAIN(TstInstrumentSearchSql)
#include "tst_instrument_search_sql.moc"
