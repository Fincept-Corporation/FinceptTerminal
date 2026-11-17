"""
GS-Quant Wrapper Worker Handler
================================
Dispatch router for C++ commands -> Python gs_quant_wrapper operations.
Pattern: main(args) dispatches [operation, json_data].

Response contract (all ops):
    success: bool
    operation: str
    data: dict   (when success)
    error: str   (when failure)
    error_kind: str | None    (validation | runtime | unknown_op)
    warning: str              (optional; the panel shows it as a banner)
Tracebacks go to stderr. Undefined values are null, never 0.0.
Everything here is plain numpy/pandas/scipy; the gs_quant package is not needed
for any operation (risk_analytics imports it lazily, only for scenario objects).
"""

import sys
import os
import json
import traceback
import math
import numpy as np
import pandas as pd
from datetime import datetime

# Add parent directory to path for absolute imports when run as script
_script_dir = os.path.dirname(os.path.abspath(__file__))
_parent_dir = os.path.dirname(_script_dir)
if _parent_dir not in sys.path:
    sys.path.insert(0, _parent_dir)

try:
    import fincept_json
except ImportError:  # run by hand without PYTHONPATH=scripts
    sys.path.append(os.path.dirname(_parent_dir))
    import fincept_json


# ============================================================================
# INPUT COERCION + VALIDATION
# ============================================================================

class ValidationError(ValueError):
    """Raised when an op's input doesn't pass coercion/length checks."""
    pass


def _coerce_floats(value, field_name):
    """
    Accept list[number], tuple[number], np.ndarray, or a CSV / whitespace-separated string.
    Returns list[float]. Empty / None -> [].
    """
    if value is None:
        return []
    if isinstance(value, (list, tuple)):
        out = []
        for i, v in enumerate(value):
            try:
                out.append(float(v))
            except (TypeError, ValueError):
                raise ValidationError(
                    f"{field_name}[{i}] is not numeric: {v!r}")
        return out
    if isinstance(value, np.ndarray):
        return [float(v) for v in value.tolist()]
    if isinstance(value, str):
        s = value.strip()
        if not s:
            return []
        # Accept comma-, semicolon-, whitespace-, or newline-separated
        for sep in [",", ";", "\t", "\n"]:
            s = s.replace(sep, " ")
        parts = [p for p in s.split(" ") if p.strip()]
        out = []
        for i, p in enumerate(parts):
            try:
                out.append(float(p))
            except ValueError:
                raise ValidationError(
                    f"{field_name}[{i}] is not numeric: {p!r}")
        return out
    # Single scalar
    try:
        return [float(value)]
    except (TypeError, ValueError):
        raise ValidationError(f"{field_name} is not numeric: {value!r}")


def _require_min_length(values, n, field_name):
    if len(values) < n:
        raise ValidationError(
            f"{field_name} needs at least {n} values, got {len(values)}")


def _series_from_list(values, dates=None, name="series"):
    """Helper: Convert list of values (+ optional dates) to pd.Series of floats."""
    if dates:
        idx = pd.to_datetime(dates)
    else:
        idx = pd.date_range("2020-01-01", periods=len(values), freq="B")
    return pd.Series(values, index=idx, name=name, dtype=float)


def _df_from_dict(data_dict, dates=None):
    """Helper: Convert dict-of-lists to pd.DataFrame of floats, indexed by date."""
    if dates:
        idx = pd.to_datetime(dates)
    else:
        first_key = next(iter(data_dict))
        idx = pd.date_range("2020-01-01", periods=len(data_dict[first_key]), freq="B")
    # Build column-by-column with positional float arrays (avoid pandas
    # auto-aligning to a default RangeIndex which would zero everything out).
    df = pd.DataFrame(index=idx)
    for k, v in data_dict.items():
        arr = np.asarray(v, dtype=float)
        if len(arr) != len(idx):
            raise ValidationError(
                f"prices[{k}] length {len(arr)} != index length {len(idx)}")
        df[k] = arr
    return df


def _safe_float(v, default=None):
    """Convert a value to float; NaN / inf / non-numeric -> `default` (None = JSON null).

    Undefined results (a Sharpe ratio with zero volatility, an Omega ratio with no
    losing period) must reach the panel as null, not as a real-looking 0.0."""
    try:
        f = float(v)
    except (TypeError, ValueError):
        return default
    if math.isnan(f) or math.isinf(f):
        return default
    return f


def _safe_int(v, default=0):
    try:
        f = float(v)
        if math.isnan(f) or math.isinf(f):
            return default
        return int(f)
    except (TypeError, ValueError):
        return default


# ============================================================================
# OPERATION HANDLERS
# ============================================================================

