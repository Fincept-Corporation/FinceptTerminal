"""
Execution building blocks shared by every engine (decisions 2, 4 and 5).

FILL CONVENTION (decision 2)
  A signal is formed on bar t's CLOSE. Default fill: bar t+1's OPEN ('next_open').
  'next_close' (bar t+1's close) and 'same_close' (bar t's close - only on explicit request,
  and disclosed with a SAME_BAR_FILL warning) are selectable per run. fill_model_block()
  builds the result's assumptions.fillModel.

CALENDARS (decision 5)
  Bars keep the exchange's session date (price_store never shifts dates). align_panel()
  puts several symbols on one timeline: the UNION of their session dates; on a date a
  market was closed its prices are forward-filled from its last close (open = high = low =
  close = last close, volume 0) and the bar is marked NOT tradable. Signals are never
  forward-filled: base.simulator ignores signals and defers orders on non-tradable bars.

OPEN POSITIONS (decision 4)
  Positions still open at the last bar are marked to market at the last close and listed
  as trades with exitDate null, exitReason 'open', pnl = unrealised P&L net of the costs
  already paid. Trade statistics count them separately (tradeStats.openTrades / openPnl);
  Monte Carlo over trades includes them at their marked value (base.monte_carlo).

FIFO PAIRING
  fifo_round_trips() turns fills into round trips, splitting partial closes, flips from long
  to short, and the open remainder. Each trade carries the commission, slippage, borrow and
  dividend amounts of its share of the fills (pro rata by quantity), so
      sum(trade pnl incl. open) = final equity - initial equity - cash interest + financing paid
  and grossPnl = pnl + commission + slippageCost + borrowCost - dividends.

POSITION EPISODES (rebalancing portfolios)
  position_episodes() reports one trade per symbol position from the fill that opens it until it
  is flat again; rebalancing top-ups / trims stay inside the episode (their cash flows are in its
  P&L), so win rate and profit factor describe positions, not FIFO fragments of weight changes.
  The same identity holds: sum(episode pnl incl. open) = equity change (less interest).
"""

from __future__ import annotations

import math
from dataclasses import dataclass, field
from typing import Any, Dict, List, Optional, Sequence

import numpy as np

FILL_MODES = ('next_open', 'next_close', 'same_close')
FILL_MODE_CODES = {'next_open': 0, 'next_close': 1, 'same_close': 2}
# Where inside its bar a fill happened (for MAE / MFE windows).
PHASE_OPEN, PHASE_INTRABAR, PHASE_CLOSE = 0, 1, 2
REASONS = ('signal', 'stop_loss', 'take_profit', 'trailing_stop', 'end_of_data', 'rebalance')

_FILL_TEXT = {
    'next_open': 'Signals are formed on the close of bar t and filled at the OPEN of bar t+1.',
    'next_close': 'Signals are formed on the close of bar t and filled at the CLOSE of bar t+1.',
    'same_close': 'Signals are formed on the close of bar t and filled at the SAME close (bar t). This assumes '
                  'trading at a price that was only known once it printed - optimistic; requested explicitly.',
}


def normalize_fill_mode(value: Optional[str]) -> str:
    """Request value -> one of FILL_MODES (default next_open). Accepts common aliases."""
    v = str(value or 'next_open').strip().lower().replace('-', '_').replace(' ', '_')
    aliases = {'open': 'next_open', 'next_bar_open': 'next_open', 'nextopen': 'next_open',
               'close': 'next_close', 'next_bar_close': 'next_close', 'nextclose': 'next_close',
               'same_bar_close': 'same_close', 'on_close': 'same_close', 'trade_on_close': 'same_close'}
    v = aliases.get(v, v)
    if v not in FILL_MODES:
        raise ValueError(f'fill timing must be one of {", ".join(FILL_MODES)}, not {value!r}')
    return v


def fill_mode_from_request(request: Optional[Dict[str, Any]]) -> str:
    req = request if isinstance(request, dict) else {}
    return normalize_fill_mode(req.get('fillTiming') or req.get('fillModel') or req.get('executionTiming'))


def fill_model_block(mode: str) -> Dict[str, str]:
    """assumptions.fillModel of contract v2."""
    m = normalize_fill_mode(mode)
    return {'signalAt': 'close', 'fillAt': m, 'description': _FILL_TEXT[m]}


def fill_price_arrays(open_, close, mode: str):
    """(price, phase) a decision taken on bar t fills at: arrays aligned to the DECISION bar.
    next_open -> open[t+1], next_close -> close[t+1] (NaN for the last bar), same_close -> close[t]."""
    o = np.asarray(open_, dtype=float)
    c = np.asarray(close, dtype=float)
    m = normalize_fill_mode(mode)
    if m == 'same_close':
        return c.copy(), PHASE_CLOSE
    src = o if m == 'next_open' else c
    out = np.full_like(src, np.nan)
    out[:-1] = src[1:]
    return out, (PHASE_OPEN if m == 'next_open' else PHASE_CLOSE)


