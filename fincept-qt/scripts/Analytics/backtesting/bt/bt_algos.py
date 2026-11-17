"""
The bt library's part of the "bt" engine: its algo stack DECIDES the target weights of the
allocation strategies, and its Backtest EXECUTES them when the run asks for bt's native fill
(fillTiming next_close). Imported only for allocation strategies (bt + ffn + scikit-learn +
matplotlib cost ~2 s to import).

DECISION PASS (decide)
  A bt.Strategy is built from the strategy's stack (bt_strategies.allocation_stack): bt's own
  schedule algo (RunDaily / RunWeekly / RunMonthly / RunQuarterly / RunYearly / RunEveryNPeriods /
  RunOnce), bt's SelectAll / SelectN / WeighEqually, and the bar-lookback algos below (bt.Algo
  subclasses that call ffn's weighting functions). The engine drives it bar by bar the way
  bt.Backtest does (setup on the price frame with bt's t0-1 seed row, update, run) but with a
  Recorder as the last algo instead of Rebalance: the schedule algo is evaluated on every bar,
  the rest of the stack on the bars where it fires. A decision taken on bar t uses bars <= t
  (lag 0); base.simulator / bt's Backtest fill it at bar t+1 (or at t's close for same_close).
  The weights recorded are complete rows: a symbol the stack did not weight gets 0.

BAR LOOKBACKS (audit T3: v1 sliced pd.DateOffset(days=n), ~0.7 x n trading days)
  The window of a decision on bar t is the last `lookback` bar returns (prices t-lookback .. t).
  Returns are taken only on bars where the symbol really traded (a closed market's carried-forward
  price would add zero returns - mixed calendars); a symbol needs at least half of the window
  (and 3 returns) to be weighted, otherwise it gets 0 until its history is long enough. Covariance
  estimators use the bars on which every weighted symbol traded.

NATIVE EXECUTION (native_execute; fillTiming next_close on one trading calendar)
  bt.Backtest(Strategy([WeighTarget(decided weights on their fill bars), Rebalance()])) - bt's own
  accounting: target value = weight x portfolio value, positions closed when their weight is 0,
  whole shares (integer_positions) unless fractional shares were requested, fills at the bar's
  close. Costs go through bt's CostModel hook: commission of the cost model on the slipped price
  + |q| x close x slippage fraction of that bar and symbol; the engine itemises the two parts
  again from bt's position changes (fills at close x (1 +- slippage)), which is the same cash.
"""

from __future__ import annotations

import math
import types
from typing import Any, Dict, List, Optional, Tuple

import numpy as np

from bt_data import import_bt_library, library_error, library_version

_bt = import_bt_library()
if _bt is None:   # the engine checks library_installed() before importing this module
    raise ImportError(f'the bt library is not installed ({library_error()})')
Algo = _bt.core.Algo


class BarFrame:
    """The price panel as bt sees it + positional lookups for the bar-lookback algos."""

    def __init__(self, dates, symbols, close, tradable, ppy: float):
        import pandas as pd
        self.dates = pd.DatetimeIndex(dates)
        self.symbols = list(symbols)
        self.close = np.asarray(close, dtype=float)
        self.tradable = np.asarray(tradable, dtype=bool) & np.isfinite(self.close)
        self.ppy = float(ppy)
        n, k = self.close.shape
        r = np.full((n, k), np.nan)
        with np.errstate(divide='ignore', invalid='ignore'):
            r[1:] = self.close[1:] / self.close[:-1] - 1.0
        self.ret = np.where(self.tradable & np.isfinite(r), r, np.nan)
        self.col = {s: j for j, s in enumerate(self.symbols)}
        self.row_of = {t: i for i, t in enumerate(self.dates)}
        seed = pd.DataFrame(np.nan, columns=self.symbols, index=[self.dates[0] - pd.DateOffset(days=1)])
        self.bt_frame = pd.concat([seed, pd.DataFrame(self.close, index=self.dates, columns=self.symbols)])
        # Parameter-free parts of every stack, computed by bt's own algos once per frame and reused by all
        # parameter sets of an optimisation: the bars each schedule fires on, SelectAll's selection per date.
        self.schedule_cache: Dict[str, List[Any]] = {}
        self.select_all_cache: Dict[Any, List[str]] = {}

    def window(self, i: int, cols: List[int], lookback: int) -> Optional[Tuple[np.ndarray, List[int]]]:
        """(returns window (lookback, m), eligible column ids) for a decision on bar i, or None."""
        if i < lookback:
            return None
        R = self.ret[i - lookback + 1:i + 1][:, cols]
        need = max(3, int(math.ceil(0.5 * lookback)))
        ok = [c for c, cnt in zip(cols, np.isfinite(R).sum(axis=0)) if cnt >= need]
        if not ok:
            return None
        return self.ret[i - lookback + 1:i + 1][:, ok], ok


