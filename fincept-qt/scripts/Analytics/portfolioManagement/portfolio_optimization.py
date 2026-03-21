"""
Portfolio Optimization — Multiple strategies using scipy optimization.
Input: JSON via stdin: {"symbols": ["AAPL","MSFT"], "strategy": "max_sharpe"}
Output: JSON to stdout with weights and performance metrics
"""
import sys
import json
import numpy as np


def convert_numpy(obj):
    if isinstance(obj, dict):
        return {k: convert_numpy(v) for k, v in obj.items()}
    elif isinstance(obj, (list, tuple)):
        return [convert_numpy(v) for v in obj]
    elif isinstance(obj, (np.integer,)):
        return int(obj)
    elif isinstance(obj, (np.floating,)):
        return float(obj)
    elif isinstance(obj, np.ndarray):
        return obj.tolist()
    return obj


def fetch_returns(symbols, period="1y"):
    import yfinance as yf
    data = yf.download(symbols, period=period, interval="1d", progress=False)
    if data is None or data.empty:
        return None
    close = data["Close"]
    if isinstance(close, type(data)):
        close = close[symbols]
    elif len(symbols) == 1:
        import pandas as pd
        close = pd.DataFrame({symbols[0]: close})
    returns = close.pct_change().dropna()
    return returns


def optimize_portfolio(symbols, strategy, params=None):
    if params is None:
        params = {}
    returns = fetch_returns(symbols)
    if returns is None or returns.empty:
        return {"error": "Could not fetch price data"}

    n = len(symbols)
    mean_returns = returns.mean().values * 252
    cov_matrix = returns.cov().values * 252
    rf = 0.04

    from scipy.optimize import minimize

    def neg_sharpe(w):
        port_ret = np.dot(w, mean_returns)
        port_vol = np.sqrt(np.dot(w, np.dot(cov_matrix, w)))
        return -(port_ret - rf) / port_vol if port_vol > 0 else 0

    def port_volatility(w):
        return np.sqrt(np.dot(w, np.dot(cov_matrix, w)))

    def port_return(w):
        return np.dot(w, mean_returns)

    bounds = tuple((0, 1) for _ in range(n))
    constraints = [{"type": "eq", "fun": lambda w: np.sum(w) - 1}]
    w0 = np.ones(n) / n

    try:
        if strategy == "max_sharpe":
            result = minimize(neg_sharpe, w0, bounds=bounds, constraints=constraints, method="SLSQP")
        elif strategy == "min_volatility":
            result = minimize(port_volatility, w0, bounds=bounds, constraints=constraints, method="SLSQP")
        elif strategy == "efficient_risk":
            target_vol = params.get("target_volatility", 0.15)
            constraints.append({"type": "ineq", "fun": lambda w: target_vol - port_volatility(w)})
            result = minimize(lambda w: -port_return(w), w0, bounds=bounds, constraints=constraints, method="SLSQP")
        elif strategy == "efficient_return":
            target_ret = params.get("target_return", 0.10)
            constraints.append({"type": "eq", "fun": lambda w: port_return(w) - target_ret})
            result = minimize(port_volatility, w0, bounds=bounds, constraints=constraints, method="SLSQP")
        elif strategy == "max_quadratic_utility":
            risk_aversion = params.get("risk_aversion", 1.0)
            def neg_utility(w):
                ret = np.dot(w, mean_returns)
                var = np.dot(w, np.dot(cov_matrix, w))
                return -(ret - 0.5 * risk_aversion * var)
            result = minimize(neg_utility, w0, bounds=bounds, constraints=constraints, method="SLSQP")
        elif strategy == "risk_parity":
            def risk_parity_obj(w):
                vol = np.sqrt(np.dot(w, np.dot(cov_matrix, w)))
                if vol == 0:
                    return 0
                marginal = np.dot(cov_matrix, w)
                risk_contrib = w * marginal / vol
                target_rc = vol / n
                return np.sum((risk_contrib - target_rc) ** 2)
            result = minimize(risk_parity_obj, w0, bounds=bounds, constraints=constraints, method="SLSQP")
        elif strategy == "black_litterman":
            # Simple Black-Litterman with market-implied equilibrium
            delta = 2.5
            pi = delta * np.dot(cov_matrix, w0)  # market implied returns
            bl_returns = pi  # Without views, BL returns = equilibrium
            def neg_sharpe_bl(w):
                ret = np.dot(w, bl_returns)
                vol = np.sqrt(np.dot(w, np.dot(cov_matrix, w)))
                return -(ret - rf) / vol if vol > 0 else 0
            result = minimize(neg_sharpe_bl, w0, bounds=bounds, constraints=constraints, method="SLSQP")
        elif strategy == "hrp":
            # Hierarchical Risk Parity via simple inverse-variance
            inv_var = 1.0 / np.diag(cov_matrix)
            weights = inv_var / inv_var.sum()
            result = type("obj", (object,), {"x": weights, "success": True})()
        elif strategy == "custom_constraints":
            # Equal weight as default custom
            weights = np.ones(n) / n
            result = type("obj", (object,), {"x": weights, "success": True})()
        else:
            return {"error": f"Unknown strategy: {strategy}"}

        if not result.success and strategy not in ("hrp", "custom_constraints"):
            return {"error": f"Optimization did not converge: {getattr(result, 'message', '')}"}

        weights = result.x
        port_ret = float(np.dot(weights, mean_returns))
        port_vol = float(np.sqrt(np.dot(weights, np.dot(cov_matrix, weights))))
        sharpe = float((port_ret - rf) / port_vol) if port_vol > 0 else 0

        weights_dict = {symbols[i]: float(weights[i]) for i in range(n)}

        return {
            "weights": weights_dict,
            "expected_annual_return": port_ret,
            "annual_volatility": port_vol,
            "sharpe_ratio": sharpe,
            "strategy": strategy,
        }

    except Exception as e:
        import traceback
        return {"error": str(e), "traceback": traceback.format_exc()}


def main():
    stdin_data = sys.stdin.read()
    if not stdin_data.strip():
        print(json.dumps({"error": "No input data"}))
        return

    params = json.loads(stdin_data)
    symbols = params.get("symbols", [])
    strategy = params.get("strategy", "max_sharpe")

    if not symbols:
        print(json.dumps({"error": "No symbols provided"}))
        return

    result = optimize_portfolio(symbols, strategy, params)
    print(json.dumps(convert_numpy(result)))


if __name__ == "__main__":
    # Support command arg (e.g. "optimize") but ignore it — we always optimize
    main()
