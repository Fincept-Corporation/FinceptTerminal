import math

import pytest

from fincept_engine import option_pricing as op

CASES = [
    # S, K, T, r, q, sigma
    (42.0, 40.0, 0.5, 0.10, 0.0, 0.20),
    (100.0, 95.0, 0.5, 0.10, 0.05, 0.20),
    (100.0, 130.0, 2.0, 0.03, 0.02, 0.45),
    (4000.0, 3600.0, 7 / 365, 0.05, 0.015, 0.18),
    (25.0, 25.0, 30 / 365, 0.0, 0.0, 0.9),
    (10.0, 14.0, 1.0, -0.005, 0.03, 0.3),
]


def test_norm_functions():
    assert op.norm_cdf(0.0) == 0.5
    assert op.norm_cdf(1.96) == pytest.approx(0.9750021048517795, abs=1e-14)
    assert op.norm_cdf(-1.0) + op.norm_cdf(1.0) == pytest.approx(1.0, abs=1e-15)
    assert op.norm_cdf(-10.0) == pytest.approx(7.619853024160527e-24, rel=1e-10)
    assert op.norm_pdf(0.0) == pytest.approx(1 / math.sqrt(2 * math.pi))


def test_hull_textbook_values():
    # Hull, Options Futures and Other Derivatives: S=42, K=40, r=10%, sigma=20%, T=0.5
    assert round(op.bs_price(42, 40, 0.5, 0.10, 0.0, 0.20, True), 2) == 4.76
    assert round(op.bs_price(42, 40, 0.5, 0.10, 0.0, 0.20, False), 2) == 0.81


def test_merton_dividend_yield_reference():
    # Haug, The Complete Guide to Option Pricing Formulas (Merton 1973): S=100, X=95, T=0.5, r=10%, q=5%, v=20%
    assert op.bs_price(100, 95, 0.5, 0.10, 0.05, 0.20, False) == pytest.approx(2.4648, abs=5e-4)


def test_black76_references():
    # Hull: European futures option F=20, K=20, r=9%, T=4/12, sigma=25% -> 1.12 (call = put at the money)
    assert round(op.black76_price(20, 20, 4 / 12, 0.09, 0.25, False), 2) == 1.12
    assert round(op.black76_price(20, 20, 4 / 12, 0.09, 0.25, True), 2) == 1.12
    # Haug: F=19, X=19, T=0.75, r=10%, v=28% -> c = p = 1.7011
    assert op.black76_price(19, 19, 0.75, 0.10, 0.28, True) == pytest.approx(1.7011, abs=1e-4)


@pytest.mark.parametrize("S,K,T,r,q,sigma", CASES)
def test_put_call_parity(S, K, T, r, q, sigma):
    c = op.bs_price(S, K, T, r, q, sigma, True)
    p = op.bs_price(S, K, T, r, q, sigma, False)
    assert c - p == pytest.approx(S * math.exp(-q * T) - K * math.exp(-r * T), abs=1e-10 * max(1.0, S / 100))
    cf = op.black76_price(S, K, T, r, sigma, True)
    pf = op.black76_price(S, K, T, r, sigma, False)
    assert cf - pf == pytest.approx(math.exp(-r * T) * (S - K), abs=1e-10 * max(1.0, S / 100))


def _fd_greeks_bsm(S, K, T, r, q, sigma, is_call):
    f = lambda S_=S, T_=T, r_=r, s_=sigma: op.bs_price(S_, K, T_, r_, q, s_, is_call)
    hs, ht, hr, hv = 1e-4 * S, 1e-5, 1e-6, 1e-6
    delta = (f(S_=S + hs) - f(S_=S - hs)) / (2 * hs)
    gamma = (f(S_=S + hs) - 2 * f() + f(S_=S - hs)) / hs ** 2
    vega = (f(s_=sigma + hv) - f(s_=sigma - hv)) / (2 * hv) / 100
    theta = -(f(T_=T + ht) - f(T_=T - ht)) / (2 * ht)
    rho = (f(r_=r + hr) - f(r_=r - hr)) / (2 * hr) / 100
    return delta, gamma, vega, theta, rho


