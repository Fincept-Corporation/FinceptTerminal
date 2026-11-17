#pragma once
// NumberFormat — the app's single number / price / money formatter.
//
//   namespace fmt = fincept::ui::formatting;     // a local alias reads well
//   fmt::price(1.08342, {"USD", -1, 0, true})    -> "1.0834"      (FX: pip precision)
//   fmt::price(0.00001234, {"USD"})              -> "0.00001234"  (sub-unit)
//   fmt::price(2845, {"JPY"})                    -> "2,845"
//   fmt::price_with_symbol(351.71, {"USD"})      -> "$351.71"     (a price: never converted)
//   fmt::money(-1234.5, "USD")                   -> "-$1,234.50"
//   fmt::money(12345678, "INR", fmt::Compact::FromSettings) -> "₹1.23 Cr"
//   fmt::money(123.45, "GBp")                    -> "123.45p"
//   fmt::signed_money(1234.5, "USD")             -> "+$1,234.50" (P&L)
//   fmt::converted_money(c) / fmt::fx_note(c)    -> "≈ $13,183.00" / "1 GBP = 1.3183 USD · Nov 16, 2026 14:32"
//   fmt::money_in_column(v, row_ccy, mix)        -> "USD 1,234.50" in a column mixing currencies
//   fmt::header(tr("Value"), "USD")              -> "Value ($)"
//   fmt::axis_money(1.5e6, "USD")                -> "$1.5M"
//   fmt::export_number(1234.5)                   -> "1234.5"      (CSV / clipboard / JSON)
//   fmt::pct(1.234)                              -> "+1.23%"     (input in percent)
//   fmt::fraction_pct(0.1234)                    -> "+12.34%"    (input a fraction)
//
// Every function: locale digits, decimal separator and grouping from
// FormatSettings, and the em-dash placeholder() for NaN / ±inf. Values that
// round to zero print unsigned ("0.00", never "-0.00"). Money follows the
// user's symbol style, position, spacing, negative style, decimals policy,
// minor-unit choice and per-currency overrides; prices, percentages, plain
// numbers and chart axes always use a leading minus.
//
// Rules (see the currency design): prices never convert; aggregates convert
// (in the service that owns them, or via FxRateService::convert_for_display);
// a missing rate is "—", never 1.0; exports are machine-readable with the ISO
// code in its own column; an index level, FX rate or yield is not money
// (is_money_quote()).
//
// Currency lookups (symbols, decimals, pence) live in CurrencyTable.h, which
// this header includes; Money / ConvertedMoney / Scale in Money.h.

#include "ui/formatting/CurrencyTable.h"
#include "ui/formatting/FormatSettings.h"
#include "ui/formatting/Money.h"

#include <QString>
#include <QStringList>

#include <cmath>

