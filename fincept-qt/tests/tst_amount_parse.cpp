// Unit tests for fmt::parse_amount() (src/ui/formatting/ParseAmount.h, implemented
// in NumberFormat.cpp): the inverse of money().
//
// The headline property is the round trip: for every combination of the user's
// settings (number locale, symbol style / position / spacing, negative style,
// grouping, decimals policy, minor units), parse_amount(money(v, ccy), ccy) gives
// back ccy and the value as printed — money() of the result prints the same text.
// Then the cases people type: other separators, glued or spaced units,
// disambiguated symbols, pence, K / M / lakh / crore, accounting negatives,
// no-break spaces and bidi marks; and the refusals, each with a reason.

#include "ui/formatting/CurrencyTable.h"
#include "ui/formatting/FormatSettings.h"
#include "ui/formatting/NumberFormat.h"
#include "ui/formatting/ParseAmount.h"

#include <QTest>

#include <cmath>

namespace fmt = fincept::ui::formatting;

namespace {

fmt::FormatSettings& fs() {
    return fmt::FormatSettings::instance();
}

const QLocale kEnUs(QLocale::English, QLocale::UnitedStates);
const QLocale kDeDe(QLocale::German, QLocale::Germany);

// A typed / printed amount must parse to exactly `value` in `ccy`.
bool parses_to(const QString& text, double value, const QString& ccy, const QString& dflt = {}, QString* why = nullptr) {
    const fmt::ParsedAmount a = fmt::parse_amount(text, dflt);
    QString msg;
    if (!a.ok())
        msg = QStringLiteral("refused: %1").arg(a.error);
    else if (std::fabs(*a.value - value) > 1e-9 * std::max(1.0, std::fabs(value)))
        msg = QStringLiteral("value %1, expected %2").arg(*a.value, 0, 'g', 17).arg(value, 0, 'g', 17);
    else if (a.currency != ccy)
        msg = QStringLiteral("currency '%1', expected '%2'").arg(a.currency, ccy);
    if (why)
        *why = QStringLiteral("'%1' -> %2").arg(text, msg);
    return msg.isEmpty();
}

bool refused(const QString& text, const QString& dflt = {}, QString* why = nullptr) {
    const fmt::ParsedAmount a = fmt::parse_amount(text, dflt);
    if (why)
        *why = a.ok() ? QStringLiteral("'%1' parsed to %2 %3").arg(text).arg(*a.value).arg(a.currency) : a.error;
    return !a.ok() && !a.error.isEmpty();
}

// Digits after the locale's decimal point in a printed amount (0 when none).
int printed_decimals(const QString& printed) {
    const QString dp = fs().locale().decimalPoint();
    const qsizetype p = dp.isEmpty() ? -1 : printed.lastIndexOf(dp);
    if (p < 0)
        return 0;
    int n = 0;
    for (qsizetype i = p + dp.size(); i < printed.size() && printed.at(i).digitValue() >= 0; ++i)
        ++n;
    return n;
}

} // namespace

#define CHECK_PARSE(text, value, ccy, dflt)                                                                            \
    do {                                                                                                               \
        QString why_;                                                                                                  \
        QVERIFY2(parses_to(text, value, ccy, dflt, &why_), qPrintable(why_));                                          \
    } while (0)

#define CHECK_REFUSED(text, dflt)                                                                                      \
    do {                                                                                                               \
        QString why_;                                                                                                  \
        QVERIFY2(refused(text, dflt, &why_), qPrintable(why_));                                                        \
    } while (0)

class TstAmountParse : public QObject {
    Q_OBJECT

  private slots:
    void init();

    void plain_and_symbol_amounts();
    void negatives();
    void other_separators_and_locales();
    void ambiguous_symbols_follow_the_field();
    void minor_units();
    void size_suffixes();
    void overrides_and_custom_codes();
    void other_currency_is_reported();
    void refusals_have_reasons();

