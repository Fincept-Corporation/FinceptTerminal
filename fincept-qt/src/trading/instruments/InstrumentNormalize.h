#pragma once
// InstrumentNormalize — the single source of truth for the CANONICAL instrument
// format shared by every broker parser ("equalize"). Every broker's master is
// reduced to native fields, then these pure functions synthesise the identical
// canonical `symbol` / `exchange` regardless of which broker produced the row.
//
// Canonical forms (mirrors OpenAlgo):
//   FUT     NAME + DDMMMYY + "FUT"                e.g. NIFTY28MAR24FUT
//   CE/PE   NAME + DDMMMYY + STRIKE + "CE"|"PE"   e.g. NIFTY28MAR2425000CE
//   EQ      trading symbol, -EQ/-BE/-MF/-SG/-SM suffix stripped
//   INDEX   mapped underlying name (alias map → spaces/hyphens stripped)
//   expiry stored "DD-MMM-YY" (upper); in-symbol form is "DDMMMYY".

#include "trading/instruments/InstrumentTypes.h"

#include <QString>

namespace fincept::trading::norm {

/// Parse many broker expiry formats into the compact in-symbol form "28MAR24".
/// Accepts: "2024-03-28", "28MAR2024", "28-MAR-2026", "28/07/26", "ddMMMyy",
/// "yyyy-MM-dd HH:mm:ss". Returns "" if unparseable/empty.
QString expiry_to_nodash(const QString& raw);

/// "28MAR24" → "28-MAR-24". Returns "" unless input is exactly 7 chars.
QString nodash_to_display(const QString& nd);

/// Convenience: raw broker expiry → display "28-MAR-24" ("" if unparseable).
QString expiry_display(const QString& raw);

/// 25000.0 → "25000"; 287.5 → "287.5"; 133.75 → "133.75". Whole numbers drop the
/// decimal; fractional values are trimmed of trailing zeros.
QString format_strike(double strike);

/// Human-friendly expiry for symbol pickers: "07-JUL-26" → "7 Jul 26".
/// Returns the input unchanged if it isn't the stored DD-MMM-YY form.
QString expiry_friendly(const QString& display_expiry);

/// Human-readable picker label so users aren't shown the raw canonical symbol
/// (e.g. NIFTY07JUL2618250CE). Built from the catalog fields:
///   FUT   → "NIFTY 30 Jun 26 FUT"
///   CE/PE → "NIFTY 7 Jul 26 18250 CE"
///   EQ    → the trading symbol (already clean, e.g. RELIANCE)
///   INDEX → the underlying name (e.g. NIFTY)
/// `fallback_symbol` is used for EQ / when fields are missing.
QString display_name(const QString& name, InstrumentType itype, const QString& expiry, double strike,
                     const QString& fallback_symbol);

/// RELIANCE-EQ / -BE / -MF / -SG / -SM → RELIANCE (case-insensitive suffix).
QString strip_eq_suffix(const QString& trading_symbol);

/// Underlying/index name → canonical ("NIFTY 50"→"NIFTY", "NIFTYBANK"→"BANKNIFTY",
/// "SNSX50"→"SENSEX50"). Handles both spaced and pre-stripped inputs.
QString normalise_index_symbol(const QString& raw_name);

/// Build the canonical symbol. `expiry_nd` is the compact "28MAR24" form (""
/// for EQ/INDEX). `eq_or_index_fallback` is the broker trading symbol (EQ) or
/// raw index name (INDEX) used when no synthesis applies.
QString synthesize_symbol(const QString& name, InstrumentType itype, const QString& expiry_nd, double strike,
                          const QString& eq_or_index_fallback);

/// Canonical exchange. Brokers pass an already-broker-mapped exchange (NSE/BSE/
/// NFO/BFO/CDS/BCD/MCX). For INDEX types the "_INDEX" suffix is ensured.
QString normalise_exchange(const QString& exchange, InstrumentType itype);

/// Stable numeric token for brokers whose native token is non-numeric (Upstox
/// "NSE_EQ|INE...", Samco "758960_NSE"). Uses the leading integer when present,
/// else a positive 63-bit FNV-1a hash of the full key. Guarantees a non-zero,
/// per-key-distinct value so UNIQUE(instrument_token, broker_id) never collapses.
qint64 stable_token(const QString& broker_native_token);

} // namespace fincept::trading::norm
