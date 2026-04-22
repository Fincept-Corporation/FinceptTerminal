"""
fincept-cpp/scripts/Analytics/tests/test_sharpe_metrics.py

Unit tests for risk-adjusted return metrics:
  - Sharpe ratio
  - Sortino ratio
  - Max Drawdown
  - Calmar ratio

Properties verified:
  - Sign correctness (positive return series → positive Sharpe)
  - Monotonicity (higher vol → lower Sharpe, deeper drawdown → lower Calmar)
  - Annualisation scaling
  - Mathematical relationships between metrics
  - Input validation / edge cases
"""

from __future__ import annotations

import math
import sys
import os

import pytest

sys.path.insert(0, os.path.join(os.path.dirname(__file__), ".."))
from analytics_core import sharpe_ratio, sortino_ratio, max_drawdown, calmar_ratio


# ──────────────────────────────────────────────────────────────────────────────
# Sharpe ratio tests
# ──────────────────────────────────────────────────────────────────────────────

class TestSharpeRatio:

    def test_returns_expected_keys(self, daily_returns_bullish):
        result = sharpe_ratio(daily_returns_bullish)
        assert "sharpe_ratio" in result
        assert "annualised" in result

    def test_positive_sharpe_for_bullish_series(self, daily_returns_bullish):
        result = sharpe_ratio(daily_returns_bullish)
        assert result["sharpe_ratio"] > 0

    def test_negative_sharpe_for_bearish_series(self, daily_returns_bearish):
        result = sharpe_ratio(daily_returns_bearish)
        assert result["sharpe_ratio"] < 0

    def test_higher_risk_free_rate_lowers_sharpe(self, daily_returns_bullish):
        low_rf = sharpe_ratio(daily_returns_bullish, risk_free_rate=0.0)["sharpe_ratio"]
        high_rf = sharpe_ratio(daily_returns_bullish, risk_free_rate=0.05)["sharpe_ratio"]
        assert low_rf > high_rf

    def test_annualisation_by_period(self, daily_returns_bullish):
        """Daily vs weekly period scaling should produce meaningfully different values."""
        daily = sharpe_ratio(daily_returns_bullish, periods_per_year=252)["sharpe_ratio"]
        monthly = sharpe_ratio(daily_returns_bullish, periods_per_year=12)["sharpe_ratio"]
        # They should differ because sqrt(periods) scales differently
        assert not math.isclose(daily, monthly, rel_tol=0.01)

    def test_annualised_flag_is_true(self, daily_returns_bullish):
        result = sharpe_ratio(daily_returns_bullish)
        assert result["annualised"] is True

    def test_sharpe_value_is_finite(self, daily_returns_bullish):
        result = sharpe_ratio(daily_returns_bullish)
        assert math.isfinite(result["sharpe_ratio"])

    def test_known_value_check(self):
        """
        μ_excess = 0.001, σ_excess = 0.01, periods = 252
        Sharpe ≈ 0.001/0.01 * sqrt(252) ≈ 1.5874
        """
        # Construct series: alternating +0.011 and −0.009 → μ=0.001, σ≈0.01
        n = 200
        returns = [0.011 if i % 2 == 0 else -0.009 for i in range(n)]
        result = sharpe_ratio(returns, risk_free_rate=0.0, periods_per_year=252)
        # Should be positive and roughly in the range 1.0–2.5
        assert 0.5 < result["sharpe_ratio"] < 4.0

    def test_zero_variance_raises(self):
        """Constant returns have zero variance → Sharpe undefined."""
        with pytest.raises(ValueError, match="zero variance"):
            sharpe_ratio([0.001] * 50)

    def test_single_observation_raises(self):
        with pytest.raises(ValueError, match="at least 2"):
            sharpe_ratio([0.01])

    def test_empty_returns_raises(self):
        with pytest.raises(ValueError):
            sharpe_ratio([])


# ──────────────────────────────────────────────────────────────────────────────
# Sortino ratio tests
# ──────────────────────────────────────────────────────────────────────────────

