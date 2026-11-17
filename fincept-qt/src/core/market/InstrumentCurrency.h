#pragma once
// InstrumentCurrency — an instrument's trading currency inferred from its
// (Yahoo-style) symbol when the quote does not carry one, and minor-unit
// handling. Qt Core only; safe from any thread.
//
// Minor units: London quotes most shares in pence (GBp / GBX), Johannesburg in
// cents (ZAc), Tel Aviv in agorot (ILA), Kuwait in fils (KWF), US grain futures
// in cents (USX). Reading those as the major unit is a 100x (1000x) error.

#include <QString>

namespace fincept::core::market {

/// Trading currency implied by a symbol, as quoted (minor units kept, e.g. "GBp"
/// for "VOD.L"). Empty when it cannot be told: indices ("^GSPC"), futures
/// ("GC=F", several quote in cents), unknown suffixes, multi-currency books
/// (".IL"). Rules, in order:
///   "EURUSD=X" -> quote currency "USD" ("JPY=X" = USD/JPY -> "JPY")
///   suffix     -> listing venue's currency (".NS" INR, ".L" GBp, ".JO" ZAc, ...)
///   "BTC-USD"  -> quote currency; a stablecoin quote is its own currency
///                 ("SOL-USDT" -> "USDT", converted at USDT-USD, not taken as USD)
///   bare       -> `exchange_hint` venue's currency, else "USD" (US listing)
QString currency_for_symbol(const QString& symbol, const QString& exchange_hint = {});

/// How many quoted units make one major unit: 100 for GBp/GBX, ZAc/ZAC, ILA,
/// USX/USc; 1000 for KWF; 1 otherwise (including unknown / empty codes).
/// Kept in step with ui/formatting/CurrencyTable (core cannot include ui);
/// tests/tst_market_sessions.cpp cross-checks the two.
double minor_unit_divisor(const QString& ccy);

/// True for the minor-unit codes above.
bool is_minor_unit(const QString& ccy);

/// ISO 4217 major currency of a code: "GBp" -> "GBP", "ZAc" -> "ZAR", "ILA" ->
/// "ILS", "KWF" -> "KWD", "USX" -> "USD"; other codes upper-cased and trimmed.
QString major_currency(const QString& ccy);

/// `amount` quoted in `ccy` expressed in the major unit (pence -> pounds).
double to_major_units(double amount, const QString& ccy);

} // namespace fincept::core::market
