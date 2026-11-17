// Unit tests for src/ui/formatting/{NumberFormat,DateFormat,CurrencyTable,FormatSettings}
// (currency & number format settings included: symbol style, position, negative
// style, grouping, decimals policy, minor units, overrides, conversions, exports)
//
// The formatters every screen is migrating to. The cases below are the ones the
// scattered per-screen helpers got wrong: sub-unit and FX prices rounded to 2
// decimals, JPY with cents, LSE pence shown as pounds, no lakh / crore, "$-5"
// instead of "-$5", "-0.00%", "nan", and daily bars drifting a day west of UTC.
//
// Every test pins the number locale (en_US unless stated), the zone and the
// clock, so the expectations hold on any machine and in any CI locale.

#include "ui/formatting/CurrencyTable.h"
#include "ui/formatting/DateFormat.h"
#include "ui/formatting/FormatSettings.h"
#include "ui/formatting/NumberFormat.h"

#include <QElapsedTimer>
#include <QSignalSpy>
#include <QTest>

#include <limits>

namespace fmt = fincept::ui::formatting;

class TstNumberFormat : public QObject {
    Q_OBJECT

  private slots:
    void init();

    void prices_pick_decimals_from_magnitude();
    void prices_zero_decimal_currencies();
    void prices_fx_pairs();
    void prices_tick_and_explicit_decimals();
    void prices_missing_and_negative();
    void prices_with_symbol_and_change();

    void money_symbol_and_sign();
    void money_minor_units();
    void money_compact();
    void money_locale_placement();

    void compact_kmb();
    void compact_lakh_crore();
    void compact_auto_and_settings();

    void percentages();
    void quantities_and_volume();
    void legacy_format_compact_volume();
    void misc_numbers();
    void non_finite_everywhere();

    void currency_table();

    void dates_utc_midnight_does_not_shift();
    void dates_styles_and_locales();
    void times_and_zones();
    void relative_times();
    void axis_labels();

    void settings_roundtrip();

    // ── Currency & number format settings (batch 0 of the currency design) ──
    void table_minor_units_and_codes();
    void table_symbols_are_unambiguous();
    void compact_minor_units();
    void compact_respects_user_scheme();
    void decimals_policy();
    void crypto_amounts_and_pairs();
    void symbol_style();
    void symbol_disambiguation_by_display_currency();
    void symbol_disambiguation_in_a_view();
    void headers_and_units();
    void position_and_spacing();
    void negative_style();
    void grouping();
    void minor_units_major();
    void mixed_columns();
    void overrides();
    void not_money();
    void converted_figures();
    void exports();
    void axes();
    void never_converted();
    void bidi_marks_removed();
    void settings_object();
    void settings_validation();
    void settings_migration_helpers();
    void system_locale_currency_pattern();
    void rounding_nudge();
    void money_cost();

  private:
    static qint64 utc(int y, int mo, int d, int h = 0, int mi = 0, int s = 0) {
        return QDateTime(QDate(y, mo, d), QTime(h, mi, s), QTimeZone(QTimeZone::UTC)).toSecsSinceEpoch();
    }
};

static const QLocale kEnUs(QLocale::English, QLocale::UnitedStates);

void TstNumberFormat::init() {
    auto& s = fmt::FormatSettings::instance();
    s.reset_to_defaults();
    s.set_values(fmt::FormatSettings::Values{}); // every currency setting at its default too
    s.set_locale(kEnUs);
    s.set_compact_scheme(fmt::Compact::Auto);
    s.set_timezone_mode(fmt::TimeZoneMode::UTC);
    s.set_hour_cycle(fmt::HourCycle::H24);
    s.set_auto_display_currency(QStringLiteral("USD"));
}

// ── Prices ───────────────────────────────────────────────────────────────────

void TstNumberFormat::prices_pick_decimals_from_magnitude() {
    const fmt::InstrumentFmt usd{QStringLiteral("USD")};
    QCOMPARE(fmt::price(1234.5, usd), QStringLiteral("1,234.50"));
    QCOMPARE(fmt::price(12.3, usd), QStringLiteral("12.30"));
    // Penny stocks and small-cap crypto keep their significant digits.
    QCOMPARE(fmt::price(0.5, usd), QStringLiteral("0.5000"));
    QCOMPARE(fmt::price(0.0345, usd), QStringLiteral("0.0345"));
    QCOMPARE(fmt::price(0.000123, usd), QStringLiteral("0.000123"));
    QCOMPARE(fmt::price(0.00001234, usd), QStringLiteral("0.00001234"));
    QCOMPARE(fmt::price(0.0, usd), QStringLiteral("0.00"));
    // Three-decimal currencies.
    QCOMPARE(fmt::price(1.234, {QStringLiteral("KWD")}), QStringLiteral("1.234"));
    // No currency at all: same magnitude rules.
    QCOMPARE(fmt::price(42512.34), QStringLiteral("42,512.34"));
}

void TstNumberFormat::prices_zero_decimal_currencies() {
    const fmt::InstrumentFmt jpy{QStringLiteral("JPY")};
    QCOMPARE(fmt::price(2845.0, jpy), QStringLiteral("2,845"));
    QCOMPARE(fmt::price(2845.5, jpy), QStringLiteral("2,845.5")); // a 0.5-yen tick is kept
    QCOMPARE(fmt::price(15000000.0, jpy), QStringLiteral("15,000,000"));
    QCOMPARE(fmt::price(72400.0, {QStringLiteral("KRW")}), QStringLiteral("72,400"));
    QCOMPARE(fmt::price(0.0234, jpy), QStringLiteral("0.0234")); // sub-yen still gets digits
}

void TstNumberFormat::prices_fx_pairs() {
    const auto eurusd = fmt::instrument_for_symbol(QStringLiteral("EURUSD=X"));
    QVERIFY(eurusd.fx);
    QCOMPARE(eurusd.currency, QStringLiteral("USD"));
    QCOMPARE(fmt::price(1.08342, eurusd), QStringLiteral("1.0834"));

    const auto usdjpy = fmt::instrument_for_symbol(QStringLiteral("JPY=X")); // Yahoo: USDJPY
    QVERIFY(usdjpy.fx);
    QCOMPARE(usdjpy.currency, QStringLiteral("JPY"));
    QCOMPARE(fmt::price(151.2346, usdjpy), QStringLiteral("151.235"));

    const auto eurgbp = fmt::instrument_for_symbol(QStringLiteral("EUR/GBP"));
    QVERIFY(eurgbp.fx);
    QCOMPARE(fmt::price(0.85672, eurgbp), QStringLiteral("0.85672"));

    QVERIFY(fmt::instrument_for_symbol(QStringLiteral("AUDCAD")).fx);
    QVERIFY(!fmt::instrument_for_symbol(QStringLiteral("BTC-USD")).fx); // crypto, not FX
    QVERIFY(!fmt::instrument_for_symbol(QStringLiteral("AAPL")).fx);
    // An explicit currency wins over the pair's quote currency.
    QCOMPARE(fmt::instrument_for_symbol(QStringLiteral("EURUSD=X"), QStringLiteral("EUR")).currency,
             QStringLiteral("EUR"));
}

void TstNumberFormat::prices_tick_and_explicit_decimals() {
    QCOMPARE(fmt::price(101.25, {QStringLiteral("USD"), -1, 0.25}), QStringLiteral("101.25"));
    QCOMPARE(fmt::price(1.2, {QString(), -1, 0.0001}), QStringLiteral("1.2000"));
    QCOMPARE(fmt::price(2845.0, {QStringLiteral("JPY"), -1, 0.1}), QStringLiteral("2,845.0"));
    QCOMPARE(fmt::price(12.3456, {QStringLiteral("USD"), 3}), QStringLiteral("12.346"));
    QCOMPARE(fmt::price(12.3456, {QStringLiteral("USD"), 0}), QStringLiteral("12"));
    QCOMPARE(fmt::price_decimals(1.0, {QString(), -1, 0.05}), 2);
    QCOMPARE(fmt::price_decimals(1.0, {QString(), -1, 1.0}), 0);
}

void TstNumberFormat::prices_missing_and_negative() {
    QCOMPARE(fmt::price(std::numeric_limits<double>::quiet_NaN()), fmt::placeholder());
    QCOMPARE(fmt::price(std::numeric_limits<double>::infinity()), fmt::placeholder());
    QCOMPARE(fmt::placeholder(), QStringLiteral("\u2014"));
    // Negative prices exist (spreads, WTI in April 2020).
    QCOMPARE(fmt::price(-37.63, {QStringLiteral("USD")}), QStringLiteral("-37.63"));
}

void TstNumberFormat::prices_with_symbol_and_change() {
    QCOMPARE(fmt::price_with_symbol(1234.5, {QStringLiteral("USD")}), QStringLiteral("$1,234.50"));
    QCOMPARE(fmt::price_with_symbol(0.0345, {QStringLiteral("USD")}), QStringLiteral("$0.0345"));
    QCOMPARE(fmt::price_with_symbol(-2.5, {QStringLiteral("USD")}), QStringLiteral("-$2.50"));
    QCOMPARE(fmt::price_with_symbol(123.45, {QStringLiteral("GBp")}), QStringLiteral("123.45p"));
    QCOMPARE(fmt::price_with_symbol(2845.0, {QStringLiteral("JPY")}), QStringLiteral("\u00A52,845"));

    QCOMPARE(fmt::price_change(1.5, 1234.5), QStringLiteral("+1.50"));
    QCOMPARE(fmt::price_change(-0.00012, 1.0834, fmt::instrument_for_symbol(QStringLiteral("EURUSD=X"))),
             QStringLiteral("-0.0001"));
    QCOMPARE(fmt::price_change(-0.001, 1234.5), QStringLiteral("0.00")); // no "-0.00"
}

// ── Money ────────────────────────────────────────────────────────────────────

void TstNumberFormat::money_symbol_and_sign() {
    QCOMPARE(fmt::money(1234.56, QStringLiteral("USD")), QStringLiteral("$1,234.56"));
    QCOMPARE(fmt::money(-1234.56, QStringLiteral("USD")), QStringLiteral("-$1,234.56")); // not "$-1,234.56"
    QCOMPARE(fmt::money(-0.001, QStringLiteral("USD")), QStringLiteral("$0.00"));
    QCOMPARE(fmt::money(1234567.0, QStringLiteral("JPY")), QStringLiteral("\u00A51,234,567"));
    QCOMPARE(fmt::money(1.234, QStringLiteral("KWD")), QStringLiteral("KWD 1.234"));
    // Letter symbols get a gap; glyph symbols do not.
    QCOMPARE(fmt::money(1234.5, QStringLiteral("CHF")), QStringLiteral("CHF 1,234.50"));
    QCOMPARE(fmt::money(-1234.5, QStringLiteral("CHF")), QStringLiteral("-CHF 1,234.50"));
    QCOMPARE(fmt::money(99.0, QStringLiteral("usd")), QStringLiteral("$99.00"));
    // Unknown and empty codes.
    QCOMPARE(fmt::money(10.0, QStringLiteral("XYZ")), QStringLiteral("XYZ 10.00"));
    QCOMPARE(fmt::money(10.0, QString()), QStringLiteral("10.00"));
    // Explicit decimals.
    QCOMPARE(fmt::money(1234.5678, QStringLiteral("USD"), fmt::Compact::None, 4), QStringLiteral("$1,234.5678"));
    // P&L.
    QCOMPARE(fmt::signed_money(1234.5, QStringLiteral("USD")), QStringLiteral("+$1,234.50"));
    QCOMPARE(fmt::signed_money(-1234.5, QStringLiteral("USD")), QStringLiteral("-$1,234.50"));
    QCOMPARE(fmt::signed_money(0.0, QStringLiteral("USD")), QStringLiteral("$0.00"));
}

