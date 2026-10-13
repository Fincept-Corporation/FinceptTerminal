"""
portfolio_metrics.py — one-shot market analytics for the Portfolio tab.

Replaces the three separate cold Python spawns the Portfolio screen used to make
on every load (per-symbol correlation download, benchmark history, risk-free
rate). Everything comes from ONE threaded yf.download() call.

CLI
---
    python portfolio_metrics.py analytics            # JSON payload on stdin (what the app does)
    python portfolio_metrics.py analytics '<json>'   # payload as argv (manual runs)

Payload
-------
    {
      "symbols":     ["AAPL", "MSFT", ...],   # current holdings, yfinance tickers (required)
      "weights":     {"AAPL": 0.42, ...},     # market-value weights as FRACTIONS (optional, default equal)
      "benchmark":   "SPY",                   # optional, default "SPY"
      "period":      "1y",                    # optional yfinance period for the history window
      "corr_window": 63                       # optional trailing observations for the correlation matrix
    }

Output (stdout, one JSON object; all returns/ratios are FRACTIONS, not percent)
------
    {
      "as_of": "YYYY-MM-DD",                  # last date in the downloaded data
      "period": "1y",
      "benchmark": {"symbol": "SPY", "dates": [...], "closes": [...]},
      "risk_free": {"rate": 0.0423, "symbol": "^IRX", "as_of": "YYYY-MM-DD",
                    "description": "US 13-week T-bill discount yield"},          # omitted if unavailable
      "correlation": {"window": 63, "start": "...", "end": "...",
                      "matrix": {"AAPL|MSFT": 0.61, ...}},                     # Pearson, daily returns
      "portfolio": {                                                             # ex-ante: CURRENT weights
          "observations": 251, "start": "...", "end": "...", "periods_per_year": 252.0,
          "coverage": 1.0,                    # share of the weight that had price history
          "ann_vol": 0.183,                   # annualised stdev of daily returns
          "var_95_1d": 0.0182,                # 1-day historical VaR, 95%, as a positive loss fraction
          "cvar_95_1d": 0.0261,               # 1-day expected shortfall beyond the VaR
          "beta": 1.07                        # OLS vs benchmark daily returns (null if not estimable)
      },
      "holdings": {"AAPL": {"ann_vol": 0.27, "beta": 1.2, "risk_contribution": 0.31,
                            "observations": 250}, ...},     # risk_contribution = share of portfolio variance
      "missing": ["XYZ"],                      # holdings yfinance returned no history for
      "warnings": ["..."]                      # non-fatal notes (never under "error")
    }

Errors: {"error": "..."} on stdout with exit code 1. Logs go to stderr only.
"""

import contextlib
import io
import json
import logging
import math
import os
import sys

try:
    import fincept_json
except ImportError:  # scripts dir not on sys.path (unusual launcher)
    sys.path.append(os.path.dirname(os.path.abspath(__file__)))
    import fincept_json

RISK_FREE_SYMBOL = "^IRX"  # CBOE 13-week T-bill discount yield, quoted in percent
DEFAULT_BENCHMARK = "SPY"
DEFAULT_PERIOD = "1y"
DEFAULT_CORR_WINDOW = 63  # ~3 months of trading days
MIN_CORR_OBS = 20
MIN_RISK_OBS = 30
ALLOWED_PERIODS = {"3mo", "6mo", "1y", "2y", "5y"}

try:
    _NET_TIMEOUT = float(os.environ.get("FINCEPT_YF_TIMEOUT", "20"))
except (TypeError, ValueError):
    _NET_TIMEOUT = 20.0


def _log(msg):
    print(f"[portfolio_metrics] {msg}", file=sys.stderr, flush=True)


def _fail(msg):
    fincept_json.emit({"error": msg})
    sys.exit(1)


def _finite(x):
    try:
        v = float(x)
    except (TypeError, ValueError):
        return None
    return v if math.isfinite(v) else None