def op_risk_metrics(data):
    """Comprehensive risk metrics: volatility, VaR, CVaR, drawdown, ratios."""
    from gs_quant_wrapper import ts_risk_measures as risk

    returns_list = _coerce_floats(data.get("returns"), "returns")
    _require_min_length(returns_list, 5, "returns")
    risk_free_rate = _safe_float(data.get("risk_free_rate"), 0.0)
    dates = data.get("dates")
    ret = _series_from_list(returns_list, dates, "returns")

    vol = risk.volatility(ret)
    dd = risk.max_drawdown(ret)
    sharpe = risk.sharpe_ratio(ret, risk_free_rate)
    sortino_val = risk.sortino_ratio(ret, risk_free_rate)
    calmar_val = risk.calmar_ratio(ret)
    omega_val = risk.omega_ratio(ret)
    var_95 = risk.value_at_risk(ret, 0.95)
    var_99 = risk.value_at_risk(ret, 0.99)
    dside = risk.downside_risk(ret)
    daily_r = risk.daily_risk(ret)
    annual_r = risk.annual_risk(ret)
    dl = risk.drawdown_length(ret)
    rec = risk.recovery_periods(ret)

    def _when(stamp):
        """Drawdown markers as dates when the caller supplied dates, otherwise as
        observation numbers. Without dates the series is indexed on a synthetic
        business-day calendar starting 2020-01-01, and reporting those invented
        dates ("peak 2020-01-01") reads as real history."""
        if stamp is None:
            return None
        if dates:
            return str(stamp)
        try:
            return f"obs {int(ret.index.get_loc(stamp))}"
        except Exception:
            return None

    return {
        "n_observations": len(ret),
        "volatility_annualized": _safe_float(vol),
        "daily_risk": _safe_float(daily_r),
        "annual_risk": _safe_float(annual_r),
        "downside_risk": _safe_float(dside),
        "max_drawdown": _safe_float(dd.get("max_drawdown")),
        "max_drawdown_pct": _safe_float(dd.get("max_drawdown_pct")),
        "peak_date": _when(dd.get("peak_date")),
        "trough_date": _when(dd.get("trough_date")),
        "recovery_date": _when(dd.get("recovery_date")),
        "max_drawdown_length": _safe_int(dl.max() if hasattr(dl, "max") else dl),
        # Trough -> first new high, over drawdowns that recovered; null when none did
        # (the old value was the drawdown length again).
        "max_recovery_period": rec["max_recovery_period"],
        "n_recovered_drawdowns": rec["n_recovered_drawdowns"],
        "ongoing_drawdown_periods_since_trough": rec["ongoing_drawdown_periods_since_trough"],
        "sharpe_ratio": _safe_float(sharpe),
        "sortino_ratio": _safe_float(sortino_val),
        "calmar_ratio": _safe_float(calmar_val),
        # Infinite when there is no losing period -> null (undefined), not 0.0.
        "omega_ratio": _safe_float(omega_val),
        "var_95": _safe_float(var_95),
        "var_99": _safe_float(var_99),
    }


def op_portfolio_analytics(data):
    """Portfolio analytics with benchmark comparison."""
    from gs_quant_wrapper import ts_portfolio_analytics as port

    returns_list = _coerce_floats(data.get("returns"), "returns")
    benchmark_list = _coerce_floats(data.get("benchmark_returns"), "benchmark_returns")
    _require_min_length(returns_list, 5, "returns")
    _require_min_length(benchmark_list, 5, "benchmark_returns")
    if len(returns_list) != len(benchmark_list):
        raise ValidationError(
            f"returns ({len(returns_list)}) and benchmark_returns ({len(benchmark_list)}) "
            f"must have the same length")

    risk_free_rate = _safe_float(data.get("risk_free_rate"), 0.0)
    dates = data.get("dates")

    port_ret = _series_from_list(returns_list, dates, "portfolio")
    bench_ret = _series_from_list(benchmark_list, dates, "benchmark")

    result = {
        "n_observations": len(port_ret),
        "pnl_final": _safe_float(port.portfolio_pnl(port_ret, 1.0).iloc[-1]),
        "alpha": _safe_float(port.portfolio_alpha(port_ret, bench_ret, risk_free_rate)),
        "annual_risk": _safe_float(port.portfolio_annual_risk(port_ret)),
        "sharpe_ratio": _safe_float(port.portfolio_sharpe_ratio(port_ret, risk_free_rate)),
        "sortino_ratio": _safe_float(port.portfolio_sortino_ratio(port_ret, risk_free_rate)),
        "calmar_ratio": _safe_float(port.portfolio_calmar_ratio(port_ret)),
        "treynor_measure": _safe_float(port.portfolio_treynor_measure(port_ret, bench_ret, risk_free_rate)),
        "modigliani_ratio": _safe_float(port.portfolio_modigliani_ratio(port_ret, bench_ret, risk_free_rate)),
        "max_drawdown": _safe_float(port.portfolio_max_drawdown(port_ret)),
        "drawdown_length": _safe_int(port.portfolio_drawdown_length(port_ret)),
        "tracking_error": _safe_float(port.portfolio_tracking_error(port_ret, bench_ret)),
        "information_ratio": _safe_float(port.portfolio_information_ratio(port_ret, bench_ret)),
        "hit_rate": _safe_float(port.portfolio_hit_rate(port_ret)),
        "r_squared": _safe_float(port.portfolio_r_squared(port_ret, bench_ret)),
        "skewness": _safe_float(port.portfolio_skewness(port_ret)),
        "kurtosis": _safe_float(port.portfolio_kurtosis(port_ret)),
        "jensen_alpha": _safe_float(port.portfolio_jensen_alpha(port_ret, bench_ret, risk_free_rate)),
        "jensen_alpha_bull": _safe_float(port.portfolio_jensen_alpha_bull(port_ret, bench_ret, risk_free_rate)),
        "jensen_alpha_bear": _safe_float(port.portfolio_jensen_alpha_bear(port_ret, bench_ret, risk_free_rate)),
    }

    capture = port.portfolio_capture_ratio(port_ret, bench_ret)
    result["up_capture"] = _safe_float(capture.get("up_capture"))
    result["down_capture"] = _safe_float(capture.get("down_capture"))
    result["capture_ratio"] = _safe_float(capture.get("capture_ratio"))

    return result


