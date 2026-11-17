#include "ui/formatting/NumberFormat.h"

#include "ui/formatting/DateFormat.h"
#include "ui/formatting/ParseAmount.h"

#include <QCoreApplication>
#include <QHash>
#include <QLocale>
#include <QVector>

#include <algorithm>
#include <cfloat>
#include <cmath>

namespace fincept::ui::formatting {

namespace numfmt_detail {

constexpr int kNumfmtMaxDecimals = 10;
/// Binary doubles store 2.675 as 2.67499999…; a few ULPs push such values onto
/// the decimal they were typed as before rounding. Applied only while the
/// shift stays far below the last printed decimal.
constexpr double kNumfmtRoundingNudge = 4.0 * DBL_EPSILON;
constexpr double kNumfmtNudgeLimit = 1e12;

int numfmt_clamp_decimals(int d) {
    return std::clamp(d, 0, kNumfmtMaxDecimals);
}

double numfmt_pow10(int d) {
    static const double kPow[] = {1.0, 1e1, 1e2, 1e3, 1e4, 1e5, 1e6, 1e7, 1e8, 1e9, 1e10};
    return kPow[numfmt_clamp_decimals(d)];
}

double numfmt_round_to(double a, int d) {
    const double p = numfmt_pow10(d);
    return std::round(a * p) / p;
}

bool numfmt_rounds_to_zero(double a, int d) {
    return std::round(std::fabs(a) * numfmt_pow10(d)) == 0.0;
}

double numfmt_nudged(double a, int d) {
    return a * numfmt_pow10(d) < kNumfmtNudgeLimit ? a * (1.0 + kNumfmtRoundingNudge) : a;
}

int numfmt_log10_int(int divisor) {
    int n = 0;
    for (int x = divisor; x >= 10; x /= 10)
        ++n;
    return n;
}

QString numfmt_tr(const char* source, const char* comment) {
    return QCoreApplication::translate("fincept::ui::formatting::NumberFormat", source, comment);
}

/// Bidi controls (LRM, RLM, ALM) around signs and currency patterns break
/// column alignment and copy / paste; the terminal lays numbers out left-to-right.
QString numfmt_strip_bidi(QString s) {
    s.remove(QChar(0x200E));
    s.remove(QChar(0x200F));
    s.remove(QChar(0x061C));
    return s;
}

QString numfmt_minus(const QLocale& loc) {
    const QString m = numfmt_strip_bidi(loc.negativeSign());
    return m.isEmpty() ? QStringLiteral("-") : m;
}

QString numfmt_plus(const QLocale& loc) {
    const QString p = numfmt_strip_bidi(loc.positiveSign());
    return p.isEmpty() ? QStringLiteral("+") : p;
}

struct NumfmtGroupSizes {
    int primary = 3;   ///< digits in the group next to the decimal point
    int secondary = 3; ///< digits in every group further left (2 for en_IN)
};

/// What the digit formatter needs from a locale, worked out once per locale
/// and thread (QLocale copies with other options allocate; comparing two
/// QLocale values is a pointer compare).
struct NumfmtLocaleInfo {
    bool valid = false;
    QLocale src = QLocale::c();
    QLocale bare = QLocale::c(); ///< src without group separators
    QString decimal_point;
    QString group_sep;
    NumfmtGroupSizes sizes; ///< read from the locale's own rendering of 123456789
};

const NumfmtLocaleInfo& numfmt_locale_info(const QLocale& loc) {
    thread_local NumfmtLocaleInfo info;
    if (info.valid && loc == info.src)
        return info;
    info.valid = true;
    info.src = loc;
    info.bare = loc;
    info.bare.setNumberOptions(loc.numberOptions() | QLocale::OmitGroupSeparator);
    info.decimal_point = loc.decimalPoint();
    info.group_sep = loc.groupSeparator();
    if (info.group_sep.isEmpty())
        info.group_sep = QStringLiteral(",");
    NumfmtGroupSizes g;
    QLocale grouped = loc;
    grouped.setNumberOptions(grouped.numberOptions() & ~QLocale::OmitGroupSeparator);
    const QStringList parts =
        numfmt_strip_bidi(grouped.toString(qlonglong(123456789))).split(grouped.groupSeparator());
    if (!grouped.groupSeparator().isEmpty() && parts.size() >= 2) {
        g.primary = int(parts.last().size());
        g.secondary = parts.size() >= 3 ? int(parts.at(parts.size() - 2).size()) : g.primary;
    }
    if (g.primary < 2 || g.primary > 4)
        g.primary = 3;
    if (g.secondary < 2 || g.secondary > 4)
        g.secondary = g.primary;
    info.sizes = g;
    return info;
}

/// |a| with `d` decimals in the locale's digits and decimal point, grouped per
/// `g`. No sign.
QString numfmt_digits(double a, int d, const QLocale& loc, Grouping g) {
    d = numfmt_clamp_decimals(d);
    a = std::fabs(a);
    if (g == Grouping::Locale)
        return numfmt_strip_bidi(loc.toString(a, 'f', d));
    const NumfmtLocaleInfo& li = numfmt_locale_info(loc);
    const QString raw = numfmt_strip_bidi(li.bare.toString(a, 'f', d));
    if (g == Grouping::None)
        return raw;
    const qsizetype cut = (d > 0 && !li.decimal_point.isEmpty()) ? raw.lastIndexOf(li.decimal_point) : -1;
    const QStringView int_part = cut >= 0 ? QStringView(raw).left(cut) : QStringView(raw);
    const NumfmtGroupSizes sz = g == Grouping::Indian ? NumfmtGroupSizes{3, 2} : li.sizes;
    const qsizetype n = int_part.size();
    if (n <= sz.primary)
        return raw;
    for (const QChar c : int_part)
        if (c.isSurrogate()) // digits outside the BMP: let QLocale group them
            return numfmt_strip_bidi(loc.toString(a, 'f', d));
    // Groups from the left: a short leading group, then `secondary`-sized
    // groups, then the `primary` group next to the decimal point.
    const qsizetype rest = n - sz.primary;
    qsizetype lead = rest % sz.secondary;
    if (lead == 0)
        lead = sz.secondary;
    QString out;
    out.reserve(raw.size() + (n / 2 + 1) * li.group_sep.size());
    out.append(int_part.left(lead));
    for (qsizetype pos = lead; pos < rest; pos += sz.secondary) {
        out.append(li.group_sep);
        out.append(int_part.mid(pos, sz.secondary));
    }
    out.append(li.group_sep);
    out.append(int_part.mid(rest));
    if (cut >= 0)
        out.append(QStringView(raw).mid(cut));
    return out;
}

/// Removes trailing zero digits after the decimal point, keeping at least
/// `min_decimals`; drops the decimal point when none remain.
QString numfmt_trim_zeros(QString s, const QLocale& loc, int min_decimals) {
    const QString dp = loc.decimalPoint();
    const qsizetype p = dp.isEmpty() ? -1 : s.lastIndexOf(dp);
    if (p < 0)
        return s;
    const QString zero = loc.zeroDigit();
    qsizetype frac = s.size() - (p + dp.size());
    while (frac > min_decimals && !zero.isEmpty() && s.endsWith(zero)) {
        s.chop(zero.size());
        --frac;
    }
    if (frac == 0)
        s.chop(dp.size());
    return s;
}

/// Grouped |v| with `d` decimals and an optional sign. A value that rounds to
/// zero is printed unsigned ("0.00", not "-0.00").
QString numfmt_signed_body(double v, int d, bool show_plus, const QLocale& loc, Grouping g) {
    d = numfmt_clamp_decimals(d);
    const double a = std::fabs(v);
    const QString body = numfmt_digits(a, d, loc, g);
    if (numfmt_rounds_to_zero(a, d))
        return body;
    if (v < 0)
        return numfmt_minus(loc) + body;
    return show_plus ? numfmt_plus(loc) + body : body;
}

struct NumfmtText {
    const char* source;  // "%1K"
    const char* comment; // disambiguation, part of the translation key
};

struct NumfmtUnit {
    double div;
    NumfmtText text;
};

// clang-format off
const NumfmtUnit kNumfmtKmbUnits[] = {
    {1e3,  QT_TRANSLATE_NOOP3("fincept::ui::formatting::NumberFormat", "%1K", "number abbreviation: thousand")},
    {1e6,  QT_TRANSLATE_NOOP3("fincept::ui::formatting::NumberFormat", "%1M", "number abbreviation: million")},
    {1e9,  QT_TRANSLATE_NOOP3("fincept::ui::formatting::NumberFormat", "%1B", "number abbreviation: billion")},
    {1e12, QT_TRANSLATE_NOOP3("fincept::ui::formatting::NumberFormat", "%1T", "number abbreviation: trillion")},
};
const NumfmtUnit kNumfmtLakhUnits[] = {
    {1e3,  QT_TRANSLATE_NOOP3("fincept::ui::formatting::NumberFormat", "%1K", "number abbreviation: thousand")},
    {1e5,  QT_TRANSLATE_NOOP3("fincept::ui::formatting::NumberFormat", "%1 L", "number abbreviation: lakh (100,000)")},
    {1e7,  QT_TRANSLATE_NOOP3("fincept::ui::formatting::NumberFormat", "%1 Cr", "number abbreviation: crore (10,000,000)")},
    {1e12, QT_TRANSLATE_NOOP3("fincept::ui::formatting::NumberFormat", "%1 L Cr", "number abbreviation: lakh crore (10^12)")},
};
// clang-format on
constexpr int kNumfmtUnitCount = 4;

/// Unsigned abbreviated body for |a|. `decimals` < 0 picks 1 for K and 2 for
/// larger units; below 1,000 `small_decimals` applies (< 0: none for whole
/// numbers, else 2). `trim` drops trailing zeros (chart axes). Sets *zero
/// when the result rounds to zero.
QString numfmt_compact_body(double a, bool lakh, int decimals, int small_decimals, const QLocale& loc, Grouping g,
                            bool trim, bool* zero) {
    const NumfmtUnit* units = lakh ? kNumfmtLakhUnits : kNumfmtKmbUnits;
    a = std::fabs(a);
    *zero = false;

    int u = -1;
    for (int i = kNumfmtUnitCount - 1; i >= 0; --i) {
        if (a >= units[i].div) {
            u = i;
            break;
        }
    }
    if (u < 0) {
        int d = small_decimals >= 0 ? small_decimals : (decimals >= 0 ? decimals : (a == std::floor(a) ? 0 : 2));
        d = numfmt_clamp_decimals(d);
        if (numfmt_round_to(a, d) < units[0].div) {
            *zero = numfmt_rounds_to_zero(a, d);
            const QString body = numfmt_digits(a, d, loc, g);
            return trim ? numfmt_trim_zeros(body, loc, 0) : body;
        }
        u = 0; // 999.999 rounds up to 1,000: show it as 1.0K
    }
    for (;;) {
        const double scaled = a / units[u].div;
        // Automatic precision keeps about four significant digits: the lakh
        // scheme has a 100,000x gap between crore and lakh crore, so a fixed
        // two decimals would print "80,775.00 Cr".
        int auto_d = u == 0 ? 1 : 2;
        if (scaled >= 1000.0)
            auto_d = 0;
        else if (scaled >= 100.0)
            auto_d = std::min(auto_d, 1);
        const int d = numfmt_clamp_decimals(decimals >= 0 ? decimals : auto_d);
        // 999,950 at one decimal is "1000.0K": move it to the next unit.
        if (u + 1 < kNumfmtUnitCount && numfmt_round_to(scaled, d) * units[u].div >= units[u + 1].div) {
            ++u;
            continue;
        }
        QString num = numfmt_digits(scaled, d, loc, g);
        if (trim)
            num = numfmt_trim_zeros(num, loc, 0);
        return QCoreApplication::translate("fincept::ui::formatting::NumberFormat", units[u].text.source,
                                           units[u].text.comment)
            .arg(num);
    }
}

bool numfmt_is_lakh_currency(const QString& ccy) {
    const QString major = major_currency(ccy);
    return major == QLatin1String("INR") || major == QLatin1String("PKR") || major == QLatin1String("BDT") ||
           major == QLatin1String("NPR") || major == QLatin1String("LKR");
}

bool numfmt_is_lakh_territory(const QLocale& loc) {
    switch (loc.territory()) {
        case QLocale::India:
        case QLocale::Pakistan:
        case QLocale::Bangladesh:
        case QLocale::Nepal:
        case QLocale::SriLanka:
            return true;
        default:
            return false;
    }
}

/// FromSettings and Auto honour the user's scheme when they chose one; with
/// the user on Automatic the currency (or, without one, the number locale)
/// decides. None / KMB / LakhCrore passed explicitly are kept.
Compact numfmt_resolve(Compact scheme, const QString& ccy, const FormatSettings::Snapshot& s) {
    if (scheme != Compact::FromSettings && scheme != Compact::Auto)
        return scheme;
    if (s.compact == Compact::KMB || s.compact == Compact::LakhCrore)
        return s.compact;
    if (!ccy.trimmed().isEmpty())
        return numfmt_is_lakh_currency(ccy) ? Compact::LakhCrore : Compact::KMB;
    return numfmt_is_lakh_territory(s.locale) ? Compact::LakhCrore : Compact::KMB;
}

/// How the number locale places a currency symbol, read from
/// QLocale::toCurrencyString() once per locale and cached per thread.
struct NumfmtCurrencyPattern {
    bool valid = false;
    bool symbol_first = true;
    QString pre, mid, post;
};

const NumfmtCurrencyPattern& numfmt_currency_pattern(const QLocale& loc) {
    thread_local bool cached_valid = false;
    thread_local QLocale cached_loc = QLocale::c();
    thread_local NumfmtCurrencyPattern cached;
    if (cached_valid && loc == cached_loc)
        return cached;

    NumfmtCurrencyPattern pat;
    const QString marker = QStringLiteral("\u00A4");
    // The pattern comes from the locale's CLDR data: the *system* locale
    // (QLocale() before any setDefault) asks the OS, which ignores the
    // requested precision and would leave ".00" in the pattern.
    const QLocale cldr(loc.language(), loc.script(), loc.territory());
    const QString p = numfmt_strip_bidi(cldr.toCurrencyString(1.0, marker, 0));
    const QString one = numfmt_strip_bidi(cldr.toString(1));
    const qsizetype is = p.indexOf(marker);
    const qsizetype in = p.indexOf(one);
    // The C locale's pattern ("1¤") is not a real convention: use the prefix default.
    if (loc.language() != QLocale::C && is >= 0 && in >= 0) {
        pat.valid = true;
        pat.symbol_first = is < in;
        if (pat.symbol_first) {
            pat.pre = p.left(is);
            pat.mid = p.mid(is + marker.size(), in - is - marker.size());
            pat.post = p.mid(in + one.size());
        } else {
            pat.pre = p.left(in);
            pat.mid = p.mid(in + one.size(), is - in - one.size());
            pat.post = p.mid(is + marker.size());
        }
        // Accounting-style "(¤1)" wrappers belong to negative patterns only.
        pat.pre.remove(QLatin1Char('('));
        pat.post.remove(QLatin1Char(')'));
        // Belt and braces: decimals a platform added after the "1" are not part of the pattern.
        QString& tail = pat.symbol_first ? pat.post : pat.mid;
        const QString dp = cldr.decimalPoint();
        if (!dp.isEmpty() && tail.startsWith(dp)) {
            qsizetype k = dp.size();
            while (k < tail.size() && tail.at(k).isDigit())
                ++k;
            tail.remove(0, k);
        }
    }
    cached_valid = true;
    cached_loc = loc;
    cached = pat;
    return cached;
}

/// A unit whose edge next to the digits is a letter needs a gap ("CHF 5",
/// "R 735.50", "kr 10"); a single non-Latin glyph used as a sign ("Ӿ") does not.
bool numfmt_needs_letter_gap(const QString& unit, bool edge_is_back) {
    const QChar edge = edge_is_back ? unit.back() : unit.front();
    return edge.isLetter() && (unit.size() > 1 || edge.unicode() < 0x80);
}

/// Places `unit` around an unsigned number body per the user's position and
/// spacing settings.
QString numfmt_place(const QString& body, const QString& unit, bool suffix_unit, const FormatSettings::Snapshot& s) {
    if (unit.isEmpty())
        return body;
    if (suffix_unit) // pence / cents / agorot: "123.45p", "1,234 ag."
        return body + (unit.size() > 1 ? QStringLiteral(" ") : QString()) + unit;

    const QString nbsp = QStringLiteral("\u00A0");
    switch (s.symbol_position) {
        case SymbolPosition::Before: {
            QString mid;
            if (s.symbol_spacing == SymbolSpacing::Space)
                mid = nbsp;
            else if (s.symbol_spacing == SymbolSpacing::Auto && numfmt_needs_letter_gap(unit, true))
                mid = QStringLiteral(" "); // "CHF 1,234.50", not "CHF1,234.50"
            return unit + mid + body;
        }
        case SymbolPosition::After:
            return body + (s.symbol_spacing == SymbolSpacing::None ? QString() : nbsp) + unit;
        case SymbolPosition::Locale:
            break;
    }
    const NumfmtCurrencyPattern pat = numfmt_currency_pattern(s.locale);
    const bool symbol_first = pat.valid ? pat.symbol_first : true;
    QString mid = pat.valid ? pat.mid : QString();
    if (s.symbol_spacing == SymbolSpacing::Space) {
        mid = nbsp;
    } else if (s.symbol_spacing == SymbolSpacing::None) {
        mid.clear();
    } else if (mid.isEmpty()) {
        // A letter symbol next to a digit needs a gap: "CHF 1,234.50", not "CHF1,234.50".
        if (numfmt_needs_letter_gap(unit, symbol_first))
            mid = QStringLiteral(" ");
    }
    return symbol_first ? pat.pre + unit + mid + body + pat.post : pat.pre + body + mid + unit + pat.post;
}

QString numfmt_with_sign(const QString& placed, bool negative, bool show_plus, const FormatSettings::Snapshot& s,
                         bool allow_parens) {
    if (negative) {
        if (allow_parens && s.negative_style == NegativeStyle::Parentheses)
            return QLatin1Char('(') + placed + QLatin1Char(')');
        return numfmt_minus(s.locale) + placed;
    }
    return show_plus ? numfmt_plus(s.locale) + placed : placed;
}

/// Decimals for a full (not abbreviated) amount, and how far trailing zeros
/// may be trimmed (< 0: not at all).
struct NumfmtFullDecimals {
    int d;
    int trim_min;
};

NumfmtFullDecimals numfmt_full_decimals(double a, int base_dec, bool crypto, int explicit_dec,
                                        const FormatSettings::Snapshot& s, bool precise) {
    if (explicit_dec >= 0)
        return {numfmt_clamp_decimals(explicit_dec), -1};
    const bool crypto_trim = crypto && base_dec > 3;
    if (!precise && s.decimals_policy == DecimalsPolicy::Fixed)
        return {numfmt_clamp_decimals(s.fixed_decimals), -1};
    if ((precise || s.decimals_policy == DecimalsPolicy::Adaptive) && a > 0.0 &&
        numfmt_rounds_to_zero(numfmt_nudged(a, base_dec), base_dec)) {
        // Never print a non-zero amount as zero: four significant digits.
        const int first = int(std::floor(-std::log10(a))) + 1;
        return {numfmt_clamp_decimals(first + 3), crypto_trim ? 2 : base_dec};
    }
    if (crypto_trim)
        return {base_dec, 2};
    return {numfmt_clamp_decimals(base_dec), -1};
}

enum class NumfmtUnitMode {
    Normal, ///< currency_unit() rules (style, override, display currency / mix)
    Code,   ///< the canonical code, whatever the style
    Strict, ///< the unambiguous symbol in Symbol style
};

struct NumfmtMoneyReq {
    Compact scheme = Compact::None;
    int decimals = -1;
    bool show_plus = false;
    NumfmtUnitMode unit_mode = NumfmtUnitMode::Normal;
    const CurrencyMix* mix = nullptr;
    bool precise = false;
};

QString numfmt_money(double v, const QString& ccy, const NumfmtMoneyReq& r, const FormatSettings::Snapshot& s) {
    if (!std::isfinite(v))
        return placeholder();
    const QString code = ccy.trimmed();
    double a = std::fabs(v);

    CurrencySpec spec;
    const bool known = !code.isEmpty() && currency_table_detail::effective_currency_unnamed(code, s, &spec);
    if (!known) {
        spec = CurrencySpec{};
        spec.code = spec.major = code.toUpper();
        spec.symbol = spec.plain = spec.strict = spec.code;
    }
    const Compact resolved = numfmt_resolve(r.scheme, code, s);
    int base_dec = spec.decimals;
    bool suffix_unit = known && spec.suffix;

    // Minor units: the user's "major unit" choice, and every abbreviated
    // amount ("£15.0K", never "1.50Mp"), are in the major currency.
    if (known && spec.minor_divisor > 1 && (s.minor_units == MinorUnits::Major || resolved != Compact::None)) {
        const int extra = numfmt_log10_int(spec.minor_divisor);
        const int minor_dec = spec.decimals;
        a /= double(spec.minor_divisor);
        CurrencySpec major;
        if (currency_table_detail::effective_currency_unnamed(spec.major, s, &major))
            spec = major;
        else
            spec.code = spec.major;
        base_dec = resolved == Compact::None ? std::max(spec.decimals, minor_dec + extra) : spec.decimals;
        suffix_unit = false;
    }

    bool zero = false;
    QString body;
    const NumfmtFullDecimals fd = numfmt_full_decimals(a, base_dec, spec.crypto, r.decimals, s, r.precise);
    const double an = numfmt_nudged(a, fd.d);
    if (resolved == Compact::None || numfmt_round_to(an, fd.d) < 1000.0) {
        body = numfmt_digits(an, fd.d, s.locale, s.grouping);
        zero = numfmt_rounds_to_zero(an, fd.d);
        if (fd.trim_min >= 0)
            body = numfmt_trim_zeros(body, s.locale, fd.trim_min);
    } else {
        body = numfmt_compact_body(a, resolved == Compact::LakhCrore, r.decimals, fd.d, s.locale, s.grouping, false,
                                   &zero);
    }

    QString unit;
    if (!code.isEmpty()) {
        if (r.unit_mode == NumfmtUnitMode::Code || s.symbol_style == SymbolStyle::Code) {
            unit = spec.code;
            suffix_unit = false;
        } else if (!known) {
            unit = spec.code;
        } else if (r.unit_mode == NumfmtUnitMode::Strict && !suffix_unit) {
            unit = spec.strict;
        } else {
            unit = currency_unit(spec.code, s, r.mix);
        }
    }
    const QString placed = numfmt_place(body, unit, suffix_unit, s);
    return numfmt_with_sign(placed, v < 0 && !zero, r.show_plus && !zero, s, true);
}

/// Decimals implied by a tick size: 0.05 -> 2, 0.0001 -> 4, 0.25 -> 2, 1 -> 0.
int numfmt_decimals_of_tick(double tick) {
    for (int d = 0; d <= kNumfmtMaxDecimals; ++d) {
        const double x = tick * numfmt_pow10(d);
        if (std::fabs(x - std::round(x)) < 1e-9 * std::max(1.0, x))
            return d;
    }
    return kNumfmtMaxDecimals;
}

/// Fewest decimals (0..max_d) that show `a` exactly: 2845 -> 0, 2845.5 -> 1.
int numfmt_fraction_digits(double a, int max_d) {
    for (int d = 0; d < max_d; ++d) {
        const double x = a * numfmt_pow10(d);
        if (std::fabs(x - std::round(x)) < 1e-6)
            return d;
    }
    return max_d;
}

/// A plain fiat currency code (not a minor unit, not crypto) — for FX detection.
bool numfmt_is_fiat_code(const QString& code) {
    CurrencySpec spec;
    return code.size() == 3 && find_currency(code, &spec) && spec.minor_divisor == 1 && spec.decimals <= 4 &&
           !spec.crypto && spec.code == code;
}

/// Significant-digit precision for an FX rate in a note: pip precision, but at
/// least five significant digits for tiny rates (1 VND = 0.000038826 USD).
int numfmt_rate_decimals(double rate) {
    const double a = std::fabs(rate);
    int d = price_decimals(a, InstrumentFmt{QString(), -1, 0, true});
    if (a > 0 && a < 0.01)
        d = std::max(d, int(std::floor(-std::log10(a))) + 5);
    return numfmt_clamp_decimals(d);
}

} // namespace numfmt_detail

// ── Basics ──────────────────────────────────────────────────────────────────

QString placeholder() {
    return QStringLiteral("\u2014");
}

QString number(double v, int decimals) {
    if (!std::isfinite(v))
        return placeholder();
    const auto s = FormatSettings::instance().snapshot();
    return numfmt_detail::numfmt_signed_body(v, decimals, false, s.locale, s.grouping);
}

QString signed_number(double v, int decimals) {
    if (!std::isfinite(v))
        return placeholder();
    const auto s = FormatSettings::instance().snapshot();
    return numfmt_detail::numfmt_signed_body(v, decimals, true, s.locale, s.grouping);
}

// ── Prices ──────────────────────────────────────────────────────────────────

InstrumentFmt instrument_for_symbol(const QString& symbol, const QString& currency) {
    InstrumentFmt f;
    f.currency = currency.trimmed();
    QString upper = symbol.trimmed().toUpper();
    // A derivatives contract "BASE/QUOTE:SETTLE" ("BTC/USDT:USDT", "BTC/USD:BTC") is
    // priced in its quote asset: drop the settlement suffix. A venue prefix before
    // the pair ("NSE:RELIANCE") has no '/' ahead of its ':' and is untouched.
    if (const qsizetype slash = upper.indexOf(QLatin1Char('/')); slash > 0) {
        if (const qsizetype colon = upper.indexOf(QLatin1Char(':'), slash + 1); colon > slash)
            upper.truncate(colon);
    }
    QString s = upper;
    const bool yahoo_fx = s.endsWith(QLatin1String("=X"));
    if (yahoo_fx)
        s.chop(2);
    s.remove(QLatin1Char('/'));
    if (s.size() == 7 && s.at(3) == QLatin1Char('-'))
        s.remove(3, 1); // "EUR-USD"; crypto "BTC-USD" fails the fiat test below

    QString quote;
    if (yahoo_fx && s.size() == 3 && numfmt_detail::numfmt_is_fiat_code(s)) {
        quote = s; // Yahoo "JPY=X" is USDJPY
    } else if (s.size() == 6 && numfmt_detail::numfmt_is_fiat_code(s.left(3)) &&
               numfmt_detail::numfmt_is_fiat_code(s.mid(3))) {
        quote = s.mid(3);
    }
    if (!quote.isEmpty()) {
        f.fx = true;
        if (f.currency.isEmpty())
            f.currency = quote;
        return f;
    }

    // Crypto pairs written with a separator: "BTC-USD", "ETH/USDT", "ETH-BTC".
    // The quote side must be a currency and the base must not be a fiat code
    // (that would be FX), so share classes ("BRK-B", "RDS-A") are untouched.
    if (!yahoo_fx && f.currency.isEmpty()) {
        const qsizetype sep = std::max(upper.lastIndexOf(QLatin1Char('-')), upper.lastIndexOf(QLatin1Char('/')));
        if (sep > 0 && sep + 1 < upper.size()) {
            const QString base = upper.left(sep);
            CurrencySpec q;
            if (find_currency(upper.mid(sep + 1), &q) && q.minor_divisor == 1 && !q.metal &&
                !numfmt_detail::numfmt_is_fiat_code(base) && !base.contains(QLatin1Char('.')))
                f.currency = q.code;
        }
    }
    return f;
}

int price_decimals(double v, const InstrumentFmt& inst) {
    if (inst.decimals >= 0)
        return numfmt_detail::numfmt_clamp_decimals(inst.decimals);
    if (inst.tick > 0 && std::isfinite(inst.tick))
        return numfmt_detail::numfmt_decimals_of_tick(inst.tick);

    const double a = std::isfinite(v) ? std::fabs(v) : 0.0;
    if (inst.fx) {
        if (a == 0.0)
            return 4;
        if (a >= 1000)
            return 2;
        if (a >= 100)
            return 3; // USDJPY 151.235
        if (a >= 1)
            return 4; // EURUSD 1.0834, USDINR 83.1250
        if (a >= 0.01)
            return 5; // EURGBP 0.85672
        return 6;
    }

    const int cd = inst.currency.isEmpty() ? 2 : currency_decimals(inst.currency);
    if (cd > 3) { // crypto quote asset (BTC, ETH): more precision at every size
        if (a >= 1000)
            return 2;
        if (a >= 1)
            return 4;
        if (a >= 0.01 || a == 0.0)
            return 6;
        return 8;
    }
    if (a == 0.0)
        return cd == 0 ? 0 : std::max(2, cd);
    if (a >= 1) {
        if (cd == 0) // JPY / KRW: whole units, but keep a 0.1 / 0.5 tick if present
            return numfmt_detail::numfmt_fraction_digits(a, 2);
        return std::max(2, cd);
    }
    if (a >= 0.01)
        return 4;
    if (a >= 0.0001)
        return 6;
    return 8;
}

QString price(double v, const InstrumentFmt& inst) {
    if (!std::isfinite(v))
        return placeholder();
    const auto s = FormatSettings::instance().snapshot();
    return numfmt_detail::numfmt_signed_body(v, price_decimals(v, inst), false, s.locale, s.grouping);
}

QString price_with_symbol(double v, const InstrumentFmt& inst) {
    if (!std::isfinite(v))
        return placeholder();
    const auto s = FormatSettings::instance().snapshot();
    int d = price_decimals(v, inst);
    double a = std::fabs(v);
    const QString code = inst.currency.trimmed();
    QString unit;
    bool suffix = false;
    if (!inst.fx && !code.isEmpty()) { // an FX rate is not money: no unit
        CurrencySpec spec;
        const bool known = currency_table_detail::effective_currency_unnamed(code, s, &spec);
        QString unit_code = known ? spec.code : code.toUpper();
        suffix = known && spec.suffix;
        if (known && spec.minor_divisor > 1 && s.minor_units == MinorUnits::Major) {
            a /= double(spec.minor_divisor);
            d = numfmt_detail::numfmt_clamp_decimals(d + numfmt_detail::numfmt_log10_int(spec.minor_divisor));
            unit_code = spec.major;
            suffix = false;
        }
        if (s.symbol_style == SymbolStyle::Code) {
            unit = unit_code;
            suffix = false;
        } else {
            unit = known ? currency_unit(unit_code, s, nullptr) : unit_code;
        }
    }
    const QString body = numfmt_detail::numfmt_digits(a, d, s.locale, s.grouping);
    const QString placed = numfmt_detail::numfmt_place(body, unit, suffix, s);
    return numfmt_detail::numfmt_with_sign(placed, v < 0 && !numfmt_detail::numfmt_rounds_to_zero(a, d), false, s,
                                           false);
}

QString price_change(double change, double ref_price, const InstrumentFmt& inst) {
    if (!std::isfinite(change))
        return placeholder();
    const auto s = FormatSettings::instance().snapshot();
    return numfmt_detail::numfmt_signed_body(change, price_decimals(ref_price, inst), true, s.locale, s.grouping);
}

// ── Money ───────────────────────────────────────────────────────────────────

QString money(double v, const QString& ccy, Compact scheme, int decimals) {
    numfmt_detail::NumfmtMoneyReq r;
    r.scheme = scheme;
    r.decimals = decimals;
    return numfmt_detail::numfmt_money(v, ccy, r, FormatSettings::instance().snapshot());
}

QString signed_money(double v, const QString& ccy, Compact scheme, int decimals) {
    numfmt_detail::NumfmtMoneyReq r;
    r.scheme = scheme;
    r.decimals = decimals;
    r.show_plus = true;
    return numfmt_detail::numfmt_money(v, ccy, r, FormatSettings::instance().snapshot());
}

QString money(const Money& m, Compact scheme, int decimals) {
    return money(m.amount, m.currency, scheme, decimals);
}

QString signed_money(const Money& m, Compact scheme, int decimals) {
    return signed_money(m.amount, m.currency, scheme, decimals);
}

QString money_precise(double v, const QString& ccy) {
    numfmt_detail::NumfmtMoneyReq r;
    r.precise = true;
    return numfmt_detail::numfmt_money(v, ccy, r, FormatSettings::instance().snapshot());
}

QString money_in_column(double v, const QString& ccy, bool column_mixed, Compact scheme, int decimals, bool signed_) {
    const auto s = FormatSettings::instance().snapshot();
    numfmt_detail::NumfmtMoneyReq r;
    r.scheme = scheme;
    r.decimals = decimals;
    r.show_plus = signed_;
    if (column_mixed)
        r.unit_mode = s.code_when_mixed ? numfmt_detail::NumfmtUnitMode::Code : numfmt_detail::NumfmtUnitMode::Strict;
    return numfmt_detail::numfmt_money(v, ccy, r, s);
}

QString money_in_column(double v, const QString& ccy, const CurrencyMix& column, Compact scheme, int decimals,
                        bool signed_) {
    const auto s = FormatSettings::instance().snapshot();
    CurrencyMix mix = column;
    mix.add(ccy);
    numfmt_detail::NumfmtMoneyReq r;
    r.scheme = scheme;
    r.decimals = decimals;
    r.show_plus = signed_;
    r.mix = &mix;
    if (mix.mixed() && s.code_when_mixed)
        r.unit_mode = numfmt_detail::NumfmtUnitMode::Code;
    return numfmt_detail::numfmt_money(v, ccy, r, s);
}

QString converted_money(const ConvertedMoney& c, Compact scheme, bool signed_) {
    if (!c.ok())
        return placeholder();
    const QString text = signed_ ? signed_money(c.amount, c.currency, scheme) : money(c.amount, c.currency, scheme);
    if (c.stale || c.approximate)
        return QStringLiteral("\u2248\u00A0") + text;
    return text;
}

QString fx_note(const ConvertedMoney& c) {
    if (!c.converted || !std::isfinite(c.rate))
        return {};
    const QString src = major_currency(c.source_currency);
    const QString rate = numfmt_detail::numfmt_digits(c.rate, numfmt_detail::numfmt_rate_decimals(c.rate),
                                                      FormatSettings::instance().locale(), Grouping::Always);
    const QString dot = QStringLiteral(" \u00B7 ");
    QString note = numfmt_detail::numfmt_tr("1 %1 = %2 %3", "FX rate note: 1 GBP = 1.3183 USD").arg(src, rate, c.currency);
    if (c.as_of.isValid())
        note += dot + date_time(c.as_of);
    if (c.triangulated)
        note += dot + numfmt_detail::numfmt_tr("via USD", "FX rate note: cross rate computed through USD");
    if (c.stale)
        note += dot + numfmt_detail::numfmt_tr("stale", "FX rate note: older than the user's threshold");
    if (c.approximate)
        note += dot + (c.approx_reason.isEmpty()
                           ? numfmt_detail::numfmt_tr("approximate", "FX rate note: rate assumed, not observed")
                           : c.approx_reason);
    return note;
}

QString vendor_cost(double usd, int decimals) {
    return money(usd, QStringLiteral("USD"), Compact::None, decimals);
}

QString plan_price(double usd) {
    if (!std::isfinite(usd))
        return placeholder();
    const bool whole = std::fabs(usd - std::round(usd)) < 1e-9;
    return money(usd, QStringLiteral("USD"), Compact::None, whole ? 0 : 2);
}

// ── Units and headers ───────────────────────────────────────────────────────

QString display_currency() {
    return FormatSettings::instance().display_currency();
}

QString scaled_unit(const QString& ccy, Scale scale) {
    const auto s = FormatSettings::instance().snapshot();
    const QString code = ccy.trimmed().isEmpty() ? s.display_currency : ccy.trimmed();
    // Scaled amounts are always in the major unit.
    const QString unit = currency_unit(scale == Scale::One ? code : major_currency(code), s, nullptr);
    QString abbr;
    switch (scale) {
        case Scale::One:
            return unit;
        case Scale::Thousand:
            abbr = numfmt_detail::numfmt_tr("K", "unit of a column scaled to thousands");
            break;
        case Scale::Million:
            abbr = numfmt_detail::numfmt_tr("M", "unit of a column scaled to millions");
            break;
        case Scale::Billion:
            abbr = numfmt_detail::numfmt_tr("B", "unit of a column scaled to billions");
            break;
        case Scale::Trillion:
            abbr = numfmt_detail::numfmt_tr("T", "unit of a column scaled to trillions");
            break;
        case Scale::Lakh:
            abbr = numfmt_detail::numfmt_tr("L", "unit of a column scaled to lakhs (100,000)");
            break;
        case Scale::Crore:
            abbr = numfmt_detail::numfmt_tr("Cr", "unit of a column scaled to crores (10,000,000)");
            break;
    }
    if (unit.isEmpty())
        return abbr;
    const bool gap = unit.back().isLetterOrNumber() || abbr.size() > 1;
    return unit + (gap ? QStringLiteral(" ") : QString()) + abbr;
}

QString header(const QString& title, const QString& ccy) {
    if (ccy == QLatin1String("-"))
        return title;
    return numfmt_detail::numfmt_tr("%1 (%2)", "column header with its currency unit: Value ($)")
        .arg(title, currency_unit(ccy.trimmed().isEmpty() ? display_currency() : ccy));
}

QString header(const QString& title, const CurrencyMix& column) {
    if (column.isEmpty() || column.mixed())
        return title;
    const auto s = FormatSettings::instance().snapshot();
    const QString unit = s.symbol_style == SymbolStyle::Code ? column.single()
                                                              : currency_unit(column.single(), s, &column);
    return numfmt_detail::numfmt_tr("%1 (%2)", "column header with its currency unit: Value ($)").arg(title, unit);
}

QString header_scaled(const QString& title, const QString& ccy, Scale scale) {
    return numfmt_detail::numfmt_tr("%1 (%2)", "column header with its currency unit: Value ($)")
        .arg(title, scaled_unit(ccy, scale));
}

// ── Not money ───────────────────────────────────────────────────────────────

bool is_money_quote(const QString& quote_type, const QString& symbol) {
    const QString qt = quote_type.trimmed().toUpper();
    if (qt == QLatin1String("INDEX") || qt == QLatin1String("CURRENCY"))
        return false;
    if (qt.isEmpty()) {
        const QString sym = symbol.trimmed();
        if (sym.startsWith(QLatin1Char('^')) || sym.endsWith(QLatin1String("=X"), Qt::CaseInsensitive))
            return false;
    }
    return true;
}

// ── Chart axes ──────────────────────────────────────────────────────────────

QString axis_money(double v, const QString& ccy) {
    if (!std::isfinite(v))
        return placeholder();
    const auto s = FormatSettings::instance().snapshot();
    QString code = ccy.trimmed();
    double a = std::fabs(v);
    CurrencySpec spec;
    if (!code.isEmpty() && currency_table_detail::effective_currency_unnamed(code, s, &spec) && spec.minor_divisor > 1) {
        a /= double(spec.minor_divisor); // amounts on an axis are in the major unit
        code = spec.major;
    }
    const Compact resolved = numfmt_detail::numfmt_resolve(Compact::FromSettings, code, s);
    bool zero = false;
    const QString body = numfmt_detail::numfmt_compact_body(a, resolved == Compact::LakhCrore, -1, -1, s.locale,
                                                            s.grouping, true, &zero);
    const QString unit = code.isEmpty() ? QString() : currency_unit(code, s, nullptr);
    const QString placed = numfmt_detail::numfmt_place(body, unit, false, s);
    return numfmt_detail::numfmt_with_sign(placed, v < 0 && !zero, false, s, false);
}

QString axis_printf_format(const QString& ccy, int decimals) {
    const auto s = FormatSettings::instance().snapshot();
    const QString body = QStringLiteral("%.%1f").arg(numfmt_detail::numfmt_clamp_decimals(decimals));
    const QString code = ccy.trimmed();
    if (code.isEmpty())
        return body;
    CurrencySpec spec;
    const bool known = currency_table_detail::effective_currency_unnamed(code, s, &spec);
    bool suffix = known && spec.suffix;
    QString unit = currency_unit(code, s, nullptr);
    if (s.symbol_style == SymbolStyle::Code)
        suffix = false;
    unit.replace(QLatin1Char('%'), QLatin1String("%%"));
    return numfmt_detail::numfmt_place(body, unit, suffix, s);
}

// ── Exports ─────────────────────────────────────────────────────────────────

QString export_number(double v, int max_decimals) {
    if (!std::isfinite(v))
        return {};
    const QLocale c = QLocale::c(); // '.', no grouping, ASCII '-'
    QString out;
    if (max_decimals < 0) {
        // 15 significant digits (what a double reliably holds), then the
        // shortest fixed-point text that reads back as that value.
        const double v15 = QString::number(v, 'g', 15).toDouble();
        out = c.toString(v15, 'f', QLocale::FloatingPointShortest);
    } else {
        out = numfmt_detail::numfmt_trim_zeros(c.toString(v, 'f', numfmt_detail::numfmt_clamp_decimals(max_decimals)),
                                               c, 0);
    }
    if (out == QLatin1String("-0"))
        out = QStringLiteral("0");
    return out;
}

ExportMoney export_money(double v, const QString& ccy) {
    return ExportMoney{export_number(v), ccy.trimmed()};
}

QStringList export_converted(const ConvertedMoney& c) {
    return {export_number(c.amount),
            c.currency,
            export_number(c.source_amount),
            c.source_currency,
            export_number(c.rate),
            c.as_of.isValid() ? c.as_of.toUTC().toString(Qt::ISODate) : QString()};
}

// ── Compact ─────────────────────────────────────────────────────────────────

Compact resolve_compact(Compact scheme, const QString& ccy) {
    return numfmt_detail::numfmt_resolve(scheme, ccy, FormatSettings::instance().snapshot());
}

QString compact(double v, Compact scheme, const QString& ccy, int decimals) {
    if (!std::isfinite(v))
        return placeholder();
    const FormatSettings::Snapshot s = FormatSettings::instance().snapshot();
    const Compact resolved = numfmt_detail::numfmt_resolve(scheme, ccy, s);
    if (resolved == Compact::None) {
        const double a = std::fabs(v);
        const int d = decimals >= 0 ? decimals : (a == std::floor(a) ? 0 : 2);
        return numfmt_detail::numfmt_signed_body(v, d, false, s.locale, s.grouping);
    }
    bool zero = false;
    const QString body = numfmt_detail::numfmt_compact_body(v, resolved == Compact::LakhCrore, decimals, -1, s.locale,
                                                            s.grouping, false, &zero);
    return v < 0 && !zero ? numfmt_detail::numfmt_minus(s.locale) + body : body;
}

// ── Percentages ─────────────────────────────────────────────────────────────

QString pct(double percent, int decimals, bool signed_) {
    if (!std::isfinite(percent))
        return placeholder();
    const auto s = FormatSettings::instance().snapshot();
    return numfmt_detail::numfmt_signed_body(percent, decimals, signed_, s.locale, s.grouping) +
           numfmt_detail::numfmt_strip_bidi(s.locale.percent());
}

QString fraction_pct(double fraction, int decimals, bool signed_) {
    return pct(fraction * 100.0, decimals, signed_);
}

QString pp(double points, int decimals, bool signed_) {
    if (!std::isfinite(points))
        return placeholder();
    const auto s = FormatSettings::instance().snapshot();
    return numfmt_detail::numfmt_tr("%1 pp", "percentage points")
        .arg(numfmt_detail::numfmt_signed_body(points, decimals, signed_, s.locale, s.grouping));
}

QString bp(double basis_points, int decimals, bool signed_) {
    if (!std::isfinite(basis_points))
        return placeholder();
    const auto s = FormatSettings::instance().snapshot();
    return numfmt_detail::numfmt_tr("%1 bp", "basis points")
        .arg(numfmt_detail::numfmt_signed_body(basis_points, decimals, signed_, s.locale, s.grouping));
}

// ── Quantities ──────────────────────────────────────────────────────────────

QString quantity(double q, int max_decimals) {
    if (!std::isfinite(q))
        return placeholder();
    const auto s = FormatSettings::instance().snapshot();
    if (std::fabs(q - std::round(q)) < 1e-9)
        return numfmt_detail::numfmt_signed_body(std::round(q), 0, false, s.locale, s.grouping);
    const QString body = numfmt_detail::numfmt_signed_body(q, max_decimals, false, s.locale, s.grouping);
    return numfmt_detail::numfmt_trim_zeros(body, s.locale, 0);
}

QString volume(double v, Compact scheme) {
    if (!std::isfinite(v) || v < 0)
        return placeholder();
    const FormatSettings::Snapshot s = FormatSettings::instance().snapshot();
    const Compact resolved = numfmt_detail::numfmt_resolve(scheme, QString(), s);
    if (resolved == Compact::None || v < 1000)
        return quantity(v, 2);
    bool zero = false;
    return numfmt_detail::numfmt_compact_body(v, resolved == Compact::LakhCrore, -1, 0, s.locale, s.grouping, false,
                                              &zero);
}

QString ratio(double v, int decimals) {
    if (!std::isfinite(v))
        return placeholder();
    const auto s = FormatSettings::instance().snapshot();
    return numfmt_detail::numfmt_tr("%1x", "a multiple, e.g. P/E 12.3x")
        .arg(numfmt_detail::numfmt_signed_body(v, decimals, false, s.locale, s.grouping));
}

QString format_compact_volume(qint64 volume) {
    if (volume <= 0)
        return QStringLiteral("--");
    const auto s = FormatSettings::instance().snapshot();
    bool zero = false;
    return numfmt_detail::numfmt_compact_body(double(volume), false, 2, 0, s.locale, s.grouping, false, &zero);
}

// ── Parsing (ParseAmount.h) ─────────────────────────────────────────────────

namespace numfmt_detail {

/// One character as the parser reads it: bidi marks and BOM dropped (null),
/// every space-like character a plain space, the Unicode minus forms '-', a
/// full-width plus '+', typographic apostrophes (Swiss grouping) '\''.
QChar numfmt_parse_char(QChar c) {
    switch (c.unicode()) {
        case 0x200E: case 0x200F: case 0x061C: case 0xFEFF: case 0x202A: case 0x202B: case 0x202C:
        case 0x202D: case 0x202E: case 0x2066: case 0x2067: case 0x2068: case 0x2069:
            return QChar();
        case 0x00A0: case 0x202F: case 0x2002: case 0x2003: case 0x2004: case 0x2005: case 0x2006:
        case 0x2007: case 0x2008: case 0x2009: case 0x200A: case 0x205F: case 0x3000: case '\t':
        case '\n': case '\r':
            return QLatin1Char(' ');
        case 0x2212: case 0xFE63: case 0xFF0D:
            return QLatin1Char('-');
        case 0xFF0B:
            return QLatin1Char('+');
        case 0x2019: case 0x2018: case 0x02BC:
            return QLatin1Char('\'');
        default:
            return c;
    }
}

QString numfmt_parse_norm(const QString& in) {
    QString out;
    out.reserve(in.size());
    for (const QChar c : in) {
        const QChar m = numfmt_parse_char(c);
        if (!m.isNull())
            out.append(m);
    }
    return out.simplified();
}

QChar numfmt_parse_locale_char(const QString& sep, QChar fallback) {
    for (const QChar c : sep) {
        const QChar m = numfmt_parse_char(c);
        if (!m.isNull())
            return m;
    }
    return fallback;
}

bool numfmt_parse_all_letters(QStringView s) {
    if (s.isEmpty())
        return false;
    for (const QChar c : s)
        if (!c.isLetter())
            return false;
    return true;
}

/// Currency unit tokens -> the codes they can stand for.
struct NumfmtParseUnits {
    QHash<QString, QStringList> exact;  ///< as printed
    QHash<QString, QStringList> folded; ///< upper-cased letter symbols ("KR" -> SEK, NOK, DKK)
    int max_len = 0;