void TstNumberFormat::money_minor_units() {
    QCOMPARE(fmt::money(123.45, QStringLiteral("GBp")), QStringLiteral("123.45p"));
    QCOMPARE(fmt::money(123.45, QStringLiteral("GBX")), QStringLiteral("123.45p"));
    QCOMPARE(fmt::money(12345.0, QStringLiteral("ZAc")), QStringLiteral("12,345c"));
    QCOMPARE(fmt::money(1234.0, QStringLiteral("ILA")), QStringLiteral("1,234 ag."));
    QCOMPARE(fmt::money(-50.5, QStringLiteral("GBp")), QStringLiteral("-50.50p"));
    // "GBP" is pounds, "GBp" is pence: case matters for exactly this code.
    QCOMPARE(fmt::money(123.45, QStringLiteral("GBP")), QStringLiteral("\u00A3123.45"));

    QCOMPARE(fmt::minor_unit_divisor(QStringLiteral("GBp")), 100);
    QCOMPARE(fmt::minor_unit_divisor(QStringLiteral("GBX")), 100);
    QCOMPARE(fmt::minor_unit_divisor(QStringLiteral("ZAc")), 100);
    QCOMPARE(fmt::minor_unit_divisor(QStringLiteral("ILA")), 100);
    QCOMPARE(fmt::minor_unit_divisor(QStringLiteral("KWF")), 1000);
    QCOMPARE(fmt::minor_unit_divisor(QStringLiteral("GBP")), 1);
    QCOMPARE(fmt::minor_unit_divisor(QStringLiteral("XYZ")), 1);
    QCOMPARE(fmt::major_currency(QStringLiteral("GBp")), QStringLiteral("GBP"));
    QCOMPARE(fmt::major_currency(QStringLiteral("GBX")), QStringLiteral("GBP"));
    QCOMPARE(fmt::major_currency(QStringLiteral("ZAc")), QStringLiteral("ZAR"));
    QCOMPARE(fmt::major_currency(QStringLiteral("ILA")), QStringLiteral("ILS"));
    QCOMPARE(fmt::major_currency(QStringLiteral("usd")), QStringLiteral("USD"));
    QCOMPARE(fmt::to_major_units(12345.0, QStringLiteral("GBp")), 123.45);

    // Pence converted to pounds for a position value.
    const double pence = 12345.0;
    QCOMPARE(fmt::money(fmt::to_major_units(pence, QStringLiteral("GBp")), fmt::major_currency(QStringLiteral("GBp"))),
             QStringLiteral("\u00A3123.45"));
}

void TstNumberFormat::money_compact() {
    QCOMPARE(fmt::money(1.5e9, QStringLiteral("USD"), fmt::Compact::KMB), QStringLiteral("$1.50B"));
    QCOMPARE(fmt::money(-1.5e9, QStringLiteral("USD"), fmt::Compact::KMB), QStringLiteral("-$1.50B"));
    QCOMPARE(fmt::money(950.25, QStringLiteral("USD"), fmt::Compact::KMB), QStringLiteral("$950.25"));
    QCOMPARE(fmt::money(950.0, QStringLiteral("JPY"), fmt::Compact::KMB), QStringLiteral("\u00A5950"));
    QCOMPARE(fmt::money(12345678.0, QStringLiteral("INR"), fmt::Compact::Auto), QStringLiteral("\u20B91.23 Cr"));
    QCOMPARE(fmt::money(12345678.0, QStringLiteral("USD"), fmt::Compact::Auto), QStringLiteral("$12.35M"));
    QCOMPARE(fmt::money(2.5e12, QStringLiteral("USD"), fmt::Compact::KMB, 1), QStringLiteral("$2.5T"));
}

void TstNumberFormat::money_locale_placement() {
    auto& s = fmt::FormatSettings::instance();

    s.set_locale(QLocale(QLocale::German, QLocale::Germany));
    QCOMPARE(fmt::money(1234.56, QStringLiteral("EUR")), QStringLiteral("1.234,56\u00A0\u20AC"));
    QCOMPARE(fmt::money(-1234.56, QStringLiteral("EUR")), QStringLiteral("-1.234,56\u00A0\u20AC"));
    QCOMPARE(fmt::price(1234.5, {QStringLiteral("EUR")}), QStringLiteral("1.234,50"));
    QCOMPARE(fmt::pct(-1.5), QStringLiteral("-1,50%"));

    // Indian grouping comes from the locale, not from the currency.
    s.set_locale(QLocale(QLocale::English, QLocale::India));
    QCOMPARE(fmt::money(12345678.9, QStringLiteral("INR")), QStringLiteral("\u20B91,23,45,678.90"));
    QCOMPARE(fmt::number(12345678.9, 2), QStringLiteral("1,23,45,678.90"));
}

// ── Compact ──────────────────────────────────────────────────────────────────

void TstNumberFormat::compact_kmb() {
    const auto kmb = fmt::Compact::KMB;
    QCOMPARE(fmt::compact(0.0, kmb), QStringLiteral("0"));
    QCOMPARE(fmt::compact(12.5, kmb), QStringLiteral("12.50"));
    QCOMPARE(fmt::compact(999.0, kmb), QStringLiteral("999"));
    QCOMPARE(fmt::compact(1500.0, kmb), QStringLiteral("1.5K"));
    QCOMPARE(fmt::compact(12345678.0, kmb), QStringLiteral("12.35M"));
    QCOMPARE(fmt::compact(1.234e9, kmb), QStringLiteral("1.23B"));
    QCOMPARE(fmt::compact(1.234e12, kmb), QStringLiteral("1.23T"));
    QCOMPARE(fmt::compact(-1.5e6, kmb), QStringLiteral("-1.50M"));
    // Rounding up across a unit boundary moves to the next unit.
    QCOMPARE(fmt::compact(999950.0, kmb), QStringLiteral("1.00M"));
    QCOMPARE(fmt::compact(999.999, kmb), QStringLiteral("1.0K"));
    QCOMPARE(fmt::compact(1234.0, fmt::Compact::None), QStringLiteral("1,234"));
}

void TstNumberFormat::compact_lakh_crore() {
    const auto lc = fmt::Compact::LakhCrore;
    QCOMPARE(fmt::compact(50000.0, lc), QStringLiteral("50.0K"));
    QCOMPARE(fmt::compact(99999.0, lc), QStringLiteral("1.00 L"));
    QCOMPARE(fmt::compact(150000.0, lc), QStringLiteral("1.50 L"));
    QCOMPARE(fmt::compact(12345678.0, lc), QStringLiteral("1.23 Cr"));
    QCOMPARE(fmt::compact(5e11, lc), QStringLiteral("50,000 Cr"));
    QCOMPARE(fmt::compact(1.2e12, lc), QStringLiteral("1.20 L Cr"));
    QCOMPARE(fmt::compact(-2.5e7, lc), QStringLiteral("-2.50 Cr"));
}

void TstNumberFormat::compact_auto_and_settings() {
    auto& s = fmt::FormatSettings::instance();
    QCOMPARE(fmt::compact(12345678.0, fmt::Compact::Auto, QStringLiteral("INR")), QStringLiteral("1.23 Cr"));
    QCOMPARE(fmt::compact(12345678.0, fmt::Compact::Auto, QStringLiteral("PKR")), QStringLiteral("1.23 Cr"));
    QCOMPARE(fmt::compact(12345678.0, fmt::Compact::Auto, QStringLiteral("USD")), QStringLiteral("12.35M"));
    // Auto without a currency follows the number locale's territory.
    QCOMPARE(fmt::compact(12345678.0, fmt::Compact::Auto), QStringLiteral("12.35M"));
    s.set_locale(QLocale(QLocale::English, QLocale::India));
    QCOMPARE(fmt::compact(12345678.0, fmt::Compact::Auto), QStringLiteral("1.23 Cr"));
    s.set_locale(kEnUs);

    // FromSettings (the default) uses the user's scheme.
    s.set_compact_scheme(fmt::Compact::LakhCrore);
    QCOMPARE(fmt::compact(12345678.0), QStringLiteral("1.23 Cr"));
    QCOMPARE(fmt::resolve_compact(fmt::Compact::FromSettings), fmt::Compact::LakhCrore);
    s.set_compact_scheme(fmt::Compact::KMB);
    QCOMPARE(fmt::compact(12345678.0), QStringLiteral("12.35M"));
    QCOMPARE(fmt::compact(12345678.0, fmt::Compact::FromSettings, QStringLiteral("INR")), QStringLiteral("12.35M"));
    // None / FromSettings are not storable schemes.
    s.set_compact_scheme(fmt::Compact::None);
    QCOMPARE(s.compact_scheme(), fmt::Compact::Auto);
}

// ── Percentages, quantities, misc ────────────────────────────────────────────

void TstNumberFormat::percentages() {
    QCOMPARE(fmt::pct(1.234), QStringLiteral("+1.23%"));
    QCOMPARE(fmt::pct(-1.234), QStringLiteral("-1.23%"));
    QCOMPARE(fmt::pct(0.0), QStringLiteral("0.00%"));
    QCOMPARE(fmt::pct(-0.001), QStringLiteral("0.00%")); // never "-0.00%"
    QCOMPARE(fmt::pct(0.001), QStringLiteral("0.00%"));  // nor "+0.00%"
    QCOMPARE(fmt::pct(1.5, 1, false), QStringLiteral("1.5%"));
    QCOMPARE(fmt::pct(-1.5, 1, false), QStringLiteral("-1.5%"));
    QCOMPARE(fmt::fraction_pct(0.1234), QStringLiteral("+12.34%"));
    QCOMPARE(fmt::fraction_pct(-0.05, 1), QStringLiteral("-5.0%"));
    QCOMPARE(fmt::pp(0.35), QStringLiteral("+0.35 pp"));
    QCOMPARE(fmt::pp(-1.0, 1), QStringLiteral("-1.0 pp"));
    QCOMPARE(fmt::bp(25.0), QStringLiteral("+25 bp"));
    QCOMPARE(fmt::bp(-12.4), QStringLiteral("-12 bp"));
    QCOMPARE(fmt::bp(7.5, 1, false), QStringLiteral("7.5 bp"));
}

void TstNumberFormat::quantities_and_volume() {
    QCOMPARE(fmt::quantity(100.0), QStringLiteral("100"));
    QCOMPARE(fmt::quantity(1234.0), QStringLiteral("1,234"));
    QCOMPARE(fmt::quantity(0.12345678), QStringLiteral("0.1235"));
    QCOMPARE(fmt::quantity(1.5), QStringLiteral("1.5"));
    QCOMPARE(fmt::quantity(-2.25), QStringLiteral("-2.25"));
    QCOMPARE(fmt::quantity(1.00001), QStringLiteral("1"));
    QCOMPARE(fmt::quantity(12345.678, 2), QStringLiteral("12,345.68"));

    QCOMPARE(fmt::volume(0.0), QStringLiteral("0"));
    QCOMPARE(fmt::volume(950.0), QStringLiteral("950"));
    QCOMPARE(fmt::volume(0.25), QStringLiteral("0.25")); // fractional crypto volume
    QCOMPARE(fmt::volume(12345678.0, fmt::Compact::KMB), QStringLiteral("12.35M"));
    QCOMPARE(fmt::volume(12345678.0, fmt::Compact::LakhCrore), QStringLiteral("1.23 Cr"));
    QCOMPARE(fmt::volume(-1.0), fmt::placeholder());
}

