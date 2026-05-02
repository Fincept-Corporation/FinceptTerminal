#pragma once
// Kraken symbol + interval mapping.
//
// Fincept symbols are CCXT-style (e.g. "BTC/USDT"). Kraken WS v2 uses the same
// slash format ("BTC/USD", "BTC/USDT") but most volume lives on /USD pairs.
// This header centralises the mapping so the WS client and book code can stay
// dumb about quote-currency politics.

#include <QString>
#include <QStringList>

namespace fincept::trading::kraken {

/// Convert a Fincept symbol to Kraken's WS v2 form.
///
/// Strategy:
///   1. If the symbol already exists on Kraken (we don't validate here — the
///      subscribe ack will tell us), pass it through.
///   2. Caller is responsible for trying /USDT first then falling back to /USD
///      via `usd_fallback()` if the subscribe ack returns success=false.
///
/// We return a verbatim copy for now; the fallback is the real work.
QString to_kraken(const QString& fincept_symbol);

/// Convert a Kraken WS v2 symbol back to the Fincept form the rest of the
/// terminal expects. Mirrors the Python ws_stream.py symbol_map message —
/// but we keep it stateless: the caller passes the original requested
/// symbol along with the resolved one and we store the inverse map.
QString from_kraken(const QString& kraken_symbol, const QString& original_request);

/// Build a /USD-quoted variant of a /USDT-quoted symbol. Returns empty when
/// the input is not /USDT-quoted (caller should not retry in that case).
///
/// Examples:
///   "BTC/USDT"        → "BTC/USD"
///   "ETH/USDT:USDT"   → "ETH/USD"     (perp settle suffix dropped)
///   "BTC/USD"         → ""            (already USD)
///   "WIF/USDC"        → ""            (USDC fallback not handled here)
QString usd_fallback(const QString& fincept_symbol);

/// Convert a Fincept timeframe string ("1m", "5m", "1h", "1d") to the
/// Kraken WS v2 ohlc `interval` integer (minutes). Returns 0 for unknown
/// timeframes — caller should treat 0 as "skip OHLC subscription".
///
/// Kraken accepts: 1, 5, 15, 30, 60, 240, 1440, 10080, 21600.
int interval_minutes(const QString& timeframe);

/// Inverse of interval_minutes — used when we need to forward a Kraken
/// `interval_begin` candle update back upstream tagged with the timeframe
/// string our publisher topics expect.
QString timeframe_string(int kraken_interval);

} // namespace fincept::trading::kraken