    void round_trip_every_setting();
    void round_trip_compact();
    void round_trip_signed_and_prices();
};

void TstAmountParse::init() {
    fs().reset_to_defaults();
    fs().set_locale(kEnUs);
    for (const QString& code : fs().currency_overrides().keys())
        fs().remove_currency_override(code);
}

void TstAmountParse::plain_and_symbol_amounts() {
    CHECK_PARSE(QStringLiteral("1234.5"), 1234.5, QString(), QString());
    CHECK_PARSE(QStringLiteral("1,234.50"), 1234.5, QStringLiteral("USD"), QStringLiteral("USD"));
    CHECK_PARSE(QStringLiteral("$1,234.50"), 1234.5, QStringLiteral("USD"), QString());
    CHECK_PARSE(QStringLiteral("$ 1,234.50"), 1234.5, QStringLiteral("USD"), QString());
    CHECK_PARSE(QStringLiteral("$\u00A01,234.50"), 1234.5, QStringLiteral("USD"), QString());
    CHECK_PARSE(QStringLiteral("  $1,234.50  "), 1234.5, QStringLiteral("USD"), QString());
    CHECK_PARSE(QStringLiteral("US$5"), 5, QStringLiteral("USD"), QString());
    CHECK_PARSE(QStringLiteral("C$5"), 5, QStringLiteral("CAD"), QString());
    CHECK_PARSE(QStringLiteral("A$5"), 5, QStringLiteral("AUD"), QString());
    CHECK_PARSE(QStringLiteral("CHF 5"), 5, QStringLiteral("CHF"), QString());
    CHECK_PARSE(QStringLiteral("CHF5"), 5, QStringLiteral("CHF"), QString());
    CHECK_PARSE(QStringLiteral("USD 1,234.50"), 1234.5, QStringLiteral("USD"), QString());
    CHECK_PARSE(QStringLiteral("1,234.50 USD"), 1234.5, QStringLiteral("USD"), QString());
    CHECK_PARSE(QStringLiteral("usd 10"), 10, QStringLiteral("USD"), QString());
    CHECK_PARSE(QStringLiteral("10 eur"), 10, QStringLiteral("EUR"), QString());
    CHECK_PARSE(QStringLiteral("\u20AC1.50"), 1.5, QStringLiteral("EUR"), QString());
    CHECK_PARSE(QStringLiteral("\u00A31,000"), 1000, QStringLiteral("GBP"), QString());
    CHECK_PARSE(QStringLiteral("\u20B912,34,567.89"), 1234567.89, QStringLiteral("INR"), QString());
    CHECK_PARSE(QStringLiteral("JP\u00A5500"), 500, QStringLiteral("JPY"), QString());
    CHECK_PARSE(QStringLiteral("CN\u00A5500"), 500, QStringLiteral("CNY"), QString());
    CHECK_PARSE(QStringLiteral("USDT 25.10"), 25.1, QStringLiteral("USDT"), QString());
    CHECK_PARSE(QStringLiteral(".5"), 0.5, QString(), QString());
    CHECK_PARSE(QStringLiteral("$.50"), 0.5, QStringLiteral("USD"), QString());
    CHECK_PARSE(QStringLiteral("\u2248 $13,183.00"), 13183, QStringLiteral("USD"), QString()); // converted_money()
    CHECK_PARSE(QStringLiteral("\u200F$5\u200E"), 5, QStringLiteral("USD"), QString());          // bidi marks
    CHECK_PARSE(QStringLiteral("0"), 0, QStringLiteral("EUR"), QStringLiteral("eur"));            // canonical default
    const fmt::ParsedAmount a = fmt::parse_amount(QStringLiteral("$5"));
    QVERIFY(a.currency_in_text);
    QVERIFY(!fmt::parse_amount(QStringLiteral("5"), QStringLiteral("USD")).currency_in_text);
}

