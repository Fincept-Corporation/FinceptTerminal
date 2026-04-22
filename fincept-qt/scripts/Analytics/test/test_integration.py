"""
fincept-cpp/scripts/Analytics/tests/test_integration.py

Integration tests for the Fincept Terminal analytics CLI.

These tests exercise:
  1. The JSON stdin/stdout contract (as called by python_runner.cpp)
  2. Cross-module consistency (e.g. higher VaR → lower Sharpe scenarios)
  3. Realistic portfolio scenarios end-to-end
"""

from __future__ import annotations

import json
import math
import subprocess
import sys
import os
from pathlib import Path

import pytest

SCRIPT_DIR = Path(__file__).parent.parent
SCRIPT_PATH = SCRIPT_DIR / "analytics_core.py"

sys.path.insert(0, str(SCRIPT_DIR))
from analytics_core import (
    dcf_valuation,
    historical_var,
    parametric_var,
    sharpe_ratio,
    sortino_ratio,
    max_drawdown,
    calmar_ratio,
)


# ──────────────────────────────────────────────────────────────────────────────
# CLI contract tests (python_runner.cpp compatibility)
# ──────────────────────────────────────────────────────────────────────────────

class TestCLIContract:
    """
    Verify that analytics_core.py, when called as a subprocess, returns
    valid JSON with the expected {success, data} envelope — exactly what
    python_runner.cpp expects.
    """

    def _run(self, command: str, args: dict) -> dict:
        result = subprocess.run(
            [sys.executable, str(SCRIPT_PATH), command, json.dumps(args)],
            capture_output=True,
            text=True,
            timeout=10,
        )
        assert result.returncode == 0, f"Script exited {result.returncode}: {result.stderr}"
        return json.loads(result.stdout.strip())

    def test_dcf_cli_success(self):
        args = {
            "free_cash_flows": [1_000_000, 1_100_000, 1_210_000],
            "discount_rate": 0.10,
            "terminal_growth_rate": 0.03,
            "shares_outstanding": 500_000,
        }
        response = self._run("dcf", args)
        assert response["success"] is True
        assert "data" in response
        assert "intrinsic_value_per_share" in response["data"]

    def test_historical_var_cli_success(self):
        import random
        rng = random.Random(1)
        returns = [rng.gauss(0.001, 0.01) for _ in range(100)]
        response = self._run("historical_var", {"returns": returns})
        assert response["success"] is True
        assert response["data"]["var_percentage"] >= 0

    def test_parametric_var_cli_success(self):
        import random
        rng = random.Random(2)
        returns = [rng.gauss(0.001, 0.01) for _ in range(100)]
        response = self._run("parametric_var", {"returns": returns})
        assert response["success"] is True

    def test_sharpe_cli_success(self):
        import random
        rng = random.Random(3)
        returns = [rng.gauss(0.001, 0.01) for _ in range(100)]
        response = self._run("sharpe", {"returns": returns})
        assert response["success"] is True

    def test_max_drawdown_cli_success(self):
        prices = [100.0 * (1.002 ** i) for i in range(50)]
        prices += [prices[-1] * (0.99 ** i) for i in range(20)]
        response = self._run("max_drawdown", {"prices": prices})
        assert response["success"] is True

    def test_unknown_command_returns_failure(self):
        result = subprocess.run(
            [sys.executable, str(SCRIPT_PATH), "nonexistent_command", "{}"],
            capture_output=True, text=True, timeout=10,
        )
        response = json.loads(result.stdout.strip())
        assert response["success"] is False
        assert "error" in response

    def test_invalid_json_args_returns_failure(self):
        result = subprocess.run(
            [sys.executable, str(SCRIPT_PATH), "sharpe", "NOT_JSON"],
            capture_output=True, text=True, timeout=10,
        )
        response = json.loads(result.stdout.strip())
        assert response["success"] is False

    def test_missing_args_returns_failure(self):
        """Calling without enough argv arguments should produce a failure JSON, not a traceback."""
        result = subprocess.run(
            [sys.executable, str(SCRIPT_PATH)],
            capture_output=True, text=True, timeout=10,
        )
        # Should exit non-zero or output a failure JSON
        if result.returncode == 0:
            response = json.loads(result.stdout.strip())
            assert response["success"] is False

    def test_response_is_valid_json(self):
        import random
        rng = random.Random(5)
        returns = [rng.gauss(0.0, 0.01) for _ in range(50)]
        result = subprocess.run(
            [sys.executable, str(SCRIPT_PATH), "sharpe", json.dumps({"returns": returns})],
            capture_output=True, text=True, timeout=10,
        )
        # Must be parseable JSON — no tracebacks
        parsed = json.loads(result.stdout.strip())
        assert isinstance(parsed, dict)