void TstNumberFormat::legacy_format_compact_volume() {
    // The two existing callers (watchlist, markets panel) rely on these.
    QCOMPARE(fmt::format_compact_volume(0), QStringLiteral("--"));
    QCOMPARE(fmt::format_compact_volume(-5), QStringLiteral("--"));
    QCOMPARE(fmt::format_compact_volume(999), QStringLiteral("999"));
    QCOMPARE(fmt::format_compact_volume(12345), QStringLiteral("12.35K"));
    QCOMPARE(fmt::format_compact_volume(12345678), QStringLiteral("12.35M"));
    QCOMPARE(fmt::format_compact_volume(1234567890), QStringLiteral("1.23B"));
}

void TstNumberFormat::misc_numbers() {
    QCOMPARE(fmt::number(1234567.891), QStringLiteral("1,234,567.89"));
    QCOMPARE(fmt::number(-0.004), QStringLiteral("0.00"));
    QCOMPARE(fmt::signed_number(1234.5), QStringLiteral("+1,234.50"));
    QCOMPARE(fmt::signed_number(-3.0), QStringLiteral("-3.00"));
    QCOMPARE(fmt::signed_number(0.0), QStringLiteral("0.00"));
    QCOMPARE(fmt::ratio(12.3456), QStringLiteral("12.35x"));
    QCOMPARE(fmt::ratio(-4.0, 1), QStringLiteral("-4.0x"));

    // The C locale omits group separators; the formatter never does.
    fmt::FormatSettings::instance().set_locale(QLocale::c());
    QCOMPARE(fmt::number(1234567.5), QStringLiteral("1,234,567.50"));
    QCOMPARE(fmt::money(1234.5, QStringLiteral("USD")), QStringLiteral("$1,234.50"));
}

void TstNumberFormat::non_finite_everywhere() {
    const double nan = std::numeric_limits<double>::quiet_NaN();
    const double inf = std::numeric_limits<double>::infinity();
    const QString dash = fmt::placeholder();
    for (const double v : {nan, inf, -inf}) {
        QCOMPARE(fmt::price(v), dash);
        QCOMPARE(fmt::price_with_symbol(v, {QStringLiteral("USD")}), dash);
        QCOMPARE(fmt::price_change(v, 1.0), dash);
        QCOMPARE(fmt::number(v), dash);
        QCOMPARE(fmt::signed_number(v), dash);
        QCOMPARE(fmt::money(v, QStringLiteral("USD")), dash);
        QCOMPARE(fmt::money(v, QStringLiteral("USD"), fmt::Compact::KMB), dash);
        QCOMPARE(fmt::signed_money(v, QStringLiteral("USD")), dash);
        QCOMPARE(fmt::compact(v, fmt::Compact::KMB), dash);
        QCOMPARE(fmt::pct(v), dash);
        QCOMPARE(fmt::fraction_pct(v), dash);
        QCOMPARE(fmt::pp(v), dash);
        QCOMPARE(fmt::bp(v), dash);
        QCOMPARE(fmt::quantity(v), dash);
        QCOMPARE(fmt::volume(v), dash);
        QCOMPARE(fmt::ratio(v), dash);
    }
}

// ── Currency table ───────────────────────────────────────────────────────────

void TstNumberFormat::currency_table() {
    QCOMPARE(fmt::currency_symbol(QStringLiteral("USD")), QStringLiteral("$"));
    QCOMPARE(fmt::currency_symbol(QStringLiteral("usd")), QStringLiteral("$"));
    QCOMPARE(fmt::currency_symbol(QStringLiteral("EUR")), QStringLiteral("\u20AC"));
    QCOMPARE(fmt::currency_symbol(QStringLiteral("INR")), QStringLiteral("\u20B9"));
    QCOMPARE(fmt::currency_symbol(QStringLiteral("GBp")), QStringLiteral("p"));
    QCOMPARE(fmt::currency_symbol(QStringLiteral("XYZ")), QStringLiteral("XYZ"));
    QCOMPARE(fmt::currency_symbol(QString()), QString());
    QCOMPARE(fmt::currency_decimals(QStringLiteral("JPY")), 0);
    QCOMPARE(fmt::currency_decimals(QStringLiteral("KWD")), 3);
    QCOMPARE(fmt::currency_decimals(QStringLiteral("XYZ")), 2);
    QCOMPARE(fmt::currency_name(QStringLiteral("USD")), QStringLiteral("US Dollar"));

    // The ~40 most traded currencies are all present.
    const QStringList majors = {
        "USD", "EUR", "JPY", "GBP", "CNY", "AUD", "CAD", "CHF", "HKD", "SGD", "SEK", "KRW", "NOK", "NZD",
        "INR", "MXN", "TWD", "ZAR", "BRL", "DKK", "PLN", "THB", "ILS", "IDR", "CZK", "AED", "TRY", "HUF",
        "CLP", "SAR", "PHP", "MYR", "COP", "RUB", "RON", "PEN", "BHD", "BGN", "ARS", "KWD", "QAR", "PKR",
    };
    for (const QString& c : majors)
        QVERIFY2(fmt::is_known_currency(c), qPrintable(c));
    QVERIFY(fmt::known_currency_codes().size() >= 40);

    fmt::CurrencySpec spec;
    QVERIFY(fmt::find_currency(QStringLiteral("GBp"), &spec));
    QCOMPARE(spec.major, QStringLiteral("GBP"));
    QVERIFY(spec.suffix);
    QVERIFY(!fmt::find_currency(QStringLiteral("NOPE"), &spec));
}

// ── Dates ────────────────────────────────────────────────────────────────────

void TstNumberFormat::dates_utc_midnight_does_not_shift() {
    auto& s = fmt::FormatSettings::instance();
    const qint64 bar = utc(2026, 11, 16); // daily bar for Monday 16 Nov

    QVERIFY(s.set_fixed_timezone("America/New_York"));
    // As an instant it is the evening of the 15th in New York...
    QCOMPARE(fmt::date(bar), QStringLiteral("Nov 15, 2026"));
    QCOMPARE(fmt::axis_label(bar, 30 * 86400), QStringLiteral("Nov 15"));
    // ...but as a daily bar it is the 16th, in every zone.
    QCOMPARE(fmt::date_from_utc_midnight(bar), QDate(2026, 11, 16));
    QCOMPARE(fmt::bar_date(bar), QStringLiteral("Nov 16, 2026"));
    QCOMPARE(fmt::axis_label_daily(bar, 30 * 86400), QStringLiteral("Nov 16"));

    QVERIFY(s.set_fixed_timezone("Pacific/Honolulu"));
    QCOMPARE(fmt::bar_date(bar, fmt::DateStyle::Short), QStringLiteral("11/16/26"));

    QCOMPARE(fmt::calendar_date(QDate(2026, 12, 18)), QStringLiteral("Dec 18, 2026"));
    QCOMPARE(fmt::calendar_date(QDate()), fmt::placeholder());
}

void TstNumberFormat::dates_styles_and_locales() {
    auto& s = fmt::FormatSettings::instance();
    const qint64 ts = utc(2026, 11, 16, 14, 30, 5);

    QCOMPARE(fmt::date(ts, fmt::DateStyle::Short), QStringLiteral("11/16/26"));
    QCOMPARE(fmt::date(ts, fmt::DateStyle::Medium), QStringLiteral("Nov 16, 2026"));
    QCOMPARE(fmt::date(ts, fmt::DateStyle::Long), QStringLiteral("Monday, November 16, 2026"));
    QCOMPARE(fmt::date(QDateTime()), fmt::placeholder());

    // Month names and field order come from the locale.
    s.set_locale(QLocale(QLocale::German, QLocale::Germany));
    QCOMPARE(fmt::date(ts), QStringLiteral("16. Nov. 2026"));
    QCOMPARE(fmt::date(ts, fmt::DateStyle::Short), QStringLiteral("16.11.26"));
    QCOMPARE(fmt::axis_label(ts, 30 * 86400), QStringLiteral("16. Nov."));
    QCOMPARE(fmt::axis_label(ts, 2 * 365 * 86400), QStringLiteral("Nov. 2026"));

    s.set_locale(QLocale(QLocale::Japanese, QLocale::Japan));
    QCOMPARE(fmt::date(ts), QStringLiteral("2026\u5E7411\u670816\u65E5"));
    QCOMPARE(fmt::axis_label(ts, 30 * 86400), QStringLiteral("11\u670816\u65E5"));
    QCOMPARE(fmt::axis_label(ts, 2 * 365 * 86400), QStringLiteral("2026\u5E7411\u6708"));

    s.set_locale(QLocale(QLocale::French, QLocale::France));
    QCOMPARE(fmt::date(ts), QStringLiteral("16 nov. 2026"));
}

void TstNumberFormat::times_and_zones() {
    auto& s = fmt::FormatSettings::instance();
    const qint64 ts = utc(2026, 11, 16, 14, 30, 5);

    QCOMPARE(fmt::time(ts), QStringLiteral("14:30"));
    QCOMPARE(fmt::time(ts, true), QStringLiteral("14:30:05"));
    QCOMPARE(fmt::date_time(ts), QStringLiteral("Nov 16, 2026 14:30"));
    QCOMPARE(fmt::zone_label(QDateTime::fromSecsSinceEpoch(ts, QTimeZone(QTimeZone::UTC))), QStringLiteral("UTC"));

    s.set_hour_cycle(fmt::HourCycle::H12);
    QCOMPARE(fmt::time(ts), QStringLiteral("2:30 PM"));
    QCOMPARE(fmt::time(ts, true), QStringLiteral("2:30:05 PM"));
    s.set_hour_cycle(fmt::HourCycle::Locale); // en_US: 12-hour, CLDR's own separator (U+202F in Qt 6.8)
    const QString locale_time = fmt::time(ts);
    QVERIFY2(locale_time.startsWith(QStringLiteral("2:30")) && locale_time.endsWith(QStringLiteral("PM")),
             qPrintable(locale_time));
    s.set_hour_cycle(fmt::HourCycle::H24);

    // Exchange mode uses the zone the caller passes...
    s.set_timezone_mode(fmt::TimeZoneMode::Exchange);
    QCOMPARE(fmt::time(ts, false, QTimeZone("Asia/Kolkata")), QStringLiteral("20:00"));
    QCOMPARE(fmt::time(ts, false, QTimeZone("America/New_York")), QStringLiteral("09:30"));
    // ...and is ignored in the other modes.
    s.set_timezone_mode(fmt::TimeZoneMode::UTC);
    QCOMPARE(fmt::time(ts, false, QTimeZone("Asia/Kolkata")), QStringLiteral("14:30"));
    QVERIFY(s.set_fixed_timezone("Asia/Tokyo"));
    QCOMPARE(fmt::date_time(ts, fmt::DateStyle::Medium, false, QTimeZone("Asia/Kolkata")),
             QStringLiteral("Nov 16, 2026 23:30"));
    QVERIFY(!s.set_fixed_timezone("Mars/Olympus_Mons"));
    QCOMPARE(s.fixed_timezone_id(), QByteArray("Asia/Tokyo"));
}

void TstNumberFormat::relative_times() {
    const qint64 now = utc(2026, 11, 16, 14, 30, 5);
    QCOMPARE(fmt::relative(now - 10, now), QStringLiteral("just now"));
    QCOMPARE(fmt::relative(now - 300, now), QStringLiteral("5m ago"));
    QCOMPARE(fmt::relative(now - 7200, now), QStringLiteral("2h ago"));
    QCOMPARE(fmt::relative(now - 3 * 86400, now), QStringLiteral("3d ago"));
    QCOMPARE(fmt::relative(now + 300, now), QStringLiteral("in 5m"));
    QCOMPARE(fmt::relative(now - 30 * 86400, now), QStringLiteral("Oct 17, 2026"));
    const QDateTime dt = QDateTime::fromSecsSinceEpoch(now - 120, QTimeZone(QTimeZone::UTC));
    QCOMPARE(fmt::relative(dt, QDateTime::fromSecsSinceEpoch(now, QTimeZone(QTimeZone::UTC))),
             QStringLiteral("2m ago"));
    QCOMPARE(fmt::relative(QDateTime(), QDateTime::currentDateTimeUtc()), fmt::placeholder());
}

