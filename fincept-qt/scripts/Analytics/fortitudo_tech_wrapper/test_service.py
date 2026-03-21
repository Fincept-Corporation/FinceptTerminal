"""Test script for fortitudo_service.py"""

import sys
import os
import json

# Add this directory to path
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import fortitudo_service as fs
import numpy as np

def test_all():
    print("=" * 60)
    print("FORTITUDO.TECH SERVICE TESTS")
    print("=" * 60)
    print()

    # Test 1: Check Status
    print("1. CHECK STATUS")
    print("-" * 40)
    status = fs.check_status()
    print(f"   Available: {status['available']}")
    print(f"   Wrappers: {status['wrappers_available']}")
    print(f"   Version: {status.get('version')}")
    print(f"   Message: {status.get('message')}")
    print()

    if not status['available']:
        print("ERROR: Fortitudo.tech not available, cannot continue tests")
        return

    # Test 2: Option Pricing
    print("2. OPTION PRICING")
    print("-" * 40)
    option_params = {
        'spot_price': 100,
        'strike': 105,
        'volatility': 0.25,
        'risk_free_rate': 0.05,
        'dividend_yield': 0.0,
        'time_to_maturity': 1.0
    }
    opt_result = fs.option_pricing(option_params)
    if opt_result['success']:
        print(f"   Forward Price: ${opt_result['forward_price']:.2f}")
        print(f"   Call Price: ${opt_result['call_price']:.4f}")
        print(f"   Put Price: ${opt_result['put_price']:.4f}")
        print(f"   Straddle: ${opt_result['straddle']['straddle_price']:.4f}")
    else:
        print(f"   ERROR: {opt_result.get('error')}")
    print()

    # Test 3: Entropy Pooling
    print("3. ENTROPY POOLING")
    print("-" * 40)
    ep_params = {
        'n_scenarios': 100,
        'max_probability': 0.03
    }
    ep_result = fs.entropy_pooling(ep_params)
    if ep_result['success']:
        print(f"   Effective Scenarios (Prior): {ep_result['effective_scenarios_prior']:.1f}")
        print(f"   Effective Scenarios (Posterior): {ep_result['effective_scenarios_posterior']:.1f}")
        print(f"   Max Probability: {ep_result['max_probability']*100:.2f}%")
        print(f"   Min Probability: {ep_result['min_probability']*100:.4f}%")
    else:
        print(f"   ERROR: {ep_result.get('error')}")
    print()

    # Test 4: Portfolio Metrics
    print("4. PORTFOLIO METRICS")
    print("-" * 40)

    # Generate sample returns
    np.random.seed(42)
    n_scenarios = 200
    returns = {
        'Stocks': {f'2024-01-{i+1:02d}': float(0.0003 + (np.random.random() - 0.5) * 0.02) for i in range(min(n_scenarios, 28))},
        'Bonds': {f'2024-01-{i+1:02d}': float(0.0001 + (np.random.random() - 0.5) * 0.005) for i in range(min(n_scenarios, 28))}
    }
    weights = [0.6, 0.4]

    params = {
        'returns': json.dumps(returns),
        'weights': json.dumps(weights),
        'alpha': 0.05
    }
    result = fs.portfolio_metrics(params)
    if result['success']:
        m = result['metrics']
        print(f"   Expected Return: {m['expected_return']*100:.4f}%")
        print(f"   Volatility: {m['volatility']*100:.4f}%")
        print(f"   VaR (95%): {m['var']*100:.4f}%")
        print(f"   CVaR (95%): {m['cvar']*100:.4f}%")
        print(f"   Sharpe Ratio: {m['sharpe_ratio']:.3f}")
        print(f"   N Scenarios: {result['n_scenarios']}")
        print(f"   N Assets: {result['n_assets']}")
    else:
        print(f"   ERROR: {result.get('error')}")
    print()

    # Test 5: Full Analysis
    print("5. FULL ANALYSIS")
    print("-" * 40)
    full_params = {
        'returns': json.dumps(returns),
        'weights': json.dumps(weights),
        'alpha': 0.05,
        'half_life': 120
    }
    full_result = fs.full_analysis(full_params)
    if full_result['success']:
        analysis = full_result['analysis']
        print(f"   Equal Weight Sharpe: {analysis['metrics_equal_weight']['sharpe_ratio']:.3f}")
        print(f"   Exp Decay Sharpe: {analysis['metrics_exp_decay']['sharpe_ratio']:.3f}")
        print(f"   Half-Life: {analysis['half_life']} days")
        print(f"   Alpha: {analysis['alpha']}")
    else:
        print(f"   ERROR: {full_result.get('error')}")
    print()

    print("=" * 60)
    print("ALL TESTS COMPLETED SUCCESSFULLY!")
    print("=" * 60)


if __name__ == "__main__":
    test_all()
