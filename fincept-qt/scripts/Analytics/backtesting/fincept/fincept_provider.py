#!/usr/bin/env python3
"""
Fincept Engine Backtesting Provider
Executes strategies from the 418+ Fincept Terminal strategy registry.

Wire shape (matches other providers — see base/base_provider.py::json_response):
  Success: {"success": true, "data": {"performance": {...}, "trades": [...],
                                      "equity": [...], "statistics": {...}, ...}}
  Failure: {"success": false, "error": "...", "traceback": "..."}

Stdout is reserved exclusively for the final JSON payload — every diagnostic
goes to stderr so the C++ host's `extract_json` never picks up a leading '['
from a log line like "[Fincept Engine] Executing...".
"""

import sys
import json
import math
from pathlib import Path
from datetime import datetime, timedelta
from typing import Dict, Any, List, Optional

# Add paths
BASE_DIR = Path(__file__).resolve().parent.parent
STRATEGIES_DIR = BASE_DIR.parent.parent / "strategies"
sys.path.insert(0, str(STRATEGIES_DIR))
sys.path.insert(0, str(BASE_DIR / "base"))
# For json_response (camelCase conversion + NaN sanitisation, shared with the
# other providers).
sys.path.insert(0, str(BASE_DIR))

from fincept_strategy_runner import FinceptStrategyRunner
from base.base_provider import json_response, parse_json_input


def _log(msg: str) -> None:
    """All provider diagnostics route to stderr to keep stdout JSON-clean."""
    print(f"[Fincept Engine] {msg}", file=sys.stderr)


def _safe_float(x, default: float = 0.0) -> float:
    """Coerce to float; return default for NaN/Inf/None/non-numeric."""
    try:
        f = float(x)
        if math.isnan(f) or math.isinf(f):
            return default
        return f
    except (TypeError, ValueError):
        return default


def _day_str(ts) -> str:
    """'YYYY-MM-DD' from the runner's 'YYYY-MM-DD HH:MM:SS' bar time."""
    return str(ts).split(' ')[0].split('T')[0]


def _normalize_equity(equity_curve: List[Dict[str, Any]], initial_capital: float) -> List[Dict[str, Any]]:
    """Runner equity points are {time, equity}; the frontend's chart reads
    {date, equity, returns, drawdown} (drawdown a NEGATIVE fraction, like every other
    provider). With the old shape the chart found no `date` and stayed blank."""
    out: List[Dict[str, Any]] = []
    peak = initial_capital if initial_capital > 0 else 0.0
    for pt in equity_curve or []:
        if not isinstance(pt, dict):
            continue
        v = _safe_float(pt.get('equity'), float('nan'))
        if math.isnan(v):
            continue
        peak = max(peak, v)
        out.append({
            'date': _day_str(pt.get('time', pt.get('date', ''))),
            'equity': v,
            'returns': ((v - initial_capital) / initial_capital) if initial_capital > 0 else 0.0,
            'drawdown': ((v - peak) / peak) if peak > 0 else 0.0,
        })
    return out


