"""Fund / liquid-alternative return analytics against a benchmark (monthly).

CLI (the terminal):  python fund_analytics.py analyze --stdin      (input JSON object on stdin)
CLI (by hand):       python fund_analytics.py analyze '<json>'
Output: {"success": true, "data": {...}} or {"success": false, "error": "..."}. Rates are fractions.

analyze input
  source      "series" (returns you supply) | "ticker" (monthly adjusted closes from Yahoo Finance)
  series      [{"date": "yyyy-mm-dd" (optional), "value": number}] - one row per MONTH
  unit        "percent" (1.5 = +1.5 %) | "decimal" (0.015); default percent
  ticker      Yahoo symbol (source = ticker)
  period      3y | 5y | 10y | max (source = ticker; default 5y)
  benchmark   Yahoo symbol (optional; Yahoo is called only when it is set)
  benchmark_series  [{"date"?, "value"}] (optional: a benchmark you supply, same unit; matched by
              month when both have dates, else by position from the end)
  rf_annual   annual risk-free rate (fraction, default 0)
  unsmooth    true -> also report first-order unsmoothed statistics (Geltner 1993)
  window      rolling window in months (default 12)

Methods (monthly data, annualised):
  CAGR, volatility (sample stdev x sqrt 12), Sharpe and Sortino (monthly rf from the annual rate; downside
  deviation below rf over all months), maximum drawdown - identical to cli.py's _monthly_return_stats.
  Beta = cov(f, b) / var(b) on excess returns; Jensen's alpha = 12 x (mean excess f - beta x mean excess b);
  correlation, R^2, tracking error (stdev of f - b, x sqrt 12), information ratio (12 x mean(f - b) / TE).
  Up / down capture: geometric mean return of the fund over the months the benchmark rose (fell) divided by
  the benchmark's geometric mean over the same months (Morningstar's capture-ratio method).
  VaR / CVaR 95 %: historical, monthly, reported as losses (positive numbers).
  Unsmoothing: Geltner (1993) first order, r*_t = (r_t - rho r_(t-1)) / (1 - rho), rho = lag-1
  autocorrelation of the series (or `rho` when given).
"""

import json
import math
import os
import sys
from datetime import date, datetime

_HERE = os.path.dirname(os.path.abspath(__file__))
_SCRIPTS_DIR = os.path.abspath(os.path.join(_HERE, "..", ".."))
for _p in (_HERE, _SCRIPTS_DIR):
    if _p not in sys.path:
        sys.path.append(_p)

try:
    import fincept_json
except ImportError:  # pragma: no cover
    fincept_json = None

from private_markets import InputError, quiet_stdout  # noqa: E402

MIN_MONTHS = 6


# ── Statistics ───────────────────────────────────────────────────────────────

def monthly_stats(returns, rf_annual=0.0):
    """Same definitions as cli.py _monthly_return_stats (kept in step by tests)."""
    n = len(returns)
    growth, peak, max_dd = 1.0, 1.0, 0.0
    for r in returns:
        growth *= (1.0 + r)
        peak = max(peak, growth)
        max_dd = max(max_dd, 1.0 - growth / peak)
    years = n / 12.0
    cagr = growth ** (1.0 / years) - 1.0 if growth > 0 and years > 0 else None
    mean = sum(returns) / n
    vol_m = math.sqrt(sum((r - mean) ** 2 for r in returns) / (n - 1)) if n > 1 else 0.0
    rf_m = (1.0 + rf_annual) ** (1.0 / 12.0) - 1.0
    mean_excess = sum(r - rf_m for r in returns) / n
    downside = math.sqrt(sum(min(0.0, r - rf_m) ** 2 for r in returns) / n)
    return {
        "months": n,
        "cumulative_return": growth - 1.0,
        "cagr": cagr,
        "annualized_volatility": vol_m * math.sqrt(12.0),
        "sharpe_ratio": (mean_excess / vol_m) * math.sqrt(12.0) if vol_m > 0 else None,
        "sortino_ratio": (mean_excess / downside) * math.sqrt(12.0) if downside > 0 else None,
        "max_drawdown": max_dd,
        "best_month_return": max(returns),
        "worst_month_return": min(returns),
        "positive_months_rate": sum(1 for r in returns if r > 0) / n,
    }