namespace fincept::ui::formatting {

/// The missing-value placeholder: "—" (em dash).
QString placeholder();
inline bool is_missing(double v) {
    return !std::isfinite(v);
}

/// What a price is quoted in, so the right number of decimals can be picked.
struct InstrumentFmt {
    QString currency;  ///< quote currency ("USD", "JPY", "GBp"); empty = unknown
    int decimals = -1; ///< fixed decimals; -1 = from `tick`, else from magnitude
    double tick = 0;   ///< minimum price increment; > 0 fixes decimals to the tick's
    bool fx = false;   ///< an FX rate (EURUSD): pip precision, and never a currency symbol
};

/// Builds an InstrumentFmt from a ticker. Recognises FX pairs ("EURUSD=X",
/// "JPY=X", "EUR/USD", "EURUSD") and crypto pairs written with a separator
/// ("BTC-USD", "ETH/USDT": the quote asset is the currency; a share class such
/// as "BRK-B" is untouched), and takes the quote currency when `currency` is empty.
InstrumentFmt instrument_for_symbol(const QString& symbol, const QString& currency = QString());

/// Decimals price() uses. Order: inst.decimals, then tick, then magnitude:
///   FX:      >=1000 -> 2, >=100 -> 3, >=1 -> 4, >=0.01 -> 5, smaller -> 6
///   others:  >=1 -> 2 (3 for KWD-style currencies; JPY-style zero-decimal
///            currencies 0, or 1-2 when the value has a fraction),
///            >=0.01 -> 4, >=0.0001 -> 6, smaller -> 8
int price_decimals(double v, const InstrumentFmt& inst = InstrumentFmt());
/// A price, no currency symbol, always as quoted (pence stay pence): "1,234.50", "0.0345", "1.0834".
QString price(double v, const InstrumentFmt& inst = InstrumentFmt());
/// A price with its currency unit placed per the user's settings: "$1,234.50",
/// "123.45p" ("£1.2345" when minor units are shown in the major unit). An FX
/// rate (inst.fx) gets no unit. Negative prices keep the minus sign.
QString price_with_symbol(double v, const InstrumentFmt& inst);
/// A signed price change in the decimals of `ref_price`: "+1,234.50", "-0.0012".
QString price_change(double change, double ref_price, const InstrumentFmt& inst = InstrumentFmt());

/// A grouped number: "1,234,567.89".
QString number(double v, int decimals = 2);
/// A grouped number with an explicit "+" on positives: "+1,234.50", "-3.00", "0.00".
QString signed_number(double v, int decimals = 2);

// ── Money ────────────────────────────────────────────────────────────────────

/// An amount of money. `scheme` = None (default) prints the full amount;
/// any other value abbreviates (FromSettings and Auto = the user's scheme;
/// when the user left it on Automatic, lakh / crore for INR-family currencies
/// and K/M/B otherwise). `decimals` = -1 uses the user's decimals policy (by
/// default the currency's own: USD 2, JPY 0, KWD 3, crypto up to 8 with
/// trailing zeros trimmed) for full amounts and the compact() defaults when
/// abbreviated; an explicit value always wins. Unit and placement follow the
/// user's symbol style / position / spacing (default: the number locale's
/// pattern, "$1.00" en_US, "1,00 €" de_DE). Minor-unit codes keep their own
/// unit ("123.45p") unless the user shows them in the major unit; abbreviated
/// amounts are always in the major unit ("£15.0K"). An empty code prints no
/// unit; an unknown code is used as the unit ("XYZ 10.00").
QString money(double v, const QString& ccy, Compact scheme = Compact::None, int decimals = -1);
/// money() with "+" on positives, for P&L: "+$1,234.50" / "-$1,234.50" (or "($1,234.50)").
QString signed_money(double v, const QString& ccy, Compact scheme = Compact::None, int decimals = -1);
QString money(const Money& m, Compact scheme = Compact::None, int decimals = -1);
QString signed_money(const Money& m, Compact scheme = Compact::None, int decimals = -1);
/// money() that never prints a non-zero amount as zero ($0.00001234), whatever
/// the decimals policy. For crypto balances, per-unit costs, fees.
QString money_precise(double v, const QString& ccy);

/// A cell of a table column (or any value shown beside values in other
/// currencies). `column_mixed` = the column can hold more than one currency:
/// with the user's "codes in mixed columns" setting on (default) the ISO code
/// is printed ("USD 1,234.50", "GBp 117.70"), otherwise the unambiguous
/// symbol ("US$1,234.50").
QString money_in_column(double v, const QString& ccy, bool column_mixed, Compact scheme = Compact::None,
                        int decimals = -1, bool signed_ = false);
/// money_in_column() knowing the column's currencies: ISO codes when `column`
/// mixes currencies (and the setting is on); otherwise the plain symbol where
/// no other currency of the column shares it ("$" in a CAD-only column).
QString money_in_column(double v, const QString& ccy, const CurrencyMix& column, Compact scheme = Compact::None,
                        int decimals = -1, bool signed_ = false);

/// A converted figure: money(c.amount, c.currency, scheme), prefixed "≈ " when
/// the rate is stale or approximate; placeholder() when !c.ok().
QString converted_money(const ConvertedMoney& c, Compact scheme = Compact::None, bool signed_ = false);
/// Tooltip / footnote for a converted figure: "1 GBP = 1.3183 USD · Nov 16, 2026 14:32"
/// (+ " · via USD", " · stale", or the approximation reason). Empty when
/// !c.converted. The time is the rate's (user's zone and clock).
QString fx_note(const ConvertedMoney& c);

/// Always USD, never converted: model-vendor cost ("$0.0123") ...
QString vendor_cost(double usd, int decimals = 4);
/// ... and Fincept plan price ("$10", "$9.99").
QString plan_price(double usd);

// ── Units and headers ────────────────────────────────────────────────────────

/// FormatSettings::instance().display_currency(): never empty.
QString display_currency();
/// Unit of a scaled column: "$M", "USD M", "₹ Cr", "US$B". Scale::One =
/// currency_unit(ccy). Empty ccy = the display currency; minor units -> major.
QString scaled_unit(const QString& ccy, Scale scale);
/// "Value ($)" / "Value (USD)" / "Value (US$)" per the symbol style. Empty
/// ccy = the display currency. Pass QStringLiteral("-") for a column that
/// mixes currencies (no unit) -> "Value".
QString header(const QString& title, const QString& ccy = {});
/// header() for a column holding the currencies of `column`: no unit when it
/// mixes currencies, else the unit as money_in_column() prints it.
QString header(const QString& title, const CurrencyMix& column);
/// "Deal value ($M)", "Net flow (₹ Cr)".
QString header_scaled(const QString& title, const QString& ccy, Scale scale);

// ── Not money ────────────────────────────────────────────────────────────────

/// False for INDEX and CURRENCY quote types, and (when quote_type is empty) for
/// "^…" and "…=X" symbols. Never put a currency symbol on, or convert,
/// anything this rejects (index levels, FX rates).
bool is_money_quote(const QString& quote_type, const QString& symbol = {});

// ── Chart axes ───────────────────────────────────────────────────────────────

/// Tick label for a money axis: abbreviated per the user's scheme (trailing
/// zeros trimmed; below 1,000 no decimals for whole numbers), always a minus
/// sign (no parentheses), unit per settings: "$1.5M", "-₹2.5 Cr", "£950", "$0".
QString axis_money(double v, const QString& ccy);
/// printf-style format for QValueAxis::setLabelFormat: "$%.0f", "%.2f €",
/// "USD %.0f" ('%' in a unit escaped). printf has no grouping: prefer
/// axis_money() where the chart allows a formatter callback.
QString axis_printf_format(const QString& ccy, int decimals);

// ── Exports ──────────────────────────────────────────────────────────────────

/// Machine-readable: C locale, '.', no grouping, ASCII '-', never an exponent;
/// shortest round-trip up to `max_decimals` (-1 = 15 significant digits).
/// NaN / inf -> empty string.
QString export_number(double v, int max_decimals = -1);
struct ExportMoney {
    QString amount;
    QString currency;
};
/// {export_number(v), code as produced}. Minor-unit prices stay as quoted with their code ("GBp").
ExportMoney export_money(double v, const QString& ccy);
/// Columns for a converted figure, in this order: amount, currency,
/// source_amount, source_currency, fx_rate, fx_time (ISO 8601 UTC).
QStringList export_converted(const ConvertedMoney& c);

// ── Compact numbers, percentages, quantities ─────────────────────────────────

/// An abbreviated number: KMB "1.5K" "12.35M" "1.23B" "4.56T"; LakhCrore
/// "1.5K" "1.50 L" "2.35 Cr" "1.20 L Cr". `ccy` only matters for Auto.
/// `decimals` = -1: 1 for K, 2 for the larger units, and below 1,000 none for
/// whole numbers else 2. A value that rounds up into the next unit moves there
/// (999,950 -> "1.00M"). Compact::None prints the full grouped number.
QString compact(double v, Compact scheme = Compact::FromSettings, const QString& ccy = QString(), int decimals = -1);
/// Resolves FromSettings / Auto to KMB or LakhCrore (None stays None).
Compact resolve_compact(Compact scheme, const QString& ccy = QString());

/// A percentage; the input is already in percent units (1.23 -> "+1.23%").
QString pct(double percent, int decimals = 2, bool signed_ = true);
/// A fraction as a percentage (0.0123 -> "+1.23%").
QString fraction_pct(double fraction, int decimals = 2, bool signed_ = true);
/// Percentage points, for a difference of two percentages: "+0.35 pp".
QString pp(double points, int decimals = 2, bool signed_ = true);
/// Basis points; the input is in bp (25 -> "+25 bp"). From a percent change
/// pass `pct_change * 100`.
QString bp(double basis_points, int decimals = 0, bool signed_ = true);

/// A share / contract quantity: whole numbers without decimals ("1,234"),
/// fractional ones (crypto, fractional shares) with up to `max_decimals`,
/// trailing zeros trimmed ("0.1235", "1.5").
QString quantity(double q, int max_decimals = 4);
/// Traded volume, abbreviated: "12.35M"; below 1,000 as quantity(). 0 -> "0",
/// negative / non-finite -> placeholder().
QString volume(double v, Compact scheme = Compact::FromSettings);
/// A multiple: "12.35x" (P/E, EV/EBITDA, leverage).
QString ratio(double v, int decimals = 2);

/// Legacy (watchlist / markets volume column): KMB with 2 decimals, "--" for
/// <= 0. New code should use volume().
QString format_compact_volume(qint64 volume);

} // namespace fincept::ui::formatting
