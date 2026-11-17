# ============================================================================
# Fincept Terminal - Strategy Engine Option Pricing
# Black-Scholes-Merton (spot, continuous dividend yield) and Black-76 (futures)
# prices, analytic greeks in LEAN's units and a bracketed implied-volatility
# solver. Pure `math` (no numpy/scipy import: the engine's cold start matters).
# ============================================================================

import math

# Every result that uses these prices must carry this statement (option_model.disclosure() includes it).
EUROPEAN_EXERCISE_DISCLOSURE = (
    "American-style options (US equity and ETF options, most futures options) are priced as European: "
    "no early-exercise premium is modelled, which understates deep in-the-money puts, and calls just "
    "before an ex-dividend date.")

GREEK_UNITS = (
    "Greeks use LEAN's units: delta and gamma per 1.00 move of the underlying (the spot price for "
    "Black-Scholes-Merton, the futures price for Black-76); vega per 1 volatility point (+0.01); rho per "
    "1 percentage point of the risk-free rate (+0.01); theta is the time decay per year (LEAN "
    "Greeks.Theta) and theta_per_day = theta / 365. Time to expiry is calendar time / 365 days.")

MODEL_BSM = 'black_scholes_merton'
MODEL_BLACK76 = 'black76'

IV_LOWER_BOUND = 1e-4
IV_UPPER_BOUND = 5.0

_SQRT2 = math.sqrt(2.0)
_INV_SQRT_2PI = 1.0 / math.sqrt(2.0 * math.pi)
_erfc = math.erfc
_exp = math.exp
_log = math.log
_sqrt = math.sqrt


def norm_cdf(x: float) -> float:
    """Standard normal CDF (erfc form: accurate in both tails)."""
    return 0.5 * _erfc(-x / _SQRT2)


def norm_pdf(x: float) -> float:
    return _INV_SQRT_2PI * _exp(-0.5 * x * x)


def intrinsic_value(underlying: float, strike: float, is_call: bool) -> float:
    return max(underlying - strike, 0.0) if is_call else max(strike - underlying, 0.0)


# ---------------------------------------------------------------------------
# Core: Black-Scholes-Merton with continuous yield q. Black-76 is the same
# formula with S=F and q=r (only rho differs, handled by the caller).
# Returns (price, delta, gamma, vega, theta, rho) in LEAN units, or just
# (price,) when greeks=False.
# ---------------------------------------------------------------------------

def _expired(S, K, is_call, greeks):
    p = intrinsic_value(S, K, is_call)
    if not greeks:
        return (p,)
    if is_call:
        delta = 1.0 if S > K else (0.5 if S == K else 0.0)
    else:
        delta = -1.0 if S < K else (-0.5 if S == K else 0.0)
    return (p, delta, 0.0, 0.0, 0.0, 0.0)