void TstAmountParse::negatives() {
    CHECK_PARSE(QStringLiteral("-$1,234.50"), -1234.5, QStringLiteral("USD"), QString());
    CHECK_PARSE(QStringLiteral("$-1,234.50"), -1234.5, QStringLiteral("USD"), QString());
    CHECK_PARSE(QStringLiteral("($1,234.50)"), -1234.5, QStringLiteral("USD"), QString());
    CHECK_PARSE(QStringLiteral("(1,234.50)"), -1234.5, QString(), QString());
    CHECK_PARSE(QStringLiteral("1,234.50-"), -1234.5, QString(), QString());
    CHECK_PARSE(QStringLiteral("1,234.50 USD-"), -1234.5, QStringLiteral("USD"), QString());
    CHECK_PARSE(QStringLiteral("\u22121,234.50"), -1234.5, QString(), QString()); // U+2212 minus
    CHECK_PARSE(QStringLiteral("+$5"), 5, QStringLiteral("USD"), QString());
    CHECK_PARSE(QStringLiteral("-0.00"), 0, QString(), QString());
    QVERIFY(!std::signbit(*fmt::parse_amount(QStringLiteral("-0")).value)); // never -0
}

void TstAmountParse::other_separators_and_locales() {
    // en_US locale: the common foreign forms still read one way.
    CHECK_PARSE(QStringLiteral("1.234,56"), 1234.56, QString(), QString());
    CHECK_PARSE(QStringLiteral("1 234.56"), 1234.56, QString(), QString());
    CHECK_PARSE(QStringLiteral("1'234.56"), 1234.56, QString(), QString());
    CHECK_PARSE(QStringLiteral("1\u2019234.56"), 1234.56, QString(), QString());
    CHECK_PARSE(QStringLiteral("12,34,567.89"), 1234567.89, QString(), QString()); // Indian grouping
    CHECK_PARSE(QStringLiteral("1,234"), 1234, QString(), QString());              // locale grouping, 3 digits
    CHECK_PARSE(QStringLiteral("1,5"), 1.5, QString(), QString());                 // cannot be grouping
    CHECK_PARSE(QStringLiteral("0,123"), 0.123, QString(), QString());             // a leading 0 is not grouped
    CHECK_PARSE(QStringLiteral("1,2345"), 1.2345, QString(), QString());           // not 3 digits: not grouping
    CHECK_PARSE(QStringLiteral("1234567"), 1234567, QString(), QString());

    fs().set_locale(kDeDe);
    CHECK_PARSE(QStringLiteral("1.234,56 \u20AC"), 1234.56, QStringLiteral("EUR"), QString());
    CHECK_PARSE(QStringLiteral("1234,56"), 1234.56, QString(), QString());
    CHECK_PARSE(QStringLiteral("1.234"), 1234, QString(), QString()); // the locale's grouping
    CHECK_PARSE(QStringLiteral("1,5"), 1.5, QString(), QString());
    CHECK_PARSE(QStringLiteral("1.5"), 1.5, QString(), QString());    // cannot be grouping
    CHECK_PARSE(QStringLiteral("1,234.56"), 1234.56, QString(), QString());

    fs().set_locale(QLocale(QLocale::French, QLocale::France));
    CHECK_PARSE(QStringLiteral("1\u202F234,56\u00A0\u20AC"), 1234.56, QStringLiteral("EUR"), QString());
    CHECK_PARSE(QStringLiteral("1.5"), 1.5, QString(), QString());
    CHECK_REFUSED(QStringLiteral("1.500"), QString()); // '.' is neither the decimal point nor grouping here

    fs().set_locale(QLocale(QLocale::German, QLocale::Switzerland));
    CHECK_PARSE(QStringLiteral("CHF 1\u2019234.56"), 1234.56, QStringLiteral("CHF"), QString());

    fs().set_locale(QLocale(QLocale::Swedish, QLocale::Sweden));
    CHECK_PARSE(QStringLiteral("\u22121\u00A0234,56 kr"), -1234.56, QStringLiteral("SEK"), QStringLiteral("SEK"));

    fs().set_locale(QLocale(QLocale::Arabic, QLocale::Egypt));
    CHECK_PARSE(QStringLiteral("\u0661\u066C\u0662\u0663\u0664\u066B\u0665\u0660"), 1234.5, QString(), QString());

    fs().set_locale(kEnUs);
}

