// tst_amount_input.cpp — ui/formatting/AmountInput.h: the field rules on top of parse_amount() that
// replaced the private amount parsers of the M&A deal dialog (deal value / revenue / EBITDA, the
// multiples and percentages), the money columns of the data grids (M&A comparables, private-markets
// and cash-flow ledgers, alternative holdings) and the option pricer's discrete dividends:
//   * K / M / B / lakh / crore, symbols, accounting negatives, the user's own separators;
//   * an amount in ANOTHER currency than the field's is refused with both currencies named (never
//     converted); pounds typed into a pence field are brought to pence;
//   * a plain number refuses a currency, accepts a trailing "x" / "%";
//   * a stored value written back with amount_input_text() reads back exactly under every locale
//     (C-locale text "0.825" would read as 825 under a "1.234,5" locale).
// Qt Core only (NumberFormat.cpp + settings, as tst_amount_parse).

#include "ui/formatting/AmountInput.h"
#include "ui/formatting/FormatSettings.h"

#include <QTest>

#include <cmath>

namespace fmt = fincept::ui::formatting;

class TstAmountInput : public QObject {
    Q_OBJECT

  private slots:
    void init() {
        fmt::FormatSettings::instance().reset_to_defaults();
        fmt::FormatSettings::instance().set_locale(QLocale(QLocale::English, QLocale::UnitedStates));
    }

    void deal_amounts_in_the_field_currency() {
        // What the deal dialog's placeholder promises, and the forms people paste.
        QCOMPARE(*fmt::read_amount_input(QStringLiteral("68.7B"), QStringLiteral("USD")).value, 68.7e9);
        QCOMPARE(*fmt::read_amount_input(QStringLiteral("350M"), QStringLiteral("USD")).value, 350e6);
        QCOMPARE(*fmt::read_amount_input(QStringLiteral("$1,250,000"), QStringLiteral("USD")).value, 1250000.0);
        QCOMPARE(*fmt::read_amount_input(QStringLiteral("(2.5M)"), QStringLiteral("USD")).value, -2.5e6);
        QCOMPARE(*fmt::read_amount_input(QStringLiteral("USD 5bn"), QStringLiteral("USD")).value, 5e9);
        const fmt::AmountInput a = fmt::read_amount_input(QStringLiteral("5"), QStringLiteral("USD"));
        QVERIFY(a.ok());
        QCOMPARE(a.currency, QStringLiteral("USD"));
    }

    void other_currency_is_refused_not_converted() {
        const fmt::AmountInput a = fmt::read_amount_input(QStringLiteral("€5B"), QStringLiteral("USD"));
        QVERIFY(!a.ok());
        QVERIFY2(a.error.contains(QStringLiteral("EUR")) && a.error.contains(QStringLiteral("USD")), qPrintable(a.error));
        QVERIFY(a.error.contains(QStringLiteral("not converted")));
        // Pounds into a pence-quoted share's dividend field: the same money, in the field's unit.
        const fmt::AmountInput p = fmt::read_amount_input(QStringLiteral("£0.05"), QStringLiteral("GBp"));
        QVERIFY2(p.ok(), qPrintable(p.error));
        QCOMPARE(*p.value, 5.0);
        QCOMPARE(p.currency, QStringLiteral("GBp"));
        // A column without a fixed currency takes the one typed, as typed.
        const fmt::AmountInput any = fmt::read_amount_input(QStringLiteral("€5B"), QString());
        QVERIFY(any.ok());
        QCOMPARE(*any.value, 5e9);
        QCOMPARE(any.currency, QStringLiteral("EUR"));
    }

    void refusals_carry_the_reason() {
        for (const QString& t : {QStringLiteral("abc"), QStringLiteral("12%"), QString(), QStringLiteral("1 2")}) {
            const fmt::AmountInput a = fmt::read_amount_input(t, QStringLiteral("USD"));
            QVERIFY2(!a.ok(), qPrintable(t));
            QVERIFY2(!a.error.isEmpty(), qPrintable(t));
        }
    }