class _BarAlgo(Algo):
    def __init__(self, frame: BarFrame, lookback: int):
        super().__init__()
        self.f = frame
        self.lookback = int(lookback)

    def _row(self, target) -> int:
        return self.f.row_of[target.now]

    def _cols(self, names) -> List[int]:
        return [self.f.col[s] for s in names if s in self.f.col]


class StatTotalReturnBars(_BarAlgo):
    """temp['stat'] = total return of each selected symbol over the last `lookback` bars."""

    def __call__(self, target):
        import pandas as pd
        i = self._row(target)
        if i < self.lookback:
            return False
        sel = [s for s in target.temp.get('selected', []) if s in self.f.col]
        cols = [self.f.col[s] for s in sel]
        a = self.f.close[i - self.lookback, cols]
        b = self.f.close[i, cols]
        ok = np.isfinite(a) & np.isfinite(b) & (a > 0)
        with np.errstate(divide='ignore', invalid='ignore'):
            r = b / a - 1.0
        target.temp['stat'] = pd.Series(r[ok], index=pd.Index([s for s, m in zip(sel, ok) if m], dtype=object),
                                        dtype=float)
        return True


class AbsoluteMomentumBars(_BarAlgo):
    """Keep only selected symbols whose return over `lookback` bars exceeds `threshold` (else cash)."""

    def __init__(self, frame, lookback, threshold=0.0):
        super().__init__(frame, lookback)
        self.threshold = float(threshold)

    def __call__(self, target):
        i = self._row(target)
        if i < self.lookback:
            return False
        c = self.f.close
        keep = []
        for s in target.temp.get('selected', []):
            j = self.f.col[s]
            a, b = c[i - self.lookback, j], c[i, j]
            if np.isfinite(a) and np.isfinite(b) and a > 0 and b / a - 1.0 > self.threshold:
                keep.append(s)
        target.temp['selected'] = keep
        return True


class _WeighBars(_BarAlgo):
    """Common path: 0 / 1 selected handled like bt's algos; else weights from the window."""

    def __call__(self, target):
        sel = list(target.temp.get('selected', []))
        if len(sel) == 0:
            target.temp['weights'] = {}
            return True
        i = self._row(target)
        win = self.f.window(i, self._cols(sel), self.lookback)
        if win is None:
            return False
        R, ok = win
        names = [self.f.symbols[j] for j in ok]
        if len(names) == 1:
            target.temp['weights'] = {names[0]: 1.0}
            return True
        w = self.weights(R, names)
        if w is None:
            return False
        target.temp['weights'] = {s: float(x) for s, x in w.items() if np.isfinite(x)}
        return True

    def weights(self, R: np.ndarray, names: List[str]):
        raise NotImplementedError

    @staticmethod
    def _complete_rows(R, names):
        import pandas as pd
        df = pd.DataFrame(R, columns=names).dropna()
        return df if len(df) >= max(5, len(names) + 1) else None


class WeighInvVolBars(_WeighBars):
    """bt WeighInvVol on bar windows: weight proportional to 1 / std(returns, ddof=1)."""

    def weights(self, R, names):
        import pandas as pd
        with np.errstate(divide='ignore', invalid='ignore'):
            sd = np.nanstd(R, axis=0, ddof=1)
            inv = np.where(sd > 0, 1.0 / sd, np.nan)
        if not np.isfinite(inv).any():
            return None
        inv = np.where(np.isfinite(inv), inv, 0.0)
        return pd.Series(inv / inv.sum(), index=names)