def slipped_price(price, side: int, fraction):
    """Adverse fill price: buys (side +1) pay price x (1 + f), sells (side -1) get price x (1 - f)."""
    return np.asarray(price, dtype=float) * (1.0 + np.sign(side) * np.asarray(fraction, dtype=float))


def max_affordable_qty(budget: float, price: float, cost_model=None, fractional: bool = True) -> float:
    """Largest quantity whose notional + commission fits `budget` at `price` (fill price)."""
    if not (budget > 0 and price > 0):
        return 0.0
    q = budget / price
    if cost_model is not None:
        ct, cv = cost_model.commission_type, cost_model.commission_value
        if ct == 'pct_notional':
            q = budget / (price * (1.0 + cv))
        elif ct == 'per_share':
            q = budget / (price + cv)
        else:
            q = (budget - cv) / price
        fee = cost_model.commission(q, price)
        if q > 0 and fee > 0 and q * price + fee > budget * (1 + 1e-12):
            # minimum / maximum clauses: solve with the fee fixed at its actual value
            q = (budget - fee) / price
    q = max(0.0, q)
    return float(q if fractional else math.floor(q))


# ════════════════════════════════════════════════════════════════════════════
# Calendar alignment
# ════════════════════════════════════════════════════════════════════════════

@dataclass
class Panel:
    """Several symbols on one timeline. Arrays are (n_bars, n_symbols)."""
    dates: Any                       # pandas DatetimeIndex (tz-naive, session dates)
    symbols: List[str]
    open: np.ndarray
    high: np.ndarray
    low: np.ndarray
    close: np.ndarray
    volume: np.ndarray
    tradable: np.ndarray             # bool: the symbol really traded on this bar
    dividends: Optional[np.ndarray] = None   # cash per share on the ex-date (raw prices only)
    splits: Optional[np.ndarray] = None      # split ratio on the ex-date, 1.0 otherwise
    adjustment: str = 'split+dividend'
    warnings: List[Dict[str, Any]] = field(default_factory=list)
    notes: List[str] = field(default_factory=list)

    @property
    def n(self) -> int:
        return len(self.dates)

    def col(self, symbol: str) -> int:
        return self.symbols.index(symbol)


def align_panel(frames: Dict[str, Any], how: str = 'union', start=None, end=None) -> Panel:
    """Align {symbol: DataFrame[Open, High, Low, Close, Volume (, Dividends, Splits)]} on one
    timeline. how='union' (default): every session date of any symbol; a closed market's
    prices are forward-filled from its last close and the bar is not tradable.
    how='intersection': only dates on which every symbol traded."""
    import pandas as pd
    if not frames:
        raise ValueError('no price data to align')
    syms = list(frames.keys())
    idxs = []
    clean = {}
    for s in syms:
        f = frames[s]
        idx = pd.DatetimeIndex(f.index)
        if idx.tz is not None:
            idx = idx.tz_localize(None)
        f = f.copy()
        f.index = idx
        f = f[~f.index.duplicated(keep='last')].sort_index()
        if start is not None:
            f = f[f.index >= pd.Timestamp(start)]
        if end is not None:
            f = f[f.index <= pd.Timestamp(end) + pd.Timedelta(hours=23, minutes=59)]
        clean[s] = f
        idxs.append(f.index)
    if how == 'intersection':
        common = idxs[0]
        for ix in idxs[1:]:
            common = common.intersection(ix)
        dates = common.sort_values()
    else:
        dates = idxs[0]
        for ix in idxs[1:]:
            dates = dates.union(ix)
        dates = dates.sort_values()
    n, k = len(dates), len(syms)
    arr = {c: np.full((n, k), np.nan) for c in ('Open', 'High', 'Low', 'Close', 'Volume')}
    div = np.zeros((n, k))
    spl = np.ones((n, k))
    tradable = np.zeros((n, k), dtype=bool)
    warnings: List[Dict[str, Any]] = []
    have_actions = False
    for j, s in enumerate(syms):
        f = clean[s].reindex(dates)
        real = f['Close'].notna().to_numpy() & np.isfinite(f['Close'].to_numpy(dtype=float))
        tradable[:, j] = real
        close = f['Close'].astype(float).ffill()
        listed = close.notna().to_numpy()
        for c in ('Open', 'High', 'Low'):
            v = f[c].astype(float).to_numpy() if c in f else f['Close'].astype(float).to_numpy()
            arr[c][:, j] = np.where(real, v, close.to_numpy())
        arr['Close'][:, j] = close.to_numpy()
        vol = f['Volume'].astype(float).to_numpy() if 'Volume' in f else np.zeros(n)
        arr['Volume'][:, j] = np.where(real, np.nan_to_num(vol), 0.0)
        if 'Dividends' in f:
            div[:, j] = np.nan_to_num(f['Dividends'].astype(float).to_numpy())
            have_actions = True
        if 'Splits' in f:
            sp = f['Splits'].astype(float).to_numpy()
            spl[:, j] = np.where(np.isfinite(sp) & (sp > 0), sp, 1.0)
            have_actions = True
        if how == 'union' and k > 1:
            filled = int(np.sum(listed & ~real))
            if filled:
                first = dates[int(np.argmax(listed))]
                warnings.append({'code': 'CALENDAR_MISMATCH', 'severity': 'info', 'symbol': s,
                                 'message': (f'{s} did not trade on {filled} of the {n} dates in the combined calendar '
                                             f'(from {first.date()}); its last close was carried forward on those '
                                             f'dates for valuation only - no signals or fills happen there.')})
    notes = []
    if how == 'union' and k > 1:
        notes.append('Symbols are aligned on the union of their session dates; on a date a market was closed its '
                     'price is carried forward for valuation only (no signals, no fills).')
    return Panel(dates=dates, symbols=syms, open=arr['Open'], high=arr['High'], low=arr['Low'],
                 close=arr['Close'], volume=arr['Volume'], tradable=tradable,
                 dividends=div if have_actions else None, splits=spl if have_actions else None,
                 warnings=warnings, notes=notes)


