"""
Business cycle / market-regime indicators from free market data (Yahoo Finance via yfinance).

CLI (one JSON object on stdout, diagnostics on stderr):
    python business_cycle.py get_indicators '{"timeout": 20}'

Params (all optional):
    timeout   seconds allowed for the market-data download (default 20, 3..120)

Response (flat object; any field whose source failed is null and listed in "unavailable"):
    sp500, vix, treasury_10y_yield, treasury_30y_yield, treasury_3m_yield,
    gold_price, crude_oil_price, us_dollar_index            latest close (see "sources" for unit/date)
    <name>_change_pct                                        % change vs the previous close
    <yield>_change_bp                                        change vs the previous close in basis points
                                                             (10y, 30y, 3m yields and the 3m discount rate)
    treasury_3m_yield                                        % bond-equivalent yield of the 13-week bill
    treasury_3m_discount_rate                                ^IRX as quoted (T-bill discount basis, %)
    yield_curve_spread_10y_3m, yield_curve_spread_30y_10y    percentage points
    yield_curve_inverted                                     bool (10y < 3m)
    gold_oil_ratio                                           gold USD/oz over WTI USD/bbl
    sp500_52w_high, sp500_52w_low                            highest / lowest daily CLOSE of the last year
    sp500_sma_50, sp500_sma_200, sp500_above_sma200          simple moving averages of daily closes; bool
    sp500_ytd_return_pct                                     % vs the last close of the previous year
    sp500_1y_return_pct                                      % vs the first close of the 1-year window
    market_sentiment                                         VIX regime label (<15, 15-20, 20-30, >=30)
    sources                                                  {name: {ticker, provider, unit, as_of}}
    unavailable                                              [{name, ticker, reason}]
    as_of                                                    latest bar date across the series
    generated_at                                             UTC timestamp of this run
or {"error": "<plain message>"} when nothing could be fetched or the input is invalid.
"""
import json
import os
import sys
from datetime import datetime, timezone

try:
    import fincept_json
except ImportError:  # run by hand without PYTHONPATH=scripts
    sys.path.append(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))
    import fincept_json


PROVIDER = "Yahoo Finance (yfinance)"

# name -> (ticker, unit)
SERIES = {
    "sp500": ("^GSPC", "index points"),
    "vix": ("^VIX", "index points (annualised implied volatility, %)"),
    "treasury_10y_yield": ("^TNX", "% (yield)"),
    "treasury_30y_yield": ("^TYX", "% (yield)"),
    "treasury_3m_discount_rate": ("^IRX", "% (13-week T-bill discount rate)"),
    "gold_price": ("GC=F", "USD per troy ounce (front-month future)"),
    "crude_oil_price": ("CL=F", "USD per barrel (WTI front-month future)"),
    "us_dollar_index": ("DX-Y.NYB", "index points (ICE DXY)"),
}

TBILL_DAYS = 91  # ^IRX is the 13-week bill
YIELD_SERIES = {"treasury_10y_yield", "treasury_30y_yield", "treasury_3m_discount_rate"}


def _log(msg):
    print(msg, file=sys.stderr)


def _bill_bond_equivalent_yield(discount_pct):
    """T-bill discount rate (%) -> bond-equivalent (investment) yield (%):
    price = 1 - d*t/360, BEY = (1/price - 1) * 365/t = 365 d / (360 - d t)."""
    d = discount_pct / 100.0
    return 365.0 * d / (360.0 - d * TBILL_DAYS) * 100.0


def _parse_params(argv):
    params = {}
    if len(argv) > 2 and argv[2].strip():
        try:
            params = json.loads(argv[2])
        except ValueError as exc:
            raise ValueError(f"Invalid JSON parameters: {exc}")
        if not isinstance(params, dict):
            raise ValueError("Parameters must be a JSON object")
    timeout = params.get("timeout", 20)
    try:
        timeout = float(timeout)
    except (TypeError, ValueError):
        raise ValueError(f"timeout must be a number of seconds, got {timeout!r}")
    if not 3 <= timeout <= 120:
        raise ValueError("timeout must be between 3 and 120 seconds")
    return {"timeout": timeout}


def _closes(frame, ticker):
    """Non-NaN daily closes of one ticker from a group_by='ticker' download."""
    try:
        col = frame[ticker]["Close"]
    except (KeyError, TypeError):
        return None
    col = col.dropna()
    return col if len(col) else None