def _build_round_trips(fills: List[Dict[str, Any]],
                       open_positions: Optional[List[Dict[str, Any]]] = None) -> List[Dict[str, Any]]:
    """Turn the runner's raw FILLS ({time, symbol, quantity(signed), price, type}) into
    closed round-trip trades with realised P&L (average-cost method).

    The runner reports each order, with no pnl; `total_trades` therefore counted buys AND
    sells separately, win rate / profit factor / expectancy were all 0, and the DETAILS table
    was full of dashes. A reducing fill realises P&L against the position's average cost; a
    fill through zero closes the old position and opens the remainder the other way. A position
    still open at the end (`open_positions`) is reported as an open trade marked at its last
    price, so a buy-and-hold run is not shown as "zero trades".
    """
    positions: Dict[str, List[Any]] = {}  # symbol -> [signed qty, avg cost, open time]
    rows: List[Dict[str, Any]] = []
    for f in fills or []:
        if not isinstance(f, dict):
            continue
        sym = str(f.get('symbol', ''))
        q = _safe_float(f.get('quantity'))
        px = _safe_float(f.get('price'))
        t = f.get('time')
        if q == 0 or px <= 0:
            continue
        pos = positions.setdefault(sym, [0.0, 0.0, None])
        qty, avg, opened = pos
        if qty == 0 or (qty > 0) == (q > 0):  # open / add
            new_qty = qty + q
            avg = (abs(qty) * avg + abs(q) * px) / abs(new_qty)
            pos[:] = [new_qty, avg, opened if qty != 0 else t]
            continue
        closing = min(abs(q), abs(qty))
        direction = 1 if qty > 0 else -1
        pnl = (px - avg) * closing * direction
        try:
            from datetime import datetime as _dt
            holding = (_dt.strptime(str(t)[:19], '%Y-%m-%d %H:%M:%S') -
                       _dt.strptime(str(opened)[:19], '%Y-%m-%d %H:%M:%S')).days
        except Exception:
            holding = None
        rows.append({
            'id': f'trade_{len(rows)}',
            'symbol': sym,
            'entry_date': _day_str(opened),
            'exit_date': _day_str(t),
            'side': 'long' if direction > 0 else 'short',
            'quantity': closing,
            'entry_price': avg,
            'exit_price': px,
            'pnl': pnl,
            'pnl_percent': ((px / avg - 1.0) * direction) if avg > 0 else 0.0,
            'holding_period': holding,
            'exit_reason': 'signal',
            'commission': 0.0,
            'slippage': 0.0,
        })
        new_qty = qty + q
        if new_qty == 0:
            pos[:] = [0.0, 0.0, None]
        elif (new_qty > 0) != (qty > 0):  # flipped: the remainder opens the other way at px
            pos[:] = [new_qty, px, t]
        else:  # partial reduce keeps the original cost basis
            pos[:] = [new_qty, avg, opened]

    for op in open_positions or []:
        sym = str(op.get('symbol', ''))
        qty = _safe_float(op.get('quantity'))
        mkt = _safe_float(op.get('market_price'))
        if qty == 0 or mkt <= 0 or sym not in positions:
            continue
        _, avg, opened = positions[sym]
        direction = 1 if qty > 0 else -1
        last_t = op.get('time')
        try:
            from datetime import datetime as _dt
            holding = (_dt.strptime(str(last_t)[:19], '%Y-%m-%d %H:%M:%S') -
                       _dt.strptime(str(opened)[:19], '%Y-%m-%d %H:%M:%S')).days
        except Exception:
            holding = None
        rows.append({
            'id': f'trade_{len(rows)}',
            'symbol': sym,
            'entry_date': _day_str(opened),
            'exit_date': _day_str(last_t),
            'side': 'long' if direction > 0 else 'short',
            'quantity': abs(qty),
            'entry_price': avg,
            'exit_price': mkt,
            'pnl': (mkt - avg) * abs(qty) * direction,
            'pnl_percent': ((mkt / avg - 1.0) * direction) if avg > 0 else 0.0,
            'holding_period': holding,
            'exit_reason': 'open_at_end',
            'commission': 0.0,
            'slippage': 0.0,
        })
    return rows


