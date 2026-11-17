#include "ui/formatting/FormatSettings.h"

#include "ui/formatting/CurrencyTable.h"

#include <QCoreApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QMutexLocker>
#include <QRegularExpression>

#include <algorithm>

namespace fincept::ui::formatting {

namespace fmtsettings_detail {

constexpr const char* kFmtKeyLocale = "appearance.number_locale";
constexpr const char* kFmtKeyCompact = "appearance.compact_numbers";
constexpr const char* kFmtKeyTimezone = "appearance.display_timezone";
constexpr const char* kFmtKeyClock = "appearance.clock";
constexpr const char* kFmtKeyDisplayCurrency = "appearance.display_currency";
constexpr const char* kFmtKeySymbolStyle = "appearance.currency_symbol_style";
constexpr const char* kFmtKeySymbolPosition = "appearance.currency_symbol_position";
constexpr const char* kFmtKeySymbolSpacing = "appearance.currency_symbol_spacing";
constexpr const char* kFmtKeyNegativeStyle = "appearance.negative_style";
constexpr const char* kFmtKeyGrouping = "appearance.number_grouping";
constexpr const char* kFmtKeyMoneyDecimals = "appearance.money_decimals";
constexpr const char* kFmtKeyMinorUnits = "appearance.minor_units";
constexpr const char* kFmtKeyCodeWhenMixed = "appearance.code_when_mixed";
constexpr const char* kFmtKeyFxStale = "appearance.fx_stale_minutes";
constexpr const char* kFmtKeyFxPnlSplit = "appearance.fx_pnl_split";
constexpr const char* kFmtKeyOverrides = "appearance.currency_overrides";

constexpr int kFmtMinStaleMinutes = 1;
constexpr int kFmtMaxStaleMinutes = 1440;
constexpr int kFmtMaxFixedDecimals = 8;
constexpr int kFmtMaxOverrideDecimals = 10;
constexpr int kFmtMaxSymbolChars = 8;

QString fmtsettings_key(const char* k) {
    return QString::fromLatin1(k);
}

QLocale fmtsettings_with_grouping(QLocale locale) {
    // QLocale::c() sets OmitGroupSeparator; a formatter always wants grouping.
    locale.setNumberOptions(locale.numberOptions() & ~QLocale::OmitGroupSeparator);
    return locale;
}

Compact fmtsettings_normalise_compact(Compact c) {
    return (c == Compact::KMB || c == Compact::LakhCrore) ? c : Compact::Auto;
}

QString fmtsettings_compact_to_string(Compact c) {
    switch (c) {
        case Compact::KMB:
            return QStringLiteral("kmb");
        case Compact::LakhCrore:
            return QStringLiteral("lakh_crore");
        default:
            return QStringLiteral("auto");
    }
}

Compact fmtsettings_compact_from_string(const QString& s) {
    if (s == QLatin1String("kmb"))
        return Compact::KMB;
    if (s == QLatin1String("lakh_crore"))
        return Compact::LakhCrore;
    return Compact::Auto;
}

QString fmtsettings_clock_to_string(HourCycle h) {
    switch (h) {
        case HourCycle::H12:
            return QStringLiteral("12h");
        case HourCycle::Locale:
            return QStringLiteral("locale");
        default:
            return QStringLiteral("24h");
    }
}

HourCycle fmtsettings_clock_from_string(const QString& s) {
    if (s == QLatin1String("12h"))
        return HourCycle::H12;
    if (s == QLatin1String("locale"))
        return HourCycle::Locale;
    return HourCycle::H24;
}

/// Enum <-> stored string, one table per enum (first entry = the default).
template <typename E>
struct FmtEnumName {
    E value;
    const char* name;
};

const FmtEnumName<SymbolStyle> kFmtSymbolStyles[] = {
    {SymbolStyle::Symbol, "symbol"}, {SymbolStyle::Code, "code"}, {SymbolStyle::Disambiguated, "disambiguated"}};
const FmtEnumName<SymbolPosition> kFmtSymbolPositions[] = {
    {SymbolPosition::Locale, "locale"}, {SymbolPosition::Before, "before"}, {SymbolPosition::After, "after"}};
const FmtEnumName<SymbolSpacing> kFmtSymbolSpacings[] = {
    {SymbolSpacing::Auto, "auto"}, {SymbolSpacing::None, "none"}, {SymbolSpacing::Space, "space"}};
const FmtEnumName<NegativeStyle> kFmtNegativeStyles[] = {{NegativeStyle::Minus, "minus"},
                                                         {NegativeStyle::Parentheses, "parentheses"}};
const FmtEnumName<Grouping> kFmtGroupings[] = {
    {Grouping::Always, "always"}, {Grouping::Locale, "locale"}, {Grouping::Indian, "indian"}, {Grouping::None, "none"}};
const FmtEnumName<MinorUnits> kFmtMinorUnits[] = {{MinorUnits::AsQuoted, "quoted"}, {MinorUnits::Major, "major"}};

template <typename E, size_t N>
QString fmtsettings_enum_to_string(const FmtEnumName<E> (&table)[N], E v) {
    for (const auto& e : table)
        if (e.value == v)
            return QString::fromLatin1(e.name);
    return QString::fromLatin1(table[0].name);
}

template <typename E, size_t N>
E fmtsettings_enum_from_string(const FmtEnumName<E> (&table)[N], const QString& s) {
    for (const auto& e : table)
        if (s == QLatin1String(e.name))
            return e.value;
    return table[0].value;
}

template <typename E, size_t N>
E fmtsettings_enum_valid(const FmtEnumName<E> (&table)[N], E v) {
    for (const auto& e : table)
        if (e.value == v)
            return v;
    return table[0].value;
}

/// A stored locale name is usable when QLocale recognised its language
/// (unknown names silently resolve to some other locale).
bool fmtsettings_locale_name_valid(const QString& name, QLocale* out) {
    if (name.isEmpty())
        return false;
    const QLocale l(name);
    const QString wanted = name.section(QLatin1Char('-'), 0, 0).section(QLatin1Char('_'), 0, 0);
    if (l.language() == QLocale::C || l.language() == QLocale::AnyLanguage ||
        QLocale::languageToCode(l.language()).compare(wanted, Qt::CaseInsensitive) != 0)
        return false;
    *out = l;
    return true;
}

/// Canonical spelling of a table code usable as a display currency ("eur" ->
/// "EUR"): not a minor unit, not a precious metal (nothing converts into it);
/// empty otherwise.
QString fmtsettings_known_major_code(const QString& code) {
    CurrencySpec spec;
    if (!find_currency(code.trimmed(), &spec) || spec.minor_divisor != 1 || spec.metal)
        return {};
    return spec.code;
}

bool fmtsettings_custom_code_valid(const QString& code) {
    static const QRegularExpression re(QStringLiteral("^[A-Za-z][A-Za-z0-9]{2,7}$"));
    return re.match(code).hasMatch() && !is_known_currency(code);
}

bool fmtsettings_symbol_valid(const QString& sym) {
    if (sym.size() > kFmtMaxSymbolChars)
        return false;
    for (const QChar c : sym) {
        if (c.isDigit() || c == QLatin1Char('%') || c.category() == QChar::Other_Control ||
            c.category() == QChar::Other_Format)
            return false;
    }
    return true;
}

/// Validates one override. Returns the canonical key in *key (empty when the
/// code is invalid), the cleaned override in *out, and whether it is worth
/// storing (false = equal to the table row: remove it instead).
bool fmtsettings_check_override(const QString& code_in, const CurrencyOverride& in, QString* key,
                                CurrencyOverride* out, bool* worth_storing) {
    const QString code = code_in.trimmed();
    CurrencyOverride o;
    o.symbol = in.symbol.trimmed();
    o.decimals = in.decimals;
    o.name = in.name.trimmed();
    if (!fmtsettings_symbol_valid(o.symbol) || o.decimals < -1 || o.decimals > kFmtMaxOverrideDecimals ||
        o.name.size() > 64)
        return false;

    CurrencySpec spec;
    if (find_currency(code, &spec)) {
        *key = spec.code;
        o.name.clear(); // names of known currencies are the table's (translated)
        if (o.symbol == spec.symbol)
            o.symbol.clear();
        if (o.decimals == spec.decimals)
            o.decimals = -1;
        *worth_storing = !o.symbol.isEmpty() || o.decimals >= 0;
    } else {
        if (!fmtsettings_custom_code_valid(code))
            return false;
        *key = code.toUpper();
        *worth_storing = !o.symbol.isEmpty() || o.decimals >= 0 || !o.name.isEmpty();
    }
    *out = o;
    return true;
}

QHash<QString, CurrencyOverride> fmtsettings_clean_overrides(const QHash<QString, CurrencyOverride>& in) {
    QHash<QString, CurrencyOverride> out;
    for (auto it = in.constBegin(); it != in.constEnd(); ++it) {
        QString key;
        CurrencyOverride o;
        bool store = false;
        if (fmtsettings_check_override(it.key(), it.value(), &key, &o, &store) && store)
            out.insert(key, o);
    }
    return out;
}

/// Display currency as stored: "" (Automatic), a known non-minor code or a custom override code.
QString fmtsettings_clean_display(const QString& code, const QHash<QString, CurrencyOverride>& overrides) {
    const QString c = code.trimmed();
    if (c.isEmpty())
        return {};
    const QString known = fmtsettings_known_major_code(c);
    if (!known.isEmpty())
        return known;
    const QString up = c.toUpper();
    return overrides.contains(up) ? up : QString();
}

} // namespace fmtsettings_detail

using namespace fmtsettings_detail;

FormatSettings& FormatSettings::instance() {
    static FormatSettings* s = [] {
        auto* obj = new FormatSettings();
        // The singleton lives as long as the process; give it the main thread's
        // affinity even if a worker thread happened to format something first.
        if (auto* app = QCoreApplication::instance(); app && obj->thread() != app->thread())
            obj->moveToThread(app->thread());
        return obj;
    }();
    return *s;
}

FormatSettings::Values FormatSettings::normalised(Values v) {
    if (v.locale_follows_app)
        v.pinned_locale = QLocale::c();
    v.compact = fmtsettings_normalise_compact(v.compact);
    // A fixed zone id is kept while another mode is active (switching back to
    // Fixed restores it), so it is only validated when it is in use.
    if (v.tz_mode == TimeZoneMode::Fixed && (v.fixed_tz.isEmpty() || !QTimeZone::isTimeZoneIdAvailable(v.fixed_tz)))
        v.tz_mode = TimeZoneMode::Local;
    v.symbol_style = fmtsettings_enum_valid(kFmtSymbolStyles, v.symbol_style);
    v.symbol_position = fmtsettings_enum_valid(kFmtSymbolPositions, v.symbol_position);
    v.symbol_spacing = fmtsettings_enum_valid(kFmtSymbolSpacings, v.symbol_spacing);
    v.negative_style = fmtsettings_enum_valid(kFmtNegativeStyles, v.negative_style);
    v.grouping = fmtsettings_enum_valid(kFmtGroupings, v.grouping);
    v.minor_units = fmtsettings_enum_valid(kFmtMinorUnits, v.minor_units);
    if (v.decimals_policy != DecimalsPolicy::Adaptive && v.decimals_policy != DecimalsPolicy::Fixed)
        v.decimals_policy = DecimalsPolicy::CurrencyDefault;
    v.fixed_decimals = std::clamp(v.fixed_decimals, 0, kFmtMaxFixedDecimals);
    v.fx_stale_minutes = std::clamp(v.fx_stale_minutes, kFmtMinStaleMinutes, kFmtMaxStaleMinutes);
    v.overrides = fmtsettings_clean_overrides(v.overrides);
    v.display_currency = fmtsettings_clean_display(v.display_currency, v.overrides);
    return v;
}

QString FormatSettings::effective_display_locked() const {
    return v_.display_currency.isEmpty() ? auto_display_ : v_.display_currency;
}

bool FormatSettings::assign_locked(const Values& v) {
    if (v == v_)
        return false;
    v_ = v;
    return true;
}

QLocale FormatSettings::grouped_locale_locked() const {
    const QLocale src = v_.locale_follows_app ? QLocale() : v_.pinned_locale;
    if (!grouped_valid_ || !(src == grouped_src_)) {
        grouped_src_ = src;
        grouped_ = fmtsettings_with_grouping(src);
        grouped_valid_ = true;
    }
    return grouped_;
}

FormatSettings::Snapshot FormatSettings::state_locked() const {
    Snapshot s;
    s.locale_follows_app = v_.locale_follows_app;
    s.locale = grouped_locale_locked();
    s.compact = v_.compact;
    s.tz_mode = v_.tz_mode;
    s.fixed_tz = v_.fixed_tz;
    s.hour_cycle = v_.hour_cycle;
    s.display_currency = effective_display_locked();
    s.display_currency_auto = v_.display_currency.isEmpty();
    s.symbol_style = v_.symbol_style;
    s.symbol_position = v_.symbol_position;
    s.symbol_spacing = v_.symbol_spacing;
    s.negative_style = v_.negative_style;
    s.grouping = v_.grouping;
    s.decimals_policy = v_.decimals_policy;
    s.fixed_decimals = v_.fixed_decimals;
    s.minor_units = v_.minor_units;
    s.code_when_mixed = v_.code_when_mixed;
    s.fx_stale_minutes = v_.fx_stale_minutes;
    s.fx_pnl_split = v_.fx_pnl_split;
    s.overrides = v_.overrides;
    return s;
}

FormatSettings::Snapshot FormatSettings::snapshot() const {
    QMutexLocker lock(&mutex_);
    return state_locked();
}

FormatSettings::Values FormatSettings::values() const {
    QMutexLocker lock(&mutex_);
    return v_;
}

void FormatSettings::set_values(const Values& v) {
    const Values n = normalised(v);
    bool any = false;
    {
        QMutexLocker lock(&mutex_);
        any = assign_locked(n);
    }
    if (any)
        emit changed();
}

// One setter = lock, compare, assign, unlock, emit once.
#define FMTSETTINGS_SET_FIELD(field, value)                                                                            \
    do {                                                                                                               \
        {                                                                                                              \
            QMutexLocker lock(&mutex_);                                                                                \
            if (v_.field == (value))                                                                                   \
                return;                                                                                                \
            v_.field = (value);                                                                                        \
        }                                                                                                              \
        emit changed();                                                                                                \
    } while (false)

// ── Locale ──────────────────────────────────────────────────────────────────

QLocale FormatSettings::locale() const {
    QMutexLocker lock(&mutex_);
    return grouped_locale_locked();
}

bool FormatSettings::locale_follows_app() const {
    QMutexLocker lock(&mutex_);
    return v_.locale_follows_app;
}

void FormatSettings::set_locale(const QLocale& locale) {
    {
        QMutexLocker lock(&mutex_);
        if (!v_.locale_follows_app && v_.pinned_locale == locale)
            return;
        v_.locale_follows_app = false;
        v_.pinned_locale = locale;
    }
    emit changed();
}

void FormatSettings::follow_app_locale() {
    {
        QMutexLocker lock(&mutex_);
        if (v_.locale_follows_app)
            return;
        v_.locale_follows_app = true;
        v_.pinned_locale = QLocale::c();
    }
    emit changed();
}

void FormatSettings::notify_app_locale_changed() {
    if (locale_follows_app())
        emit changed();
}

// ── Compact ─────────────────────────────────────────────────────────────────

Compact FormatSettings::compact_scheme() const {
    QMutexLocker lock(&mutex_);
    return v_.compact;
}

void FormatSettings::set_compact_scheme(Compact scheme) {
    scheme = fmtsettings_normalise_compact(scheme);
    FMTSETTINGS_SET_FIELD(compact, scheme);
}

// ── Time zone ───────────────────────────────────────────────────────────────

TimeZoneMode FormatSettings::timezone_mode() const {
    QMutexLocker lock(&mutex_);
    return v_.tz_mode;
}

QByteArray FormatSettings::fixed_timezone_id() const {
    QMutexLocker lock(&mutex_);
    return v_.fixed_tz;
}

void FormatSettings::set_timezone_mode(TimeZoneMode mode) {
    if (mode == TimeZoneMode::Fixed) {
        // Fixed needs an id: re-use the last one, if any. Otherwise a no-op.
        set_fixed_timezone(fixed_timezone_id());
        return;
    }
    {
        QMutexLocker lock(&mutex_);
        if (v_.tz_mode == mode)
            return;
        v_.tz_mode = mode; // the last fixed id is kept so switching back to Fixed restores it
    }
    emit changed();
}

bool FormatSettings::set_fixed_timezone(const QByteArray& iana_id) {
    if (iana_id.isEmpty() || !QTimeZone::isTimeZoneIdAvailable(iana_id))
        return false;
    {
        QMutexLocker lock(&mutex_);
        if (v_.tz_mode == TimeZoneMode::Fixed && v_.fixed_tz == iana_id)
            return true;
        v_.tz_mode = TimeZoneMode::Fixed;
        v_.fixed_tz = iana_id;
    }
    emit changed();
    return true;
}

QTimeZone FormatSettings::display_zone(const QTimeZone& exchange_tz) const {
    TimeZoneMode mode;
    QByteArray fixed;
    {
        QMutexLocker lock(&mutex_);
        mode = v_.tz_mode;
        fixed = v_.fixed_tz;
    }
    switch (mode) {
        case TimeZoneMode::UTC:
            return QTimeZone(QTimeZone::UTC);
        case TimeZoneMode::Exchange:
            if (exchange_tz.isValid())
                return exchange_tz;
            break;
        case TimeZoneMode::Fixed: {
            const QTimeZone tz(fixed);
            if (tz.isValid())
                return tz;
            break;
        }
        case TimeZoneMode::Local:
            break;
    }
    return QTimeZone(QTimeZone::LocalTime);
}

// ── Clock ───────────────────────────────────────────────────────────────────

HourCycle FormatSettings::hour_cycle() const {
    QMutexLocker lock(&mutex_);
    return v_.hour_cycle;
}

void FormatSettings::set_hour_cycle(HourCycle cycle) {
    if (cycle != HourCycle::H12 && cycle != HourCycle::Locale)
        cycle = HourCycle::H24;
    FMTSETTINGS_SET_FIELD(hour_cycle, cycle);
}

// ── Display currency ────────────────────────────────────────────────────────

QString FormatSettings::display_currency() const {
    QMutexLocker lock(&mutex_);
    return effective_display_locked();
}

bool FormatSettings::display_currency_is_auto() const {
    QMutexLocker lock(&mutex_);
    return v_.display_currency.isEmpty();
}

QString FormatSettings::auto_display_currency() const {
    QMutexLocker lock(&mutex_);
    return auto_display_;
}

bool FormatSettings::set_display_currency(const QString& code) {
    const QString trimmed = code.trimmed();
    {
        QMutexLocker lock(&mutex_);
        const QString clean = fmtsettings_clean_display(trimmed, v_.overrides);
        if (!trimmed.isEmpty() && clean.isEmpty())
            return false;
        if (v_.display_currency == clean)
            return true;
        v_.display_currency = clean;
    }
    emit changed();
    return true;
}

void FormatSettings::set_auto_display_currency(const QString& code) {
    const QString clean = fmtsettings_known_major_code(code);
    if (clean.isEmpty())
        return;
    bool effective_moved = false;
    {
        QMutexLocker lock(&mutex_);
        if (auto_display_ == clean)
            return;
        auto_display_ = clean;
        effective_moved = v_.display_currency.isEmpty();
    }
    if (effective_moved)
        emit changed();
}

// ── Currency style ──────────────────────────────────────────────────────────

SymbolStyle FormatSettings::symbol_style() const {
    QMutexLocker lock(&mutex_);
    return v_.symbol_style;
}

void FormatSettings::set_symbol_style(SymbolStyle style) {
    style = fmtsettings_enum_valid(kFmtSymbolStyles, style);
    FMTSETTINGS_SET_FIELD(symbol_style, style);
}

SymbolPosition FormatSettings::symbol_position() const {
    QMutexLocker lock(&mutex_);
    return v_.symbol_position;
}

void FormatSettings::set_symbol_position(SymbolPosition position) {
    position = fmtsettings_enum_valid(kFmtSymbolPositions, position);
    FMTSETTINGS_SET_FIELD(symbol_position, position);
}

SymbolSpacing FormatSettings::symbol_spacing() const {
    QMutexLocker lock(&mutex_);
    return v_.symbol_spacing;
}

void FormatSettings::set_symbol_spacing(SymbolSpacing spacing) {
    spacing = fmtsettings_enum_valid(kFmtSymbolSpacings, spacing);
    FMTSETTINGS_SET_FIELD(symbol_spacing, spacing);
}

NegativeStyle FormatSettings::negative_style() const {
    QMutexLocker lock(&mutex_);
    return v_.negative_style;
}

void FormatSettings::set_negative_style(NegativeStyle style) {
    style = fmtsettings_enum_valid(kFmtNegativeStyles, style);
    FMTSETTINGS_SET_FIELD(negative_style, style);
}

Grouping FormatSettings::grouping() const {
    QMutexLocker lock(&mutex_);
    return v_.grouping;
}

void FormatSettings::set_grouping(Grouping grouping) {
    grouping = fmtsettings_enum_valid(kFmtGroupings, grouping);
    FMTSETTINGS_SET_FIELD(grouping, grouping);
}

DecimalsPolicy FormatSettings::money_decimals_policy() const {
    QMutexLocker lock(&mutex_);
    return v_.decimals_policy;
}

int FormatSettings::fixed_decimals() const {
    QMutexLocker lock(&mutex_);
    return v_.fixed_decimals;
}

void FormatSettings::set_money_decimals(DecimalsPolicy policy, int fixed) {
    if (policy != DecimalsPolicy::Adaptive && policy != DecimalsPolicy::Fixed)
        policy = DecimalsPolicy::CurrencyDefault;
    fixed = std::clamp(fixed, 0, kFmtMaxFixedDecimals);
    {
        QMutexLocker lock(&mutex_);
        if (v_.decimals_policy == policy && v_.fixed_decimals == fixed)
            return;
        v_.decimals_policy = policy;
        v_.fixed_decimals = fixed;
    }
    emit changed();
}

MinorUnits FormatSettings::minor_units() const {
    QMutexLocker lock(&mutex_);
    return v_.minor_units;
}

void FormatSettings::set_minor_units(MinorUnits mode) {
    mode = fmtsettings_enum_valid(kFmtMinorUnits, mode);
    FMTSETTINGS_SET_FIELD(minor_units, mode);
}

bool FormatSettings::code_when_mixed() const {
    QMutexLocker lock(&mutex_);
    return v_.code_when_mixed;
}

void FormatSettings::set_code_when_mixed(bool on) {
    FMTSETTINGS_SET_FIELD(code_when_mixed, on);
}

// ── FX ──────────────────────────────────────────────────────────────────────

int FormatSettings::fx_stale_minutes() const {
    QMutexLocker lock(&mutex_);
    return v_.fx_stale_minutes;
}

void FormatSettings::set_fx_stale_minutes(int minutes) {
    minutes = std::clamp(minutes, kFmtMinStaleMinutes, kFmtMaxStaleMinutes);
    FMTSETTINGS_SET_FIELD(fx_stale_minutes, minutes);
}

bool FormatSettings::fx_pnl_split() const {
    QMutexLocker lock(&mutex_);
    return v_.fx_pnl_split;
}

void FormatSettings::set_fx_pnl_split(bool on) {
    FMTSETTINGS_SET_FIELD(fx_pnl_split, on);
}

#undef FMTSETTINGS_SET_FIELD

// ── Overrides ───────────────────────────────────────────────────────────────

bool FormatSettings::set_currency_override(const QString& code, const CurrencyOverride& o) {
    QString key;
    CurrencyOverride clean;
    bool store = false;
    if (!fmtsettings_check_override(code, o, &key, &clean, &store))
        return false;
    {
        QMutexLocker lock(&mutex_);
        const auto it = v_.overrides.constFind(key);
        if (store) {
            if (it != v_.overrides.constEnd() && it.value() == clean)
                return true;
            v_.overrides.insert(key, clean);
        } else {
            if (it == v_.overrides.constEnd())
                return true;
            v_.overrides.remove(key);
            // A custom display currency cannot outlive its definition.
            if (v_.display_currency == key && !is_known_currency(key))
                v_.display_currency.clear();
        }
    }
    emit changed();
    return true;
}

void FormatSettings::remove_currency_override(const QString& code) {
    const QString c = code.trimmed();
    {
        QMutexLocker lock(&mutex_);
        CurrencySpec spec;
        QString key = find_currency(c, &spec) ? spec.code : c.toUpper();
        if (!v_.overrides.contains(key))
            return;
        v_.overrides.remove(key);
        if (v_.display_currency == key && !is_known_currency(key))
            v_.display_currency.clear();
    }
    emit changed();
}

QHash<QString, CurrencyOverride> FormatSettings::currency_overrides() const {
    QMutexLocker lock(&mutex_);
    return v_.overrides;
}

bool FormatSettings::currency_override(const QString& code, CurrencyOverride* out) const {
    QMutexLocker lock(&mutex_);
    if (v_.overrides.isEmpty())
        return false;
    const QString c = code.trimmed();
    auto it = v_.overrides.constFind(c);
    if (it == v_.overrides.constEnd()) {
        CurrencySpec spec;
        const QString key = find_currency(c, &spec) ? spec.code : c.toUpper();
        it = v_.overrides.constFind(key);
        if (it == v_.overrides.constEnd())
            return false;
    }
    if (out)
        *out = it.value();
    return true;
}

// ── Persistence helpers ─────────────────────────────────────────────────────

QStringList FormatSettings::setting_keys() {
    return {fmtsettings_key(kFmtKeyLocale),          fmtsettings_key(kFmtKeyCompact),        fmtsettings_key(kFmtKeyTimezone),
            fmtsettings_key(kFmtKeyClock),           fmtsettings_key(kFmtKeyDisplayCurrency), fmtsettings_key(kFmtKeySymbolStyle),
            fmtsettings_key(kFmtKeySymbolPosition),  fmtsettings_key(kFmtKeySymbolSpacing),   fmtsettings_key(kFmtKeyNegativeStyle),
            fmtsettings_key(kFmtKeyGrouping),        fmtsettings_key(kFmtKeyMoneyDecimals),   fmtsettings_key(kFmtKeyMinorUnits),
            fmtsettings_key(kFmtKeyCodeWhenMixed),   fmtsettings_key(kFmtKeyFxStale),         fmtsettings_key(kFmtKeyFxPnlSplit),
            fmtsettings_key(kFmtKeyOverrides)};
}

QString FormatSettings::overrides_to_json(const QHash<QString, CurrencyOverride>& overrides) {
    if (overrides.isEmpty())
        return {};
    QJsonObject root;
    for (auto it = overrides.constBegin(); it != overrides.constEnd(); ++it) {
        QJsonObject o;
        if (!it.value().symbol.isEmpty())
            o.insert(QStringLiteral("s"), it.value().symbol);
        if (it.value().decimals >= 0)
            o.insert(QStringLiteral("d"), it.value().decimals);
        if (!it.value().name.isEmpty())
            o.insert(QStringLiteral("n"), it.value().name);
        root.insert(it.key(), o);
    }
    return QString::fromUtf8(QJsonDocument(root).toJson(QJsonDocument::Compact));
}

QHash<QString, CurrencyOverride> FormatSettings::overrides_from_json(const QString& json) {
    QHash<QString, CurrencyOverride> raw;
    if (json.trimmed().isEmpty())
        return raw;
    QJsonParseError err{};
    const QJsonDocument doc = QJsonDocument::fromJson(json.toUtf8(), &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject())
        return raw;
    const QJsonObject root = doc.object();
    for (auto it = root.constBegin(); it != root.constEnd(); ++it) {
        if (!it.value().isObject())
            continue;
        const QJsonObject o = it.value().toObject();
        CurrencyOverride ov;
        ov.symbol = o.value(QStringLiteral("s")).toString();
        const QJsonValue d = o.value(QStringLiteral("d"));
        ov.decimals = d.isDouble() ? d.toInt(-1) : -1;
        ov.name = o.value(QStringLiteral("n")).toString();
        raw.insert(it.key(), ov);
    }
    return fmtsettings_clean_overrides(raw);
}

QHash<QString, QString> FormatSettings::settings_values() const {
    QMutexLocker lock(&mutex_);
    QHash<QString, QString> v;
    v.insert(fmtsettings_key(kFmtKeyLocale), v_.locale_follows_app ? QString() : v_.pinned_locale.bcp47Name());
    v.insert(fmtsettings_key(kFmtKeyCompact), fmtsettings_compact_to_string(v_.compact));
    QString tz;
    switch (v_.tz_mode) {
        case TimeZoneMode::UTC:
            tz = QStringLiteral("utc");
            break;
        case TimeZoneMode::Exchange:
            tz = QStringLiteral("exchange");
            break;
        case TimeZoneMode::Fixed:
            tz = QString::fromLatin1(v_.fixed_tz);
            break;
        case TimeZoneMode::Local:
            tz = QStringLiteral("local");
            break;
    }
    v.insert(fmtsettings_key(kFmtKeyTimezone), tz);
    v.insert(fmtsettings_key(kFmtKeyClock), fmtsettings_clock_to_string(v_.hour_cycle));
    v.insert(fmtsettings_key(kFmtKeyDisplayCurrency), v_.display_currency);
    v.insert(fmtsettings_key(kFmtKeySymbolStyle), fmtsettings_enum_to_string(kFmtSymbolStyles, v_.symbol_style));
    v.insert(fmtsettings_key(kFmtKeySymbolPosition), fmtsettings_enum_to_string(kFmtSymbolPositions, v_.symbol_position));
    v.insert(fmtsettings_key(kFmtKeySymbolSpacing), fmtsettings_enum_to_string(kFmtSymbolSpacings, v_.symbol_spacing));
    v.insert(fmtsettings_key(kFmtKeyNegativeStyle), fmtsettings_enum_to_string(kFmtNegativeStyles, v_.negative_style));
    v.insert(fmtsettings_key(kFmtKeyGrouping), fmtsettings_enum_to_string(kFmtGroupings, v_.grouping));
    QString dec;
    switch (v_.decimals_policy) {
        case DecimalsPolicy::Adaptive:
            dec = QStringLiteral("adaptive");
            break;
        case DecimalsPolicy::Fixed:
            dec = QString::number(v_.fixed_decimals);
            break;
        case DecimalsPolicy::CurrencyDefault:
            dec = QStringLiteral("currency");
            break;
    }
    v.insert(fmtsettings_key(kFmtKeyMoneyDecimals), dec);
    v.insert(fmtsettings_key(kFmtKeyMinorUnits), fmtsettings_enum_to_string(kFmtMinorUnits, v_.minor_units));
    v.insert(fmtsettings_key(kFmtKeyCodeWhenMixed), v_.code_when_mixed ? QStringLiteral("1") : QStringLiteral("0"));
    v.insert(fmtsettings_key(kFmtKeyFxStale), QString::number(v_.fx_stale_minutes));
    v.insert(fmtsettings_key(kFmtKeyFxPnlSplit), v_.fx_pnl_split ? QStringLiteral("1") : QStringLiteral("0"));
    v.insert(fmtsettings_key(kFmtKeyOverrides), overrides_to_json(v_.overrides));
    return v;
}

void FormatSettings::apply_settings_values(const QHash<QString, QString>& values) {
    auto get = [&values](const char* key) { return values.value(fmtsettings_key(key)).trimmed(); };

    Values n;
    QLocale pinned;
    if (fmtsettings_locale_name_valid(get(kFmtKeyLocale), &pinned)) {
        n.locale_follows_app = false;
        n.pinned_locale = pinned;
    }
    n.compact = fmtsettings_compact_from_string(get(kFmtKeyCompact));

    const QString tz_value = get(kFmtKeyTimezone);
    if (tz_value == QLatin1String("utc")) {
        n.tz_mode = TimeZoneMode::UTC;
    } else if (tz_value == QLatin1String("exchange")) {
        n.tz_mode = TimeZoneMode::Exchange;
    } else if (!tz_value.isEmpty() && tz_value != QLatin1String("local")) {
        const QByteArray id = tz_value.toUtf8();
        if (QTimeZone::isTimeZoneIdAvailable(id)) {
            n.tz_mode = TimeZoneMode::Fixed;
            n.fixed_tz = id;
        }
    }
    n.hour_cycle = fmtsettings_clock_from_string(get(kFmtKeyClock));

    // Overrides first: a custom display currency is valid only with its definition.
    n.overrides = overrides_from_json(values.value(fmtsettings_key(kFmtKeyOverrides)));
    n.display_currency = get(kFmtKeyDisplayCurrency);
    n.symbol_style = fmtsettings_enum_from_string(kFmtSymbolStyles, get(kFmtKeySymbolStyle));
    n.symbol_position = fmtsettings_enum_from_string(kFmtSymbolPositions, get(kFmtKeySymbolPosition));
    n.symbol_spacing = fmtsettings_enum_from_string(kFmtSymbolSpacings, get(kFmtKeySymbolSpacing));
    n.negative_style = fmtsettings_enum_from_string(kFmtNegativeStyles, get(kFmtKeyNegativeStyle));
    n.grouping = fmtsettings_enum_from_string(kFmtGroupings, get(kFmtKeyGrouping));

    const QString dec = get(kFmtKeyMoneyDecimals);
    bool num_ok = false;
    const int fixed = dec.toInt(&num_ok);
    if (dec == QLatin1String("adaptive")) {
        n.decimals_policy = DecimalsPolicy::Adaptive;
    } else if (num_ok && fixed >= 0 && fixed <= kFmtMaxFixedDecimals) {
        n.decimals_policy = DecimalsPolicy::Fixed;
        n.fixed_decimals = fixed;
    }
    n.minor_units = fmtsettings_enum_from_string(kFmtMinorUnits, get(kFmtKeyMinorUnits));
    n.code_when_mixed = get(kFmtKeyCodeWhenMixed) != QLatin1String("0");
    const QString stale = get(kFmtKeyFxStale);
    const int stale_min = stale.toInt(&num_ok);
    if (num_ok)
        n.fx_stale_minutes = stale_min;
    n.fx_pnl_split = get(kFmtKeyFxPnlSplit) != QLatin1String("0");

    set_values(n);
}

void FormatSettings::reset_to_defaults() {
    apply_settings_values({});
}

// ── Pure helpers ────────────────────────────────────────────────────────────

QString FormatSettings::resolve_auto_display_currency(bool region_chosen, const QString& region_currency,
                                                      const QLocale& system_locale) {
    const QString region = fmtsettings_known_major_code(region_currency);
    if (region_chosen && !region.isEmpty())
        return region;
    const QString iso = fmtsettings_known_major_code(system_locale.currencySymbol(QLocale::CurrencyIsoCode));
    if (!iso.isEmpty())
        return iso;
    if (!region.isEmpty())
        return region;
    return QStringLiteral("USD");
}

QString FormatSettings::migrate_legacy_display_currency(const QString& legacy_general_currency) {
    const QString code = fmtsettings_known_major_code(legacy_general_currency);
    // "USD" was the old default: a user who never chose and one who chose it
    // back cannot be told apart, so both get Automatic.
    if (code.isEmpty() || code == QLatin1String("USD"))
        return {};
    return code;
}

} // namespace fincept::ui::formatting