class WeighERCBars(_WeighBars):
    """bt WeighERC on bar windows: ffn.calc_erc_weights (Ledoit-Wolf covariance, cyclical coordinate descent)."""

    def weights(self, R, names):
        df = self._complete_rows(R, names)
        if df is None:
            return None
        return _bt.ffn.calc_erc_weights(df, covar_method='ledoit-wolf', risk_parity_method='ccd',
                                        maximum_iterations=100, tolerance=1e-8)


class WeighMeanVarBars(_WeighBars):
    """bt WeighMeanVar on bar windows: ffn.calc_mean_var_weights - maximum Sharpe (tangency) weights,
    long only (bounds 0..1), Ledoit-Wolf covariance, rf 0 (bt's default)."""

    def weights(self, R, names):
        df = self._complete_rows(R, names)
        if df is None:
            return None
        return _bt.ffn.calc_mean_var_weights(df, weight_bounds=(0.0, 1.0), rf=0.0, covar_method='ledoit-wolf')


class WeighMinVarBars(_WeighBars):
    """Long-only minimum-variance weights: min w' S w subject to sum w = 1, 0 <= w <= 1 (SLSQP), S = the
    Ledoit-Wolf covariance of the window - no expected-return estimate. (bt / ffn have no minimum-
    variance weighting; v1 ran WeighMeanVar under this name - audit T0.2.)"""

    def weights(self, R, names):
        import pandas as pd
        import sklearn.covariance
        from scipy.optimize import minimize
        df = self._complete_rows(R, names)
        if df is None:
            return None
        S = sklearn.covariance.ledoit_wolf(df.to_numpy())[0]
        m = len(names)
        w0 = np.full(m, 1.0 / m)
        res = minimize(lambda w: float(w @ S @ w), w0, jac=lambda w: 2.0 * S @ w, method='SLSQP',
                       bounds=[(0.0, 1.0)] * m, constraints=({'type': 'eq', 'fun': lambda w: w.sum() - 1.0},),
                       options={'maxiter': 200, 'ftol': 1e-12})
        w = np.clip(res.x if res.success else w0, 0.0, 1.0)
        tot = w.sum()
        return pd.Series(w / tot if tot > 0 else w0, index=names)


class TargetVolBars(_BarAlgo):
    """bt TargetVol on bar windows: scale temp['weights'] by target / (sqrt(w' S w x bars per year)) with the
    standard covariance S of the last `lookback` bars; the total is then capped at `cap` (1 = no leverage,
    the rest stays in cash)."""

    def __init__(self, frame, target, lookback, cap=1.0):
        super().__init__(frame, lookback)
        self.target = float(target)
        self.cap = float(cap)

    def __call__(self, target):
        import pandas as pd
        w = target.temp.get('weights')
        if w is None or len(w) == 0:
            return True
        w = dict(w)
        names = list(w)
        i = self._row(target)
        win = self.f.window(i, self._cols(names), self.lookback)
        if win is None:
            return False
        R, ok = win
        keep = [self.f.symbols[j] for j in ok]
        df = pd.DataFrame(R, columns=keep).dropna()
        if len(df) < max(5, len(keep) + 1):
            return False
        wv = np.array([w[s] for s in keep], dtype=float)
        if wv.sum() > 0:
            wv = wv / wv.sum() * sum(w.values())
        cov = df.cov().to_numpy()
        vol = float(np.sqrt(max(0.0, wv @ cov @ wv) * self.f.ppy))
        if not np.isfinite(vol) or vol <= 0:
            return False
        scaled = wv * (self.target / vol)
        tot = float(np.abs(scaled).sum())
        if tot > self.cap > 0:
            scaled = scaled * (self.cap / tot)
        target.temp['weights'] = {s: float(x) for s, x in zip(keep, scaled)}
        return True


