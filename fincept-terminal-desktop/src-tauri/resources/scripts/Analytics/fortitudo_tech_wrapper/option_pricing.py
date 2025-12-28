"""
Fortitudo.tech Option Pricing Wrapper
======================================
Black-Scholes option pricing and forward calculations

Usage:
    python option_pricing.py
"""

import json
from typing import Dict, Any
import fortitudo.tech as ft


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
    price = ft.call_option(
        F=forward_price,
        K=strike,
        sigma=volatility,
        r=risk_free_rate,
        T=time_to_maturity
    )
    return float(price)


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
    price = ft.put_option(
        F=forward_price,
        K=strike,
        sigma=volatility,
        r=risk_free_rate,
        T=time_to_maturity
    )
    return float(price)


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
    fwd = ft.forward(
        S=spot_price,
        r=risk_free_rate,
        q=dividend_yield,
        T=time_to_maturity
    )
    return float(fwd)


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