def _download(tickers, period):
    """One threaded yfinance download. yfinance prints progress/errors to stdout,
    which would corrupt the JSON the host parses, so both streams are captured."""
    import yfinance as yf

    yf_logger = logging.getLogger("yfinance")
    prev = yf_logger.level
    yf_logger.setLevel(logging.CRITICAL)
    out_buf, err_buf = io.StringIO(), io.StringIO()
    try:
        with contextlib.redirect_stdout(out_buf), contextlib.redirect_stderr(err_buf):
            data = yf.download(
                tickers=tickers,
                period=period,
                interval="1d",
                group_by="ticker",
                auto_adjust=True,
                progress=False,
                threads=True,
                timeout=_NET_TIMEOUT,
            )
    finally:
        yf_logger.setLevel(prev)
    noise = (out_buf.getvalue() + err_buf.getvalue()).strip()
    if noise:
        _log("yfinance: " + noise.replace("\n", " | ")[:400])
    return data


def _close_series(data, ticker, single):
    """Close prices for one ticker as a float Series with a tz-naive date index."""
    import pandas as pd

    try:
        if single:
            col = data["Close"]
        else:
            col = data[ticker]["Close"]
    except Exception:
        return None
    if isinstance(col, pd.DataFrame):  # newer yfinance can nest a 1-column frame
        if col.shape[1] == 0:
            return None
        col = col.iloc[:, 0]
    s = pd.to_numeric(col, errors="coerce").dropna()
    s = s[s > 0]
    if s.empty:
        return None
    idx = pd.to_datetime(s.index)
    if getattr(idx, "tz", None) is not None:
        idx = idx.tz_localize(None)
    s.index = idx.normalize()
    s = s[~s.index.duplicated(keep="last")].sort_index()
    return s


def _periods_per_year(index):
    """Observations per year implied by the sample itself (≈252 for one equity
    market, more for a mixed-calendar or crypto book)."""
    if len(index) < 2:
        return 252.0
    span_days = (index[-1] - index[0]).days
    if span_days <= 0:
        return 252.0
    ppy = (len(index) - 1) / (span_days / 365.25)
    return float(min(366.0, max(50.0, ppy)))