def get_indicators(timeout=20.0):
    # The shared bar cache (fincept_yahoo): one long download per series shared by every
    # script; daily bars are refreshed after 15 minutes while the market is open. Its own
    # network timeout applies (FINCEPT_YF_TIMEOUT); `timeout` is kept for the contract.
    import fincept_yahoo as fy

    tickers = [t for t, _ in SERIES.values()]
    try:
        frame = fy.download(tickers, period="1y", interval="1d", auto_adjust=False, group_by="ticker")
    except Exception as exc:
        return {"error": f"Market data download failed: {exc}"}
    if frame is None or frame.empty:
        return {"error": "Market data download returned no rows (network or Yahoo unavailable)"}

    out = {}
    sources = {}
    unavailable = []
    closes = {}
    for name, (ticker, unit) in SERIES.items():
        series = _closes(frame, ticker)
        if series is None:
            out[name] = None
            out[f"{name}_change_pct"] = None
            if name in YIELD_SERIES:
                out[f"{name}_change_bp"] = None
            unavailable.append({"name": name, "ticker": ticker, "reason": "no data returned"})
            continue
        closes[name] = series
        last = float(series.iloc[-1])
        prev = float(series.iloc[-2]) if len(series) > 1 else None
        out[name] = round(last, 4)
        out[f"{name}_change_pct"] = round((last / prev - 1.0) * 100.0, 4) if prev else None
        if name in YIELD_SERIES:
            out[f"{name}_change_bp"] = round((last - prev) * 100.0, 2) if prev is not None else None
        sources[name] = {"ticker": ticker, "provider": PROVIDER, "unit": unit,
                         "as_of": series.index[-1].date().isoformat()}

    # 3-month yield on the same (bond-equivalent) basis as the 10y/30y yields.
    disc = out.get("treasury_3m_discount_rate")
    if disc is not None:
        bey = _bill_bond_equivalent_yield(disc)
        out["treasury_3m_yield"] = round(bey, 4)
        prev_disc = closes["treasury_3m_discount_rate"]
        if len(prev_disc) > 1:
            prev_bey = _bill_bond_equivalent_yield(float(prev_disc.iloc[-2]))
            out["treasury_3m_yield_change_pct"] = round((bey / prev_bey - 1.0) * 100.0, 4) if prev_bey else None
            out["treasury_3m_yield_change_bp"] = round((bey - prev_bey) * 100.0, 2)
        else:
            out["treasury_3m_yield_change_pct"] = None
            out["treasury_3m_yield_change_bp"] = None
        sources["treasury_3m_yield"] = dict(sources["treasury_3m_discount_rate"],
                                            unit="% (bond-equivalent yield from the ^IRX discount rate)")
    else:
        out["treasury_3m_yield"] = None
        out["treasury_3m_yield_change_pct"] = None
        out["treasury_3m_yield_change_bp"] = None

    y10, y3m, y30 = out.get("treasury_10y_yield"), out.get("treasury_3m_yield"), out.get("treasury_30y_yield")
    out["yield_curve_spread_10y_3m"] = round(y10 - y3m, 4) if y10 is not None and y3m is not None else None
    out["yield_curve_inverted"] = (y10 < y3m) if y10 is not None and y3m is not None else None
    out["yield_curve_spread_30y_10y"] = round(y30 - y10, 4) if y30 is not None and y10 is not None else None

    gold, oil = out.get("gold_price"), out.get("crude_oil_price")
    # A non-positive oil price (April 2020) makes the ratio meaningless, not infinite.
    out["gold_oil_ratio"] = round(gold / oil, 4) if gold is not None and oil is not None and oil > 0 else None

    sp = closes.get("sp500")
    if sp is not None:
        vals = sp.values.astype(float)
        current = float(vals[-1])
        out["sp500_52w_high"] = round(float(vals.max()), 2)
        out["sp500_52w_low"] = round(float(vals.min()), 2)
        out["sp500_sma_50"] = round(float(vals[-50:].mean()), 2) if len(vals) >= 50 else None
        out["sp500_sma_200"] = round(float(vals[-200:].mean()), 2) if len(vals) >= 200 else None
        out["sp500_above_sma200"] = (current > out["sp500_sma_200"]) if out["sp500_sma_200"] is not None else None
        out["sp500_1y_return_pct"] = round((current / float(vals[0]) - 1.0) * 100.0, 4)
        year_start = datetime(sp.index[-1].year, 1, 1).date()
        before = sp[[d.date() < year_start for d in sp.index]]
        if len(before):
            base = float(before.iloc[-1])
            out["sp500_ytd_return_pct"] = round((current / base - 1.0) * 100.0, 4)
            out["sp500_ytd_base_date"] = before.index[-1].date().isoformat()
        else:
            out["sp500_ytd_return_pct"] = None
            out["sp500_ytd_base_date"] = None
        out["sp500_stats_window"] = {"from": sp.index[0].date().isoformat(),
                                     "to": sp.index[-1].date().isoformat(), "observations": int(len(vals))}
    else:
        for k in ("sp500_52w_high", "sp500_52w_low", "sp500_sma_50", "sp500_sma_200", "sp500_above_sma200",
                  "sp500_1y_return_pct", "sp500_ytd_return_pct", "sp500_ytd_base_date", "sp500_stats_window"):
            out[k] = None

    vix = out.get("vix")
    if vix is None:
        out["market_sentiment"] = None
    elif vix < 15:
        out["market_sentiment"] = "Low Fear (Complacency)"
    elif vix < 20:
        out["market_sentiment"] = "Normal"
    elif vix < 30:
        out["market_sentiment"] = "Elevated Fear"
    else:
        out["market_sentiment"] = "Extreme Fear"

    if not sources:
        return {"error": "Could not fetch any indicator from Yahoo Finance (network or provider unavailable)"}

    out["sources"] = sources
    out["unavailable"] = unavailable
    out["as_of"] = max(s["as_of"] for s in sources.values())
    out["generated_at"] = datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ")
    return out


COMMANDS = {"get_indicators"}


def main(argv=None):
    argv = sys.argv if argv is None else argv
    command = argv[1] if len(argv) > 1 else "get_indicators"
    if command not in COMMANDS:
        fincept_json.emit({"error": f"Unknown command: {command}. Available: {', '.join(sorted(COMMANDS))}"})
        return
    try:
        params = _parse_params(argv)
    except ValueError as exc:
        fincept_json.emit({"error": str(exc)})
        return
    try:
        result = get_indicators(params["timeout"])
    except Exception as exc:  # never a traceback on stdout
        _log(f"business_cycle: {type(exc).__name__}: {exc}")
        result = {"error": f"{type(exc).__name__}: {exc}"}
    fincept_json.emit(result)


if __name__ == "__main__":
    main()
