"""
portfolio_metrics.py — market analytics, NAV replay, what-if and historical stress
for the Portfolio tab. Every return is measured in the portfolio's BASE currency:
prices are converted with daily FX closes (Yahoo "XXXUSD=X" / "USDXXX=X" legs,
crossed through USD), so currency risk is part of every figure.

CLI (payload = one JSON object on stdin, or as argv[2] for manual runs)
---
    python portfolio_metrics.py analytics          # ex-ante risk, correlation, benchmark, risk-free, VaR
    python portfolio_metrics.py nav_replay         # daily NAV history from the ledger
    python portfolio_metrics.py what_if            # before/after risk of a re-weighting
    python portfolio_metrics.py historical_stress  # current book through a past window

ONE set of definitions (shared with the C++ PortfolioPerformance and the other
portfolio scripts, which import the helpers below):
  * observations    = daily returns compounded onto the benchmark's trading days
                      (compound_onto); periods_per_year from the payload (the
                      benchmark venue's trading days), else measured from the
                      benchmark's own observation count (calendar_periods_per_year).
  * volatility      = sample stdev × √periods_per_year
  * Sharpe          = (mean − rf_per_period) / stdev × √ppy, rf_per_period = (1+rf)^(1/ppy) − 1
  * Sortino         = same numerator / downside deviation vs rf over ALL periods
  * annualised ret. = (1 + total)^(365.25 / calendar days) − 1

analytics payload
-----------------
    {"symbols": [...], "weights": {SYM: fraction of the valued book}, "currencies": {SYM: "GBp"},
     "base_currency": "USD", "benchmark": "SPY" | "SPY:60,AGG:40", "period": "1y|2y|5y|10y|max",
     "corr_window": 63, "periods_per_year": 252,
     "var": {"confidences": [0.95, 0.99], "horizon_days": 1, "mc_paths": 10000, "seed": null},
     "risk_free_currency": "USD"}
analytics output (fractions, not percent)
-----------------
    {"as_of", "period", "base_currency",
     "benchmark": {"symbol", "dates", "closes" (base-currency level), "legs": [...], "total_return": bool},
     "risk_free": {"rate", "currency", "symbol", "description", "as_of"}            # omitted if unavailable
     "correlation": {"window", "start", "end", "matrix": {"A|B": r}},
     "portfolio": {"observations", "start", "end", "periods_per_year", "coverage", "ann_vol", "var_95_1d",
                   "cvar_95_1d", "param_var_95_1d", "beta", "benchmark_corr", "tracking_error",
                   "diversification_ratio", "fx_vol_contribution"},
     "var": [{"confidence", "horizon_days", "historical", "cvar", "parametric", "monte_carlo",
              "monte_carlo_cvar", "horizon_scaled"}],
     "var_confidence", "var_horizon_days", "mc_paths",
     "holdings": {SYM: {"ann_vol", "beta", "risk_contribution", "marginal_var", "component_var", "fx_vol",
                        "observations", "weight"}},
     "missing": [...], "warnings": [...]}

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

DEFAULT_PERIOD = "1y"
DEFAULT_CORR_WINDOW = 63  # ~3 months of trading days
MIN_CORR_OBS = 20
MIN_RISK_OBS = 30
ALLOWED_PERIODS = {"3mo", "6mo", "1y", "2y", "5y", "10y", "max"}
RISK_WINDOW_DAYS = 366     # ex-ante risk always on the trailing year
FX_FFILL_DAYS = 5          # an FX close is carried over at most this many rows
GLOBAL_BENCHMARK = "ACWI"


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


# ── Currencies ────────────────────────────────────────────────────────────────

# Minor-unit codes exactly as Yahoo reports them (case matters: "GBp" is pence, "GBP" pounds).
_MINOR = {"GBp": ("GBP", 100.0), "GBX": ("GBP", 100.0), "ZAc": ("ZAR", 100.0), "ZAC": ("ZAR", 100.0),
          "ILA": ("ILS", 100.0), "KWF": ("KWD", 1000.0), "USX": ("USD", 100.0), "USc": ("USD", 100.0)}
# Mirror of core/market/InstrumentCurrency for symbols whose quote did not say.
_SUFFIX_CCY = {
    "NS": "INR", "BO": "INR", "L": "GBp", "IL": "", "TO": "CAD", "V": "CAD", "CN": "CAD", "NE": "CAD",
    "AX": "AUD", "NZ": "NZD", "HK": "HKD", "T": "JPY", "KS": "KRW", "KQ": "KRW", "TW": "TWD", "TWO": "TWD",
    "SS": "CNY", "SZ": "CNY", "SI": "SGD", "DE": "EUR", "F": "EUR", "BE": "EUR", "DU": "EUR", "HM": "EUR",
    "HA": "EUR", "MU": "EUR", "SG": "EUR", "PA": "EUR", "AS": "EUR", "BR": "EUR", "LS": "EUR", "MC": "EUR",
    "MI": "EUR", "IR": "EUR", "VI": "EUR", "HE": "EUR", "AT": "EUR", "SW": "CHF", "ST": "SEK", "OL": "NOK",
    "CO": "DKK", "IC": "ISK", "SA": "BRL", "MX": "MXN", "JO": "ZAc", "TA": "ILA", "KL": "MYR", "BK": "THB",
    "JK": "IDR", "IS": "TRY", "SR": "SAR", "WA": "PLN", "PR": "CZK", "BD": "HUF", "CA": "EGP", "QA": "QAR",
    "VN": "VND", "PS": "PHP", "AE": "AED", "KW": "KWF", "BA": "ARS", "SN": "CLP", "LM": "PEN", "CL": "COP",
}


def major_ccy(ccy):
    """(ISO major code, divisor) of a quoted currency: "GBp" -> ("GBP", 100)."""
    c = (ccy or "").strip()
    if c in _MINOR:
        return _MINOR[c]
    return (c.upper(), 1.0) if c else ("", 1.0)


def ccy_for_symbol(sym):
    """Quoted currency implied by a Yahoo symbol ("" when unknown)."""
    s = (sym or "").strip().upper()
    if not s or s.startswith("^"):
        return ""
    if s.endswith("=X"):
        return s[3:6] if len(s) >= 8 else s[:3]
    if s.endswith("=F"):
        return "USD"
    if "." in s:
        return _SUFFIX_CCY.get(s.rsplit(".", 1)[1], "")
    if "-" in s:
        q = s.rsplit("-", 1)[1]
        if q in ("USDT", "USDC"):
            return "USD"
        if len(q) == 3 and q.isalpha():
            return q
    return "USD"


_INDEX_CCY = {"^GSPC": "USD", "^IXIC": "USD", "^DJI": "USD", "^RUT": "USD", "^FTSE": "GBP", "^FTAS": "GBP",
              "^STOXX50E": "EUR", "^STOXX": "EUR", "^GDAXI": "EUR", "^FCHI": "EUR", "^N225": "JPY",
              "^HSI": "HKD", "^NSEI": "INR", "^BSESN": "INR", "^AXJO": "AUD", "^GSPTSE": "CAD", "^SSMI": "CHF",
              "^NZ50": "NZD", "^OMX": "SEK", "^OMXC25": "DKK", "^KS11": "KRW", "^TWII": "TWD", "^BVSP": "BRL",
              "^MXX": "MXN", "^JKSE": "IDR", "^KLSE": "MYR", "^STI": "SGD", "000001.SS": "CNY"}


def usd_leg(ccy):
    """(Yahoo symbol, invert) of a currency's USD leg; (None, False) for USD."""
    c = (ccy or "").upper()
    if not c or c == "USD":
        return None, False
    if c in ("EUR", "GBP", "AUD", "NZD"):
        return f"{c}USD=X", False  # USD per 1 c
    return f"USD{c}=X", True       # c per 1 USD -> invert


