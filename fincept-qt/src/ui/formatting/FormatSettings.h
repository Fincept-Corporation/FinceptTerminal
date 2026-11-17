#pragma once
// FormatSettings — the user's number / currency / date display preferences,
// shared by every formatter in ui/formatting (NumberFormat.h, DateFormat.h).
//
// Pure Qt Core: this object holds and validates the values only. Loading from
// and saving to SettingsRepository lives in FormatSettingsStore.{h,cpp}, so the
// formatters (and their unit test) do not drag in QtSql. The settings UI is
// FormatSettingsPanel.{h,cpp}.
//
// Reads are thread-safe (a snapshot is taken under a mutex); setters belong on
// the GUI thread. changed() fires exactly once after any effective change and
// never for a setter call that changes nothing.
//
// Display currency: the ONE currency aggregates are converted into when a
// screen has no more specific currency of its own (portfolio base, account
// currency). "" = Automatic, which resolves to the user's region / system
// locale currency (set_auto_display_currency(), done by FormatSettingsStore).
// Prices are never converted; they keep the instrument's own currency.

#include <QByteArray>
#include <QHash>
#include <QLocale>
#include <QMutex>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QTimeZone>

namespace fincept::ui::formatting {

/// How large numbers are abbreviated.
enum class Compact {
    None,         ///< no abbreviation: the full grouped number
    FromSettings, ///< whatever FormatSettings::compact_scheme() says
    KMB,          ///< K / M / B / T
    LakhCrore,    ///< K / L / Cr / L Cr (South-Asian numbering)
    Auto,         ///< the user's scheme when they chose one; otherwise LakhCrore for
                  ///< INR/PKR/BDT/NPR/LKR (or, with no currency, a South-Asian
                  ///< number locale) and KMB for everything else
};

/// Which zone timestamps are shown in.
enum class TimeZoneMode {
    Local,    ///< the machine's zone
    UTC,      ///< UTC
    Exchange, ///< the instrument's exchange zone (callers pass it); Local when unknown
    Fixed,    ///< a fixed IANA zone, FormatSettings::fixed_timezone_id()
};

/// 12- or 24-hour clock.
enum class HourCycle {
    H24,    ///< 14:05
    H12,    ///< 2:05 PM
    Locale, ///< whatever the number locale's short time format uses
};

/// What money() prints as the currency unit.
enum class SymbolStyle {
    Symbol,        ///< "$", "€", "₹"; a symbol several shown currencies share is
                   ///< disambiguated automatically ("US$" / "C$" side by side)
    Code,          ///< ISO code: "USD 1,234.50"
    Disambiguated, ///< always the unambiguous form: "US$", "C$", "JP¥", "CN¥"
};
/// Where the unit goes. Suffix units (pence "p", cents "c") always follow the number.
enum class SymbolPosition {
    Locale, ///< the number locale's own pattern ("$1.00" en_US, "1,00 €" de_DE)
    Before, ///< "€1.234,56"
    After,  ///< "1,234.56 $"
};
/// The gap between unit and number.
enum class SymbolSpacing {
    Auto,  ///< the locale's gap; a space where a letter would touch a digit ("CHF 5")
    None,  ///< never a gap: "CHF5", "5$"
    Space, ///< always a (no-break) space: "$ 5"
};
/// How negative amounts are written (money only; prices, percentages and chart
/// axes always keep the minus sign).
enum class NegativeStyle {
    Minus,       ///< "-$1,234.50"
    Parentheses, ///< "($1,234.50)"
};
/// Digit grouping.
enum class Grouping {
    Always, ///< the locale's separator from 1,000 up (also in es/it/pl, whose CLDR rule leaves 4 digits ungrouped)
    Locale, ///< exactly what the number locale does
    Indian, ///< 12,34,567.89 with the locale's separator
    None,   ///< 1234567.89
};
/// Decimals for full (not abbreviated) amounts of money.
enum class DecimalsPolicy {
    CurrencyDefault, ///< the currency's own (USD 2, JPY 0, KWD 3; crypto up to 8, trailing zeros trimmed)
    Adaptive,        ///< the currency's own, but never print a non-zero amount as zero ($0.00042)
    Fixed,           ///< FormatSettings fixed_decimals (0..8)
};
/// How amounts quoted in a minor unit (GBp pence, ZAc cents, USX US cents) are shown.
enum class MinorUnits {
    AsQuoted, ///< "117.70p"
    Major,    ///< "£1.1770" (divided by 100)
};

/// A user's change to one currency's display, or a custom currency code.
struct CurrencyOverride {
    QString symbol{};  ///< empty = the table's symbol
    int decimals = -1; ///< -1 = the table's decimals; 0..10
    QString name{};    ///< custom codes only; empty otherwise
    bool operator==(const CurrencyOverride&) const = default;
};

class FormatSettings : public QObject {
    Q_OBJECT
  public:
    static FormatSettings& instance();