void TstNumberFormat::axis_labels() {
    const qint64 ts = utc(2026, 11, 16, 14, 30, 5);
    QCOMPARE(fmt::axis_label(ts, 60), QStringLiteral("14:30:05"));
    QCOMPARE(fmt::axis_label(ts, 86400), QStringLiteral("14:30"));
    QCOMPARE(fmt::axis_label(ts, 5 * 86400), QStringLiteral("Nov 16 14:30"));
    QCOMPARE(fmt::axis_label(ts, 30 * 86400), QStringLiteral("Nov 16"));
    QCOMPARE(fmt::axis_label(ts, 2 * 365 * 86400), QStringLiteral("Nov 2026"));
    QCOMPARE(fmt::axis_label(ts, 10 * 365 * 86400), QStringLiteral("2026"));
    QCOMPARE(fmt::axis_label_daily(utc(2026, 11, 16), 5 * 86400), QStringLiteral("Nov 16"));
    QCOMPARE(fmt::axis_label_daily(utc(2026, 11, 16), 3 * 365 * 86400), QStringLiteral("Nov 2026"));
    QCOMPARE(fmt::axis_pattern(86400), QStringLiteral("HH:mm"));
}

// ── Settings ─────────────────────────────────────────────────────────────────

void TstNumberFormat::settings_roundtrip() {
    auto& s = fmt::FormatSettings::instance();
    QSignalSpy spy(&s, &fmt::FormatSettings::changed);

    // Every key at its default, then the four date / number ones changed.
    QHash<QString, QString> stored = s.settings_values();
    stored.insert(QStringLiteral("appearance.number_locale"), QStringLiteral("en-IN"));
    stored.insert(QStringLiteral("appearance.compact_numbers"), QStringLiteral("lakh_crore"));
    stored.insert(QStringLiteral("appearance.display_timezone"), QStringLiteral("Asia/Kolkata"));
    stored.insert(QStringLiteral("appearance.clock"), QStringLiteral("12h"));
    s.apply_settings_values(stored);
    QCOMPARE(spy.count(), 1); // one notification for the whole batch

    QVERIFY(!s.locale_follows_app());
    QCOMPARE(s.locale().territory(), QLocale::India);
    QCOMPARE(s.compact_scheme(), fmt::Compact::LakhCrore);
    QCOMPARE(s.timezone_mode(), fmt::TimeZoneMode::Fixed);
    QCOMPARE(s.fixed_timezone_id(), QByteArray("Asia/Kolkata"));
    QCOMPARE(s.hour_cycle(), fmt::HourCycle::H12);
    QCOMPARE(s.settings_values(), stored);

    // Re-applying the same values is not a change.
    s.apply_settings_values(stored);
    QCOMPARE(spy.count(), 1);

    // Garbage falls back to the defaults.
    QHash<QString, QString> junk;
    junk.insert(QStringLiteral("appearance.number_locale"), QStringLiteral("zz-NOTALOCALE"));
    junk.insert(QStringLiteral("appearance.compact_numbers"), QStringLiteral("roman"));
    junk.insert(QStringLiteral("appearance.display_timezone"), QStringLiteral("Mars/Olympus_Mons"));
    junk.insert(QStringLiteral("appearance.clock"), QStringLiteral("36h"));
    s.apply_settings_values(junk);
    QVERIFY(s.locale_follows_app());
    QCOMPARE(s.compact_scheme(), fmt::Compact::Auto);
    QCOMPARE(s.timezone_mode(), fmt::TimeZoneMode::Local);
    QCOMPARE(s.hour_cycle(), fmt::HourCycle::H24);

    QCOMPARE(fmt::FormatSettings::setting_keys().size(), 16);
    s.set_timezone_mode(fmt::TimeZoneMode::UTC);
    QCOMPARE(s.settings_values().value(QStringLiteral("appearance.display_timezone")), QStringLiteral("utc"));
    QCOMPARE(s.display_zone(QTimeZone("Asia/Tokyo")), QTimeZone(QTimeZone::UTC));
    s.set_timezone_mode(fmt::TimeZoneMode::Exchange);
    QCOMPARE(s.display_zone(QTimeZone("Asia/Tokyo")), QTimeZone("Asia/Tokyo"));
}

// ══ Currency & number format settings ════════════════════════════════════════
// Case numbers (#n) are those of the currency design's test list (D.7).

namespace {
const QLocale kDeDe(QLocale::German, QLocale::Germany);
const QString kNbsp = QStringLiteral("\u00A0");
fmt::FormatSettings& fs() {
    return fmt::FormatSettings::instance();
}
QString usd(double v) {
    return fmt::money(v, QStringLiteral("USD"));
}
} // namespace

void TstNumberFormat::table_minor_units_and_codes() {
    QCOMPARE(fmt::money(448.5, QStringLiteral("USX")), QStringLiteral("448.50\u00A2"));                // #1
    QCOMPARE(fmt::to_major_units(448.5, QStringLiteral("USX")), 4.485);                                // #2
    QCOMPARE(fmt::major_currency(QStringLiteral("USc")), QStringLiteral("USD"));                       // #3
    QCOMPARE(fmt::minor_unit_divisor(QStringLiteral("USc")), 100);
    QCOMPARE(fmt::currency_decimals(QStringLiteral("CLF")), 4);                                        // #4
    QCOMPARE(fmt::currency_decimals(QStringLiteral("XOF")), 0);
    QCOMPARE(fmt::currency_decimals(QStringLiteral("IQD")), 3);
    QCOMPARE(fmt::currency_decimals(QStringLiteral("UGX")), 0);
    QCOMPARE(fmt::currency_symbol(QStringLiteral("ETH")), QStringLiteral("ETH"));                      // #5
    QVERIFY(fmt::is_crypto_currency(QStringLiteral("USDT")));                                          // #6
    QVERIFY(fmt::is_crypto_currency(QStringLiteral("BTC")));
    QVERIFY(!fmt::is_crypto_currency(QStringLiteral("USD")));
    QVERIFY(fmt::is_minor_unit(QStringLiteral("GBp")));                                                // #7
    QVERIFY(!fmt::is_minor_unit(QStringLiteral("GBP")));
    QVERIFY(fmt::is_minor_unit(QStringLiteral("USX")));
    QCOMPARE(fmt::currency_symbol(QStringLiteral("USD"), fmt::SymbolStyle::Disambiguated), QStringLiteral("US$")); // #8
    QCOMPARE(fmt::currency_symbol(QStringLiteral("JPY"), fmt::SymbolStyle::Disambiguated),
             QStringLiteral("JP\u00A5"));                                                              // #9
    QCOMPARE(fmt::currency_symbol(QStringLiteral("CAD"), fmt::SymbolStyle::Disambiguated), QStringLiteral("C$")); // #10
    QCOMPARE(fmt::currency_symbol(QStringLiteral("GBp"), fmt::SymbolStyle::Code), QStringLiteral("GBp")); // #11

    // Gaps: the other spellings feeds send, ISO 4217 exceptions, stablecoins, metals.
    QCOMPARE(fmt::major_currency(QStringLiteral("ZAC")), QStringLiteral("ZAR"));
    QCOMPARE(fmt::major_currency(QStringLiteral("ILa")), QStringLiteral("ILS"));
    QCOMPARE(fmt::major_currency(QStringLiteral("GBx")), QStringLiteral("GBP"));
    for (const char* zero : {"JPY", "KRW", "VND", "CLP", "ISK", "XAF", "PYG", "RWF", "BIF", "DJF", "GNF", "KMF",
                             "VUV", "XPF", "IDR"})
        QVERIFY2(fmt::currency_decimals(QString::fromLatin1(zero)) == 0, zero);
    for (const char* three : {"BHD", "KWD", "OMR", "JOD", "TND", "IQD", "LYD"})
        QVERIFY2(fmt::currency_decimals(QString::fromLatin1(three)) == 3, three);
    QVERIFY(fmt::is_stablecoin(QStringLiteral("USDT")));
    QVERIFY(fmt::is_stablecoin(QStringLiteral("FDUSD")));
    QVERIFY(!fmt::is_stablecoin(QStringLiteral("BTC")));
    QCOMPARE(fmt::money(0.12345678, QStringLiteral("ETH")), QStringLiteral("ETH 0.12345678"));
    QVERIFY(fmt::is_known_currency(QStringLiteral("XAU")));
    QVERIFY(fmt::known_currency_codes().size() >= 150);
    QCOMPARE(fmt::currency_name(QStringLiteral("BGN")), QStringLiteral("Bulgarian Lev (until 2025)"));
}

void TstNumberFormat::table_symbols_are_unambiguous() {
    // Context-free symbols and strict symbols must each name exactly one major
    // currency; otherwise a mixed view without a CurrencyMix would be ambiguous.
    QHash<QString, QString> by_symbol;
    QHash<QString, QString> by_strict;
    for (const QString& code : fmt::known_currency_codes()) {
        fmt::CurrencySpec spec;
        QVERIFY(fmt::find_currency(code, &spec));
        if (spec.minor_divisor != 1)
            continue;
        QVERIFY2(!by_symbol.contains(spec.symbol),
                 qPrintable(code + " shares its symbol with " + by_symbol.value(spec.symbol)));
        by_symbol.insert(spec.symbol, code);
        QVERIFY2(!by_strict.contains(spec.strict),
                 qPrintable(code + " shares its strict symbol with " + by_strict.value(spec.strict)));
        by_strict.insert(spec.strict, code);
        QVERIFY2(!spec.name.isEmpty(), qPrintable(code));
        QVERIFY2(spec.decimals >= 0 && spec.decimals <= 8, qPrintable(code));
    }
}

void TstNumberFormat::compact_minor_units() {
    // B.1 #2: an abbreviated pence amount switches to pounds ("1.50Mp" before).
    QCOMPARE(fmt::money(1.5e6, QStringLiteral("GBp"), fmt::Compact::KMB), QStringLiteral("\u00A315.0K"));  // #12
    QCOMPARE(fmt::money(950.0, QStringLiteral("GBp"), fmt::Compact::KMB), QStringLiteral("\u00A39.50"));   // #13
    QCOMPARE(fmt::money(73550.0, QStringLiteral("ZAc"), fmt::Compact::KMB), QStringLiteral("R 735.50"));   // #14
    QCOMPARE(fmt::money(-2.5e9, QStringLiteral("USX"), fmt::Compact::KMB), QStringLiteral("-$25.00M"));
}

void TstNumberFormat::compact_respects_user_scheme() {
    // B.1 #16: Compact::Auto at a call site follows the user's explicit scheme.
    fs().set_compact_scheme(fmt::Compact::KMB);
    QCOMPARE(fmt::money(12345678.0, QStringLiteral("INR"), fmt::Compact::Auto), QStringLiteral("\u20B912.35M"));
    fs().set_compact_scheme(fmt::Compact::LakhCrore);
    QCOMPARE(fmt::money(12345678.0, QStringLiteral("USD"), fmt::Compact::Auto), QStringLiteral("$1.23 Cr"));
    QCOMPARE(fmt::money(12345678.0, QStringLiteral("USD"), fmt::Compact::FromSettings), QStringLiteral("$1.23 Cr"));
    // An explicit scheme is still honoured.
    QCOMPARE(fmt::money(12345678.0, QStringLiteral("USD"), fmt::Compact::KMB), QStringLiteral("$12.35M"));
    fs().set_compact_scheme(fmt::Compact::Auto);
    QCOMPARE(fmt::money(12345678.0, QStringLiteral("INR"), fmt::Compact::Auto), QStringLiteral("\u20B91.23 Cr"));
}