def _mean(x):
    return sum(x) / len(x)


def _cov(a, b):
    ma, mb = _mean(a), _mean(b)
    return sum((x - ma) * (y - mb) for x, y in zip(a, b)) / (len(a) - 1)


def _geo_mean(rs):
    g = 1.0
    for r in rs:
        g *= (1.0 + r)
    return g ** (1.0 / len(rs)) - 1.0 if g > 0 else None


def relative_stats(f, b, rf_annual=0.0):
    """Beta, alpha, correlation, capture ratios, tracking error of fund f vs benchmark b (aligned)."""
    n = len(f)
    rf_m = (1.0 + rf_annual) ** (1.0 / 12.0) - 1.0
    fe = [x - rf_m for x in f]
    be = [x - rf_m for x in b]
    var_b = _cov(be, be)
    out = {"months": n}
    if var_b > 0:
        beta = _cov(fe, be) / var_b
        out["beta"] = beta
        out["alpha_annual"] = 12.0 * (_mean(fe) - beta * _mean(be))
        var_f = _cov(fe, fe)
        corr = _cov(fe, be) / math.sqrt(var_f * var_b) if var_f > 0 else None
        out["correlation"] = corr
        out["r_squared"] = corr * corr if corr is not None else None
    diff = [x - y for x, y in zip(f, b)]
    te_m = math.sqrt(_cov(diff, diff)) if n > 1 else 0.0
    out["tracking_error"] = te_m * math.sqrt(12.0)
    out["information_ratio"] = (12.0 * _mean(diff)) / out["tracking_error"] if te_m > 0 else None
    up = [(x, y) for x, y in zip(f, b) if y > 0]
    down = [(x, y) for x, y in zip(f, b) if y < 0]
    if up:
        gf, gb = _geo_mean([x for x, _ in up]), _geo_mean([y for _, y in up])
        out["up_capture"] = gf / gb if (gf is not None and gb) else None
        out["up_months"] = len(up)
    if down:
        gf, gb = _geo_mean([x for x, _ in down]), _geo_mean([y for _, y in down])
        out["down_capture"] = gf / gb if (gf is not None and gb) else None
        out["down_months"] = len(down)
    return out


def var_cvar(returns, level=0.95):
    """Historical monthly VaR / CVaR at `level`, as positive losses (None when there is no loss tail)."""
    s = sorted(returns)
    n = len(s)
    k = max(1, int(math.floor((1.0 - level) * n)))
    tail = s[:k]
    var = -s[k - 1]
    cvar = -sum(tail) / len(tail)
    return var, cvar


def lag1_autocorrelation(r):
    m = _mean(r)
    num = sum((r[i] - m) * (r[i - 1] - m) for i in range(1, len(r)))
    den = sum((x - m) ** 2 for x in r)
    return num / den if den > 0 else 0.0


def unsmooth(r, rho=None):
    """Geltner first-order unsmoothing. Returns (series, rho). The first month has no predecessor and is dropped."""
    if rho is None:
        rho = lag1_autocorrelation(r)
    if not (-0.99 < rho < 0.99):
        raise InputError("The series is too autocorrelated (rho = %.2f) to unsmooth." % rho)
    return [(r[i] - rho * r[i - 1]) / (1.0 - rho) for i in range(1, len(r))], rho


def rolling(returns, window, rf_annual=0.0):
    out = []
    for i in range(window, len(returns) + 1):
        w = returns[i - window:i]
        s = monthly_stats(w, rf_annual)
        out.append({"index": i - 1, "return": s["cumulative_return"], "volatility": s["annualized_volatility"],
                    "sharpe": s["sharpe_ratio"]})
    return out


def drawdowns(returns):
    g, peak, out = 1.0, 1.0, []
    for r in returns:
        g *= (1.0 + r)
        peak = max(peak, g)
        out.append(g / peak - 1.0)
    return out


def growth_of_100(returns):
    g, out = 100.0, []
    for r in returns:
        g *= (1.0 + r)
        out.append(g)
    return out


# ── Inputs ───────────────────────────────────────────────────────────────────