    /// Everything the user can set. Plain data, comparable.
    struct Values {
        // number locale, compact scheme, time zone, clock
        bool locale_follows_app = true;
        QLocale pinned_locale = QLocale::c(); ///< used when !locale_follows_app
        Compact compact = Compact::Auto;
        TimeZoneMode tz_mode = TimeZoneMode::Local;
        QByteArray fixed_tz;
        HourCycle hour_cycle = HourCycle::H24;
        // currency
        QString display_currency; ///< "" = Automatic
        SymbolStyle symbol_style = SymbolStyle::Symbol;
        SymbolPosition symbol_position = SymbolPosition::Locale;
        SymbolSpacing symbol_spacing = SymbolSpacing::Auto;
        NegativeStyle negative_style = NegativeStyle::Minus;
        Grouping grouping = Grouping::Always;
        DecimalsPolicy decimals_policy = DecimalsPolicy::CurrencyDefault;
        int fixed_decimals = 2; ///< used when decimals_policy == Fixed; 0..8
        MinorUnits minor_units = MinorUnits::AsQuoted;
        bool code_when_mixed = true; ///< ISO codes in table columns that mix currencies
        int fx_stale_minutes = 30;   ///< an FX rate older than this is marked stale; 1..1440
        bool fx_pnl_split = true;    ///< show price P&L and FX P&L apart when a holding's currency differs from the base
        QHash<QString, CurrencyOverride> overrides; ///< key = canonical code
        bool operator==(const Values&) const = default;
    };

    /// Everything a formatter needs, copied once per call.
    struct Snapshot {
        QLocale locale;                  ///< effective number locale, group separators always on
        bool locale_follows_app = true;  ///< locale tracks QLocale() (the UI language)
        Compact compact = Compact::Auto; ///< KMB, LakhCrore or Auto
        TimeZoneMode tz_mode = TimeZoneMode::Local;
        QByteArray fixed_tz; ///< IANA id when tz_mode == Fixed
        HourCycle hour_cycle = HourCycle::H24;
        QString display_currency;          ///< EFFECTIVE display currency, never empty
        bool display_currency_auto = true; ///< display_currency came from Automatic
        SymbolStyle symbol_style = SymbolStyle::Symbol;
        SymbolPosition symbol_position = SymbolPosition::Locale;
        SymbolSpacing symbol_spacing = SymbolSpacing::Auto;
        NegativeStyle negative_style = NegativeStyle::Minus;
        Grouping grouping = Grouping::Always;
        DecimalsPolicy decimals_policy = DecimalsPolicy::CurrencyDefault;
        int fixed_decimals = 2;
        MinorUnits minor_units = MinorUnits::AsQuoted;
        bool code_when_mixed = true;
        int fx_stale_minutes = 30;
        bool fx_pnl_split = true;
        QHash<QString, CurrencyOverride> overrides; ///< implicitly shared: O(1) to copy
    };
    Snapshot snapshot() const;

    /// Every user value at once (under the mutex).
    Values values() const;
    /// Validates every field (invalid ones fall back to their default, overrides
    /// that fail validation are dropped) and emits changed() once if anything
    /// differs. Use it for "Reset to defaults" and other multi-field edits.
    void set_values(const Values& v);

