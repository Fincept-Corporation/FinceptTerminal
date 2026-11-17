"""
Equity risk premium (ERP) of the S&P 500 over the 10-year US Treasury, three methods.

CLI (one JSON object on stdout, diagnostics on stderr):
    python equity_risk_premium.py estimate_erp '{}'
    python equity_risk_premium.py estimate_erp '{"risk_free_rate": 0.045, "buyback_yield": 0.02,
                                                  "real_earnings_growth": 0.02, "windows_years": [5, 10]}'

All rates are decimal fractions per year (0.05 = 5 %). Params (all optional):
    risk_free_rate        nominal 10y Treasury yield. Default: latest ^TNX close / 100 (Yahoo).
    expected_inflation    expected annual inflation. Default: FRED T10YIE (10-year breakeven) when
                          FRED_API_KEY is set; otherwise the Federal Reserve's 2 % longer-run goal,
                          labelled as an assumption.
    pe_ratio              S&P 500 P/E for the earnings-yield method. Default: SPY trailing P/E (Yahoo).
    dividend_yield        Default: SPY distributions of the last 12 months / last SPY close (Yahoo).
    buyback_yield         net buyback yield. No free source: without it the supply-side ERP is null.
    real_earnings_growth  long-run real earnings (≈ real GDP) growth. Without it supply-side is null.
    windows_years         historical windows, default [5, 10] (each 1..30)
    timeout               seconds per network call, default 20 (3..120)

Methods
    historical_<N>y   realised: CAGR of the S&P 500 TOTAL-RETURN index (^SP500TR) minus the
                      annualised total return of a constant-maturity 10-year Treasury over the same
                      N years (monthly returns from ^TNX: Y0/12 + price change of a par bond with
                      coupon Y0 repriced at Y1 with 10 - 1/12 years left, Swinkels 2019).
    implied_earnings_yield   E/P - real 10y rate; real rate = (1 + rf)/(1 + expected inflation) - 1.
                      (E/P estimates a REAL expected return, so it is compared with a real rate.)
    supply_side       dividend yield + buyback yield + real earnings growth + expected inflation - rf
                      (Grinold-Kroner without a repricing term).
    consensus_erp_estimate = mean of {longest historical window, implied_earnings_yield, supply_side}
                      that are available; erp_range_low/high span every estimate.

Response: erp_estimates {method: {erp, ...components, method}}, the flat fields below, inputs
{name: {value, source, as_of}}, units, as_of, generated_at; or {"error": "<plain message>"}.
    historical_<N>y_erp, sp500_<N>y_annualized, treasury10y_<N>y_annualized,
    implied_erp_earnings_yield, sp500_earnings_yield, sp500_pe_ratio,
    supply_side_erp, dividend_yield, risk_free_rate_10y, real_risk_free_rate_10y,
    expected_inflation, consensus_erp_estimate, erp_range_low, erp_range_high
"""
import json
import math
import os
import re
import sys
from datetime import datetime, timezone

try:
    import fincept_json
except ImportError:  # run by hand without PYTHONPATH=scripts
    sys.path.append(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))
    import fincept_json


YAHOO = "Yahoo Finance (yfinance)"
FED_INFLATION_GOAL = 0.02          # FOMC longer-run inflation goal (policy anchor, not a market quote)
BOND_MATURITY_YEARS = 10.0


def _log(msg):
    print(msg, file=sys.stderr)


def _today():
    return datetime.now(timezone.utc).date().isoformat()


def _with_timeout(fn, timeout):
    """fn() in a daemon thread; TimeoutError after `timeout` s (the thread is abandoned)."""
    import threading
    box = {}

    def run():
        try:
            box["value"] = fn()
        except Exception as exc:  # re-raised in the caller
            box["error"] = exc

    worker = threading.Thread(target=run, daemon=True)
    worker.start()
    worker.join(timeout)
    if worker.is_alive():
        raise TimeoutError(f"no answer within {timeout:g} s")
    if "error" in box:
        raise box["error"]
    return box["value"]