# ════════════════════════════════════════════════════════════════════════════
# Accounting helpers
# ════════════════════════════════════════════════════════════════════════════

def year_fractions(dates, day_count: float = 360.0) -> np.ndarray:
    """dt[i] = calendar days between bar i-1 and bar i / day_count (dt[0] = 0)."""
    import pandas as pd
    idx = pd.DatetimeIndex(dates)
    out = np.zeros(len(idx))
    if len(idx) > 1:
        out[1:] = np.diff(idx.asi8) / 86400e9 / float(day_count)
    return out


def accrue_cash_and_borrow(cash: float, short_value: float, dt: float, cash_rate: float,
                           financing_rate: float, borrow_rate: float):
    """One accrual step (used by the simulator kernel, exposed for engines that keep their own
    loop). short_value >= 0 is the market value of short positions at the previous close.
    Returns (interest (+ earned / - paid), borrow fee >= 0). Interest accrues on free cash = cash
    net of short proceeds (no short rebate); a negative free balance pays financing_rate."""
    free = cash - short_value
    interest = free * (cash_rate if free > 0 else financing_rate) * dt
    borrow = short_value * borrow_rate * dt
    return float(interest), float(borrow)


def mark_to_market(quantities, prices) -> np.ndarray:
    """Position values (signed) = quantity x price, element-wise; NaN prices count as 0."""
    q = np.asarray(quantities, dtype=float)
    p = np.asarray(prices, dtype=float)
    return np.where(np.isfinite(p), q * p, 0.0)


def positions_table(symbols: Sequence[str], qty, avg_price, last_price, equity: float) -> List[Dict[str, Any]]:
    """Contract `positions[]`: positions open at the last bar."""
    out = []
    for s, q, a, p in zip(symbols, qty, avg_price, last_price):
        if abs(q) < 1e-12:
            continue
        mv = float(q * p)
        out.append({'symbol': s, 'quantity': float(q), 'avgPrice': float(a), 'lastPrice': float(p),
                    'marketValue': mv, 'weight': (mv / equity) if equity else None,
                    'unrealizedPnl': float((p - a) * q)})
    return out


# ════════════════════════════════════════════════════════════════════════════
# Fills -> round trips
# ════════════════════════════════════════════════════════════════════════════

def _fmt(ts) -> str:
    return ts.strftime('%Y-%m-%d %H:%M:%S') if (ts.hour or ts.minute or ts.second) else ts.strftime('%Y-%m-%d')


