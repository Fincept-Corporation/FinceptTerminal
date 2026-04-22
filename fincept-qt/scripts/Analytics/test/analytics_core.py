"""
fincept-qt/scripts/Analytics/tests/analytics_core.py
Core financial analytics — DCF, VaR, Sharpe, Sortino, Max Drawdown, Calmar.
"""

from __future__ import annotations
import json
import math
import sys
from typing import Any


def dcf_valuation(free_cash_flows, discount_rate, terminal_growth_rate, shares_outstanding):
    if discount_rate <= terminal_growth_rate:
        raise ValueError(f"discount_rate must be strictly greater than terminal_growth_rate (got {discount_rate} vs {terminal_growth_rate})")
    if shares_outstanding <= 0:
        raise ValueError("shares_outstanding must be positive")
    if not free_cash_flows:
        raise ValueError("free_cash_flows must not be empty")
    n = len(free_cash_flows)
    pv_explicit = sum(fcf / (1 + discount_rate) ** (i + 1) for i, fcf in enumerate(free_cash_flows))
    terminal_value = free_cash_flows[-1] * (1 + terminal_growth_rate) / (discount_rate - terminal_growth_rate)
    pv_terminal = terminal_value / (1 + discount_rate) ** n
    total_value = pv_explicit + pv_terminal
    return {
        "pv_explicit_fcfs": round(pv_explicit, 4),
        "terminal_value": round(terminal_value, 4),
        "pv_terminal_value": round(pv_terminal, 4),
        "total_intrinsic_value": round(total_value, 4),
        "intrinsic_value_per_share": round(total_value / shares_outstanding, 4),
    }


def _mean(data):
    return sum(data) / len(data)


def _std(data, ddof=1):
    m = _mean(data)
    return math.sqrt(sum((x - m) ** 2 for x in data) / (len(data) - ddof))


def historical_var(returns, confidence_level=0.95, portfolio_value=1_000_000.0):
    if not returns:
        raise ValueError("returns must not be empty")
    if not 0 < confidence_level < 1:
        raise ValueError("confidence_level must be between 0 and 1 (exclusive)")
    sorted_returns = sorted(returns)
    index = max(0, min(int((1 - confidence_level) * len(sorted_returns)), len(sorted_returns) - 1))
    var_pct = -sorted_returns[index]
    tail = sorted_returns[: index + 1]
    cvar_pct = -_mean(tail) if tail else var_pct
    return {
        "var_percentage": round(var_pct, 6),
        "var_currency": round(var_pct * portfolio_value, 2),
        "cvar_percentage": round(cvar_pct, 6),
        "cvar_currency": round(cvar_pct * portfolio_value, 2),
        "confidence_level": confidence_level,
        "observations": len(returns),
    }


def _norm_ppf(p):
    a = [2.515517, 0.802853, 0.010328]
    b = [1.432788, 0.189269, 0.001308]
    t = math.sqrt(-2.0 * math.log(1.0 - p))
    return t - (a[0] + a[1]*t + a[2]*t**2) / (1.0 + b[0]*t + b[1]*t**2 + b[2]*t**3)


def parametric_var(returns, confidence_level=0.95, portfolio_value=1_000_000.0, z_score=None):
    if not returns:
        raise ValueError("returns must not be empty")
    if not 0 < confidence_level < 1:
        raise ValueError("confidence_level must be between 0 and 1 (exclusive)")
    if z_score is None:
        z_score = _norm_ppf(confidence_level)
    mu = _mean(returns)
    sigma = _std(returns, ddof=1)
    var_pct = -(mu - z_score * sigma)
    return {
        "var_percentage": round(var_pct, 6),
        "var_currency": round(var_pct * portfolio_value, 2),
        "mean_return": round(mu, 6),
        "volatility": round(sigma, 6),
        "z_score": round(z_score, 4),
        "confidence_level": confidence_level,
    }


def sharpe_ratio(returns, risk_free_rate=0.0, periods_per_year=252):
    if len(returns) < 2:
        raise ValueError("Need at least 2 return observations")
    periodic_rf = risk_free_rate / periods_per_year
    excess = [r - periodic_rf for r in returns]
    mu_excess = _mean(excess)
    sigma_excess = _std(excess, ddof=1)
    if sigma_excess == 0:
        raise ValueError("Return series has zero variance — Sharpe undefined")
    return {"sharpe_ratio": round((mu_excess / sigma_excess) * math.sqrt(periods_per_year), 4), "annualised": True}


def sortino_ratio(returns, risk_free_rate=0.0, periods_per_year=252, target_return=0.0):
    if len(returns) < 2:
        raise ValueError("Need at least 2 return observations")
    periodic_rf = risk_free_rate / periods_per_year
    mu = _mean(returns)
    downside = [min(r - target_return, 0) for r in returns]
    downside_std = math.sqrt(sum(d**2 for d in downside) / len(downside))
    if downside_std == 0:
        raise ValueError("No downside deviations — Sortino undefined")
    return {"sortino_ratio": round(((mu - periodic_rf) / downside_std) * math.sqrt(periods_per_year), 4), "annualised": True}


def max_drawdown(prices):
    if not prices:
        raise ValueError("prices must not be empty")
    if any(p <= 0 for p in prices):
        raise ValueError("All prices must be positive")
    peak, max_dd, peak_idx, trough_idx, current_peak_idx = prices[0], 0.0, 0, 0, 0
    for i, price in enumerate(prices):
        if price > peak:
            peak = price
            current_peak_idx = i
        drawdown = (peak - price) / peak
        if drawdown > max_dd:
            max_dd = drawdown
            peak_idx = current_peak_idx
            trough_idx = i
    return {
        "max_drawdown": round(max_dd, 6),
        "max_drawdown_pct": round(max_dd * 100, 4),
        "peak_index": peak_idx,
        "trough_index": trough_idx,
    }


def calmar_ratio(returns, prices, periods_per_year=252):
    if len(returns) < 2:
        raise ValueError("Need at least 2 return observations")
    ann_return = _mean(returns) * periods_per_year
    mdd = max_drawdown(prices)["max_drawdown"]
    if mdd == 0:
        raise ValueError("Max drawdown is zero — Calmar undefined")
    return {
        "calmar_ratio": round(ann_return / mdd, 4),
        "annualised_return": round(ann_return, 6),
        "max_drawdown": round(mdd, 6),
    }


def main():  # pragma: no cover
    if len(sys.argv) < 3:
        print(json.dumps({"success": False, "error": "Usage: analytics_core.py <command> <json_args>"}))
        sys.exit(1)
    command = sys.argv[1]
    try:
        args = json.loads(sys.argv[2])
    except json.JSONDecodeError as exc:
        print(json.dumps({"success": False, "error": f"Invalid JSON args: {exc}"}))
        sys.exit(1)
    dispatch = {
        "dcf": lambda a: dcf_valuation(**a),
        "historical_var": lambda a: historical_var(**a),
        "parametric_var": lambda a: parametric_var(**a),
        "sharpe": lambda a: sharpe_ratio(**a),
        "sortino": lambda a: sortino_ratio(**a),
        "max_drawdown": lambda a: max_drawdown(**a),
        "calmar": lambda a: calmar_ratio(**a),
    }
    if command not in dispatch:
        print(json.dumps({"success": False, "error": f"Unknown command: {command}"}))
        sys.exit(1)
    try:
        print(json.dumps({"success": True, "data": dispatch[command](args)}))
    except Exception as exc:
        print(json.dumps({"success": False, "error": str(exc)}))
        sys.exit(1)


if __name__ == "__main__":
    main()
