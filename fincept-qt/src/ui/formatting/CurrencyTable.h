#pragma once
// The one authoritative currency table: ISO 4217 code -> display symbol,
// decimals, name, plus the minor-unit quote currencies exchanges report
// (GBp / GBX pence, ZAc cents, ILA agorot, KWF fils, USX / USc US cents),
// precious metals and the crypto / stablecoin codes crypto venues quote in.
//
// Lookup is exact first (so "GBp" pence is not "GBP" pounds), then
// case-insensitive ("usd" -> USD, "gbx" -> GBX).
//
// Every code has three printed forms:
//   symbol  the context-free default, never ambiguous across the table:
//           "$" (USD), "C$", "A$", "¥" (JPY), "CN¥", "£", "€", "₹", "SEK"
//   plain   the local symbol: "$" for every dollar, "¥" for yen and yuan, "kr"
//           for krona / krone — used where the view shows no other currency
//           that shares it
//   strict  unambiguous everywhere: "US$", "C$", "JP¥", "CN¥", "SEK"
// currency_unit() picks the right one for the user's SymbolStyle, the display
// currency and (optionally) the other currencies a view shows (CurrencyMix).

#include "ui/formatting/FormatSettings.h"

#include <QString>
#include <QStringList>

namespace fincept::ui::formatting {

struct CurrencySpec {
    QString code;          ///< canonical code as listed, e.g. "USD", "GBp"
    QString symbol;        ///< context-free display symbol, e.g. "$", "€", "C$", "CHF", "p"
    QString name;          ///< translated name, e.g. "US Dollar"
    int decimals = 2;      ///< display decimals for amounts (JPY 0, KWD 3)
    int minor_divisor = 1; ///< units per major unit for minor-unit codes (GBp 100), else 1
    QString major;         ///< major currency (GBp -> GBP); == code for ordinary currencies
    bool suffix = false;   ///< symbol goes after the number ("123.45p"), whatever the locale
    QString plain;         ///< local symbol ("$" for CAD); == symbol when the table has none
    QString strict;        ///< unambiguous symbol ("US$", "JP¥"); == symbol when the table has none
    bool crypto = false;     ///< a crypto asset or stablecoin (BTC, ETH, USDT)
    bool stablecoin = false; ///< a USD stablecoin (USDT, USDC, DAI, ...)
    bool metal = false;      ///< XAU / XAG / XPT / XPD (troy ounce)
    bool custom = false;     ///< not in the table: a user-defined code (see effective_currency())
};

/// Table lookup (no user overrides); returns false (and leaves *out untouched)
/// for an unknown code.
bool find_currency(const QString& code, CurrencySpec* out);
bool is_known_currency(const QString& code);

/// Every code in the table (canonical spelling, no aliases), in table order:
/// fiat, minor units, metals, crypto.
QStringList known_currency_codes();

/// "$", "€", "₹", "CHF", "p" … Unknown codes return the code itself
/// (upper-cased, trimmed); an empty code returns an empty string. Prefer
/// money() / price_with_symbol() over gluing this to a number yourself: they
/// place the symbol per locale and handle suffix symbols such as pence.
/// Table only: no user overrides, no symbol style.
QString currency_symbol(const QString& code);
/// The table's form for `style`: Symbol -> the context-free symbol ("C$"),
/// Code -> the canonical code ("CAD", "GBp"), Disambiguated -> the strict
/// symbol ("US$", "JP¥"). No user overrides. Unknown codes -> upper-cased code.
QString currency_symbol(const QString& code, SymbolStyle style);
/// Display decimals for amounts in `code`: the user's override, else the
/// table's, else 2.
int currency_decimals(const QString& code);
/// Translated currency name (a custom code's own name); the code itself when unknown.
QString currency_name(const QString& code);

bool is_crypto_currency(const QString& code);
bool is_stablecoin(const QString& code);
bool is_minor_unit(const QString& code);

/// 100 for GBp/GBX, ZAc/ZAC, ILA, USX/USc; 1000 for KWF; 1 otherwise.
int minor_unit_divisor(const QString& code);
/// GBp/GBX -> GBP, ZAc -> ZAR, ILA -> ILS, KWF -> KWD, USX -> USD; otherwise
/// the code upper-cased (unknown codes pass through).
QString major_currency(const QString& code);
/// Converts an amount quoted in `code` to its major currency: 12345 GBp -> 123.45.
double to_major_units(double amount, const QString& code);

/// The table row with the user's override applied (symbol, decimals, name).
/// A custom code the user defined gets custom = true. False for an unknown
/// code with no override (*out untouched).
bool effective_currency(const QString& code, CurrencySpec* out);
/// effective_currency() against a settings snapshot the caller already holds.
bool effective_currency(const QString& code, const FormatSettings::Snapshot& s, CurrencySpec* out);

namespace currency_table_detail {
/// Internal fast path for the formatters: effective_currency() without the
/// translated name (spec.name is empty for table codes).
bool effective_currency_unnamed(const QString& code, const FormatSettings::Snapshot& s, CurrencySpec* out);
} // namespace currency_table_detail

/// The currencies one view (a table column, a panel, a chart) shows side by
/// side. Pass it to currency_unit() / money_in_column() / header() so a symbol
/// several of them share is disambiguated ("US$" next to "C$") while a symbol
/// only one of them uses stays plain ("$" in a CAD-only view).
class CurrencyMix {
  public:
    CurrencyMix() = default;
    explicit CurrencyMix(const QStringList& codes);
    /// Adds a currency as produced ("GBp" counts as GBP); empty codes are ignored.
    void add(const QString& code);
    /// Distinct major currencies, in the order first seen.
    const QStringList& majors() const { return majors_; }
    bool isEmpty() const { return majors_.isEmpty(); }
    /// More than one major currency.
    bool mixed() const { return majors_.size() > 1; }
    /// The only major currency when !mixed(); empty otherwise.
    QString single() const { return majors_.size() == 1 ? majors_.first() : QString(); }

  private:
    QStringList majors_;
};

/// The unit money() prints for `code` under the current settings: the user's
/// override symbol, else by SymbolStyle — Code: the canonical code; Strict:
/// the strict symbol; Symbol: the display currency gets its plain symbol, any
/// other currency its context-free symbol (or the strict one when that would
/// read like the display currency's: "US$" for a user whose currency is CAD).
/// Empty `code` = the display currency. Never empty for a non-empty code.
QString currency_unit(const QString& code = {});
/// currency_unit() for a view showing the currencies in `mix` (Symbol style:
/// the plain symbol unless another currency of the mix shares it).
QString currency_unit(const QString& code, const CurrencyMix& mix);
/// currency_unit() against a settings snapshot the caller already holds
/// (formatters that print many cells take one snapshot). `mix` may be null.
QString currency_unit(const QString& code, const FormatSettings::Snapshot& s, const CurrencyMix* mix);

} // namespace fincept::ui::formatting