# Frontend's display_result() reads these performance keys (all in fraction
# form, e.g. 0.12 = 12%). Other providers all emit them via _enrich_metrics.
def _enrich_metrics_from_equity(
    runner_perf: Dict[str, Any],
    equity_curve: List[Dict[str, Any]],
    trades: List[Dict[str, Any]],
    initial_capital: float,
) -> Dict[str, Any]:
    """Expand the runner's bare 6-key performance dict into the full set the
    frontend (and every other provider) expects, computing missing metrics
    from the equity curve.
    """
    # Convert equity curve into per-bar returns and drawdowns.
    eq_values = []
    for pt in equity_curve or []:
        v = pt.get('equity') if isinstance(pt, dict) else None
        if v is not None:
            eq_values.append(_safe_float(v))
    if not eq_values:
        eq_values = [initial_capital]

    daily_returns: List[float] = []
    for i in range(1, len(eq_values)):
        prev = eq_values[i - 1]
        if prev > 0:
            daily_returns.append((eq_values[i] - prev) / prev)
        else:
            daily_returns.append(0.0)

    # Drawdown from running max.
    peak = eq_values[0] if eq_values else initial_capital
    max_dd = 0.0
    for v in eq_values:
        if v > peak:
            peak = v
        if peak > 0:
            dd = (peak - v) / peak
            if dd > max_dd:
                max_dd = dd

    # Sharpe / Sortino — annualised (252 trading days).
    sharpe = 0.0
    sortino = 0.0
    volatility = 0.0
    if daily_returns:
        n = len(daily_returns)
        mean_r = sum(daily_returns) / n
        var = sum((r - mean_r) ** 2 for r in daily_returns) / max(1, n - 1)
        std = math.sqrt(var)
        ann_factor = math.sqrt(252)
        if std > 1e-12:
            sharpe = (mean_r * 252) / (std * ann_factor)
        volatility = std * ann_factor

        # Standard downside deviation: sqrt(mean(min(r, 0)^2)) over ALL bars. (It was the RMS
        # of the negative bars only — divided by the number of losing bars — which overstates
        # downside risk and understates Sortino whenever losses are the minority.)
        d_var = sum(min(r, 0.0) ** 2 for r in daily_returns) / n
        d_std = math.sqrt(d_var)
        if d_std > 1e-12:
            sortino = (mean_r * 252) / (d_std * ann_factor)

    # Total return / annualised return — from the equity curve, as a FRACTION. The runner's
    # own `total_return` is already in PERCENT, and used here as a fraction it showed a +12.5%
    # run as "1250%" and blew the annualised figure up with it.
    total_return = 0.0
    if initial_capital > 0 and eq_values:
        total_return = (eq_values[-1] - initial_capital) / initial_capital
    days = max(1, len(eq_values))
    years = days / 252.0
    if years > 0 and (1 + total_return) > 0:
        annualized_return = (1 + total_return) ** (1 / years) - 1
    else:
        annualized_return = -1.0 if (1 + total_return) <= 0 else total_return

    calmar = (annualized_return / max_dd) if max_dd > 1e-12 else 0.0

    # Trade-level stats (closed round trips; see _build_round_trips).
    total_trades = len(trades) if trades else 0
    pnls: List[float] = []
    for t in trades or []:
        # Runner emits trades with quantity/price/type — no direct pnl.
        # Callers may pass pre-computed pnl; otherwise default to 0.
        if isinstance(t, dict) and 'pnl' in t:
            pnls.append(_safe_float(t.get('pnl')))
    wins = [p for p in pnls if p > 0]
    losses = [p for p in pnls if p < 0]
    winning_trades = len(wins)
    losing_trades = len(losses)
    win_rate = (winning_trades / total_trades) if total_trades > 0 else 0.0
    loss_rate = (losing_trades / total_trades) if total_trades > 0 else 0.0
    average_win = (sum(wins) / len(wins)) if wins else 0.0
    average_loss = (sum(losses) / len(losses)) if losses else 0.0
    largest_win = max(wins) if wins else 0.0
    largest_loss = min(losses) if losses else 0.0
    gross_profit = sum(wins) if wins else 0.0
    gross_loss = abs(sum(losses)) if losses else 0.0
    # Winners and no losing trade is +inf (the JSON layer emits null -> the UI shows "∞").
    if gross_loss > 1e-12:
        profit_factor = gross_profit / gross_loss
    else:
        profit_factor = float('inf') if gross_profit > 0 else 0.0

    return {
        'total_return': total_return,
        'annualized_return': annualized_return,
        'sharpe_ratio': sharpe,
        'sortino_ratio': sortino,
        'max_drawdown': max_dd,
        'win_rate': win_rate,
        'loss_rate': loss_rate,
        'profit_factor': profit_factor,
        'volatility': volatility,
        'calmar_ratio': calmar,
        'total_trades': total_trades,
        'winning_trades': winning_trades,
        'losing_trades': losing_trades,
        'average_win': average_win,
        'average_loss': average_loss,
        'largest_win': largest_win,
        'largest_loss': largest_loss,
        'average_trade_return': (sum(pnls) / len(pnls)) if pnls else 0.0,
        'expectancy': win_rate * average_win + loss_rate * average_loss,
        'total_orders': int(runner_perf.get('total_trades', 0)),  # raw fills (buys + sells)
        # Strategy metadata kept on the perf dict for parity with the
        # original (lighter) shape.
        'strategy_id': runner_perf.get('strategy_id'),
        'strategy_name': runner_perf.get('strategy_name'),
        'final_equity': _safe_float(runner_perf.get('final_equity', eq_values[-1] if eq_values else initial_capital)),
        'initial_cash': _safe_float(runner_perf.get('initial_cash', initial_capital)),
    }


