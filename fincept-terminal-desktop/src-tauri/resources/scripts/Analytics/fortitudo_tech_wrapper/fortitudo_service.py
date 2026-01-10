"""
Fortitudo.tech Service - Python backend for portfolio risk analytics via PyO3
===============================================================================

Provides JSON-RPC interface for Rust/Tauri to call fortitudo.tech analytics functions.
Includes portfolio risk metrics, option pricing, entropy pooling, and exposure stacking.

Supports Python 3.14+ via pure NumPy/SciPy fallback implementations when fortitudo.tech
is not available (requires Python <3.14).
"""

import json
import sys
import os
from typing import Dict, List, Any, Optional
from datetime import datetime

# Add this script's directory to Python path for local imports
_script_dir = os.path.dirname(os.path.abspath(__file__))
if _script_dir not in sys.path:
    sys.path.insert(0, _script_dir)

import pandas as pd
import numpy as np

try:
    import fortitudo.tech as ft
    FORTITUDO_AVAILABLE = True
    FORTITUDO_VERSION = getattr(ft, '__version__', '1.2')
except ImportError:
    FORTITUDO_AVAILABLE = False
    FORTITUDO_VERSION = 'fallback'

# Import local wrapper modules (with fallback support)
try:
    from functions import (
        calculate_portfolio_volatility,
        calculate_portfolio_var,
        calculate_portfolio_cvar,
        calculate_covariance_matrix,
        calculate_correlation_matrix,
        calculate_simulation_moments,
        calculate_exp_decay_probabilities,
        calculate_normal_calibration,
        calculate_all_metrics,
        FORTITUDO_AVAILABLE as FUNCTIONS_AVAILABLE
    )
    from option_pricing import (
        price_call_option,
        price_put_option,
        calculate_forward_price,
        price_option_straddle,
        calculate_put_call_parity_check,
        calculate_all_greeks,
        FORTITUDO_AVAILABLE as OPTIONS_AVAILABLE
    )
    from advanced import (
        apply_entropy_pooling,
        apply_entropy_pooling_simple,
        calculate_exposure_stacking,
        FORTITUDO_AVAILABLE as ADVANCED_AVAILABLE
    )
    from optimization import (
        MeanVarianceOptimizer,
        MeanCVaROptimizer
    )
    WRAPPERS_AVAILABLE = True
    OPTIMIZATION_AVAILABLE = True
except ImportError as e:
    WRAPPERS_AVAILABLE = False
    OPTIMIZATION_AVAILABLE = False
    print(f"[fortitudo_service] Warning: Could not import wrappers: {e}", file=sys.stderr)


def serialize_result(data: Any) -> str:
    """Serialize result to JSON, handling numpy/pandas types"""
    def convert(obj):
        if obj is None:
            return None
        if isinstance(obj, (np.integer, np.floating)):
            if np.isnan(obj):
                return None
            return float(obj)
        elif isinstance(obj, np.ndarray):
            return obj.tolist()
        elif isinstance(obj, np.bool_):
            return bool(obj)
        elif isinstance(obj, pd.DataFrame):
            # Handle DataFrames with tuple column names (MultiIndex)
            result = {}
            for col in obj.columns:
                col_key = str(col) if isinstance(col, tuple) else col
                result[col_key] = {}
                for idx in obj.index:
                    idx_key = str(idx) if isinstance(idx, tuple) else str(idx)
                    result[col_key][idx_key] = convert(obj.loc[idx, col])
            return result
        elif isinstance(obj, pd.Series):
            # Handle Series with tuple index
            return {str(k) if isinstance(k, tuple) else str(k): convert(v) for k, v in obj.items()}
        elif isinstance(obj, pd.Timestamp):
            return str(obj)
        elif isinstance(obj, datetime):
            return obj.isoformat()
        elif isinstance(obj, dict):
            # Convert tuple keys to strings for JSON compatibility
            return {str(k) if isinstance(k, tuple) else k: convert(v) for k, v in obj.items()}
        elif isinstance(obj, (list, tuple)):
            return [convert(i) for i in obj]
        return obj

    return json.dumps(convert(data), default=str)