@pytest.mark.parametrize("S,K,T,r,q,sigma", CASES)
@pytest.mark.parametrize("is_call", [True, False])
def test_bsm_greeks_match_finite_differences(S, K, T, r, q, sigma, is_call):
    analytic = op.bs_greeks(S, K, T, r, q, sigma, is_call)
    numeric = _fd_greeks_bsm(S, K, T, r, q, sigma, is_call)
    scale = max(1.0, S / 100)
    for name, a, n in zip(('delta', 'gamma', 'vega', 'theta', 'rho'), analytic, numeric):
        tol = 2e-4 * scale if name == 'gamma' else 5e-6 * scale * (100 if name == 'theta' else 1)
        assert a == pytest.approx(n, rel=1e-4, abs=tol), name
    p, *g = op.bs_price_and_greeks(S, K, T, r, q, sigma, is_call)
    assert p == op.bs_price(S, K, T, r, q, sigma, is_call)
    assert tuple(g) == analytic


@pytest.mark.parametrize("F,K,T,r,sigma", [(20, 20, 4 / 12, 0.09, 0.25), (4500, 4200, 0.2, 0.05, 0.2),
                                           (75, 90, 1.5, 0.02, 0.4)])
@pytest.mark.parametrize("is_call", [True, False])
def test_black76_greeks_match_finite_differences(F, K, T, r, sigma, is_call):
    f = lambda F_=F, T_=T, r_=r, s_=sigma: op.black76_price(F_, K, T_, r_, s_, is_call)
    hf, ht, hr, hv = 1e-4 * F, 1e-5, 1e-6, 1e-6
    numeric = ((f(F_=F + hf) - f(F_=F - hf)) / (2 * hf),
               (f(F_=F + hf) - 2 * f() + f(F_=F - hf)) / hf ** 2,
               (f(s_=sigma + hv) - f(s_=sigma - hv)) / (2 * hv) / 100,
               -(f(T_=T + ht) - f(T_=T - ht)) / (2 * ht),
               (f(r_=r + hr) - f(r_=r - hr)) / (2 * hr) / 100)
    analytic = op.black76_greeks(F, K, T, r, sigma, is_call)
    scale = max(1.0, F / 100)
    for name, a, n in zip(('delta', 'gamma', 'vega', 'theta', 'rho'), analytic, numeric):
        assert a == pytest.approx(n, rel=1e-4, abs=2e-4 * scale), name


def test_lean_units():
    # vega per vol point, rho per rate point, theta per year (LEAN Greeks.Theta)
    S, K, T, r, q, s = 100.0, 100.0, 1.0, 0.05, 0.0, 0.2
    d, g, v, th, rho = op.bs_greeks(S, K, T, r, q, s, True)
    assert op.bs_price(S, K, T, r, q, s + 0.01, True) - op.bs_price(S, K, T, r, q, s, True) == \
        pytest.approx(v, rel=2e-2)
    assert op.bs_price(S, K, T, r + 0.01, q, s, True) - op.bs_price(S, K, T, r, q, s, True) == \
        pytest.approx(rho, rel=3e-2)
    one_day = op.bs_price(S, K, T - 1 / 365, r, q, s, True) - op.bs_price(S, K, T, r, q, s, True)
    assert one_day == pytest.approx(th / 365, rel=1e-2)
    assert th < 0


@pytest.mark.parametrize("S,K,T,r,q,sigma", CASES)
@pytest.mark.parametrize("is_call", [True, False])
def test_implied_volatility_round_trip(S, K, T, r, q, sigma, is_call):
    p = op.bs_price(S, K, T, r, q, sigma, is_call)
    lo, _ = op.no_arbitrage_bounds(S, K, T, r, q, is_call)
    if p - lo < 1e-9 * S:     # no time value left to invert
        pytest.skip("price at the lower bound")
    iv = op.implied_volatility(p, S, K, T, r, q, is_call)
    assert iv is not None
    assert op.bs_price(S, K, T, r, q, iv, is_call) == pytest.approx(p, abs=1e-9 * max(1.0, S))
    assert iv == pytest.approx(sigma, rel=1e-6)