    // ── Number locale ────────────────────────────────────────────────────────
    /// Effective number locale: QLocale() (the app language set by
    /// LanguageManager) unless overridden. Group separators are always enabled.
    QLocale locale() const;
    bool locale_follows_app() const;
    /// Pin the number locale (e.g. en_IN for 1,23,45,678 grouping).
    void set_locale(const QLocale& locale);
    /// Go back to following QLocale() (the default).
    void follow_app_locale();
    /// Call when QLocale::setDefault() changed; re-emits changed() if the
    /// locale follows the app. FormatSettingsStore wires this to LanguageManager.
    void notify_app_locale_changed();

    // ── Compact numbers ──────────────────────────────────────────────────────
    /// KMB, LakhCrore or Auto (never None / FromSettings).
    Compact compact_scheme() const;
    /// None / FromSettings are stored as Auto.
    void set_compact_scheme(Compact scheme);

    // ── Time zone ────────────────────────────────────────────────────────────
    TimeZoneMode timezone_mode() const;
    QByteArray fixed_timezone_id() const;
    /// Local, UTC or Exchange. Fixed needs an id: use set_fixed_timezone().
    void set_timezone_mode(TimeZoneMode mode);
    /// Switch to a fixed IANA zone ("Asia/Kolkata"). Returns false (and changes
    /// nothing) when the id is not a zone this machine knows.
    bool set_fixed_timezone(const QByteArray& iana_id);
    /// The zone timestamps are displayed in. `exchange_tz` is used in Exchange
    /// mode; an invalid one falls back to the local zone.
    QTimeZone display_zone(const QTimeZone& exchange_tz = QTimeZone()) const;

    // ── Clock ────────────────────────────────────────────────────────────────
    HourCycle hour_cycle() const;
    void set_hour_cycle(HourCycle cycle);

    // ── Display currency ─────────────────────────────────────────────────────
    /// Effective display currency, never empty ("USD" until anything is known).
    QString display_currency() const;
    bool display_currency_is_auto() const;
    /// What Automatic currently resolves to.
    QString auto_display_currency() const;
    /// "" = Automatic. A known non-minor code (any case: "eur" -> "EUR") or a
    /// custom override code. Returns false and changes nothing for anything else
    /// ("GBp", "XX", "zzz").
    bool set_display_currency(const QString& code);
    /// What Automatic resolves to (the region's / system locale's currency).
    /// Called by FormatSettingsStore; emits changed() only when the effective
    /// display currency moves (i.e. while in Automatic). Invalid codes are ignored.
    void set_auto_display_currency(const QString& code);

    // ── Currency style ───────────────────────────────────────────────────────
    SymbolStyle symbol_style() const;
    void set_symbol_style(SymbolStyle style);
    SymbolPosition symbol_position() const;
    void set_symbol_position(SymbolPosition position);
    SymbolSpacing symbol_spacing() const;
    void set_symbol_spacing(SymbolSpacing spacing);
    NegativeStyle negative_style() const;
    void set_negative_style(NegativeStyle style);
    Grouping grouping() const;
    void set_grouping(Grouping grouping);
    DecimalsPolicy money_decimals_policy() const;
    int fixed_decimals() const;
    /// One call for the policy and the fixed count (clamped 0..8; kept even when
    /// the policy is not Fixed, so switching back restores it).
    void set_money_decimals(DecimalsPolicy policy, int fixed = 2);
    MinorUnits minor_units() const;
    void set_minor_units(MinorUnits mode);
    bool code_when_mixed() const;
    void set_code_when_mixed(bool on);

    // ── FX ───────────────────────────────────────────────────────────────────
    int fx_stale_minutes() const;
    /// Clamped to 1..1440.
    void set_fx_stale_minutes(int minutes);
    bool fx_pnl_split() const;
    void set_fx_pnl_split(bool on);

    // ── Per-currency overrides ───────────────────────────────────────────────
    /// Known code (stored under its canonical spelling: "usd" -> "USD", "GBp"
    /// stays) or a custom code (3..8 letters/digits starting with a letter,
    /// stored upper-case, must not clash with a known code). symbol: 0..8
    /// characters, no digits, no '%', no control characters; decimals -1..10.
    /// An override equal to the table row is removed instead of stored.
    /// Returns false (nothing changed) when invalid.
    bool set_currency_override(const QString& code, const CurrencyOverride& o);
    void remove_currency_override(const QString& code);
    QHash<QString, CurrencyOverride> currency_overrides() const;
    /// The override for `code` (exact canonical key, then case-insensitive).
    bool currency_override(const QString& code, CurrencyOverride* out) const;

