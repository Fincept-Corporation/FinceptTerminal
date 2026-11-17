"""Independent pandas / numpy reference for the backtesting engines (seed: the audit's ref.py).

Nothing here imports base/ or any engine: it is the yardstick they are measured against.

Conventions (the definitions every engine is held to):
  total return      E_end / E_0 - 1
  CAGR              (E_end / E_0) ** (365.25 / calendar_days) - 1      (calendar basis)
  ann. return(bars) (E_end / E_0) ** (ppy / (n-1)) - 1
  volatility        std(simple returns, ddof=1) * sqrt(ppy)
  Sharpe            mean(r - rf/ppy) / std(r, ddof=1) * sqrt(ppy)
  Sortino           mean(r - rf/ppy) * sqrt(ppy) / sqrt(mean(min(r - rf/ppy, 0)^2))
  max drawdown      max_t 1 - E_t / max(E_0..E_t)   (positive fraction); duration = longest run of bars below a peak
  Calmar            CAGR / max drawdown
  beta              cov(r, b) / var(b);  alpha (Jensen, annual) = (mean(r) - rf/ppy - beta*(mean(b) - rf/ppy)) * ppy
  tracking error    std(r - b, ddof=1) * sqrt(ppy); information ratio = mean(r - b) * ppy / TE
  win rate          winners / closed trades;  profit factor = gross profit / gross loss (money, after costs)
  expectancy        mean closed-trade pnl (money)

Simulators (long / flat, one symbol): fills 'next_open' | 'next_close' | 'same_close'; commission =
fraction of notional per fill; slippage = adverse fraction of the fill price; entry sizes
size_frac x cash / (fill price x (1 + commission)); fractional or whole shares; equity marked at
the close. Monthly equal weight: decision on the first (or last) bar of each month, fees paid out
of the portfolio by a 4-step fixed point, fills same close / next close / next open.
"""
import math

import numpy as np
import pandas as pd


# ---------------------------------------------------------------- metrics
def metrics(equity, rf=0.0, ppy=252.0, bench=None):
    e = pd.Series(equity).astype(float)
    r = e.pct_change().dropna()
    n = len(e)
    out = {}
    out['totalReturn'] = e.iloc[-1] / e.iloc[0] - 1
    days = (e.index[-1] - e.index[0]).days if isinstance(e.index, pd.DatetimeIndex) else None
    out['cagr'] = (e.iloc[-1] / e.iloc[0]) ** (365.25 / days) - 1 if days else None
    out['annReturnBars'] = (e.iloc[-1] / e.iloc[0]) ** (ppy / (n - 1)) - 1
    out['volatility'] = r.std(ddof=1) * math.sqrt(ppy)
    ex = r - rf / ppy
    out['sharpe'] = ex.mean() / r.std(ddof=1) * math.sqrt(ppy) if r.std(ddof=1) > 0 else None
    dn = np.sqrt((np.minimum(ex, 0) ** 2).mean())
    out['sortino'] = ex.mean() * ppy / (dn * math.sqrt(ppy)) if dn > 0 else None
    out['downsideDeviation'] = dn * math.sqrt(ppy)
    peak = e.cummax()
    dd = 1 - e / peak
    out['maxDrawdown'] = dd.max()
    longest = cur = 0
    for x in dd.values:
        cur = cur + 1 if x > 1e-12 else 0
        longest = max(longest, cur)
    out['maxDrawdownDurationBars'] = longest
    out['calmar'] = out['cagr'] / out['maxDrawdown'] if out['cagr'] is not None and out['maxDrawdown'] > 0 else None
    out['bestDay'], out['worstDay'] = r.max(), r.min()
    out['var95'] = np.percentile(r.values, 5)
    out['cvar95'] = r[r <= out['var95']].mean()
    if bench is not None:
        b = pd.Series(bench).astype(float).reindex(e.index).ffill().pct_change().dropna()
        rr = r.reindex(b.index).fillna(0.0)
        beta = np.cov(rr, b, ddof=1)[0, 1] / b.var(ddof=1)
        out['beta'] = beta
        out['alpha'] = (rr.mean() - rf / ppy - beta * (b.mean() - rf / ppy)) * ppy
        act = rr - b
        out['trackingError'] = act.std(ddof=1) * math.sqrt(ppy)
        out['informationRatio'] = act.mean() * ppy / out['trackingError'] if out['trackingError'] > 0 else None
        out['correlation'] = np.corrcoef(rr, b)[0, 1]
    return {k: (float(v) if v is not None else None) for k, v in out.items()}


