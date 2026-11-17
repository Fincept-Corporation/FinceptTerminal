// Unit tests for the Relationship Map's money text (header-only, Qt Core):
//   src/screens/relationship_map/RelationshipMapFormat.h   price_text / amount_text
// over the shared formatter (src/ui/formatting). The number locale and the compact
// scheme are pinned so the expectations do not depend on the machine.
//
// The defect these guard: a London company's market cap (reported by Yahoo in pounds)
// was labelled with the pence unit — a 100x error — and an empty or minor-unit code
// was upper-cased or read as USD.

#include "screens/relationship_map/RelationshipMapFormat.h"
#include "ui/formatting/FormatSettings.h"

#include <QLocale>
#include <QTest>

#include <cmath>

using namespace fincept;
namespace fmt = fincept::ui::formatting;

class TestRelmapFormat : public QObject {
    Q_OBJECT

  private slots:
    void initTestCase() {
        fmt::FormatSettings::instance().set_locale(QLocale(QLocale::English, QLocale::UnitedStates));
        fmt::FormatSettings::instance().set_compact_scheme(fmt::Compact::KMB);
    }

    // ── Prices: the quote currency, minor units kept ─────────────────────────
    void price_in_quote_currency() {
        QCOMPARE(relmap::price_text(189.3, QStringLiteral("USD")), QStringLiteral("$189.30"));
        QCOMPARE(relmap::price_text(2845.1, QStringLiteral("INR")), QString::fromUtf8("₹2,845.10"));
        // Pence stay pence, with the suffix unit.
        QCOMPARE(relmap::price_text(1234.5, QStringLiteral("GBp")), QStringLiteral("1,234.50p"));
    }

    void price_minor_unit_code_not_upper_cased() {
        // "ZAc" is South African cents; the old helper upper-cased it to the unknown "ZAC".
        const QString t = relmap::price_text(7355, QStringLiteral("ZAc"));
        QVERIFY2(!t.contains(QStringLiteral("ZAC")), qPrintable(t));
        QVERIFY2(t.endsWith(QLatin1Char('c')), qPrintable(t));
    }

    void price_unknown_currency_has_no_symbol() {
        // Never an assumed "$".
        QCOMPARE(relmap::price_text(189.3, QString()), QStringLiteral("189.30"));
        QCOMPARE(relmap::price_text(189.3, QStringLiteral("  ")), QStringLiteral("189.30"));
    }

    // ── Amounts: the major currency, abbreviated ─────────────────────────────
    void amount_of_pence_quoted_company_is_in_pounds() {
        const QString cap = relmap::amount_text(25e9, QStringLiteral("GBp"));
        QVERIFY2(cap.startsWith(QString::fromUtf8("£")), qPrintable(cap));
        QVERIFY2(!cap.contains(QLatin1Char('p')), qPrintable(cap));
        QVERIFY2(cap.endsWith(QLatin1Char('B')), qPrintable(cap));
        // Same for South African cents -> rand.
        const QString zar = relmap::amount_text(1e9, QStringLiteral("ZAc"));
        QVERIFY2(zar.startsWith(QLatin1Char('R')), qPrintable(zar));
    }

    void amount_compact_and_full() {
        QCOMPARE(relmap::amount_text(1.5e9, QStringLiteral("USD")), QStringLiteral("$1.50B"));
        QCOMPARE(relmap::amount_text(0.52, QStringLiteral("GBp"), /*compact=*/false), QString::fromUtf8("£0.52"));
    }

    void amount_unknown_currency_has_no_symbol() {
        QCOMPARE(relmap::amount_text(1.5e9, QString()), QStringLiteral("1.50B"));
        QCOMPARE(relmap::amount_text(1234.5, QString(), /*compact=*/false), QStringLiteral("1,234.50"));
    }

    void missing_values_print_the_placeholder() {
        QCOMPARE(relmap::price_text(std::nan(""), QStringLiteral("USD")), fmt::placeholder());
        QCOMPARE(relmap::amount_text(std::nan(""), QStringLiteral("GBp")), fmt::placeholder());
    }

    // ── Statement figures: the financial currency ────────────────────────────
    void statement_currency_prefers_the_financial_currency() {
        // A pence-quoted London listing that reports in dollars: revenue / EPS / pay in USD.
        QCOMPARE(relmap::statement_currency(QStringLiteral("USD"), QStringLiteral("GBp")), QStringLiteral("USD"));
        QCOMPARE(relmap::amount_text(2.5e9, relmap::statement_currency(QStringLiteral("USD"), QStringLiteral("GBp"))),
                 QStringLiteral("$2.50B"));
        // Unknown financial currency: the quote currency's major unit (never pence).
        QCOMPARE(relmap::statement_currency(QString(), QStringLiteral("GBp")), QStringLiteral("GBP"));
        QCOMPARE(relmap::statement_currency(QStringLiteral("  "), QStringLiteral("ZAc")), QStringLiteral("ZAR"));
        // Neither known: no symbol at all (never an assumed "$").
        QCOMPARE(relmap::statement_currency(QString(), QString()), QString());
        QCOMPARE(relmap::amount_text(1.5e9, relmap::statement_currency(QString(), QString())), QStringLiteral("1.50B"));
        // A minor-unit financial currency is still printed in its major unit.
        QCOMPARE(relmap::statement_currency(QStringLiteral("GBp"), QStringLiteral("USD")), QStringLiteral("GBP"));
    }

    void eps_in_the_financial_currency_keeps_decimals() {
        const QString eps = relmap::amount_text(1.234, relmap::statement_currency(QStringLiteral("USD"),
                                                                                  QStringLiteral("GBp")),
                                                /*compact=*/false);
        QCOMPARE(eps, QStringLiteral("$1.23"));
    }

    // ── Partial results: which missing sections a detail-panel node mentions ──
    void missing_sections_by_node_kind() {
        const QStringList missing = {QStringLiteral("institutional holders"), QStringLiteral("calendar"),
                                     QStringLiteral("peer MSFT"), QStringLiteral("peer NVDA"),
                                     QStringLiteral("recommendations"), QStringLiteral("insider transactions")};
        QCOMPARE(relmap::missing_for_kind(QStringLiteral("company"), missing), missing);
        QCOMPARE(relmap::missing_for_kind(QStringLiteral("institutional"), missing),
                 QStringList{QStringLiteral("institutional holders")});
        QVERIFY(relmap::missing_for_kind(QStringLiteral("mutualfund"), missing).isEmpty());
        QCOMPARE(relmap::missing_for_kind(QStringLiteral("peer"), missing),
                 (QStringList{QStringLiteral("peer MSFT"), QStringLiteral("peer NVDA")}));
        QCOMPARE(relmap::missing_for_kind(QStringLiteral("analyst"), missing),
                 QStringList{QStringLiteral("recommendations")});
        QCOMPARE(relmap::missing_for_kind(QStringLiteral("insider"), missing),
                 QStringList{QStringLiteral("insider transactions")});
        QCOMPARE(relmap::missing_for_kind(QStringLiteral("calendar"), missing), QStringList{QStringLiteral("calendar")});
        QVERIFY(relmap::missing_for_kind(QStringLiteral("metric"), missing).isEmpty());
        QVERIFY(relmap::missing_for_kind(QStringLiteral("company"), {}).isEmpty());
    }
};

QTEST_GUILESS_MAIN(TestRelmapFormat)
#include "tst_relmap_format.moc"
