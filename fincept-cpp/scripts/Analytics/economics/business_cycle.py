"""
Business Cycle Indicators — Fetch key economic indicators via yfinance/FRED proxies.
Input: JSON via stdin (can be empty {})
Output: JSON with current economic indicators
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


def get_indicators():
    """Fetch market-based economic indicators using yfinance"""
    import yfinance as yf

    indicators = {}

    # Market proxies for economic indicators
    tickers = {
        "^GSPC": "sp500",
        "^VIX": "vix",
        "^TNX": "treasury_10y_yield",
        "^IRX": "treasury_3m_yield",
        "^TYX": "treasury_30y_yield",
        "GC=F": "gold_price",
        "CL=F": "crude_oil_price",
        "DX-Y.NYB": "us_dollar_index",
    }

    for ticker, name in tickers.items():
        try:
            t = yf.Ticker(ticker)
            info = t.fast_info
            price = float(getattr(info, "last_price", 0) or 0)
            prev = float(getattr(info, "previous_close", 0) or 0)
            if price > 0:
                change_pct = ((price - prev) / prev * 100) if prev > 0 else 0
                indicators[name] = price
                indicators[f"{name}_change_pct"] = round(change_pct, 4)
        except Exception:
            pass

    # Yield curve spread (10Y - 3M) — recession indicator
    y10 = indicators.get("treasury_10y_yield", 0)
    y3m = indicators.get("treasury_3m_yield", 0)
    if y10 > 0 and y3m > 0:
        indicators["yield_curve_spread_10y_3m"] = round(y10 - y3m, 4)
        indicators["yield_curve_inverted"] = "Yes" if y10 < y3m else "No"

    # 30Y - 10Y spread
    y30 = indicators.get("treasury_30y_yield", 0)
    if y30 > 0 and y10 > 0:
        indicators["yield_curve_spread_30y_10y"] = round(y30 - y10, 4)

    # Gold/Oil ratio
    gold = indicators.get("gold_price", 0)
    oil = indicators.get("crude_oil_price", 0)
    if gold > 0 and oil > 0:
        indicators["gold_oil_ratio"] = round(gold / oil, 2)

    # S&P 500 stats
    try:
        sp = yf.download("^GSPC", period="1y", interval="1d", progress=False)
        if sp is not None and not sp.empty:
            close = sp["Close"]
            if hasattr(close, "values"):
                vals = close.values.flatten()
                current = float(vals[-1])
                high_52w = float(np.max(vals))
                low_52w = float(np.min(vals))
                sma_200 = float(np.mean(vals[-200:])) if len(vals) >= 200 else float(np.mean(vals))
                sma_50 = float(np.mean(vals[-50:])) if len(vals) >= 50 else float(np.mean(vals))
                indicators["sp500_52w_high"] = round(high_52w, 2)
                indicators["sp500_52w_low"] = round(low_52w, 2)
                indicators["sp500_sma_200"] = round(sma_200, 2)
                indicators["sp500_sma_50"] = round(sma_50, 2)
                indicators["sp500_above_sma200"] = "Yes" if current > sma_200 else "No"
                indicators["sp500_ytd_return_pct"] = round(
                    (current / float(vals[0]) - 1) * 100, 2
                )
    except Exception:
        pass

    # VIX interpretation
    vix = indicators.get("vix", 0)
    if vix > 0:
        if vix < 15:
            indicators["market_sentiment"] = "Low Fear (Complacency)"
        elif vix < 20:
            indicators["market_sentiment"] = "Normal"
        elif vix < 30:
            indicators["market_sentiment"] = "Elevated Fear"
        else:
            indicators["market_sentiment"] = "Extreme Fear"

    if not indicators:
        return {"error": "Could not fetch any indicators"}

    return indicators


def main():
    # Read stdin (may be empty)
    try:
        sys.stdin.read()
    except Exception:
        pass

    result = get_indicators()
    print(json.dumps(convert_numpy(result)))


if __name__ == "__main__":
    main()