def op_greeks(data):
    """Calculate Greeks for an option (Black-Scholes-Merton; dividend_yield optional, default 0)."""
    from gs_quant_wrapper.risk_analytics import RiskAnalytics, RiskConfig, GREEK_UNITS

    spot = _safe_float(data.get("spot"), 100.0)
    strike = _safe_float(data.get("strike"), 100.0)
    expiry = _safe_float(data.get("expiry"), 0.25)
    rate = _safe_float(data.get("rate"), 0.05)
    vol = _safe_float(data.get("vol"), 0.2)
    dividend_yield = _safe_float(data.get("dividend_yield"), 0.0)
    option_type = str(data.get("option_type", "call")).lower()

    if spot <= 0:
        raise ValidationError(f"spot must be > 0, got {spot}")
    if strike <= 0:
        raise ValidationError(f"strike must be > 0, got {strike}")
    if expiry <= 0:
        raise ValidationError(f"expiry must be > 0 (in years), got {expiry}")
    if vol <= 0:
        raise ValidationError(f"vol must be > 0, got {vol}")
    if option_type not in ("call", "put"):
        raise ValidationError(f"option_type must be 'call' or 'put', got {option_type!r}")

    risk = RiskAnalytics(RiskConfig())
    # SIGNATURE: calculate_all_greeks(option_type, spot, strike, time_to_expiry, volatility,
    #                                 risk_free_rate, dividend_yield)
    greeks = risk.calculate_all_greeks(option_type, spot, strike, expiry, vol, rate, dividend_yield)

    raw = greeks.__dict__ if hasattr(greeks, "__dict__") else dict(greeks)
    return {
        "spot": spot,
        "strike": strike,
        "expiry_years": expiry,
        "rate": rate,
        "vol": vol,
        "dividend_yield": dividend_yield,
        "option_type": option_type,
        "moneyness": _safe_float(spot / strike),
        "model": "black_scholes_merton",
        "greeks": {k: _safe_float(v) for k, v in raw.items()},
        "units": GREEK_UNITS,
    }


def op_var_analysis(data):
    """Value at Risk: parametric, historical, Monte Carlo, CVaR."""
    from gs_quant_wrapper.risk_analytics import RiskAnalytics, RiskConfig

    returns_list = _coerce_floats(data.get("returns"), "returns")
    _require_min_length(returns_list, 30, "returns")
    confidence = _safe_float(data.get("confidence"), 0.95)
    if not (0.0 < confidence < 1.0):
        raise ValidationError(f"confidence must be in (0, 1), got {confidence}")
    position_value = _safe_float(data.get("position_value"), 1_000_000.0)
    if position_value <= 0:
        raise ValidationError(f"position_value must be > 0, got {position_value}")
    dates = data.get("dates")

    ret = _series_from_list(returns_list, dates, "returns")
    risk = RiskAnalytics(RiskConfig())

    # Real signatures (verified via inspect):
    # calculate_parametric_var(portfolio_value, returns, confidence_level)
    # calculate_historical_var(portfolio_value, returns, confidence_level)
    # calculate_monte_carlo_var(portfolio_value, mean_return, std_return, confidence_level)
    # calculate_cvar(portfolio_value, returns, confidence_level)
    parametric = risk.calculate_parametric_var(position_value, ret, confidence)
    historical = risk.calculate_historical_var(position_value, ret, confidence)
    mc = risk.calculate_monte_carlo_var(position_value, float(ret.mean()),
                                        float(ret.std()), confidence)
    cvar = risk.calculate_cvar(position_value, ret, confidence)

    def _var_to_dict(v):
        raw = v.__dict__ if hasattr(v, "__dict__") else (
            v if isinstance(v, dict) else {"value": v})
        out = {}
        for k, val in raw.items():
            if isinstance(val, (int, float, np.floating, np.integer)):
                out[k] = _safe_float(val)
            else:
                out[k] = str(val)
        return out

    return {
        "n_observations": len(ret),
        "position_value": position_value,
        "confidence": confidence,
        "parametric_var": _var_to_dict(parametric),
        "historical_var": _var_to_dict(historical),
        "monte_carlo_var": _var_to_dict(mc),
        "cvar": _var_to_dict(cvar),
    }