def fx_tickers(ccys, base):
    out = []
    for c in set(list(ccys) + [base]):
        t, _ = usd_leg(c)
        if t and t not in out:
            out.append(t)
    return out


def fx_series(closes, ccy, base, index=None):
    """Base units per 1 major unit of `ccy` (pandas Series), from the USD legs in
    `closes` (ticker -> close series). None when a leg is missing."""
    import pandas as pd

    def leg(c):
        t, inv = usd_leg(c)
        if t is None:
            return None  # USD itself
        s = closes.get(t)
        if s is None or s.empty:
            return False
        return (1.0 / s) if inv else s

    ccy, base = (ccy or "").upper(), (base or "").upper()
    if ccy == base:
        if index is None:
            return None
        return pd.Series(1.0, index=index)
    a, b = leg(ccy), leg(base)   # USD per 1 unit
    if a is False or b is False:
        return None
    if a is None:
        s = 1.0 / b
    elif b is None:
        s = a
    else:
        joined = pd.concat([a.rename("a"), b.rename("b")], axis=1).sort_index().ffill(limit=FX_FFILL_DAYS)
        s = (joined["a"] / joined["b"]).dropna()
    s = s[~s.index.duplicated(keep="last")].sort_index()
    if index is not None:
        s = s.reindex(s.index.union(index)).sort_index().ffill(limit=FX_FFILL_DAYS).reindex(index)
    return s


# ── Download / series helpers ────────────────────────────────────────────────

def download(tickers, period=None, start=None, end=None, auto_adjust=True, actions=False):
    """Daily bars of `tickers` in yf.download()'s layout (group_by="ticker"), served by
    fincept_yahoo's shared bar cache: one long download per symbol shared by every script and
    process, later calls slice it locally or fetch only the missing tail. Library output is
    captured: stdout carries only the JSON the host parses."""
    import fincept_yahoo as fy

    yf_logger = logging.getLogger("yfinance")
    prev = yf_logger.level
    yf_logger.setLevel(logging.CRITICAL)
    out_buf, err_buf = io.StringIO(), io.StringIO()
    kwargs = dict(tickers=list(tickers), interval="1d", group_by="ticker", auto_adjust=auto_adjust,
                  actions=actions)
    if start is not None:
        kwargs["start"] = start
        if end is not None:
            kwargs["end"] = end
    else:
        kwargs["period"] = period or DEFAULT_PERIOD
    try:
        with contextlib.redirect_stdout(out_buf), contextlib.redirect_stderr(err_buf):
            data = fy.download(**kwargs)
    finally:
        yf_logger.setLevel(prev)
    noise = (out_buf.getvalue() + err_buf.getvalue()).strip()
    if noise:
        _log("yfinance: " + noise.replace("\n", " | ")[:400])
    return data


def column(data, ticker, field="Close", single=False, positive=True):
    """One field of one ticker as a float Series with a tz-naive date index."""
    import pandas as pd

    # yfinance's column layout depends on the version and the ticker count: a
    # one-ticker download may come back flat (field) or grouped (ticker, field).
    col = None
    cols = getattr(data, "columns", None)
    if isinstance(cols, pd.MultiIndex):
        for key in ((ticker, field), (field, ticker)):
            if key in cols:
                col = data[key]
                break
    if col is None and single:
        try:
            col = data[field]
        except Exception:
            col = None
    if col is None:
        try:
            col = data[ticker][field]
        except Exception:
            return None
    if isinstance(col, pd.DataFrame):  # newer yfinance can nest a 1-column frame
        if col.shape[1] == 0:
            return None
        col = col.iloc[:, 0]
    s = pd.to_numeric(col, errors="coerce").dropna()
    if positive:
        s = s[s > 0]
    if s.empty:
        return None
    idx = pd.to_datetime(s.index)
    if getattr(idx, "tz", None) is not None:
        idx = idx.tz_localize(None)
    s.index = idx.normalize()
    s = s[~s.index.duplicated(keep="last")].sort_index()
    return s


def closes_for(data, tickers, field="Close", single=None):
    """`single`: the DOWNLOAD had one ticker (flat columns). Default: len(tickers) == 1,
    which is only right when `tickers` is the full download list."""
    if single is None:
        single = len(tickers) == 1
    out = {}
    for t in tickers:
        s = column(data, t, field, single)
        if s is not None and len(s) >= 1:
            out[t] = s
    return out


def to_base(series, quoted_ccy, base, fx_closes):
    """Price series in the base currency (major units). None when FX is missing."""
    major, div = major_ccy(quoted_ccy)
    if not major:
        major = base
    s = series / div
    if major == base:
        return s
    fx = fx_series(fx_closes, major, base, s.index)
    if fx is None:
        return None
    out = (s * fx).dropna()
    return out if len(out) else None


def calendar_periods_per_year(index):
    """Observations per year of a calendar (≈252 for one equity venue, 365 crypto)."""
    if len(index) < 2:
        return 252.0
    span = (index[-1] - index[0]).days
    if span <= 0:
        return 252.0
    return float(min(366.0, max(50.0, (len(index) - 1) / (span / 365.25))))


def compound_onto(returns, cal_index):
    """Compound daily returns onto calendar dates: each return goes to the first
    calendar date on/after it (a weekend crypto move lands on Monday)."""
    import numpy as np
    import pandas as pd

    r = returns.dropna()
    if r.empty or len(cal_index) == 0:
        return r
    pos = np.searchsorted(cal_index.values, r.index.values, side="left")
    ok = pos < len(cal_index)
    r = r[ok]
    keys = cal_index[pos[ok]]
    g = (1.0 + r).groupby(keys).prod() - 1.0
    g.index = pd.DatetimeIndex(g.index)
    return g


def weekday_calendar(start, end):
    import pandas as pd
    return pd.bdate_range(start, end)


def parse_benchmark(spec):
    """[(symbol, weight)] normalised; [] on error."""
    out, total = [], 0.0
    for part in str(spec or "").split(","):
        p = part.strip()
        if not p:
            continue
        sym, w = p, 1.0
        if ":" in p:
            sym, ws = p.rsplit(":", 1)
            w = _finite(ws)
            if w is None or w <= 0:
                return []
        sym = sym.strip().upper()
        if not sym:
            return []
        out.append((sym, w))
        total += w
    if not out or total <= 0:
        return []
    return [(s, w / total) for s, w in out]


def sharpe_sortino(rets, rf_annual, ppy):
    """(ann_vol, sharpe, sortino) with the shared definitions; None when undefined."""
    import numpy as np

    r = rets.dropna().values.astype(float)
    if len(r) < 2:
        return None, None, None
    rf_p = (1.0 + rf_annual) ** (1.0 / ppy) - 1.0
    sd = float(np.std(r, ddof=1))
    mean = float(np.mean(r))
    vol = sd * math.sqrt(ppy)
    sharpe = (mean - rf_p) / sd * math.sqrt(ppy) if sd > 0 else None
    dd = float(np.sqrt(np.mean(np.minimum(r - rf_p, 0.0) ** 2)))
    sortino = (mean - rf_p) / dd * math.sqrt(ppy) if dd > 0 else None
    return vol, sharpe, sortino