void TstNumberFormat::decimals_policy() {
    QCOMPARE(usd(0.00001234), QStringLiteral("$0.00"));                                                     // #15
    QCOMPARE(fmt::money_precise(0.00001234, QStringLiteral("USD")), QStringLiteral("$0.00001234"));          // #16
    QCOMPARE(fmt::money_precise(0.004, QStringLiteral("USD")), QStringLiteral("$0.004"));                    // #17
    QCOMPARE(fmt::money_precise(1234.5, QStringLiteral("USD")), QStringLiteral("$1,234.50"));                // #18
    QCOMPARE(fmt::money_precise(0.4, QStringLiteral("JPY")), QStringLiteral("\u00A50.4"));
    fs().set_money_decimals(fmt::DecimalsPolicy::Adaptive);
    QCOMPARE(usd(0.004), QStringLiteral("$0.004"));                                                          // #19
    QCOMPARE(fmt::signed_money(-0.00042, QStringLiteral("USD")), QStringLiteral("-$0.00042"));               // #20
    QCOMPARE(usd(1234.5), QStringLiteral("$1,234.50"));                                                      // #21
    fs().set_money_decimals(fmt::DecimalsPolicy::Fixed, 0);
    QCOMPARE(usd(1234.56), QStringLiteral("$1,235"));                                                        // #22
    QCOMPARE(fmt::money(1234.5678, QStringLiteral("USD"), fmt::Compact::None, 3), QStringLiteral("$1,234.568")); // #24
    fs().set_money_decimals(fmt::DecimalsPolicy::Fixed, 4);
    QCOMPARE(fmt::money(1.5, QStringLiteral("JPY")), QStringLiteral("\u00A51.5000"));                        // #23
    // money_precise ignores Fixed: it never hides a non-zero amount.
    QCOMPARE(fmt::money_precise(0.00001234, QStringLiteral("USD")), QStringLiteral("$0.00001234"));
    // Fixed is clamped.
    fs().set_money_decimals(fmt::DecimalsPolicy::Fixed, 42);
    QCOMPARE(fs().fixed_decimals(), 8);
}

void TstNumberFormat::crypto_amounts_and_pairs() {
    QCOMPARE(fmt::money(0.5, QStringLiteral("BTC")), QStringLiteral("\u20BF0.50"));                     // #25
    QCOMPARE(fmt::money(0.12345678, QStringLiteral("BTC")), QStringLiteral("\u20BF0.12345678"));        // #26
    QCOMPARE(fmt::money(0.00042, QStringLiteral("SOL")), QStringLiteral("SOL 0.00042"));                // #27
    QCOMPARE(fmt::money(1234.5, QStringLiteral("USDT")), QStringLiteral("USDT 1,234.50"));              // #28
    const auto btc = fmt::instrument_for_symbol(QStringLiteral("BTC-USD"));                            // #29
    QCOMPARE(btc.currency, QStringLiteral("USD"));
    QVERIFY(!btc.fx);
    QCOMPARE(fmt::instrument_for_symbol(QStringLiteral("ETH/USDT")).currency, QStringLiteral("USDT"));  // #30
    QCOMPARE(fmt::instrument_for_symbol(QStringLiteral("BRK-B")).currency, QString());                  // #31
    QCOMPARE(fmt::price_with_symbol(0.05321, {QStringLiteral("BTC")}), QStringLiteral("\u20BF0.053210")); // #32
    // Gaps: a BTC-quoted pair, a stablecoin pair, an explicit currency wins.
    QCOMPARE(fmt::instrument_for_symbol(QStringLiteral("ETH-BTC")).currency, QStringLiteral("BTC"));
    QCOMPARE(fmt::instrument_for_symbol(QStringLiteral("USDT-USD")).currency, QStringLiteral("USD"));
    QCOMPARE(fmt::instrument_for_symbol(QStringLiteral("BTC-USD"), QStringLiteral("EUR")).currency,
             QStringLiteral("EUR"));
    QVERIFY(fmt::instrument_for_symbol(QStringLiteral("EUR-USD")).fx);
    QCOMPARE(fmt::instrument_for_symbol(QStringLiteral("BF-B.L")).currency, QString());
    // Derivatives contracts "BASE/QUOTE:SETTLE" are priced in the quote asset; a venue prefix is not a contract.
    QCOMPARE(fmt::instrument_for_symbol(QStringLiteral("BTC/USDT:USDT")).currency, QStringLiteral("USDT"));
    QCOMPARE(fmt::instrument_for_symbol(QStringLiteral("BTC/USD:BTC")).currency, QStringLiteral("USD"));
    QCOMPARE(fmt::instrument_for_symbol(QStringLiteral("ETH/USDC:USDC")).currency, QStringLiteral("USDC"));
    QCOMPARE(fmt::instrument_for_symbol(QStringLiteral("NSE:RELIANCE")).currency, QString());
    QCOMPARE(fmt::money(2.0, QStringLiteral("BTC")), QStringLiteral("\u20BF2.00"));
}

void TstNumberFormat::symbol_style() {
    fs().set_symbol_style(fmt::SymbolStyle::Code);
    QCOMPARE(fmt::money(1234.56, QStringLiteral("USD")), QStringLiteral("USD 1,234.56"));            // #33
    QCOMPARE(fmt::money(-1234.56, QStringLiteral("USD")), QStringLiteral("-USD 1,234.56"));          // #34
    QCOMPARE(fmt::money(123.45, QStringLiteral("GBp")), QStringLiteral("GBp 123.45"));               // #35
    QCOMPARE(fmt::price_with_symbol(117.7, {QStringLiteral("GBp")}), QStringLiteral("GBp 117.70"));
    fs().set_locale(kDeDe);
    QCOMPARE(fmt::money(1234.56, QStringLiteral("EUR")), QStringLiteral("1.234,56") + kNbsp + "EUR"); // #36
    fs().set_locale(kEnUs);
    fs().set_symbol_style(fmt::SymbolStyle::Disambiguated);
    QCOMPARE(fmt::money(1234.56, QStringLiteral("USD")), QStringLiteral("US$1,234.56"));             // #37
    QCOMPARE(fmt::money(2845.0, QStringLiteral("JPY")), QStringLiteral("JP\u00A52,845"));            // #38
    QCOMPARE(fmt::money(123.45, QStringLiteral("GBp")), QStringLiteral("123.45p"));                  // #39
    QCOMPARE(fmt::money(10.0, QStringLiteral("CAD")), QStringLiteral("C$10.00"));
}

void TstNumberFormat::symbol_disambiguation_by_display_currency() {
    // Decision 6, no view context: the display currency gets its plain symbol;
    // another currency keeps its table symbol unless that reads like the
    // display currency's.
    QCOMPARE(fmt::money(10.0, QStringLiteral("CAD")), QStringLiteral("C$10.00"));
    QCOMPARE(fmt::money(1000.0, QStringLiteral("JPY")), QStringLiteral("\u00A51,000"));
    QVERIFY(fs().set_display_currency(QStringLiteral("CAD")));
    QCOMPARE(fmt::money(10.0, QStringLiteral("CAD")), QStringLiteral("$10.00"));
    QCOMPARE(fmt::money(10.0, QStringLiteral("USD")), QStringLiteral("US$10.00"));
    QCOMPARE(fmt::money(10.0, QStringLiteral("AUD")), QStringLiteral("A$10.00"));
    QCOMPARE(fmt::currency_unit(), QStringLiteral("$"));
    QVERIFY(fs().set_display_currency(QStringLiteral("CNY")));
    QCOMPARE(fmt::money(10.0, QStringLiteral("CNY")), QStringLiteral("\u00A510.00"));
    QCOMPARE(fmt::money(1000.0, QStringLiteral("JPY")), QStringLiteral("JP\u00A51,000"));
    QVERIFY(fs().set_display_currency(QStringLiteral("SEK")));
    QCOMPARE(fmt::money(10.0, QStringLiteral("SEK")), QStringLiteral("kr 10.00"));
    QCOMPARE(fmt::money(10.0, QStringLiteral("NOK")), QStringLiteral("NOK 10.00"));
    QVERIFY(fs().set_display_currency(QStringLiteral("EUR")));
    QCOMPARE(fmt::money(10.0, QStringLiteral("USD")), QStringLiteral("$10.00"));
    QCOMPARE(fmt::money(10.0, QStringLiteral("CAD")), QStringLiteral("C$10.00"));
}

void TstNumberFormat::symbol_disambiguation_in_a_view() {
    fs().set_code_when_mixed(false);
    const fmt::CurrencyMix cad_only(QStringList{QStringLiteral("CAD")});
    QCOMPARE(fmt::money_in_column(10.0, QStringLiteral("CAD"), cad_only), QStringLiteral("$10.00"));
    QCOMPARE(fmt::currency_unit(QStringLiteral("CAD"), cad_only), QStringLiteral("$"));
    const fmt::CurrencyMix usd_cad(QStringList{QStringLiteral("USD"), QStringLiteral("CAD")});
    QVERIFY(usd_cad.mixed());
    QCOMPARE(fmt::money_in_column(10.0, QStringLiteral("USD"), usd_cad), QStringLiteral("US$10.00"));
    QCOMPARE(fmt::money_in_column(10.0, QStringLiteral("CAD"), usd_cad), QStringLiteral("C$10.00"));
    const fmt::CurrencyMix usd_eur(QStringList{QStringLiteral("USD"), QStringLiteral("EUR")});
    QCOMPARE(fmt::money_in_column(10.0, QStringLiteral("USD"), usd_eur), QStringLiteral("$10.00"));
    QCOMPARE(fmt::money_in_column(10.0, QStringLiteral("EUR"), usd_eur), QStringLiteral("\u20AC10.00"));
    const fmt::CurrencyMix yen_yuan(QStringList{QStringLiteral("JPY"), QStringLiteral("CNY")});
    QCOMPARE(fmt::money_in_column(1000.0, QStringLiteral("JPY"), yen_yuan), QStringLiteral("JP\u00A51,000"));
    QCOMPARE(fmt::money_in_column(10.0, QStringLiteral("CNY"), yen_yuan), QStringLiteral("CN\u00A510.00"));
    const fmt::CurrencyMix kronor(QStringList{QStringLiteral("SEK"), QStringLiteral("NOK")});
    QCOMPARE(fmt::money_in_column(10.0, QStringLiteral("SEK"), kronor), QStringLiteral("SEK 10.00"));
    // A pence quote counts as its major currency in a mix.
    const fmt::CurrencyMix pence(QStringList{QStringLiteral("GBp"), QStringLiteral("GBP")});
    QVERIFY(!pence.mixed());
    QCOMPARE(pence.single(), QStringLiteral("GBP"));
    // With the codes setting on (default), a mixed column prints codes.
    fs().set_code_when_mixed(true);
    QCOMPARE(fmt::money_in_column(10.0, QStringLiteral("CAD"), usd_cad), QStringLiteral("CAD 10.00"));
    QCOMPARE(fmt::money_in_column(10.0, QStringLiteral("CAD"), cad_only), QStringLiteral("$10.00"));
    QCOMPARE(fmt::header(QStringLiteral("Value"), cad_only), QStringLiteral("Value ($)"));
    QCOMPARE(fmt::header(QStringLiteral("Value"), usd_cad), QStringLiteral("Value"));
}

