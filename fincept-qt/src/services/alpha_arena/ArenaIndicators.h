#pragma once
// Pure indicator helpers for arena prompts. Input candles oldest → newest.
#include "services/alpha_arena/ArenaTypes.h"
namespace fincept::arena {
double ind_ema(const QVector<Candle>& c, int period);   // 0 if c.size() < period
double ind_rsi(const QVector<Candle>& c, int period);   // Wilder; 50 if insufficient
double ind_atr(const QVector<Candle>& c, int period);   // 0 if insufficient
/// Fills ema20/ema50/rsi14/atr14 on the snapshot from candles_1h.
void apply_indicators(CoinSnapshot& s);
} // namespace fincept::arena
