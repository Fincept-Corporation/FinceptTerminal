"""
Unit tests for Value-at-Risk (VaR) analytics.
"""

from __future__ import annotations
import math
import random
import sys
import os
import pytest

sys.path.insert(0, os.path.join(os.path.dirname(__file__)))
from analytics_core import historical_var, parametric_var


class TestHistoricalVaR:

    def test_returns_expected_keys(self, daily_returns_bullish):
        result = historical_var(daily_returns_bullish)
        for key in ["var_percentage", "var_currency", "cvar_percentage", "cvar_currency", "confidence_level", "observations"]:
            assert key in result

    def test_var_is_positive(self, daily_returns_bullish):
        assert historical_var(daily_returns_bullish)["var_percentage"] >= 0

    def test_currency_var_scales_with_portfolio(self, daily_returns_bullish):
        small = historical_var(daily_returns_bullish, portfolio_value=100_000)
        large = historical_var(daily_returns_bullish, portfolio_value=1_000_000)
        assert math.isclose(large["var_currency"] / small["var_currency"], 10.0, rel_tol=1e-4)

    def test_higher_confidence_gives_higher_var(self, daily_returns_bullish):
        v90 = historical_var(daily_returns_bullish, confidence_level=0.90)["var_percentage"]
        v95 = historical_var(daily_returns_bullish, confidence_level=0.95)["var_percentage"]
        v99 = historical_var(daily_returns_bullish, confidence_level=0.99)["var_percentage"]
        assert v95 >= v90
        assert v99 >= v95

    def test_cvar_at_least_as_large_as_var(self, daily_returns_bullish):
        result = historical_var(daily_returns_bullish)
        assert result["cvar_percentage"] >= result["var_percentage"] - 1e-8

    def test_observations_count_matches(self, daily_returns_bullish):
        assert historical_var(daily_returns_bullish)["observations"] == len(daily_returns_bullish)

    def test_deterministic_known_returns(self):
        returns = [-0.05, -0.03, -0.01, 0.01, 0.03, 0.05, 0.02, 0.04]
        result = historical_var(returns, confidence_level=0.875)
        assert math.isclose(result["var_percentage"], 0.03, abs_tol=1e-6)

    def test_all_positive_returns_var_near_zero(self):
        result = historical_var([0.01, 0.02, 0.005, 0.03, 0.015], confidence_level=0.95)
        assert result["var_percentage"] <= 0.005 + 1e-8

    def test_bearish_higher_var_than_bullish(self, daily_returns_bullish, daily_returns_bearish):
        bull = historical_var(daily_returns_bullish)["var_percentage"]
        bear = historical_var(daily_returns_bearish)["var_percentage"]
        assert bear > bull

    def test_var_currency_consistent(self, daily_returns_bullish):
        pv = 500_000.0
        result = historical_var(daily_returns_bullish, portfolio_value=pv)
        assert math.isclose(result["var_currency"], result["var_percentage"] * pv, rel_tol=1e-4)

    def test_confidence_level_stored(self, daily_returns_bullish):
        assert historical_var(daily_returns_bullish, confidence_level=0.97)["confidence_level"] == 0.97


class TestHistoricalVaRValidation:

    def test_empty_returns_raises(self):
        with pytest.raises(ValueError, match="returns must not be empty"):
            historical_var([])

    def test_confidence_zero_raises(self):
        with pytest.raises(ValueError, match="confidence_level must be between 0 and 1"):
            historical_var([0.01, -0.01], confidence_level=0.0)

    def test_confidence_one_raises(self):
        with pytest.raises(ValueError):
            historical_var([0.01, -0.01], confidence_level=1.0)

    def test_single_observation_does_not_crash(self):
        result = historical_var([-0.05], confidence_level=0.95)
        assert result["var_percentage"] >= 0


class TestParametricVaR:

    def test_returns_expected_keys(self, daily_returns_bullish):
        result = parametric_var(daily_returns_bullish)
        for key in ["var_percentage", "var_currency", "mean_return", "volatility", "z_score", "confidence_level"]:
            assert key in result

    def test_var_is_non_negative(self, daily_returns_bullish):
        assert parametric_var(daily_returns_bullish)["var_percentage"] >= 0

    def test_higher_confidence_gives_higher_var(self, daily_returns_bullish):
        v90 = parametric_var(daily_returns_bullish, confidence_level=0.90)["var_percentage"]
        v95 = parametric_var(daily_returns_bullish, confidence_level=0.95)["var_percentage"]
        v99 = parametric_var(daily_returns_bullish, confidence_level=0.99)["var_percentage"]
        assert v95 >= v90
        assert v99 >= v95

    def test_z_score_95_approximately_correct(self, daily_returns_bullish):
        result = parametric_var(daily_returns_bullish, confidence_level=0.95)
        assert math.isclose(result["z_score"], 1.645, abs_tol=0.05)

    def test_z_score_99_approximately_correct(self, daily_returns_bullish):
        result = parametric_var(daily_returns_bullish, confidence_level=0.99)
        assert math.isclose(result["z_score"], 2.326, abs_tol=0.05)

    def test_custom_z_score_overrides(self, daily_returns_bullish):
        result = parametric_var(daily_returns_bullish, z_score=2.0)
        assert math.isclose(result["z_score"], 2.0, rel_tol=1e-6)

    def test_volatility_is_positive(self, daily_returns_bullish):
        assert parametric_var(daily_returns_bullish)["volatility"] > 0

    def test_bearish_has_higher_var(self, daily_returns_bullish, daily_returns_bearish):
        bull = parametric_var(daily_returns_bullish)["var_percentage"]
        bear = parametric_var(daily_returns_bearish)["var_percentage"]
        assert bear > bull

    def test_all_values_finite(self, daily_returns_bullish):
        result = parametric_var(daily_returns_bullish)
        for k, v in result.items():
            assert math.isfinite(v), f"{k} is not finite"


class TestParametricVaRValidation:

    def test_empty_returns_raises(self):
        with pytest.raises(ValueError, match="returns must not be empty"):
            parametric_var([])

    def test_confidence_zero_raises(self):
        with pytest.raises(ValueError):
            parametric_var([0.01, -0.01], confidence_level=0.0)

    def test_confidence_above_one_raises(self):
        with pytest.raises(ValueError):
            parametric_var([0.01, -0.01], confidence_level=1.01)

    def test_single_return_raises(self):
        with pytest.raises((ValueError, ZeroDivisionError)):
            parametric_var([0.01])