void TstAmountParse::ambiguous_symbols_follow_the_field() {
    // "$" is US dollar by itself, the field's own dollar in a dollar field.
    CHECK_PARSE(QStringLiteral("$5"), 5, QStringLiteral("USD"), QString());
    CHECK_PARSE(QStringLiteral("$5"), 5, QStringLiteral("CAD"), QStringLiteral("CAD"));
    CHECK_PARSE(QStringLiteral("$5"), 5, QStringLiteral("AUD"), QStringLiteral("AUD"));
    // "¥" is the yen by itself, the yuan in a yuan field.
    CHECK_PARSE(QStringLiteral("\u00A5500"), 500, QStringLiteral("JPY"), QString());
    CHECK_PARSE(QStringLiteral("\u00A5500"), 500, QStringLiteral("CNY"), QStringLiteral("CNY"));
    // "kr": the field's krone, else the display currency, else refused.
    CHECK_PARSE(QStringLiteral("5 kr"), 5, QStringLiteral("NOK"), QStringLiteral("NOK"));
    QString why;
    QVERIFY2(refused(QStringLiteral("5 kr"), QString(), &why), qPrintable(why));
    QVERIFY2(why.contains(QStringLiteral("SEK")) && why.contains(QStringLiteral("NOK")), qPrintable(why));
    QVERIFY(fs().set_display_currency(QStringLiteral("DKK")));
    CHECK_PARSE(QStringLiteral("5 kr"), 5, QStringLiteral("DKK"), QString());
    QVERIFY(fs().set_display_currency(QStringLiteral("CAD")));
    CHECK_PARSE(QStringLiteral("$5"), 5, QStringLiteral("CAD"), QString()); // the display currency's dollar
    CHECK_PARSE(QStringLiteral("US$5"), 5, QStringLiteral("USD"), QString());
}

void TstAmountParse::minor_units() {
    CHECK_PARSE(QStringLiteral("117.70p"), 117.7, QStringLiteral("GBp"), QString());
    CHECK_PARSE(QStringLiteral("117.70p"), 117.7, QStringLiteral("GBX"), QStringLiteral("GBX"));
    CHECK_PARSE(QStringLiteral("117.70p"), 1.177, QStringLiteral("GBP"), QStringLiteral("GBP")); // pence -> pounds
    CHECK_PARSE(QStringLiteral("\u00A31.1770"), 117.7, QStringLiteral("GBp"), QStringLiteral("GBp")); // pounds -> pence
    CHECK_PARSE(QStringLiteral("GBp 117.70"), 117.7, QStringLiteral("GBp"), QString());
    CHECK_PARSE(QStringLiteral("45\u00A2"), 45, QStringLiteral("USX"), QString());
    CHECK_PARSE(QStringLiteral("$4.485"), 448.5, QStringLiteral("USX"), QStringLiteral("USX"));
    CHECK_PARSE(QStringLiteral("735c"), 735, QStringLiteral("ZAc"), QString());
    CHECK_PARSE(QStringLiteral("1,234 ag."), 1234, QStringLiteral("ILA"), QString());
    CHECK_PARSE(QStringLiteral("12 fils"), 12, QStringLiteral("KWF"), QString());
}