class CachedSelectAll(Algo):
    """bt's SelectAll, memoised per date on the frame (its result depends only on that date's prices, so an
    optimisation calls bt's algo once per date instead of once per date and parameter set)."""

    def __init__(self, frame: BarFrame):
        super().__init__()
        self.f = frame
        self.inner = _bt.algos.SelectAll()

    def __call__(self, target):
        hit = self.f.select_all_cache.get(target.now)
        if hit is None:
            ok = self.inner(target)
            if not ok:
                return False
            hit = list(target.temp.get('selected', []))
            self.f.select_all_cache[target.now] = hit
        target.temp['selected'] = list(hit)
        return True


class Recorder(Algo):
    """Last algo of the decision stack: stores the complete weight row of the decision bar."""

    def __init__(self, frame: BarFrame):
        super().__init__()
        self.f = frame
        self.rows: Dict[int, np.ndarray] = {}

    def __call__(self, target):
        w = target.temp.get('weights')
        row = np.zeros(len(self.f.symbols))
        if w is not None:
            for s, x in dict(w).items():
                j = self.f.col.get(s)
                if j is not None and x is not None and np.isfinite(float(x)):
                    row[j] = float(x)
        self.rows[self.f.row_of[target.now]] = row
        return True


def schedule_algo(schedule):
    a = _bt.algos
    if schedule == 'once':
        return a.RunOnce()
    if isinstance(schedule, int):
        return a.RunEveryNPeriods(int(schedule))
    return {'daily': a.RunDaily, 'weekly': a.RunWeekly, 'monthly': a.RunMonthly, 'quarterly': a.RunQuarterly,
            'yearly': a.RunYearly}[schedule]()


def build_algo(name: str, kw: Dict[str, Any], frame: BarFrame):
    a = _bt.algos
    if name == 'SelectAll':
        return CachedSelectAll(frame)
    if name == 'WeighEqually':
        return a.WeighEqually()
    if name == 'SelectN':
        return a.SelectN(n=int(kw['n']))
    if name == 'TargetVolBars':
        return TargetVolBars(frame, kw['target'], kw['lookback'])
    if name == 'AbsoluteMomentumBars':
        return AbsoluteMomentumBars(frame, kw['lookback'], kw.get('threshold', 0.0))
    cls = {'WeighInvVolBars': WeighInvVolBars, 'WeighERCBars': WeighERCBars, 'WeighMeanVarBars': WeighMeanVarBars,
           'WeighMinVarBars': WeighMinVarBars, 'StatTotalReturnBars': StatTotalReturnBars}[name]
    return cls(frame, kw['lookback'])


def decide(stack: Dict[str, Any], frame: BarFrame, cancel=None) -> Tuple[np.ndarray, List[int]]:
    """Run the bt decision pass. Returns (W (n, k): the decided weight row on each decision bar, NaN
    elsewhere; the decision bars)."""
    rec = Recorder(frame)
    algos = [build_algo(name, kw, frame) for name, kw in stack['steps']] + [rec]
    strat = _bt.Strategy('fincept_decide', algos)
    d = frame.bt_frame
    strat.setup(d)
    strat.adjust(1_000_000.0)
    strat.update(d.index[0])
    key = repr(stack['schedule'])
    fired = frame.schedule_cache.get(key)
    if fired is None:
        # bt's schedule algo, called on every bar exactly as bt.Backtest calls the stack (it depends only on
        # the dates / the call count, so the bars it fires on are kept for every parameter set)
        sched = schedule_algo(stack['schedule'])
        stub = types.SimpleNamespace(now=None, data=d, temp={})
        fired = []
        for dt in d.index[1:]:
            stub.now = dt
            if sched(stub):
                fired.append(dt)
        frame.schedule_cache[key] = fired
    for t, dt in enumerate(fired):
        if cancel is not None and (t & 63) == 0:
            cancel.check()
        strat.update(dt)
        strat.run()
    n, k = frame.close.shape
    W = np.full((n, k), np.nan)
    rows = sorted(rec.rows)
    for i in rows:
        W[i] = rec.rows[i]
    return W, rows


NATIVE_OK = hasattr(_bt.core, 'CostModel')   # bt's per-bar cost hook (bt 1.4); older bt -> base.simulator executes


