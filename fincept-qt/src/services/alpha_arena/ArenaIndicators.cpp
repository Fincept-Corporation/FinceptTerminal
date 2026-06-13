#include "services/alpha_arena/ArenaIndicators.h"
#include <cmath>
namespace fincept::arena {

double ind_ema(const QVector<Candle>& c, int period) {
    if (c.size() < period || period <= 0) return 0;
    double ema = 0;
    for (int i = 0; i < period; ++i) ema += c[i].c;
    ema /= period;
    const double k = 2.0 / (period + 1);
    for (int i = period; i < c.size(); ++i) ema = c[i].c * k + ema * (1 - k);
    return ema;
}

double ind_rsi(const QVector<Candle>& c, int period) {
    if (c.size() <= period) return 50;
    double gain = 0, loss = 0;
    for (int i = 1; i <= period; ++i) {
        const double d = c[i].c - c[i - 1].c;
        if (d >= 0) gain += d; else loss -= d;
    }
    double ag = gain / period, al = loss / period;
    for (int i = period + 1; i < c.size(); ++i) {
        const double d = c[i].c - c[i - 1].c;
        ag = (ag * (period - 1) + (d > 0 ? d : 0)) / period;
        al = (al * (period - 1) + (d < 0 ? -d : 0)) / period;
    }
    if (al <= 1e-12 && ag <= 1e-12) return 50;
    if (al <= 1e-12) return 100;
    return 100.0 - 100.0 / (1.0 + ag / al);
}

double ind_atr(const QVector<Candle>& c, int period) {
    if (c.size() <= period) return 0;
    double atr = 0;
    for (int i = 1; i <= period; ++i)
        atr += std::max({c[i].h - c[i].l, std::fabs(c[i].h - c[i - 1].c), std::fabs(c[i].l - c[i - 1].c)});
    atr /= period;
    for (int i = period + 1; i < c.size(); ++i) {
        const double tr = std::max({c[i].h - c[i].l, std::fabs(c[i].h - c[i - 1].c), std::fabs(c[i].l - c[i - 1].c)});
        atr = (atr * (period - 1) + tr) / period;
    }
    return atr;
}

void apply_indicators(CoinSnapshot& s) {
    s.ema20 = ind_ema(s.candles_1h, 20);
    s.ema50 = ind_ema(s.candles_1h, 50);
    s.rsi14 = ind_rsi(s.candles_1h, 14);
    s.atr14 = ind_atr(s.candles_1h, 14);
}

} // namespace fincept::arena
