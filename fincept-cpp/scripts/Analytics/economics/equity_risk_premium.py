"""
Equity Risk Premium (ERP) — Multiple estimation methods.
Input: JSON via stdin (can be empty {})
Output: JSON with ERP estimates from various models
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


def estimate_erp():
    """Estimate ERP using multiple approaches"""
    import yfinance as yf

    result = {}

    # 1. Historical ERP: S&P 500 excess return over risk-free rate
    try:
        sp = yf.download("^GSPC", period="10y", interval="1mo", progress=False)
        if sp is not None and not sp.empty:
            close = sp["Close"].values.flatten()
            monthly_returns = np.diff(close) / close[:-1]
            ann_return = float((1 + np.mean(monthly_returns)) ** 12 - 1)

            # Get current risk-free rate from 10Y treasury
            try:
                tnx = yf.Ticker("^TNX")
                rf = float(getattr(tnx.fast_info, "last_price", 4.0) or 4.0) / 100
            except Exception:
                rf = 0.04

            historical_erp = ann_return - rf
            result["historical_10y_erp"] = round(historical_erp, 4)
            result["sp500_10y_annualized"] = round(ann_return, 4)
            result["risk_free_rate_10y"] = round(rf, 4)
    except Exception:
        pass

    # 2. Short-term historical (5Y)
    try:
        sp5 = yf.download("^GSPC", period="5y", interval="1mo", progress=False)
        if sp5 is not None and not sp5.empty:
            close5 = sp5["Close"].values.flatten()
            mr5 = np.diff(close5) / close5[:-1]
            ann5 = float((1 + np.mean(mr5)) ** 12 - 1)
            rf = result.get("risk_free_rate_10y", 0.04)
            result["historical_5y_erp"] = round(ann5 - rf, 4)
            result["sp500_5y_annualized"] = round(ann5, 4)
    except Exception:
        pass

    # 3. Implied ERP from earnings yield
    try:
        sp_ticker = yf.Ticker("^GSPC")
        pe = getattr(sp_ticker.info, "trailingPE", None)
        if pe is None:
            pe = sp_ticker.info.get("trailingPE", None) if hasattr(sp_ticker, "info") and isinstance(sp_ticker.info, dict) else None
        if pe and pe > 0:
            earnings_yield = 1.0 / pe
            rf = result.get("risk_free_rate_10y", 0.04)
            result["implied_erp_earnings_yield"] = round(earnings_yield - rf, 4)
            result["sp500_earnings_yield"] = round(earnings_yield, 4)
            result["sp500_pe_ratio"] = round(pe, 2)
    except Exception:
        pass

    # 4. Damodaran-style supply-side (dividend yield + buyback + growth - rf)
    try:
        spy = yf.Ticker("SPY")
        div_yield = 0.013  # approximate S&P 500 dividend yield
        try:
            dy = getattr(spy.info, "dividendYield", None)
            if dy is None and hasattr(spy, "info") and isinstance(spy.info, dict):
                dy = spy.info.get("dividendYield", None)
            if dy and dy > 0:
                div_yield = float(dy)
        except Exception:
            pass

        buyback_yield = 0.02  # approximate
        real_gdp_growth = 0.025
        inflation = 0.03

        supply_side_expected = div_yield + buyback_yield + real_gdp_growth + inflation
        rf = result.get("risk_free_rate_10y", 0.04)
        result["supply_side_erp"] = round(supply_side_expected - rf, 4)
        result["dividend_yield"] = round(div_yield, 4)
    except Exception:
        pass

    # 5. VIX-implied (rough approximation: VIX/100 * sqrt(time) as excess return proxy)
    try:
        vix = yf.Ticker("^VIX")
        vix_level = float(getattr(vix.fast_info, "last_price", 0) or 0)
        if vix_level > 0:
            # Variance risk premium approach: implied vol vs realized vol
            result["vix_current"] = round(vix_level, 2)
            result["vix_implied_erp_approx"] = round(vix_level / 100 * 0.5, 4)
    except Exception:
        pass

    # Summary
    erp_values = [v for k, v in result.items() if k.endswith("_erp") and isinstance(v, (int, float))]
    if erp_values:
        result["consensus_erp_estimate"] = round(float(np.mean(erp_values)), 4)
        result["erp_range_low"] = round(float(min(erp_values)), 4)
        result["erp_range_high"] = round(float(max(erp_values)), 4)

    if not result:
        return {"error": "Could not compute ERP estimates"}

    return result


def main():
    try:
        sys.stdin.read()
    except Exception:
        pass

    result = estimate_erp()
    print(json.dumps(convert_numpy(result)))


if __name__ == "__main__":
    main()