@pytest.mark.parametrize("sigma", [0.01, 0.05, 0.3, 1.5, 4.0])
def test_implied_volatility_range(sigma):
    p = op.bs_price(100, 105, 0.25, 0.03, 0.01, sigma, True)
    assert op.implied_volatility(p, 100, 105, 0.25, 0.03, 0.01, True) == pytest.approx(sigma, rel=1e-6)
    # in-the-money put 1% from the money (5% ITM at 1% vol has ~1e-21 time value: not identifiable)
    pf = op.black76_price(100, 101, 0.25, 0.03, sigma, False)
    assert op.black76_implied_volatility(pf, 100, 101, 0.25, 0.03, False) == pytest.approx(sigma, rel=1e-6)


def test_implied_volatility_outside_bounds_is_none():
    S, K, T, r, q = 100.0, 90.0, 0.5, 0.05, 0.0
    lo, hi = op.no_arbitrage_bounds(S, K, T, r, q, True)
    assert op.implied_volatility(lo - 0.01, S, K, T, r, q, True) is None
    assert op.implied_volatility(hi + 0.01, S, K, T, r, q, True) is None
    assert op.implied_volatility(5.0, S, K, 0.0, r, q, True) is None          # expired
    # inside the bounds but above the price at the 500% upper vol bound
    assert op.implied_volatility(hi - 1e-6, S, K, T, r, q, True) is None
    assert op.implied_volatility(float('nan'), S, K, T, r, q, True) is None


def test_edge_cases_expired_and_zero_vol():
    assert op.bs_price(105, 100, 0.0, 0.05, 0.0, 0.2, True) == 5.0
    assert op.bs_price(105, 100, -1.0, 0.05, 0.0, 0.2, False) == 0.0
    assert op.bs_greeks(105, 100, 0.0, 0.05, 0.0, 0.2, True) == (1.0, 0.0, 0.0, 0.0, 0.0)
    assert op.bs_greeks(95, 100, 0.0, 0.05, 0.0, 0.2, False) == (-1.0, 0.0, 0.0, 0.0, 0.0)
    # sigma = 0: discounted forward intrinsic value
    S, K, T, r, q = 100.0, 95.0, 1.0, 0.05, 0.02
    expected = S * math.exp(-q * T) - K * math.exp(-r * T)
    assert op.bs_price(S, K, T, r, q, 0.0, True) == pytest.approx(expected)
    assert op.bs_price(S, K, T, r, q, 0.0, False) == 0.0
    p, d, g, v, th, rho = op.bs_price_and_greeks(S, K, T, r, q, 0.0, True)
    assert d == pytest.approx(math.exp(-q * T)) and g == 0.0 and v == 0.0
    assert th == pytest.approx(q * S * math.exp(-q * T) - r * K * math.exp(-r * T))
    # tiny sigma converges to the zero-vol value
    assert op.bs_price(S, K, T, r, q, 1e-6, True) == pytest.approx(expected, abs=1e-9)


def test_dispatch():
    assert op.option_price(op.MODEL_BSM, 42, 40, 0.5, 0.1, 0.0, 0.2, True) == op.bs_price(42, 40, 0.5, 0.1, 0, 0.2, True)
    assert op.option_price(op.MODEL_BLACK76, 42, 40, 0.5, 0.1, 0.07, 0.2, True) == \
        op.black76_price(42, 40, 0.5, 0.1, 0.2, True)
    assert op.option_price_and_greeks(op.MODEL_BLACK76, 42, 40, 0.5, 0.1, 0.0, 0.2, False)[5] == \
        pytest.approx(-0.5 * op.black76_price(42, 40, 0.5, 0.1, 0.2, False) / 100)
