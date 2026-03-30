"""
QuantStats Monte Carlo Simulation
Input (stdin JSON): {"symbols": ["AAPL","MSFT"], "weights": [0.6, 0.4], "num_simulations": 1000}
Output (stdout JSON): {
    "median_return": float (%),
    "percentile_5": float (%),
    "percentile_95": float (%),
    "prob_loss": float (%),
    "expected_max_dd": float (%),
    "paths": [[252 cumulative-return values as %], ...],  -- first 50 paths only
    "num_paths_shown": int
}
All monetary/return values are in percent (multiplied by 100).
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
        v = float(obj)
        if np.isnan(v) or np.isinf(v):
            return 0.0
        return v
    elif isinstance(obj, np.ndarray):
        return [convert_numpy(x) for x in obj]
    elif isinstance(obj, float):
        if np.isnan(obj) or np.isinf(obj):
            return 0.0
    return obj


def compute_max_drawdown_paths(paths):
    """Compute max drawdown for each simulated path (array of shape [num_sims, steps])."""
    # paths shape: (num_sims, 252)  — price levels starting at 1.0
    peak = np.maximum.accumulate(paths, axis=1)
    drawdown = (paths - peak) / peak  # negative values
    max_dd = drawdown.min(axis=1)     # worst drawdown per path (negative)
    return max_dd


def run_simulation(symbols, weights, num_simulations=1000):
    import yfinance as yf

    # Download 2 years of daily close prices
    tickers = " ".join(symbols) if len(symbols) > 1 else symbols[0]
    data = yf.download(tickers, period="2y", interval="1d", progress=False, auto_adjust=True)

    if data is None or data.empty:
        return {"error": "Could not fetch price data for symbols: " + str(symbols)}

    # Extract close prices
    if len(symbols) == 1:
        import pandas as pd
        close = data["Close"]
        if not isinstance(close, pd.DataFrame):
            close = pd.DataFrame({symbols[0]: close})
    else:
        close = data["Close"]

    # Drop rows with all NaN
    close = close.dropna(how="all")

    # Align columns to requested symbols, filling missing with forward-fill
    available = [s for s in symbols if s in close.columns]
    if not available:
        return {"error": "None of the requested symbols found in downloaded data"}

    close = close[available].ffill().bfill().dropna()

    if len(close) < 30:
        return {"error": "Insufficient historical data (need at least 30 trading days)"}

    # Recompute weights for available symbols only
    w = np.array([weights[symbols.index(s)] for s in available], dtype=float)
    if w.sum() == 0:
        w = np.ones(len(available)) / len(available)
    else:
        w = w / w.sum()  # renormalise in case some symbols dropped

    # Daily returns
    daily_returns = close.pct_change().dropna()

    # Weighted portfolio daily returns
    port_returns = (daily_returns * w).sum(axis=1).values.astype(float)

    # Fit GBM parameters from historical returns
    mu_daily = float(np.mean(port_returns))
    sigma_daily = float(np.std(port_returns, ddof=1))

    if sigma_daily < 1e-8:
        sigma_daily = 1e-4  # guard against degenerate input

    # Annualised equivalents (for reference only — simulation uses daily params)
    # mu_ann = mu_daily * 252
    # sigma_ann = sigma_daily * np.sqrt(252)

    # GBM simulation
    # S_t = S_{t-1} * exp((mu - 0.5*sigma^2)*dt + sigma*sqrt(dt)*Z)
    dt = 1.0  # daily steps, dt = 1 day / 1 day = 1
    n_steps = 252
    drift = (mu_daily - 0.5 * sigma_daily ** 2) * dt
    diffusion = sigma_daily * np.sqrt(dt)

    rng = np.random.default_rng(seed=42)
    Z = rng.standard_normal((num_simulations, n_steps))

    # Log returns per step
    log_returns = drift + diffusion * Z  # shape: (num_sims, n_steps)

    # Cumulative price levels: S_0 = 1.0
    log_cumulative = np.cumsum(log_returns, axis=1)
    price_paths = np.exp(log_cumulative)  # shape: (num_sims, n_steps), starts from step 1

    # Terminal returns: (S_252 - S_0) / S_0  where S_0 = 1.0
    terminal_returns = price_paths[:, -1] - 1.0  # shape: (num_sims,)

    # Max drawdown per path (price paths need S_0 prepended)
    full_paths = np.hstack([np.ones((num_simulations, 1)), price_paths])  # shape: (num_sims, 253)
    max_dd_per_path = compute_max_drawdown_paths(full_paths)  # shape: (num_sims,), negative values

    # Summary statistics (all in %)
    median_return = float(np.median(terminal_returns)) * 100.0
    percentile_5 = float(np.percentile(terminal_returns, 5)) * 100.0
    percentile_95 = float(np.percentile(terminal_returns, 95)) * 100.0
    prob_loss = float(np.mean(terminal_returns < 0)) * 100.0
    expected_max_dd = float(np.mean(max_dd_per_path)) * 100.0

    # Build paths for chart rendering (first 50 paths only, cumulative return %)
    # Each path is 252 values: cumulative return at each day as %
    n_paths_shown = min(50, num_simulations)
    paths_for_chart = []
    for i in range(n_paths_shown):
        # price_paths[i] is S_1..S_252 relative to S_0=1
        cum_ret_pct = [(float(price_paths[i, t]) - 1.0) * 100.0 for t in range(n_steps)]
        paths_for_chart.append(cum_ret_pct)

    return {
        "median_return": median_return,
        "percentile_5": percentile_5,
        "percentile_95": percentile_95,
        "prob_loss": prob_loss,
        "expected_max_dd": expected_max_dd,
        "paths": paths_for_chart,
        "num_paths_shown": n_paths_shown,
    }


def main():
    try:
        stdin_data = sys.stdin.read()
        if not stdin_data.strip():
            print(json.dumps({"error": "No input data"}))
            return

        params = json.loads(stdin_data)
        symbols = params.get("symbols", [])
        weights = params.get("weights", [])
        num_simulations = int(params.get("num_simulations", 1000))

        if not symbols:
            print(json.dumps({"error": "No symbols provided"}))
            return

        if not weights or len(weights) != len(symbols):
            weights = [1.0 / len(symbols)] * len(symbols)

        # Clamp simulations to a reasonable range
        num_simulations = max(100, min(num_simulations, 5000))

        result = run_simulation(symbols, weights, num_simulations)
        print(json.dumps(convert_numpy(result)))

    except Exception as exc:
        print(json.dumps({"error": str(exc)}))


if __name__ == "__main__":
    main()