def analytics(payload):
    import numpy as np
    import pandas as pd

    raw_syms = payload.get("symbols") or []
    if not isinstance(raw_syms, list) or not raw_syms:
        _fail("'symbols' must be a non-empty list of tickers")
    symbols = []
    for s in raw_syms:
        t = str(s).strip().upper()
        if t and t not in symbols:
            symbols.append(t)
    if not symbols:
        _fail("'symbols' contained no usable tickers")

    benchmark = str(payload.get("benchmark") or DEFAULT_BENCHMARK).strip().upper() or DEFAULT_BENCHMARK
    period = str(payload.get("period") or DEFAULT_PERIOD).strip()
    if period not in ALLOWED_PERIODS:
        period = DEFAULT_PERIOD
    try:
        corr_window = int(payload.get("corr_window") or DEFAULT_CORR_WINDOW)
    except (TypeError, ValueError):
        corr_window = DEFAULT_CORR_WINDOW
    corr_window = max(MIN_CORR_OBS, min(corr_window, 504))

    raw_w = payload.get("weights") or {}
    weights = {}
    if isinstance(raw_w, dict):
        for k, v in raw_w.items():
            fv = _finite(v)
            if fv is not None and fv > 0:
                weights[str(k).strip().upper()] = fv

    tickers = list(dict.fromkeys(symbols + [benchmark, RISK_FREE_SYMBOL]))
    warnings = []

    try:
        data = _download(tickers, period)
    except Exception as e:  # network / Yahoo failure
        _fail(f"price download failed: {e}")
    if data is None or getattr(data, "empty", True):
        _fail("no price data returned (network down or every ticker unknown)")

    single = len(tickers) == 1
    closes = {}
    for t in tickers:
        s = _close_series(data, t, single)
        if s is not None and len(s) >= 2:
            closes[t] = s

    as_of_ts = max((s.index[-1] for s in closes.values()), default=None)
    result = {
        "as_of": as_of_ts.strftime("%Y-%m-%d") if as_of_ts is not None else None,
        "period": period,
        "warnings": warnings,
    }

    # ── Benchmark history (chart overlay + beta) ──────────────────────────────
    bench = closes.get(benchmark)
    if bench is not None:
        result["benchmark"] = {
            "symbol": benchmark,
            "dates": [d.strftime("%Y-%m-%d") for d in bench.index],
            "closes": [round(float(v), 6) for v in bench.values],
        }
    else:
        result["benchmark"] = {"symbol": benchmark, "dates": [], "closes": []}
        warnings.append(f"no history for benchmark {benchmark}")

    # ── Risk-free rate: last 13-week T-bill yield ─────────────────────────────
    irx = closes.get(RISK_FREE_SYMBOL)
    if irx is not None and not irx.empty:
        rate = _finite(irx.iloc[-1])
        if rate is not None and -1.0 < rate < 30.0:
            result["risk_free"] = {
                "rate": rate / 100.0,
                "symbol": RISK_FREE_SYMBOL,
                "as_of": irx.index[-1].strftime("%Y-%m-%d"),
                "description": "US 13-week T-bill discount yield",
            }
    if "risk_free" not in result:
        warnings.append("risk-free rate unavailable (^IRX not returned)")

    # ── Per-holding daily returns on each instrument's own trading calendar ──
    priced = [s for s in symbols if s in closes]
    missing = [s for s in symbols if s not in closes]
    result["missing"] = missing
    if missing:
        warnings.append("no price history for: " + ", ".join(missing))

    rets = {}
    for s in priced:
        r = closes[s].pct_change(fill_method=None).dropna()
        r = r[np.isfinite(r)]
        if len(r) >= 2:
            rets[s] = r
    priced = [s for s in priced if s in rets]
    if not priced:
        result["correlation"] = {"window": corr_window, "matrix": {}}
        result["holdings"] = {}
        result["portfolio"] = None
        return result

    rets_df = pd.DataFrame(rets).sort_index()  # union calendar, NaN where a market was shut

    # ── Correlation matrix over the trailing window ───────────────────────────
    tail = rets_df.tail(corr_window)
    matrix = {}
    if len(priced) >= 2:
        corr = tail.corr(min_periods=MIN_CORR_OBS)
        for a in priced:
            for b in priced:
                v = 1.0 if a == b else _finite(corr.at[a, b])
                if v is None:
                    continue
                matrix[f"{a}|{b}"] = round(max(-1.0, min(1.0, v)), 4)
    result["correlation"] = {
        "window": int(len(tail)),
        "start": tail.index[0].strftime("%Y-%m-%d") if len(tail) else None,
        "end": tail.index[-1].strftime("%Y-%m-%d") if len(tail) else None,
        "matrix": matrix,
    }

    # ── Ex-ante portfolio risk with CURRENT weights ───────────────────────────
    w = pd.Series({s: weights.get(s, 0.0) for s in priced}, dtype=float)
    total_w_all = sum(weights.get(s, 0.0) for s in symbols)
    if w.sum() <= 0:
        w = pd.Series(1.0 / len(priced), index=priced)
        coverage = 1.0
        warnings.append("no weights supplied — equal weights assumed")
    else:
        coverage = float(w.sum() / total_w_all) if total_w_all > 0 else 1.0
        w = w / w.sum()

    # Start where every material holding has history, so a recent listing's
    # missing months are not silently treated as zero-risk days.
    material = [s for s in priced if w[s] >= 0.01]
    start = max(rets[s].index[0] for s in material) if material else rets_df.index[0]
    window_df = rets_df[rets_df.index >= start]
    if len(window_df) < MIN_RISK_OBS:
        window_df = rets_df
        warnings.append("short shared history — earliest listings padded with zero returns")
    filled = window_df.fillna(0.0)  # a closed market is a zero-return day for that line
    port = filled[priced].dot(w[priced].values)

    ppy = _periods_per_year(port.index)
    portfolio = {
        "observations": int(len(port)),
        "start": port.index[0].strftime("%Y-%m-%d") if len(port) else None,
        "end": port.index[-1].strftime("%Y-%m-%d") if len(port) else None,
        "periods_per_year": round(ppy, 2),
        "coverage": round(coverage, 6),
        "ann_vol": None,
        "var_95_1d": None,
        "cvar_95_1d": None,
        "beta": None,
    }
    holdings_out = {}
    if len(port) >= MIN_RISK_OBS:
        vals = port.values.astype(float)
        portfolio["ann_vol"] = _finite(np.std(vals, ddof=1) * math.sqrt(ppy))
        q05 = float(np.percentile(vals, 5))
        tail_losses = vals[vals <= q05]
        portfolio["var_95_1d"] = _finite(max(0.0, -q05))
        portfolio["cvar_95_1d"] = _finite(max(0.0, -float(tail_losses.mean()))) if len(tail_losses) else None

        bench_r = None
        if bench is not None:
            bench_r = bench.pct_change(fill_method=None).dropna()
            joined = pd.concat([port.rename("p"), bench_r.rename("b")], axis=1, join="inner").dropna()
            if len(joined) >= MIN_RISK_OBS and joined["b"].var(ddof=1) > 0:
                portfolio["beta"] = _finite(joined["p"].cov(joined["b"]) / joined["b"].var(ddof=1))

        # Euler decomposition: share of portfolio variance carried by each line.
        cov = filled[priced].cov().values * ppy
        wv = w[priced].values
        port_var = float(wv @ cov @ wv)
        marginal = cov @ wv
        for i, s in enumerate(priced):
            own = rets[s]
            own_win = own[own.index >= window_df.index[0]]
            # Annualise each line on its OWN trading calendar (≈252 for one
            # equity market), not the union calendar of the whole book.
            own_ppy = _periods_per_year(own_win.index)
            h = {
                "observations": int(len(own_win)),
                "ann_vol": _finite(own_win.std(ddof=1) * math.sqrt(own_ppy)) if len(own_win) >= 2 else None,
                "beta": None,
                "risk_contribution": _finite(wv[i] * marginal[i] / port_var) if port_var > 0 else None,
                "weight": _finite(wv[i]),
            }
            if bench_r is not None:
                jb = pd.concat([own_win.rename("s"), bench_r.rename("b")], axis=1, join="inner").dropna()
                if len(jb) >= MIN_RISK_OBS and jb["b"].var(ddof=1) > 0:
                    h["beta"] = _finite(jb["s"].cov(jb["b"]) / jb["b"].var(ddof=1))
            holdings_out[s] = h
    else:
        warnings.append(f"only {len(port)} daily observations — ex-ante risk not estimated")

    result["portfolio"] = portfolio
    result["holdings"] = holdings_out
    return result


def _read_payload(argv):
    if len(argv) >= 3 and argv[2].strip():
        text = argv[2]
    else:
        text = sys.stdin.read()
    if not text.strip():
        _fail("no payload: pipe a JSON object on stdin")
    try:
        payload = json.loads(text)
    except Exception as e:
        _fail(f"payload is not valid JSON: {e}")
    if not isinstance(payload, dict):
        _fail("payload must be a JSON object")
    return payload


def main(argv):
    if len(argv) < 2:
        _fail("usage: portfolio_metrics.py analytics [<json>]  (payload on stdin)")
    command = argv[1].strip().lower()
    if command != "analytics":
        _fail(f"unknown command '{command}' (expected: analytics)")
    payload = _read_payload(argv)
    try:
        result = analytics(payload)
    except SystemExit:
        raise
    except Exception as e:  # pandas / numpy edge cases must still come back as JSON
        _fail(f"analytics failed: {type(e).__name__}: {e}")
    fincept_json.emit(result)


if __name__ == "__main__":
    main(sys.argv)