class FinceptProvider:
    """
    Fincept Engine backtesting provider.
    Bridges Fincept Terminal frontend to 418+ QCAlgorithm strategies.
    """

    def __init__(self):
        self.runner = FinceptStrategyRunner()

    def get_strategies(self, request: Dict[str, Any]) -> Dict[str, Any]:
        """Return strategy catalog in BT shape: {strategies: {category: [{id, name, params:[]}]}}."""
        flat = self.runner.list_strategies()
        grouped: Dict[str, List] = {}
        for s in flat:
            cat = s.get('category', 'General Strategy')
            grouped.setdefault(cat, []).append({
                'id':     s['id'],
                'name':   s['name'],
                'params': [],  # Fincept strategies use free-form strategy_params dict
            })
        return {'success': True, 'data': {'provider': 'fincept', 'strategies': grouped}}

    def get_indicators(self, request: Dict[str, Any]) -> Dict[str, Any]:
        """Fincept strategies are self-contained — no separate indicator catalog."""
        return {'indicators': {}}

    def get_command_options(self, request: Dict[str, Any]) -> Dict[str, Any]:
        """Return provider-specific option lists.

        Keys are snake_case here so we don't depend on json_response's camelCase
        conversion — but the C++ frontend's `pick(camel, snake)` helper
        (BacktestingScreen.cpp::on_command_options_loaded) tries both forms,
        so this is a safety belt; either form works.
        """
        return {
            'success': True,
            'data': {
                'position_sizing_methods': ['percent', 'fixed', 'kelly', 'vol_target', 'risk'],
                'optimize_objectives':     ['sharpe', 'sortino', 'calmar', 'return'],
                'optimize_methods':        ['grid', 'random'],
                'label_types':             [],
                'splitter_types':          [],
                'signal_generators':       [],
                'indicator_signal_modes':  [],
                'returns_analysis_types':  [],
            },
        }

    # ------------------------------------------------------------------
    # Internal: single backtest execution returning a flattened result_dict
    # in the same shape the other providers emit (`performance`, `trades`,
    # `equity`, `statistics`).
    # ------------------------------------------------------------------
    def _execute_one(
        self,
        strategy_id: str,
        symbols: List[str],
        start_date: str,
        end_date: str,
        initial_capital: float,
        strategy_params: Dict[str, Any],
    ) -> Dict[str, Any]:
        strategy_info = self.runner.get_strategy_info(strategy_id)
        if not strategy_info:
            return {
                'success': False,
                'error': f'Strategy {strategy_id} not found in registry',
            }

        _log(f"Executing strategy: {strategy_info['name']}")
        _log(f"ID: {strategy_id}")
        _log(f"Category: {strategy_info.get('category')}")
        _log(f"Symbols: {symbols}")
        _log(f"Period: {start_date} to {end_date}")

        result = self.runner.execute_strategy(
            strategy_id=strategy_id,
            params={
                'symbols': symbols,
                'start_date': start_date,
                'end_date': end_date,
                'initial_cash': initial_capital,
                'resolution': 'daily',
                'strategy_params': strategy_params,
            },
        )

        if not result.get('success'):
            return {
                'success': False,
                'error': result.get('error', 'Unknown runner error'),
                'traceback': result.get('traceback'),
            }

        # Runner returns {success: true, data: {performance, trades, equity}}.
        # Flatten — return just the inner data block plus computed metrics.
        inner = result.get('data') or {}
        runner_perf = inner.get('performance') or {}
        fills = inner.get('trades') or []
        raw_equity = inner.get('equity') or []

        # Frontend-shaped equity curve + realised round-trip trades (the runner emits raw
        # time/fill records the screen cannot render).
        equity = _normalize_equity(raw_equity, initial_capital)
        trades = _build_round_trips(fills, inner.get('open_positions'))
        performance = _enrich_metrics_from_equity(runner_perf, raw_equity, trades, initial_capital)

        # Build statistics block matching the dataclass other providers use.
        statistics = {
            'start_date': start_date,
            'end_date': end_date,
            'initial_capital': float(initial_capital),
            'final_capital': performance.get('final_equity', float(initial_capital)),
            'total_fees': 0.0,
            'total_slippage': 0.0,
            'total_trades': performance['total_trades'],
            'winning_days': 0,
            'losing_days': 0,
            'average_daily_return': 0.0,
            'best_day': 0.0,
            'worst_day': 0.0,
            'consecutive_wins': 0,
            'consecutive_losses': 0,
        }

        return {
            'success': True,
            'data': {
                'id': strategy_id,
                'status': 'completed',
                'performance': performance,
                'trades': trades,
                'fills': fills,
                'equity': equity,
                'statistics': statistics,
                'logs': [
                    f"Fincept Engine backtest of {strategy_info['name']} ({strategy_id})",
                    f"Symbols: {symbols}",
                    f"Period: {start_date} to {end_date}",
                ],
            },
        }

    def _extract_request(self, request: Dict[str, Any]):
        """Pull canonical fields from a backtest/optimize/walk_forward request,
        accepting both new (`params`/`symbols`) and legacy (`parameters`/
        `assets`) names."""
        strategy_def = request.get('strategy', {}) or {}
        strategy_id = strategy_def.get('type')
        # Frontend sends `params`; legacy callers used `parameters`.
        strategy_params = strategy_def.get('params') or strategy_def.get('parameters', {}) or {}

        # Canonical: `symbols: [str]`. Fallback: `assets: [{symbol}]`.
        symbols = request.get('symbols') or []
        if not symbols and 'assets' in request:
            symbols = [a.get('symbol') for a in (request['assets'] or [])
                       if isinstance(a, dict) and a.get('symbol')]

        start_date = request.get('startDate', '2025-01-01')
        end_date = request.get('endDate',
                               (datetime.now() - timedelta(days=1)).strftime('%Y-%m-%d'))
        initial_capital = float(request.get('initialCapital', 100000) or 100000)
        return strategy_id, symbols, start_date, end_date, initial_capital, strategy_params

    def run_backtest(self, request: Dict[str, Any]) -> Dict[str, Any]:
        """Execute a single Fincept Engine strategy backtest."""
        try:
            strategy_id, symbols, start_date, end_date, initial_capital, strategy_params = (
                self._extract_request(request))

            if not strategy_id:
                return {'success': False, 'error': 'strategy.type (strategy ID) is required'}
            if not symbols:
                return {'success': False, 'error': 'No symbols specified'}

            return self._execute_one(strategy_id, symbols, start_date, end_date,
                                     initial_capital, strategy_params)
        except Exception as e:
            import traceback
            tb = traceback.format_exc()
            print(f'[Fincept Engine] Error: {e}', file=sys.stderr)
            print(tb, file=sys.stderr)
            return {'success': False, 'error': str(e), 'traceback': tb}

    # ------------------------------------------------------------------
    # Optimize: grid or random search over user-supplied param ranges.
    # Surfaces winning iteration's data at `data.performance` so the screen's
    # display_result() renders, plus `data.optimization` block.
    # ------------------------------------------------------------------
    def optimize(self, request: Dict[str, Any]) -> Dict[str, Any]:
        try:
            import itertools
            import random

            strategy_id, symbols, start_date, end_date, initial_capital, strategy_params = (
                self._extract_request(request))
            if not strategy_id:
                return {'success': False, 'error': 'strategy.type (strategy ID) is required'}
            if not symbols:
                return {'success': False, 'error': 'No symbols specified'}

            # Frontend canonical names (fall back to legacy).
            param_ranges = request.get('paramRanges') or request.get('parameters') or {}
            objective = request.get('optimizeObjective') or request.get('objective', 'sharpe')
            method = request.get('optimizeMethod') or request.get('method', 'grid')
            max_iter = int(request.get('maxIterations', 50) or 50)

            if not param_ranges:
                return {'success': False, 'error': 'No parameter ranges supplied (expected paramRanges)'}

            # Build per-key value lists from {min, max, step} specs.
            def _native(x):
                try:
                    fx = float(x)
                    return int(fx) if fx.is_integer() else fx
                except (TypeError, ValueError):
                    return x

            param_values: Dict[str, List[Any]] = {}
            for key, spec in param_ranges.items():
                if isinstance(spec, dict):
                    mn = _native(spec.get('min', 0))
                    mx = _native(spec.get('max', 100))
                    step = _native(spec.get('step', 1)) or 1
                    vals = []
                    cur = mn
                    while cur <= mx:
                        vals.append(_native(cur))
                        cur = _native(cur + step)
                    param_values[key] = vals or [mn]
                elif isinstance(spec, list):
                    param_values[key] = [_native(v) for v in spec]
                else:
                    param_values[key] = [_native(spec)]

            keys = list(param_values.keys())
            value_lists = [param_values[k] for k in keys]
            if method == 'random':
                combos = []
                for _ in range(max_iter):
                    combos.append({k: random.choice(param_values[k]) for k in keys})
            else:  # grid
                combos = [dict(zip(keys, c)) for c in itertools.product(*value_lists)]
                if len(combos) > max_iter:
                    random.shuffle(combos)
                    combos = combos[:max_iter]

            if not combos:
                return {'success': False, 'error': 'paramRanges produced zero combinations'}

            metric_key = {
                'sharpe':  'sharpe_ratio',
                'sortino': 'sortino_ratio',
                'calmar':  'calmar_ratio',
                'return':  'total_return',
            }.get(objective, 'sharpe_ratio')

            best_score = -math.inf
            best_combo: Dict[str, Any] = {}
            best_result: Optional[Dict[str, Any]] = None
            all_results: List[Dict[str, Any]] = []

            for i, combo in enumerate(combos):
                merged_params = {**strategy_params, **combo}
                result = self._execute_one(
                    strategy_id, symbols, start_date, end_date,
                    initial_capital, merged_params,
                )
                if not result.get('success'):
                    all_results.append({'parameters': combo, 'score': None, 'error': result.get('error')})
                    continue
                perf = result['data']['performance']
                score = _safe_float(perf.get(metric_key, 0))
                all_results.append({
                    'parameters': combo,
                    'score': score,
                    'total_return': perf.get('total_return', 0),
                    'sharpe_ratio': perf.get('sharpe_ratio', 0),
                })
                if score > best_score:
                    best_score = score
                    best_combo = combo
                    best_result = result

            if best_result is None:
                return {'success': False, 'error': 'All optimization iterations failed'}

            data = best_result['data']
            data['optimization'] = {
                'objective': objective,
                'method': method,
                'objective_value': float(best_score) if best_score != -math.inf else 0.0,
                'best_params': best_combo,
                'iterations': len(combos),
                'all_results': all_results[:100],
            }
            # The screen's OPTIMIZE view reads these keys at the TOP level (every other
            # provider emits them there); they only existed nested under `optimization`, so
            # the summary cards, best-parameter cards and results table all rendered empty.
            data['iterations'] = len(combos)
            data['total_combinations'] = len(combos)
            data['method'] = method
            data['objective'] = objective
            data['best_objective_value'] = float(best_score) if best_score != -math.inf else 0.0
            data['best_parameters'] = best_combo
            data['best_performance'] = data.get('performance', {})
            data['all_results'] = [
                {
                    'parameters': r.get('parameters', {}),
                    'objective_value': r.get('score'),
                    'performance': {
                        'total_return': r.get('total_return'),
                        'sharpe_ratio': r.get('sharpe_ratio'),
                    },
                }
                for r in all_results[:100] if r.get('score') is not None
            ]
            return {'success': True, 'message': 'Optimization completed', 'data': data}

        except Exception as e:
            import traceback
            tb = traceback.format_exc()
            print(f'[Fincept Engine] Optimize error: {e}', file=sys.stderr)
            print(tb, file=sys.stderr)
            return {'success': False, 'error': str(e), 'traceback': tb}

    # ------------------------------------------------------------------
    # Walk-forward: run the supplied params on N rolling test windows,
    # surface the last fold's result + per-fold detail.
    # ------------------------------------------------------------------
    def walk_forward(self, request: Dict[str, Any]) -> Dict[str, Any]:
        try:
            strategy_id, symbols, start_date, end_date, initial_capital, strategy_params = (
                self._extract_request(request))
            if not strategy_id:
                return {'success': False, 'error': 'strategy.type (strategy ID) is required'}
            if not symbols:
                return {'success': False, 'error': 'No symbols specified'}

            n_splits = int(request.get('wfSplits', request.get('nSplits', 5)) or 5)
            train_ratio = float(request.get('wfTrainRatio', request.get('trainRatio', 0.7)) or 0.7)
            if not (1 <= n_splits <= 20):
                return {'success': False, 'error': f'wfSplits must be between 1 and 20 (got {n_splits})'}
            if not (0.1 <= train_ratio <= 0.95):
                return {'success': False, 'error': f'wfTrainRatio must be between 0.1 and 0.95 (got {train_ratio})'}

            sd = datetime.strptime(start_date, '%Y-%m-%d')
            ed = datetime.strptime(end_date, '%Y-%m-%d')
            total_days = (ed - sd).days
            split_days = total_days // n_splits
            if split_days < 5:
                return {'success': False,
                        'error': f'Date range too short for {n_splits} folds ({total_days} days)'}

            folds: List[Dict[str, Any]] = []
            last_result: Optional[Dict[str, Any]] = None

            for i in range(n_splits):
                fold_start = sd + timedelta(days=i * split_days)
                fold_end = fold_start + timedelta(days=split_days)
                train_end = fold_start + timedelta(days=int(split_days * train_ratio))
                test_start = train_end
                test_end = fold_end

                # Fincept strategies are not parameterised per fold (no per-fold
                # optimize step) — this measures parameter robustness across
                # different time windows.
                result = self._execute_one(
                    strategy_id, symbols,
                    test_start.strftime('%Y-%m-%d'),
                    test_end.strftime('%Y-%m-%d'),
                    initial_capital, strategy_params,
                )
                if not result.get('success'):
                    folds.append({
                        'fold': i,
                        'trainStart': fold_start.strftime('%Y-%m-%d'),
                        'trainEnd': train_end.strftime('%Y-%m-%d'),
                        'testStart': test_start.strftime('%Y-%m-%d'),
                        'testEnd': test_end.strftime('%Y-%m-%d'),
                        'error': result.get('error'),
                        'testReturn': 0.0,
                        'testSharpe': 0.0,
                        'testMaxDrawdown': 0.0,
                    })
                    continue
                last_result = result
                perf = result['data']['performance']
                folds.append({
                    'fold': i,
                    'trainStart': fold_start.strftime('%Y-%m-%d'),
                    'trainEnd': train_end.strftime('%Y-%m-%d'),
                    'testStart': test_start.strftime('%Y-%m-%d'),
                    'testEnd': test_end.strftime('%Y-%m-%d'),
                    'testReturn': _safe_float(perf.get('total_return')),
                    'testSharpe': _safe_float(perf.get('sharpe_ratio')),
                    'testMaxDrawdown': _safe_float(perf.get('max_drawdown')),
                })

            if last_result is None or not folds:
                return {'success': False, 'error': 'All walk-forward splits failed'}

            data = last_result['data']
            ok_folds = [f for f in folds if 'error' not in f]
            # Top-level keys the WALK-FORWARD view reads (same names the other providers use).
            oos_returns = [f['testReturn'] for f in ok_folds]
            oos_sharpes = [f['testSharpe'] for f in ok_folds]
            n_ok = len(ok_folds)
            mean_ret = (sum(oos_returns) / n_ok) if n_ok else 0.0
            data['n_windows'] = n_ok
            data['avg_oos_return'] = mean_ret
            data['avg_oos_sharpe'] = (sum(oos_sharpes) / n_ok) if n_ok else 0.0
            data['oos_return_std'] = (
                math.sqrt(sum((r - mean_ret) ** 2 for r in oos_returns) / (n_ok - 1)) if n_ok > 1 else 0.0)
            data['robustness_score'] = (sum(1 for r in oos_returns if r > 0) / n_ok) if n_ok else 0.0
            data['windows'] = [
                {
                    'window': f['fold'] + 1,
                    'train_start': f['trainStart'], 'train_end': f['trainEnd'],
                    'test_start': f['testStart'], 'test_end': f['testEnd'],
                    'out_of_sample_return': f['testReturn'],
                    'out_of_sample_sharpe': f['testSharpe'],
                }
                for f in ok_folds
            ]
            data['walk_forward'] = {
                'n_splits': n_splits,
                'train_ratio': train_ratio,
                'folds': folds,
                'mean_test_return': (sum(f['testReturn'] for f in ok_folds) / len(ok_folds)) if ok_folds else 0.0,
                'mean_test_sharpe': (sum(f['testSharpe'] for f in ok_folds) / len(ok_folds)) if ok_folds else 0.0,
            }
            return {'success': True, 'message': 'Walk-forward completed', 'data': data}

        except Exception as e:
            import traceback
            tb = traceback.format_exc()
            print(f'[Fincept Engine] Walk-forward error: {e}', file=sys.stderr)
            print(tb, file=sys.stderr)
            return {'success': False, 'error': str(e), 'traceback': tb}