void TstNumberFormat::headers_and_units() {
    QCOMPARE(fmt::header(QStringLiteral("Value"), QStringLiteral("USD")), QStringLiteral("Value ($)"));   // #40
    fs().set_symbol_style(fmt::SymbolStyle::Code);
    QCOMPARE(fmt::header(QStringLiteral("Value"), QStringLiteral("USD")), QStringLiteral("Value (USD)")); // #41
    QCOMPARE(fmt::header_scaled(QStringLiteral("Deal value"), QStringLiteral("USD"), fmt::Scale::Million),
             QStringLiteral("Deal value (USD M)"));                                                       // #46
    fs().set_symbol_style(fmt::SymbolStyle::Disambiguated);
    QCOMPARE(fmt::header(QStringLiteral("Value"), QStringLiteral("USD")), QStringLiteral("Value (US$)")); // #42
    fs().set_symbol_style(fmt::SymbolStyle::Symbol);
    QCOMPARE(fmt::header(QStringLiteral("Value"), QStringLiteral("-")), QStringLiteral("Value"));         // #43
    QCOMPARE(fmt::header_scaled(QStringLiteral("Deal value"), QStringLiteral("USD"), fmt::Scale::Million),
             QStringLiteral("Deal value ($M)"));                                                          // #45
    QCOMPARE(fmt::header_scaled(QStringLiteral("Net flow"), QStringLiteral("INR"), fmt::Scale::Crore),
             QStringLiteral("Net flow (\u20B9 Cr)"));                                                     // #47
    QCOMPARE(fmt::scaled_unit(QStringLiteral("GBp"), fmt::Scale::Billion), QStringLiteral("\u00A3B"));
    QVERIFY(fs().set_display_currency(QStringLiteral("EUR")));
    QCOMPARE(fmt::header(QStringLiteral("Value")), QStringLiteral("Value (\u20AC)"));                     // #44
    QCOMPARE(fmt::currency_unit(), QStringLiteral("\u20AC"));                                             // #48
    QCOMPARE(fmt::display_currency(), QStringLiteral("EUR"));
    fs().set_symbol_style(fmt::SymbolStyle::Code);
    QCOMPARE(fmt::currency_unit(), QStringLiteral("EUR"));
}

void TstNumberFormat::position_and_spacing() {
    fs().set_locale(kDeDe);
    fs().set_symbol_position(fmt::SymbolPosition::Before);
    QCOMPARE(fmt::money(1234.56, QStringLiteral("EUR")), QStringLiteral("\u20AC1.234,56"));            // #49
    fs().set_symbol_spacing(fmt::SymbolSpacing::Space);
    QCOMPARE(fmt::money(1234.56, QStringLiteral("EUR")), QStringLiteral("\u20AC") + kNbsp + "1.234,56"); // #50
    fs().set_locale(kEnUs);
    fs().set_symbol_spacing(fmt::SymbolSpacing::Auto);
    fs().set_symbol_position(fmt::SymbolPosition::After);
    QCOMPARE(fmt::money(1234.56, QStringLiteral("USD")), QStringLiteral("1,234.56") + kNbsp + "$");      // #51
    QCOMPARE(fmt::money(123.45, QStringLiteral("GBp")), QStringLiteral("123.45p"));                      // #55
    fs().set_symbol_spacing(fmt::SymbolSpacing::None);
    QCOMPARE(fmt::money(1234.56, QStringLiteral("USD")), QStringLiteral("1,234.56$"));                   // #52
    fs().set_symbol_position(fmt::SymbolPosition::Before);
    QCOMPARE(fmt::money(1234.5, QStringLiteral("CHF")), QStringLiteral("CHF1,234.50"));                  // #53
    fs().set_symbol_spacing(fmt::SymbolSpacing::Auto);
    QCOMPARE(fmt::money(1234.5, QStringLiteral("CHF")), QStringLiteral("CHF 1,234.50"));                 // #54
    // Locale position + Space / None.
    fs().set_symbol_position(fmt::SymbolPosition::Locale);
    fs().set_symbol_spacing(fmt::SymbolSpacing::Space);
    QCOMPARE(fmt::money(5.0, QStringLiteral("USD")), QStringLiteral("$") + kNbsp + "5.00");
    fs().set_symbol_spacing(fmt::SymbolSpacing::None);
    fs().set_locale(kDeDe);
    QCOMPARE(fmt::money(5.0, QStringLiteral("EUR")), QStringLiteral("5,00\u20AC"));
}

void TstNumberFormat::negative_style() {
    fs().set_negative_style(fmt::NegativeStyle::Parentheses);
    QCOMPARE(fmt::money(-1234.56, QStringLiteral("USD")), QStringLiteral("($1,234.56)"));                  // #56
    QCOMPARE(fmt::signed_money(1234.5, QStringLiteral("USD")), QStringLiteral("+$1,234.50"));              // #57
    QCOMPARE(fmt::signed_money(-1234.5, QStringLiteral("USD")), QStringLiteral("($1,234.50)"));            // #58
    QCOMPARE(fmt::money(-0.001, QStringLiteral("USD")), QStringLiteral("$0.00"));                          // #59
    QCOMPARE(fmt::money(-50.5, QStringLiteral("GBp")), QStringLiteral("(50.50p)"));                        // #61
    QCOMPARE(fmt::money(-1.5e9, QStringLiteral("USD"), fmt::Compact::KMB), QStringLiteral("($1.50B)"));    // #63
    QCOMPARE(fmt::price(-37.63, {QStringLiteral("USD")}), QStringLiteral("-37.63"));                       // #64
    QCOMPARE(fmt::price_with_symbol(-2.5, {QStringLiteral("USD")}), QStringLiteral("-$2.50"));
    QCOMPARE(fmt::pct(-1.5), QStringLiteral("-1.50%"));                                                    // #65
    QCOMPARE(fmt::number(-3.0), QStringLiteral("-3.00"));
    QCOMPARE(fmt::money_in_column(-5.0, QStringLiteral("EUR"), true), QStringLiteral("(EUR 5.00)"));
    fs().set_locale(kDeDe);
    QCOMPARE(fmt::money(-1234.56, QStringLiteral("EUR")), QStringLiteral("(1.234,56") + kNbsp + "\u20AC)"); // #60
    fs().set_locale(kEnUs);
    fs().set_symbol_style(fmt::SymbolStyle::Code);
    QCOMPARE(fmt::money(-1234.56, QStringLiteral("USD")), QStringLiteral("(USD 1,234.56)"));               // #62
}

void TstNumberFormat::grouping() {
    const QLocale es(QLocale::Spanish, QLocale::Spain);
    fs().set_locale(es);
    QCOMPARE(fmt::money(1234.5, QStringLiteral("EUR")), QStringLiteral("1.234,50") + kNbsp + "\u20AC");   // #66
    QCOMPARE(fmt::number(1234.5, 1), QStringLiteral("1.234,5"));
    fs().set_grouping(fmt::Grouping::Locale);
    QCOMPARE(fmt::money(1234.5, QStringLiteral("EUR")), QStringLiteral("1234,50") + kNbsp + "\u20AC");    // #67
    QCOMPARE(fmt::money(12345.5, QStringLiteral("EUR")), QStringLiteral("12.345,50") + kNbsp + "\u20AC");
    fs().set_locale(kEnUs);
    fs().set_grouping(fmt::Grouping::Indian);
    QCOMPARE(fmt::number(12345678.9), QStringLiteral("1,23,45,678.90"));                                  // #68
    QCOMPARE(fmt::number(123.0), QStringLiteral("123.00"));
    fs().set_grouping(fmt::Grouping::None);
    QCOMPARE(fmt::money(1234567.891, QStringLiteral("USD")), QStringLiteral("$1234567.89"));              // #69
    QCOMPARE(fmt::compact(5e11, fmt::Compact::LakhCrore), QStringLiteral("50000 Cr"));
    fs().set_grouping(fmt::Grouping::Always);
    fs().set_locale(QLocale(QLocale::English, QLocale::India));
    QCOMPARE(fmt::money(12345678.9, QStringLiteral("INR")), QStringLiteral("\u20B91,23,45,678.90"));      // #70
    const QLocale pl(QLocale::Polish, QLocale::Poland);
    fs().set_locale(pl);
    const QString pl_money = fmt::money(1234.5, QStringLiteral("EUR"));                                    // #71
    QVERIFY2(pl_money.startsWith(QStringLiteral("1") + pl.groupSeparator() + QStringLiteral("234,50")),
             qPrintable(pl_money));
    QVERIFY2(pl_money.endsWith(QStringLiteral("\u20AC")), qPrintable(pl_money));
}

void TstNumberFormat::minor_units_major() {
    fs().set_minor_units(fmt::MinorUnits::Major);
    QCOMPARE(fmt::money(123.45, QStringLiteral("GBp")), QStringLiteral("\u00A31.2345"));                   // #72
    QCOMPARE(fmt::price_with_symbol(117.7, {QStringLiteral("GBp")}), QStringLiteral("\u00A31.1770"));      // #73
    QCOMPARE(fmt::money(73550.0, QStringLiteral("ZAc")), QStringLiteral("R 735.50"));                      // #74
    QCOMPARE(fmt::money(14150.0, QStringLiteral("ILA")), QStringLiteral("\u20AA141.50"));                  // #75
    QCOMPARE(fmt::money(448.5, QStringLiteral("USX")), QStringLiteral("$4.4850"));                         // #76
    QCOMPARE(fmt::price(117.7, {QStringLiteral("GBp")}), QStringLiteral("117.70"));                        // #77
    QCOMPARE(fmt::money(1500.0, QStringLiteral("KWF")), QStringLiteral("KWD 1.500"));
}

void TstNumberFormat::mixed_columns() {
    QCOMPARE(fmt::money_in_column(1234.5, QStringLiteral("USD"), true), QStringLiteral("USD 1,234.50"));    // #78
    QCOMPARE(fmt::money_in_column(1234.5, QStringLiteral("USD"), false), QStringLiteral("$1,234.50"));      // #79
    QCOMPARE(fmt::money_in_column(117.7, QStringLiteral("GBp"), true), QStringLiteral("GBp 117.70"));       // #81
    QCOMPARE(fmt::money_in_column(-5.0, QStringLiteral("EUR"), true, fmt::Compact::None, -1, true),
             QStringLiteral("-EUR 5.00"));                                                                  // #82
    QCOMPARE(fmt::money_in_column(1.5e9, QStringLiteral("USD"), true, fmt::Compact::KMB),
             QStringLiteral("USD 1.50B"));
    // #80 with decision 6: without codes, a mixed column still never shows a bare
    // "$" that another dollar could share.
    fs().set_code_when_mixed(false);
    QCOMPARE(fmt::money_in_column(1234.5, QStringLiteral("USD"), true), QStringLiteral("US$1,234.50"));     // #80
    QCOMPARE(fmt::money_in_column(1234.5, QStringLiteral("EUR"), true), QStringLiteral("\u20AC1,234.50"));
}

