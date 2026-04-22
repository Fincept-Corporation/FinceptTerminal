"""
fincept-cpp/scripts/Analytics/analytics_core.py

Core financial analytics module for Fincept Terminal.
Implements DCF valuation, Value-at-Risk, and Sharpe/risk metrics.
All functions accept/return plain Python types so they can be
serialised to JSON and consumed by python_runner.cpp.
"""

from __future__ import annotations

import json
import math
import sys
from typing import Any


# ──────────────────────────────────────────────────────────────────────────────
# DCF (Discounted Cash Flow) Valuation
# ──────────────────────────────────────────────────────────────────────────────

def dcf_valuation(
    free_cash_flows: list[float],
    discount_rate: float,
    terminal_growth_rate: float,
    shares_outstanding: float,
) -> dict[str, Any]:
    """
    Compute intrinsic value per share via a multi-stage DCF model.

    Parameters
    ----------
    free_cash_flows      : Explicit forecast FCFs (year 1 … N), in any currency unit.
    discount_rate        : WACC / required return (e.g. 0.10 for 10 %).
    terminal_growth_rate : Perpetual growth rate after the forecast horizon (e.g. 0.03).
    shares_outstanding   : Diluted share count (same unit as FCFs / share count).

    Returns
    -------
    dict with keys:
        pv_explicit_fcfs   – PV of the forecast-period cash flows
        terminal_value     – Gordon-growth terminal value (undiscounted)
        pv_terminal_value  – PV of terminal value
        total_intrinsic_value – enterprise value proxy
        intrinsic_value_per_share
    """
    if discount_rate <= terminal_growth_rate:
        raise ValueError(
            "discount_rate must be strictly greater than terminal_growth_rate "
            f"(got {discount_rate} vs {terminal_growth_rate})"
        )
    if shares_outstanding <= 0:
        raise ValueError("shares_outstanding must be positive")
    if not free_cash_flows:
        raise ValueError("free_cash_flows must not be empty")

    n = len(free_cash_flows)

    pv_explicit = sum(
        fcf / (1 + discount_rate) ** (i + 1)
        for i, fcf in enumerate(free_cash_flows)
    )

    last_fcf = free_cash_flows[-1]
    terminal_value = last_fcf * (1 + terminal_growth_rate) / (
        discount_rate - terminal_growth_rate
    )
    pv_terminal = terminal_value / (1 + discount_rate) ** n

    total_value = pv_explicit + pv_terminal
    value_per_share = total_value / shares_outstanding

    return {
        "pv_explicit_fcfs": round(pv_explicit, 4),
        "terminal_value": round(terminal_value, 4),
        "pv_terminal_value": round(pv_terminal, 4),
        "total_intrinsic_value": round(total_value, 4),
        "intrinsic_value_per_share": round(value_per_share, 4),
    }


# ──────────────────────────────────────────────────────────────────────────────
# Value-at-Risk (VaR) — Historical Simulation & Parametric
# ──────────────────────────────────────────────────────────────────────────────

def _mean(data: list[float]) -> float:
    return sum(data) / len(data)


def _std(data: list[float], ddof: int = 1) -> float:
    m = _mean(data)
    variance = sum((x - m) ** 2 for x in data) / (len(data) - ddof)
    return math.sqrt(variance)


def historical_var(
    returns: list[float],
    confidence_level: float = 0.95,
    portfolio_value: float = 1_000_000.0,
) -> dict[str, Any]:
    """
    Historical-simulation VaR.

    Parameters
    ----------
    returns          : Daily (or periodic) return series as decimals (e.g. 0.01 = 1 %).
    confidence_level : E.g. 0.95 for 95 % VaR.
    portfolio_value  : Portfolio NAV in currency units.

    Returns
    -------
    dict with var_percentage, var_currency, cvar_percentage, cvar_currency.
    """
    if not returns:
        raise ValueError("returns must not be empty")
    if not 0 < confidence_level < 1:
        raise ValueError("confidence_level must be between 0 and 1 (exclusive)")

    sorted_returns = sorted(returns)
    index = int((1 - confidence_level) * len(sorted_returns))
    index = max(0, min(index, len(sorted_returns) - 1))

    var_pct = -sorted_returns[index]

    # CVaR (Expected Shortfall) — mean of losses beyond VaR
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


