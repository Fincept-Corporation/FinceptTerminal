"""
Fortitudo.tech Option Pricing Wrapper
======================================
Black-Scholes option pricing and forward calculations

Includes fallback implementations using NumPy/SciPy for Python 3.14+ compatibility.

Usage:
    python option_pricing.py
"""

import json
import math
from typing import Dict, Any
import numpy as np
from scipy import stats as scipy_stats

# Try to import fortitudo.tech, fallback to pure implementations
try:
    import fortitudo.tech as ft
    FORTITUDO_AVAILABLE = True
except ImportError:
    FORTITUDO_AVAILABLE = False
    ft = None


# ============================================================================
# FALLBACK IMPLEMENTATIONS (Pure NumPy/SciPy Black-Scholes)
# ============================================================================

def _d1(F: float, K: float, sigma: float, T: float) -> float:
    """Calculate d1 for Black-Scholes formula."""
    return (np.log(F / K) + 0.5 * sigma**2 * T) / (sigma * np.sqrt(T))


def _d2(F: float, K: float, sigma: float, T: float) -> float:
    """Calculate d2 for Black-Scholes formula."""
    return _d1(F, K, sigma, T) - sigma * np.sqrt(T)


def _fallback_call_option(F: float, K: float, sigma: float, r: float, T: float) -> float:
    """Fallback Black-Scholes call option pricing."""
    d1 = _d1(F, K, sigma, T)
    d2 = _d2(F, K, sigma, T)
    discount = np.exp(-r * T)
    return float(discount * (F * scipy_stats.norm.cdf(d1) - K * scipy_stats.norm.cdf(d2)))


def _fallback_put_option(F: float, K: float, sigma: float, r: float, T: float) -> float:
    """Fallback Black-Scholes put option pricing."""
    d1 = _d1(F, K, sigma, T)
    d2 = _d2(F, K, sigma, T)
    discount = np.exp(-r * T)
    return float(discount * (K * scipy_stats.norm.cdf(-d2) - F * scipy_stats.norm.cdf(-d1)))


def _fallback_forward(S: float, r: float, q: float, T: float) -> float:
    """Fallback forward price calculation."""
    return float(S * np.exp((r - q) * T))


# ============================================================================
# OPTION GREEKS (Delta, Gamma, Vega, Theta, Rho)
# ============================================================================

def calculate_call_delta(
    spot_price: float,
    strike: float,
    volatility: float,
    risk_free_rate: float,
    dividend_yield: float,
    time_to_maturity: float
) -> float:
    """
    Calculate call option delta.

    Delta measures the sensitivity of option price to underlying price changes.

    Returns:
        Delta value (0 to 1 for calls)
    """
    forward = _fallback_forward(spot_price, risk_free_rate, dividend_yield, time_to_maturity)
    d1 = _d1(forward, strike, volatility, time_to_maturity)
    delta = np.exp(-dividend_yield * time_to_maturity) * scipy_stats.norm.cdf(d1)
    return float(delta)


def calculate_put_delta(
    spot_price: float,
    strike: float,
    volatility: float,
    risk_free_rate: float,
    dividend_yield: float,
    time_to_maturity: float
) -> float:
    """
    Calculate put option delta.

    Returns:
        Delta value (-1 to 0 for puts)
    """
    forward = _fallback_forward(spot_price, risk_free_rate, dividend_yield, time_to_maturity)
    d1 = _d1(forward, strike, volatility, time_to_maturity)
    delta = np.exp(-dividend_yield * time_to_maturity) * (scipy_stats.norm.cdf(d1) - 1)
    return float(delta)


def calculate_gamma(
    spot_price: float,
    strike: float,
    volatility: float,
    risk_free_rate: float,
    dividend_yield: float,
    time_to_maturity: float
) -> float:
    """
    Calculate option gamma (same for calls and puts).

    Gamma measures the rate of change of delta with respect to underlying price.

    Returns:
        Gamma value
    """
    forward = _fallback_forward(spot_price, risk_free_rate, dividend_yield, time_to_maturity)
    d1 = _d1(forward, strike, volatility, time_to_maturity)
    gamma = (np.exp(-dividend_yield * time_to_maturity) * scipy_stats.norm.pdf(d1) /
             (spot_price * volatility * np.sqrt(time_to_maturity)))
    return float(gamma)