void TstAmountParse::size_suffixes() {
    CHECK_PARSE(QStringLiteral("1.5K"), 1500, QString(), QString());
    CHECK_PARSE(QStringLiteral("1.5k"), 1500, QString(), QString());
    CHECK_PARSE(QStringLiteral("12.35M"), 12.35e6, QString(), QString());
    CHECK_PARSE(QStringLiteral("1.23B"), 1.23e9, QString(), QString());
    CHECK_PARSE(QStringLiteral("4.56T"), 4.56e12, QString(), QString());
    CHECK_PARSE(QStringLiteral("1.50 L"), 150000, QString(), QString());
    CHECK_PARSE(QStringLiteral("2.35 Cr"), 23.5e6, QString(), QString());
    CHECK_PARSE(QStringLiteral("1.20 L Cr"), 1.2e12, QString(), QString());
    CHECK_PARSE(QStringLiteral("3 lakh"), 300000, QString(), QString());
    CHECK_PARSE(QStringLiteral("2 crore"), 2e7, QString(), QString());
    CHECK_PARSE(QStringLiteral("1.5 bn"), 1.5e9, QString(), QString());
    CHECK_PARSE(QStringLiteral("$1.5M"), 1.5e6, QStringLiteral("USD"), QString());
    CHECK_PARSE(QStringLiteral("-$1.5M"), -1.5e6, QStringLiteral("USD"), QString());
    CHECK_PARSE(QStringLiteral("\u20B92.35 Cr"), 23.5e6, QStringLiteral("INR"), QString());
    CHECK_PARSE(QStringLiteral("\u00A315.0K"), 1.5e6, QStringLiteral("GBp"), QStringLiteral("GBp")); // major, abbreviated
    CHECK_PARSE(QStringLiteral("1,5K \u20AC"), 1500, QStringLiteral("EUR"), QString()); // en_US: "1,5" is 1.5, then K
    // "K" is also the kyat's symbol: a size unless it is the field's currency and ends the text.
    CHECK_PARSE(QStringLiteral("K1.5K"), 1500, QStringLiteral("MMK"), QString());
    CHECK_PARSE(QStringLiteral("1,234.50 K"), 1234.5, QStringLiteral("MMK"), QStringLiteral("MMK"));
    CHECK_PARSE(QStringLiteral("1.5K K"), 1500, QStringLiteral("MMK"), QStringLiteral("MMK"));
    CHECK_REFUSED(QStringLiteral("1.5 Mio"), QString());
    CHECK_REFUSED(QStringLiteral("1.5KM"), QString());
    CHECK_REFUSED(QStringLiteral("1.5K M"), QString()); // two sizes
}

void TstAmountParse::overrides_and_custom_codes() {
    QVERIFY(fs().set_currency_override(QStringLiteral("INR"), fmt::CurrencyOverride{QStringLiteral("Rs."), -1, {}}));
    CHECK_PARSE(QStringLiteral("Rs.1,234.50"), 1234.5, QStringLiteral("INR"), QString());
    CHECK_PARSE(QStringLiteral("Rs. 1,234.50"), 1234.5, QStringLiteral("INR"), QString());
    QVERIFY(fs().set_currency_override(QStringLiteral("XYZ"),
                                       fmt::CurrencyOverride{QStringLiteral("\u04FE"), 4, QStringLiteral("My token")}));
    CHECK_PARSE(QStringLiteral("\u04FE10.0000"), 10, QStringLiteral("XYZ"), QString());
    CHECK_PARSE(QStringLiteral("XYZ 10"), 10, QStringLiteral("XYZ"), QString());
    // An unknown code money() prints as-is reads back in a field of that code.
    CHECK_PARSE(QStringLiteral("QQQ 10.00"), 10, QStringLiteral("QQQ"), QStringLiteral("QQQ"));
}

void TstAmountParse::other_currency_is_reported() {
    const fmt::ParsedAmount a = fmt::parse_amount(QStringLiteral("\u20AC5"), QStringLiteral("USD"));
    QVERIFY(a.ok());
    QCOMPARE(*a.value, 5.0);
    QCOMPARE(a.currency, QStringLiteral("EUR")); // the caller converts or refuses
    QVERIFY(a.currency_in_text);
}