    void plain_numbers() {
        QCOMPARE(*fmt::read_number_input(QStringLiteral("8.7")).value, 8.7);
        QCOMPARE(*fmt::read_number_input(QStringLiteral("8.7x")).value, 8.7);
        QCOMPARE(*fmt::read_number_input(QStringLiteral("23.1 X")).value, 23.1);
        QCOMPARE(*fmt::read_number_input(QStringLiteral("45.3%")).value, 45.3);
        QCOMPARE(*fmt::read_number_input(QStringLiteral("45.3 %")).value, 45.3);
        QCOMPARE(*fmt::read_number_input(QStringLiteral("30")).value, 30.0);
        const fmt::AmountInput c = fmt::read_number_input(QStringLiteral("$8.7"));
        QVERIFY(!c.ok());
        QVERIFY(c.error.contains(QStringLiteral("without a currency")));
        QVERIFY(!fmt::read_number_input(QStringLiteral("eight")).ok());
    }

    void users_own_separators() {
        fmt::FormatSettings::instance().set_locale(QLocale(QLocale::German, QLocale::Germany));
        QCOMPARE(*fmt::read_amount_input(QStringLiteral("1.234,56 €"), QStringLiteral("EUR")).value, 1234.56);
        QCOMPARE(*fmt::read_amount_input(QStringLiteral("68,7B"), QStringLiteral("EUR")).value, 68.7e9);
        QCOMPARE(*fmt::read_number_input(QStringLiteral("8,7")).value, 8.7);
    }

    void stored_values_read_back_exactly() {
        const QList<QLocale> locales = {QLocale(QLocale::English, QLocale::UnitedStates),
                                        QLocale(QLocale::German, QLocale::Germany),
                                        QLocale(QLocale::French, QLocale::France),
                                        QLocale(QLocale::English, QLocale::India)};
        const QList<double> values = {0.825, 0.5, 1234.567, 68734000000.0, -2500000.0, 45.3, 30.0, 0.0001};
        QStringList failures;
        for (const QLocale& loc : locales) {
            fmt::FormatSettings::instance().set_locale(loc);
            for (const double v : values) {
                const QString shown = fmt::amount_input_text(v, 6);
                const fmt::AmountInput a = fmt::read_amount_input(shown, QStringLiteral("USD"));
                const fmt::AmountInput n = fmt::read_number_input(shown);
                if (!a.ok() || std::fabs(*a.value - v) > 1e-9 * std::max(1.0, std::fabs(v)))
                    failures << QStringLiteral("%1 amount '%2' -> %3").arg(loc.name(), shown, a.ok() ? QString::number(*a.value, 'g', 17) : a.error);
                if (!n.ok() || std::fabs(*n.value - v) > 1e-9 * std::max(1.0, std::fabs(v)))
                    failures << QStringLiteral("%1 number '%2' -> %3").arg(loc.name(), shown, n.ok() ? QString::number(*n.value, 'g', 17) : n.error);
            }
        }
        QVERIFY2(failures.isEmpty(), qPrintable(failures.join(QLatin1Char('\n'))));
        QCOMPARE(fmt::amount_input_text(std::nan("")), QString());
        // What the deal dialog shows for a stored deal value reads back (rounded to what it shows).
        fmt::FormatSettings::instance().set_locale(QLocale(QLocale::German, QLocale::Germany));
        const QString compact = fmt::money(68.734e9, QStringLiteral("USD"), fmt::Compact::FromSettings);
        const fmt::AmountInput back = fmt::read_amount_input(compact, QStringLiteral("USD"));
        QVERIFY2(back.ok(), qPrintable(compact + QStringLiteral(" -> ") + back.error));
        QVERIFY(std::fabs(*back.value - 68.734e9) < 0.01e9);
    }
};

QTEST_GUILESS_MAIN(TstAmountInput)
#include "tst_amount_input.moc"