# ── parameters ───────────────────────────────────────────────────────────────

def _rate(params, key, lo=-0.5, hi=1.0):
    """Optional decimal rate parameter; percent-looking values are rejected, not guessed."""
    raw = params.get(key)
    if raw is None:
        return None
    if isinstance(raw, bool):
        raise ValueError(f"{key} must be a number")
    try:
        value = float(raw)
    except (TypeError, ValueError):
        raise ValueError(f"{key} must be a number, got {raw!r}")
    if not math.isfinite(value) or not lo <= value <= hi:
        raise ValueError(f"{key} must be a decimal fraction between {lo:g} and {hi:g} (0.045 = 4.5 %), got {raw!r}")
    return value


def _parse_params(argv):
    params = {}
    if len(argv) > 2 and argv[2].strip():
        try:
            params = json.loads(argv[2])
        except ValueError as exc:
            raise ValueError(f"Invalid JSON parameters: {exc}")
        if not isinstance(params, dict):
            raise ValueError("Parameters must be a JSON object")
    out = {
        "risk_free_rate": _rate(params, "risk_free_rate", -0.05, 0.5),
        "expected_inflation": _rate(params, "expected_inflation", -0.05, 0.5),
        "dividend_yield": _rate(params, "dividend_yield", 0.0, 0.5),
        "buyback_yield": _rate(params, "buyback_yield", -0.2, 0.5),
        "real_earnings_growth": _rate(params, "real_earnings_growth", -0.2, 0.5),
    }
    pe = params.get("pe_ratio")
    if pe is not None:
        try:
            pe = float(pe)
        except (TypeError, ValueError):
            raise ValueError(f"pe_ratio must be a number, got {pe!r}")
        if not math.isfinite(pe) or pe <= 0:
            raise ValueError("pe_ratio must be positive")
    out["pe_ratio"] = pe
    windows = params.get("windows_years", [5, 10])
    if not isinstance(windows, list) or not windows:
        raise ValueError("windows_years must be a non-empty list of whole years, e.g. [5, 10]")
    try:
        windows = sorted({int(w) for w in windows if float(w) == int(w)})
    except (TypeError, ValueError):
        raise ValueError("windows_years must contain whole numbers of years")
    if not windows or windows[0] < 1 or windows[-1] > 30:
        raise ValueError("windows_years entries must be whole years between 1 and 30")
    out["windows_years"] = windows
    timeout = params.get("timeout", 20)
    try:
        timeout = float(timeout)
    except (TypeError, ValueError):
        raise ValueError(f"timeout must be a number of seconds, got {timeout!r}")
    if not 3 <= timeout <= 120:
        raise ValueError("timeout must be between 3 and 120 seconds")
    out["timeout"] = timeout
    return out


# ── data ─────────────────────────────────────────────────────────────────────

def _download_history(years, timeout):
    """Daily closes of ^SP500TR and ^TNX aligned on common dates (DataFrame: equity, yield_pct)."""
    import pandas as pd
    import fincept_yahoo as fy
    # The shared bar cache: one long download per series shared by every script.
    frame = fy.download(["^SP500TR", "^TNX"], period=f"{years + 1}y", interval="1d", auto_adjust=False,
                        group_by="ticker")
    if frame is None or frame.empty:
        raise RuntimeError("Yahoo returned no rows for ^SP500TR / ^TNX")
    cols = {}
    for name, ticker in (("equity", "^SP500TR"), ("yield_pct", "^TNX")):
        try:
            cols[name] = frame[ticker]["Close"]
        except KeyError:
            raise RuntimeError(f"Yahoo returned no data for {ticker}")
    data = pd.DataFrame(cols).dropna()
    if data.empty:
        raise RuntimeError("No common dates for ^SP500TR and ^TNX")
    return data