def parse_returns_json(returns_json: str) -> pd.DataFrame:
    """Parse JSON returns data into DataFrame"""
    data = json.loads(returns_json)

    if isinstance(data, dict):
        first_value = next(iter(data.values()), None)
        if isinstance(first_value, dict):
            # Multi-asset: {symbol: {date: return}}
            df = pd.DataFrame(data)
            df.index = pd.to_datetime(df.index)
        else:
            # Single asset: {date: return}
            df = pd.DataFrame({'returns': data})
            df.index = pd.to_datetime(df.index)
    elif isinstance(data, list):
        df = pd.DataFrame(data)
        if 'date' in df.columns:
            df['date'] = pd.to_datetime(df['date'])
            df.set_index('date', inplace=True)
        if 'symbol' in df.columns:
            df = df.pivot(columns='symbol', values='returns')
    else:
        raise ValueError(f"Unsupported returns data format: {type(data)}")

    df = df.sort_index()
    return df


def parse_weights(weights_json: str) -> np.ndarray:
    """Parse JSON weights into numpy array"""
    data = json.loads(weights_json)
    if isinstance(data, list):
        return np.array(data)
    elif isinstance(data, dict):
        return np.array(list(data.values()))
    else:
        raise ValueError(f"Unsupported weights format: {type(data)}")


# ============================================================================
# COMMAND HANDLERS
# ============================================================================

def check_status() -> Dict[str, Any]:
    """Check fortitudo.tech library status"""
    mode = "native" if FORTITUDO_AVAILABLE else "fallback"
    return {
        "success": True,
        "available": WRAPPERS_AVAILABLE,  # Changed: now True even in fallback mode
        "native_available": FORTITUDO_AVAILABLE,
        "wrappers_available": WRAPPERS_AVAILABLE,
        "mode": mode,
        "version": FORTITUDO_VERSION,
        "message": f"Fortitudo.tech analytics ready ({mode} mode)"
    }


def portfolio_metrics(params: Dict[str, Any]) -> Dict[str, Any]:
    """Calculate comprehensive portfolio risk metrics"""
    if not WRAPPERS_AVAILABLE:
        return {"success": False, "error": "Wrapper modules not available"}

    try:
        returns = parse_returns_json(params['returns'])
        weights = parse_weights(params['weights'])
        alpha = params.get('alpha', 0.05)

        # Get optional probabilities
        probabilities = None
        if 'probabilities' in params and params['probabilities']:
            probabilities = np.array(json.loads(params['probabilities']))

        # Calculate all metrics
        metrics = calculate_all_metrics(weights, returns, probabilities, alpha)

        return {
            "success": True,
            "metrics": metrics,
            "n_scenarios": len(returns),
            "n_assets": len(weights)
        }
    except Exception as e:
        return {"success": False, "error": str(e)}


def covariance_analysis(params: Dict[str, Any]) -> Dict[str, Any]:
    """Calculate covariance and correlation matrices"""
    if not WRAPPERS_AVAILABLE:
        return {"success": False, "error": "Wrapper modules not available"}

    try:
        returns = parse_returns_json(params['returns'])

        probabilities = None
        if 'probabilities' in params and params['probabilities']:
            probabilities = np.array(json.loads(params['probabilities']))

        cov_matrix = calculate_covariance_matrix(returns, probabilities)
        corr_matrix = calculate_correlation_matrix(returns, probabilities)
        moments = calculate_simulation_moments(returns, probabilities)

        return {
            "success": True,
            "covariance_matrix": cov_matrix.to_dict() if isinstance(cov_matrix, pd.DataFrame) else cov_matrix,
            "correlation_matrix": corr_matrix.to_dict() if isinstance(corr_matrix, pd.DataFrame) else corr_matrix,
            "moments": moments.to_dict() if isinstance(moments, pd.DataFrame) else moments,
            "assets": list(returns.columns) if hasattr(returns, 'columns') else None
        }
    except Exception as e:
        return {"success": False, "error": str(e)}


def exp_decay_weighting(params: Dict[str, Any]) -> Dict[str, Any]:
    """Calculate exponentially decaying probabilities"""
    if not WRAPPERS_AVAILABLE:
        return {"success": False, "error": "Wrapper modules not available"}

    try:
        returns = parse_returns_json(params['returns'])
        half_life = params.get('half_life', 252)

        probs = calculate_exp_decay_probabilities(returns, half_life)

        return {
            "success": True,
            "probabilities": probs.flatten().tolist(),
            "half_life": half_life,
            "n_scenarios": len(probs),
            "sum": float(probs.sum())
        }
    except Exception as e:
        return {"success": False, "error": str(e)}