def fifo_round_trips(fills: Dict[str, np.ndarray], dates, symbols: Sequence[str], *,
                     last_close: Optional[np.ndarray] = None, high=None, low=None,
                     borrow_per_bar: Optional[np.ndarray] = None, short_qty: Optional[np.ndarray] = None,
                     dividends: Optional[np.ndarray] = None, splits: Optional[np.ndarray] = None,
                     positions: Optional[np.ndarray] = None) -> List[Dict[str, Any]]:
    """Round trips (contract trades[]) from simulator fills.

    fills: dict of equal-length arrays bar, asset, qty (signed: + buy / - sell), price (fill,
    slippage included), ref_price (before slippage), commission, reason (index into REASONS),
    phase (PHASE_*). last_close: (k,) marks open lots. high / low: (n, k) for MAE / MFE.
    borrow_per_bar: (n, k) borrow fee charged on bar i for asset j; positions: (n, k) signed
    quantities at each close (to allocate borrow and dividends per share). dividends / splits:
    raw-price corporate actions (n, k).
    Closed trades come in entry order, open positions last (exitReason 'open').
    """
    import pandas as pd
    idx = pd.DatetimeIndex(dates)
    n = len(idx)
    bar = np.asarray(fills.get('bar', []), dtype=np.int64)
    m = len(bar)
    asset = np.asarray(fills.get('asset', np.zeros(m)), dtype=np.int64)
    qty = np.asarray(fills.get('qty', np.zeros(m)), dtype=float)
    px = np.asarray(fills.get('price', np.zeros(m)), dtype=float)
    ref = np.asarray(fills.get('ref_price', px), dtype=float)
    comm = np.asarray(fills.get('commission', np.zeros(m)), dtype=float)
    reason = np.asarray(fills.get('reason', np.zeros(m)), dtype=np.int64)
    phase = np.asarray(fills.get('phase', np.zeros(m)), dtype=np.int64)
    k = len(symbols)

    # Borrow / dividend cash attributable to a lot. Both are charged on bar t for the shares held
    # at the close of t-1. With splits, a lot's share count changes: shares(t) = q_a * CS(t)/CS(a),
    # CS = cumulative split factor. Accumulating per-share amounts scaled by CS(t-1) gives, for a
    # lot whose CURRENT quantity at bar b is q_b:  amount = q_b / CS(b) * (cum[b] - cum[a]).
    split_arr = np.asarray(splits, dtype=float) if splits is not None else None
    n_rows = len(idx)
    CS = np.cumprod(np.where(np.isfinite(split_arr) & (split_arr > 0), split_arr, 1.0), axis=0) \
        if split_arr is not None else np.ones((n_rows, k))
    CS_prev = np.vstack([np.ones((1, k)), CS[:-1]])
    cum_borrow = cum_div = None
    if positions is not None:
        P = np.asarray(positions, dtype=float)
        prev = np.vstack([np.zeros((1, k)), P[:-1]])
        if borrow_per_bar is not None:
            with np.errstate(divide='ignore', invalid='ignore'):
                ps = np.where(prev < 0, np.asarray(borrow_per_bar, dtype=float) / np.abs(prev), 0.0)
            cum_borrow = np.cumsum(np.nan_to_num(ps) * CS_prev, axis=0)
        if dividends is not None:
            cum_div = np.cumsum(np.asarray(dividends, dtype=float) * CS_prev, axis=0)

    trades: List[Dict[str, Any]] = []
    open_trades: List[Dict[str, Any]] = []
    order = np.lexsort((np.arange(m), bar))
    lots: Dict[int, List[list]] = {j: [] for j in range(k)}
    # lot = [qty_signed, entry_price, entry_ref, entry_bar, comm_per_unit, phase, split_mult]
    last_split_bar = np.zeros(k, dtype=np.int64)

    def apply_splits(j: int, upto: int):
        if split_arr is None:
            return
        a = last_split_bar[j]
        if upto <= a:
            return
        seg = split_arr[a + 1: upto + 1, j]
        if len(seg) and np.any(seg != 1.0):
            r = float(np.prod(seg))
            for lot in lots[j]:
                lot[0] *= r
                lot[1] /= r
                lot[2] /= r
                lot[4] /= r
                lot[6] *= r
        last_split_bar[j] = upto

    def carry(j, a, b, q_signed):
        """borrow and dividends attributable to a lot entered at bar a whose quantity at bar b
        (after any splits) is q_signed."""
        bc = dc = 0.0
        unit = q_signed / CS[b, j]
        if cum_borrow is not None and q_signed < 0:
            bc = abs(unit) * float(cum_borrow[b, j] - cum_borrow[a, j])
        if cum_div is not None:
            dc = unit * float(cum_div[b, j] - cum_div[a, j])
        return bc, dc

    for i in order:
        j = int(asset[i])
        b = int(bar[i])
        apply_splits(j, b)
        q = float(qty[i])
        side = 1.0 if q > 0 else -1.0
        q_left = abs(q)
        cpu = comm[i] / abs(q) if q != 0 else 0.0
        book = lots[j]
        while q_left > 1e-12 and book and (book[0][0] > 0) != (side > 0):
            lot = book[0]
            take = min(q_left, abs(lot[0]))
            lot_long = lot[0] > 0
            sgn = 1.0 if lot_long else -1.0
            bc, dc = carry(j, lot[3], b, take * sgn)
            c_entry, c_exit = lot[4] * take, cpu * take
            slip = abs(lot[1] - lot[2]) * take + abs(px[i] - ref[i]) * take
            pnl = (px[i] - lot[1]) * take * sgn - c_entry - c_exit - bc + dc
            gross = (ref[i] - lot[2]) * take * sgn
            basis = lot[1] * take
            trades.append({
                'symbol': symbols[j], 'side': 'long' if lot_long else 'short',
                'entryDate': _fmt(idx[lot[3]]), 'exitDate': _fmt(idx[b]),
                'entryPrice': float(lot[1]), 'exitPrice': float(px[i]), 'quantity': float(take),
                'pnl': float(pnl), 'pnlPercent': float(pnl / basis) if basis > 0 else None,
                'grossPnl': float(gross), 'commission': float(c_entry + c_exit), 'slippageCost': float(slip),
                'borrowCost': float(bc), 'dividends': float(dc),
                'holdingPeriod': int(b - lot[3]), 'holdingDays': int((idx[b] - idx[lot[3]]).days),
                'exitReason': REASONS[int(reason[i])] if 0 <= reason[i] < len(REASONS) else 'signal',
                'entryBar': int(lot[3]), 'exitBar': int(b),
                '_entryPhase': int(lot[5]), '_exitPhase': int(phase[i]), '_asset': j,
            })
            lot[0] = lot[0] - take * sgn
            q_left -= take
            if abs(lot[0]) <= 1e-12:
                book.pop(0)
        if q_left > 1e-12:
            book.append([q_left * side, float(px[i]), float(ref[i]), b, cpu, int(phase[i]), 1.0])

    for j in range(k):
        apply_splits(j, n - 1)
        for lot in lots[j]:
            q_s = lot[0]
            sgn = 1.0 if q_s > 0 else -1.0
            mark = float(last_close[j]) if last_close is not None and np.isfinite(last_close[j]) else None
            bc, dc = carry(j, lot[3], n - 1, q_s)
            c_entry = lot[4] * abs(q_s)
            slip = abs(lot[1] - lot[2]) * abs(q_s)
            pnl = ((mark - lot[1]) * abs(q_s) * sgn - c_entry - bc + dc) if mark is not None else None
            open_trades.append({
                'symbol': symbols[j], 'side': 'long' if q_s > 0 else 'short',
                'entryDate': _fmt(idx[lot[3]]), 'exitDate': None, 'entryPrice': float(lot[1]),
                'exitPrice': None, 'markPrice': mark, 'quantity': float(abs(q_s)),
                'pnl': None if pnl is None else float(pnl),
                'pnlPercent': (float(pnl / (lot[1] * abs(q_s))) if (pnl is not None and lot[1] > 0) else None),
                'grossPnl': ((mark - lot[2]) * abs(q_s) * sgn) if mark is not None else None,
                'commission': float(c_entry), 'slippageCost': float(slip), 'borrowCost': float(bc),
                'dividends': float(dc), 'holdingPeriod': int(n - 1 - lot[3]),
                'holdingDays': int((idx[n - 1] - idx[lot[3]]).days), 'exitReason': 'open', 'unrealized': True,
                'entryBar': int(lot[3]), 'exitBar': None, '_entryPhase': int(lot[5]), '_exitPhase': PHASE_CLOSE,
                '_asset': j,
            })
    trades.sort(key=lambda t: (t['entryBar'], t['exitBar'] if t['exitBar'] is not None else n))
    open_trades.sort(key=lambda t: t['entryBar'])
    out = trades + open_trades
    if high is not None and low is not None:
        add_excursions(out, high, low)
    for i, t in enumerate(out, 1):
        t['id'] = f't{i}'
        for kk in ('_entryPhase', '_exitPhase', '_asset'):
            t.pop(kk, None)
    return out