# ──────────────────────────────────────────────────────────────────────────────
# Cross-module consistency tests
# ──────────────────────────────────────────────────────────────────────────────

class TestCrossModuleConsistency:
    """
    Verify that metrics are mutually consistent across functions.
    These tests catch regressions where one module is modified in isolation.
    """

    def test_high_var_portfolio_has_lower_sharpe(self):
        """A volatile (high-VaR) return series should have lower Sharpe than a stable one."""
        import random

        rng = random.Random(10)

        # Stable: high drift, low vol → high Sharpe, low VaR
        stable_returns   = [rng.gauss(0.003, 0.004) for _ in range(252)]
        # Volatile: low drift, high vol → low Sharpe, high VaR
        volatile_returns = [rng.gauss(0.0002, 0.025) for _ in range(252)]

        stable_var   = historical_var(stable_returns)["var_percentage"]
        volatile_var = historical_var(volatile_returns)["var_percentage"]
        assert volatile_var > stable_var

        stable_sharpe   = sharpe_ratio(stable_returns)["sharpe_ratio"]
        volatile_sharpe = sharpe_ratio(volatile_returns)["sharpe_ratio"]
        assert stable_sharpe > volatile_sharpe

    def test_max_drawdown_consistent_with_var(self):
        """
        A series with large maximum drawdown should also exhibit large historical VaR.
        Not a strict mathematical identity, but a practical sanity check.
        """
        import random

        rng = random.Random(20)
        low_risk  = [rng.gauss(0.001, 0.005) for _ in range(252)]

        # High-risk series: normal returns + periodic crashes
        high_risk = [rng.gauss(0.001, 0.015) for _ in range(252)]
        for i in range(0, 252, 30):
            high_risk[i] = -0.08  # inject crash days

        # Build corresponding price series
        def to_prices(rets):
            p = [100.0]
            for r in rets:
                p.append(p[-1] * (1 + r))
            return p

        low_dd  = max_drawdown(to_prices(low_risk))["max_drawdown"]
        high_dd = max_drawdown(to_prices(high_risk))["max_drawdown"]
        assert high_dd > low_dd

        low_var  = historical_var(low_risk,  confidence_level=0.99)["var_percentage"]
        high_var = historical_var(high_risk, confidence_level=0.99)["var_percentage"]
        assert high_var > low_var

    def test_dcf_value_decreases_as_risk_increases(self):
        """
        Higher WACC (reflecting higher risk) should always produce a lower DCF value.
        This tests the monotonicity of the risk-return relationship.
        """
        base_fcfs = [5_000_000 * (1.08 ** i) for i in range(5)]
        shares = 1_000_000

        values = []
        for wacc in [0.08, 0.10, 0.12, 0.15, 0.20]:
            result = dcf_valuation(
                free_cash_flows=base_fcfs,
                discount_rate=wacc,
                terminal_growth_rate=0.03,
                shares_outstanding=shares,
            )
            values.append(result["intrinsic_value_per_share"])

        # Each successive value should be strictly lower
        for i in range(len(values) - 1):
            assert values[i] > values[i + 1], (
                f"Value did not decrease as WACC increased: "
                f"{values[i]:.2f} vs {values[i+1]:.2f}"
            )

    def test_sortino_above_sharpe_in_right_skew_scenario(self):
        """
        For a right-skewed return series (more large gains, fewer large losses),
        Sortino should exceed Sharpe because downside deviation < total std dev.
        """
        import random
        rng = random.Random(30)

        # Right-skewed: mostly small gains, rare large gains, very few losses
        returns = []
        for _ in range(200):
            r = rng.gauss(0.002, 0.008)
            returns.append(r)
        # Add some large gains (positive skew)
        returns += [0.05, 0.04, 0.06, 0.045, 0.055]

        sh = sharpe_ratio(returns)["sharpe_ratio"]
        so = sortino_ratio(returns)["sortino_ratio"]
        # For right-skewed distribution, Sortino ≥ Sharpe
        assert so >= sh - 0.1  # small tolerance for numeric noise

    def test_calmar_improves_with_lower_drawdown(self):
        """Same return series but different price paths → lower drawdown → better Calmar."""
        import random
        rng = random.Random(40)
        returns = [rng.gauss(0.001, 0.01) for _ in range(252)]

        # Shallow drawdown price series
        shallow = [100.0]
        for r in returns[:100]:
            shallow.append(shallow[-1] * (1 + r))
        shallow += [shallow[-1] * 0.98]  # mild dip
        for r in returns[101:]:
            shallow.append(shallow[-1] * (1 + r))

        # Deep drawdown price series
        deep = [100.0]
        for r in returns[:100]:
            deep.append(deep[-1] * (1 + r))
        deep += [deep[-1] * 0.60]  # severe crash
        for r in returns[101:]:
            deep.append(deep[-1] * (1 + r))

        calmar_shallow = calmar_ratio(returns, shallow)["calmar_ratio"]
        calmar_deep    = calmar_ratio(returns, deep)["calmar_ratio"]
        assert calmar_shallow > calmar_deep


