"""
FFN (Financial Functions) Analysis — Deep analytics per symbol.
Input:  JSON via stdin: {"symbols": ["AAPL","MSFT"], "weights": {"AAPL": 0.6, "MSFT": 0.4}}
Output: JSON to stdout with per-symbol stats, rebased series, drawdown series,
        rolling correlations, and portfolio optimisation weights.
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


def max_streak(series):
    max_s, cur = 0, 0
    for v in series:
        if v:
            cur += 1
            max_s = max(max_s, cur)
        else:
            cur = 0
    return max_s


def portfolio_stats(close_df, weights_arr, symbols):
    """Compute blended portfolio stats from a close DataFrame and weight array."""
    import pandas as pd
    w = np.array(weights_arr)
    w = w / w.sum()
    port_returns = (close_df.pct_change().dropna() * w).sum(axis=1)
    total_ret = float((1 + port_returns).prod() - 1)
    n = len(port_returns)
    cagr = float((1 + total_ret) ** (252.0 / max(n, 1)) - 1)
    vol = float(port_returns.std() * np.sqrt(252))
    sharpe = float((cagr - 0.04) / vol) if vol > 0 else 0.0
    cum = (1 + port_returns).cumprod()
    peak = cum.expanding().max()
    dd = (cum - peak) / peak
    max_dd = float(dd.min())
    return {
        "total_return": total_ret,
        "cagr": cagr,
        "volatility": vol,
        "sharpe": sharpe,
        "max_drawdown": max_dd,
    }


def compute_ffn(symbols, weights, period="1y"):
    import yfinance as yf
    import pandas as pd

    data = yf.download(symbols, period=period, interval="1d", progress=False, auto_adjust=True)
    if data is None or data.empty:
        return {"error": "Could not fetch price data"}

    # Normalise to a Close DataFrame regardless of yfinance version
    if isinstance(data.columns, pd.MultiIndex):
        if "Close" in data.columns.get_level_values(0):
            close = data["Close"]
        elif "Adj Close" in data.columns.get_level_values(0):
            close = data["Adj Close"]
        else:
            close = data.iloc[:, 0:len(symbols)]
    else:
        close = data[["Close"]] if "Close" in data.columns else data

    if not isinstance(close, pd.DataFrame):
        close = pd.DataFrame(close)

    # If single symbol yfinance returns a Series — wrap it
    if isinstance(close, pd.Series):
        close = close.to_frame(name=symbols[0])

    # Ensure column names match symbols when single symbol
    if len(symbols) == 1 and list(close.columns) != symbols:
        close.columns = [symbols[0]]

    result = {}

    # ── Per-symbol stats ──────────────────────────────────────────────────────
    for sym in symbols:
        if sym not in close.columns:
            continue
        prices = close[sym].dropna()
        if len(prices) < 2:
            continue

        returns = prices.pct_change().dropna()
        cumulative = (1 + returns).cumprod()

        total_ret   = float(cumulative.iloc[-1] - 1)
        ann_ret     = float((1 + total_ret) ** (252.0 / max(len(returns), 1)) - 1)
        ann_vol     = float(returns.std() * np.sqrt(252))
        peak        = prices.expanding().max()
        dd          = (prices - peak) / peak
        max_dd      = float(dd.min())
        monthly_ret = float(returns.mean() * 21)
        pos         = (returns > 0).astype(int).tolist()
        neg         = (returns < 0).astype(int).tolist()

        result[sym] = {
            "total_return":          total_ret,
            "annualized_return":     ann_ret,
            "annualized_volatility": ann_vol,
            "max_drawdown":          max_dd,
            "sharpe_ratio":          float((ann_ret - 0.04) / ann_vol) if ann_vol > 0 else 0.0,
            "current_price":         float(prices.iloc[-1]),
            "start_price":           float(prices.iloc[0]),
            "best_day":              float(returns.max()),
            "worst_day":             float(returns.min()),
            "avg_daily_return":      float(returns.mean()),
            "positive_days":         int((returns > 0).sum()),
            "negative_days":         int((returns < 0).sum()),
            "max_win_streak":        max_streak(pos),
            "max_loss_streak":       max_streak(neg),
            "avg_monthly_return":    monthly_ret,
            "skewness":              float(returns.skew()),
            "kurtosis":              float(returns.kurtosis()),
        }

    # Only keep symbols we actually computed
    valid_syms = [s for s in symbols if s in result]
    if not valid_syms:
        return result

    valid_close = close[valid_syms].dropna(how="all")
    LIMIT = 252

    # ── Rebased price series ──────────────────────────────────────────────────
    rebased_out = {}
    try:
        import ffn
        rebased_df = ffn.rebase(valid_close.dropna(), 100)
        rebased_df = rebased_df.tail(LIMIT)
        for sym in valid_syms:
            if sym in rebased_df.columns:
                series = rebased_df[sym].dropna()
                rebased_out[sym] = {
                    idx.strftime("%Y-%m-%d"): float(v)
                    for idx, v in series.items()
                    if not (isinstance(v, float) and (np.isnan(v) or np.isinf(v)))
                }
    except Exception:
        # ffn not available — compute manually
        try:
            base = valid_close.dropna().tail(LIMIT)
            for sym in valid_syms:
                if sym not in base.columns:
                    continue
                s = base[sym].dropna()
                if s.empty:
                    continue
                rebased = s / s.iloc[0] * 100.0
                rebased_out[sym] = {
                    idx.strftime("%Y-%m-%d"): float(v)
                    for idx, v in rebased.items()
                    if not (isinstance(v, float) and (np.isnan(v) or np.isinf(v)))
                }
        except Exception:
            pass

    result["rebased"] = rebased_out

    # ── Drawdown series ───────────────────────────────────────────────────────
    drawdown_out = {}
    try:
        import ffn
        for sym in valid_syms:
            if sym not in valid_close.columns:
                continue
            prices_s = valid_close[sym].dropna()
            if len(prices_s) < 2:
                continue
            dd_series = ffn.to_drawdown_series(prices_s).tail(LIMIT)
            drawdown_out[sym] = {
                idx.strftime("%Y-%m-%d"): float(v)
                for idx, v in dd_series.items()
                if not (isinstance(v, float) and (np.isnan(v) or np.isinf(v)))
            }
    except Exception:
        try:
            for sym in valid_syms:
                if sym not in valid_close.columns:
                    continue
                prices_s = valid_close[sym].dropna()
                if len(prices_s) < 2:
                    continue
                peak = prices_s.expanding().max()
                dd_s = ((prices_s - peak) / peak).tail(LIMIT)
                drawdown_out[sym] = {
                    idx.strftime("%Y-%m-%d"): float(v)
                    for idx, v in dd_s.items()
                    if not (isinstance(v, float) and (np.isnan(v) or np.isinf(v)))
                }
        except Exception:
            pass

    result["drawdown_series"] = drawdown_out

    # ── Rolling correlations ──────────────────────────────────────────────────
    rolling_out = {}
    try:
        if len(valid_syms) >= 2:
            ret_df = valid_close[valid_syms].pct_change().dropna()
            WINDOW = 60
            # rolling().corr() returns a MultiIndex series: (date, sym) -> corr_with_sym
            rolling_corr = ret_df.rolling(WINDOW).corr()
            for i in range(len(valid_syms)):
                for j in range(i + 1, len(valid_syms)):
                    s1, s2 = valid_syms[i], valid_syms[j]
                    key = f"{s1}_{s2}"
                    try:
                        # Select the cross-correlation column
                        pair_series = rolling_corr.xs(s2, level=1)[s1].dropna().tail(LIMIT)
                        rolling_out[key] = {
                            idx.strftime("%Y-%m-%d"): float(v)
                            for idx, v in pair_series.items()
                            if not (isinstance(v, float) and (np.isnan(v) or np.isinf(v)))
                        }
                    except Exception:
                        pass
    except Exception:
        pass

    result["rolling_corr"] = rolling_out

    # ── Portfolio optimisation ────────────────────────────────────────────────
    opt_out = {}
    try:
        if len(valid_syms) >= 2:
            ret_df = valid_close[valid_syms].pct_change().dropna()
            n = len(valid_syms)

            if len(ret_df) >= 30:
                # Equal weights
                equal_w = {s: round(1.0 / n, 6) for s in valid_syms}

                # Current weights (passed in from C++)
                cur_w_arr = np.array([weights.get(s, 1.0 / n) for s in valid_syms])
                cur_w_arr = cur_w_arr / cur_w_arr.sum()
                current_w = {s: round(float(cur_w_arr[i]), 6) for i, s in enumerate(valid_syms)}

                # ERC weights via ffn
                erc_w = equal_w.copy()
                try:
                    import ffn
                    erc_arr = ffn.calc_erc_weights(
                        ret_df,
                        covar_method="ledoit-wolf",
                        risk_parity_method="ccd",
                        maximum_iterations=100,
                        tolerance=1e-8,
                    )
                    erc_w = {s: round(float(erc_arr[i]), 6) for i, s in enumerate(valid_syms)}
                except Exception:
                    pass

                # Inverse-vol weights via ffn
                inv_vol_w = equal_w.copy()
                try:
                    import ffn
                    iv_arr = ffn.calc_inv_vol_weights(ret_df)
                    inv_vol_w = {s: round(float(iv_arr[i]), 6) for i, s in enumerate(valid_syms)}
                except Exception:
                    try:
                        vols = ret_df.std()
                        inv_v = 1.0 / vols
                        iv_norm = inv_v / inv_v.sum()
                        inv_vol_w = {s: round(float(iv_norm[s]), 6) for s in valid_syms}
                    except Exception:
                        pass

                # Portfolio stats for each weight set
                def w_arr(w_dict):
                    return [w_dict.get(s, 0.0) for s in valid_syms]

                stats = {}
                for name, wd in [("erc", erc_w), ("inv_vol", inv_vol_w),
                                  ("equal", equal_w), ("current", current_w)]:
                    try:
                        stats[name] = portfolio_stats(valid_close[valid_syms].dropna(),
                                                       w_arr(wd), valid_syms)
                    except Exception:
                        stats[name] = {
                            "total_return": 0.0, "cagr": 0.0,
                            "volatility": 0.0, "sharpe": 0.0, "max_drawdown": 0.0,
                        }

                opt_out = {
                    "erc":     erc_w,
                    "inv_vol": inv_vol_w,
                    "equal":   equal_w,
                    "current": current_w,
                    "stats":   stats,
                }
    except Exception:
        pass

    result["optimization"] = opt_out

    return result


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
    if not symbols:
        print(json.dumps({"error": "No symbols provided"}))
        return

    # weights dict: symbol -> fraction (0-1); fall back to equal weight
    raw_weights = params.get("weights", {})
    n = len(symbols)
    weights = {}
    for s in symbols:
        weights[s] = float(raw_weights.get(s, 1.0 / n))
    total_w = sum(weights.values())
    if total_w > 0:
        weights = {s: v / total_w for s, v in weights.items()}
    else:
        weights = {s: 1.0 / n for s in symbols}

    result = compute_ffn(symbols, weights)
    print(json.dumps(convert_numpy(result)))


if __name__ == "__main__":
    main()