def op_stress_test(data):
    """Standard market stress scenarios applied to a single position."""
    from gs_quant_wrapper.risk_analytics import RiskAnalytics, RiskConfig

    position_value = _safe_float(data.get("position_value"), 1_000_000.0)
    if position_value <= 0:
        raise ValidationError(f"position_value must be > 0, got {position_value}")

    # run_all_standard_scenarios classifies positions by substring on the asset
    # name ('equity', 'bond', 'fx', 'credit', 'commodity', 'vol'/'option') and
    # applies canned shocks (2008, COVID, etc.) to notional. We accept either:
    #   - explicit `positions` dict (asset_name -> notional), or
    #   - a `mix` dict of weights to split position_value across asset classes
    #     (default: balanced 60% equity / 30% bond / 10% commodity).
    positions = data.get("positions")
    if positions:
        positions = {str(k): _safe_float(v) for k, v in positions.items()}
    else:
        mix = data.get("mix") or {"equity": 0.60, "bond": 0.30, "commodity": 0.10}
        # Normalize weights to sum to 1
        total_w = sum(_safe_float(v) for v in mix.values()) or 1.0
        positions = {str(k): position_value * (_safe_float(v) / total_w) for k, v in mix.items()}

    risk_analytics = RiskAnalytics(RiskConfig())
    scenarios = risk_analytics.run_all_standard_scenarios(position_value, positions)

    serialized = []
    if isinstance(scenarios, list):
        for sc in scenarios:
            if not isinstance(sc, dict):
                serialized.append({"scenario": str(sc)})
                continue
            row = {}
            for k, v in sc.items():
                if isinstance(v, (int, float, np.floating, np.integer)):
                    row[k] = _safe_float(v)
                elif isinstance(v, dict):
                    row[k] = {kk: _safe_float(vv) if isinstance(vv, (int, float, np.floating, np.integer)) else str(vv)
                              for kk, vv in v.items()}
                else:
                    row[k] = str(v)
            serialized.append(row)
    elif isinstance(scenarios, dict):
        # Older shape: {scenario_name: {fields...}} -- normalize to list
        for name, sc in scenarios.items():
            row = {"scenario": name}
            if isinstance(sc, dict):
                for k, v in sc.items():
                    if isinstance(v, (int, float, np.floating, np.integer)):
                        row[k] = _safe_float(v)
                    else:
                        row[k] = str(v)
            else:
                row["value"] = str(sc)
            serialized.append(row)

    return {
        "position_value": position_value,
        "n_scenarios": len(serialized),
        "scenarios": serialized,
        "positions": positions,
        "assumptions": (
            "Hard-coded historical-style shocks applied to notional by asset class "
            "(matched on the position name: equity/stock, bond/fixed/rate, fx, credit, "
            "commodity, vol/option); bonds assume duration 7, credit assumes spread duration 5. "
            "Not a model of the actual holdings."),
    }


def _bt_buy_and_hold(prices, capital, commission):
    """Buy on day 0, hold to end. Returns (portfolio_series, trades_count)."""
    px = prices.iloc[:, 0]  # use first column
    initial_price = float(px.iloc[0])
    fee = capital * commission
    invested = capital - fee
    shares = invested / initial_price
    series = (shares * px).rename("portfolio_value")
    return series, 1


def _bt_momentum(prices, capital, commission, lookback):
    """Trailing-return momentum, checked daily, trading only when the signal changes.

    Signal at day i (known at the close of day i-1): trailing `lookback`-day return
    px[i-1] / px[i-1-lookback] - 1 per asset. Hold the asset with the best trailing
    return while that return is > 0; otherwise hold cash. With one ticker this is
    time-series momentum (in the asset when its trailing return is positive, in cash
    otherwise). Trades execute at day i's close; commission on each side.

    The previous version never exited on a negative signal and, on a positive one,
    sold and re-bought the same asset every day, paying 2x commission daily.
    """
    px = prices
    n = len(px)
    cols = list(px.columns)
    cash = float(capital)
    held = None
    shares = 0.0
    values = []
    trades = 0
    for i in range(n):
        row = px.iloc[i]
        if i >= lookback + 1:
            past = px.iloc[i - 1 - lookback]
            prev = px.iloc[i - 1]
            rets = (prev / past - 1.0).replace([np.inf, -np.inf], np.nan).dropna()
            target = None
            if not rets.empty and float(rets.max()) > 0:
                target = rets.idxmax()
            if target != held:
                if held is not None:
                    cash += shares * float(row[held]) * (1 - commission)
                    shares, held = 0.0, None
                    trades += 1
                if target is not None and float(row[target]) > 0:
                    shares = cash * (1 - commission) / float(row[target])
                    cash, held = 0.0, target
                    trades += 1
        mv = shares * float(row[held]) if held is not None else 0.0
        values.append((px.index[i], cash + mv))
    return pd.Series(dict(values), name="portfolio_value"), trades