def trade_stats(trades):
    closed = [t for t in trades if t.get('exit') is not None]
    pnl = np.array([t['pnl'] for t in closed], dtype=float)
    w, l = pnl[pnl > 0], pnl[pnl < 0]
    return {
        'closedTrades': len(closed), 'winRate': len(w) / len(pnl) if len(pnl) else None,
        'profitFactor': w.sum() / -l.sum() if len(l) and l.sum() != 0 else None,
        'expectancy': pnl.mean() if len(pnl) else None, 'netPnl': pnl.sum() if len(pnl) else 0.0,
        'avgWin': w.mean() if len(w) else None, 'avgLoss': l.mean() if len(l) else None,
        'totalCommission': float(sum(t.get('fees', 0.0) for t in trades)),
    }


# ---------------------------------------------------------------- indicators
def sma(s, n):
    return s.rolling(n).mean()


def rsi_wilder(close, n=14):
    """Classic Wilder: seed = simple mean of the first n changes, then recursive smoothing."""
    d = close.diff().values
    up, dn = np.clip(d, 0, None), np.clip(-d, 0, None)
    out = np.full(len(d), np.nan)
    if len(d) > n:
        au, ad = up[1:n + 1].mean(), dn[1:n + 1].mean()
        out[n] = 100.0 if ad == 0 else 100 - 100 / (1 + au / ad)
        for i in range(n + 1, len(d)):
            au = (au * (n - 1) + up[i]) / n
            ad = (ad * (n - 1) + dn[i]) / n
            out[i] = 100.0 if ad == 0 else 100 - 100 / (1 + au / ad)
    return pd.Series(out, index=close.index)


def rsi_simple(close, n=14):
    d = close.diff()
    up = d.clip(lower=0).rolling(n).mean()
    dn = (-d.clip(upper=0)).rolling(n).mean()
    return 100 - 100 / (1 + up / dn)


# ---------------------------------------------------------------- single-asset long/flat simulator
def simulate(df, entries, exits, cash0=100000.0, fill='next_open', commission=0.0, slippage=0.0,
             size_frac=1.0, fractional=True, close_at_end=False):
    o, c = df['Open'].values, df['Close'].values
    ent = np.asarray(entries, dtype=bool)
    exi = np.asarray(exits, dtype=bool)
    n = len(df)
    cash, sh = cash0, 0.0
    eq = np.empty(n)
    trades, cur = [], None
    for i in range(n):
        if fill == 'same_close':
            sig_e, sig_x, px = ent[i], exi[i], c[i]
        else:
            sig_e = ent[i - 1] if i > 0 else False
            sig_x = exi[i - 1] if i > 0 else False
            px = o[i] if fill == 'next_open' else c[i]
        if sh > 0 and sig_x:
            fp = px * (1 - slippage)
            fee = sh * fp * commission
            cash += sh * fp - fee
            cur.update(exit=df.index[i], exit_px=fp, fees=cur['fees'] + fee,
                       pnl=sh * (fp - cur['entry_px']) - cur['fees'] - fee, bars=i - cur['i'])
            trades.append(cur)
            cur, sh = None, 0.0
        elif sh == 0 and sig_e and not sig_x:
            fp = px * (1 + slippage)
            budget = cash * size_frac
            q = budget / (fp * (1 + commission))
            if not fractional:
                q = math.floor(q)
            if q > 0:
                fee = q * fp * commission
                cash -= q * fp + fee
                sh = q
                cur = dict(entry=df.index[i], entry_px=fp, qty=q, fees=fee, i=i, exit=None)
        eq[i] = cash + sh * c[i]
    if cur is not None:
        if close_at_end:
            fp = c[-1] * (1 - slippage)
            fee = sh * fp * commission
            cur.update(exit=df.index[-1], exit_px=fp, fees=cur['fees'] + fee,
                       pnl=sh * (fp - cur['entry_px']) - cur['fees'] - fee, bars=n - 1 - cur['i'])
            eq[-1] = cash + sh * fp - fee
        else:
            cur['unrealized'] = sh * (c[-1] - cur['entry_px']) - cur['fees']
        trades.append(cur)
    e = pd.Series(eq, index=df.index)
    return {'equity': e, 'trades': trades, 'cash': cash, 'shares': sh}


def buy_hold(df, **kw):
    ent = pd.Series(False, index=df.index)
    ent.iloc[0] = True
    return simulate(df, ent, pd.Series(False, index=df.index), **kw)