# ── Risk-free rates by currency ──────────────────────────────────────────────

def _http_text(url, timeout=8):
    import urllib.request
    req = urllib.request.Request(url, headers={"User-Agent": "Mozilla/5.0", "Accept": "*/*"})
    return urllib.request.urlopen(req, timeout=timeout).read().decode("utf-8", "replace")


def _rf_ecb_estr():
    txt = _http_text("https://data-api.ecb.europa.eu/service/data/EST/B.EU000A2X2A25.WT"
                     "?lastNObservations=1&format=csvdata")
    import csv
    rows = list(csv.DictReader(io.StringIO(txt)))
    if not rows:
        return None
    v = _finite(rows[-1].get("OBS_VALUE"))
    return (v / 100.0, rows[-1].get("TIME_PERIOD"), "€STR", "Euro short-term rate (ECB)") if v is not None else None


def _rf_boe_sonia():
    import datetime as _dt
    start = (_dt.date.today() - _dt.timedelta(days=30)).strftime("%d/%b/%Y")
    txt = _http_text("https://www.bankofengland.co.uk/boeapps/database/_iadb-fromshowcolumns.asp?csv.x=yes"
                     f"&Datefrom={start}&Dateto=now&SeriesCodes=IUDSOIA&CSVF=TN&UsingCodes=Y&VPD=Y&VFD=N")
    lines = [ln for ln in txt.strip().splitlines() if ln.strip()]
    for ln in reversed(lines[1:]):
        parts = ln.split(",")
        if len(parts) >= 2:
            v = _finite(parts[1])
            if v is not None:
                as_of = parts[0].strip()
                try:
                    as_of = _dt.datetime.strptime(as_of, "%d %b %Y").date().isoformat()
                except ValueError:
                    pass
                return v / 100.0, as_of, "SONIA", "Sterling overnight index average (Bank of England)"
    return None


def _rf_boc_corra():
    txt = _http_text("https://www.bankofcanada.ca/valet/observations/AVG.INTWO/json?recent=5")
    obs = json.loads(txt).get("observations") or []
    for o in reversed(obs):
        v = _finite((o.get("AVG.INTWO") or {}).get("v"))
        if v is not None:
            return v / 100.0, o.get("d"), "CORRA", "Canadian overnight repo rate average (Bank of Canada)"
    return None


def _rf_snb_saron():
    txt = _http_text("https://data.snb.ch/api/cube/zimoma/data/csv/en")
    last = None
    for ln in txt.splitlines():
        parts = [p.strip().strip('"') for p in ln.split(";")]
        if len(parts) >= 3 and parts[1] == "SARON":
            v = _finite(parts[2])
            if v is not None:
                last = (v / 100.0, parts[0], "SARON", "Swiss average rate overnight (SNB, monthly)")
    return last


_RF_SOURCES = {"EUR": _rf_ecb_estr, "GBP": _rf_boe_sonia, "CAD": _rf_boc_corra, "CHF": _rf_snb_saron}


def risk_free_for(ccy, closes, warnings):
    """{"rate", "currency", "symbol", "description", "as_of"} or None."""
    c = (ccy or "USD").upper()
    if c == "USD":
        irx = closes.get("^IRX")
        if irx is not None and not irx.empty:
            rate = _finite(irx.iloc[-1])
            if rate is not None and -1.0 < rate < 30.0:
                # ^IRX is a DISCOUNT yield (actual/360): bond-equivalent 365·d / (360 − 91·d).
                d = rate / 100.0
                denom = 360.0 - 91.0 * d
                bey = 365.0 * d / denom if denom > 0 else d
                return {"rate": bey, "currency": "USD", "symbol": "^IRX", "as_of": irx.index[-1].strftime("%Y-%m-%d"),
                        "description": "US 13-week T-bill, bond-equivalent yield"}
        warnings.append("risk-free rate unavailable (^IRX not returned)")
        return None
    fn = _RF_SOURCES.get(c)
    if fn is None:
        warnings.append(f"no automatic risk-free source for {c} — set one in the portfolio settings")
        return None
    try:
        got = fn()
    except Exception as e:  # network / format
        warnings.append(f"{c} risk-free rate unavailable: {type(e).__name__}")
        return None
    if not got:
        warnings.append(f"{c} risk-free rate unavailable")
        return None
    rate, as_of, sym, desc = got
    return {"rate": rate, "currency": c, "symbol": sym, "as_of": as_of, "description": desc}


# ── Market data in the base currency ─────────────────────────────────────────

def load_market(symbols, currencies, base, bench_spec, period, extra=(), want_rf_ccy=None):
    """Download holdings + benchmark legs + FX legs (+ ^IRX) in one call and return
    base-currency closes. Returns dict with keys: closes_base, local_closes, bench_level,
    bench_legs, raw, warnings, missing, ccy (symbol -> quoted ccy)."""
    warnings = []
    legs = parse_benchmark(bench_spec) or [(GLOBAL_BENCHMARK, 1.0)]
    ccy = {}
    for s in list(symbols) + [l for l, _ in legs] + list(extra):
        q = (currencies or {}).get(s) or _INDEX_CCY.get(s) or ccy_for_symbol(s)
        ccy[s] = q or base
    majors = {major_ccy(c)[0] for c in ccy.values() if c}
    tickers = list(dict.fromkeys(list(symbols) + [l for l, _ in legs] + list(extra) + fx_tickers(majors, base)
                                 + (["^IRX"] if (want_rf_ccy or "").upper() == "USD" else [])))
    data = download(tickers, period=period)
    if data is None or getattr(data, "empty", True):
        _fail("no price data returned (network down or every ticker unknown)")
    raw = closes_for(data, tickers)
    closes_base, local = {}, {}
    missing = []
    for s in list(symbols) + [l for l, _ in legs] + list(extra):
        if s in closes_base:
            continue
        c = raw.get(s)
        if c is None or len(c) < 2:
            if s in symbols:
                missing.append(s)
            continue
        major, div = major_ccy(ccy[s])
        local[s] = c / div
        b = to_base(c, ccy[s], base, raw)
        if b is None or len(b) < 2:
            warnings.append(f"no {major or '?'}→{base} FX history for {s} — left out")
            if s in symbols:
                missing.append(s)
            continue
        closes_base[s] = b
    # Benchmark level in base currency, blend rebalanced daily.
    import pandas as pd
    bench_level = None
    avail = [(l, w) for l, w in legs if l in closes_base]
    if avail:
        if len(avail) == 1:
            bench_level = closes_base[avail[0][0]]
        else:
            rets = pd.DataFrame({l: closes_base[l].pct_change(fill_method=None) for l, _ in avail}).sort_index()
            tw = sum(w for _, w in avail)
            br = sum(rets[l].fillna(0.0) * (w / tw) for l, w in avail)
            br = br[rets.notna().any(axis=1)].dropna()
            bench_level = (1.0 + br).cumprod() * 100.0
        if len(avail) < len(legs):
            warnings.append("benchmark legs without history: " + ", ".join(l for l, _ in legs if l not in closes_base))
    else:
        warnings.append(f"no history for benchmark {bench_spec}")
    return {"closes_base": closes_base, "local": local, "bench_level": bench_level, "legs": legs, "raw": raw,
            "warnings": warnings, "missing": missing, "ccy": ccy}


