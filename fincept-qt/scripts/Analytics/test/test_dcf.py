"""
Unit tests for DCF (Discounted Cash Flow) valuation.
"""

from __future__ import annotations
import math
import sys
import os
import pytest

sys.path.insert(0, os.path.join(os.path.dirname(__file__)))
from analytics_core import dcf_valuation


class TestDCFValuationHappyPath:

    def test_pv_explicit_fcfs_correct(self, dcf_base_params):
        result = dcf_valuation(**dcf_base_params)
        expected = sum(
            fcf / (1 + dcf_base_params["discount_rate"]) ** (i + 1)
            for i, fcf in enumerate(dcf_base_params["free_cash_flows"])
        )
        assert math.isclose(result["pv_explicit_fcfs"], expected, rel_tol=1e-4)

    def test_terminal_value_correct(self, dcf_base_params):
        result = dcf_valuation(**dcf_base_params)
        last_fcf = dcf_base_params["free_cash_flows"][-1]
        expected_tv = last_fcf * (1 + dcf_base_params["terminal_growth_rate"]) / (
            dcf_base_params["discount_rate"] - dcf_base_params["terminal_growth_rate"]
        )
        assert math.isclose(result["terminal_value"], expected_tv, rel_tol=1e-4)

    def test_total_value_is_sum_of_parts(self, dcf_base_params):
        result = dcf_valuation(**dcf_base_params)
        expected = result["pv_explicit_fcfs"] + result["pv_terminal_value"]
        assert math.isclose(result["total_intrinsic_value"], expected, rel_tol=1e-4)

    def test_value_per_share_divides_correctly(self, dcf_base_params):
        result = dcf_valuation(**dcf_base_params)
        expected = result["total_intrinsic_value"] / dcf_base_params["shares_outstanding"]
        assert math.isclose(result["intrinsic_value_per_share"], expected, rel_tol=1e-4)

    def test_single_year_fcf(self, dcf_single_year_params):
        result = dcf_valuation(**dcf_single_year_params)
        assert result["intrinsic_value_per_share"] > 0

    def test_higher_discount_rate_lowers_value(self, dcf_base_params):
        low = dcf_valuation(**{**dcf_base_params, "discount_rate": 0.08})
        high = dcf_valuation(**{**dcf_base_params, "discount_rate": 0.15})
        assert low["intrinsic_value_per_share"] > high["intrinsic_value_per_share"]

    def test_higher_growth_increases_terminal_value(self, dcf_base_params):
        low = dcf_valuation(**{**dcf_base_params, "terminal_growth_rate": 0.01})
        high = dcf_valuation(**{**dcf_base_params, "terminal_growth_rate": 0.04})
        assert high["terminal_value"] > low["terminal_value"]

    def test_more_shares_lowers_per_share_value(self, dcf_base_params):
        few = dcf_valuation(**{**dcf_base_params, "shares_outstanding": 500_000})
        many = dcf_valuation(**{**dcf_base_params, "shares_outstanding": 5_000_000})
        assert few["intrinsic_value_per_share"] > many["intrinsic_value_per_share"]

    def test_return_keys_present(self, dcf_base_params):
        result = dcf_valuation(**dcf_base_params)
        for key in ["pv_explicit_fcfs", "terminal_value", "pv_terminal_value",
                    "total_intrinsic_value", "intrinsic_value_per_share"]:
            assert key in result

    def test_all_values_are_finite(self, dcf_base_params):
        result = dcf_valuation(**dcf_base_params)
        for key, val in result.items():
            assert math.isfinite(val), f"{key} is not finite"

    def test_terminal_value_dominates(self, dcf_base_params):
        result = dcf_valuation(**dcf_base_params)
        tv_share = result["pv_terminal_value"] / result["total_intrinsic_value"]
        assert tv_share > 0.5


class TestDCFValuationValidation:

    def test_discount_rate_must_exceed_growth_rate(self):
        with pytest.raises(ValueError, match="discount_rate must be strictly greater"):
            dcf_valuation([1_000_000], 0.03, 0.03, 1_000_000)

    def test_growth_above_discount_raises(self):
        with pytest.raises(ValueError):
            dcf_valuation([1_000_000], 0.02, 0.05, 1_000_000)

    def test_zero_shares_raises(self):
        with pytest.raises(ValueError, match="shares_outstanding must be positive"):
            dcf_valuation([1_000_000], 0.10, 0.03, 0)

    def test_negative_shares_raises(self):
        with pytest.raises(ValueError):
            dcf_valuation([1_000_000], 0.10, 0.03, -500_000)

    def test_empty_fcf_raises(self):
        with pytest.raises(ValueError, match="free_cash_flows must not be empty"):
            dcf_valuation([], 0.10, 0.03, 1_000_000)

    def test_very_high_discount_rate_works(self):
        result = dcf_valuation([1_000_000, 1_200_000], 0.50, 0.02, 100_000)
        assert result["intrinsic_value_per_share"] > 0