def _fred_latest(series_id, timeout):
    """(value in percent, date) of the latest FRED observation; raises with the key redacted."""
    key = os.environ.get("FRED_API_KEY", "").strip()
    if not key:
        raise LookupError("FRED_API_KEY is not set")
    from urllib.parse import urlencode
    from urllib.request import urlopen
    url = "https://api.stlouisfed.org/fred/series/observations?" + urlencode({
        "series_id": series_id, "api_key": key, "file_type": "json", "sort_order": "desc", "limit": 10})
    try:
        with urlopen(url, timeout=timeout) as resp:
            payload = json.loads(resp.read().decode("utf-8"))
    except Exception as exc:
        raise RuntimeError(re.sub(r"(api_key=)[^&\s]+", r"\1***", f"FRED {series_id} request failed: {exc}"))
    for obs in payload.get("observations", []):
        try:
            return float(obs["value"]), obs["date"]
        except (KeyError, TypeError, ValueError):
            continue  # FRED marks missing days with "."
    raise RuntimeError(f"FRED {series_id} returned no numeric observation")


# ── methods ──────────────────────────────────────────────────────────────────

def _bond_month_return(y0, y1, maturity=BOND_MATURITY_YEARS):
    """One-month total return of a constant-maturity par bond (semiannual coupons) bought at
    yield y0 and sold at y1 with maturity - 1/12 years left (Swinkels 2019). Decimal yields."""
    n = 2.0 * (maturity - 1.0 / 12.0)
    if abs(y1) < 1e-12:
        price = y0 / 2.0 * n + 1.0
    else:
        v = (1.0 + y1 / 2.0) ** (-n)
        price = (y0 / y1) * (1.0 - v) + v
    return price - 1.0 + y0 / 12.0


def _historical(data, years):
    """Realised ERP over the last `years` years on a monthly grid ending at the last common date."""
    import pandas as pd
    end = data.index[-1]
    grid = []
    for k in range(12 * years, -1, -1):
        target = end - pd.DateOffset(months=k)
        pos = data.index.searchsorted(target, side="right") - 1
        if pos < 0:
            return None, f"history starts {data.index[0].date()}, needs {years} years before {end.date()}"
        grid.append(data.index[pos])
    eq =data.loc[grid, "equity"].to_numpy(dtype=float)
    yd = data.loc[grid, "yield_pct"].to_numpy(dtype=float) / 100.0
    span_years = (grid[-1] - grid[0]).days / 365.25
    equity_cagr = (eq[-1] / eq[0]) ** (1.0 / span_years) - 1.0
    growth = 1.0
    for y0, y1 in zip(yd[:-1], yd[1:]):
        growth *= 1.0 + _bond_month_return(y0, y1)
    bond_cagr = growth ** (1.0 / span_years) - 1.0
    return {
        "erp": equity_cagr - bond_cagr,
        "equity_return_annualized": equity_cagr,
        "treasury10y_return_annualized": bond_cagr,
        "window_start": grid[0].date().isoformat(),
        "window_end": grid[-1].date().isoformat(),
        "method": ("S&P 500 total-return CAGR (^SP500TR) minus constant-maturity 10y Treasury "
                   "total-return CAGR (monthly, from ^TNX), same window"),
    }, None


