from typing import Dict
from py_vollib.black_scholes import black_scholes
from py_vollib.black_scholes.greeks.analytical import delta, gamma, vega, theta, rho
from py_vollib.black_scholes.implied_volatility import implied_volatility

def calculate_bs_price(S: float, K: float, t: float, r: float, sigma: float, flag: str) -> Dict:
    price = black_scholes(flag, S, K, t, r, sigma)
    return {
        'price': float(price),
        'S': S,
        'K': K,
        't': t,
        'r': r,
        'sigma': sigma,
        'flag': flag
    }

def calculate_bs_greeks(S: float, K: float, t: float, r: float, sigma: float, flag: str) -> Dict:
    d = delta(flag, S, K, t, r, sigma)
    g = gamma(flag, S, K, t, r, sigma)
    v = vega(flag, S, K, t, r, sigma)
    th = theta(flag, S, K, t, r, sigma)
    rh = rho(flag, S, K, t, r, sigma)

    return {
        'delta': float(d),
        'gamma': float(g),
        'vega': float(v),
        'theta': float(th),
        'rho': float(rh)
    }

def calculate_bs_iv(price: float, S: float, K: float, t: float, r: float, flag: str) -> Dict:
    iv = implied_volatility(price, S, K, t, r, flag)
    return {
        'implied_volatility': float(iv),
        'price': price,
        'S': S,
        'K': K,
        't': t,
        'r': r,
        'flag': flag
    }

def main():
    print("Testing py_vollib Black-Scholes Model")

    S = 100
    K = 100
    t = 0.25
    r = 0.05
    sigma = 0.2
    flag = 'c'

    print("\n1. Testing Black-Scholes Price...")
    result = calculate_bs_price(S, K, t, r, sigma, flag)
    print(f"Call Price: {result['price']:.4f}")
    assert result['price'] > 0
    print("Test 1: PASSED")

    print("\n2. Testing Black-Scholes Greeks...")
    result = calculate_bs_greeks(S, K, t, r, sigma, flag)
    print(f"Delta: {result['delta']:.4f}")
    print(f"Gamma: {result['gamma']:.4f}")
    print(f"Vega: {result['vega']:.4f}")
    assert result['delta'] > 0
    print("Test 2: PASSED")

    print("\n3. Testing Black-Scholes IV...")
    price = 3.0
    result = calculate_bs_iv(price, S, K, t, r, flag)
    print(f"Implied Volatility: {result['implied_volatility']:.4f}")
    assert result['implied_volatility'] > 0
    print("Test 3: PASSED")

    print("\nAll tests: PASSED")

if __name__ == "__main__":
    main()
