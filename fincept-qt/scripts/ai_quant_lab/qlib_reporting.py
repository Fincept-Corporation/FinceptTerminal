"""
Quant Reporting Service (factor / model report builder)
========================================================
Self-contained reporting service for the Fincept Terminal "Quant Reporting"
sub-tab. Same response contract as gs_quant_service / functime_service /
statsmodels_service / fortitudo_service.

Operates on flat numeric arrays (the wire format the C++ panel sends),
unlike the legacy panel-data DataFrame variant preserved as
qlib_reporting_legacy.py.

Response contract (all ops):
    success: bool
    operation: str
    data: dict   (when success)
    error: str   (when failure)
    error_kind: str | None    (validation | runtime | unknown_op)
    traceback: str | None     (only on uncaught exceptions)
"""

import sys
import os
import json
import math
import traceback
from typing import Dict, List, Any

import numpy as np

# Optional: scipy for proper rank/Pearson p-values. Falls back to numpy.corrcoef.
try:
    from scipy.stats import pearsonr, spearmanr
    SCIPY_OK = True
except ImportError:
    SCIPY_OK = False

# Optional: pandas for rolling window operations. Falls back to a numpy loop.
try:
    import pandas as pd
    PANDAS_OK = True
except ImportError:
    PANDAS_OK = False


# ============================================================================
# INPUT COERCION + VALIDATION
# ============================================================================

class ValidationError(ValueError):
    pass


def _coerce_floats(value, field_name):
    if value is None:
        return []
    if isinstance(value, (list, tuple)):
        out = []
        for i, v in enumerate(value):
            try:
                out.append(float(v))
            except (TypeError, ValueError):
                raise ValidationError(f"{field_name}[{i}] is not numeric: {v!r}")
        return out
    if isinstance(value, np.ndarray):
        return [float(v) for v in value.tolist()]
    if isinstance(value, str):
        s = value.strip()
        if not s:
            return []
        for sep in [",", ";", "\t", "\n"]:
            s = s.replace(sep, " ")
        parts = [p for p in s.split(" ") if p.strip()]
        out = []
        for i, p in enumerate(parts):
            try:
                out.append(float(p))
            except ValueError:
                raise ValidationError(f"{field_name}[{i}] is not numeric: {p!r}")
        return out
    try:
        return [float(value)]
    except (TypeError, ValueError):
        raise ValidationError(f"{field_name} is not numeric: {value!r}")


def _require_min_length(values, n, field_name):
    if len(values) < n:
        raise ValidationError(
            f"{field_name} needs at least {n} values, got {len(values)}")


def _safe_float(v, default=0.0):
    try:
        f = float(v)
    except (TypeError, ValueError):
        return default
    if math.isnan(f) or math.isinf(f):
        return default
    return f


def _safe_int(v, default=0):
    try:
        f = float(v)
        if math.isnan(f) or math.isinf(f):
            return default
        return int(f)
    except (TypeError, ValueError):
        return default