    // ── Persistence helpers (used by FormatSettingsStore) ────────────────────
    /// The SettingsRepository keys, all in the "appearance" category:
    ///   appearance.number_locale            ""  (follow app) | BCP-47 name, e.g. "en-IN"
    ///   appearance.compact_numbers          "auto" | "kmb" | "lakh_crore"
    ///   appearance.display_timezone         "local" | "utc" | "exchange" | IANA id
    ///   appearance.clock                    "24h" | "12h" | "locale"
    ///   appearance.display_currency         "" (automatic) | currency code
    ///   appearance.currency_symbol_style    "symbol" | "code" | "disambiguated"
    ///   appearance.currency_symbol_position "locale" | "before" | "after"
    ///   appearance.currency_symbol_spacing  "auto" | "none" | "space"
    ///   appearance.negative_style           "minus" | "parentheses"
    ///   appearance.number_grouping          "always" | "locale" | "indian" | "none"
    ///   appearance.money_decimals           "currency" | "adaptive" | "0" .. "8"
    ///   appearance.minor_units              "quoted" | "major"
    ///   appearance.code_when_mixed          "1" | "0"
    ///   appearance.fx_stale_minutes         "1" .. "1440"
    ///   appearance.fx_pnl_split             "1" | "0"
    ///   appearance.currency_overrides       compact JSON {"CAD":{"s":"$"},"XYZ":{"s":"X","d":4,"n":"Name"}} | ""
    static QStringList setting_keys();
    /// Current values, keyed by setting_keys().
    QHash<QString, QString> settings_values() const;
    /// Apply stored values. Missing, empty or invalid entries fall back to the
    /// default for that key. Emits changed() once if anything changed.
    void apply_settings_values(const QHash<QString, QString>& values);

    /// Restore every default (follow app locale, Auto, Local, 24h, every
    /// currency setting). The resolved Automatic currency is kept.
    void reset_to_defaults();

    // ── Pure helpers (no state; used by FormatSettingsStore, unit-tested) ────
    /// Overrides as stored: compact JSON, "" when there are none.
    static QString overrides_to_json(const QHash<QString, CurrencyOverride>& overrides);
    /// Parses overrides_to_json() output; invalid JSON gives no overrides and
    /// invalid entries are dropped.
    static QHash<QString, CurrencyOverride> overrides_from_json(const QString& json);
    /// What Automatic should resolve to: the user's chosen region's currency;
    /// else the system locale's currency when the table knows it; else the
    /// inferred region's currency; else "USD".
    static QString resolve_auto_display_currency(bool region_chosen, const QString& region_currency,
                                                 const QLocale& system_locale);
    /// Migration of the pre-4.6 Settings > General choice ("general.currency"):
    /// the code to adopt as the explicit display currency, or "" (Automatic)
    /// when the user had not moved away from the old default (USD) or the
    /// value is unusable.
    static QString migrate_legacy_display_currency(const QString& legacy_general_currency);

  signals:
    void changed();

  private:
    FormatSettings() = default;
    Snapshot state_locked() const;                 // caller holds mutex_
    static Values normalised(Values v);            // validates every field
    QString effective_display_locked() const;      // caller holds mutex_
    /// Replace the stored values; returns whether anything changed. Caller holds mutex_.
    bool assign_locked(const Values& v);
    /// The effective number locale with grouping on, cached (a QLocale copy
    /// with changed options allocates). Caller holds mutex_.
    QLocale grouped_locale_locked() const;

    mutable QMutex mutex_;
    Values v_;
    QString auto_display_ = QStringLiteral("USD");
    mutable QLocale grouped_src_ = QLocale::c();
    mutable QLocale grouped_ = QLocale::c();
    mutable bool grouped_valid_ = false;
};

} // namespace fincept::ui::formatting