def _bsm(S, K, T, r, q, sigma, is_call, greeks):
    if T <= 0.0:
        return _expired(S, K, is_call, greeks)
    df_r = _exp(-r * T)
    df_q = _exp(-q * T)
    if S <= 0.0 or K <= 0.0 or sigma <= 0.0:
        # Deterministic limit: the discounted forward intrinsic value.
        fwd_s = S * df_q if S > 0.0 else 0.0
        pv_k = K * df_r if K > 0.0 else 0.0
        diff = fwd_s - pv_k
        if is_call:
            p = diff if diff > 0.0 else 0.0
            if not greeks:
                return (p,)
            if diff > 0.0:
                return (p, df_q, 0.0, 0.0, q * fwd_s - r * pv_k, K * T * df_r / 100.0)
            if diff == 0.0 and S > 0.0:
                return (0.0, 0.5 * df_q, 0.0, 0.0, 0.0, 0.0)
            return (0.0, 0.0, 0.0, 0.0, 0.0, 0.0)
        p = -diff if diff < 0.0 else 0.0
        if not greeks:
            return (p,)
        if diff < 0.0:
            return (p, -df_q, 0.0, 0.0, r * pv_k - q * fwd_s, -K * T * df_r / 100.0)
        if diff == 0.0 and S > 0.0:
            return (0.0, -0.5 * df_q, 0.0, 0.0, 0.0, 0.0)
        return (0.0, 0.0, 0.0, 0.0, 0.0, 0.0)

    sqrt_t = _sqrt(T)
    sd = sigma * sqrt_t
    d1 = (_log(S / K) + (r - q + 0.5 * sigma * sigma) * T) / sd
    d2 = d1 - sd
    s_q = S * df_q
    k_r = K * df_r
    if is_call:
        nd1 = 0.5 * _erfc(-d1 / _SQRT2)
        nd2 = 0.5 * _erfc(-d2 / _SQRT2)
        p = s_q * nd1 - k_r * nd2
        if p < 0.0:
            p = 0.0
        if not greeks:
            return (p,)
        pdf = _INV_SQRT_2PI * _exp(-0.5 * d1 * d1)
        decay = -s_q * pdf * sigma / (2.0 * sqrt_t)
        return (p, df_q * nd1, df_q * pdf / (S * sd), s_q * pdf * sqrt_t / 100.0,
                decay + q * s_q * nd1 - r * k_r * nd2, k_r * T * nd2 / 100.0)
    nmd1 = 0.5 * _erfc(d1 / _SQRT2)   # N(-d1)
    nmd2 = 0.5 * _erfc(d2 / _SQRT2)   # N(-d2)
    p = k_r * nmd2 - s_q * nmd1
    if p < 0.0:
        p = 0.0
    if not greeks:
        return (p,)
    pdf = _INV_SQRT_2PI * _exp(-0.5 * d1 * d1)
    decay = -s_q * pdf * sigma / (2.0 * sqrt_t)
    return (p, -df_q * nmd1, df_q * pdf / (S * sd), s_q * pdf * sqrt_t / 100.0,
            decay - q * s_q * nmd1 + r * k_r * nmd2, -k_r * T * nmd2 / 100.0)


# ---------------------------------------------------------------------------
# Public prices and greeks
# ---------------------------------------------------------------------------

def bs_price(S: float, K: float, T: float, r: float, q: float, sigma: float, is_call: bool) -> float:
    """Black-Scholes-Merton price of a European option on spot S with continuous yield q.
    T <= 0 -> intrinsic value; sigma <= 0 -> discounted forward intrinsic value."""
    return _bsm(S, K, T, r, q, sigma, is_call, False)[0]


def bs_greeks(S, K, T, r, q, sigma, is_call):
    """(delta, gamma, vega, theta, rho) in LEAN units (see GREEK_UNITS)."""
    return _bsm(S, K, T, r, q, sigma, is_call, True)[1:]


def bs_price_and_greeks(S, K, T, r, q, sigma, is_call):
    """(price, delta, gamma, vega, theta, rho) in one pass."""
    return _bsm(S, K, T, r, q, sigma, is_call, True)


def black76_price(F: float, K: float, T: float, r: float, sigma: float, is_call: bool) -> float:
    """Black-76 price of a European option on a futures price F (discounted at r)."""
    return _bsm(F, K, T, r, r, sigma, is_call, False)[0]


def black76_price_and_greeks(F, K, T, r, sigma, is_call):
    """(price, delta, gamma, vega, theta, rho); delta/gamma are with respect to F and rho holds F fixed
    (rho = -T * price / 100)."""
    res = _bsm(F, K, T, r, r, sigma, is_call, True)
    p = res[0]
    rho = -T * p / 100.0 if T > 0.0 else 0.0
    return (p, res[1], res[2], res[3], res[4], rho)


def black76_greeks(F, K, T, r, sigma, is_call):
    return black76_price_and_greeks(F, K, T, r, sigma, is_call)[1:]


def option_price(model: str, underlying: float, K: float, T: float, r: float, q: float, sigma: float,
                 is_call: bool) -> float:
    """Dispatch on model (MODEL_BSM or MODEL_BLACK76; q is ignored for Black-76)."""
    if model == MODEL_BLACK76:
        return _bsm(underlying, K, T, r, r, sigma, is_call, False)[0]
    return _bsm(underlying, K, T, r, q, sigma, is_call, False)[0]