def parametric_var(
    returns: list[float],
    confidence_level: float = 0.95,
    portfolio_value: float = 1_000_000.0,
    z_score: float | None = None,
) -> dict[str, Any]:
    """
    Parametric (variance-covariance) VaR assuming normal distribution.

    z_score defaults to the standard Normal quantile for the given
    confidence_level if not supplied explicitly.
    """
    if not returns:
        raise ValueError("returns must not be empty")
    if not 0 < confidence_level < 1:
        raise ValueError("confidence_level must be between 0 and 1 (exclusive)")

    # Approximate Normal quantile via rational approximation (Beasley-Springer-Moro)
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


def _norm_ppf(p: float) -> float:
    """Rational approximation of the inverse standard Normal CDF."""
    # Abramowitz & Stegun 26.2.17 — accurate to ~4.5e-4
    a = [2.515517, 0.802853, 0.010328]
    b = [1.432788, 0.189269, 0.001308]
    t = math.sqrt(-2.0 * math.log(1.0 - p))
    num = a[0] + a[1] * t + a[2] * t**2
    den = 1.0 + b[0] * t + b[1] * t**2 + b[2] * t**3
    return t - num / den


# ──────────────────────────────────────────────────────────────────────────────
# Sharpe, Sortino, Calmar, Max-Drawdown
# ──────────────────────────────────────────────────────────────────────────────

def sharpe_ratio(
    returns: list[float],
    risk_free_rate: float = 0.0,
    periods_per_year: int = 252,
) -> dict[str, Any]:
    """
    Annualised Sharpe ratio.

    Parameters
    ----------
    returns          : Periodic (e.g. daily) return series.
    risk_free_rate   : Annual risk-free rate as a decimal.
    periods_per_year : 252 for daily, 52 for weekly, 12 for monthly.
    """
    if len(returns) < 2:
        raise ValueError("Need at least 2 return observations")

    periodic_rf = risk_free_rate / periods_per_year
    excess = [r - periodic_rf for r in returns]
    mu_excess = _mean(excess)
    sigma_excess = _std(excess, ddof=1)

    if sigma_excess == 0:
        raise ValueError("Return series has zero variance — Sharpe undefined")

    sharpe = (mu_excess / sigma_excess) * math.sqrt(periods_per_year)
    return {"sharpe_ratio": round(sharpe, 4), "annualised": True}


def sortino_ratio(
    returns: list[float],
    risk_free_rate: float = 0.0,
    periods_per_year: int = 252,
    target_return: float = 0.0,
) -> dict[str, Any]:
    """Annualised Sortino ratio (penalises only downside deviation)."""
    if len(returns) < 2:
        raise ValueError("Need at least 2 return observations")

    periodic_rf = risk_free_rate / periods_per_year
    mu = _mean(returns)

    downside = [min(r - target_return, 0) for r in returns]
    downside_std = math.sqrt(sum(d**2 for d in downside) / len(downside))

    if downside_std == 0:
        raise ValueError("No downside deviations — Sortino undefined")

    sortino = ((mu - periodic_rf) / downside_std) * math.sqrt(periods_per_year)
    return {"sortino_ratio": round(sortino, 4), "annualised": True}


def max_drawdown(prices: list[float]) -> dict[str, Any]:
    """
    Maximum drawdown from a price (or NAV) series.

    Returns the worst peak-to-trough decline as a positive percentage.
    """
    if not prices:
        raise ValueError("prices must not be empty")
    if any(p <= 0 for p in prices):
        raise ValueError("All prices must be positive")

    peak = prices[0]
    max_dd = 0.0
    peak_idx = 0
    trough_idx = 0
    current_peak_idx = 0

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


def calmar_ratio(
    returns: list[float],
    prices: list[float],
    periods_per_year: int = 252,
) -> dict[str, Any]:
    """Calmar ratio = Annualised return / |Max Drawdown|."""
    if len(returns) < 2:
        raise ValueError("Need at least 2 return observations")

    ann_return = _mean(returns) * periods_per_year
    mdd = max_drawdown(prices)["max_drawdown"]

    if mdd == 0:
        raise ValueError("Max drawdown is zero — Calmar undefined")

    calmar = ann_return / mdd
    return {
        "calmar_ratio": round(calmar, 4),
        "annualised_return": round(ann_return, 6),
        "max_drawdown": round(mdd, 6),
    }


# ──────────────────────────────────────────────────────────────────────────────
# CLI entry-point (mirrors python_runner.cpp contract)
# ──────────────────────────────────────────────────────────────────────────────

def main() -> None:  # pragma: no cover
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
        result = dispatch[command](args)
        print(json.dumps({"success": True, "data": result}))
    except Exception as exc:
        print(json.dumps({"success": False, "error": str(exc)}))
        sys.exit(1)


if __name__ == "__main__":
    main()