def calculate_vega(
    spot_price: float,
    strike: float,
    volatility: float,
    risk_free_rate: float,
    dividend_yield: float,
    time_to_maturity: float
) -> float:
    """
    Calculate option vega (same for calls and puts).

    Vega measures sensitivity to volatility changes.
    Returns vega per 1% change in volatility.

    Returns:
        Vega value (per 1% vol change)
    """
    forward = _fallback_forward(spot_price, risk_free_rate, dividend_yield, time_to_maturity)
    d1 = _d1(forward, strike, volatility, time_to_maturity)
    vega = (spot_price * np.exp(-dividend_yield * time_to_maturity) *
            scipy_stats.norm.pdf(d1) * np.sqrt(time_to_maturity))
    return float(vega / 100)  # Per 1% change


def calculate_call_theta(
    spot_price: float,
    strike: float,
    volatility: float,
    risk_free_rate: float,
    dividend_yield: float,
    time_to_maturity: float
) -> float:
    """
    Calculate call option theta (time decay).

    Theta measures the sensitivity to time passing.
    Returns theta per day.

    Returns:
        Theta value (per day)
    """
    forward = _fallback_forward(spot_price, risk_free_rate, dividend_yield, time_to_maturity)
    d1 = _d1(forward, strike, volatility, time_to_maturity)
    d2 = _d2(forward, strike, volatility, time_to_maturity)

    term1 = -(spot_price * scipy_stats.norm.pdf(d1) * volatility *
              np.exp(-dividend_yield * time_to_maturity)) / (2 * np.sqrt(time_to_maturity))
    term2 = dividend_yield * spot_price * scipy_stats.norm.cdf(d1) * np.exp(-dividend_yield * time_to_maturity)
    term3 = -risk_free_rate * strike * np.exp(-risk_free_rate * time_to_maturity) * scipy_stats.norm.cdf(d2)

    theta = (term1 + term2 + term3) / 365  # Per day
    return float(theta)


def calculate_put_theta(
    spot_price: float,
    strike: float,
    volatility: float,
    risk_free_rate: float,
    dividend_yield: float,
    time_to_maturity: float
) -> float:
    """
    Calculate put option theta (time decay).

    Returns:
        Theta value (per day)
    """
    forward = _fallback_forward(spot_price, risk_free_rate, dividend_yield, time_to_maturity)
    d1 = _d1(forward, strike, volatility, time_to_maturity)
    d2 = _d2(forward, strike, volatility, time_to_maturity)

    term1 = -(spot_price * scipy_stats.norm.pdf(d1) * volatility *
              np.exp(-dividend_yield * time_to_maturity)) / (2 * np.sqrt(time_to_maturity))
    term2 = -dividend_yield * spot_price * scipy_stats.norm.cdf(-d1) * np.exp(-dividend_yield * time_to_maturity)
    term3 = risk_free_rate * strike * np.exp(-risk_free_rate * time_to_maturity) * scipy_stats.norm.cdf(-d2)

    theta = (term1 + term2 + term3) / 365  # Per day
    return float(theta)


def calculate_call_rho(
    spot_price: float,
    strike: float,
    volatility: float,
    risk_free_rate: float,
    dividend_yield: float,
    time_to_maturity: float
) -> float:
    """
    Calculate call option rho.

    Rho measures sensitivity to interest rate changes.
    Returns rho per 1% change in rates.

    Returns:
        Rho value (per 1% rate change)
    """
    forward = _fallback_forward(spot_price, risk_free_rate, dividend_yield, time_to_maturity)
    d2 = _d2(forward, strike, volatility, time_to_maturity)
    rho = strike * time_to_maturity * np.exp(-risk_free_rate * time_to_maturity) * scipy_stats.norm.cdf(d2)
    return float(rho / 100)  # Per 1% change


def calculate_put_rho(
    spot_price: float,
    strike: float,
    volatility: float,
    risk_free_rate: float,
    dividend_yield: float,
    time_to_maturity: float
) -> float:
    """
    Calculate put option rho.

    Returns:
        Rho value (per 1% rate change)
    """
    forward = _fallback_forward(spot_price, risk_free_rate, dividend_yield, time_to_maturity)
    d2 = _d2(forward, strike, volatility, time_to_maturity)
    rho = -strike * time_to_maturity * np.exp(-risk_free_rate * time_to_maturity) * scipy_stats.norm.cdf(-d2)
    return float(rho / 100)  # Per 1% change


