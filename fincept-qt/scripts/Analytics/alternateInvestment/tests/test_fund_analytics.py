"""Tests for fund_analytics.py (reference vectors; no network).

Run:  python -m pytest -q scripts/Analytics/alternateInvestment/tests/test_fund_analytics.py
"""
import json
import math
import os
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ALT = os.path.dirname(HERE)
sys.path.insert(0, ALT)

import fund_analytics as fa  # noqa: E402

# A 36-month series (decimals) with drawdowns and a positive drift.
SERIES = [0.012, -0.004, 0.021, 0.008, -0.015, 0.006, 0.017, -0.022, 0.009, 0.004, 0.013, -0.006,
          0.011, 0.019, -0.031, 0.007, 0.002, 0.015, -0.009, 0.010, 0.005, 0.018, -0.012, 0.003,
          0.008, -0.002, 0.014, 0.006, -0.018, 0.021, 0.004, 0.009, -0.007, 0.012, 0.016, 0.001]


def test_stats_equal_cli_monthly_return_stats():
    # Same definitions as the Alt Investments hedge-fund analyzer (cli.py _monthly_return_stats).
    import cli  # the existing analyzer module, imported read-only
    for rf in (0.0, 0.04):
        mine = fa.monthly_stats(SERIES, rf)
        ref = cli._monthly_return_stats(SERIES, rf)
        assert set(ref) <= set(mine)
        for k, v in ref.items():
            assert (v is None and mine[k] is None) or abs(mine[k] - v) < 1e-15, k


def test_beta_alpha_exact_linear_case():
    # fund = 0.5 x bench + 0.001 every month, rf = 0: beta 0.5, monthly alpha 0.001 -> 1.2 % a year, R^2 = 1.
    bench = SERIES
    fund = [0.5 * b + 0.001 for b in bench]
    rel = fa.relative_stats(fund, bench, 0.0)
    assert abs(rel["beta"] - 0.5) < 1e-12
    assert abs(rel["alpha_annual"] - 0.012) < 1e-12
    assert abs(rel["r_squared"] - 1.0) < 1e-12


def test_capture_ratios_geometric():
    # Benchmark +1 % / -1 % months; fund +2 % in up months and -0.5 % in down months:
    # geometric means 2 % vs 1 % (up capture 2.0) and -0.5 % vs -1 % (down capture 0.5), Morningstar method.
    bench = [0.01, -0.01] * 6
    fund = [0.02, -0.005] * 6
    rel = fa.relative_stats(fund, bench, 0.0)
    assert abs(rel["up_capture"] - 2.0) < 1e-12
    assert abs(rel["down_capture"] - 0.5) < 1e-12
    assert rel["up_months"] == 6 and rel["down_months"] == 6


def test_var_cvar_historical():
    # 20 returns -10 % .. +9 % (step 1 %): the worst 5 % (1 month) is -10 % -> VaR = CVaR = 10 %.
    r = [(i - 10) / 100.0 for i in range(20)]
    v, cv = fa.var_cvar(r, 0.95)
    assert abs(v - 0.10) < 1e-12 and abs(cv - 0.10) < 1e-12
    # 40 returns: worst 2 months -20 %, -19 % -> VaR 19 %, CVaR 19.5 %.
    r = [(i - 20) / 100.0 for i in range(40)]
    v, cv = fa.var_cvar(r, 0.95)
    assert abs(v - 0.19) < 1e-12 and abs(cv - 0.195) < 1e-12


def test_unsmoothing_recovers_true_series():
    # Smooth a "true" series with rho = 0.4: obs_t = (1 - rho) true_t + rho obs_(t-1); Geltner unsmoothing with
    # the same rho returns the true series exactly (from the second month).
    rho = 0.4
    true = SERIES
    obs = [true[0]]
    for t in true[1:]:
        obs.append((1 - rho) * t + rho * obs[-1])
    us, used = fa.unsmooth(obs, rho)
    assert used == rho
    for a, b in zip(us, true[1:]):
        assert abs(a - b) < 1e-14
    # Unsmoothed volatility is higher than the smoothed one.
    assert fa.monthly_stats(us)["annualized_volatility"] > fa.monthly_stats(obs[1:])["annualized_volatility"]


def test_rolling_window_matches_direct_stats():
    roll = fa.rolling(SERIES, 12)
    assert len(roll) == 25
    direct = fa.monthly_stats(SERIES[-12:])
    assert abs(roll[-1]["return"] - direct["cumulative_return"]) < 1e-15


def test_analyze_series_percent_with_supplied_benchmark():
    rows = [{"date": "2022-%02d-28" % (i % 12 + 1) if i < 12 else ("2023-%02d-28" % (i % 12 + 1) if i < 24 else
             "2024-%02d-28" % (i % 12 + 1)), "value": r * 100} for i, r in enumerate(SERIES)]
    bench = [{"date": row["date"], "value": 2 * row["value"] - 0.1} for row in rows]
    out = fa.analyze({"source": "series", "series": rows, "unit": "percent", "benchmark_series": bench,
                      "unsmooth": True})
    assert out["months"] == 36 and out["first_month"] == "2022-01-01"
    assert abs(out["stats"]["cumulative_return"] - fa.monthly_stats(SERIES)["cumulative_return"]) < 1e-12
    assert abs(out["relative"]["beta"] - 0.5) < 1e-9
    assert "monthly_grid" in out and out["monthly_grid"]["2023"]["1"] == SERIES[12]
    assert "unsmoothed" in out


def test_errors_are_plain():
    try:
        fa.analyze({"source": "series", "series": [{"value": 1}] * 3})
        assert False
    except fa.InputError as e:
        assert "at least 6 monthly returns" in str(e)
    res = subprocess.run([sys.executable, os.path.join(ALT, "fund_analytics.py"), "analyze", "--stdin"],
                         input=json.dumps({"source": "ticker"}), capture_output=True, text=True, timeout=60)
    out = json.loads(res.stdout)
    assert out["success"] is False and "ticker" in out["error"]