def option_pricing(params: Dict[str, Any]) -> Dict[str, Any]:
    """Price options using Black-Scholes model"""
    if not WRAPPERS_AVAILABLE:
        return {"success": False, "error": "Wrapper modules not available"}

    try:
        spot = params['spot_price']
        strike = params['strike']
        volatility = params['volatility']
        risk_free_rate = params.get('risk_free_rate', 0.05)
        dividend_yield = params.get('dividend_yield', 0.0)
        time_to_maturity = params.get('time_to_maturity', 1.0)

        # Calculate forward price
        forward = calculate_forward_price(spot, risk_free_rate, dividend_yield, time_to_maturity)

        # Price call and put
        call_price = price_call_option(forward, strike, volatility, risk_free_rate, time_to_maturity)
        put_price = price_put_option(forward, strike, volatility, risk_free_rate, time_to_maturity)

        # Straddle
        straddle = price_option_straddle(forward, strike, volatility, risk_free_rate, time_to_maturity)

        # Put-call parity check
        parity = calculate_put_call_parity_check(spot, strike, volatility, risk_free_rate, dividend_yield, time_to_maturity)

        return {
            "success": True,
            "forward_price": forward,
            "call_price": call_price,
            "put_price": put_price,
            "straddle": straddle,
            "parity_check": parity
        }
    except Exception as e:
        return {"success": False, "error": str(e)}


def option_chain(params: Dict[str, Any]) -> Dict[str, Any]:
    """Generate option chain for multiple strikes"""
    if not WRAPPERS_AVAILABLE:
        return {"success": False, "error": "Wrapper modules not available"}

    try:
        spot = params['spot_price']
        strikes = params.get('strikes', [spot * 0.9, spot * 0.95, spot, spot * 1.05, spot * 1.1])
        volatility = params['volatility']
        risk_free_rate = params.get('risk_free_rate', 0.05)
        dividend_yield = params.get('dividend_yield', 0.0)
        time_to_maturity = params.get('time_to_maturity', 1.0)

        forward = calculate_forward_price(spot, risk_free_rate, dividend_yield, time_to_maturity)

        chain = []
        for strike in strikes:
            call = price_call_option(forward, strike, volatility, risk_free_rate, time_to_maturity)
            put = price_put_option(forward, strike, volatility, risk_free_rate, time_to_maturity)
            chain.append({
                "strike": strike,
                "call_price": call,
                "put_price": put,
                "moneyness": strike / spot
            })

        return {
            "success": True,
            "spot_price": spot,
            "forward_price": forward,
            "chain": chain
        }
    except Exception as e:
        return {"success": False, "error": str(e)}


def entropy_pooling(params: Dict[str, Any]) -> Dict[str, Any]:
    """Apply entropy pooling for scenario probability adjustment"""
    if not WRAPPERS_AVAILABLE:
        return {"success": False, "error": "Wrapper modules not available"}

    try:
        n_scenarios = params['n_scenarios']
        max_probability = params.get('max_probability', None)

        result = apply_entropy_pooling_simple(
            n_scenarios=n_scenarios,
            max_probability=max_probability
        )

        return {
            "success": True,
            "prior_probabilities": result['prior_probabilities'].tolist(),
            "posterior_probabilities": result['posterior_probabilities'].tolist(),
            "effective_scenarios_prior": result['effective_scenarios_prior'],
            "effective_scenarios_posterior": result['effective_scenarios_posterior'],
            "max_probability": result['max_probability'],
            "min_probability": result['min_probability']
        }
    except Exception as e:
        return {"success": False, "error": str(e)}


def exposure_stacking(params: Dict[str, Any]) -> Dict[str, Any]:
    """Calculate exposure stacking portfolio"""
    if not WRAPPERS_AVAILABLE:
        return {"success": False, "error": "Wrapper modules not available"}

    try:
        sample_portfolios = np.array(json.loads(params['sample_portfolios']))
        n_partitions = params.get('n_partitions', 4)

        result = calculate_exposure_stacking(sample_portfolios, n_partitions)

        return {
            "success": True,
            "stacked_weights": result['stacked_weights'],
            "mean_sample_weights": result['mean_sample_weights'],
            "std_sample_weights": result['std_sample_weights'],
            "n_assets": result['n_assets'],
            "n_samples": result['n_samples'],
            "n_partitions": result['n_partitions'],
            "weights_sum": result['weights_sum']
        }
    except Exception as e:
        return {"success": False, "error": str(e)}