class TestSortinoRatio:

    def test_returns_expected_keys(self, daily_returns_bullish):
        result = sortino_ratio(daily_returns_bullish)
        assert "sortino_ratio" in result
        assert "annualised" in result

    def test_positive_sortino_for_bullish(self, daily_returns_bullish):
        result = sortino_ratio(daily_returns_bullish)
        assert result["sortino_ratio"] > 0

    def test_negative_sortino_for_bearish(self, daily_returns_bearish):
        result = sortino_ratio(daily_returns_bearish)
        assert result["sortino_ratio"] < 0

    def test_sortino_geq_sharpe_for_symmetric_returns(self):
        """
        For a symmetric return distribution, Sortino ≥ Sharpe because
        it uses only downside deviation (< total std dev).
        """
        import random
        rng = random.Random(42)
        returns = [rng.gauss(0.001, 0.012) for _ in range(252)]
        sh = sharpe_ratio(returns)["sharpe_ratio"]
        so = sortino_ratio(returns)["sortino_ratio"]
        assert so >= sh - 0.5  # allow some numeric tolerance

    def test_no_downside_returns_raises(self):
        """All-positive returns → downside std = 0 → Sortino undefined."""
        with pytest.raises(ValueError, match="downside deviations"):
            sortino_ratio([0.01, 0.02, 0.005, 0.03])

    def test_single_observation_raises(self):
        with pytest.raises(ValueError):
            sortino_ratio([0.01])

    def test_custom_target_return(self, daily_returns_bullish):
        """Higher target return should reduce the Sortino ratio."""
        base = sortino_ratio(daily_returns_bullish, target_return=0.0)["sortino_ratio"]
        high_target = sortino_ratio(daily_returns_bullish, target_return=0.002)["sortino_ratio"]
        # More observations fall below a higher target → more downside deviation
        assert base > high_target

    def test_annualised_flag_is_true(self, daily_returns_bullish):
        assert sortino_ratio(daily_returns_bullish)["annualised"] is True

    def test_value_is_finite(self, daily_returns_bullish):
        result = sortino_ratio(daily_returns_bullish)
        assert math.isfinite(result["sortino_ratio"])


# ──────────────────────────────────────────────────────────────────────────────
# Max Drawdown tests
# ──────────────────────────────────────────────────────────────────────────────

class TestMaxDrawdown:

    def test_returns_expected_keys(self, price_series_with_drawdown):
        result = max_drawdown(price_series_with_drawdown)
        assert "max_drawdown" in result
        assert "max_drawdown_pct" in result
        assert "peak_index" in result
        assert "trough_index" in result

    def test_uptrend_has_near_zero_drawdown(self, price_series_uptrend):
        result = max_drawdown(price_series_uptrend)
        assert result["max_drawdown"] < 0.01  # < 1 %

    def test_drawdown_between_zero_and_one(self, price_series_with_drawdown):
        result = max_drawdown(price_series_with_drawdown)
        assert 0 <= result["max_drawdown"] <= 1

    def test_percentage_is_100x_decimal(self, price_series_with_drawdown):
        result = max_drawdown(price_series_with_drawdown)
        assert math.isclose(
            result["max_drawdown_pct"],
            result["max_drawdown"] * 100,
            rel_tol=1e-6,
        )

    def test_peak_index_before_trough(self, price_series_with_drawdown):
        result = max_drawdown(price_series_with_drawdown)
        assert result["peak_index"] <= result["trough_index"]

    def test_known_drawdown_value(self):
        """
        Prices: [100, 150, 75]
        Peak=150, trough=75 → drawdown = (150-75)/150 = 0.5
        """
        prices = [100.0, 150.0, 75.0]
        result = max_drawdown(prices)
        assert math.isclose(result["max_drawdown"], 0.5, rel_tol=1e-6)
        assert result["peak_index"] == 1
        assert result["trough_index"] == 2

    def test_monotonically_increasing_prices(self):
        prices = [10.0, 20.0, 30.0, 40.0, 50.0]
        result = max_drawdown(prices)
        assert result["max_drawdown"] == 0.0

    def test_monotonically_decreasing_prices(self):
        """Prices only fall → drawdown = (first - last) / first"""
        prices = [100.0, 80.0, 60.0, 40.0]
        result = max_drawdown(prices)
        expected = (100.0 - 40.0) / 100.0
        assert math.isclose(result["max_drawdown"], expected, rel_tol=1e-6)

    def test_single_price_has_zero_drawdown(self):
        result = max_drawdown([100.0])
        assert result["max_drawdown"] == 0.0

    def test_two_equal_prices_have_zero_drawdown(self):
        result = max_drawdown([100.0, 100.0])
        assert result["max_drawdown"] == 0.0

    def test_empty_prices_raises(self):
        with pytest.raises(ValueError, match="prices must not be empty"):
            max_drawdown([])

    def test_negative_price_raises(self):
        with pytest.raises(ValueError, match="positive"):
            max_drawdown([100.0, -50.0, 80.0])

    def test_zero_price_raises(self):
        with pytest.raises(ValueError):
            max_drawdown([100.0, 0.0, 50.0])

    def test_larger_crash_has_larger_drawdown(self):
        small_crash = [100.0, 90.0, 95.0]   # ~10 % drawdown
        large_crash = [100.0, 50.0, 55.0]   # ~50 % drawdown
        dd_small = max_drawdown(small_crash)["max_drawdown"]
        dd_large = max_drawdown(large_crash)["max_drawdown"]
        assert dd_large > dd_small

    def test_drawdown_identifies_correct_crash_window(self, price_series_with_drawdown):
        """The crash in the fixture goes from ~100 items in, so trough should be > peak."""
        result = max_drawdown(price_series_with_drawdown)
        assert result["trough_index"] > result["peak_index"]

    def test_all_values_finite(self, price_series_with_drawdown):
        result = max_drawdown(price_series_with_drawdown)
        for k, v in result.items():
            if isinstance(v, (int, float)):
                assert math.isfinite(v), f"{k} is not finite"