def _bt_mean_reversion(prices, capital, commission, window):
    """Buy first ticker when it dips below MA-1*std, sell when above MA+1*std."""
    px = prices.iloc[:, 0]
    ma = px.rolling(window).mean()
    sd = px.rolling(window).std()
    cash = capital
    shares = 0.0
    values = []
    trades = 0
    for i in range(len(px)):
        date = px.index[i]
        price = float(px.iloc[i])
        if i >= window and not math.isnan(ma.iloc[i]):
            lower = ma.iloc[i] - sd.iloc[i]
            upper = ma.iloc[i] + sd.iloc[i]
            if shares == 0 and price < lower and cash > 0:
                spend = cash * (1 - commission)
                shares = spend / price
                cash = 0.0
                trades += 1
            elif shares > 0 and price > upper:
                cash = shares * price * (1 - commission)
                shares = 0.0
                trades += 1
        values.append((date, cash + shares * price))
    return pd.Series(dict(values), name="portfolio_value"), trades


def _bt_rebalancing(prices, capital, commission, weights, freq_days=21):
    """Hold target weights, rebalance every freq_days by trading only the drift.

    On a rebalance day each position is moved to target_weight x (book value after
    costs); commission is charged on the traded notional |target - current| only.
    The previous version sold and re-bought 100 % of the book on every rebalance and
    paid commission on both legs of the whole notional.
    """
    cols = list(prices.columns)
    w = {c: float(weights.get(c, 0.0)) for c in cols}
    total = sum(w.values()) or 1.0
    w = {c: v / total for c, v in w.items()}
    freq_days = max(1, int(freq_days or 1))
    cash = float(capital)
    shares = {c: 0.0 for c in cols}
    values = []
    trades = 0
    for i in range(len(prices)):
        date = prices.index[i]
        row = prices.iloc[i]
        if i % freq_days == 0:
            tradable = [c for c in cols if not math.isnan(row[c]) and row[c] > 0]
            book = cash + sum(shares[c] * float(row[c]) for c in tradable)
            current = {c: shares[c] * float(row[c]) for c in tradable}
            # Fee on traded notional; the targets are set on the post-fee book so the
            # fee is paid out of the trade itself: book' = book - fee, fee = k * sum|w*book' - cur|.
            # Solve by fixed point (converges in a few steps since k << 1).
            book_after = book
            for _ in range(20):
                fee = commission * sum(abs(w[c] * book_after - current[c]) for c in tradable)
                nxt = book - fee
                if abs(nxt - book_after) < 1e-9:
                    break
                book_after = nxt
            for c in tradable:
                target_val = w[c] * book_after
                if abs(target_val - current[c]) > 1e-9 * max(1.0, book):
                    trades += 1
                shares[c] = target_val / float(row[c])
            cash = book_after - sum(w[c] * book_after for c in tradable)
        mv = sum(shares[c] * float(row[c]) for c in cols if not math.isnan(row[c]))
        values.append((date, cash + mv))
    return pd.Series(dict(values), name="portfolio_value"), trades


def _curve_metrics(curve, initial_capital):
    """Compute summary metrics from an equity curve series."""
    if len(curve) < 2:
        return {}
    final_value = float(curve.iloc[-1])
    total_return = final_value / initial_capital - 1.0
    daily_rets = curve.pct_change(fill_method=None).dropna()
    n_days = len(curve)
    years = max(n_days / 252.0, 1e-6)
    annualized_return = (1.0 + total_return) ** (1.0 / years) - 1.0 if final_value > 0 else -1.0
    vol = float(daily_rets.std() * math.sqrt(252)) if len(daily_rets) > 1 else None
    sharpe = (float(daily_rets.mean() / daily_rets.std() * math.sqrt(252))
              if len(daily_rets) > 1 and daily_rets.std() > 0 else None)
    # Drawdown
    peak = curve.cummax()
    dd = (curve - peak) / peak
    max_dd = float(dd.min())
    # Best / worst day
    best = float(daily_rets.max()) if len(daily_rets) else None
    worst = float(daily_rets.min()) if len(daily_rets) else None
    win_rate = float((daily_rets > 0).mean() * 100) if len(daily_rets) else None
    return {
        "final_value": _safe_float(final_value),
        "total_return": _safe_float(total_return),
        "annualized_return": _safe_float(annualized_return),
        "volatility": _safe_float(vol),
        "sharpe_ratio": _safe_float(sharpe),
        "max_drawdown": _safe_float(max_dd),
        "best_day": _safe_float(best),
        "worst_day": _safe_float(worst),
        "win_rate_pct": _safe_float(win_rate),
        "n_days": int(n_days),
    }


