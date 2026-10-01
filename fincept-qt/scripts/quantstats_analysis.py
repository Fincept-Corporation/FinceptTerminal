"""
QuantStats Analysis — Comprehensive quantitative statistics for a portfolio.
Input: JSON via stdin: {"symbols": ["AAPL","MSFT"], "weights": [0.5, 0.5]}
Output: JSON to stdout with performance, risk, and ratio metrics
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


def compute_stats(symbols, weights, period="1y"):
    import yfinance as yf

    data = yf.download(symbols, period=period, interval="1d", progress=False)
    if data is None or data.empty:
        return {"error": "Could not fetch price data"}

    close = data["Close"]
    if len(symbols) == 1:
        import pandas as pd
        if not isinstance(close, pd.DataFrame):
            close = pd.DataFrame({symbols[0]: close})

    # A symbol yfinance could not price is an all-NaN column; dropna() below would
    # then discard every row. Keep only symbols that have data.
    close = close.dropna(axis=1, how="all")
    if close.shape[1] == 0:
        return {"error": "No price data for any symbol"}

    returns = close.pct_change().dropna()
    if returns.empty:
        return {"error": "Insufficient price history"}

    # Map weights by NAME onto the surviving columns. yfinance sorts columns
    # alphabetically rather than in request order, so positional weights can be
    # applied to the wrong symbols; re-normalise in case some were dropped.
    wmap = {s: float(wt) for s, wt in zip(symbols, weights)}
    w = np.array([wmap.get(str(c), 0.0) for c in close.columns], dtype=float)
    if w.sum() <= 0:
        w = np.ones(returns.shape[1]) / returns.shape[1]
    else:
        w = w / w.sum()

    port_returns = (returns * w).sum(axis=1)
    cumulative = (1 + port_returns).cumprod()

    rf_daily = 0.04 / 252
    trading_days = len(port_returns)
    ann_factor = 252

    total_return = float(cumulative.iloc[-1] / cumulative.iloc[0] - 1) if len(cumulative) > 0 else 0
    ann_return = float((1 + total_return) ** (ann_factor / max(trading_days, 1)) - 1)
    ann_vol = float(port_returns.std() * np.sqrt(ann_factor))
    sharpe = float((ann_return - 0.04) / ann_vol) if ann_vol > 0 else 0
    sortino_vol = float(port_returns[port_returns < 0].std() * np.sqrt(ann_factor))
    sortino = float((ann_return - 0.04) / sortino_vol) if sortino_vol > 0 else 0

    peak = cumulative.expanding().max()
    drawdown = (cumulative - peak) / peak
    max_dd = float(drawdown.min())
    calmar = float(ann_return / abs(max_dd)) if max_dd != 0 else 0

    var_95 = float(np.percentile(port_returns, 5))
    cvar_95 = float(port_returns[port_returns <= var_95].mean()) if len(port_returns[port_returns <= var_95]) > 0 else var_95

    wins = int((port_returns > 0).sum())
    losses = int((port_returns < 0).sum())
    win_rate = float(wins / (wins + losses)) if (wins + losses) > 0 else 0

    best_day = float(port_returns.max())
    worst_day = float(port_returns.min())
    avg_win = float(port_returns[port_returns > 0].mean()) if wins > 0 else 0
    avg_loss = float(port_returns[port_returns < 0].mean()) if losses > 0 else 0
    profit_factor = float(abs(avg_win * wins) / abs(avg_loss * losses)) if losses > 0 and avg_loss != 0 else 0

    skew = float(port_returns.skew())
    kurt = float(port_returns.kurtosis())

    return {
        "performance": {
            "total_return": total_return,
            "annualized_return": ann_return,
            "trading_days": trading_days,
            "best_day": best_day,
            "worst_day": worst_day,
            "avg_daily_return": float(port_returns.mean()),
        },
        "risk": {
            "annualized_volatility": ann_vol,
            "max_drawdown": max_dd,
            "var_95_daily": var_95,
            "cvar_95_daily": cvar_95,
            "downside_deviation": sortino_vol,
        },
        "ratios": {
            "sharpe_ratio": sharpe,
            "sortino_ratio": sortino,
            "calmar_ratio": calmar,
            "profit_factor": profit_factor,
        },
        "distribution": {
            "skewness": skew,
            "kurtosis": kurt,
            "win_rate": win_rate,
            "win_days": wins,
            "loss_days": losses,
            "avg_win": avg_win,
            "avg_loss": avg_loss,
        }
    }


def main():
    stdin_data = sys.stdin.read()
    if not stdin_data.strip():
        print(json.dumps({"error": "No input data"}))
        return

    try:
        params = json.loads(stdin_data)
    except Exception as exc:
        print(json.dumps({"error": f"JSON parse error: {exc}"}))
        return
    symbols = params.get("symbols", [])
    weights = params.get("weights", [])

    if not symbols:
        print(json.dumps({"error": "No symbols provided"}))
        return

    if not weights:
        weights = [1.0 / len(symbols)] * len(symbols)

    try:
        result = compute_stats(symbols, weights)
    except Exception as exc:  # yfinance / pandas failures must come back as JSON, not a traceback
        result = {"error": str(exc)}
    print(json.dumps(convert_numpy(result)))


if __name__ == "__main__":
    main()