def full_analysis(params: Dict[str, Any]) -> Dict[str, Any]:
    """Complete portfolio risk analysis"""
    if not WRAPPERS_AVAILABLE:
        return {"success": False, "error": "Wrapper modules not available"}

    try:
        returns = parse_returns_json(params['returns'])
        weights = parse_weights(params['weights'])
        alpha = params.get('alpha', 0.05)
        half_life = params.get('half_life', 252)

        # Calculate exp decay probabilities
        probs = calculate_exp_decay_probabilities(returns, half_life)

        # Calculate metrics with equal weights
        metrics_equal = calculate_all_metrics(weights, returns, None, alpha)

        # Calculate metrics with exp decay weights
        metrics_weighted = calculate_all_metrics(weights, returns, probs, alpha)

        # Covariance and correlation
        cov_matrix = calculate_covariance_matrix(returns)
        corr_matrix = calculate_correlation_matrix(returns)

        # Moments
        moments = calculate_simulation_moments(returns)

        return {
            "success": True,
            "analysis": {
                "metrics_equal_weight": metrics_equal,
                "metrics_exp_decay": metrics_weighted,
                "covariance_matrix": cov_matrix.to_dict() if isinstance(cov_matrix, pd.DataFrame) else None,
                "correlation_matrix": corr_matrix.to_dict() if isinstance(corr_matrix, pd.DataFrame) else None,
                "moments": moments.to_dict() if isinstance(moments, pd.DataFrame) else None,
                "half_life": half_life,
                "alpha": alpha,
                "n_scenarios": len(returns),
                "n_assets": len(weights),
                "assets": list(returns.columns) if hasattr(returns, 'columns') else None
            }
        }
    except Exception as e:
        return {"success": False, "error": str(e)}


# ============================================================================
# MAIN ENTRY POINT
# ============================================================================

def option_greeks(params: Dict[str, Any]) -> Dict[str, Any]:
    """Calculate option Greeks (Delta, Gamma, Vega, Theta, Rho)"""
    if not WRAPPERS_AVAILABLE:
        return {"success": False, "error": "Wrapper modules not available"}

    try:
        spot = params['spot_price']
        strike = params['strike']
        volatility = params['volatility']
        risk_free_rate = params.get('risk_free_rate', 0.05)
        dividend_yield = params.get('dividend_yield', 0.0)
        time_to_maturity = params.get('time_to_maturity', 1.0)

        greeks = calculate_all_greeks(
            spot_price=spot,
            strike=strike,
            volatility=volatility,
            risk_free_rate=risk_free_rate,
            dividend_yield=dividend_yield,
            time_to_maturity=time_to_maturity
        )

        return {
            "success": True,
            **greeks
        }
    except Exception as e:
        return {"success": False, "error": str(e)}


def optimize_mean_variance(params: Dict[str, Any]) -> Dict[str, Any]:
    """Mean-Variance portfolio optimization"""
    if not OPTIMIZATION_AVAILABLE:
        return {"success": False, "error": "Optimization module not available"}

    try:
        returns = parse_returns_json(params['returns'])
        objective = params.get('objective', 'min_variance')  # min_variance, max_sharpe, target_return
        long_only = params.get('long_only', True)
        max_weight = params.get('max_weight', None)
        min_weight = params.get('min_weight', None)
        risk_free_rate = params.get('risk_free_rate', 0.0)
        target_return = params.get('target_return', None)

        optimizer = MeanVarianceOptimizer(returns, risk_free_rate=risk_free_rate)

        if objective == 'min_variance':
            result = optimizer.minimum_variance(long_only=long_only, max_weight=max_weight, min_weight=min_weight)
        elif objective == 'max_sharpe':
            result = optimizer.max_sharpe(long_only=long_only, max_weight=max_weight)
        elif objective == 'target_return' and target_return is not None:
            result = optimizer.target_return(target=target_return, long_only=long_only, max_weight=max_weight)
        else:
            result = optimizer.minimum_variance(long_only=long_only, max_weight=max_weight)

        return result
    except Exception as e:
        return {"success": False, "error": str(e)}