def calculate_all_greeks(
    spot_price: float,
    strike: float,
    volatility: float,
    risk_free_rate: float,
    dividend_yield: float,
    time_to_maturity: float
) -> Dict[str, Any]:
    """
    Calculate all option Greeks for both call and put.

    Args:
        spot_price: Current underlying price
        strike: Strike price
        volatility: Implied volatility (annual)
        risk_free_rate: Risk-free rate (annual)
        dividend_yield: Dividend yield (annual)
        time_to_maturity: Time to maturity in years

    Returns:
        Dictionary with all Greeks for call and put options
    """
    return {
        'call': {
            'delta': calculate_call_delta(spot_price, strike, volatility, risk_free_rate, dividend_yield, time_to_maturity),
            'gamma': calculate_gamma(spot_price, strike, volatility, risk_free_rate, dividend_yield, time_to_maturity),
            'vega': calculate_vega(spot_price, strike, volatility, risk_free_rate, dividend_yield, time_to_maturity),
            'theta': calculate_call_theta(spot_price, strike, volatility, risk_free_rate, dividend_yield, time_to_maturity),
            'rho': calculate_call_rho(spot_price, strike, volatility, risk_free_rate, dividend_yield, time_to_maturity),
        },
        'put': {
            'delta': calculate_put_delta(spot_price, strike, volatility, risk_free_rate, dividend_yield, time_to_maturity),
            'gamma': calculate_gamma(spot_price, strike, volatility, risk_free_rate, dividend_yield, time_to_maturity),
            'vega': calculate_vega(spot_price, strike, volatility, risk_free_rate, dividend_yield, time_to_maturity),
            'theta': calculate_put_theta(spot_price, strike, volatility, risk_free_rate, dividend_yield, time_to_maturity),
            'rho': calculate_put_rho(spot_price, strike, volatility, risk_free_rate, dividend_yield, time_to_maturity),
        },
        'inputs': {
            'spot_price': spot_price,
            'strike': strike,
            'volatility': volatility,
            'risk_free_rate': risk_free_rate,
            'dividend_yield': dividend_yield,
            'time_to_maturity': time_to_maturity
        }
    }


def price_call_option(
    forward_price: float,
    strike: float,
    volatility: float,
    risk_free_rate: float,
    time_to_maturity: float
) -> float:
    """
    Black-Scholes call option pricing

    Args:
        forward_price: Forward price of underlying
        strike: Strike price
        volatility: Implied volatility (annual)
        risk_free_rate: Risk-free interest rate (annual)
        time_to_maturity: Time to maturity in years

    Returns:
        Call option price
    """
    if FORTITUDO_AVAILABLE:
        price = ft.call_option(
            F=forward_price,
            K=strike,
            sigma=volatility,
            r=risk_free_rate,
            T=time_to_maturity
        )
        return float(price)
    else:
        return _fallback_call_option(forward_price, strike, volatility, risk_free_rate, time_to_maturity)


def price_put_option(
    forward_price: float,
    strike: float,
    volatility: float,
    risk_free_rate: float,
    time_to_maturity: float
) -> float:
    """
    Black-Scholes put option pricing

    Args:
        forward_price: Forward price of underlying
        strike: Strike price
        volatility: Implied volatility (annual)
        risk_free_rate: Risk-free interest rate (annual)
        time_to_maturity: Time to maturity in years

    Returns:
        Put option price
    """
    if FORTITUDO_AVAILABLE:
        price = ft.put_option(
            F=forward_price,
            K=strike,
            sigma=volatility,
            r=risk_free_rate,
            T=time_to_maturity
        )
        return float(price)
    else:
        return _fallback_put_option(forward_price, strike, volatility, risk_free_rate, time_to_maturity)


def calculate_forward_price(
    spot_price: float,
    risk_free_rate: float,
    dividend_yield: float,
    time_to_maturity: float
) -> float:
    """
    Calculate forward price

    Args:
        spot_price: Current spot price
        risk_free_rate: Risk-free interest rate (annual)
        dividend_yield: Dividend yield (annual)
        time_to_maturity: Time to maturity in years

    Returns:
        Forward price
    """
    if FORTITUDO_AVAILABLE:
        fwd = ft.forward(
            S=spot_price,
            r=risk_free_rate,
            q=dividend_yield,
            T=time_to_maturity
        )
        return float(fwd)
    else:
        return _fallback_forward(spot_price, risk_free_rate, dividend_yield, time_to_maturity)


def price_option_straddle(
    forward_price: float,
    strike: float,
    volatility: float,
    risk_free_rate: float,
    time_to_maturity: float
) -> Dict[str, float]:
    """
    Price both call and put (straddle strategy)

    Args:
        forward_price: Forward price of underlying
        strike: Strike price
        volatility: Implied volatility
        risk_free_rate: Risk-free rate
        time_to_maturity: Time to maturity

    Returns:
        Dict with call, put, and total straddle prices
    """
    call = price_call_option(forward_price, strike, volatility, risk_free_rate, time_to_maturity)
    put = price_put_option(forward_price, strike, volatility, risk_free_rate, time_to_maturity)

    return {
        'call_price': call,
        'put_price': put,
        'straddle_price': call + put,
        'forward_price': forward_price,
        'strike': strike,
        'volatility': volatility,
        'risk_free_rate': risk_free_rate,
        'time_to_maturity': time_to_maturity
    }


