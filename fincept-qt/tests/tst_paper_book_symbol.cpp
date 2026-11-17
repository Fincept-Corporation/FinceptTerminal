// Unit tests for src/trading/PaperBookSymbol.h — which row of a local paper book a symbol
// means. UnifiedTrading's close_position (the Equity screen / blotter exit) and
// place_smart_order (a smart order / workflow flatten on a local-paper account) both find a
// position with it, under the bare symbol (how the account path books every order) or
// "EXCHANGE:SYMBOL" (the session path, older smart orders), and then act on the row's own
// symbol. Before, the smart order looked for "EXCH:SYM" only and missed every position the
// account path had opened ("Position is zero" with the position still open).
//
// Header-only and pure (Qt Core). No .cpp, no database.

#include "trading/PaperBookSymbol.h"

#include <QString>
#include <QStringList>
#include <QTest>

namespace pb = fincept::trading::paper_book;

class TestPaperBookSymbol : public QObject {
    Q_OBJECT

  private slots:
    void bare_row_matches_with_or_without_exchange();
    void prefixed_row_matches_its_own_exchange_only();
    void exact_spelling_no_substring_no_case_fold();
    void prefixed_order_symbol_does_not_reach_a_bare_row();
    void same_rule_as_the_equity_exit_had();
};

void TestPaperBookSymbol::bare_row_matches_with_or_without_exchange() {
    // The account path (place_paper_order_for_account) stores the bare symbol: a workflow
    // flatten for RELIANCE on NSE must find it.
    QVERIFY(pb::symbol_matches(QStringLiteral("RELIANCE"), QStringLiteral("RELIANCE"), QStringLiteral("NSE")));
    QVERIFY(pb::symbol_matches(QStringLiteral("RELIANCE"), QStringLiteral("RELIANCE"), QString()));
    QVERIFY(pb::symbol_matches(QStringLiteral("NIFTY25NOV24000CE"), QStringLiteral("NIFTY25NOV24000CE"),
                               QStringLiteral("NFO")));
    QVERIFY(pb::symbol_matches(QStringLiteral("AAPL"), QStringLiteral("AAPL"), QStringLiteral("NASDAQ")));
}

void TestPaperBookSymbol::prefixed_row_matches_its_own_exchange_only() {
    // The session path and older smart orders stored "EXCH:SYM".
    QVERIFY(pb::symbol_matches(QStringLiteral("NSE:RELIANCE"), QStringLiteral("RELIANCE"), QStringLiteral("NSE")));
    QVERIFY(pb::symbol_matches(QStringLiteral("NFO:NIFTY25NOV24000CE"), QStringLiteral("NIFTY25NOV24000CE"),
                               QStringLiteral("NFO")));
    QVERIFY(!pb::symbol_matches(QStringLiteral("NSE:RELIANCE"), QStringLiteral("RELIANCE"), QStringLiteral("BSE")));
    QVERIFY(!pb::symbol_matches(QStringLiteral("NSE:RELIANCE"), QStringLiteral("RELIANCE"), QString()));
}

void TestPaperBookSymbol::exact_spelling_no_substring_no_case_fold() {
    // The engine nets by the exact stored string; so does the lookup.
    QVERIFY(!pb::symbol_matches(QStringLiteral("RELIANCEPP"), QStringLiteral("RELIANCE"), QStringLiteral("NSE")));
    QVERIFY(!pb::symbol_matches(QStringLiteral("RELIANCE"), QStringLiteral("RELIANCEPP"), QStringLiteral("NSE")));
    QVERIFY(!pb::symbol_matches(QStringLiteral("NSE:RELIANCEPP"), QStringLiteral("RELIANCE"), QStringLiteral("NSE")));
    QVERIFY(!pb::symbol_matches(QStringLiteral("reliance"), QStringLiteral("RELIANCE"), QStringLiteral("NSE")));
    QVERIFY(!pb::symbol_matches(QStringLiteral("nse:RELIANCE"), QStringLiteral("RELIANCE"), QStringLiteral("NSE")));
    QVERIFY(!pb::symbol_matches(QStringLiteral("RELIANCE-EQ"), QStringLiteral("RELIANCE"), QStringLiteral("NSE")));
    QVERIFY(!pb::symbol_matches(QStringLiteral("NIFTY"), QStringLiteral("NIFTY25NOV24000CE"), QStringLiteral("NFO")));
}

void TestPaperBookSymbol::prefixed_order_symbol_does_not_reach_a_bare_row() {
    // An order that already carries the prefix is booked under it; the bare row is another
    // position for the engine, and the rule does not pretend otherwise.
    QVERIFY(!pb::symbol_matches(QStringLiteral("RELIANCE"), QStringLiteral("NSE:RELIANCE"), QStringLiteral("NSE")));
    QVERIFY(pb::symbol_matches(QStringLiteral("NSE:RELIANCE"), QStringLiteral("NSE:RELIANCE"), QStringLiteral("NSE")));
}

void TestPaperBookSymbol::same_rule_as_the_equity_exit_had() {
    // close_position used `pos_sym == symbol || pos_sym == exchange + ":" + symbol` inline;
    // it now calls the helper. Same answer for every combination.
    const QStringList stored = {QStringLiteral("RELIANCE"), QStringLiteral("NSE:RELIANCE"),
                                QStringLiteral("BSE:RELIANCE"), QStringLiteral("RELIANCEPP"),
                                QStringLiteral(":RELIANCE"), QString(), QStringLiteral("NSE:NSE:RELIANCE")};
    const QStringList symbols = {QStringLiteral("RELIANCE"), QStringLiteral("NSE:RELIANCE"), QString()};
    const QStringList exchanges = {QStringLiteral("NSE"), QStringLiteral("BSE"), QString()};
    for (const QString& s : stored) {
        for (const QString& sym : symbols) {
            for (const QString& ex : exchanges) {
                const bool before = s == sym || s == ex + QStringLiteral(":") + sym;
                QVERIFY2(pb::symbol_matches(s, sym, ex) == before,
                         qPrintable(QStringLiteral("stored '%1' symbol '%2' exchange '%3'").arg(s, sym, ex)));
            }
        }
    }
}

QTEST_GUILESS_MAIN(TestPaperBookSymbol)
#include "tst_paper_book_symbol.moc"