def returns_frame(closes_base, syms):
    import numpy as np
    import pandas as pd
    rets = {}
    for s in syms:
        c = closes_base.get(s)
        if c is None:
            continue
        r = c.pct_change(fill_method=None).dropna()
        r = r[np.isfinite(r)]
        if len(r) >= 2:
            rets[s] = r
    return rets, (pd.DataFrame(rets).sort_index() if rets else None)


def norm_inv(p):
    """Inverse standard normal CDF (Acklam), no scipy needed."""
    a = [-3.969683028665376e+01, 2.209460984245205e+02, -2.759285104469687e+02, 1.383577518672690e+02,
         -3.066479806614716e+01, 2.506628277459239e+00]
    b = [-5.447609879822406e+01, 1.615858368580409e+02, -1.556989798598866e+02, 6.680131188771972e+01,
         -1.328068155288572e+01]
    c = [-7.784894002430293e-03, -3.223964580411365e-01, -2.400758277161838e+00, -2.549732539343734e+00,
         4.374664141464968e+00, 2.938163982698783e+00]
    d = [7.784695709041462e-03, 3.224671290700398e-01, 2.445134137142996e+00, 3.754408661907416e+00]
    plow, phigh = 0.02425, 1 - 0.02425
    if p < plow:
        q = math.sqrt(-2 * math.log(p))
        return (((((c[0] * q + c[1]) * q + c[2]) * q + c[3]) * q + c[4]) * q + c[5]) / \
               ((((d[0] * q + d[1]) * q + d[2]) * q + d[3]) * q + 1)
    if p > phigh:
        q = math.sqrt(-2 * math.log(1 - p))
        return -(((((c[0] * q + c[1]) * q + c[2]) * q + c[3]) * q + c[4]) * q + c[5]) / \
               ((((d[0] * q + d[1]) * q + d[2]) * q + d[3]) * q + 1)
    q = p - 0.5
    r = q * q
    return (((((a[0] * r + a[1]) * r + a[2]) * r + a[3]) * r + a[4]) * r + a[5]) * q / \
           (((((b[0] * r + b[1]) * r + b[2]) * r + b[3]) * r + b[4]) * r + 1)


def _var_options(payload):
    v = payload.get("var") if isinstance(payload.get("var"), dict) else {}
    confs = []
    for c in v.get("confidences") or [0.95, 0.99]:
        f = _finite(c)
        if f is not None and 0.5 <= f < 1.0 and f not in confs:
            confs.append(f)
    if not confs:
        confs = [0.95]
    try:
        h = int(v.get("horizon_days") or 1)
    except (TypeError, ValueError):
        h = 1
    try:
        paths = int(v.get("mc_paths") if v.get("mc_paths") is not None else 10000)
    except (TypeError, ValueError):
        paths = 10000
    seed = v.get("seed")
    try:
        seed = int(seed) if seed is not None else None
    except (TypeError, ValueError):
        seed = None
    return confs, max(1, min(h, 60)), max(0, min(paths, 200000)), seed


def risk_block(filled, w, bench_r, ppy, confs, horizon, mc_paths, seed, cal_index=None):
    """Ex-ante risk of weights `w` (Series over filled.columns) on daily base-currency
    returns `filled` (union calendar, NaN -> 0). Portfolio-level series are compounded
    onto `cal_index` when given. Returns (portfolio dict, var list, per-holding dict)."""
    import numpy as np
    import pandas as pd

    syms = list(w.index)
    wv = w.values.astype(float)
    port_daily = filled[syms].dot(wv)
    port = compound_onto(port_daily, cal_index) if cal_index is not None and len(cal_index) else port_daily
    out = {"observations": int(len(port)), "periods_per_year": round(ppy, 2),
           "start": port.index[0].strftime("%Y-%m-%d") if len(port) else None,
           "end": port.index[-1].strftime("%Y-%m-%d") if len(port) else None,
           "ann_vol": None, "var_95_1d": None, "cvar_95_1d": None, "param_var_95_1d": None, "beta": None,
           "benchmark_corr": None, "tracking_error": None, "diversification_ratio": None}
    var_list, holdings = [], {}
    if len(port) < MIN_RISK_OBS:
        return out, var_list, holdings
    vals = port.values.astype(float)
    sd = float(np.std(vals, ddof=1))
    mu = float(np.mean(vals))
    out["ann_vol"] = _finite(sd * math.sqrt(ppy))
    q05 = float(np.quantile(vals, 0.05))
    out["var_95_1d"] = _finite(max(0.0, -q05))
    tail = vals[vals <= q05]
    out["cvar_95_1d"] = _finite(max(0.0, -float(tail.mean()))) if len(tail) else None
    out["param_var_95_1d"] = _finite(max(0.0, 1.6448536 * sd - mu))
    if bench_r is not None:
        joined = pd.concat([port.rename("p"), bench_r.rename("b")], axis=1, join="inner").dropna()
        if len(joined) >= MIN_RISK_OBS and joined["b"].var(ddof=1) > 0:
            out["beta"] = _finite(joined["p"].cov(joined["b"]) / joined["b"].var(ddof=1))
            out["benchmark_corr"] = _finite(joined["p"].corr(joined["b"]))
            out["tracking_error"] = _finite(float((joined["p"] - joined["b"]).std(ddof=1)) * math.sqrt(ppy))

    # Asset returns on the same observation grid, for covariance / MC.
    if cal_index is not None and len(cal_index):
        asset = pd.DataFrame({s: compound_onto(filled[s], cal_index) for s in syms}).reindex(port.index).fillna(0.0)
    else:
        asset = filled[syms].reindex(port.index).fillna(0.0)
    cov_d = asset.cov().values
    mu_d = asset.mean().values
    cov = cov_d * ppy
    port_var = float(wv @ cov @ wv)
    if port_var > 0:
        stand_alone = np.sqrt(np.clip(np.diag(cov), 0.0, None))
        out["diversification_ratio"] = _finite(float(wv @ stand_alone) / math.sqrt(port_var))

    # VaR at each confidence for the horizon.
    rng = np.random.default_rng(seed)
    port_h = None
    n = len(vals)
    if horizon > 1 and n - horizon + 1 >= 100:
        port_h = (1.0 + port).rolling(horizon).apply(np.prod, raw=True).dropna().values - 1.0
    asset_mat = asset.values
    mc_returns = None
    if mc_paths > 0 and len(asset_mat) >= MIN_RISK_OBS:
        # Bootstrap (historical-simulation) Monte Carlo: each path draws `horizon` whole
        # days of joint asset returns, so fat tails and co-movement are kept.
        chunks, done = [], 0
        while done < mc_paths:
            k = min(20000, mc_paths - done)
            idx = rng.integers(0, len(asset_mat), size=(k, horizon))
            growth = np.prod(1.0 + asset_mat[idx], axis=1)  # (k, n_assets)
            chunks.append((growth - 1.0) @ wv)
            done += k
        mc_returns = np.concatenate(chunks)
    sd_d = float(np.sqrt(max(float(wv @ cov_d @ wv), 0.0)))
    mu_p = float(wv @ mu_d)
    for c in confs:
        e = {"confidence": c, "horizon_days": horizon, "historical": None, "cvar": None, "parametric": None,
             "monte_carlo": None, "monte_carlo_cvar": None, "horizon_scaled": False}
        a = 1.0 - c
        if horizon == 1:
            sample = vals
            scale = 1.0
        elif port_h is not None:
            sample = port_h
            scale = 1.0
        else:
            sample = vals
            scale = math.sqrt(horizon)
            e["horizon_scaled"] = True
        q = float(np.quantile(sample, a))
        e["historical"] = _finite(max(0.0, -q * scale))
        t = sample[sample <= q]
        e["cvar"] = _finite(max(0.0, -float(t.mean()) * scale)) if len(t) else None
        z = norm_inv(c)
        e["parametric"] = _finite(max(0.0, z * sd_d * math.sqrt(horizon) - mu_p * horizon))
        if mc_returns is not None and len(mc_returns):
            mq = float(np.quantile(mc_returns, a))
            e["monte_carlo"] = _finite(max(0.0, -mq))
            mt = mc_returns[mc_returns <= mq]
            e["monte_carlo_cvar"] = _finite(max(0.0, -float(mt.mean()))) if len(mt) else None
        var_list.append(e)

    # Per holding: Euler variance share, marginal / component parametric VaR (first confidence).
    z0 = norm_inv(confs[0])
    marginal = cov_d @ wv
    for i, s in enumerate(syms):
        h = {"weight": _finite(wv[i]), "risk_contribution": None, "marginal_var": None, "component_var": None}
        if port_var > 0:
            h["risk_contribution"] = _finite(wv[i] * (cov @ wv)[i] / port_var)
        if sd_d > 0:
            m_var = z0 * math.sqrt(horizon) * marginal[i] / sd_d - mu_d[i] * horizon
            h["marginal_var"] = _finite(m_var)
            h["component_var"] = _finite(wv[i] * m_var)
        holdings[s] = h
    return out, var_list, holdings


