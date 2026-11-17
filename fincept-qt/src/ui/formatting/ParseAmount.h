#pragma once
// ParseAmount — the inverse of money(): reads an amount the user typed, or one the
// formatters printed, under any of the user's number / currency settings.
//
//   namespace fmt = fincept::ui::formatting;
//   const fmt::ParsedAmount a = fmt::parse_amount(edit->text(), "USD");
//   if (!a.ok()) { show(a.error); return; }
//   if (a.currency != "USD") { ...the user typed another currency: convert or refuse... }
//   use(*a.value);
//
// Qt Core only (implemented in NumberFormat.cpp, which it shares the compact
// suffix tables with). Thread-safe like the formatters (one settings snapshot
// per call).

#include <QString>

#include <optional>

namespace fincept::ui::formatting {

/// What parse_amount() read.
struct ParsedAmount {
    /// The amount, in the unit of `currency` (pence for "GBp"); nullopt when the
    /// text is not one amount — `error` says why.
    std::optional<double> value;
    /// Currency of `value`: the one the text named (canonical code: "USD" for "$",
    /// "US$" or "usd"; "GBp" for "117.70p"), else `default_ccy`; empty when neither.
    /// A named currency with the same major currency as `default_ccy` (pounds typed
    /// into a pence field, "£1.18" for "GBp") is converted to `default_ccy`'s unit
    /// (118 GBp). A different one is returned as named: the caller converts or
    /// refuses it.
    QString currency;
    /// The text itself named a currency (a symbol or an ISO code).
    bool currency_in_text = false;
    /// Why `value` is empty: one plain, translated sentence. Empty on success.
    QString error;

    bool ok() const { return value.has_value(); }
};

/// Reads one amount:
///  * digits of any script; the number locale's decimal point and grouping, and
///    the other common forms ("1,234.56", "1.234,56", "1 234,56", "1'234.56",
///    Indian "12,34,567.89", ungrouped). With only one separator, the locale's
///    decimal point is the decimal point; the locale's group separator groups only
///    before exactly three digits; a separator the locale uses for neither is a
///    decimal point unless exactly three digits follow (then refused as ambiguous).
///  * a currency symbol or ISO code before or after the number, glued or spaced:
///    "$1,234.50", "1.234,56 €", "USD 1,234.50", "CHF 5", "US$5", "C$5", "JP¥5",
///    the user's own override symbols and custom codes. A symbol several
///    currencies share ("$", "kr", "¥", "p") is read as `default_ccy`, else as the
///    display currency, else as the currency whose own symbol it is; otherwise
///    refused as ambiguous.
///  * minor units: "117.70p" = 117.70 GBp, "45¢" = 45 USX, "735c" = 735 ZAc.
///  * size suffixes K / M / B / T and L / Cr / L Cr (also their translations and
///    "k", "mn", "bn", "lakh", "crore", ...): "1.5K" = 1,500, "2.35 Cr" = 23,500,000.
///  * negatives "-", "−" (U+2212), "(…)", a trailing minus ("1,234.50-"); a "+".
///  * no-break / narrow / thin spaces, bidi marks, a leading "≈" or "~".
/// Refused with a reason: empty text, "—", a percentage, two numbers, two
/// currencies, two signs, unknown words, separators that cannot be read one way.
/// Round trip: for every setting, parse_amount(money(v, ccy), ccy) gives ccy and v
/// as printed (money() of the result prints the same text).
ParsedAmount parse_amount(const QString& text, const QString& default_ccy = {});

} // namespace fincept::ui::formatting