void TstAmountParse::refusals_have_reasons() {
    CHECK_REFUSED(QString(), QString());
    CHECK_REFUSED(QStringLiteral("   "), QString());
    CHECK_REFUSED(fmt::placeholder(), QString());
    CHECK_REFUSED(QStringLiteral("12%"), QString());
    CHECK_REFUSED(QStringLiteral("12 %"), QString());
    CHECK_REFUSED(QStringLiteral("abc"), QString());
    CHECK_REFUSED(QStringLiteral("nan"), QString());
    CHECK_REFUSED(QStringLiteral("1e6"), QString());
    CHECK_REFUSED(QStringLiteral("1 2"), QString());          // two numbers
    CHECK_REFUSED(QStringLiteral("1,23,4567"), QString());    // grouping that is neither western nor Indian
    CHECK_REFUSED(QStringLiteral("1.234.56"), QString());
    CHECK_REFUSED(QStringLiteral("1,234.567,8"), QString());
    CHECK_REFUSED(QStringLiteral("1,,234"), QString());
    CHECK_REFUSED(QStringLiteral("1,234."), QString());
    CHECK_REFUSED(QStringLiteral("$5 \u20AC"), QString());     // two currencies
    CHECK_REFUSED(QStringLiteral("--5"), QString());          // two signs
    CHECK_REFUSED(QStringLiteral("-(5)"), QString());
    CHECK_REFUSED(QStringLiteral("(-5)"), QString());
    CHECK_REFUSED(QStringLiteral("(5"), QString());
    CHECK_REFUSED(QStringLiteral("5 apples"), QString());
    CHECK_REFUSED(QStringLiteral("$"), QString());
    QString why;
    QVERIFY(refused(QStringLiteral("12%"), QString(), &why));
    QVERIFY2(why.contains(QStringLiteral("percentage")), qPrintable(why));
}