# ── analytics ────────────────────────────────────────────────────────────────

def _clean_symbols(raw):
    out = []
    for s in raw or []:
        t = str(s).strip().upper()
        if t and t not in out:
            out.append(t)
    return out


def _weights(raw):
    w = {}
    if isinstance(raw, dict):
        for k, v in raw.items():
            fv = _finite(v)
            if fv is not None and fv > 0:
                w[str(k).strip().upper()] = fv
    return w


def analytics(payload):
    import numpy as np
    import pandas as pd

    symbols = _clean_symbols(payload.get("symbols"))
    if not symbols:
        _fail("'symbols' must be a non-empty list of tickers")
    base = (str(payload.get("base_currency") or "USD").strip().upper()) or "USD"
    bench_spec = str(payload.get("benchmark") or GLOBAL_BENCHMARK).strip() or GLOBAL_BENCHMARK
    period = str(payload.get("period") or DEFAULT_PERIOD).strip()
    if period not in ALLOWED_PERIODS:
        period = DEFAULT_PERIOD
    try:
        corr_window = int(payload.get("corr_window") or DEFAULT_CORR_WINDOW)
    except (TypeError, ValueError):
        corr_window = DEFAULT_CORR_WINDOW
    corr_window = max(MIN_CORR_OBS, min(corr_window, 504))
    weights = _weights(payload.get("weights"))
    currencies = {str(k).strip().upper(): str(v) for k, v in (payload.get("currencies") or {}).items()}
    confs, horizon, mc_paths, seed = _var_options(payload)
    rf_ccy = str(payload.get("risk_free_currency") or base).strip().upper()

    try:
        mk = load_market(symbols, currencies, base, bench_spec, period, want_rf_ccy=rf_ccy)
    except SystemExit:
        raise
    except Exception as e:
        _fail(f"price download failed: {e}")
    warnings = mk["warnings"]
    closes_base = mk["closes_base"]

    as_of_ts = max((s.index[-1] for s in closes_base.values()), default=None)
    result = {"as_of": as_of_ts.strftime("%Y-%m-%d") if as_of_ts is not None else None, "period": period,
              "base_currency": base, "warnings": warnings}

    bench_level = mk["bench_level"]
    legs = mk["legs"]
    result["benchmark"] = {
        "symbol": bench_spec,
        "legs": [{"symbol": l, "weight": w} for l, w in legs],
        "dates": [d.strftime("%Y-%m-%d") for d in bench_level.index] if bench_level is not None else [],
        "closes": [round(float(v), 8) for v in bench_level.values] if bench_level is not None else [],
    }

    rf = risk_free_for(rf_ccy, mk["raw"], warnings)
    if rf:
        result["risk_free"] = rf

    missing = [s for s in symbols if s in mk["missing"] or s not in closes_base]
    result["missing"] = missing
    if missing:
        warnings.append("no price history for: " + ", ".join(missing))

    rets, rets_df = returns_frame(closes_base, symbols)
    priced = [s for s in symbols if s in rets]
    if not priced:
        result["correlation"] = {"window": corr_window, "matrix": {}}
        result["holdings"] = {}
        result["portfolio"] = None
        result["var"] = []
        return result

    # Correlation over the trailing window, union calendar (pairwise complete).
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
    result["correlation"] = {"window": int(len(tail)),
                             "start": tail.index[0].strftime("%Y-%m-%d") if len(tail) else None,
                             "end": tail.index[-1].strftime("%Y-%m-%d") if len(tail) else None, "matrix": matrix}

    # Weights over the priced lines.
    w = pd.Series({s: weights.get(s, 0.0) for s in priced}, dtype=float)
    total_w_all = sum(weights.get(s, 0.0) for s in symbols)
    if w.sum() <= 0:
        w = pd.Series(1.0 / len(priced), index=priced)
        coverage = 1.0
        warnings.append("no weights supplied — equal weights assumed")
    else:
        coverage = float(w.sum() / total_w_all) if total_w_all > 0 else 1.0
        w = w / w.sum()
    w = w[w > 0]
    if w.empty:
        result["portfolio"] = None
        result["holdings"] = {}
        result["var"] = []
        return result
    syms = list(w.index)

    risk_df = rets_df[rets_df.index > rets_df.index[-1] - pd.Timedelta(days=RISK_WINDOW_DAYS)]
    # Start where every material holding has history — a younger listing's missing
    # months are not treated as zero-risk days, and nothing is back-filled.
    material = [s for s in syms if w[s] >= 0.01]
    start = max(rets[s].index[0] for s in material) if material else risk_df.index[0]
    window_df = risk_df[risk_df.index >= start]
    if len(window_df) < MIN_RISK_OBS:
        warnings.append(f"only {len(window_df)} days of shared history since the youngest material listing — "
                        "ex-ante risk not estimated")
        window_df = window_df.iloc[0:0]
    filled = window_df[syms].fillna(0.0)  # a closed market is a zero-return day for that line

    # Observation calendar = the benchmark's trading days; ppy from the payload.
    cal = None
    if bench_level is not None and len(bench_level) > 1:
        cal = bench_level.index[bench_level.index >= (filled.index[0] if len(filled) else bench_level.index[0])]
    ppy = _finite(payload.get("periods_per_year")) or (calendar_periods_per_year(bench_level.index)
                                                       if bench_level is not None else 252.0)
    bench_r = bench_level.pct_change(fill_method=None).dropna() if bench_level is not None else None

    if len(filled) >= MIN_RISK_OBS:
        port_out, var_list, hold = risk_block(filled, w, bench_r, ppy, confs, horizon, mc_paths, seed, cal)
    else:
        port_out, var_list, hold = {"observations": int(len(filled)), "periods_per_year": ppy}, [], {}
    port_out["coverage"] = round(coverage, 6)

    # Currency risk: the same book's variance with local-currency returns.
    port_out["fx_vol_contribution"] = None
    if len(filled) >= MIN_RISK_OBS:
        loc = {}
        for s in syms:
            lc = mk["local"].get(s)
            if lc is not None:
                lr = lc.pct_change(fill_method=None).dropna()
                loc[s] = lr
        if len(loc) == len(syms):
            ldf = pd.DataFrame(loc).reindex(filled.index).fillna(0.0)
            lp = ldf.dot(w.values)
            bp = filled.dot(w.values)
            vb, vl = float(bp.var(ddof=1)), float(lp.var(ddof=1))
            if vb > 0:
                port_out["fx_vol_contribution"] = _finite(1.0 - vl / vb)

    holdings_out = {}
    for s in syms:
        own = rets[s]
        own_win = own[own.index >= (filled.index[0] if len(filled) else own.index[0])]
        own_ppy = calendar_periods_per_year(own_win.index)
        h = hold.get(s, {"weight": _finite(w[s])})
        h["observations"] = int(len(own_win))
        h["ann_vol"] = _finite(own_win.std(ddof=1) * math.sqrt(own_ppy)) if len(own_win) >= 2 else None
        h["beta"] = None
        if bench_r is not None:
            jb = pd.concat([own_win.rename("s"), bench_r.rename("b")], axis=1, join="inner").dropna()
            if len(jb) >= MIN_RISK_OBS and jb["b"].var(ddof=1) > 0:
                h["beta"] = _finite(jb["s"].cov(jb["b"]) / jb["b"].var(ddof=1))
        maj = major_ccy(mk["ccy"].get(s, base))[0] or base
        if maj == base:
            h["fx_vol"] = 0.0
        else:
            fx = fx_series(mk["raw"], maj, base)
            if fx is not None and len(fx) > 30:
                fr = fx.pct_change(fill_method=None).dropna().tail(260)
                h["fx_vol"] = _finite(float(fr.std(ddof=1)) * math.sqrt(calendar_periods_per_year(fr.index)))
            else:
                h["fx_vol"] = None
        holdings_out[s] = h

    result["portfolio"] = port_out
    result["var"] = var_list
    result["var_confidence"] = confs[0]
    result["var_horizon_days"] = horizon
    result["mc_paths"] = mc_paths if var_list else 0
    result["holdings"] = holdings_out
    return result