def position_episodes(fills: Dict[str, np.ndarray], dates, symbols: Sequence[str], *,
                      last_close: Optional[np.ndarray] = None, high=None, low=None,
                      borrow_per_bar: Optional[np.ndarray] = None, positions: Optional[np.ndarray] = None,
                      dividends: Optional[np.ndarray] = None, splits: Optional[np.ndarray] = None,
                      excursions: bool = True) -> List[Dict[str, Any]]:
    """Trades as POSITION EPISODES (contract trades[]) - for portfolios that rebalance.

    An episode is one symbol's position from the fill that opens it out of flat until the fill
    that brings it back to flat (a fill that crosses zero closes the episode and opens the next
    one with the remainder, its costs split pro rata). Rebalancing top-ups and trims stay INSIDE
    their episode: their cash flows (and commission / slippage) are part of its P&L, so a trim of
    a winner is not counted as a winning trade (FIFO pairing of rebalance fills produced hundreds
    of such fragments - audit T2.1). An episode still open at the last bar is marked to market
    at last_close (exitReason 'open'). Hence
        sum(episode pnl incl. open) = final equity - initial equity - cash interest + financing paid
    exactly as for fifo_round_trips, and for a strategy that only enters and exits whole
    positions (signals mode) the episodes ARE its round trips.

    Fields (contract trade + extras): entryPrice = quantity-weighted average price of the fills
    that ADDED to the position; exitPrice = that of the fills that REDUCED it (null while open);
    quantity = the largest absolute position held during the episode; pnl = net cash flow of all
    its fills (sales - purchases - commission, slippage inside the fill prices) - borrow +
    dividends (+ market value at last_close when open); commission / slippageCost / borrowCost /
    dividends summed over the episode; fills / adds / reduces = fill counts; episode = True.
    exitReason = the reason of the fill that closed it ('rebalance' for a target weight of 0).
    The normaliser derives pnlPercent = pnl / (entryPrice x quantity), i.e. on the largest
    position at its average cost.
    Inputs as fifo_round_trips (fills: bar, asset, qty signed, price, ref_price, commission,
    reason, phase; positions (n, k) to allocate raw-price dividends; splits (n, k) ratios scale
    the open quantity)."""
    import pandas as pd
    idx = pd.DatetimeIndex(dates)
    n = len(idx)
    bar = np.asarray(fills.get('bar', []), dtype=np.int64)
    m = len(bar)
    asset = np.asarray(fills.get('asset', np.zeros(m)), dtype=np.int64)
    qty = np.asarray(fills.get('qty', np.zeros(m)), dtype=float)
    px = np.asarray(fills.get('price', np.zeros(m)), dtype=float)
    ref = np.asarray(fills.get('ref_price', px), dtype=float)
    comm = np.asarray(fills.get('commission', np.zeros(m)), dtype=float)
    reason = np.asarray(fills.get('reason', np.zeros(m)), dtype=np.int64)
    phase = np.asarray(fills.get('phase', np.zeros(m)), dtype=np.int64)
    k = len(symbols)
    borrow = np.asarray(borrow_per_bar, dtype=float) if borrow_per_bar is not None else None
    cum_borrow = np.cumsum(np.nan_to_num(borrow), axis=0) if borrow is not None else None
    div_cash = None
    if dividends is not None and positions is not None:
        P = np.asarray(positions, dtype=float)
        prev = np.vstack([np.zeros((1, k)), P[:-1]])
        div_cash = np.cumsum(prev * np.nan_to_num(np.asarray(dividends, dtype=float)), axis=0)
    spl = np.asarray(splits, dtype=float) if splits is not None else None

    def _range_sum(cum, j, a, b):
        """sum of the per-bar amounts of bars a+1..b (charged for holding over (a, b])."""
        if cum is None or b <= a:
            return 0.0
        return float(cum[b, j] - cum[a, j])

    order = np.lexsort((np.arange(m), bar))
    closed: List[Dict[str, Any]] = []
    opened: List[Dict[str, Any]] = []
    state: Dict[int, Dict[str, Any]] = {}
    last_bar = np.zeros(k, dtype=np.int64)

    def new_episode(j, b, sign, ph):
        return {'j': j, 'a': b, 'sign': sign, 'q': 0.0, 'peak': 0.0, 'add_q': 0.0, 'add_v': 0.0, 'red_q': 0.0,
                'red_v': 0.0, 'cash': 0.0, 'comm': 0.0, 'slip': 0.0, 'fills': 0, 'adds': 0, 'reduces': 0,
                'ph': ph}

    def apply(ep, dq, p, r, c):
        """Book a fill of dq (signed) at fill price p, reference price r, commission c."""
        ep['cash'] += -dq * p - c
        ep['comm'] += c
        ep['slip'] += abs(p - r) * abs(dq)
        ep['fills'] += 1
        if (dq > 0) == (ep['sign'] > 0):
            ep['add_q'] += abs(dq)
            ep['add_v'] += abs(dq) * p
            ep['adds'] += 1
        else:
            ep['red_q'] += abs(dq)
            ep['red_v'] += abs(dq) * p
            ep['reduces'] += 1
        ep['q'] += dq
        ep['peak'] = max(ep['peak'], abs(ep['q']))

    def finish(ep, b, rsn, xph, mark):
        j, a = ep['j'], ep['a']
        open_ = b is None
        end = n - 1 if open_ else b
        bc = _range_sum(cum_borrow, j, a, end)
        dc = _range_sum(div_cash, j, a, end)
        mv = (ep['q'] * mark) if (open_ and mark is not None) else 0.0
        pnl = None if (open_ and mark is None) else ep['cash'] + mv - bc + dc
        entry = ep['add_v'] / ep['add_q'] if ep['add_q'] > 0 else None
        t = {'symbol': symbols[j], 'side': 'long' if ep['sign'] > 0 else 'short',
             'entryDate': _fmt(idx[a]), 'exitDate': None if open_ else _fmt(idx[b]),
             'entryPrice': float(entry) if entry is not None else None,
             'exitPrice': None if open_ else (float(ep['red_v'] / ep['red_q']) if ep['red_q'] > 0 else None),
             'quantity': float(ep['peak']), 'pnl': None if pnl is None else float(pnl),
             'commission': float(ep['comm']), 'slippageCost': float(ep['slip']), 'borrowCost': float(bc),
             'dividends': float(dc), 'holdingPeriod': int(end - a), 'holdingDays': int((idx[end] - idx[a]).days),
             'exitReason': 'open' if open_ else rsn, 'entryBar': int(a), 'exitBar': None if open_ else int(b),
             'fills': int(ep['fills']), 'adds': int(ep['adds']), 'reduces': int(ep['reduces']), 'episode': True,
             '_entryPhase': int(ep['ph']), '_exitPhase': int(xph), '_asset': j}
        if open_:
            t['markPrice'] = mark
            t['unrealized'] = True
            t['openQuantity'] = float(abs(ep['q']))
        if pnl is not None:
            gross = pnl + ep['comm'] + ep['slip'] + bc - dc
            t['grossPnl'] = float(gross)
            basis = (entry or 0.0) * ep['peak']
            t['pnlPercent'] = float(pnl / basis) if basis > 0 else None
        else:
            t['grossPnl'] = None
            t['pnlPercent'] = None
        (opened if open_ else closed).append(t)

    for i in order:
        j = int(asset[i])
        b = int(bar[i])
        ep = state.get(j)
        if spl is not None and ep is not None and b > last_bar[j]:
            seg = spl[last_bar[j] + 1: b + 1, j]
            if len(seg) and np.any(seg != 1.0):
                r_ = float(np.prod(np.where(np.isfinite(seg) & (seg > 0), seg, 1.0)))
                ep['q'] *= r_
                ep['peak'] = max(ep['peak'], abs(ep['q']))
        last_bar[j] = b
        dq = float(qty[i])
        if dq == 0.0:
            continue
        p, r, c = float(px[i]), float(ref[i]), float(comm[i])
        rsn = REASONS[int(reason[i])] if 0 <= reason[i] < len(REASONS) else 'signal'
        while dq != 0.0:
            if ep is None:
                ep = new_episode(j, b, 1.0 if dq > 0 else -1.0, int(phase[i]))
                state[j] = ep
                apply(ep, dq, p, r, c)
                dq = 0.0
                break
            q0 = ep['q']
            if (dq > 0) == (q0 > 0):            # adds to the position
                apply(ep, dq, p, r, c)
                dq = 0.0
                break
            tol = 1e-9 * max(1.0, abs(q0))
            if abs(dq) < abs(q0) - tol:          # partial reduce
                apply(ep, dq, p, r, c)
                dq = 0.0
                break
            # closes the position (and maybe flips): split the fill pro rata
            part = abs(q0) / abs(dq) if abs(dq) > 0 else 1.0
            part = min(1.0, part)
            close_dq = -q0
            apply(ep, close_dq, p, r, c * part)
            ep['q'] = 0.0
            finish(ep, b, rsn, int(phase[i]), None)
            state.pop(j, None)
            ep = None
            rest = dq - close_dq
            c = c * (1.0 - part)
            dq = rest if abs(rest) > tol else 0.0
    for j, ep in sorted(state.items()):
        mark = None
        if last_close is not None and np.isfinite(last_close[j]):
            mark = float(last_close[j])
        if spl is not None and last_bar[j] < n - 1:
            seg = spl[last_bar[j] + 1:, j]
            if len(seg) and np.any(seg != 1.0):
                ep['q'] *= float(np.prod(np.where(np.isfinite(seg) & (seg > 0), seg, 1.0)))
        finish(ep, None, 'open', PHASE_CLOSE, mark)
    closed.sort(key=lambda t: (t['entryBar'], t['exitBar']))
    opened.sort(key=lambda t: t['entryBar'])
    out = closed + opened
    if excursions and high is not None and low is not None:
        add_excursions(out, high, low)
    for i, t in enumerate(out, 1):
        t['id'] = f't{i}'
        for kk in ('_entryPhase', '_exitPhase', '_asset'):
            t.pop(kk, None)
    return out