def _sample_curve(values, max_points=300):
    """Subsample [(label, value)] to ~max_points entries, always keeping the last."""
    if not values:
        return []
    n = len(values)
    step = max(1, n // max_points)
    out = []
    for i, (label, v) in enumerate(values):
        if i % step == 0:
            out.append({"index": i, "label": str(label), "value": _safe_float(v)})
    if out and out[-1]["index"] != n - 1:
        out.append({"index": n - 1, "label": str(values[-1][0]), "value": _safe_float(values[-1][1])})
    return out


def _correlation(x, y):
    """Return (pearson_r, pearson_p, spearman_r, spearman_p), p-values 0/1 if scipy missing."""
    x_arr = np.asarray(x, dtype=float)
    y_arr = np.asarray(y, dtype=float)
    if SCIPY_OK:
        try:
            pr, pp = pearsonr(x_arr, y_arr)
        except Exception:
            pr, pp = float("nan"), 1.0
        try:
            sr, sp = spearmanr(x_arr, y_arr)
        except Exception:
            sr, sp = float("nan"), 1.0
        return _safe_float(pr), _safe_float(pp), _safe_float(sr), _safe_float(sp)
    # Numpy fallback
    if x_arr.std() == 0 or y_arr.std() == 0:
        return 0.0, 1.0, 0.0, 1.0
    pr = float(np.corrcoef(x_arr, y_arr)[0, 1])
    # Spearman = Pearson on rank
    rx = np.argsort(np.argsort(x_arr)).astype(float)
    ry = np.argsort(np.argsort(y_arr)).astype(float)
    sr = float(np.corrcoef(rx, ry)[0, 1])
    return _safe_float(pr), 0.0, _safe_float(sr), 0.0


# ============================================================================
# OPERATION HANDLERS
# ============================================================================

def op_check_status(_data):
    return {
        "scipy": SCIPY_OK,
        "pandas": PANDAS_OK,
        "backend": "numpy" + (" + scipy" if SCIPY_OK else "") + (" + pandas" if PANDAS_OK else ""),
        "ops_available": list(OPERATIONS.keys()),
    }


def op_ic_analysis(data):
    """
    Information Coefficient analysis between predictions and realized returns.

    Inputs:
      predictions : list[float]  (required, >= 10)
      returns     : list[float]  (required, same length as predictions)
      method      : 'pearson' | 'spearman' | 'both'   (default 'both')
      window      : int          (rolling-IC window size; default 20)
    """
    preds = _coerce_floats(data.get("predictions"), "predictions")
    rets = _coerce_floats(data.get("returns"), "returns")
    _require_min_length(preds, 10, "predictions")
    if len(preds) != len(rets):
        raise ValidationError(
            f"predictions ({len(preds)}) and returns ({len(rets)}) must have the same length")
    method = str(data.get("method", "both")).lower()
    if method not in ("pearson", "spearman", "both"):
        raise ValidationError(
            f"method must be 'pearson', 'spearman', or 'both'; got {method!r}")
    window = max(5, min(_safe_int(data.get("window", 20)), len(preds) // 2))

    pr, pp, sr, sp = _correlation(preds, rets)

    # Rolling IC over the requested window
    rolling = []
    for i in range(window, len(preds) + 1):
        win_p = preds[i - window:i]
        win_r = rets[i - window:i]
        try:
            r_pr, _pp, r_sr, _sp = _correlation(win_p, win_r)
        except Exception:
            r_pr, r_sr = 0.0, 0.0
        row = {"end_index": i - 1}
        if method in ("pearson", "both"):
            row["ic"] = _safe_float(r_pr)
        if method in ("spearman", "both"):
            row["rank_ic"] = _safe_float(r_sr)
        rolling.append(row)

    # Aggregate stats from rolling
    ic_values = [r["ic"] for r in rolling if "ic" in r]
    rank_ic_values = [r["rank_ic"] for r in rolling if "rank_ic" in r]

    def _ir(vals):
        if not vals:
            return 0.0, 0.0, 0.0, 0.0
        arr = np.asarray(vals, dtype=float)
        mu = float(arr.mean())
        sd = float(arr.std()) or 0.0
        ir = mu / sd if sd > 0 else 0.0
        pos_rate = float((arr > 0).mean() * 100.0)
        return _safe_float(mu), _safe_float(sd), _safe_float(ir), _safe_float(pos_rate)

    ic_mean, ic_std, icir, ic_pos_pct = _ir(ic_values)
    rank_ic_mean, rank_ic_std, rank_icir, rank_ic_pos_pct = _ir(rank_ic_values)

    # Verdict heuristic on IR
    def _verdict(ir):
        if ir >= 0.75: return "STRONG"
        if ir >= 0.40: return "MODERATE"
        if ir >= 0.10: return "WEAK"
        if ir > 0: return "VERY WEAK"
        return "NO SIGNAL"

    return {
        "n_observations": len(preds),
        "method": method,
        "window": window,
        "n_rolling_points": len(rolling),
        "overall_pearson_ic": _safe_float(pr),
        "overall_pearson_p": _safe_float(pp),
        "overall_spearman_ic": _safe_float(sr),
        "overall_spearman_p": _safe_float(sp),
        "ic_mean": ic_mean,
        "ic_std": ic_std,
        "icir": icir,
        "ic_positive_pct": ic_pos_pct,
        "ic_verdict": _verdict(icir),
        "rank_ic_mean": rank_ic_mean,
        "rank_ic_std": rank_ic_std,
        "rank_icir": rank_icir,
        "rank_ic_positive_pct": rank_ic_pos_pct,
        "rank_ic_verdict": _verdict(rank_icir),
        "rolling_series": rolling,
    }


def op_cumulative_returns(data):
    """
    Cumulative-return curve for portfolio + optional benchmark.

    Inputs:
      returns           : list[float]  (required, >= 5)
      benchmark_returns : list[float]  (optional, must match length if provided)
      title             : str          (optional, label only)
    """
    rets = _coerce_floats(data.get("returns"), "returns")
    _require_min_length(rets, 5, "returns")
    title = str(data.get("title", "Cumulative Returns"))

    has_bench = data.get("benchmark_returns") is not None and \
                len(_coerce_floats(data.get("benchmark_returns"), "benchmark_returns")) > 0

    if has_bench:
        bench = _coerce_floats(data.get("benchmark_returns"), "benchmark_returns")
        if len(bench) != len(rets):
            raise ValidationError(
                f"returns ({len(rets)}) and benchmark_returns ({len(bench)}) must have the same length")
    else:
        bench = []

    arr = np.asarray(rets, dtype=float)
    cum = np.cumprod(1.0 + arr)
    n = len(arr)

    total_return = float(cum[-1] - 1.0)
    years = max(n / 252.0, 1e-6)
    ann_return = (1.0 + total_return) ** (1.0 / years) - 1.0 if cum[-1] > 0 else -1.0
    daily_vol = float(arr.std())
    ann_vol = daily_vol * math.sqrt(252)
    sharpe = (float(arr.mean()) / daily_vol * math.sqrt(252)) if daily_vol > 0 else 0.0
    win_rate = float((arr > 0).mean() * 100.0)
    best_day = float(arr.max())
    worst_day = float(arr.min())

    # Drawdown
    peak = np.maximum.accumulate(cum)
    dd = (cum - peak) / peak
    max_dd = float(dd.min())
    # Find max-DD location
    trough_idx = int(np.argmin(dd))
    peak_idx = int(np.argmax(cum[:trough_idx + 1])) if trough_idx > 0 else 0

    portfolio_curve = _sample_curve(list(zip(range(n), cum.tolist())), 300)

    payload = {
        "n_observations": n,
        "title": title,
        "has_benchmark": has_bench,
        "total_return": _safe_float(total_return),
        "total_return_pct": _safe_float(total_return * 100.0),
        "annualized_return": _safe_float(ann_return),
        "annualized_return_pct": _safe_float(ann_return * 100.0),
        "annualized_volatility": _safe_float(ann_vol),
        "annualized_volatility_pct": _safe_float(ann_vol * 100.0),
        "sharpe_ratio": _safe_float(sharpe),
        "max_drawdown": _safe_float(max_dd),
        "max_drawdown_pct": _safe_float(max_dd * 100.0),
        "max_drawdown_peak_index": peak_idx,
        "max_drawdown_trough_index": trough_idx,
        "best_day": _safe_float(best_day),
        "worst_day": _safe_float(worst_day),
        "win_rate_pct": _safe_float(win_rate),
        "final_value": _safe_float(cum[-1]),
        "portfolio_curve": portfolio_curve,
    }

    if has_bench:
        bench_arr = np.asarray(bench, dtype=float)
        bench_cum = np.cumprod(1.0 + bench_arr)
        bench_total = float(bench_cum[-1] - 1.0)
        bench_ann = (1.0 + bench_total) ** (1.0 / years) - 1.0 if bench_cum[-1] > 0 else -1.0
        excess = arr - bench_arr
        te = float(excess.std()) * math.sqrt(252)
        info_ratio = (float(excess.mean()) * math.sqrt(252) / te) if te > 0 else 0.0
        payload.update({
            "benchmark_total_return": _safe_float(bench_total),
            "benchmark_total_return_pct": _safe_float(bench_total * 100.0),
            "benchmark_annualized_return_pct": _safe_float(bench_ann * 100.0),
            "benchmark_final_value": _safe_float(bench_cum[-1]),
            "alpha_pct": _safe_float((total_return - bench_total) * 100.0),
            "tracking_error_pct": _safe_float(te * 100.0),
            "information_ratio": _safe_float(info_ratio),
            "benchmark_curve": _sample_curve(list(zip(range(n), bench_cum.tolist())), 300),
        })

    return payload


def op_risk_report(data):
    """
    Drawdown + rolling volatility report for a return series.

    Inputs:
      returns       : list[float]  (required, >= 30)
      rolling_window: int          (default 20, in trading days)
    """
    rets = _coerce_floats(data.get("returns"), "returns")
    _require_min_length(rets, 30, "returns")
    window = max(5, min(_safe_int(data.get("rolling_window", 20)), len(rets) // 2))

    arr = np.asarray(rets, dtype=float)
    n = len(arr)
    cum = np.cumprod(1.0 + arr)
    peak = np.maximum.accumulate(cum)
    dd = (cum - peak) / peak

    # Rolling vol via simple loop (matches numpy without pandas dependency for portability)
    rolling_vol = np.full(n, np.nan)
    for i in range(window - 1, n):
        rolling_vol[i] = float(arr[i - window + 1:i + 1].std()) * math.sqrt(252)

    max_dd = float(dd.min())
    trough_idx = int(np.argmin(dd))
    peak_idx = int(np.argmax(cum[:trough_idx + 1])) if trough_idx > 0 else 0

    # Recovery: first index after trough where cum >= peak[trough]
    recovery_idx = -1
    if trough_idx < n - 1:
        target = peak[trough_idx]
        rest = cum[trough_idx + 1:]
        recovered = np.where(rest >= target)[0]
        if len(recovered) > 0:
            recovery_idx = int(trough_idx + 1 + recovered[0])

    # Tail metrics
    var_5 = float(np.percentile(arr, 5))
    var_1 = float(np.percentile(arr, 1))
    cvar_5 = float(arr[arr <= var_5].mean()) if (arr <= var_5).any() else 0.0
    cvar_1 = float(arr[arr <= var_1].mean()) if (arr <= var_1).any() else 0.0

    # Volatility regime cards
    rv_clean = rolling_vol[~np.isnan(rolling_vol)]
    avg_vol = _safe_float(float(rv_clean.mean()) if len(rv_clean) else 0.0)
    max_vol = _safe_float(float(rv_clean.max()) if len(rv_clean) else 0.0)
    min_vol = _safe_float(float(rv_clean.min()) if len(rv_clean) else 0.0)
    cur_vol = _safe_float(float(rv_clean[-1]) if len(rv_clean) else 0.0)

    return {
        "n_observations": n,
        "rolling_window": window,
        "max_drawdown": _safe_float(max_dd),
        "max_drawdown_pct": _safe_float(max_dd * 100.0),
        "max_drawdown_peak_index": peak_idx,
        "max_drawdown_trough_index": trough_idx,
        "max_drawdown_recovery_index": recovery_idx,
        "max_drawdown_recovered": recovery_idx >= 0,
        "drawdown_duration_days": (recovery_idx if recovery_idx >= 0 else n - 1) - peak_idx,
        "current_drawdown_pct": _safe_float(float(dd[-1]) * 100.0),
        "var_5pct_daily": _safe_float(var_5),
        "var_1pct_daily": _safe_float(var_1),
        "cvar_5pct_daily": _safe_float(cvar_5),
        "cvar_1pct_daily": _safe_float(cvar_1),
        "avg_rolling_vol": avg_vol,
        "current_rolling_vol": cur_vol,
        "max_rolling_vol": max_vol,
        "min_rolling_vol": min_vol,
        "drawdown_curve": _sample_curve(list(zip(range(n), dd.tolist())), 300),
        "rolling_vol_curve": _sample_curve(
            [(i, v) for i, v in enumerate(rolling_vol.tolist()) if not math.isnan(v)], 300),
    }


def op_model_performance(data):
    """
    Combined model evaluation: IC + quantile spread + summary verdict.

    Inputs:
      predictions : list[float]  (required, >= 20)
      returns     : list[float]  (required, same length)
      model_name  : str          (label only)
      n_quantiles : int          (default 5)
    """
    preds = _coerce_floats(data.get("predictions"), "predictions")
    rets = _coerce_floats(data.get("returns"), "returns")
    _require_min_length(preds, 20, "predictions")
    if len(preds) != len(rets):
        raise ValidationError(
            f"predictions ({len(preds)}) and returns ({len(rets)}) must have the same length")
    model_name = str(data.get("model_name", "Model"))
    n_q = max(2, min(_safe_int(data.get("n_quantiles", 5)), 10))

    p_arr = np.asarray(preds, dtype=float)
    r_arr = np.asarray(rets, dtype=float)
    n = len(p_arr)

    # IC
    pr, pp, sr, sp = _correlation(p_arr, r_arr)

    # Direction accuracy
    dir_acc = float(np.mean(np.sign(p_arr) == np.sign(r_arr)) * 100.0)
    # Hit rate (predicted positive, realized positive)
    pos_mask = p_arr > 0
    hit_rate = float(np.mean(r_arr[pos_mask] > 0) * 100.0) if pos_mask.any() else 0.0

    # Quantile decomposition
    quantile_rows = []
    if n >= n_q * 5:
        order = np.argsort(p_arr)
        chunks = np.array_split(order, n_q)
        for q_idx, idx_chunk in enumerate(chunks):
            sub = r_arr[idx_chunk]
            quantile_rows.append({
                "quantile": q_idx + 1,
                "n": int(len(idx_chunk)),
                "pred_min": _safe_float(float(p_arr[idx_chunk].min())),
                "pred_max": _safe_float(float(p_arr[idx_chunk].max())),
                "mean_return": _safe_float(float(sub.mean())),
                "median_return": _safe_float(float(np.median(sub))),
                "win_rate_pct": _safe_float(float((sub > 0).mean() * 100.0)),
            })

    long_short_ret = 0.0
    if quantile_rows:
        long_short_ret = quantile_rows[-1]["mean_return"] - quantile_rows[0]["mean_return"]

    # Verdict
    def _verdict(ic, ls):
        score = ic + (ls * 5.0)  # heuristic blend
        if score >= 0.30: return "STRONG ALPHA"
        if score >= 0.15: return "MODERATE ALPHA"
        if score >= 0.05: return "WEAK ALPHA"
        if score > 0: return "MARGINAL SIGNAL"
        return "NO ALPHA"

    return {
        "n_observations": n,
        "model_name": model_name,
        "n_quantiles": n_q,
        "pearson_ic": _safe_float(pr),
        "pearson_p_value": _safe_float(pp),
        "spearman_ic": _safe_float(sr),
        "spearman_p_value": _safe_float(sp),
        "direction_accuracy_pct": _safe_float(dir_acc),
        "hit_rate_pct": _safe_float(hit_rate),
        "long_short_spread": _safe_float(long_short_ret),
        "long_short_spread_bps": _safe_float(long_short_ret * 10000.0),
        "quantile_table": quantile_rows,
        "monotonic": _is_monotonic([q["mean_return"] for q in quantile_rows]) if quantile_rows else False,
        "verdict": _verdict(sr, long_short_ret),
    }


def _is_monotonic(values):
    """True when values are non-decreasing OR non-increasing (allow tiny float noise)."""
    if len(values) < 2:
        return False
    diffs = np.diff(values)
    return bool(np.all(diffs >= -1e-9)) or bool(np.all(diffs <= 1e-9))


def op_factor_quantiles(data):
    """
    Sort predictions into N quantiles, show realized return spread + win rate per bucket.
    Cleaner standalone version of the quantile table inside model_performance.

    Inputs:
      predictions : list[float]   (required, >= 20)
      returns     : list[float]   (required, same length)
      n_quantiles : int           (default 5, capped at 10)
    """
    preds = _coerce_floats(data.get("predictions"), "predictions")
    rets = _coerce_floats(data.get("returns"), "returns")
    _require_min_length(preds, 20, "predictions")
    if len(preds) != len(rets):
        raise ValidationError(
            f"predictions ({len(preds)}) and returns ({len(rets)}) must have the same length")
    n_q = max(2, min(_safe_int(data.get("n_quantiles", 5)), 10))

    p_arr = np.asarray(preds, dtype=float)
    r_arr = np.asarray(rets, dtype=float)
    n = len(p_arr)
    if n < n_q * 5:
        raise ValidationError(
            f"need at least {n_q * 5} obs for {n_q} quantiles, got {n}")

    order = np.argsort(p_arr)
    chunks = np.array_split(order, n_q)
    quantile_rows = []
    for q_idx, idx_chunk in enumerate(chunks):
        sub = r_arr[idx_chunk]
        quantile_rows.append({
            "quantile": q_idx + 1,
            "n": int(len(idx_chunk)),
            "pred_min": _safe_float(float(p_arr[idx_chunk].min())),
            "pred_max": _safe_float(float(p_arr[idx_chunk].max())),
            "pred_mean": _safe_float(float(p_arr[idx_chunk].mean())),
            "ret_mean": _safe_float(float(sub.mean())),
            "ret_median": _safe_float(float(np.median(sub))),
            "ret_std": _safe_float(float(sub.std())),
            "win_rate_pct": _safe_float(float((sub > 0).mean() * 100.0)),
        })

    spread = quantile_rows[-1]["ret_mean"] - quantile_rows[0]["ret_mean"]
    monotone = _is_monotonic([q["ret_mean"] for q in quantile_rows])

    # Sharpe of long-top / short-bottom equally weighted
    top_idx = chunks[-1]
    bot_idx = chunks[0]
    ls_returns = r_arr[top_idx].mean() - r_arr[bot_idx].mean()
    ls_std = float(r_arr.std())
    ls_sharpe = ls_returns / ls_std * math.sqrt(252) if ls_std > 0 else 0.0

    # Verdict
    def _verdict(spread, mono):
        if mono and spread > 0.01: return "STRONG MONOTONE"
        if mono: return "MONOTONE — WEAK SPREAD"
        if spread > 0.01: return "STRONG SPREAD — NON-MONOTONE"
        if spread > 0: return "WEAK"
        return "INVERTED"

    return {
        "n_observations": n,
        "n_quantiles": n_q,
        "long_short_spread": _safe_float(spread),
        "long_short_spread_bps": _safe_float(spread * 10000.0),
        "long_short_sharpe_annualized": _safe_float(ls_sharpe),
        "monotonic": bool(monotone),
        "verdict": _verdict(spread, monotone),
        "quantiles": quantile_rows,
    }


# ============================================================================
# DISPATCH TABLE
# ============================================================================

OPERATIONS = {
    "check_status": op_check_status,
    "ic_analysis": op_ic_analysis,
    "cumulative_returns": op_cumulative_returns,
    "risk_report": op_risk_report,
    "model_performance": op_model_performance,
    "factor_quantiles": op_factor_quantiles,
}


def dispatch(operation, data):
    handler = OPERATIONS.get(operation)
    if handler is None:
        return {
            "success": False, "operation": operation,
            "error": f"Unknown operation: {operation}",
            "error_kind": "unknown_op",
            "available": list(OPERATIONS.keys()),
        }
    try:
        result = handler(data or {})
        return {"success": True, "operation": operation, "data": result}
    except ValidationError as e:
        return {"success": False, "operation": operation, "error": str(e),
                "error_kind": "validation"}
    except Exception as e:
        return {"success": False, "operation": operation, "error": str(e),
                "error_kind": "runtime", "traceback": traceback.format_exc()}


def main(args):
    if len(args) < 1:
        print(json.dumps({
            "success": False,
            "error": "Usage: qlib_reporting.py <operation> [json_data]",
            "error_kind": "usage",
        }))
        return
    operation = args[0]
    data = {}
    if len(args) > 1:
        try:
            data = json.loads(args[1])
        except json.JSONDecodeError as e:
            print(json.dumps({
                "success": False, "operation": operation,
                "error": f"Invalid JSON: {e}", "error_kind": "validation",
            }))
            return
    result = dispatch(operation, data)
    print(json.dumps(result, default=str))


if __name__ == "__main__":
    main(sys.argv[1:])