# ── what_if ──────────────────────────────────────────────────────────────────

def what_if(payload):
    import pandas as pd

    symbols = _clean_symbols(payload.get("symbols"))
    if not symbols:
        _fail("'symbols' must be a non-empty list of tickers")
    base = (str(payload.get("base_currency") or "USD").strip().upper()) or "USD"
    bench_spec = str(payload.get("benchmark") or GLOBAL_BENCHMARK).strip() or GLOBAL_BENCHMARK
    before = _weights(payload.get("weights"))
    after = _weights(payload.get("weights_after"))
    if not after:
        _fail("'weights_after' must map at least one symbol to a weight > 0")
    currencies = {str(k).strip().upper(): str(v) for k, v in (payload.get("currencies") or {}).items()}
    confs, horizon, _mc, seed = _var_options(payload)
    mk = load_market(symbols, currencies, base, bench_spec, "1y")
    warnings = mk["warnings"]
    rets, rets_df = returns_frame(mk["closes_base"], symbols)
    if rets_df is None:
        _fail("no price history for any symbol")
    bench_level = mk["bench_level"]
    bench_r = bench_level.pct_change(fill_method=None).dropna() if bench_level is not None else None
    ppy = _finite(payload.get("periods_per_year")) or 252.0

    def evaluate(wmap, label):
        w = pd.Series({s: v for s, v in wmap.items() if s in rets}, dtype=float)
        dropped = [s for s in wmap if s not in rets]
        if dropped:
            warnings.append(f"{label}: no price history for " + ", ".join(dropped) + " — left out")
        if w.sum() <= 0:
            return None
        cov_share = float(w.sum() / sum(wmap.values()))
        w = w / w.sum()
        syms = list(w.index)
        start = max(rets[s].index[0] for s in syms if w[s] >= 0.01) if any(w >= 0.01) else rets_df.index[0]
        win = rets_df[rets_df.index >= start][syms].fillna(0.0)
        if len(win) < MIN_RISK_OBS:
            warnings.append(f"{label}: only {len(win)} days of shared history")
            return None
        cal = bench_level.index[bench_level.index >= win.index[0]] if bench_level is not None else None
        p, v, h = risk_block(win, w, bench_r, ppy, confs, horizon, 0, seed, cal)
        first = v[0] if v else {}
        return {"ann_vol": p.get("ann_vol"), "beta": p.get("beta"), "tracking_error": p.get("tracking_error"),
                "diversification_ratio": p.get("diversification_ratio"), "var": first.get("historical"),
                "cvar": first.get("cvar"), "parametric_var": first.get("parametric"),
                "var_confidence": confs[0], "var_horizon_days": horizon, "observations": p.get("observations"),
                "coverage": cov_share, "risk_contribution": {s: h[s]["risk_contribution"] for s in h}}

    b = evaluate(before, "before") if before else None
    a = evaluate(after, "after")
    delta = {}
    if a and b:
        for k in ("ann_vol", "beta", "tracking_error", "var", "cvar", "diversification_ratio"):
            if a.get(k) is not None and b.get(k) is not None:
                delta[k] = a[k] - b[k]
    nav = _finite(payload.get("nav"))
    return {"before": b, "after": a, "delta": delta, "weights": {"before": before, "after": after},
            "nav": nav, "base_currency": base, "benchmark": bench_spec, "warnings": warnings}


# ── historical_stress ────────────────────────────────────────────────────────