def option_price_and_greeks(model: str, underlying, K, T, r, q, sigma, is_call):
    if model == MODEL_BLACK76:
        return black76_price_and_greeks(underlying, K, T, r, sigma, is_call)
    return _bsm(underlying, K, T, r, q, sigma, is_call, True)


# ---------------------------------------------------------------------------
# Implied volatility
# ---------------------------------------------------------------------------

def no_arbitrage_bounds(S, K, T, r, q, is_call):
    """(lower, upper) model-free bounds of a European option price."""
    df_r = math.exp(-r * T)
    df_q = math.exp(-q * T)
    if is_call:
        return max(S * df_q - K * df_r, 0.0), S * df_q
    return max(K * df_r - S * df_q, 0.0), K * df_r


def _brent(f, a, b, fa, fb, xtol, max_iter):
    """Brent-Dekker root finder on a bracket [a, b] with f(a)*f(b) <= 0."""
    c, fc = b, fb
    d = e = b - a
    for _ in range(max_iter):
        if (fb > 0.0) == (fc > 0.0):
            c, fc = a, fa
            d = e = b - a
        if abs(fc) < abs(fb):
            a, b, c = b, c, b
            fa, fb, fc = fb, fc, fb
        tol1 = 2.0 * 2.2e-16 * abs(b) + 0.5 * xtol
        xm = 0.5 * (c - b)
        if abs(xm) <= tol1 or fb == 0.0:
            return b
        if abs(e) >= tol1 and abs(fa) > abs(fb):
            s = fb / fa
            if a == c:
                p = 2.0 * xm * s
                qq = 1.0 - s
            else:
                qq = fa / fc
                rr = fb / fc
                p = s * (2.0 * xm * qq * (qq - rr) - (b - a) * (rr - 1.0))
                qq = (qq - 1.0) * (rr - 1.0) * (s - 1.0)
            if p > 0.0:
                qq = -qq
            p = abs(p)
            if 2.0 * p < min(3.0 * xm * qq - abs(tol1 * qq), abs(e * qq)):
                e, d = d, p / qq
            else:
                d = xm
                e = d
        else:
            d = xm
            e = d
        a, fa = b, fb
        b += d if abs(d) > tol1 else (tol1 if xm > 0 else -tol1)
        fb = f(b)
    return b


def implied_volatility(price: float, S: float, K: float, T: float, r: float, q: float, is_call: bool,
                       lower: float = IV_LOWER_BOUND, upper: float = IV_UPPER_BOUND,
                       xtol: float = 1e-10, max_iter: int = 200):
    """Black-Scholes-Merton implied volatility by Brent's method on [lower, upper].

    Returns None when T <= 0, when the price is outside the no-arbitrage bounds, or when no volatility
    in [lower, upper] reproduces it (the price at `lower` is above it / the price at `upper` below it)."""
    if price is None or not (price == price) or T <= 0.0 or S <= 0.0 or K <= 0.0:
        return None
    lo_b, hi_b = no_arbitrage_bounds(S, K, T, r, q, is_call)
    eps = 1e-12 * max(1.0, S, K)
    if price < lo_b - eps or price > hi_b + eps:
        return None

    def f(sig):
        return _bsm(S, K, T, r, q, sig, is_call, False)[0] - price

    f_lo = f(lower)
    if f_lo > 0.0:
        return lower if f_lo <= eps else None
    if f_lo == 0.0:
        return lower
    f_hi = f(upper)
    if f_hi < 0.0:
        return upper if -f_hi <= eps else None
    if f_hi == 0.0:
        return upper
    return _brent(f, lower, upper, f_lo, f_hi, xtol, max_iter)


def black76_implied_volatility(price: float, F: float, K: float, T: float, r: float, is_call: bool,
                               lower: float = IV_LOWER_BOUND, upper: float = IV_UPPER_BOUND,
                               xtol: float = 1e-10, max_iter: int = 200):
    """Black-76 implied volatility (same contract as implied_volatility)."""
    return implied_volatility(price, F, K, T, r, r, is_call, lower, upper, xtol, max_iter)
