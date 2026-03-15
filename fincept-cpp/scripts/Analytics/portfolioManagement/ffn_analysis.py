"""
FFN (Financial Functions) Analysis — Deep analytics per symbol.
Input: JSON via stdin: {"symbols": ["AAPL","MSFT"]}
Output: JSON to stdout with per-symbol stats
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


def compute_ffn(symbols, period="1y"):
    import yfinance as yf

    data = yf.download(symbols, period=period, interval="1d", progress=False)
    if data is None or data.empty:
        return {"error": "Could not fetch price data"}

    close = data["Close"]
    if len(symbols) == 1:
        import pandas as pd
        if not isinstance(close, pd.DataFrame):
            close = pd.DataFrame({symbols[0]: close})

    result = {}
    for sym in symbols:
        if sym not in close.columns:
            continue
        prices = close[sym].dropna()
        if len(prices) < 2:
            continue

        returns = prices.pct_change().dropna()
        cumulative = (1 + returns).cumprod()

        # Performance
        total_ret = float(cumulative.iloc[-1] - 1)
        ann_ret = float((1 + total_ret) ** (252 / max(len(returns), 1)) - 1)
        ann_vol = float(returns.std() * np.sqrt(252))

        # Drawdown
        peak = prices.expanding().max()
        dd = (prices - peak) / peak
        max_dd = float(dd.min())

        # Streak analysis
        pos = (returns > 0).astype(int)
        neg = (returns < 0).astype(int)

        def max_streak(series):
            max_s = 0
            cur = 0
            for v in series:
                if v:
                    cur += 1
                    max_s = max(max_s, cur)
                else:
                    cur = 0
            return max_s

        # Monthly returns approximation
        monthly_ret = float(returns.mean() * 21)

        result[sym] = {
            "total_return": total_ret,
            "annualized_return": ann_ret,
            "annualized_volatility": ann_vol,
            "max_drawdown": max_dd,
            "sharpe_ratio": float((ann_ret - 0.04) / ann_vol) if ann_vol > 0 else 0,
            "current_price": float(prices.iloc[-1]),
            "start_price": float(prices.iloc[0]),
            "best_day": float(returns.max()),
            "worst_day": float(returns.min()),
            "avg_daily_return": float(returns.mean()),
            "positive_days": int((returns > 0).sum()),
            "negative_days": int((returns < 0).sum()),
            "max_win_streak": max_streak(pos),
            "max_loss_streak": max_streak(neg),
            "avg_monthly_return": monthly_ret,
            "skewness": float(returns.skew()),
            "kurtosis": float(returns.kurtosis()),
        }

    return result


def main():
    stdin_data = sys.stdin.read()
    if not stdin_data.strip():
        print(json.dumps({"error": "No input data"}))
        return

    params = json.loads(stdin_data)
    symbols = params.get("symbols", [])

    if not symbols:
        print(json.dumps({"error": "No symbols provided"}))
        return

    result = compute_ffn(symbols)
    print(json.dumps(convert_numpy(result)))


if __name__ == "__main__":
    main()