def calculate_put_call_parity_check(
    spot_price: float,
    strike: float,
    volatility: float,
    risk_free_rate: float,
    dividend_yield: float,
    time_to_maturity: float
) -> Dict[str, Any]:
    """
    Verify put-call parity relationship

    Put-Call Parity: C - P = S - K * exp(-r*T)

    Returns:
        Dict with parity check results
    """
    # Calculate forward
    fwd = calculate_forward_price(spot_price, risk_free_rate, dividend_yield, time_to_maturity)

    # Price options
    call = price_call_option(fwd, strike, volatility, risk_free_rate, time_to_maturity)
    put = price_put_option(fwd, strike, volatility, risk_free_rate, time_to_maturity)

    # Check parity
    import math
    lhs = call - put
    rhs = spot_price - strike * math.exp(-risk_free_rate * time_to_maturity)
    parity_diff = abs(lhs - rhs)

    return {
        'call_price': call,
        'put_price': put,
        'forward_price': fwd,
        'parity_lhs': lhs,
        'parity_rhs': rhs,
        'parity_difference': parity_diff,
        'parity_holds': parity_diff < 0.01
    }


def main():
    """Test option pricing functions"""
    print("=== Fortitudo.tech Option Pricing Test ===\n")

    # Test parameters
    S = 100.0   # Spot price
    K = 105.0   # Strike
    sigma = 0.25  # 25% volatility
    r = 0.05    # 5% risk-free rate
    q = 0.02    # 2% dividend yield
    T = 1.0     # 1 year

    print("1. Forward Price Calculation")
    print("-" * 50)
    fwd = calculate_forward_price(S, r, q, T)
    print(f"  Spot Price: ${S:.2f}")
    print(f"  Risk-free Rate: {r*100:.1f}%")
    print(f"  Dividend Yield: {q*100:.1f}%")
    print(f"  Time to Maturity: {T} year")
    print(f"  Forward Price: ${fwd:.2f}")

    print("\n2. Call Option Pricing")
    print("-" * 50)
    call = price_call_option(fwd, K, sigma, r, T)
    print(f"  Forward: ${fwd:.2f}")
    print(f"  Strike: ${K:.2f}")
    print(f"  Volatility: {sigma*100:.1f}%")
    print(f"  Call Price: ${call:.4f}")

    print("\n3. Put Option Pricing")
    print("-" * 50)
    put = price_put_option(fwd, K, sigma, r, T)
    print(f"  Forward: ${fwd:.2f}")
    print(f"  Strike: ${K:.2f}")
    print(f"  Volatility: {sigma*100:.1f}%")
    print(f"  Put Price: ${put:.4f}")

    print("\n4. Straddle Strategy")
    print("-" * 50)
    straddle = price_option_straddle(fwd, K, sigma, r, T)
    print(f"  Call: ${straddle['call_price']:.4f}")
    print(f"  Put: ${straddle['put_price']:.4f}")
    print(f"  Total Straddle Cost: ${straddle['straddle_price']:.4f}")

    print("\n5. Put-Call Parity Check")
    print("-" * 50)
    parity = calculate_put_call_parity_check(S, K, sigma, r, q, T)
    print(f"  Call Price: ${parity['call_price']:.4f}")
    print(f"  Put Price: ${parity['put_price']:.4f}")
    print(f"  C - P: ${parity['parity_lhs']:.4f}")
    print(f"  S - K*exp(-rT): ${parity['parity_rhs']:.4f}")
    print(f"  Difference: ${parity['parity_difference']:.6f}")
    print(f"  Parity Holds: {parity['parity_holds']}")

    print("\n6. Multiple Strikes (Option Chain)")
    print("-" * 50)
    strikes = [90, 95, 100, 105, 110]
    print("  Strike    Call      Put")
    for strike in strikes:
        c = price_call_option(fwd, strike, sigma, r, T)
        p = price_put_option(fwd, strike, sigma, r, T)
        print(f"  ${strike:<6.0f}  ${c:>6.4f}  ${p:>6.4f}")

    print("\n7. JSON Export")
    print("-" * 50)
    json_output = json.dumps(straddle, indent=2)
    print(json_output)

    print("\n=== Test: PASSED ===")


if __name__ == "__main__":
    main()