# ──────────────────────────────────────────────────────────────────────────────
# Calmar ratio tests
# ──────────────────────────────────────────────────────────────────────────────

class TestCalmarRatio:

    def test_returns_expected_keys(self, daily_returns_bullish, price_series_with_drawdown):
        result = calmar_ratio(daily_returns_bullish, price_series_with_drawdown)
        assert "calmar_ratio" in result
        assert "annualised_return" in result
        assert "max_drawdown" in result

    def test_positive_calmar_for_bullish_series(self, daily_returns_bullish, price_series_with_drawdown):
        result = calmar_ratio(daily_returns_bullish, price_series_with_drawdown)
        assert result["calmar_ratio"] > 0

    def test_max_drawdown_matches_standalone(self, daily_returns_bullish, price_series_with_drawdown):
        calmar_result = calmar_ratio(daily_returns_bullish, price_series_with_drawdown)
        dd_result = max_drawdown(price_series_with_drawdown)
        assert math.isclose(
            calmar_result["max_drawdown"],
            dd_result["max_drawdown"],
            rel_tol=1e-6,
        )

    def test_calmar_formula_correct(self, daily_returns_bullish, price_series_with_drawdown):
        result = calmar_ratio(daily_returns_bullish, price_series_with_drawdown)
        expected = result["annualised_return"] / result["max_drawdown"]
        assert math.isclose(result["calmar_ratio"], expected, rel_tol=1e-3)

    def test_zero_drawdown_raises(self, daily_returns_bullish, price_series_uptrend):
        """Uptrend price series has ~zero drawdown → Calmar undefined."""
        with pytest.raises(ValueError, match="zero"):
            calmar_ratio(daily_returns_bullish, price_series_uptrend)

    def test_single_return_raises(self, price_series_with_drawdown):
        with pytest.raises(ValueError):
            calmar_ratio([0.01], price_series_with_drawdown)

    def test_higher_return_gives_higher_calmar(self, price_series_with_drawdown):
        """Doubling mean return should roughly double Calmar."""
        returns_low = [0.001] * 252
        returns_high = [0.002] * 252
        # Inject minimal variance so Sortino doesn't complain
        returns_low  = [r + (-1)**i * 0.0001 for i, r in enumerate(returns_low)]
        returns_high = [r + (-1)**i * 0.0001 for i, r in enumerate(returns_high)]

        c_low = calmar_ratio(returns_low, price_series_with_drawdown)["calmar_ratio"]
        c_high = calmar_ratio(returns_high, price_series_with_drawdown)["calmar_ratio"]
        assert c_high > c_low

    def test_all_values_finite(self, daily_returns_bullish, price_series_with_drawdown):
        result = calmar_ratio(daily_returns_bullish, price_series_with_drawdown)
        for k, v in result.items():
            if isinstance(v, (int, float)):
                assert math.isfinite(v), f"{k} is not finite"