def add_excursions(trades: List[Dict[str, Any]], high, low, open_=None) -> None:
    """MAE (<= 0) and MFE (>= 0) of each trade as fractions of its entry price, from the bars
    the position was really exposed to (B9): a fill at a bar's OPEN is exposed to that bar's
    whole range; a fill at a CLOSE is not exposed to that bar's range; an exit at an open is
    exposed to the exit bar's open only; an intrabar (stop) exit to the exit price only.
    Uses the private _entryPhase / _exitPhase / _asset fields set by fifo_round_trips; trades
    without them are treated as next-open fills on asset column 0 (legacy engines)."""
    H = np.asarray(high, dtype=float)
    L = np.asarray(low, dtype=float)
    if H.ndim == 1:
        H, L = H[:, None], L[:, None]
    n = H.shape[0]
    for t in trades:
        a = t.get('entryBar')
        if a is None:
            continue
        a = int(a)
        b = t.get('exitBar')
        b = n - 1 if b is None else int(b)
        j = int(t.get('_asset', 0))
        ep = t.get('_entryPhase', PHASE_OPEN)
        xp = t.get('_exitPhase', PHASE_OPEN if t.get('exitBar') is not None else PHASE_CLOSE)
        lo_i = a if ep == PHASE_OPEN else a + 1
        hi_i = b if xp == PHASE_CLOSE else b - 1
        entry = float(t.get('entryPrice') or 0)
        if entry <= 0:
            continue
        prices_hi = [entry]
        prices_lo = [entry]
        xprice = t.get('exitPrice') if t.get('exitPrice') is not None else t.get('markPrice')
        if xprice is not None:
            prices_hi.append(float(xprice))
            prices_lo.append(float(xprice))
        if hi_i >= lo_i:
            hs = H[lo_i:hi_i + 1, j]
            ls = L[lo_i:hi_i + 1, j]
            if np.isfinite(hs).any():
                prices_hi.append(float(np.nanmax(hs)))
            if np.isfinite(ls).any():
                prices_lo.append(float(np.nanmin(ls)))
        hi, lo = max(prices_hi), min(prices_lo)
        if t.get('side') == 'short':
            t['mae'] = min(0.0, -(hi / entry - 1.0))
            t['mfe'] = max(0.0, 1.0 - lo / entry)
        else:
            t['mae'] = min(0.0, lo / entry - 1.0)
            t['mfe'] = max(0.0, hi / entry - 1.0)