def historical_stress(payload):
    import datetime as _dt
    import pandas as pd

    symbols = _clean_symbols(payload.get("symbols"))
    weights = _weights(payload.get("weights"))
    if not symbols or not weights:
        _fail("'symbols' and 'weights' are required")
    base = (str(payload.get("base_currency") or "USD").strip().upper()) or "USD"
    bench_spec = str(payload.get("benchmark") or GLOBAL_BENCHMARK).strip() or GLOBAL_BENCHMARK
    try:
        start = _dt.date.fromisoformat(str(payload.get("start"))[:10])
        end = _dt.date.fromisoformat(str(payload.get("end"))[:10])
    except Exception:
        _fail("'start' and 'end' must be YYYY-MM-DD")
    if end <= start:
        _fail("'end' must be after 'start'")
    currencies = {str(k).strip().upper(): str(v) for k, v in (payload.get("currencies") or {}).items()}
    legs = parse_benchmark(bench_spec) or [(GLOBAL_BENCHMARK, 1.0)]
    ccy = {s: (currencies.get(s) or _INDEX_CCY.get(s) or ccy_for_symbol(s) or base) for s in symbols + [l for l, _ in legs]}
    majors = {major_ccy(c)[0] for c in ccy.values()}
    tickers = list(dict.fromkeys(symbols + [l for l, _ in legs] + fx_tickers(majors, base)))
    data = download(tickers, start=(start - _dt.timedelta(days=10)).isoformat(),
                    end=(end + _dt.timedelta(days=1)).isoformat())
    if data is None or getattr(data, "empty", True):
        _fail("no price data for that window")
    raw = closes_for(data, tickers)  # full download list: `single` inferred correctly
    t0, t1 = pd.Timestamp(start), pd.Timestamp(end)
    warnings, per, missing = [], {}, []
    paths = {}
    for s in symbols + [l for l, _ in legs]:
        c = raw.get(s)
        b = to_base(c, ccy[s], base, raw) if c is not None else None
        if b is None:
            if s in symbols:
                missing.append(s)
            continue
        before = b[b.index <= t0]
        inside = b[(b.index > t0) & (b.index <= t1)]
        if before.empty or inside.empty:
            if s in symbols:
                missing.append(s)
            continue
        p0 = float(before.iloc[-1])
        paths[s] = pd.concat([before.tail(1), inside]) / p0
    tw = sum(weights.get(s, 0.0) for s in symbols)
    covered = [s for s in symbols if s in paths and weights.get(s, 0.0) > 0]
    cov_w = sum(weights[s] for s in covered)
    out = {"start": start.isoformat(), "end": end.isoformat(), "base_currency": base, "missing": missing,
           "coverage": (cov_w / tw) if tw > 0 else 0.0, "holdings": {}, "warnings": warnings}
    if not covered:
        out["warnings"].append("no holding has prices for that window")
        out["return"] = None
        return out
    for s in covered:
        wn = weights[s] / cov_w
        r = float(paths[s].iloc[-1] - 1.0)
        out["holdings"][s] = {"return": r, "contribution": wn * r, "weight": wn}
    # Buy-and-hold path of the covered book (each line from its window-start price).
    book = pd.DataFrame({s: paths[s] for s in covered}).sort_index().ffill().dropna()
    level = (book * pd.Series({s: weights[s] / cov_w for s in covered})).sum(axis=1)
    out["return"] = float(level.iloc[-1] - 1.0)
    out["max_drawdown"] = float((level / level.cummax() - 1.0).min())
    out["path"] = {"dates": [d.strftime("%Y-%m-%d") for d in level.index], "level": [float(v) for v in level.values]}
    nav = _finite(payload.get("nav"))
    if nav is not None:
        out["pnl"] = nav * (cov_w / tw if tw > 0 else 0.0) * out["return"]
    bl = [(l, wt) for l, wt in legs if l in paths]
    if bl:
        tot = sum(wt for _, wt in bl)
        out["benchmark_return"] = float(sum(float(paths[l].iloc[-1] - 1.0) * wt / tot for l, wt in bl))
    if missing:
        warnings.append("not priced over the window (left out, coverage < 1): " + ", ".join(missing))
    return out


# ── nav_replay ───────────────────────────────────────────────────────────────

_TYPE_RANK = {"SPLIT": 0, "DEPOSIT": 1}