def main():
    """CLI entry point. Stdout is reserved for the final JSON; everything
    diagnostic must go to stderr (see _log)."""
    if len(sys.argv) < 3:
        print(json_response({
            'success': False,
            'error': 'Usage: fincept_provider.py <command> <args_json>',
        }))
        sys.exit(1)

    command = sys.argv[1]
    args_json = sys.argv[2]

    try:
        args = parse_json_input(args_json)
    except Exception as e:
        print(json_response({
            'success': False,
            'error': f'Invalid JSON: {e}',
        }))
        sys.exit(1)

    provider = FinceptProvider()

    if command in ('run_backtest', 'execute_fincept_strategy'):
        result = provider.run_backtest(args)
    elif command == 'optimize':
        result = provider.optimize(args)
    elif command == 'walk_forward':
        result = provider.walk_forward(args)
    elif command == 'get_strategies':
        result = provider.get_strategies(args)
    elif command == 'get_indicators':
        result = provider.get_indicators(args)
    elif command == 'get_command_options':
        result = provider.get_command_options(args)
    else:
        result = {'success': False, 'error': f'Unknown command: {command}'}

    # json_response handles camelCase conversion + NaN/Inf sanitisation,
    # matching every other provider's wire format.
    print(json_response(result))


if __name__ == '__main__':
    main()