def _parse_date(s):
    s = str(s or "").strip()[:10]
    if not s:
        return None
    try:
        return datetime.strptime(s, "%Y-%m-%d").date()
    except ValueError:
        return None


def parse_series(rows, unit):
    """[(month date | None, decimal return)], number of rows dropped."""
    scale = 0.01 if unit != "decimal" else 1.0
    out, ignored = [], 0
    for r in rows or []:
        if not isinstance(r, dict):
            ignored += 1
            continue
        try:
            v = float(r.get("value"))
        except (TypeError, ValueError):
            ignored += 1
            continue
        if not math.isfinite(v) or v * scale <= -1.0:
            ignored += 1
            continue
        d = _parse_date(r.get("date"))
        out.append((date(d.year, d.month, 1) if d else None, v * scale))
    dated = [x for x in out if x[0] is not None]
    if dated and len(dated) == len(out):
        out.sort(key=lambda x: x[0])
    return out, ignored


def fetch_monthly(symbol, period):
    """Monthly total returns (adjusted close) from Yahoo Finance: [(month date, return)]."""
    try:
        import yfinance as yf
    except ImportError:
        raise InputError("The yfinance package is missing, so prices cannot be loaded.")
    try:
        with quiet_stdout():  # yfinance prints failures to stdout, which carries only the JSON reply
            hist = yf.Ticker(symbol).history(period=period, interval="1mo", auto_adjust=True)
    except Exception as e:
        raise InputError("Could not load prices for %s (%s)." % (symbol, str(e).splitlines()[0][:120] if str(e) else
                                                                 type(e).__name__))
    if hist is None or len(hist) < 3:
        raise InputError("Yahoo Finance has no monthly price history for %s." % symbol)
    closes = []
    for ts, c in zip(hist.index, hist["Close"]):
        try:
            c = float(c)
        except (TypeError, ValueError):
            continue
        if math.isfinite(c) and c > 0:
            closes.append((date(ts.year, ts.month, 1), c))
    # One close per month (the last one Yahoo gives for the month).
    by_month = {}
    for d, c in closes:
        by_month[d] = c
    months = sorted(by_month)
    today = date.today()
    if months and months[-1] == date(today.year, today.month, 1):
        months = months[:-1]  # the running month is not a monthly return yet
    out = []
    for i in range(1, len(months)):
        out.append((months[i], by_month[months[i]] / by_month[months[i - 1]] - 1.0))
    if len(out) < 2:
        raise InputError("Not enough monthly history for %s." % symbol)
    return out


def _align(f, b):
    """Match by month when both are dated, else by position from the end."""
    if f and b and all(d is not None for d, _ in f) and all(d is not None for d, _ in b):
        bm = {d: r for d, r in b}
        pairs = [(d, r, bm[d]) for d, r in f if d in bm]
        return pairs
    n = min(len(f), len(b))
    fs, bs = f[len(f) - n:], b[len(b) - n:]
    return [(fd, fr, br) for (fd, fr), (_, br) in zip(fs, bs)]


# ── Command ──────────────────────────────────────────────────────────────────