def nav_replay(payload):
    """Daily NAV in the base currency from the ledger.

    Prices: split-adjusted closes multiplied back by every later split factor, i.e.
    the RAW price traded that day (no dividend adjustment), so they match the trade
    prices and quantities in the ledger; a split moves price and quantity together.
    Dividends are the ledger's own DIVIDEND rows (income, or cash when track_cash).
    Splits in Yahoo's data that the ledger does not record (no SPLIT row within 5
    days) are applied to the replayed quantity and reported in "warnings".
    A day on which a held position (or a cash balance) has no price / FX is NOT
    output: a partial NAV would fake a return.
    """
    import datetime as _dt
    import pandas as pd

    txns = [t for t in (payload.get("transactions") or []) if isinstance(t, dict)]
    base = (str(payload.get("base_currency") or "USD").strip().upper()) or "USD"
    track_cash = bool(payload.get("track_cash"))
    instr = {str(k).strip().upper(): str(v) for k, v in (payload.get("instrument_currencies") or {}).items()}
    rows = []
    for i, t in enumerate(txns):
        typ = str(t.get("type", "")).upper()
        d = str(t.get("date", ""))[:10]
        try:
            _dt.date.fromisoformat(d)
        except Exception:
            continue
        q, p = _finite(t.get("quantity")), _finite(t.get("price"))
        if q is None or p is None:
            continue
        rows.append({"i": i, "date": d, "type": typ, "symbol": str(t.get("symbol") or "").strip().upper(),
                     "q": q, "p": p, "fees": max(0.0, _finite(t.get("fees")) or 0.0),
                     "taxes": max(0.0, _finite(t.get("taxes")) or 0.0), "ccy": str(t.get("currency") or "").strip(),
                     "fx": _finite(t.get("fx_rate")) or 0.0, "created": str(t.get("created_at") or "")})
    if not rows:
        return {"dates": [], "navs": [], "costs": [], "warnings": ["no usable transactions"]}
    # Repository order is newest first: oldest first, entry order inside a day.
    rows.sort(key=lambda r: (r["date"], _TYPE_RANK.get(r["type"], 2), r["created"], -r["i"]))

    def row_ccy(r):
        if r["ccy"]:
            return r["ccy"]
        if r["type"] in ("DEPOSIT", "WITHDRAWAL", "FEE", "TAX", "INTEREST") or not r["symbol"]:
            return base
        return instr.get(r["symbol"]) or ccy_for_symbol(r["symbol"]) or base

    trade_syms = sorted({r["symbol"] for r in rows if r["type"] in ("BUY", "SELL") and r["symbol"]})
    sym_ccy = {s: instr.get(s) or ccy_for_symbol(s) or base for s in trade_syms}
    cash_ccys = {major_ccy(row_ccy(r))[0] for r in rows} if track_cash else set()
    majors = {major_ccy(c)[0] for c in sym_ccy.values()} | cash_ccys | {major_ccy(row_ccy(r))[0] for r in rows}
    first = _dt.date.fromisoformat(rows[0]["date"])
    today = _dt.date.today()
    if first > today:
        return {"dates": [], "navs": [], "costs": [], "error": "earliest transaction date is in the future"}
    tickers = list(dict.fromkeys(trade_syms + fx_tickers(majors, base)))
    data = download(tickers, start=(first - _dt.timedelta(days=14)).isoformat(),
                    end=(today + _dt.timedelta(days=1)).isoformat(), auto_adjust=False, actions=True)
    if data is None or getattr(data, "empty", True):
        return {"dates": [], "navs": [], "costs": [], "error": "no price data returned"}
    single = len(tickers) == 1
    warnings = []
    closes, splits = {}, {}
    for s in trade_syms:
        c = column(data, s, "Close", single)
        if c is None:
            continue
        sp = column(data, s, "Stock Splits", single, positive=True)
        sp = sp[(sp > 0) & (sp != 1.0)] if sp is not None else None
        if sp is not None and len(sp):
            # Raw price = split-adjusted close × product of every LATER split factor.
            factor = pd.Series(1.0, index=c.index)
            for d, f in sp.items():
                factor[c.index < d] *= float(f)
            c = c * factor
            splits[s] = [(d, float(f)) for d, f in sp.items()]
        closes[s] = c
    fx_closes = closes_for(data, fx_tickers(majors, base), single=single)
    missing = [s for s in trade_syms if s not in closes]

    # Ledger splits, to tell which of Yahoo's splits are not recorded.
    recorded = {}
    for r in rows:
        if r["type"] == "SPLIT":
            recorded.setdefault(r["symbol"], []).append(_dt.date.fromisoformat(r["date"]))
    auto_splits = {}
    for s, evs in splits.items():
        for d, f in evs:
            dd = d.date()
            if dd < first:
                continue
            if any(abs((dd - x).days) <= 5 for x in recorded.get(s, [])):
                continue
            auto_splits.setdefault(d.strftime("%Y-%m-%d"), []).append((s, f))
            warnings.append(f"{s}: {f:g}-for-1 split on {dd.isoformat()} is in the market data but not in the "
                            "ledger — applied to the replayed quantity; record it so the live position matches")

    # Calendar = union of every traded instrument's dates from the first trade.
    idx = None
    for c in closes.values():
        idx = c.index if idx is None else idx.union(c.index)
    if idx is None:
        return {"dates": [], "navs": [], "costs": [], "missing": missing,
                "warnings": warnings + ["no price history for any traded symbol"]}
    idx = idx[(idx >= pd.Timestamp(first)) & (idx <= pd.Timestamp(today))]
    px = pd.DataFrame({s: c for s, c in closes.items()}).reindex(idx.union(
        pd.DatetimeIndex(sorted({d for c in closes.values() for d in c.index})))).sort_index().ffill(limit=7)
    px = px.reindex(idx)
    fx_cache = {}

    def fx_on(ccy_major, ts):
        if ccy_major == base:
            return 1.0
        s = fx_cache.get(ccy_major)
        if s is None:
            s = fx_series(fx_closes, ccy_major, base, idx)
            fx_cache[ccy_major] = s if s is not None else False
        if s is False or s is None:
            return None
        v = s.get(ts)
        return float(v) if v is not None and math.isfinite(v) else None

    qty, cost_base, cash = {}, {}, {}
    out_d, out_nav, out_cost = [], [], []
    skipped, ri = 0, 0
    missing_fx = set()
    for ts in idx:
        day = ts.strftime("%Y-%m-%d")
        held_at_open = any(q > 1e-12 for q in qty.values()) or (
            track_cash and any(abs(b) > 1e-9 for b in cash.values()))
        for s, f in auto_splits.get(day, []):
            if qty.get(s, 0) > 0:
                qty[s] *= f
        while ri < len(rows) and rows[ri]["date"] <= day:
            r = rows[ri]
            ri += 1
            typ, s = r["type"], r["symbol"]
            quoted = row_ccy(r)
            major, div = major_ccy(quoted)
            major = major or base
            fx = r["fx"] if r["fx"] > 0 else fx_on(major, ts)
            gross = r["q"] * r["p"] / div
            fees, taxes = r["fees"] / div, r["taxes"] / div
            if typ == "BUY":
                held = qty.get(s, 0.0)
                qty[s] = held + r["q"]
                cb = cost_base.get(s, 0.0)
                cost_base[s] = cb + (gross + fees + taxes) * (fx if fx else float("nan"))
                cash[major] = cash.get(major, 0.0) - (gross + fees + taxes)
            elif typ == "SELL":
                held = qty.get(s, 0.0)
                sold = min(r["q"], held)
                if held > 0:
                    cost_base[s] = cost_base.get(s, 0.0) * (1.0 - sold / held)
                qty[s] = held - sold
                cash[major] = cash.get(major, 0.0) + (gross - fees - taxes)
            elif typ == "SPLIT":
                if r["q"] > 0 and qty.get(s, 0) > 0:
                    qty[s] *= r["q"]
            elif typ == "DIVIDEND":
                cash[major] = cash.get(major, 0.0) + gross - taxes - fees
            elif typ == "DEPOSIT":
                cash[major] = cash.get(major, 0.0) + abs(gross)
            elif typ == "WITHDRAWAL":
                cash[major] = cash.get(major, 0.0) - abs(gross)
            elif typ in ("FEE", "TAX"):
                cash[major] = cash.get(major, 0.0) - abs(gross)
            elif typ == "INTEREST":
                cash[major] = cash.get(major, 0.0) + abs(gross) - taxes
        nav, ok = 0.0, True
        holds_cash = track_cash and any(abs(b) > 1e-9 for b in cash.values())
        if not held_at_open and not holds_cash and not any(q > 1e-12 for q in qty.values()):
            continue  # nothing in the book yet: no NAV point (a book emptied TODAY still gets its 0)
        for s, q in qty.items():
            if q <= 1e-12:
                continue
            p = px[s].get(ts) if s in px.columns else None
            if p is None or not math.isfinite(p):
                ok = False
                break
            major, div = major_ccy(sym_ccy.get(s, base))
            fx = fx_on(major or base, ts)
            if fx is None:
                missing_fx.add(major)
                ok = False
                break
            nav += q * float(p) / div * fx
        cost = sum(v for s, v in cost_base.items() if qty.get(s, 0) > 1e-12)
        if ok and track_cash:
            for c, bal in cash.items():
                if abs(bal) < 1e-9:
                    continue
                fx = fx_on(c, ts)
                if fx is None:
                    missing_fx.add(c)
                    ok = False
                    break
                nav += bal * fx
                cost += bal * fx
        if not ok:
            skipped += 1
            continue
        out_d.append(day)
        out_nav.append(round(nav, 6))
        out_cost.append(round(cost, 6) if math.isfinite(cost) else None)
    if skipped:
        warnings.append(f"{skipped} day(s) not written: a held position or cash balance had no price / FX rate")
    if missing:
        warnings.append("no price history for: " + ", ".join(missing))
    if missing_fx:
        warnings.append("no FX history for: " + ", ".join(sorted(missing_fx)))
    return {"dates": out_d, "navs": out_nav, "costs": out_cost, "missing": missing, "warnings": warnings,
            "base_currency": base,
            "splits_detected": [{"date": d, "symbol": s, "factor": f} for d, lst in auto_splits.items()
                                for s, f in lst]}


# ── CLI ──────────────────────────────────────────────────────────────────────

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


COMMANDS = {"analytics": analytics, "nav_replay": nav_replay, "what_if": what_if,
            "historical_stress": historical_stress}


def main(argv):
    if len(argv) < 2:
        _fail("usage: portfolio_metrics.py <" + "|".join(COMMANDS) + "> [<json>]  (payload on stdin)")
    command = argv[1].strip().lower()
    fn = COMMANDS.get(command)
    if fn is None:
        _fail(f"unknown command '{command}' (expected one of: {', '.join(COMMANDS)})")
    payload = _read_payload(argv)
    try:
        result = fn(payload)
    except SystemExit:
        raise
    except Exception as e:  # pandas / numpy edge cases must still come back as JSON
        _fail(f"{command} failed: {type(e).__name__}: {e}")
    fincept_json.emit(result)


if __name__ == "__main__":
    main(sys.argv)