void TstNumberFormat::overrides() {
    QVERIFY(fs().set_currency_override(QStringLiteral("CAD"), {QStringLiteral("$")}));
    QCOMPARE(fmt::money(10.0, QStringLiteral("CAD")), QStringLiteral("$10.00"));                    // #83
    fs().set_symbol_style(fmt::SymbolStyle::Code);
    QCOMPARE(fmt::money(10.0, QStringLiteral("CAD")), QStringLiteral("CAD 10.00"));                 // #84
    fs().set_symbol_style(fmt::SymbolStyle::Symbol);
    QVERIFY(fs().set_currency_override(QStringLiteral("JPY"), {QString(), 2}));
    QCOMPARE(fmt::money(1234.5, QStringLiteral("JPY")), QStringLiteral("\u00A51,234.50"));          // #85
    QCOMPARE(fmt::currency_decimals(QStringLiteral("JPY")), 2);
    QVERIFY(fs().set_currency_override(QStringLiteral("XYZ"), {QStringLiteral("\u04FE"), 4}));
    QCOMPARE(fmt::money(1.5, QStringLiteral("XYZ")), QStringLiteral("\u04FE1.5000"));               // #86
    fs().remove_currency_override(QStringLiteral("XYZ"));
    QCOMPARE(fmt::money(1.5, QStringLiteral("XYZ")), QStringLiteral("XYZ 1.50"));                   // #87
    QVERIFY(!fs().set_currency_override(QStringLiteral("US"), {QStringLiteral("$")}));              // #88
    QVERIFY(!fs().set_currency_override(QStringLiteral("CAD"), {QStringLiteral("12")}));
    QVERIFY(!fs().set_currency_override(QStringLiteral("CAD"), {QStringLiteral("$"), 11}));
    QVERIFY(fs().set_currency_override(QStringLiteral("usd"), {QStringLiteral("US$")}));
    QVERIFY(fs().currency_overrides().contains(QStringLiteral("USD")));
    QCOMPARE(fs().currency_overrides().value(QStringLiteral("USD")).symbol, QStringLiteral("US$"));
    // Gaps: an override equal to the table is removed; a custom code cannot
    // shadow a known one; a custom display currency; '%' and controls rejected.
    QVERIFY(fs().set_currency_override(QStringLiteral("CAD"), {QStringLiteral("C$")}));
    QVERIFY(!fs().currency_overrides().contains(QStringLiteral("CAD")));
    QVERIFY(!fs().set_currency_override(QStringLiteral("CAD"), {QStringLiteral("C%")}));
    QVERIFY(!fs().set_currency_override(QStringLiteral("CAD"), {QStringLiteral("C\u200E$")}));
    QVERIFY(!fs().set_display_currency(QStringLiteral("TOKN")));
    QVERIFY(fs().set_currency_override(QStringLiteral("tokn"), {QStringLiteral("T"), 3, QStringLiteral("Token")}));
    QVERIFY(fs().set_display_currency(QStringLiteral("TOKN")));
    QCOMPARE(fs().display_currency(), QStringLiteral("TOKN"));
    QCOMPARE(fmt::currency_name(QStringLiteral("TOKN")), QStringLiteral("Token"));
    QCOMPARE(fmt::money(1.5, QStringLiteral("TOKN")), QStringLiteral("T 1.500"));
    fs().remove_currency_override(QStringLiteral("TOKN")); // the display currency falls back to Automatic
    QVERIFY(fs().display_currency_is_auto());
}

void TstNumberFormat::not_money() {
    QCOMPARE(fmt::price_with_symbol(159.452, fmt::instrument_for_symbol(QStringLiteral("USDJPY=X"))),
             QStringLiteral("159.452"));                                                                     // #89
    QVERIFY(!fmt::is_money_quote(QStringLiteral("INDEX")));                                                  // #90
    QVERIFY(!fmt::is_money_quote(QStringLiteral("CURRENCY")));
    QVERIFY(fmt::is_money_quote(QStringLiteral("EQUITY")));
    QVERIFY(fmt::is_money_quote(QStringLiteral("FUTURE"), QStringLiteral("ZC=F")));
    QVERIFY(!fmt::is_money_quote(QString(), QStringLiteral("^GSPC")));                                       // #91
    QVERIFY(!fmt::is_money_quote(QString(), QStringLiteral("EURUSD=X")));
    QVERIFY(fmt::is_money_quote(QString(), QStringLiteral("AAPL")));
}

namespace {
fmt::ConvertedMoney sample_conversion() {
    fmt::ConvertedMoney c;
    c.amount = 13183.0;
    c.currency = QStringLiteral("USD");
    c.source_amount = 10000.0;
    c.source_currency = QStringLiteral("GBP");
    c.rate = 1.3183;
    c.as_of = QDateTime(QDate(2026, 11, 16), QTime(14, 32), QTimeZone(QTimeZone::UTC));
    c.converted = true;
    return c;
}
} // namespace

void TstNumberFormat::converted_figures() {
    fmt::ConvertedMoney c = sample_conversion();
    QCOMPARE(fmt::converted_money(c), QStringLiteral("$13,183.00"));                                        // #92
    const QString note = QStringLiteral("1 GBP = 1.3183 USD \u00B7 Nov 16, 2026 14:32");
    QCOMPARE(fmt::fx_note(c), note);                                                                         // #95
    c.triangulated = true;
    QCOMPARE(fmt::fx_note(c), note + QStringLiteral(" \u00B7 via USD"));                                    // #96
    c.triangulated = false;
    c.stale = true;
    QCOMPARE(fmt::converted_money(c), QStringLiteral("\u2248\u00A0$13,183.00"));                            // #93
    QCOMPARE(fmt::fx_note(c), note + QStringLiteral(" \u00B7 stale"));                                      // #97
    c.stale = false;
    c.source_currency = QStringLiteral("GBp");
    QVERIFY(fmt::fx_note(c).startsWith(QStringLiteral("1 GBP = ")));                                       // #99
    c.converted = false;
    QCOMPARE(fmt::fx_note(c), QString());                                                                    // #98
    c.amount = std::numeric_limits<double>::quiet_NaN();
    QCOMPARE(fmt::converted_money(c), fmt::placeholder());                                                   // #94
    // Stablecoin taken 1:1 (decision 4): approximate, with the reason.
    fmt::ConvertedMoney p;
    p.amount = 500.0;
    p.currency = QStringLiteral("USD");
    p.source_amount = 500.0;
    p.source_currency = QStringLiteral("USDT");
    p.rate = 1.0;
    p.converted = true;
    p.approximate = true;
    p.approx_reason = QStringLiteral("no USDT rate: 1 USDT taken as 1 USD");
    QCOMPARE(fmt::converted_money(p), QStringLiteral("\u2248\u00A0$500.00"));
    QCOMPARE(fmt::fx_note(p), QStringLiteral("1 USDT = 1.0000 USD \u00B7 no USDT rate: 1 USDT taken as 1 USD"));
    QCOMPARE(fmt::converted_money(sample_conversion(), fmt::Compact::KMB, true), QStringLiteral("+$13.2K"));
    // A tiny rate keeps its significant digits.
    fmt::ConvertedMoney v = sample_conversion();
    v.source_currency = QStringLiteral("VND");
    v.rate = 0.0000388261;
    QVERIFY2(fmt::fx_note(v).startsWith(QStringLiteral("1 VND = 0.000038826 USD")), qPrintable(fmt::fx_note(v)));
}

void TstNumberFormat::exports() {
    QCOMPARE(fmt::export_number(1234.5), QStringLiteral("1234.5"));                                         // #100
    QCOMPARE(fmt::export_number(1e-7), QStringLiteral("0.0000001"));                                        // #101
    QCOMPARE(fmt::export_number(1234567.891, 2), QStringLiteral("1234567.89"));                             // #102
    QCOMPARE(fmt::export_number(std::numeric_limits<double>::quiet_NaN()), QString());                      // #103
    fs().set_locale(kDeDe);
    QCOMPARE(fmt::export_number(1234.5), QStringLiteral("1234.5"));                                         // #104
    fs().set_locale(QLocale(QLocale::Swedish, QLocale::Sweden));
    QCOMPARE(fmt::export_number(-5.0), QStringLiteral("-5"));
    fs().set_locale(kEnUs);
    const auto em = fmt::export_money(117.7, QStringLiteral("GBp"));                                         // #105
    QCOMPARE(em.amount, QStringLiteral("117.7"));
    QCOMPARE(em.currency, QStringLiteral("GBp"));
    QCOMPARE(fmt::export_converted(sample_conversion()),                                                    // #106
             (QStringList{"13183", "USD", "10000", "GBP", "1.3183", "2026-11-16T14:32:00Z"}));
    // Gaps: 0.1 + 0.2 is not "0.30000000000000004"; no exponent for large values; -0.
    QCOMPARE(fmt::export_number(0.1 + 0.2), QStringLiteral("0.3"));
    QCOMPARE(fmt::export_number(1e20), QStringLiteral("100000000000000000000"));
    QCOMPARE(fmt::export_number(-0.0), QStringLiteral("0"));
    QCOMPARE(fmt::export_number(2.5, 2), QStringLiteral("2.5"));
}

void TstNumberFormat::axes() {
    QCOMPARE(fmt::axis_money(1500000.0, QStringLiteral("USD")), QStringLiteral("$1.5M"));                   // #107
    QCOMPARE(fmt::axis_money(-2.5e7, QStringLiteral("INR")), QStringLiteral("-\u20B92.5 Cr"));              // #108
    QCOMPARE(fmt::axis_money(0.0, QStringLiteral("USD")), QStringLiteral("$0"));                            // #110
    QCOMPARE(fmt::axis_money(950.0, QStringLiteral("GBP")), QStringLiteral("\u00A3950"));
    QCOMPARE(fmt::axis_printf_format(QStringLiteral("USD"), 0), QStringLiteral("$%.0f"));                   // #111
    fs().set_negative_style(fmt::NegativeStyle::Parentheses);
    QCOMPARE(fmt::axis_money(-2.5e7, QStringLiteral("INR")), QStringLiteral("-\u20B92.5 Cr"));              // #109
    fs().set_locale(kDeDe);
    QCOMPARE(fmt::axis_printf_format(QStringLiteral("EUR"), 2), QStringLiteral("%.2f") + kNbsp + "\u20AC"); // #112
    // Gaps: pence on a money axis are pounds; a unit with '%' is escaped.
    fs().set_locale(kEnUs);
    QCOMPARE(fmt::axis_money(1.5e6, QStringLiteral("GBp")), QStringLiteral("\u00A315K"));
    QVERIFY(fs().set_currency_override(QStringLiteral("EUR"), {QStringLiteral("E%")}) == false);
    QCOMPARE(fmt::axis_money(950.5, QStringLiteral("USD")), QStringLiteral("$950.5"));
}

void TstNumberFormat::never_converted() {
    QCOMPARE(fmt::vendor_cost(0.01234), QStringLiteral("$0.0123"));                                         // #113
    QCOMPARE(fmt::plan_price(10.0), QStringLiteral("$10"));                                                 // #114
    QCOMPARE(fmt::plan_price(9.99), QStringLiteral("$9.99"));
    fs().set_symbol_style(fmt::SymbolStyle::Code);
    QCOMPARE(fmt::vendor_cost(0.01234), QStringLiteral("USD 0.0123"));
    fs().set_symbol_style(fmt::SymbolStyle::Symbol);
    QVERIFY(fs().set_display_currency(QStringLiteral("EUR")));
    QCOMPARE(fmt::vendor_cost(0.5, 2), QStringLiteral("$0.50"));                                            // #115
    // A Canadian user must not read a USD price as CAD.
    QVERIFY(fs().set_display_currency(QStringLiteral("CAD")));
    QCOMPARE(fmt::plan_price(10.0), QStringLiteral("US$10"));
}

void TstNumberFormat::bidi_marks_removed() {
    for (const QLocale& loc : {QLocale(QLocale::Hebrew, QLocale::Israel), QLocale(QLocale::Arabic, QLocale::SaudiArabia),
                               QLocale(QLocale::Persian, QLocale::Iran)}) {
        fs().set_locale(loc);
        for (const QString& text : {fmt::money(-1234567.891, QStringLiteral("USD")), fmt::signed_number(-5.0),     // #116
                                    fmt::pct(-1.5), fmt::money(12.5, QStringLiteral("EUR"))}) {
            QVERIFY2(!text.contains(QChar(0x200E)) && !text.contains(QChar(0x200F)) && !text.contains(QChar(0x061C)),
                     qPrintable(loc.bcp47Name() + ": " + text));
        }
    }
}