def estimate_erp(p):
    import fincept_yahoo as fy  # shared Yahoo layer: daily profile cache, bar cache, limiter

    inputs = {}
    estimates = {}
    notes = []
    timeout = p["timeout"]

    # Market history (also gives the current 10y yield).
    data = None
    try:
        data = _download_history(max(p["windows_years"]), timeout)
    except Exception as exc:
        notes.append(f"history unavailable: {exc}")
        _log(f"equity_risk_premium: history: {exc}")

    # Nominal 10y risk-free rate.
    if p["risk_free_rate"] is not None:
        rf = p["risk_free_rate"]
        inputs["risk_free_rate_10y"] = {"value": rf, "source": "input risk_free_rate", "as_of": None}
    elif data is not None:
        rf = float(data["yield_pct"].iloc[-1]) / 100.0
        inputs["risk_free_rate_10y"] = {"value": rf, "source": f"^TNX close, {YAHOO}",
                                        "as_of": data.index[-1].date().isoformat()}
    else:
        rf = None
        inputs["risk_free_rate_10y"] = {"value": None, "source": "unavailable (pass risk_free_rate)",
                                        "as_of": None}

    # Expected inflation.
    if p["expected_inflation"] is not None:
        infl = p["expected_inflation"]
        inputs["expected_inflation"] = {"value": infl, "source": "input expected_inflation", "as_of": None}
    else:
        try:
            value, date = _fred_latest("T10YIE", timeout)
            infl = value / 100.0
            inputs["expected_inflation"] = {"value": infl, "source": "FRED T10YIE (10-year breakeven inflation)",
                                            "as_of": date}
        except Exception as exc:
            infl = FED_INFLATION_GOAL
            why = "FRED_API_KEY not set" if isinstance(exc, LookupError) else str(exc)
            inputs["expected_inflation"] = {
                "value": infl, "as_of": None,
                "source": (f"assumption: Federal Reserve 2 % longer-run inflation goal ({why}; set "
                           f"FRED_API_KEY for the market 10-year breakeven, or pass expected_inflation)")}

    real_rf = (1.0 + rf) / (1.0 + infl) - 1.0 if rf is not None else None

    # 1. Historical windows.
    for years in p["windows_years"]:
        key = f"historical_{years}y"
        if data is None:
            estimates[key] = {"erp": None, "reason": "price history unavailable"}
            continue
        res, why = _historical(data, years)
        estimates[key] = res if res is not None else {"erp": None, "reason": why}

    # SPY: P/E (the day's profile, shared) and trailing dividends (the bar cache).
    pe = p["pe_ratio"]
    if pe is not None:
        inputs["pe_ratio"] = {"value": pe, "source": "input pe_ratio", "as_of": None}
    else:
        try:
            info = _with_timeout(lambda: fy.info("SPY"), timeout)  # fy.info takes no timeout of its own
            pe = info.get("trailingPE")
            pe = float(pe) if pe is not None and float(pe) > 0 else None
        except Exception as exc:
            _log(f"equity_risk_premium: SPY info: {exc}")
            pe = None
        inputs["pe_ratio"] = {"value": pe, "as_of": _today() if pe is not None else None,
                              "source": f"SPY trailingPE, {YAHOO}" if pe is not None
                              else "unavailable (pass pe_ratio)"}

    div_yield = p["dividend_yield"]
    if div_yield is not None:
        inputs["dividend_yield"] = {"value": div_yield, "source": "input dividend_yield", "as_of": None}
    else:
        try:
            import pandas as pd
            hist = fy.ticker_history("SPY", period="13mo", interval="1d", actions=True, auto_adjust=False)
            last_date = hist.index[-1]
            last_close = float(hist["Close"].dropna().iloc[-1])
            divs = hist["Dividends"]
            window = divs[divs.index > last_date - pd.DateOffset(years=1)]
            div_yield = float(window.sum()) / last_close if last_close > 0 else None
            inputs["dividend_yield"] = {
                "value": div_yield, "as_of": last_date.date().isoformat(),
                "source": f"SPY distributions over the last 12 months / last SPY close, {YAHOO}"}
        except Exception as exc:
            _log(f"equity_risk_premium: SPY dividends: {exc}")
            div_yield = None
            inputs["dividend_yield"] = {"value": None, "source": "unavailable (pass dividend_yield)",
                                        "as_of": None}

    # 2. Implied ERP from the earnings yield.
    if pe is None or real_rf is None:
        estimates["implied_earnings_yield"] = {
            "erp": None, "reason": "needs a P/E ratio and a risk-free rate (see inputs)"}
    else:
        ey = 1.0 / pe
        estimates["implied_earnings_yield"] = {
            "erp": ey - real_rf, "earnings_yield": ey, "pe_ratio": pe, "real_risk_free_rate": real_rf,
            "method": "earnings yield (1/PE) minus real 10y rate (1+rf)/(1+expected inflation)-1"}

    # 3. Supply side.
    missing = [k for k, v in (("buyback_yield", p["buyback_yield"]),
                              ("real_earnings_growth", p["real_earnings_growth"])) if v is None]
    if div_yield is None:
        missing.append("dividend_yield")
    if rf is None:
        missing.append("risk_free_rate")
    for k in ("buyback_yield", "real_earnings_growth"):
        inputs[k] = {"value": p[k], "source": f"input {k}" if p[k] is not None
                     else f"not supplied (no free data source; pass {k})", "as_of": None}
    if missing:
        estimates["supply_side"] = {"erp": None, "reason": "needs " + ", ".join(missing)}
    else:
        expected = div_yield + p["buyback_yield"] + p["real_earnings_growth"] + infl
        estimates["supply_side"] = {
            "erp": expected - rf, "expected_equity_return": expected, "dividend_yield": div_yield,
            "buyback_yield": p["buyback_yield"], "real_earnings_growth": p["real_earnings_growth"],
            "expected_inflation": infl,
            "method": "dividend yield + buyback yield + real earnings growth + expected inflation - rf"}

    # Flat fields (legacy names kept) and the summary.
    out = {"erp_estimates": estimates}
    for years in p["windows_years"]:
        h = estimates[f"historical_{years}y"]
        out[f"historical_{years}y_erp"] = h.get("erp")
        out[f"sp500_{years}y_annualized"] = h.get("equity_return_annualized")
        out[f"treasury10y_{years}y_annualized"] = h.get("treasury10y_return_annualized")
    ie = estimates["implied_earnings_yield"]
    out["implied_erp_earnings_yield"] = ie.get("erp")
    out["sp500_earnings_yield"] = ie.get("earnings_yield")
    out["sp500_pe_ratio"] = pe
    out["supply_side_erp"] = estimates["supply_side"].get("erp")
    out["dividend_yield"] = div_yield
    out["risk_free_rate_10y"] = rf
    out["real_risk_free_rate_10y"] = real_rf
    out["expected_inflation"] = infl

    longest = f"historical_{max(p['windows_years'])}y"
    core = [estimates[k]["erp"] for k in (longest, "implied_earnings_yield", "supply_side")
            if estimates[k].get("erp") is not None]
    every = [e["erp"] for e in estimates.values() if e.get("erp") is not None]
    if not every:
        return {"error": "Could not compute any ERP estimate: " + ("; ".join(notes) or "no data")}
    out["consensus_erp_estimate"] = sum(core) / len(core) if core else None
    out["consensus_method"] = (f"mean of the available estimates among {longest}, implied_earnings_yield, "
                               f"supply_side ({len(core)} used)")
    out["erp_range_low"] = min(every)
    out["erp_range_high"] = max(every)
    out["inputs"] = inputs
    out["units"] = "decimal fractions per year (0.05 = 5 %); pe_ratio is a multiple"
    if notes:
        out["notes"] = notes
    out["as_of"] = data.index[-1].date().isoformat() if data is not None else _today()
    out["generated_at"] = datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ")
    return _round(out)


def _round(obj, nd=6):
    if isinstance(obj, float):
        return round(obj, nd) if math.isfinite(obj) else None
    if isinstance(obj, dict):
        return {k: _round(v, nd) for k, v in obj.items()}
    if isinstance(obj, list):
        return [_round(v, nd) for v in obj]
    return obj


COMMANDS = {"estimate_erp"}


def main(argv=None):
    argv = sys.argv if argv is None else argv
    command = argv[1] if len(argv) > 1 else "estimate_erp"
    if command not in COMMANDS:
        fincept_json.emit({"error": f"Unknown command: {command}. Available: {', '.join(sorted(COMMANDS))}"})
        return
    try:
        params = _parse_params(argv)
    except ValueError as exc:
        fincept_json.emit({"error": str(exc)})
        return
    try:
        result = estimate_erp(params)
    except Exception as exc:  # never a traceback on stdout
        _log(f"equity_risk_premium: {type(exc).__name__}: {exc}")
        result = {"error": f"{type(exc).__name__}: {exc}"}
    fincept_json.emit(result)


if __name__ == "__main__":
    main()