def analyze(inp):
    source = (inp.get("source") or "series").lower()
    rf = float(inp.get("rf_annual") or 0.0)
    window = max(3, min(int(inp.get("window") or 12), 60))
    unit = (inp.get("unit") or "percent").lower()
    notes = []
    ignored = 0
    label = "Your series"
    if source == "ticker":
        sym = str(inp.get("ticker") or "").strip().upper()
        if not sym:
            raise InputError("Enter the fund's or ETF's ticker.")
        period = str(inp.get("period") or "5y")
        if period not in ("1y", "2y", "3y", "5y", "10y", "max"):
            period = "5y"
        series = fetch_monthly(sym, period)
        label = sym
        notes.append("Monthly total returns of %s from Yahoo Finance adjusted closes (dividends reinvested)." % sym)
    else:
        series, ignored = parse_series(inp.get("series"), unit)
        if ignored:
            notes.append("%d row(s) were not a valid monthly return and were left out." % ignored)
    if len(series) < MIN_MONTHS:
        raise InputError("Enter at least %d monthly returns (one per month)." % MIN_MONTHS)

    bench_label, bench = None, None
    bsym = str(inp.get("benchmark") or "").strip().upper()
    if inp.get("benchmark_series"):
        bench, _ = parse_series(inp.get("benchmark_series"), unit)
        bench_label = "Your benchmark"
    elif bsym:
        period = "max" if source != "ticker" else str(inp.get("period") or "5y")
        try:
            bench = fetch_monthly(bsym, period if period in ("1y", "2y", "3y", "5y", "10y", "max") else "5y")
            bench_label = bsym
        except InputError as e:
            notes.append("%s Benchmark figures not shown; the fund's own figures are unaffected." % str(e))
            bench = None

    returns = [r for _, r in series]
    dates = [d.isoformat() if d else None for d, _ in series]
    out = {
        "label": label,
        "months": len(returns),
        "first_month": dates[0], "last_month": dates[-1],
        "stats": monthly_stats(returns, rf),
        "rf_annual": rf,
        "returns": returns,
        "dates": dates,
        "growth": growth_of_100(returns),
        "drawdown": drawdowns(returns),
        "rolling": rolling(returns, window, rf) if len(returns) >= window else [],
        "window": window,
    }
    v, cv = var_cvar(returns)
    out["stats"]["var_95"] = v
    out["stats"]["cvar_95"] = cv
    out["stats"]["autocorrelation_lag1"] = lag1_autocorrelation(returns)
    if dates[0] is None:
        notes.append("No dates given: months are numbered from the first row; the monthly grid is not shown.")
    else:
        grid = {}
        for d, r in series:
            grid.setdefault(str(d.year), {})[str(d.month)] = r
        out["monthly_grid"] = grid

    if bench:
        pairs = _align(series, bench)
        if len(pairs) >= MIN_MONTHS:
            f = [p[1] for p in pairs]
            b = [p[2] for p in pairs]
            rel = relative_stats(f, b, rf)
            rel["benchmark"] = bench_label
            rel["benchmark_stats"] = monthly_stats(b, rf)
            rel["fund_stats_same_months"] = monthly_stats(f, rf)
            out["relative"] = rel
            out["benchmark_growth"] = growth_of_100(b)
            out["benchmark_dates"] = [p[0].isoformat() if p[0] else None for p in pairs]
            out["fund_growth_aligned"] = growth_of_100(f)
            if len(pairs) < len(series):
                notes.append("Benchmark comparison over the %d months both series cover." % len(pairs))
        else:
            notes.append("The benchmark overlaps fewer than %d months of the series; no comparison shown." % MIN_MONTHS)

    if inp.get("unsmooth"):
        try:
            rho_in = inp.get("rho")
            us, rho = unsmooth(returns, float(rho_in) if rho_in not in (None, "") else None)
            if len(us) >= 2:
                s = monthly_stats(us, rf)
                out["unsmoothed"] = {"rho": rho, "stats": s,
                                     "volatility_ratio": (s["annualized_volatility"] / out["stats"]["annualized_volatility"])
                                     if out["stats"]["annualized_volatility"] > 0 else None}
        except InputError as e:
            notes.append(str(e))
    out["notes"] = notes
    return out


def run(command, inp):
    if command == "analyze":
        return analyze(inp)
    raise InputError("Unknown command '%s' (analyze)." % command)


def _emit(obj):
    if fincept_json is not None:
        print(fincept_json.dumps(obj))
    else:  # pragma: no cover
        print(json.dumps(obj, default=str))


def main(argv):
    if not argv:
        _emit({"success": False, "error": "Usage: fund_analytics.py analyze --stdin"})
        return 1
    try:
        raw = sys.stdin.read() if (len(argv) > 1 and argv[1] == "--stdin") else (argv[1] if len(argv) > 1 else "{}")
        inp = json.loads(raw or "{}")
        if not isinstance(inp, dict):
            raise InputError("The input must be a JSON object.")
        _emit({"success": True, "command": argv[0], "data": run(argv[0], inp)})
        return 0
    except InputError as e:
        _emit({"success": False, "error": str(e)})
        return 0
    except json.JSONDecodeError as e:
        _emit({"success": False, "error": "The input is not valid JSON (%s)." % e.msg})
        return 0
    except Exception as e:
        _emit({"success": False, "error": "%s: %s" % (type(e).__name__, str(e)[:300])})
        return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