class _CostHook(_bt.core.CostModel if NATIVE_OK else object):
    """bt's per-trade cost hook: the run's commission on the slipped price + the slippage money
    (`sigma` carries the bar's slippage fraction of that symbol)."""

    def __init__(self, cost_model):
        self.cm = cost_model

    def cost(self, q, p, V, sigma):
        aq = abs(float(q))
        if aq == 0.0:
            return 0.0
        s = float(sigma)
        fp = p * (1.0 + s) if q > 0 else p * (1.0 - s)
        return float(self.cm.commission(aq, fp)) + aq * p * s


def native_execute(panel, W: np.ndarray, cost_model, *, fractional: bool, initial_capital: float):
    """Execute decided weights (decision bar t) at bar t+1's CLOSE in bt's own Backtest. Returns a
    base.simulator.SimResult (backend 'bt') so the result is built exactly like a simulator run."""
    import pandas as pd
    from base.simulator import SimResult
    n, k = panel.close.shape
    syms = list(panel.symbols)
    close = pd.DataFrame(panel.close, index=panel.dates, columns=syms)
    rows = [i for i in range(n - 1) if np.isfinite(W[i]).any()]
    fill_rows = [i + 1 for i in rows]
    tw = np.array([W[i] for i in rows]) if rows else np.zeros((0, k))
    if len(rows):
        listed = np.isfinite(panel.close[fill_rows])
        tw = np.where(listed, np.nan_to_num(tw), np.nan)
    weights = pd.DataFrame(tw, index=panel.dates[fill_rows], columns=syms)
    slip = cost_model.slippage_fractions(panel.close, panel.volume)
    slip = np.where(np.isfinite(slip), slip, 0.0)
    vol_df = pd.DataFrame(np.ones((n, k)), index=panel.dates, columns=syms)
    slip_df = pd.DataFrame(slip, index=panel.dates, columns=syms)
    strat = _bt.Strategy('fincept_native', [_bt.algos.WeighTarget(weights), _bt.algos.Rebalance()])
    bk = _bt.Backtest(strat, close, initial_capital=float(initial_capital), commissions=_CostHook(cost_model),
                      integer_positions=not fractional, progress_bar=False, volume=vol_df, volatility=slip_df)
    bk.run()
    s = bk.strategy
    eq = s.values.reindex(panel.dates).to_numpy(dtype=float)
    pos = s.positions.reindex(index=panel.dates, columns=syms).fillna(0.0).to_numpy(dtype=float)
    prev = np.vstack([np.zeros((1, k)), pos[:-1]])
    dq = pos - prev
    ib, jb = np.nonzero(np.abs(dq) > 1e-12)
    order = np.lexsort((jb, ib))
    ib, jb = ib[order], jb[order]
    q = dq[ib, jb]
    ref = panel.close[ib, jb]
    sf = slip[ib, jb]
    fp = np.where(q > 0, ref * (1.0 + sf), ref * (1.0 - sf))
    comm = np.array([float(cost_model.commission(abs(a), b)) for a, b in zip(q, fp)]) if len(q) else np.zeros(0)
    fills = {'bar': ib.astype(np.int64), 'asset': jb.astype(np.int64), 'qty': q.astype(float), 'price': fp,
             'ref_price': ref.astype(float), 'commission': comm, 'reason': np.full(len(q), 5, np.int64),
             'phase': np.full(len(q), 2, np.int64)}
    cash = eq - np.nansum(pos * np.nan_to_num(panel.close), axis=1)
    bt_fees = float(np.nansum(s.fees.to_numpy(dtype=float))) if hasattr(s, 'fees') else None
    res = SimResult(panel=panel, equity=eq, cash=cash, positions=pos, borrow=np.zeros((n, k)), fill_arrays=fills,
                    accruals={'interest': 0.0, 'financing': 0.0, 'borrow': 0.0, 'capped': 0.0, 'dividends': 0.0},
                    initial_capital=float(initial_capital), fill_mode='next_close', cost_model=cost_model,
                    mode='weights', backend='bt', warnings=list(panel.warnings))
    res.bt_fees = bt_fees
    return res


def version() -> Optional[str]:
    return library_version()