def optimize_mean_cvar(params: Dict[str, Any]) -> Dict[str, Any]:
    """Mean-CVaR portfolio optimization"""
    if not OPTIMIZATION_AVAILABLE:
        return {"success": False, "error": "Optimization module not available"}

    try:
        returns = parse_returns_json(params['returns'])
        objective = params.get('objective', 'min_cvar')  # min_cvar, target_return
        alpha = params.get('alpha', 0.05)
        long_only = params.get('long_only', True)
        max_weight = params.get('max_weight', None)
        min_weight = params.get('min_weight', None)
        risk_free_rate = params.get('risk_free_rate', 0.0)
        target_return = params.get('target_return', None)

        optimizer = MeanCVaROptimizer(returns, alpha=alpha, risk_free_rate=risk_free_rate)

        if objective == 'min_cvar':
            result = optimizer.minimum_cvar(long_only=long_only, max_weight=max_weight, min_weight=min_weight)
        elif objective == 'target_return' and target_return is not None:
            result = optimizer.target_return(target=target_return, long_only=long_only, max_weight=max_weight)
        else:
            result = optimizer.minimum_cvar(long_only=long_only, max_weight=max_weight)

        return result
    except Exception as e:
        return {"success": False, "error": str(e)}


def efficient_frontier_mv(params: Dict[str, Any]) -> Dict[str, Any]:
    """Generate Mean-Variance efficient frontier"""
    if not OPTIMIZATION_AVAILABLE:
        return {"success": False, "error": "Optimization module not available"}

    try:
        returns = parse_returns_json(params['returns'])
        n_points = params.get('n_points', 20)
        long_only = params.get('long_only', True)
        max_weight = params.get('max_weight', None)
        risk_free_rate = params.get('risk_free_rate', 0.0)

        optimizer = MeanVarianceOptimizer(returns, risk_free_rate=risk_free_rate)
        result = optimizer.efficient_frontier(n_points=n_points, long_only=long_only, max_weight=max_weight)

        return result
    except Exception as e:
        return {"success": False, "error": str(e)}


def efficient_frontier_cvar(params: Dict[str, Any]) -> Dict[str, Any]:
    """Generate Mean-CVaR efficient frontier"""
    if not OPTIMIZATION_AVAILABLE:
        return {"success": False, "error": "Optimization module not available"}

    try:
        returns = parse_returns_json(params['returns'])
        n_points = params.get('n_points', 20)
        alpha = params.get('alpha', 0.05)
        long_only = params.get('long_only', True)
        max_weight = params.get('max_weight', None)

        optimizer = MeanCVaROptimizer(returns, alpha=alpha)
        result = optimizer.efficient_frontier(n_points=n_points, long_only=long_only, max_weight=max_weight)

        return result
    except Exception as e:
        return {"success": False, "error": str(e)}


def main(args: list) -> str:
    """Main entry point for Tauri command execution"""
    if not args:
        return serialize_result({"success": False, "error": "No command specified"})

    command = args[0]

    # Parse params - either from args or stdin
    params = {}
    if len(args) > 1:
        if args[1] == "--stdin":
            # Read params from stdin (for large payloads)
            try:
                stdin_data = sys.stdin.read()
                params = json.loads(stdin_data)
            except json.JSONDecodeError as e:
                return serialize_result({"success": False, "error": f"Invalid JSON from stdin: {e}"})
        else:
            try:
                params = json.loads(args[1])
            except json.JSONDecodeError as e:
                return serialize_result({"success": False, "error": f"Invalid JSON params: {e}"})

    # Route commands
    handlers = {
        "check_status": lambda: check_status(),
        "portfolio_metrics": lambda: portfolio_metrics(params),
        "covariance_analysis": lambda: covariance_analysis(params),
        "exp_decay_weighting": lambda: exp_decay_weighting(params),
        "option_pricing": lambda: option_pricing(params),
        "option_chain": lambda: option_chain(params),
        "option_greeks": lambda: option_greeks(params),
        "entropy_pooling": lambda: entropy_pooling(params),
        "exposure_stacking": lambda: exposure_stacking(params),
        "full_analysis": lambda: full_analysis(params),
        "optimize_mean_variance": lambda: optimize_mean_variance(params),
        "optimize_mean_cvar": lambda: optimize_mean_cvar(params),
        "efficient_frontier_mv": lambda: efficient_frontier_mv(params),
        "efficient_frontier_cvar": lambda: efficient_frontier_cvar(params),
    }

    if command in handlers:
        result = handlers[command]()
        return serialize_result(result)
    else:
        return serialize_result({
            "success": False,
            "error": f"Unknown command: {command}",
            "available_commands": list(handlers.keys())
        })


if __name__ == "__main__":
    result = main(sys.argv[1:] if len(sys.argv) > 1 else [])
    print(result)
