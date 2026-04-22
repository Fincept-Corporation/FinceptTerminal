"""
Shared pytest fixtures for the Fincept Terminal analytics test suite.
"""

from __future__ import annotations
import math
import pytest


@pytest.fixture
def daily_returns_bullish():
    import random
    rng = random.Random(42)
    return [rng.gauss(0.0008, 0.012) for _ in range(252)]


@pytest.fixture
def daily_returns_bearish():
    import random
    rng = random.Random(99)
    return [rng.gauss(-0.001, 0.018) for _ in range(252)]


@pytest.fixture
def price_series_uptrend():
    prices = [100.0]
    for _ in range(200):
        prices.append(round(prices[-1] * 1.002, 4))
    return prices


@pytest.fixture
def price_series_with_drawdown():
    prices = [100.0]
    for _ in range(100):
        prices.append(round(prices[-1] * 1.004, 4))
    for _ in range(80):
        prices.append(round(prices[-1] * 0.991, 4))
    for _ in range(60):
        prices.append(round(prices[-1] * 1.003, 4))
    return prices


@pytest.fixture
def dcf_base_params():
    return {
        "free_cash_flows": [10_000_000, 11_000_000, 12_100_000, 13_310_000, 14_641_000],
        "discount_rate": 0.10,
        "terminal_growth_rate": 0.03,
        "shares_outstanding": 1_000_000,
    }


@pytest.fixture
def dcf_single_year_params():
    return {
        "free_cash_flows": [5_000_000],
        "discount_rate": 0.12,
        "terminal_growth_rate": 0.02,
        "shares_outstanding": 500_000,
    }