def fills_table(fills: Dict[str, np.ndarray], dates, symbols: Sequence[str]) -> List[Dict[str, Any]]:
    """Contract `fills[]` rows."""
    import pandas as pd
    idx = pd.DatetimeIndex(dates)
    out = []
    for i in range(len(fills.get('bar', []))):
        q = float(fills['qty'][i])
        px = float(fills['price'][i])
        ref = float(fills['ref_price'][i]) if 'ref_price' in fills else px
        r = int(fills['reason'][i]) if 'reason' in fills else 0
        out.append({'date': _fmt(idx[int(fills['bar'][i])]), 'symbol': symbols[int(fills['asset'][i])],
                    'side': 'buy' if q > 0 else 'sell', 'quantity': abs(q), 'price': px,
                    'commission': float(fills['commission'][i]), 'slippageCost': abs(px - ref) * abs(q),
                    'reason': REASONS[r] if 0 <= r < len(REASONS) else 'signal'})
    return out


def weights_block(dates, symbols: Sequence[str], position_values, equity, max_rows: int = 600,
                  min_symbols: int = 2) -> Optional[Dict[str, Any]]:
    """Contract `weights` for multi-asset runs: {t: [date], symbols: [str], w: [[weight]]} with
    weight = signed position value / equity at the bar's close (null when equity <= 0). Sampled
    every `step` bars (weekly for long daily runs, at most `max_rows` rows) and always including
    the last bar. None for fewer than `min_symbols` symbols. position_values: (n, k) money."""
    import pandas as pd
    k = len(symbols)
    v = np.asarray(position_values, dtype=float)
    if v.ndim == 1:
        v = v[:, None]
    e = np.asarray(equity, dtype=float)
    n = len(e)
    if k < min_symbols or n == 0 or v.shape != (n, k):
        return None
    step = max(1, int(math.ceil(n / max_rows)), 5 if n > 5 * 52 else 1)
    rows = list(range(0, n, step))
    if rows[-1] != n - 1:
        rows.append(n - 1)
    idx = pd.DatetimeIndex(dates)
    w = []
    for i in rows:
        if e[i] > 0 and np.isfinite(e[i]):
            w.append([round(float(x / e[i]), 5) if np.isfinite(x) else None for x in v[i]])
        else:
            w.append([None] * k)
    return {'t': [_fmt(idx[i]) for i in rows], 'symbols': list(symbols), 'w': w}