def sma_cross_signals(close, fast=20, slow=50, gate=True):
    """Cross events. gate=True: a cross needs both averages valid on the previous bar too (base.signals);
    gate=False: the first bar with both averages counts as a cross (the audit ref / vectorbt)."""
    f, s = sma(close, fast), sma(close, slow)
    above = (f > s) & s.notna()
    below = (f < s) & s.notna()
    prev_valid = s.notna().shift(1, fill_value=False) if gate else pd.Series(True, index=close.index)
    ent = above & ~above.shift(1, fill_value=False) & prev_valid
    exi = below & ~below.shift(1, fill_value=False) & prev_valid
    return ent, exi


def sma_cross(df, fast=20, slow=50, gate=True, **kw):
    ent, exi = sma_cross_signals(df['Close'], fast, slow, gate)
    return simulate(df, ent, exi, **kw)


def rsi_signals(close, period=14, lo=30, hi=70, kind='wilder', cross=True):
    r = (rsi_wilder if kind == 'wilder' else rsi_simple)(close, period)
    if cross:
        ent = (r < lo) & (r.shift(1) >= lo)
        exi = (r > hi) & (r.shift(1) <= hi)
    else:
        ent, exi = r < lo, r > hi
    return ent.fillna(False), exi.fillna(False)


def rsi_mr(df, period=14, lo=30, hi=70, kind='wilder', cross=True, **kw):
    ent, exi = rsi_signals(df['Close'], period, lo, hi, kind, cross)
    return simulate(df, ent, exi, **kw)


# ---------------------------------------------------------------- monthly equal weight
def ew_monthly(closes, opens=None, cash0=100000.0, commission=0.0, when='first_bar_of_month', fill='same_close'):
    """Equal weight, rebalanced monthly. Decision on the first (or last) bar of each month at its close;
    fill at that close ('same_close'), the next bar's close ('next_close') or the next bar's open
    ('next_open', needs `opens`). Fees = commission x traded notional, paid out of the portfolio."""
    px_c = closes.dropna()
    n, k = px_c.shape
    months = px_c.index.to_period('M')
    if when == 'first_bar_of_month':
        flag = np.r_[True, months[1:] != months[:-1]]
    else:
        flag = np.r_[months[1:] != months[:-1], True]
    if fill in ('next_close', 'next_open'):
        flag = np.r_[False, flag[:-1]]
    fill_px = px_c.values if fill != 'next_open' else opens.reindex(px_c.index).values
    sh = np.zeros(k)
    cash = cash0
    eq = np.empty(n)
    turnover, fees = 0.0, 0.0
    for i in range(n):
        if flag[i]:
            p = fill_px[i]
            val = cash + (sh * p).sum()
            fee = 0.0
            for _ in range(4):
                tgt = (val - fee) / k / p
                fee = np.abs((tgt - sh) * p).sum() * commission
            traded = np.abs((tgt - sh) * p).sum()
            turnover += traded
            fees += fee
            cash = val - fee - (tgt * p).sum()
            sh = tgt
        eq[i] = cash + (sh * px_c.values[i]).sum()
    return {'equity': pd.Series(eq, index=px_c.index), 'turnover': turnover, 'fees': fees}


# ---------------------------------------------------------------- engine output helpers
def engine_equity(data):
    e = data['equity']
    return pd.Series([p['equity'] for p in e], index=pd.to_datetime([str(p['date'])[:10] for p in e]))


def compare_equity(data, ref_eq):
    e = engine_equity(data)
    j = e.index.intersection(ref_eq.index)
    rel = (e.reindex(j) / ref_eq.reindex(j) - 1).abs()
    return dict(n_engine=len(e), n_ref=len(ref_eq), n_common=len(j), max_rel_diff=float(rel.max()),
                first_div=str(rel[rel > 1e-6].index[0].date()) if (rel > 1e-6).any() else None,
                final_engine=float(e.iloc[-1]), final_ref=float(ref_eq.iloc[-1]))


def compare_trades(data, ref_trades):
    a = {(str(t.get('entryDate'))[:10], str(t.get('exitDate'))[:10] if t.get('exitDate') else None)
         for t in data.get('trades', [])}
    b = {(str(t['entry'].date()), str(t['exit'].date()) if t.get('exit') is not None else None) for t in ref_trades}
    return dict(n_engine=len(a), n_ref=len(b), same=len(a & b), only_engine=sorted(a - b)[:5],
                only_ref=sorted(b - a)[:5])