void TstNumberFormat::settings_object() {
    auto& s = fs();
    QCOMPARE(fmt::FormatSettings::setting_keys().size(), 16);                                                 // #117
    QSignalSpy spy(&s, &fmt::FormatSettings::changed);

    QHash<QString, QString> all;                                                                              // #118
    all.insert(QStringLiteral("appearance.number_locale"), QStringLiteral("de-CH"));
    all.insert(QStringLiteral("appearance.compact_numbers"), QStringLiteral("kmb"));
    all.insert(QStringLiteral("appearance.display_timezone"), QStringLiteral("utc"));
    all.insert(QStringLiteral("appearance.clock"), QStringLiteral("12h"));
    all.insert(QStringLiteral("appearance.display_currency"), QStringLiteral("EUR"));
    all.insert(QStringLiteral("appearance.currency_symbol_style"), QStringLiteral("code"));
    all.insert(QStringLiteral("appearance.currency_symbol_position"), QStringLiteral("after"));
    all.insert(QStringLiteral("appearance.currency_symbol_spacing"), QStringLiteral("space"));
    all.insert(QStringLiteral("appearance.negative_style"), QStringLiteral("parentheses"));
    all.insert(QStringLiteral("appearance.number_grouping"), QStringLiteral("indian"));
    all.insert(QStringLiteral("appearance.money_decimals"), QStringLiteral("4"));
    all.insert(QStringLiteral("appearance.minor_units"), QStringLiteral("major"));
    all.insert(QStringLiteral("appearance.code_when_mixed"), QStringLiteral("0"));
    all.insert(QStringLiteral("appearance.fx_stale_minutes"), QStringLiteral("90"));
    all.insert(QStringLiteral("appearance.fx_pnl_split"), QStringLiteral("0"));
    all.insert(QStringLiteral("appearance.currency_overrides"), QStringLiteral("{\"CAD\":{\"s\":\"$\"}}"));
    QCOMPARE(all.size(), fmt::FormatSettings::setting_keys().size());
    s.apply_settings_values(all);
    QCOMPARE(spy.count(), 1);
    QCOMPARE(s.settings_values(), all);
    s.apply_settings_values(all);
    QCOMPARE(spy.count(), 1);

    s.set_values(fmt::FormatSettings::Values{});
    QCOMPARE(spy.count(), 2);
    spy.clear();
    fmt::FormatSettings::Values v = s.values();                                                               // #119
    v.symbol_style = fmt::SymbolStyle::Disambiguated;
    v.negative_style = fmt::NegativeStyle::Parentheses;
    v.grouping = fmt::Grouping::None;
    v.fx_stale_minutes = 5;
    v.display_currency = QStringLiteral("jpy");
    s.set_values(v);
    QCOMPARE(spy.count(), 1);
    QCOMPARE(s.display_currency(), QStringLiteral("JPY"));
    s.set_values(s.values());
    QCOMPARE(spy.count(), 1);

    s.set_symbol_style(s.symbol_style());                                                                     // #120
    s.set_negative_style(fmt::NegativeStyle::Parentheses);
    s.set_fx_stale_minutes(5);
    s.set_code_when_mixed(s.code_when_mixed());
    s.set_money_decimals(s.money_decimals_policy(), s.fixed_decimals());
    QCOMPARE(spy.count(), 1);
    s.set_fx_pnl_split(false);
    QCOMPARE(spy.count(), 2);
    QVERIFY(!s.fx_pnl_split());
}

void TstNumberFormat::settings_validation() {
    auto& s = fs();
    QSignalSpy spy(&s, &fmt::FormatSettings::changed);
    QVERIFY(s.set_display_currency(QStringLiteral("INR")));
    QCOMPARE(spy.count(), 1);
    QVERIFY(!s.set_display_currency(QStringLiteral("GBp")));                                                  // #121
    QVERIFY(!s.set_display_currency(QStringLiteral("XX")));
    QVERIFY(!s.set_display_currency(QStringLiteral("zzz")));
    QVERIFY(!s.set_display_currency(QStringLiteral("XAU"))); // nothing converts into gold
    QCOMPARE(s.display_currency(), QStringLiteral("INR"));
    QCOMPARE(spy.count(), 1);
    s.set_auto_display_currency(QStringLiteral("GBP"));                                                       // #123
    QCOMPARE(s.display_currency(), QStringLiteral("INR"));
    QCOMPARE(spy.count(), 1);

    QVERIFY(s.set_display_currency(QString()));                                                               // #122
    QCOMPARE(spy.count(), 2);
    s.set_auto_display_currency(QStringLiteral("EUR"));
    QCOMPARE(s.display_currency(), QStringLiteral("EUR"));
    QVERIFY(s.display_currency_is_auto());
    QCOMPARE(spy.count(), 3);
    s.set_auto_display_currency(QStringLiteral("EUR"));
    s.set_auto_display_currency(QStringLiteral("GBp")); // a minor unit is never a display currency
    QCOMPARE(s.display_currency(), QStringLiteral("EUR"));
    QCOMPARE(spy.count(), 3);

    QHash<QString, fmt::CurrencyOverride> ov;                                                                 // #124
    ov.insert(QStringLiteral("CAD"), {QStringLiteral("$")});
    ov.insert(QStringLiteral("XYZ"), {QStringLiteral("\u04FE"), 4, QStringLiteral("My token")});
    const QString json = fmt::FormatSettings::overrides_to_json(ov);
    QCOMPARE(fmt::FormatSettings::overrides_from_json(json), ov);
    QCOMPARE(fmt::FormatSettings::overrides_to_json({}), QString());

    QHash<QString, QString> bad;                                                                              // #125
    bad.insert(QStringLiteral("appearance.currency_overrides"), QStringLiteral("{not json"));
    bad.insert(QStringLiteral("appearance.negative_style"), QStringLiteral("parentheses"));
    s.apply_settings_values(bad);
    QVERIFY(s.currency_overrides().isEmpty());
    QCOMPARE(s.negative_style(), fmt::NegativeStyle::Parentheses);

    s.set_fx_stale_minutes(0);                                                                                // #126
    QCOMPARE(s.fx_stale_minutes(), 1);
    s.set_fx_stale_minutes(5000);
    QCOMPARE(s.fx_stale_minutes(), 1440);

    QHash<QString, QString> dec;                                                                              // #127
    dec.insert(QStringLiteral("appearance.money_decimals"), QStringLiteral("3"));
    s.apply_settings_values(dec);
    QCOMPARE(s.money_decimals_policy(), fmt::DecimalsPolicy::Fixed);
    QCOMPARE(s.fixed_decimals(), 3);
    dec.insert(QStringLiteral("appearance.money_decimals"), QStringLiteral("adaptive"));
    s.apply_settings_values(dec);
    QCOMPARE(s.money_decimals_policy(), fmt::DecimalsPolicy::Adaptive);
    dec.insert(QStringLiteral("appearance.money_decimals"), QStringLiteral("banana"));
    s.apply_settings_values(dec);
    QCOMPARE(s.money_decimals_policy(), fmt::DecimalsPolicy::CurrencyDefault);
    // Unknown enum strings and out-of-range numbers fall back / clamp.
    QHash<QString, QString> junk;
    junk.insert(QStringLiteral("appearance.currency_symbol_style"), QStringLiteral("hieroglyph"));
    junk.insert(QStringLiteral("appearance.fx_stale_minutes"), QStringLiteral("99999"));
    junk.insert(QStringLiteral("appearance.display_currency"), QStringLiteral("GBp"));
    junk.insert(QStringLiteral("appearance.currency_overrides"),
                QStringLiteral("{\"CAD\":{\"s\":\"1$\"},\"JPY\":{\"d\":2},\"x\":{\"s\":\"q\"}}"));
    s.apply_settings_values(junk);
    QCOMPARE(s.symbol_style(), fmt::SymbolStyle::Symbol);
    QCOMPARE(s.fx_stale_minutes(), 1440);
    QVERIFY(s.display_currency_is_auto());
    QCOMPARE(s.currency_overrides().keys(), QList<QString>{QStringLiteral("JPY")}); // invalid entries dropped
}

void TstNumberFormat::settings_migration_helpers() {
    using FS = fmt::FormatSettings;
    // Decision 8: the old Settings > General choice carries over unless it was the default.
    QCOMPARE(FS::migrate_legacy_display_currency(QStringLiteral("EUR")), QStringLiteral("EUR"));
    QCOMPARE(FS::migrate_legacy_display_currency(QStringLiteral("inr")), QStringLiteral("INR"));
    QCOMPARE(FS::migrate_legacy_display_currency(QStringLiteral("USD")), QString());
    QCOMPARE(FS::migrate_legacy_display_currency(QString()), QString());
    QCOMPARE(FS::migrate_legacy_display_currency(QStringLiteral("GBp")), QString());
    QCOMPARE(FS::migrate_legacy_display_currency(QStringLiteral("Fr")), QString());
    // Decision 1: Automatic = the chosen region's currency, else the system
    // locale's, else the inferred region's, else USD.
    QCOMPARE(FS::resolve_auto_display_currency(true, QStringLiteral("EUR"), kEnUs), QStringLiteral("EUR"));
    QCOMPARE(FS::resolve_auto_display_currency(false, QStringLiteral("EUR"), QLocale(QLocale::German, QLocale::Switzerland)),
             QStringLiteral("CHF"));
    QCOMPARE(FS::resolve_auto_display_currency(false, QStringLiteral("INR"), QLocale(QLocale::English, QLocale::India)),
             QStringLiteral("INR"));
    QCOMPARE(FS::resolve_auto_display_currency(false, QStringLiteral("CNY"), QLocale::c()), QStringLiteral("CNY"));
    QCOMPARE(FS::resolve_auto_display_currency(false, QString(), QLocale::c()), QStringLiteral("USD"));
    QCOMPARE(FS::resolve_auto_display_currency(true, QStringLiteral("GBp"), kDeDe), QStringLiteral("EUR"));
}

void TstNumberFormat::system_locale_currency_pattern() {
    // Following the app with no app locale set means the OS locale, whose
    // currency formatting ignores the requested precision: the placement
    // pattern must not pick up its decimals ("$10.00.00").
    fs().follow_app_locale();
    const QString m = usd(10.0);
    const QString dp = QLocale().decimalPoint();
    QVERIFY2(m.count(dp) == 1, qPrintable(m));
    QVERIFY2(m.contains(QStringLiteral("10")) && m.contains(QStringLiteral("$")), qPrintable(m));
}

void TstNumberFormat::rounding_nudge() {
    QCOMPARE(usd(2.675), QStringLiteral("$2.68"));                                                            // #128
    QCOMPARE(usd(1.005), QStringLiteral("$1.01"));
    // The nudge never shows up in large amounts.
    QCOMPARE(usd(1e13), QStringLiteral("$10,000,000,000,000.00"));
    QCOMPARE(usd(0.0), QStringLiteral("$0.00"));
    QCOMPARE(usd(-2.675), QStringLiteral("-$2.68"));
}

void TstNumberFormat::money_cost() {
    // The design budgets a few microseconds per money() call (10,000 cells per
    // event-loop turn). Measured, not asserted tightly: fail only if a call
    // costs more than 50 µs on average.
    QElapsedTimer t;
    t.start();
    constexpr int kN = 20000;
    int len = 0;
    for (int i = 0; i < kN; ++i)
        len += fmt::money(1234.5 + i, (i & 1) ? QStringLiteral("USD") : QStringLiteral("GBp")).size();
    const double us = double(t.nsecsElapsed()) / 1000.0 / kN;
    qInfo("money(): %.2f us per call (%d chars)", us, len);
    QVERIFY2(us < 50.0, qPrintable(QString::number(us)));
}

QTEST_GUILESS_MAIN(TstNumberFormat)
#include "tst_number_format.moc"
