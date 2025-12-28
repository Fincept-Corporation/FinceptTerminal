from typing import Dict
from py_vollib.black_scholes_merton import black_scholes_merton
from py_vollib.black_scholes_merton.greeks.analytical import delta, gamma, vega, theta, rho
from py_vollib.black_scholes_merton.implied_volatility import implied_volatility

def calculate_bsm_price(S: float, K: float, t: float, r: float, sigma: float, q: float, flag: str) -> Dict:
    price = black_scholes_merton(flag, S, K, t, r, sigma, q)
    return {
        'price': float(price),
        'S': S,
        'K': K,
        't': t,
        'r': r,
        'sigma': sigma,
        'q': q,
        'flag': flag
    }

def calculate_bsm_greeks(S: float, K: float, t: float, r: float, sigma: float, q: float, flag: str) -> Dict:
    d = delta(flag, S, K, t, r, sigma, q)
    g = gamma(flag, S, K, t, r, sigma, q)
    v = vega(flag, S, K, t, r, sigma, q)
    th = theta(flag, S, K, t, r, sigma, q)
    rh = rho(flag, S, K, t, r, sigma, q)

    return {
        'delta': float(d),
        'gamma': float(g),
        'vega': float(v),
        'theta': float(th),
        'rho': float(rh)
    }

def calculate_bsm_iv(price: float, S: float, K: float, t: float, r: float, q: float, flag: str) -> Dict:
    iv = implied_volatility(price, S, K, t, r, q, flag)
    return {
        'implied_volatility': float(iv),
        'price': price,
        'S': S,
        'K': K,
        't': t,
        'r': r,
        'q': q,
        'flag': flag
    }

def main():
    print("Testing py_vollib Black-Scholes-Merton Model")

    S = 100
    K = 100
    t = 0.25
    r = 0.05
    sigma = 0.2
    q = 0.02
    flag = 'c'

    print("\n1. Testing BSM Price...")
    result = calculate_bsm_price(S, K, t, r, sigma, q, flag)
    print(f"Call Price: {result['price']:.4f}")
    assert result['price'] > 0
    print("Test 1: PASSED")

    print("\n2. Testing BSM Greeks...")
    result = calculate_bsm_greeks(S, K, t, r, sigma, q, flag)
    print(f"Delta: {result['delta']:.4f}")
    print(f"Gamma: {result['gamma']:.4f}")
    print(f"Vega: {result['vega']:.4f}")
    assert result['delta'] > 0
    print("Test 2: PASSED")

    print("\n3. Testing BSM IV...")
    price = 3.0
    result = calculate_bsm_iv(price, S, K, t, r, q, flag)
    print(f"Implied Volatility: {result['implied_volatility']:.4f}")
    assert result['implied_volatility'] > 0
    print("Test 3: PASSED")

    print("\nAll tests: PASSED")

if __name__ == "__main__":
    main()