void TstAmountParse::round_trip_every_setting() {
    const QList<QLocale> locales = {
        kEnUs,
        kDeDe,
        QLocale(QLocale::French, QLocale::France),
        QLocale(QLocale::German, QLocale::Switzerland),
        QLocale(QLocale::English, QLocale::India),
        QLocale(QLocale::Japanese, QLocale::Japan),
        QLocale(QLocale::Swedish, QLocale::Sweden),
        QLocale(QLocale::Arabic, QLocale::Egypt),
    };
    const QStringList ccys = {
        QStringLiteral("USD"), QStringLiteral("EUR"), QStringLiteral("GBP"), QStringLiteral("GBp"),
        QStringLiteral("JPY"), QStringLiteral("CNY"), QStringLiteral("INR"), QStringLiteral("CHF"),
        QStringLiteral("SEK"), QStringLiteral("CAD"), QStringLiteral("KWD"), QStringLiteral("BTC"),
        QStringLiteral("USDT"), QStringLiteral("ZAc"), QStringLiteral("ILA"), QStringLiteral("KWF"),
        QStringLiteral("USX"), QStringLiteral("MMK"), QStringLiteral("QQQ"), // QQQ: not a currency code
    };
    const QList<double> values = {0.0, 1234.5, -1234.5, 0.07, 1234567.891, -0.004, 98765.4321};
    const fmt::SymbolStyle styles[] = {fmt::SymbolStyle::Symbol, fmt::SymbolStyle::Code,
                                       fmt::SymbolStyle::Disambiguated};
    const fmt::SymbolPosition positions[] = {fmt::SymbolPosition::Locale, fmt::SymbolPosition::Before,
                                             fmt::SymbolPosition::After};
    const fmt::SymbolSpacing spacings[] = {fmt::SymbolSpacing::Auto, fmt::SymbolSpacing::None,
                                           fmt::SymbolSpacing::Space};
    const fmt::NegativeStyle negs[] = {fmt::NegativeStyle::Minus, fmt::NegativeStyle::Parentheses};
    const fmt::Grouping groupings[] = {fmt::Grouping::Always, fmt::Grouping::Locale, fmt::Grouping::Indian,
                                       fmt::Grouping::None};
    struct Dec {
        fmt::DecimalsPolicy policy;
        int fixed;
    };
    const Dec decs[] = {{fmt::DecimalsPolicy::CurrencyDefault, 2},
                        {fmt::DecimalsPolicy::Adaptive, 2},
                        {fmt::DecimalsPolicy::Fixed, 0},
                        {fmt::DecimalsPolicy::Fixed, 4}};

    int combos = 0;
    int checked = 0;
    int failures = 0;
    QStringList first_failures;
    for (const QLocale& loc : locales) {
        for (const auto st : styles) {
            for (const auto pos : positions) {
                for (const auto sp : spacings) {
                    for (const auto neg : negs) {
                        for (const auto grp : groupings) {
                            const Dec dec = decs[combos % 4];
                            const fmt::MinorUnits mu = (combos / 4) % 2 ? fmt::MinorUnits::Major : fmt::MinorUnits::AsQuoted;
                            ++combos;
                            fmt::FormatSettings::Values v = fs().values();
                            v.locale_follows_app = false;
                            v.pinned_locale = loc;
                            v.symbol_style = st;
                            v.symbol_position = pos;
                            v.symbol_spacing = sp;
                            v.negative_style = neg;
                            v.grouping = grp;
                            v.decimals_policy = dec.policy;
                            v.fixed_decimals = dec.fixed;
                            v.minor_units = mu;
                            fs().set_values(v);
                            for (const QString& ccy : ccys) {
                                const int div = fmt::minor_unit_divisor(ccy);
                                for (const double x : values) {
                                    ++checked;
                                    const QString printed = fmt::money(x, ccy);
                                    const fmt::ParsedAmount a = fmt::parse_amount(printed, ccy);
                                    QString err;
                                    if (!a.ok()) {
                                        err = QStringLiteral("refused: ") + a.error;
                                    } else if (a.currency != ccy) {
                                        err = QStringLiteral("currency ") + a.currency;
                                    } else if (fmt::money(*a.value, ccy) != printed) {
                                        err = QStringLiteral("reprints as '%1'").arg(fmt::money(*a.value, ccy));
                                    } else {
                                        double tol = 0.5 * std::pow(10.0, -printed_decimals(printed));
                                        if (mu == fmt::MinorUnits::Major && div > 1)
                                            tol *= div; // printed in the major unit
                                        tol += 1e-9 * std::fabs(x);
                                        if (std::fabs(*a.value - x) > tol)
                                            err = QStringLiteral("value %1 (tolerance %2)").arg(*a.value).arg(tol);
                                    }
                                    if (!err.isEmpty()) {
                                        ++failures;
                                        if (first_failures.size() < 25)
                                            first_failures << QStringLiteral("%1 st%2 pos%3 sp%4 neg%5 grp%6 dec%7/%8 mu%9: "
                                                                             "%10 %11 -> '%12' -> %13")
                                                                  .arg(loc.name())
                                                                  .arg(int(st))
                                                                  .arg(int(pos))
                                                                  .arg(int(sp))
                                                                  .arg(int(neg))
                                                                  .arg(int(grp))
                                                                  .arg(int(dec.policy))
                                                                  .arg(dec.fixed)
                                                                  .arg(int(mu))
                                                                  .arg(ccy)
                                                                  .arg(x, 0, 'g', 12)
                                                                  .arg(printed, err);
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    QVERIFY2(failures == 0, qPrintable(QStringLiteral("%1 of %2 round trips failed:\n").arg(failures).arg(checked) +
                                       first_failures.join(QLatin1Char('\n'))));
    QVERIFY(checked > 200000);
}

void TstAmountParse::round_trip_compact() {
    const QList<QLocale> locales = {kEnUs, kDeDe, QLocale(QLocale::French, QLocale::France),
                                    QLocale(QLocale::English, QLocale::India)};
    const QStringList ccys = {QStringLiteral("USD"), QStringLiteral("EUR"), QStringLiteral("GBp"),
                              QStringLiteral("INR"), QStringLiteral("JPY"), QStringLiteral("CHF"),
                              QStringLiteral("MMK"), QString()};
    const QList<double> values = {999.0, 1549.0, 999950.0, 12345678.9, -2.35e7, 1.2e12, -480000.0};
    const fmt::Compact schemes[] = {fmt::Compact::KMB, fmt::Compact::LakhCrore};
    const fmt::SymbolStyle styles[] = {fmt::SymbolStyle::Symbol, fmt::SymbolStyle::Code,
                                       fmt::SymbolStyle::Disambiguated};
    QStringList failures;
    for (const QLocale& loc : locales) {
        for (const auto scheme : schemes) {
            for (const auto st : styles) {
                for (const auto neg : {fmt::NegativeStyle::Minus, fmt::NegativeStyle::Parentheses}) {
                    fmt::FormatSettings::Values v = fs().values();
                    v.locale_follows_app = false;
                    v.pinned_locale = loc;
                    v.symbol_style = st;
                    v.negative_style = neg;
                    v.compact = scheme;
                    fs().set_values(v);
                    for (const QString& ccy : ccys) {
                        for (const double x : values) {
                            const QString printed = fmt::money(x, ccy, fmt::Compact::FromSettings);
                            const fmt::ParsedAmount a = fmt::parse_amount(printed, ccy);
                            if (!a.ok())
                                failures << QStringLiteral("%1 '%2': %3").arg(loc.name(), printed, a.error);
                            else if (fmt::money(*a.value, ccy, fmt::Compact::FromSettings) != printed ||
                                     a.currency != ccy)
                                failures << QStringLiteral("%1 '%2' -> %3 %4 -> '%5'")
                                                .arg(loc.name(), printed)
                                                .arg(*a.value, 0, 'g', 12)
                                                .arg(a.currency, fmt::money(*a.value, ccy, fmt::Compact::FromSettings));
                        }
                    }
                }
            }
        }
    }
    QVERIFY2(failures.isEmpty(), qPrintable(failures.mid(0, 25).join(QLatin1Char('\n'))));
}

void TstAmountParse::round_trip_signed_and_prices() {
    QStringList failures;
    for (const QLocale& loc : {kEnUs, kDeDe, QLocale(QLocale::Swedish, QLocale::Sweden)}) {
        fs().set_locale(loc);
        for (const QString& ccy : {QStringLiteral("USD"), QStringLiteral("EUR"), QStringLiteral("GBp"),
                                   QStringLiteral("JPY"), QStringLiteral("USDT")}) {
            for (const double x : {1234.5, -1234.5, 0.0345, 117.7}) {
                const QString s = fmt::signed_money(x, ccy);
                const fmt::ParsedAmount a = fmt::parse_amount(s, ccy);
                if (!a.ok() || fmt::signed_money(*a.value, ccy) != s)
                    failures << QStringLiteral("%1 signed '%2': %3").arg(loc.name(), s, a.ok() ? fmt::signed_money(*a.value, ccy) : a.error);
                const fmt::InstrumentFmt inst = fmt::instrument_for_symbol(QStringLiteral("X"), ccy);
                const QString p = fmt::price_with_symbol(x, inst);
                const fmt::ParsedAmount b = fmt::parse_amount(p, ccy);
                if (!b.ok() || fmt::price_with_symbol(*b.value, inst) != p)
                    failures << QStringLiteral("%1 price '%2': %3").arg(loc.name(), p, b.ok() ? fmt::price_with_symbol(*b.value, inst) : b.error);
            }
        }
    }
    QVERIFY2(failures.isEmpty(), qPrintable(failures.join(QLatin1Char('\n'))));
}

QTEST_GUILESS_MAIN(TstAmountParse)
#include "tst_amount_parse.moc"