    void add(const QString& token, const QString& code, bool fold) {
        const QString t = numfmt_parse_norm(token);
        if (t.isEmpty() || code.isEmpty())
            return;
        QStringList& e = exact[t];
        if (!e.contains(code))
            e.append(code);
        if (fold && numfmt_parse_all_letters(t)) {
            QStringList& f = folded[t.toUpper()];
            if (!f.contains(code))
                f.append(code);
        }
        max_len = std::max(max_len, int(t.size()));
    }
};

/// Every symbol / plain / strict form and code of the table (no user overrides).
const NumfmtParseUnits& numfmt_parse_table_units() {
    static const NumfmtParseUnits units = [] {
        NumfmtParseUnits u;
        for (const QString& code : known_currency_codes()) {
            CurrencySpec sp;
            if (!find_currency(code, &sp))
                continue;
            u.add(sp.code, sp.code, false); // codes fold through find_currency() ("gbp" -> GBP, not GBp)
            u.add(sp.symbol, sp.code, true);
            u.add(sp.plain, sp.code, true);
            u.add(sp.strict, sp.code, true);
        }
        return u;
    }();
    return units;
}

struct NumfmtCompactToken {
    QString text;
    double mult;
    bool fold; ///< matched case-insensitively
};

/// Size suffixes: the formatter's own (in the current translation and in
/// English) and the forms people type.
QVector<NumfmtCompactToken> numfmt_parse_compact_tokens() {
    QVector<NumfmtCompactToken> v;
    auto add_template = [&v](const NumfmtUnit& u) {
        QString t = QCoreApplication::translate("fincept::ui::formatting::NumberFormat", u.text.source, u.text.comment);
        t.replace(QStringLiteral("%1"), QString());
        t = numfmt_parse_norm(t);
        if (!t.isEmpty())
            v.append({t, u.div, false});
        QString e = QString::fromLatin1(u.text.source);
        e.replace(QStringLiteral("%1"), QString());
        e = e.trimmed();
        if (!e.isEmpty() && e != t)
            v.append({e, u.div, false});
    };
    for (const NumfmtUnit& u : kNumfmtKmbUnits)
        add_template(u);
    for (const NumfmtUnit& u : kNumfmtLakhUnits)
        add_template(u);
    static const struct {
        const char* text;
        double mult;
    } kWords[] = {
        {"k", 1e3},        {"thousand", 1e3}, {"m", 1e6},     {"mn", 1e6},        {"mm", 1e6},
        {"million", 1e6},  {"b", 1e9},        {"bn", 1e9},    {"billion", 1e9},   {"t", 1e12},
        {"tn", 1e12},      {"trillion", 1e12}, {"lakh", 1e5},  {"lakhs", 1e5},     {"lac", 1e5},
        {"lacs", 1e5},     {"cr", 1e7},       {"crore", 1e7}, {"crores", 1e7},    {"lakh crore", 1e12},
        {"lakh crores", 1e12},
    };
    for (const auto& w : kWords)
        v.append({QString::fromLatin1(w.text), w.mult, true});
    return v;
}

/// A match of `len` characters at `pos` may not run on into a word: "K" is
/// not the start of "Kr".
bool numfmt_parse_boundary_ok(const QString& a, int pos, int len) {
    const int end = pos + len;
    if (end >= a.size())
        return true;
    return !(a.at(end - 1).isLetter() && a.at(end).isLetter());
}

struct NumfmtParseCtx {
    const NumfmtParseUnits* table = nullptr;
    NumfmtParseUnits extra; ///< user overrides, custom codes, the default currency
    QVector<NumfmtCompactToken> compact;
    QString dflt; ///< the caller's currency (canonical), may be empty
    bool unit_in_prefix = false; ///< a currency unit was already read before the number
};

/// Longest currency unit at `pos` (exact before case-folded at each length).
int numfmt_parse_unit_at(const QString& a, int pos, const NumfmtParseCtx& ctx, QString* token, QStringList* codes) {
    const int max_len = std::min(int(a.size()) - pos, std::max(ctx.table->max_len, ctx.extra.max_len));
    for (int len = max_len; len >= 1; --len) {
        if (!numfmt_parse_boundary_ok(a, pos, len))
            continue;
        const QString sub = a.mid(pos, len);
        QStringList found = ctx.extra.exact.value(sub);
        for (const QString& c : ctx.table->exact.value(sub))
            if (!found.contains(c))
                found.append(c);
        if (found.isEmpty() && numfmt_parse_all_letters(sub)) {
            CurrencySpec sp;
            if (sub.size() >= 3 && find_currency(sub, &sp)) // "usd", "Gbp" -> the table's own spelling
                found.append(sp.code);
            const QString up = sub.toUpper();
            for (const QString& c : ctx.extra.folded.value(up) + ctx.table->folded.value(up))
                if (!found.contains(c))
                    found.append(c);
        }
        if (!found.isEmpty()) {
            *token = sub;
            *codes = found;
            return len;
        }
    }
    return 0;
}

/// Longest size suffix at `pos`.
int numfmt_parse_compact_at(const QString& a, int pos, const NumfmtParseCtx& ctx, double* mult) {
    int best = 0;
    for (const NumfmtCompactToken& t : ctx.compact) {
        const int len = int(t.text.size());
        if (len <= best || pos + len > a.size())
            continue;
        if (QStringView(a).mid(pos, len).compare(t.text, t.fold ? Qt::CaseInsensitive : Qt::CaseSensitive) != 0)
            continue;
        if (!numfmt_parse_boundary_ok(a, pos, len))
            continue;
        best = len;
        *mult = t.mult;
    }
    return best;
}

/// What surrounds the number: signs, at most one currency unit, at most one
/// size suffix (after the number only).
struct NumfmtAffix {
    int minus = 0;
    int plus = 0;
    QString unit_token;
    QStringList unit_codes;
    QString size_token;
    double mult = 1.0;
};

bool numfmt_parse_affix(const QString& a, bool after_number, const NumfmtParseCtx& ctx, NumfmtAffix* out,
                        QString* why) {
    int i = 0;
    while (i < a.size()) {
        const QChar c = a.at(i);
        if (c == QLatin1Char(' ') || c == QChar(0x2248) || c == QLatin1Char('~')) {
            ++i;
            continue;
        }
        if (c == QLatin1Char('-')) {
            ++out->minus;
            ++i;
            continue;
        }
        if (c == QLatin1Char('+')) {
            ++out->plus;
            ++i;
            continue;
        }
        double mult = 1.0;
        const int clen = after_number ? numfmt_parse_compact_at(a, i, ctx, &mult) : 0;
        QString token;
        QStringList codes;
        const int ulen = numfmt_parse_unit_at(a, i, ctx, &token, &codes);
        bool take_size = clen > 0 && clen >= ulen; // "1.5K": thousand, not the kyat
        if (take_size && clen == ulen && out->unit_token.isEmpty() && !ctx.unit_in_prefix) {
            // The same letters are a size and a currency ("K", the kyat's symbol). It is
            // the currency when the size was already read ("1,5K K"), or when it is the
            // caller's currency and nothing follows ("1.234,50 K" in a kyat field).
            bool nothing_follows = true;
            for (int j = i + ulen; j < a.size() && nothing_follows; ++j)
                nothing_follows = a.at(j) == QLatin1Char(' ') || a.at(j) == QLatin1Char('-') ||
                                  a.at(j) == QLatin1Char('+');
            if (!out->size_token.isEmpty() || (!ctx.dflt.isEmpty() && codes.contains(ctx.dflt) && nothing_follows))
                take_size = false;
        }
        if (take_size) {
            if (!out->size_token.isEmpty()) {
                *why = numfmt_tr("Two size suffixes (%1 and %2) in one amount.", "parse_amount error")
                           .arg(out->size_token, a.mid(i, clen));
                return false;
            }
            out->size_token = a.mid(i, clen);
            out->mult = mult;
            i += clen;
            continue;
        }
        if (ulen > 0) {
            if (!out->unit_token.isEmpty()) {
                *why = numfmt_tr("Two currencies (%1 and %2) in one amount.", "parse_amount error")
                           .arg(out->unit_token, token);
                return false;
            }
            out->unit_token = token;
            out->unit_codes = codes;
            i += ulen;
            continue;
        }
        int j = i + 1;
        while (j < a.size() && a.at(j) != QLatin1Char(' ') && a.at(j).isLetter() == c.isLetter())
            ++j;
        *why = numfmt_tr("“%1” is neither a currency nor a size such as K, M, L or Cr.", "parse_amount error")
                   .arg(a.mid(i, j - i));
        return false;
    }
    return true;
}

/// The digits between the first and the last digit: decimal point and grouping
/// (see parse_amount()). Writes the plain value.
bool numfmt_parse_core(const QString& core, const QLocale& loc, double* out, QString* why) {
    const QChar ldec = numfmt_parse_locale_char(loc.decimalPoint(), QLatin1Char('.'));
    const QChar lgrp = numfmt_parse_locale_char(loc.groupSeparator(), QLatin1Char(','));
    const QChar ar_dec(0x066B);
    QStringList runs;
    QVector<QChar> seps;
    QString cur;
    for (const QChar c : core) {
        const int dv = c.digitValue();
        if (dv >= 0 && dv <= 9 && !c.isSurrogate()) {
            cur.append(QChar(char16_t(u'0' + dv)));
            continue;
        }
        QChar k;
        if (c == QLatin1Char('.') || c == QLatin1Char(',') || c == QLatin1Char(' ') || c == QLatin1Char('\'') ||
            c == ar_dec)
            k = c;
        else if (c.unicode() == 0x066C) // Arabic thousands separator
            k = QLatin1Char('\'');
        else if (c == ldec || c == lgrp)
            k = c;
        else {
            *why = numfmt_tr("“%1” cannot be part of a number.", "parse_amount error").arg(c);
            return false;
        }
        const bool decimal_like = k == QLatin1Char('.') || k == QLatin1Char(',') || k == ar_dec || k == ldec;
        if (cur.isEmpty() && !(runs.isEmpty() && seps.isEmpty() && decimal_like)) {
            *why = numfmt_tr("Two separators in a row in “%1”.", "parse_amount error").arg(core);
            return false;
        }
        runs.append(cur);
        cur.clear();
        seps.append(k);
    }
    if (cur.isEmpty()) {
        *why = numfmt_tr("“%1” ends with a separator.", "parse_amount error").arg(core);
        return false;
    }
    runs.append(cur);

    auto count = [&seps](QChar k) { return int(std::count(seps.cbegin(), seps.cend(), k)); };
    const int n_dot = count(QLatin1Char('.'));
    const int n_comma = count(QLatin1Char(','));
    const int n_ar = count(ar_dec);
    auto last_of = [&seps](QChar k) {
        for (int i = int(seps.size()) - 1; i >= 0; --i)
            if (seps.at(i) == k)
                return i;
        return -1;
    };
    int dec_idx = -1;
    if (n_ar > 1) {
        *why = numfmt_tr("“%1” has more than one decimal point.", "parse_amount error").arg(core);
        return false;
    }
    if (n_ar == 1) {
        dec_idx = last_of(ar_dec);
    } else if (n_dot > 0 && n_comma > 0) {
        // Both: the last one is the decimal point and appears once ("1,234.56", "1.234,56").
        const int i = std::max(last_of(QLatin1Char('.')), last_of(QLatin1Char(',')));
        if (count(seps.at(i)) != 1) {
            *why = numfmt_tr("Check the separators in “%1”.", "parse_amount error").arg(core);
            return false;
        }
        dec_idx = i;
    } else if (n_dot + n_comma == 1) {
        const int i = std::max(last_of(QLatin1Char('.')), last_of(QLatin1Char(',')));
        const QChar k = seps.at(i);
        const qsizetype after = runs.at(i + 1).size();
        const bool leading_zero = runs.at(0) == QLatin1String("0");
        if (k == ldec || runs.at(0).isEmpty()) {
            dec_idx = i;
        } else if (k == lgrp) {
            dec_idx = (after == 3 && !leading_zero) ? -1 : i; // "1,234" groups; "1,5" and "0,123" do not
        } else if (after == 3 && !leading_zero) {
            // A separator this locale uses for neither, before three digits: 1.5 thousand or 1.500?
            *why = numfmt_tr("“%1” is ambiguous: write the decimal point as “%2”, or leave the "
                             "separator out.",
                             "parse_amount error")
                       .arg(core, QString(ldec));
            return false;
        } else {
            dec_idx = i;
        }
    }
    // dec_idx < 0 with several '.' or ',' of one kind: all grouping ("1,234,567").
    if (dec_idx >= 0 && dec_idx != int(seps.size()) - 1) {
        *why = numfmt_tr("Digits after the decimal point cannot be grouped in “%1”.", "parse_amount error")
                   .arg(core);
        return false;
    }

    const int int_runs = dec_idx >= 0 ? dec_idx + 1 : int(runs.size());
    if (int_runs > 1) {
        // Grouping: one separator throughout; western 3-digit groups or Indian 2s + a final 3.
        const QChar g = seps.at(0);
        for (int i = 1; i < int_runs - 1; ++i)
            if (seps.at(i) != g) {
                *why = numfmt_tr("Mixed digit grouping in “%1”.", "parse_amount error").arg(core);
                return false;
            }
        const qsizetype first = runs.at(0).size();
        bool western = first >= 1 && first <= 3;
        for (int i = 1; i < int_runs && western; ++i)
            western = runs.at(i).size() == 3;
        bool indian = int_runs >= 3 && first >= 1 && first <= 2 && runs.at(int_runs - 1).size() == 3;
        for (int i = 1; i < int_runs - 1 && indian; ++i)
            indian = runs.at(i).size() == 2;
        if (!western && !indian) {
            *why = numfmt_tr("Check the digit grouping in “%1”.", "parse_amount error").arg(core);
            return false;
        }
    }
    QString plain;
    for (int i = 0; i < int_runs; ++i)
        plain += runs.at(i);
    if (plain.isEmpty())
        plain = QStringLiteral("0");
    if (dec_idx >= 0)
        plain += QLatin1Char('.') + runs.at(dec_idx + 1);
    bool ok = false;
    const double v = plain.toDouble(&ok); // C locale
    if (!ok || !std::isfinite(v)) {
        *why = numfmt_tr("“%1” is not a number.", "parse_amount error").arg(core);
        return false;
    }
    *out = v;
    return true;
}

/// The currency a unit token stands for (see parse_amount()).
QString numfmt_parse_pick(const QStringList& codes, const QString& token, const QString& dflt,
                          const FormatSettings::Snapshot& s, QString* why) {
    if (codes.size() == 1)
        return codes.first();
    if (!dflt.isEmpty()) {
        if (codes.contains(dflt))
            return dflt;
        const QString dmaj = major_currency(dflt);
        for (const QString& c : codes) // "p" in a GBP field: pence, then converted
            if (major_currency(c) == dmaj)
                return c;
    }
    if (codes.contains(s.display_currency))
        return s.display_currency;
    for (const QString& c : codes) { // the currency whose own context-free symbol / code it is
        CurrencySpec sp;
        if (find_currency(c, &sp) && (sp.symbol == token || sp.code == token))
            return c;
    }
    *why = numfmt_tr("“%1” can mean %2 — type the currency code instead.", "parse_amount error")
               .arg(token, codes.join(QStringLiteral(", ")));
    return {};
}

} // namespace numfmt_detail

ParsedAmount parse_amount(const QString& text, const QString& default_ccy) {
    namespace d = numfmt_detail;
    const FormatSettings::Snapshot s = FormatSettings::instance().snapshot();
    ParsedAmount r;
    QString dflt = default_ccy.trimmed();
    if (!dflt.isEmpty()) {
        CurrencySpec sp;
        if (currency_table_detail::effective_currency_unnamed(dflt, s, &sp))
            dflt = sp.code; // canonical spelling ("usd" -> USD; "GBp" stays)
    }
    r.currency = dflt;
    auto fail = [&r](const QString& why) {
        r.value.reset();
        r.error = why;
        return r;
    };

    QString t = d::numfmt_parse_norm(text);
    if (t.isEmpty())
        return fail(d::numfmt_tr("Enter an amount.", "parse_amount error"));
    if (t == placeholder() || t == QLatin1String("--"))
        return fail(d::numfmt_tr("“%1” stands for a missing value, not an amount.", "parse_amount error").arg(t));
    for (const QChar c : t)
        if (c == QLatin1Char('%') || c.unicode() == 0x2030 || c.unicode() == 0x066A || c.unicode() == 0xFF05)
            return fail(d::numfmt_tr("A percentage is not an amount — enter the amount itself.", "parse_amount error"));
    while (t.startsWith(QChar(0x2248)) || t.startsWith(QLatin1Char('~')))
        t = t.mid(1).trimmed();
    bool negative = false;
    if (t.size() >= 2 && t.startsWith(QLatin1Char('(')) && t.endsWith(QLatin1Char(')'))) { // accounting negative
        negative = true;
        t = t.mid(1, t.size() - 2).trimmed();
    }
    if (t.contains(QLatin1Char('(')) || t.contains(QLatin1Char(')')))
        return fail(d::numfmt_tr("Unbalanced parentheses in “%1”.", "parse_amount error").arg(text.trimmed()));

    int first = -1;
    int last = -1;
    for (int i = 0; i < t.size(); ++i) {
        const int dv = t.at(i).digitValue();
        if (dv >= 0 && dv <= 9) {
            if (first < 0)
                first = i;
            last = i;
        }
    }
    if (first < 0)
        return fail(d::numfmt_tr("“%1” is not a number.", "parse_amount error").arg(text.trimmed()));
    // A decimal point before the first digit (".5", "$.50"), unless it ends a unit ("Bs.1,50").
    if (first > 0) {
        const QChar p = t.at(first - 1);
        if ((p == QLatin1Char('.') || p == QLatin1Char(',') || p.unicode() == 0x066B) &&
            (first < 2 || !t.at(first - 2).isLetter()))
            --first;
    }

    d::NumfmtParseCtx ctx;
    ctx.table = &d::numfmt_parse_table_units();
    for (auto it = s.overrides.cbegin(); it != s.overrides.cend(); ++it) {
        ctx.extra.add(it.value().symbol, it.key(), true);
        ctx.extra.add(it.key(), it.key(), true); // a custom code ("XYZ") — known codes are in the table
    }
    if (!dflt.isEmpty())
        ctx.extra.add(dflt, dflt, true); // an unknown code money() prints as-is ("XYZ 10.00")
    ctx.compact = d::numfmt_parse_compact_tokens();
    ctx.dflt = dflt;

    const QString prefix = t.left(first);
    const QString core = t.mid(first, last - first + 1);
    const QString suffix = t.mid(last + 1);
    d::NumfmtAffix pre;
    d::NumfmtAffix suf;
    QString why;
    if (!d::numfmt_parse_affix(prefix, false, ctx, &pre, &why))
        return fail(why);
    ctx.unit_in_prefix = !pre.unit_token.isEmpty(); // "K 1.5K": the trailing K is the size
    if (!d::numfmt_parse_affix(suffix, true, ctx, &suf, &why))
        return fail(why);
    const int minus = pre.minus + suf.minus;
    const int plus = pre.plus + suf.plus;
    if (minus + plus > 1 || (negative && minus + plus > 0))
        return fail(d::numfmt_tr("More than one sign in “%1”.", "parse_amount error").arg(text.trimmed()));
    if (minus == 1)
        negative = true;
    if (!pre.unit_token.isEmpty() && !suf.unit_token.isEmpty())
        return fail(d::numfmt_tr("Two currencies (%1 and %2) in one amount.", "parse_amount error")
                        .arg(pre.unit_token, suf.unit_token));

    double number = 0.0;
    if (!d::numfmt_parse_core(core, s.locale, &number, &why))
        return fail(why);
    double value = number * suf.mult;
    if (negative)
        value = -value;

    const d::NumfmtAffix& unit = pre.unit_token.isEmpty() ? suf : pre;
    if (!unit.unit_token.isEmpty()) {
        const QString code = d::numfmt_parse_pick(unit.unit_codes, unit.unit_token, dflt, s, &why);
        if (code.isEmpty())
            return fail(why);
        r.currency_in_text = true;
        r.currency = code;
        if (!dflt.isEmpty() && code != dflt && major_currency(code) == major_currency(dflt)) {
            // Pounds typed into a pence field (or pence into pounds): the field's unit.
            value = value * double(minor_unit_divisor(dflt)) / double(minor_unit_divisor(code));
            r.currency = dflt;
        }
    }
    if (!std::isfinite(value))
        return fail(d::numfmt_tr("“%1” is too large.", "parse_amount error").arg(text.trimmed()));
    r.value = value == 0.0 ? 0.0 : value; // never -0
    r.error.clear();
    return r;
}

} // namespace fincept::ui::formatting