def op_backtest(data):
    """Run a simple strategy backtest on price history."""
    strategy = str(data.get("strategy", "buy_and_hold")).lower()
    # Normalize C++ aliases to canonical names
    aliases = {"buy_hold": "buy_and_hold", "buy-and-hold": "buy_and_hold",
               "meanreversion": "mean_reversion", "mean-reversion": "mean_reversion"}
    strategy = aliases.get(strategy, strategy)

    initial_capital = _safe_float(data.get("initial_capital"), 100_000.0)
    if initial_capital <= 0:
        raise ValidationError(f"initial_capital must be > 0, got {initial_capital}")
    commission = _safe_float(data.get("commission"), 0.001)
    lookback = _safe_int(data.get("lookback", 20))
    rebalance_freq = data.get("rebalance_freq", "monthly")
    dates = data.get("dates")

    # ── Build the price DataFrame ────────────────────────────────────────────
    prices_input = data.get("prices")
    ticker = str(data.get("ticker", "")).strip().upper()

    if isinstance(prices_input, dict) and prices_input:
        # Already a dict of ticker -> list[float]
        cleaned = {k: _coerce_floats(v, f"prices[{k}]") for k, v in prices_input.items()}
        df = _df_from_dict(cleaned, dates)
    elif isinstance(prices_input, (list, tuple)) and prices_input:
        prices_list = _coerce_floats(prices_input, "prices")
        col = ticker or "ASSET"
        df = pd.DataFrame({col: prices_list},
                          index=pd.date_range("2020-01-01", periods=len(prices_list), freq="B"))
    elif ticker:
        # Fetch from yfinance as a convenience for the UI
        try:
            import fincept_yahoo as fy  # shared Yahoo session + on-disk bar cache
        except ImportError:
            raise ValidationError(
                "ticker provided but neither 'prices' supplied nor yfinance available")
        period = str(data.get("period", "2y"))
        hist = fy.ticker_history(ticker, period=period, auto_adjust=True)
        if hist.empty or "Close" not in hist.columns:
            raise ValidationError(
                f"yfinance returned no price history for ticker {ticker!r} (period={period})")
        df = pd.DataFrame({ticker: hist["Close"].astype(float).values},
                          index=hist.index.tz_localize(None) if hist.index.tz is not None else hist.index)
    else:
        raise ValidationError(
            "backtest needs either 'prices' (list or dict) or a 'ticker' to fetch")

    if len(df) < max(lookback + 5, 30):
        raise ValidationError(
            f"need at least {max(lookback + 5, 30)} price observations, got {len(df)}")

    # Pick the active ticker for single-asset strategies
    if ticker and ticker in df.columns:
        single_df = df[[ticker]]
    else:
        single_df = df.iloc[:, [0]]
        ticker = single_df.columns[0]

    if strategy == "buy_and_hold":
        curve, trades_count = _bt_buy_and_hold(single_df, initial_capital, commission)
        active = [ticker]
    elif strategy == "momentum":
        curve, trades_count = _bt_momentum(df, initial_capital, commission, lookback)
        active = list(df.columns)
    elif strategy == "mean_reversion":
        curve, trades_count = _bt_mean_reversion(single_df, initial_capital, commission, lookback)
        active = [ticker]
    elif strategy == "rebalancing":
        weights = data.get("weights")
        if not weights:
            n = len(df.columns)
            weights = {col: 1.0 / n for col in df.columns}
        rebal_days = _safe_int(data.get("rebalance_days", 21))
        curve, trades_count = _bt_rebalancing(df, initial_capital, commission, weights, rebal_days)
        active = list(df.columns)
    else:
        raise ValidationError(
            f"unknown strategy {strategy!r}; expected one of: "
            "buy_and_hold, momentum, mean_reversion, rebalancing")

    # Sample curve to ~250 points for the wire
    step = max(1, len(curve) // 250)
    equity_curve = [
        {"date": str(d)[:10], "value": _safe_float(v)}
        for i, (d, v) in enumerate(curve.items()) if i % step == 0
    ]
    # Always include the very last point
    if equity_curve and equity_curve[-1]["date"] != str(curve.index[-1])[:10]:
        equity_curve.append({"date": str(curve.index[-1])[:10],
                             "value": _safe_float(curve.iloc[-1])})

    metrics = _curve_metrics(curve, initial_capital)
    metrics["initial_capital"] = _safe_float(initial_capital)
    metrics["num_trades"] = int(trades_count)

    return {
        "strategy": strategy,
        "ticker": ticker,
        "tickers": active,
        "initial_capital": initial_capital,
        "n_observations": int(len(df)),
        "start_date": str(df.index[0])[:10],
        "end_date": str(df.index[-1])[:10],
        "metrics": metrics,
        "equity_curve": equity_curve,
    }


def op_statistics(data):
    """Descriptive statistics of a single series.

    Optional `series_type`: "auto" (default) | "prices" | "returns". It only decides
    the basis of `realized_variance`, which is a returns concept: for prices it is the
    sum of squared log returns of the values, for returns the sum of squared values.
    "auto" treats a strictly positive series as prices. `semi_variance` is the
    below-MEAN semivariance of the values (mean of min(x - mean, 0)^2), which is
    meaningful for any series and sits next to `variance` on the same scale.
    """
    from gs_quant_wrapper import ts_math_statistics as ms

    values = _coerce_floats(data.get("values"), "values")
    _require_min_length(values, 2, "values")
    dates = data.get("dates")
    series = _series_from_list(values, dates, "data")
    series_type = str(data.get("series_type", "auto")).lower()
    if series_type not in ("auto", "prices", "returns"):
        raise ValidationError(f"series_type must be auto, prices or returns, got {series_type!r}")

    arr = series.to_numpy(dtype=float)
    as_prices = series_type == "prices" or (series_type == "auto" and bool((arr > 0).all()))
    if as_prices:
        if not (arr > 0).all():
            raise ValidationError("series_type 'prices' needs strictly positive values")
        log_rets = np.diff(np.log(arr))
        realized_variance = float(np.sum(log_rets ** 2))
        realized_basis = "sum of squared log returns of the values (series treated as prices)"
    else:
        realized_variance = float(np.sum(arr ** 2))
        realized_basis = "sum of squared values (series treated as returns)"
    semi_variance = ms.semi_variance(series, threshold=float(series.mean()))

    pcts = ms.percentiles(series)

    return {
        "n_observations": len(series),
        "mean": _safe_float(ms.mean(series)),
        "median": _safe_float(ms.median(series)),
        "std": _safe_float(ms.std(series)),
        "variance": _safe_float(ms.var(series)),
        "min": _safe_float(ms.min_(series)),
        "max": _safe_float(ms.max_(series)),
        "range": _safe_float(ms.range_(series)),
        "skewness": _safe_float(ms.skewness(series)),
        "kurtosis": _safe_float(ms.kurtosis(series)),
        "count": _safe_int(ms.count(series)),
        "sum": _safe_float(ms.sum_(series)),
        "semi_variance": _safe_float(semi_variance),
        "semi_variance_threshold": "mean",
        "realized_variance": _safe_float(realized_variance),
        "realized_variance_basis": realized_basis,
        "series_type_used": "prices" if as_prices else "returns",
        "percentiles": {str(_safe_int(k)): _safe_float(v) for k, v in pcts.items()},
    }


def _returns_frame(data):
    """`returns` {asset: [r...]} (same length) or `tickers` CSV + `period` (yfinance
    daily closes -> simple returns). Returns a DataFrame (rows = periods)."""
    returns = data.get("returns")
    tickers = data.get("tickers")
    if isinstance(returns, dict) and returns:
        cleaned = {str(k): _coerce_floats(v, f"returns[{k}]") for k, v in returns.items()}
        lengths = {len(v) for v in cleaned.values()}
        if len(lengths) != 1:
            raise ValidationError(
                f"all return series must have the same length, got {sorted(lengths)}")
        df = pd.DataFrame(cleaned)
    elif tickers:
        try:
            import fincept_yahoo as fy  # shared Yahoo session + on-disk bar cache
        except ImportError:
            raise ValidationError("yfinance is not installed; send `returns` instead of `tickers`")
        tk = [t.strip().upper() for t in str(tickers).split(",") if t.strip()]
        if len(tk) < 2:
            raise ValidationError("component VaR needs at least 2 tickers")
        period = str(data.get("period", "1y"))
        raw = fy.download(tk, period=period, auto_adjust=True)
        if raw is None or raw.empty:
            raise ValidationError(f"yfinance returned no data for {','.join(tk)} (period={period})")
        close = raw["Close"] if "Close" in raw.columns.get_level_values(0) else raw
        if isinstance(close, pd.Series):
            close = close.to_frame(name=tk[0])
        missing = [t for t in tk if t not in close.columns or close[t].dropna().empty]
        if missing:
            raise ValidationError(f"no price history for: {', '.join(missing)}")
        df = close[tk].pct_change(fill_method=None)
    else:
        raise ValidationError("component_var needs `returns` ({asset: [returns...]}) or `tickers`")
    df = df.apply(pd.to_numeric, errors="coerce").dropna(how="any")
    if df.shape[1] < 2:
        raise ValidationError(f"component VaR needs at least 2 assets, got {df.shape[1]}")
    if len(df) < 30:
        raise ValidationError(f"returns need at least 30 complete observations per asset, got {len(df)}")
    return df


def op_component_var(data):
    """Parametric (delta-normal) component VaR of a multi-asset portfolio.

    Inputs:
      returns        : {asset: [daily returns...]} (same length, >= 30)   -- or --
      tickers/period : "AAPL,MSFT,..." / "1y" (fetched with yfinance)
      weights        : [w...] in asset order, or {asset: w}; default equal weights.
                       Normalised to sum to 1 when they do not.
      position_value : float (default 1e6)
      confidence     : float in (0, 1) (default 0.95)
      horizon_days   : int (default 1) -- sqrt-of-time scaling of the per-period covariance
    Zero-mean delta-normal VaR on the sample covariance of the returns as given
    (per period); component VaRs sum to the portfolio VaR (Euler allocation).
    """
    from gs_quant_wrapper.risk_analytics import RiskAnalytics, RiskConfig

    df = _returns_frame(data)
    assets = list(df.columns)
    n = len(assets)

    raw_w = data.get("weights")
    if raw_w is None or raw_w == [] or raw_w == {}:
        w = np.full(n, 1.0 / n)
    elif isinstance(raw_w, dict):
        # Tickers are upper-cased when fetched, so match weight keys without case.
        by_upper = {str(k).strip().upper(): v for k, v in raw_w.items()}
        known = {str(a).upper() for a in assets}
        unknown = [k for k in by_upper if k not in known]
        if unknown:
            raise ValidationError(f"weights given for unknown assets: {', '.join(unknown)}")
        w = np.asarray([_safe_float(by_upper.get(str(a).upper()), 0.0) for a in assets], dtype=float)
    else:
        w = np.asarray(_coerce_floats(raw_w, "weights"), dtype=float)
        if len(w) != n:
            raise ValidationError(f"weights ({len(w)}) must match the number of assets ({n})")
    total = float(w.sum())
    if abs(total) < 1e-12:
        raise ValidationError("weights sum to zero; cannot express positions as fractions of position_value")
    normalised = abs(total - 1.0) > 1e-9
    if normalised:
        w = w / total

    confidence = _safe_float(data.get("confidence"), 0.95)
    if not (0.0 < confidence < 1.0):
        raise ValidationError(f"confidence must be in (0, 1), got {confidence}")
    position_value = _safe_float(data.get("position_value"), 1_000_000.0)
    if position_value <= 0:
        raise ValidationError(f"position_value must be > 0, got {position_value}")
    horizon = max(1, _safe_int(data.get("horizon_days"), 1))

    risk = RiskAnalytics(RiskConfig())
    try:
        res = risk.calculate_component_var(position_value, w, df, confidence, horizon)
    except ValueError as e:
        raise ValidationError(str(e))

    z = res["z_score"]
    stdev = np.sqrt(np.diag(np.atleast_2d(df.cov().to_numpy(dtype=float))))
    standalone = z * stdev * np.sqrt(horizon) * np.abs(w) * position_value
    rows = []
    for i, a in enumerate(assets):
        rows.append({
            "asset": a,
            "weight": float(w[i]),
            "position_value": float(w[i] * position_value),
            "marginal_var": res["marginal_var"][i],
            "marginal_var_amount": res["marginal_var"][i] * position_value,
            "component_var": res["component_var"][i],
            "pct_contribution": res["percentage_contribution"][i],
            "standalone_var": float(standalone[i]),
        })
    out = {
        "n_observations": int(len(df)),
        "n_assets": n,
        "position_value": position_value,
        "confidence": confidence,
        "horizon_days": horizon,
        "method": "parametric delta-normal, zero mean, sample covariance of the given returns",
        "portfolio_var": res["portfolio_var"],
        "portfolio_var_pct": res["portfolio_var"] / position_value * 100.0,
        "portfolio_volatility": res["portfolio_volatility"],
        "sum_standalone_var": float(standalone.sum()),
        "diversification_benefit": float(standalone.sum() - res["portfolio_var"]),
        "weights_normalised": normalised,
        "assets": rows,
    }
    if normalised:
        out["_warning"] = f"weights summed to {total:g}; they were rescaled to sum to 1"
    return out


# ============================================================================
# DISPATCH TABLE
# ============================================================================

OPERATIONS = {
    "risk_metrics": op_risk_metrics,
    "portfolio_analytics": op_portfolio_analytics,
    "greeks": op_greeks,
    "var_analysis": op_var_analysis,
    "stress_test": op_stress_test,
    "backtest": op_backtest,
    "statistics": op_statistics,
    "component_var": op_component_var,
}


def dispatch(operation, data):
    """Dispatch to operation handler. Always returns a dict with a `success` key."""
    handler = OPERATIONS.get(operation)
    if handler is None:
        return {
            "success": False,
            "operation": operation,
            "error": f"Unknown operation: {operation}",
            "error_kind": "unknown_op",
            "available": list(OPERATIONS.keys()),
        }
    try:
        result = handler(data or {})
        envelope = {
            "success": True,
            "operation": operation,
            "data": result,
        }
        # The panel shows a top-level "warning" as a banner above the results.
        warning = result.pop("_warning", None) if isinstance(result, dict) else None
        if warning:
            envelope["warning"] = warning
        return envelope
    except ValidationError as e:
        return {
            "success": False,
            "operation": operation,
            "error": str(e),
            "error_kind": "validation",
        }
    except Exception as e:
        traceback.print_exc(file=sys.stderr)  # diagnostics to stderr only
        return {
            "success": False,
            "operation": operation,
            "error": f"{type(e).__name__}: {e}" if str(e) else type(e).__name__,
            "error_kind": "runtime",
        }


def main(args):
    """Entry point: args = [operation, json_data]"""
    if len(args) < 2:
        fincept_json.emit({
            "success": False,
            "error": "Usage: gs_quant_service.py <operation> <json_data>",
            "error_kind": "usage",
        })
        return

    operation = args[0]
    try:
        data = json.loads(args[1]) if args[1].strip() else {}
    except json.JSONDecodeError as e:
        fincept_json.emit({
            "success": False,
            "operation": operation,
            "error": f"Invalid JSON input: {e}",
            "error_kind": "validation",
        })
        return
    if not isinstance(data, dict):
        fincept_json.emit({"success": False, "operation": operation,
                           "error": "parameters must be a JSON object", "error_kind": "validation"})
        return

    fincept_json.emit(dispatch(operation, data))


if __name__ == "__main__":
    main(sys.argv[1:])