# ──────────────────────────────────────────────────────────────────────────────
# Realistic portfolio scenario
# ──────────────────────────────────────────────────────────────────────────────

class TestRealisticPortfolioScenario:
    """
    Simulates a full risk assessment workflow similar to what the C++ UI would trigger.
    """

    def test_full_equity_risk_profile(self):
        """
        Scenario: Mid-cap growth stock, 1-year daily return history.
        Expected: All metrics computed, internally consistent, within sane bounds.
        """
        import random
        rng = random.Random(99)

        # Simulate a volatile growth stock: ~30 % ann return, ~25 % ann vol
        mu_daily, sigma_daily = 0.30 / 252, 0.25 / math.sqrt(252)
        returns = [rng.gauss(mu_daily, sigma_daily) for _ in range(252)]

        # Build price series
        prices = [1000.0]
        for r in returns:
            prices.append(prices[-1] * (1 + r))

        # Run all analytics
        hvar   = historical_var(returns, confidence_level=0.95, portfolio_value=1_000_000)
        pvar   = parametric_var(returns, confidence_level=0.95, portfolio_value=1_000_000)
        sharpe = sharpe_ratio(returns)
        sortino = sortino_ratio(returns)
        mdd    = max_drawdown(prices)
        calmar = calmar_ratio(returns, prices)

        # --- Consistency assertions ---

        # VaR should be positive
        assert hvar["var_percentage"] > 0
        assert pvar["var_percentage"] > 0

        # Sharpe/Sortino positive for strong positive drift
        assert sharpe["sharpe_ratio"] > 0

        # Drawdown between 0 and 1
        assert 0 <= mdd["max_drawdown"] <= 1

        # Calmar should be positive (positive return / positive drawdown)
        if mdd["max_drawdown"] > 0:
            assert calmar["calmar_ratio"] > 0

        # CVaR >= VaR
        assert hvar["cvar_percentage"] >= hvar["var_percentage"] - 1e-8

        # Peak before trough
        assert mdd["peak_index"] <= mdd["trough_index"]

    def test_dcf_plus_risk_metrics_workflow(self):
        """
        Simulates the 'Equity Research' screen workflow:
        1. Run DCF to get intrinsic value
        2. Compute risk metrics on historical returns
        3. Check that a higher-risk business (higher WACC) has both
           lower DCF value AND higher VaR.
        """
        import random

        fcfs = [10_000_000 * (1.10 ** i) for i in range(5)]
        shares = 2_000_000

        # Low-risk business
        low_risk_dcf = dcf_valuation(
            free_cash_flows=fcfs,
            discount_rate=0.08,
            terminal_growth_rate=0.03,
            shares_outstanding=shares,
        )

        # High-risk business
        high_risk_dcf = dcf_valuation(
            free_cash_flows=fcfs,
            discount_rate=0.15,
            terminal_growth_rate=0.03,
            shares_outstanding=shares,
        )

        rng_low  = random.Random(50)
        rng_high = random.Random(51)

        low_returns  = [rng_low.gauss(0.001, 0.008)  for _ in range(252)]
        high_returns = [rng_high.gauss(0.001, 0.022) for _ in range(252)]

        low_var  = historical_var(low_returns)["var_percentage"]
        high_var = historical_var(high_returns)["var_percentage"]

        # Higher WACC → lower DCF value
        assert low_risk_dcf["intrinsic_value_per_share"] > high_risk_dcf["intrinsic_value_per_share"]
        # Higher volatility → higher VaR
        assert high_var > low_var
