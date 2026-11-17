"""
Equity Research data helpers (Fincept Terminal, Equity Research screen).

Commands (one JSON object on stdout; {"error", "code", "symbol"} on failure):

  events <symbol>
      Dividends and splits (as quoted: London in pence, Johannesburg in cents),
      earnings history (estimate vs actual EPS, surprise, report date — reported
      where Yahoo has it, else estimated from the volume spike after the quarter
      end), the next earnings date with its estimate range, the ex-dividend / pay
      dates and the per-share fields the host's info payload does not carry
      (trailing / forward EPS, dividend rate, payout ratio, fiscal-year end).

  peers_compare <sym1,sym2,...>
      One row per symbol: name, sector, industry, country, exchange, trading and
      reporting currency, price, day change, market cap (as reported AND in USD,
      so caps listed in different currencies compare), valuation multiples incl.
      EV/EBITDA, returns, margins, growth, leverage and liquidity ratios.

  peers_suggest <symbol> [count]
      Same-industry companies on the subject's own market first (Yahoo equity
      screener by industry + region, then sector + region), topped up with the
      global industry leaders. Cross-listings of the same company are dropped.

  risk_free <currency>
      The 10-year government bond yield for a currency: ^TNX for USD (live),
      OECD long-term rates via FRED for other currencies when reachable, else a
      reference value flagged "reference" so the host can say it must be checked.

Usage: python equity_research_data.py <command> <args>

Yahoo is reached through fincept_yahoo only (shared session, limiter, caches): profiles via
fy.info (a day), prices and FX via fy.quotes / fy.fx_rate, price history via the bar cache,
and every other Yahoo answer through fy.cached with its freshness class - calendar, dividends /
splits, EPS statements, earnings history and report dates a day; screener answers and Yahoo's
industry / sector leaders 3 days. A restart within those windows sends nothing to Yahoo.
"""

import concurrent.futures
import contextlib
import logging
import math
import os
import re
import sys
import threading
import time
import urllib.request
from datetime import date, datetime, timedelta, timezone

try:
    import fincept_json
except ImportError:  # scripts dir not on sys.path (unusual launcher)
    sys.path.append(os.path.dirname(os.path.abspath(__file__)))
    import fincept_json

import fincept_yahoo as fy  # noqa: E402  (guards every yfinance request of the process)

_DAY_S = 86400                 # profile, calendar, statements, dividends / splits, earnings history
_PEER_LIST_S = 3 * 86400       # screener answers, industry / sector leaders


# ── helpers ──────────────────────────────────────────────────────────────────


def _quiet():
    """yfinance prints some failures to stdout, which carries only the JSON line: whatever is
    printed inside the block goes to stderr. Thread-safe and reentrant (peers_compare enters it
    from 8 worker threads at once). yfinance's error LOG lines are left alone: _yf_errors()
    reads them to tell an empty answer from a failed request."""
    return fincept_json.stdout_to_stderr()


# A yfinance error line that means "try again later", not "Yahoo has nothing".
_TRANSIENT_LOG = re.compile(r"\b(429|5\d\d)\b|too many requests|rate limit|timed? ?out|curl|connect|resolve",
                            re.I)


@contextlib.contextmanager
def _yf_errors():
    """The ERROR lines yfinance logs on THIS thread inside the block: it logs a failed
    quoteSummary / timeseries / history request and hands back an empty answer."""
    me = threading.get_ident()
    sink = []

    class _Sink(logging.Handler):
        def emit(self, record):
            if record.levelno >= logging.ERROR and record.thread == me:
                try:
                    sink.append(record.getMessage())
                except Exception:
                    pass

    h = _Sink(level=logging.ERROR)
    lg = logging.getLogger("yfinance")
    lg.addHandler(h)
    try:
        yield sink
    finally:
        lg.removeHandler(h)


def _settle(value, errors, empty):
    """What fy.cached keeps of a yfinance answer that may hide a failure: a non-empty value as
    is; an empty one after a logged network / rate-limit error raises (nothing is cached and an
    older copy is served); an empty one after another logged error is kept for an hour only."""
    if not empty:
        return value
    if errors:
        if any(_TRANSIENT_LOG.search(e) for e in errors):
            raise ConnectionError("Yahoo Finance did not answer: " + errors[-1][:160])
        return fy.Partial(value, 3600)
    return value


def _diag(message):
    """One diagnostic line on stderr (stdout is the JSON result channel; never data)."""
    sys.stderr.write(f"[equity_research_data] {message}\n")
    sys.stderr.flush()


def _num(v):
    try:
        if v is None:
            return None
        f = float(v)
        return f if math.isfinite(f) else None
    except (TypeError, ValueError):
        return None


def _day(v):
    """A date-like value (pandas Timestamp, datetime, date, epoch seconds) as YYYY-MM-DD."""
    if v is None:
        return None
    try:
        if isinstance(v, (int, float)):
            if not math.isfinite(float(v)) or v <= 0:
                return None
            return datetime.fromtimestamp(float(v), tz=timezone.utc).strftime("%Y-%m-%d")
        if hasattr(v, "strftime"):
            return v.strftime("%Y-%m-%d")
        s = str(v)
        return s[:10] if len(s) >= 10 else None
    except Exception:
        return None


def _failure(symbol, code, message):
    return {"error": message, "code": code, "symbol": symbol}


def _classify(exc):
    text = str(exc).lower()
    if "too many requests" in text or "rate limit" in text:
        return "rate_limited"
    for marker in ("failed to connect", "could not resolve", "connection", "timed out", "network", "curl"):
        if marker in text:
            return "network"
    return "unknown"


# ── events ───────────────────────────────────────────────────────────────────


_MARKET_INDEX_BY_SUFFIX = {
    "": "^GSPC", "L": "^FTSE", "T": "^N225", "NS": "^NSEI", "BO": "^BSESN", "DE": "^GDAXI", "F": "^GDAXI",
    "PA": "^FCHI", "HK": "^HSI", "TO": "^GSPTSE", "AX": "^AXJO", "KS": "^KS11", "KQ": "^KQ11", "SW": "^SSMI",
    "AS": "^AEX", "MI": "FTSEMIB.MI", "MC": "^IBEX", "SS": "000001.SS", "SZ": "399001.SZ", "TW": "^TWII",
    "SA": "^BVSP", "ST": "^OMX", "SI": "^STI", "JO": "^J203.JO", "MX": "^MXX", "BR": "^BFX",
}


def _market_index_for(symbol):
    suffix = symbol.split(".", 1)[1].upper() if "." in symbol else ""
    return _MARKET_INDEX_BY_SUFFIX.get(suffix)


def _estimate_report_dates(hist, quarter_ends, market=None):
    """Report day of each quarter, estimated as the session 10-64 days after the quarter
    end with the most unusual volume and stock-specific move (the market index's move
    taken out, so a market-wide sell-off is not mistaken for results day). None when no
    session stands out — the caller then shows no reaction."""
    out = {}
    if hist is None or hist.empty or "Volume" not in hist.columns or "Close" not in hist.columns:
        return out
    vol = hist["Volume"].astype(float)
    close = hist["Close"].astype(float)
    ret = close.pct_change()
    if market is not None and not market.empty and "Close" in market.columns:
        try:
            import pandas as pd
            mret = market["Close"].astype(float).pct_change()
            by_day = {d.date(): v for d, v in zip(mret.index, mret.values)}
            mvals = [by_day.get(d.date(), 0.0) for d in hist.index]
            ret = ret - pd.Series(mvals, index=ret.index).fillna(0.0)
        except Exception:
            pass
    absret = ret.abs()
    med_vol = vol.rolling(20, min_periods=5).median().shift(1)
    med_ret = absret.rolling(20, min_periods=5).median().shift(1)
    idx_dates = [d.date() for d in hist.index]

    def quad_witching(d):
        # Third Friday of Mar/Jun/Sep/Dec: index rebalancing volume, not news.
        return d.month in (3, 6, 9, 12) and d.weekday() == 4 and 15 <= d.day <= 21

    for q in quarter_ends:
        try:
            qd = datetime.strptime(q, "%Y-%m-%d").date()
        except ValueError:
            continue
        # Most listings report 2-9 weeks after the quarter end (US 10-Q 40-45 days, Japan
        # and India 45, year-end results up to 60).
        lo, hi = qd + timedelta(days=10), qd + timedelta(days=64)
        best, best_score, best_vr = None, 0.0, 0.0
        for i, d in enumerate(idx_dates):
            if d < lo or d > hi or quad_witching(d):
                continue
            mv, mr = med_vol.iloc[i], med_ret.iloc[i]
            if not (mv > 0) or not math.isfinite(mv):
                continue
            vr = vol.iloc[i] / mv
            rr = absret.iloc[i] / mr if (mr > 0 and math.isfinite(mr)) else 1.0
            score = vr * (1.0 + min(rr, 6.0))
            if score > best_score:
                best_score, best, best_vr = score, d, vr
        if best is not None and best_vr >= 1.6:
            out[q] = best.strftime("%Y-%m-%d")
    return out


def _reported_earnings_dates(symbol, t):
    """[{report_date, eps fields}] from Yahoo's earnings calendar when it can be read (HTML
    scrape needs lxml; the screener endpoint may lag). Cached a day per symbol; a failed read
    is retried after an hour (a network / rate-limit failure is not cached at all)."""
    def fetch():
        rows, errors = [], []
        for getter in ("get_earnings_dates", "_get_earnings_dates_using_screener"):
            fn = getattr(t, getter, None)
            if fn is None:
                continue
            try:
                with _quiet(), _yf_errors() as errs:
                    df = fn(limit=28)
                errors += errs
            except Exception as exc:
                errors.append(f"{type(exc).__name__}: {exc}")
                df = None
            if df is None or getattr(df, "empty", True):
                continue
            for ts, row in df.iterrows():
                d = _day(ts)
                if not d:
                    continue
                rows.append({
                    "report_date": d,
                    "eps_estimate": _num(row.get("EPS Estimate")),
                    "eps_actual": _num(row.get("Reported EPS")),
                    "surprise_pct": _num(row.get("Surprise(%)")),
                })
            if rows:
                break
        return _settle(rows, errors, not rows)
    try:
        return fy.cached("earnings_dates", symbol, fetch, max_age=_DAY_S)
    except Exception:
        return []


def _calendar_value(v):
    """A calendar cell as JSON: dates 'YYYY-MM-DD', numbers float (None when not finite)."""
    if v is None:
        return None
    if hasattr(v, "strftime"):
        return _day(v)
    if isinstance(v, (int, float)) and not isinstance(v, bool):
        return _num(v)
    try:
        import numpy as np
        if isinstance(v, np.generic):
            return _num(v)
    except Exception:
        pass
    return str(v)


def calendar(symbol, ticker=None):
    """yf.Ticker(symbol).calendar as a JSON dict (dates 'YYYY-MM-DD', lists kept: "Earnings
    Date" is [first day, last day]), cached a day for every script (Equity Research events,
    the Relationship Map's calendar section). `ticker`: the yf.Ticker to ask on a cache miss."""
    def fetch():
        t = ticker if ticker is not None else fy.yf().Ticker(symbol)
        with _quiet(), _yf_errors() as errs:
            cal = t.calendar
        out = {}
        for k, v in (cal.items() if isinstance(cal, dict) else []):
            out[str(k)] = [_calendar_value(x) for x in v] if isinstance(v, (list, tuple)) else _calendar_value(v)
        return _settle(out, errs, not out)
    return fy.cached("calendar", symbol, fetch, max_age=_DAY_S)


def _actions(symbol, t):
    """{"dividends": [{date, amount}], "splits": [{date, ratio}]} as quoted (pence for LSE,
    cents for JSE), cached a day per symbol."""
    def fetch():
        with _quiet(), _yf_errors() as errs:
            div = t.dividends
            sp = t.splits
        out = {"dividends": [], "splits": []}
        for ts, amount in (div.items() if div is not None else []):
            a = _num(amount)
            if a is not None and a > 0:
                out["dividends"].append({"date": _day(ts), "amount": a})
        for ts, ratio in (sp.items() if sp is not None else []):
            r = _num(ratio)
            if r is not None and r > 0:
                out["splits"].append({"date": _day(ts), "ratio": r})
        return _settle(out, errs, not out["dividends"] and not out["splits"])
    return fy.cached("actions", symbol, fetch, max_age=_DAY_S)


def _eps_rows(table):
    """EPS / net income / revenue per period out of {period: {line item: value}}, oldest first."""
    rows = []
    for col, items in (table or {}).items():
        items = items if isinstance(items, dict) else {}

        def pick(*names):
            for n in names:
                if n in items:
                    return _num(items[n])
            return None
        rows.append({"period": _day(col), "eps_diluted": pick("Diluted EPS"), "eps_basic": pick("Basic EPS"),
                     "net_income": pick("Net Income Common Stockholders", "Net Income"),
                     "revenue": pick("Total Revenue", "Operating Revenue")})
    rows.sort(key=lambda r: r["period"] or "")
    return rows


def _statement_table(df):
    """A yfinance statement frame as {period: {line item: value}} (missing cells left out) -
    the shape yfinance_data.py keeps in the shared "financials" cache."""
    out = {}
    if df is None or getattr(df, "empty", True):
        return out
    for col in df.columns:
        cells = {}
        for idx, v in df[col].items():
            f = _num(v)
            if f is not None:
                cells[str(idx)] = f
        out[str(col)] = cells
    return out


def _eps_history(symbol, t):
    """{"quarterly": rows, "annual": rows} from the income statements, a day per symbol. The
    statements the Financials tab cached today (yfinance_data.py, kind "financials") serve
    when they hold both the annual and the quarterly income statement."""
    fin = fy.recall("financials", symbol, _DAY_S)
    if isinstance(fin, dict) and not fin.get("error") and fin.get("income_statement") \
            and "quarterly_income_stmt" not in (fin.get("_missing") or []):
        return {"quarterly": _eps_rows((fin.get("quarterly") or {}).get("income_statement")),
                "annual": _eps_rows(fin.get("income_statement"))}

    def fetch():
        with _quiet(), _yf_errors() as errs:
            q = _statement_table(t.quarterly_income_stmt)
            a = _statement_table(t.income_stmt)
        out = {"quarterly": _eps_rows(q), "annual": _eps_rows(a)}
        return _settle(out, errs, not out["quarterly"] and not out["annual"])
    return fy.cached("eps_history", symbol, fetch, max_age=_DAY_S)


def _earnings_history(symbol, t):
    """The last four quarters' estimate vs actual EPS (quoteSummary earningsHistory), a day per
    symbol: [{quarter, eps_estimate, eps_actual, surprise (fraction)}]."""
    def fetch():
        with _quiet(), _yf_errors() as errs:
            eh = t.earnings_history
        rows = []
        if eh is not None and not getattr(eh, "empty", True):
            for q, row in eh.iterrows():
                d = _day(q)
                if d:
                    rows.append({"quarter": d, "eps_estimate": _num(row.get("epsEstimate")),
                                 "eps_actual": _num(row.get("epsActual")),
                                 "surprise": _num(row.get("surprisePercent"))})
        return _settle(rows, errs, not rows)
    return fy.cached("earnings_history", symbol, fetch, max_age=_DAY_S)


def get_events(symbol):
    try:
        info = fy.info(symbol)  # one fetch per symbol per day, shared by every script
    except Exception as e:
        return _failure(symbol, _classify(e), f"Could not load events for {symbol}: {e}")
    if not info or not (info.get("longName") or info.get("shortName") or info.get("regularMarketPrice")):
        return _failure(symbol, "not_found", f"Yahoo Finance has no data for {symbol}")
    t = fy.yf().Ticker(symbol)  # asked only for what the caches do not hold

    out = {
        "symbol": symbol,
        "currency": info.get("currency"),
        "financial_currency": info.get("financialCurrency"),
        "quote_type": info.get("quoteType"),
        "exchange": info.get("exchange"),
        "full_exchange_name": info.get("fullExchangeName"),
        "exchange_tz": info.get("exchangeTimezoneName"),
        "industry_key": info.get("industryKey"),
        "sector_key": info.get("sectorKey"),
        "trailing_eps": _num(info.get("trailingEps")),
        "forward_eps": _num(info.get("forwardEps")),
        "book_value": _num(info.get("bookValue")),
        "dividend_rate": _num(info.get("dividendRate")),
        "trailing_dividend_rate": _num(info.get("trailingAnnualDividendRate")),
        "dividend_yield_pct": _num(info.get("dividendYield")),
        "five_year_avg_yield_pct": _num(info.get("fiveYearAvgDividendYield")),
        "payout_ratio": _num(info.get("payoutRatio")),
        "ex_dividend_date": _day(info.get("exDividendDate")),
        "dividend_pay_date": _day(info.get("dividendDate")),
        "last_fiscal_year_end": _day(info.get("lastFiscalYearEnd")),
        "most_recent_quarter": _day(info.get("mostRecentQuarter")),
        "shares_outstanding": _num(info.get("sharesOutstanding")),
        "next_earnings": None,
        "next_earnings_end": None,
        "earnings_estimate": None,
        "revenue_estimate": None,
        "dividends": [],
        "splits": [],
        "earnings": [],
        "eps_quarterly": [],
        "eps_annual": [],
    }

    # Upcoming earnings: the calendar first, the info timestamps as fallback.
    try:
        cal = calendar(symbol, t) or {}
    except Exception:
        cal = {}
    ed = cal.get("Earnings Date") if isinstance(cal, dict) else None
    if isinstance(ed, (list, tuple)) and ed:
        out["next_earnings"] = _day(ed[0])
        if len(ed) > 1:
            out["next_earnings_end"] = _day(ed[-1])
    if isinstance(cal, dict):
        out["earnings_estimate"] = {"avg": _num(cal.get("Earnings Average")), "low": _num(cal.get("Earnings Low")),
                                    "high": _num(cal.get("Earnings High"))}
        out["revenue_estimate"] = {"avg": _num(cal.get("Revenue Average")), "low": _num(cal.get("Revenue Low")),
                                   "high": _num(cal.get("Revenue High"))}
        if not out["ex_dividend_date"]:
            out["ex_dividend_date"] = _day(cal.get("Ex-Dividend Date"))
        if not out["dividend_pay_date"]:
            out["dividend_pay_date"] = _day(cal.get("Dividend Date"))
    if not out["next_earnings"]:
        ts = _num(info.get("earningsTimestampStart")) or _num(info.get("earningsTimestamp"))
        if ts and ts > time.time() - 86400:
            out["next_earnings"] = _day(ts)

    # Dividends / splits (as quoted — pence for LSE, cents for JSE).
    try:
        acts = _actions(symbol, t)
        out["dividends"] = list(acts.get("dividends") or [])
        out["splits"] = list(acts.get("splits") or [])
    except Exception as exc:
        _diag(f"WARNING Dividend / split history unavailable symbol={symbol} error={type(exc).__name__}")

    # EPS history from the statements (longer than the estimate history).
    try:
        eps = _eps_history(symbol, t)
        out["eps_quarterly"] = list(eps.get("quarterly") or [])
        out["eps_annual"] = list(eps.get("annual") or [])
    except Exception as exc:
        _diag(f"WARNING EPS history unavailable symbol={symbol} error={type(exc).__name__}")

    # Estimate vs actual: the last four quarters (quoteSummary earningsHistory) …
    hist_rows = {}
    try:
        for r in _earnings_history(symbol, t):
            sp_ = r.get("surprise")
            hist_rows[r["quarter"]] = {"quarter": r["quarter"], "eps_estimate": r.get("eps_estimate"),
                                       "eps_actual": r.get("eps_actual"),
                                       "surprise_pct": sp_ * 100.0 if sp_ is not None else None,
                                       "report_date": None, "date_source": "none"}
    except Exception as exc:
        _diag(f"WARNING Earnings history unavailable symbol={symbol} error={type(exc).__name__}")
    # Every quarter end we know of (statements + estimate history) gets a row.
    eps_by_q = {r["period"]: r for r in out["eps_quarterly"] if r.get("period")}
    for q in eps_by_q:
        if q not in hist_rows:
            hist_rows[q] = {"quarter": q, "eps_estimate": None, "eps_actual": eps_by_q[q]["eps_diluted"],
                            "surprise_pct": None, "report_date": None, "date_source": "none"}
    # Report dates from Yahoo's calendar where it still serves them: the first report
    # 5-95 days after a quarter end belongs to that quarter.
    reported = sorted(_reported_earnings_dates(symbol, t), key=lambda r: r["report_date"])
    used = set()
    for q in sorted(hist_rows):
        qd = datetime.strptime(q, "%Y-%m-%d").date()
        for k, rep in enumerate(reported):
            if k in used:
                continue
            rd = datetime.strptime(rep["report_date"], "%Y-%m-%d").date()
            if timedelta(days=5) <= rd - qd <= timedelta(days=95):
                r = hist_rows[q]
                r["report_date"], r["date_source"] = rep["report_date"], "reported"
                for f in ("eps_estimate", "eps_actual", "surprise_pct"):
                    if r.get(f) is None:
                        r[f] = rep[f]
                used.add(k)
                break
    # Older reported quarters the statements do not cover.
    for k, rep in enumerate(reported):
        if k in used or rep["eps_actual"] is None:
            continue
        if datetime.strptime(rep["report_date"], "%Y-%m-%d").date() > date.today():
            continue
        hist_rows["~" + rep["report_date"]] = {
            "quarter": None, "eps_estimate": rep["eps_estimate"], "eps_actual": rep["eps_actual"],
            "surprise_pct": rep["surprise_pct"], "report_date": rep["report_date"], "date_source": "reported"}

    # The latest report day Yahoo knows exactly (info earningsTimestamp, when in the past).
    last_ts = _num(info.get("earningsTimestamp"))
    if last_ts and last_ts < time.time():
        last_day = datetime.fromtimestamp(last_ts, tz=timezone.utc).date()
        for q in sorted((k for k, r in hist_rows.items() if r.get("quarter")), reverse=True):
            qd = datetime.strptime(q, "%Y-%m-%d").date()
            if timedelta(days=5) <= last_day - qd <= timedelta(days=95):
                if hist_rows[q].get("date_source") != "reported":
                    hist_rows[q]["report_date"], hist_rows[q]["date_source"] = last_day.isoformat(), "reported"
                break

    # Report dates Yahoo did not give: estimate them from the volume spike.
    need = [q for q, r in hist_rows.items() if r.get("quarter") and not r.get("report_date")]
    if need:
        try:
            # The shared bar cache: the index's 5 years serve every symbol of that market.
            hist = fy.ticker_history(symbol, period="5y", interval="1d", auto_adjust=False, actions=False)
            market = None
            index_sym = _market_index_for(symbol)
            if index_sym:
                try:
                    market = fy.ticker_history(index_sym, period="5y", interval="1d", actions=False)
                except Exception:
                    market = None
            est = _estimate_report_dates(hist, need, market)
        except Exception:
            est = {}
        for q, d in est.items():
            hist_rows[q]["report_date"], hist_rows[q]["date_source"] = d, "estimated"
    # Drop quarters that carry nothing to show (no EPS and no date).
    rows = [r for r in hist_rows.values()
            if r.get("report_date") or r.get("eps_actual") is not None or r.get("eps_estimate") is not None]
    out["earnings"] = sorted(rows, key=lambda r: r.get("quarter") or r.get("report_date") or "")
    return out


# ── peers ────────────────────────────────────────────────────────────────────

_MINOR_TO_MAJOR = {"GBp": "GBP", "GBX": "GBP", "ZAc": "ZAR", "ZAC": "ZAR", "ILA": "ILS", "KWF": "KWD"}


def _major(ccy):
    return _MINOR_TO_MAJOR.get(ccy, (ccy or "").upper()) if ccy else ""


def _usd_rate(ccy):
    """USD per one unit of `ccy` (major units); None when unknown (never 1.0 for another
    currency). From fincept_yahoo's quote cache."""
    major = _major(ccy)
    if not major:
        return None
    try:
        return fy.fx_rate(major, "USD")
    except Exception:
        return None


def _peer_row(symbol, quote=None):
    """One comparison row: fundamentals from the day's profile (fy.info), price / day change /
    market cap / currency from the live quote (`quote`: the v7 row, None when Yahoo's quote
    does not know the symbol - the profile's values are used then)."""
    try:
        i = fy.info(symbol)
    except Exception as e:
        return {"symbol": symbol, "error": str(e)}
    q = quote or {}
    if not (i.get("longName") or i.get("shortName") or i.get("regularMarketPrice")):
        return {"symbol": symbol, "error": "not found"}
    ccy = q.get("currency") or i.get("currency")
    if q:
        price = _num(q.get("regularMarketPrice"))
        change = _num(q.get("regularMarketChangePercent"))
        mcap = _num(q.get("marketCap"))  # major units of the trading currency (pounds, not pence)
    else:
        price = _num(i.get("currentPrice")) or _num(i.get("regularMarketPrice"))
        change = _num(i.get("regularMarketChangePercent"))
        mcap = _num(i.get("marketCap"))
    if price is None:
        price = _num(i.get("currentPrice")) or _num(i.get("regularMarketPrice"))
    if mcap is None:
        mcap = _num(i.get("marketCap"))
    usd = _usd_rate(ccy) if mcap else None
    return {
        "symbol": symbol,
        "name": i.get("longName") or i.get("shortName"),
        "sector": i.get("sector"),
        "industry": i.get("industry"),
        "country": i.get("country"),
        "exchange": i.get("exchange"),
        "currency": ccy,
        "financial_currency": i.get("financialCurrency"),
        "price": price,
        "change_pct": change,
        "market_cap": mcap,
        "market_cap_usd": mcap * usd if (mcap and usd) else None,
        "enterprise_value": _num(i.get("enterpriseValue")),
        "pe": _num(i.get("trailingPE")),
        "forward_pe": _num(i.get("forwardPE")),
        "pb": _num(i.get("priceToBook")),
        "ps": _num(i.get("priceToSalesTrailing12Months")),
        "peg": _num(i.get("trailingPegRatio")) or _num(i.get("pegRatio")),
        "ev_ebitda": _num(i.get("enterpriseToEbitda")),
        "ev_revenue": _num(i.get("enterpriseToRevenue")),
        "roe": _num(i.get("returnOnEquity")),
        "roa": _num(i.get("returnOnAssets")),
        "gross_margin": _num(i.get("grossMargins")),
        "operating_margin": _num(i.get("operatingMargins")),
        "profit_margin": _num(i.get("profitMargins")),
        "revenue_growth": _num(i.get("revenueGrowth")),
        "earnings_growth": _num(i.get("earningsGrowth")),
        "debt_to_equity": _num(i.get("debtToEquity")),       # percent (152 = 1.52x)
        "current_ratio": _num(i.get("currentRatio")),
        "quick_ratio": _num(i.get("quickRatio")),
        "dividend_yield_pct": _num(i.get("dividendYield")),  # percent
        "beta": _num(i.get("beta")),
    }


def get_peers_compare(symbols):
    syms = [s.strip() for s in symbols if s and s.strip()]
    if not syms:
        return {"error": "No symbols", "code": "unknown", "symbol": ""}
    # Every price / change / market cap / currency in one batched quote request, the USD legs
    # of their currencies in one more; the rows then read both from the quote cache.
    try:
        quotes = fy.quotes(syms)
    except Exception as exc:
        _diag(f"WARNING Peer quotes unavailable error={type(exc).__name__}")
        quotes = {}
    pairs = sorted({f"{_major(r.get('currency'))}USD=X" for r in quotes.values()
                    if _major(r.get("currency")) not in ("", "USD")})
    if pairs:
        try:
            fy.quotes(pairs)
        except Exception:
            pass
    rows = {}
    with concurrent.futures.ThreadPoolExecutor(max_workers=min(8, len(syms))) as pool:
        futs = {pool.submit(_peer_row, s, quotes.get(s)): s for s in syms}
        for f in concurrent.futures.as_completed(futs):
            s = futs[f]
            try:
                rows[s] = f.result()
            except Exception as e:
                rows[s] = {"symbol": s, "error": str(e)}
    ordered = [rows[s] for s in syms if s in rows]
    return {"symbol": syms[0], "peers": [r for r in ordered if "error" not in r],
            "missing": [r["symbol"] for r in ordered if "error" in r]}


_REGION_BY_COUNTRY = {
    "United States": "us", "United Kingdom": "gb", "Japan": "jp", "India": "in", "Germany": "de", "France": "fr",
    "Canada": "ca", "Australia": "au", "Hong Kong": "hk", "China": "cn", "Taiwan": "tw", "South Korea": "kr",
    "Switzerland": "ch", "Netherlands": "nl", "Spain": "es", "Italy": "it", "Sweden": "se", "Norway": "no",
    "Denmark": "dk", "Finland": "fi", "Belgium": "be", "Ireland": "ie", "Brazil": "br", "Mexico": "mx",
    "Singapore": "sg", "South Africa": "za", "Israel": "il", "Indonesia": "id", "Malaysia": "my",
    "Thailand": "th", "New Zealand": "nz", "Austria": "at", "Portugal": "pt", "Saudi Arabia": "sa",
    "Turkey": "tr", "Poland": "pl", "Chile": "cl", "Argentina": "ar", "Philippines": "ph", "Greece": "gr",
}


def _screener_key(name):
    return re.sub(r"\s+", " ", re.sub(r"[^A-Za-z0-9 ]", " ", name or "")).strip().lower()


def top_companies(cls, key):
    """Yahoo's largest companies of an industry (cls "Industry") or sector ("Sector") key:
    [{"symbol", "name", "weight" (market weight)}], largest first. Cached 3 days for every
    script (Equity Research peers, Relationship Map peers, fetch_peers.py)."""
    def fetch():
        yfm = fy.yf()
        with _quiet(), _yf_errors() as errs:
            df = getattr(yfm, cls)(key).top_companies
        rows = []
        if df is not None and not getattr(df, "empty", True):
            has_name, has_weight = "name" in df.columns, "market weight" in df.columns
            for sym in df.index:
                rec = df.loc[sym]
                if hasattr(rec, "ndim") and rec.ndim > 1:  # a symbol listed twice
                    rec = rec.iloc[0]
                name = rec.get("name") if has_name else None
                rows.append({"symbol": str(sym).upper(),
                             "name": name if isinstance(name, str) and name.strip() else None,
                             "weight": _num(rec.get("market weight")) if has_weight else None})
        return _settle(rows, errs, not rows)
    return fy.cached(cls.lower() + "_top", key, fetch, max_age=_PEER_LIST_S)


def _screen_quotes(field, value, region):
    """Yahoo's equity screen `field` == value in `region`, largest 40 by market cap: the quote
    fields the peer search reads. Cached 3 days per screen."""
    def fetch():
        yfm = fy.yf()
        EquityQuery = yfm.EquityQuery
        q = EquityQuery("and", [EquityQuery("eq", [field, value]), EquityQuery("eq", ["region", region])])
        with _quiet():
            res = yfm.screen(q, sortField="intradaymarketcap", sortAsc=False, size=40)
        keep = ("symbol", "quoteType", "longName", "shortName", "exchange", "marketCap")
        return [{k: qt.get(k) for k in keep} for qt in (res or {}).get("quotes", []) or [] if isinstance(qt, dict)]
    return fy.cached("peer_screen", f"{field}|{value}|{region}|intradaymarketcap|40", fetch, max_age=_PEER_LIST_S)


def get_peers_suggest(symbol, count=6):
    try:
        info = fy.info(symbol)  # shared daily profile
    except Exception as e:
        return _failure(symbol, _classify(e), f"Could not look up {symbol}: {e}")
    industry, sector = info.get("industry"), info.get("sector")
    region = _REGION_BY_COUNTRY.get(info.get("country") or "")
    base = symbol.split(".")[0].upper()
    own_suffix = symbol.split(".", 1)[1].upper() if "." in symbol else ""
    own_cap = _num(info.get("marketCap"))
    # A same-industry screen sorted by size still lists micro-caps when the industry is
    # thin (Apple's "Consumer Electronics"); below 2% of the subject's size they are not
    # comparables, and the sector screen fills in instead.
    min_cap = own_cap * 0.02 if own_cap else None
    picked, seen_names = [], set()

    def add(sym, name=None, exch=None, same_market_only=False, cap=None):
        if not sym:
            return
        s = str(sym).upper()
        root = s.split(".")[0]
        if s == symbol.upper() or root == base or s in picked:
            return
        # Same listing venue family: NYSE and Nasdaq both bare, LSE ".L", NSE ".NS" —
        # drops cross-trading venues (.XC, .IL) and the BSE twin of an NSE listing.
        s_suffix = s.split(".", 1)[1] if "." in s else ""
        if same_market_only and s_suffix != own_suffix:
            return
        if exch and str(exch).upper() in ("PNK", "OQX", "OQB", "OEM", "OTC", "OBB"):
            return  # OTC lines of foreign companies (SSNLF = Samsung), not local peers
        if s_suffix == "L" and root[:1] == "0":
            return  # LSE international order book (foreign shares quoted in London)
        if min_cap and cap is not None and cap < min_cap:
            return
        key = (name or "").strip().lower()
        if key and key in seen_names:
            return  # the same company listed twice (NSE + BSE, LSE + cross-trading venue)
        if key:
            seen_names.add(key)
        picked.append(s)

    try:
        from yfinance import EquityQuery
        from yfinance.const import EQUITY_SCREENER_EQ_MAP
        valid_ind = {}
        for _sec, names in (EQUITY_SCREENER_EQ_MAP.get("industry") or {}).items():
            for n in names:
                valid_ind[_screener_key(n)] = n
        valid_regions = set(EQUITY_SCREENER_EQ_MAP.get("region") or [])
    except Exception:
        EquityQuery, valid_ind, valid_regions = None, {}, set()

    own_country = info.get("country")
    country_cache = {}

    def countries_of(symbols):
        """Company domicile per symbol (parallel profile look-ups from the shared daily cache)."""
        todo = [s for s in symbols if s not in country_cache]
        if todo:
            def one(s):
                try:
                    return s, fy.info(s).get("country")
                except Exception:
                    return s, None
            with concurrent.futures.ThreadPoolExecutor(max_workers=min(8, len(todo))) as pool:
                for s, c in pool.map(one, todo):
                    country_cache[s] = c
        return {s: country_cache.get(s) for s in symbols}

    def run_screen(field, value):
        if EquityQuery is None or not value or not region or region not in valid_regions:
            return
        try:
            res_quotes = _screen_quotes(field, value, region)
        except Exception as exc:
            _diag(f"WARNING Peer screen failed field={field} region={region} error={type(exc).__name__}")
            return
        quotes = [qt for qt in res_quotes if qt.get("quoteType") in (None, "EQUITY") and qt.get("symbol")]
        # Outside the US a region screen is a screen of LISTINGS: Frankfurt / Xetra quote
        # Apple (APC.DE), Microsoft (MSF.DE), Nvidia (NVD.DE) … and sorted by size those
        # were SAP's "peers". Keep companies domiciled in the subject's own country.
        if region != "us" and own_country:
            # Below 2 % of the subject's size a listing is dropped by add() anyway: skip its look-up.
            quotes = [qt for qt in quotes if not (min_cap and _num(qt.get("marketCap")) is not None
                                                  and _num(qt.get("marketCap")) < min_cap)]
            cand = [str(qt["symbol"]).upper() for qt in quotes]
            dom = countries_of(cand)
            quotes = [qt for qt in quotes if dom.get(str(qt["symbol"]).upper()) == own_country]
        for quote in quotes:
            add(quote.get("symbol"), quote.get("longName") or quote.get("shortName"), quote.get("exchange"),
                same_market_only=True, cap=_num(quote.get("marketCap")))
            if len(picked) >= count:
                break

    suffix = symbol.split(".", 1)[1].upper() if "." in symbol else ""

    def industry_leaders(same_listing_only):
        key = info.get("industryKey")
        if not key or len(picked) >= count:
            return
        try:
            leaders = top_companies("Industry", key)
        except Exception as exc:
            _diag(f"WARNING Industry leaders lookup failed industry={key} error={type(exc).__name__}")
            return
        if not leaders:
            return
        own_weight = next((r.get("weight") for r in leaders if r["symbol"] == symbol.upper()), None)
        for r in leaders:
            s = r["symbol"]
            s_suffix = s.split(".", 1)[1] if "." in s else ""
            if same_listing_only and s_suffix != suffix:
                continue
            if own_weight:
                w = r.get("weight")
                if w is not None and w < own_weight * 0.02:
                    continue  # a micro-cap next to an industry giant
            add(s, r.get("name"))
            if len(picked) >= count:
                break

    # Same industry on the same market, then the industry's leaders listed on the same
    # kind of venue, then the sector on the same market, then the global industry leaders.
    run_screen("industry", valid_ind.get(_screener_key(industry)))
    industry_leaders(True)
    if len(picked) < count:
        run_screen("sector", sector)
    industry_leaders(False)

    # Top up with Yahoo's global sector leaders.
    if len(picked) < count:
        for key, cls in ((info.get("sectorKey"), "Sector"),):
            if not key or len(picked) >= count:
                continue
            try:
                for r in top_companies(cls, key):
                    add(r["symbol"], r.get("name"))
                    if len(picked) >= count:
                        break
            except Exception:
                continue
    return {"symbol": symbol, "industry": industry, "sector": sector, "country": info.get("country"),
            "region": region, "peers": picked[:count]}


# ── risk-free ────────────────────────────────────────────────────────────────

# OECD long-term (10-year) government bond yields on FRED, monthly, percent.
_FRED_10Y = {
    "GBP": "IRLTLT01GBM156N", "EUR": "IRLTLT01DEM156N", "JPY": "IRLTLT01JPM156N", "INR": "INDIRLTLT01STM",
    "CAD": "IRLTLT01CAM156N", "AUD": "IRLTLT01AUM156N", "CHF": "IRLTLT01CHM156N", "SEK": "IRLTLT01SEM156N",
    "NOK": "IRLTLT01NOM156N", "DKK": "IRLTLT01DKM156N", "NZD": "IRLTLT01NZM156N", "KRW": "IRLTLT01KRM156N",
    "ZAR": "IRLTLT01ZAM156N", "MXN": "IRLTLT01MXM156N", "ILS": "IRLTLT01ILM156N", "PLN": "IRLTLT01PLM156N",
    "CZK": "IRLTLT01CZM156N", "HUF": "IRLTLT01HUM156N", "CLP": "IRLTLT01CLM156N", "COP": "IRLTLT01COM156N",
}

# Reference 10-year yields (percent, mid-2025) used only when no live source answers.
# The host labels them "reference — check" and lets the user type the current yield.
_REFERENCE_10Y = {
    "USD": 4.4, "GBP": 4.6, "EUR": 2.6, "JPY": 1.5, "INR": 6.3, "CAD": 3.3, "AUD": 4.3, "CHF": 0.4,
    "SEK": 2.3, "NOK": 3.9, "DKK": 2.4, "NZD": 4.6, "KRW": 2.8, "ZAR": 10.0, "MXN": 9.3, "ILS": 4.5,
    "PLN": 5.5, "CZK": 4.0, "HUF": 6.8, "CLP": 6.0, "COP": 11.5, "CNY": 1.7, "HKD": 3.3, "SGD": 2.4,
    "TWD": 1.5, "BRL": 13.5, "IDR": 6.8, "MYR": 3.6, "THB": 1.8, "PHP": 6.3, "SAR": 5.0, "AED": 4.6,
    "TRY": 30.0,
}


def get_risk_free(ccy):
    major = {"GBp": "GBP", "GBX": "GBP", "ZAc": "ZAR", "ZAC": "ZAR", "ILA": "ILS", "KWF": "KWD"}.get(ccy, (ccy or "").upper())
    if not major:
        return {"error": "No currency", "code": "unknown", "symbol": ""}
    if major == "USD":
        try:
            # The quote cache (fast_info.lastPrice downloaded a year of bars for one number).
            px = _num((fy.quotes(["^TNX"]).get("^TNX") or {}).get("regularMarketPrice"))
            if px and 0 < px < 20:
                return {"currency": major, "rate_pct": float(px), "as_of": date.today().isoformat(),
                        "source": "live", "label": "US 10Y Treasury (^TNX)"}
        except Exception:
            pass
    sid = _FRED_10Y.get(major)
    if sid:
        try:
            req = urllib.request.Request(f"https://fred.stlouisfed.org/graph/fredgraph.csv?id={sid}",
                                         headers={"User-Agent": "Mozilla/5.0"})
            with urllib.request.urlopen(req, timeout=6) as resp:
                lines = resp.read().decode("utf-8", "replace").strip().splitlines()
            for line in reversed(lines[1:]):
                parts = line.split(",")
                if len(parts) == 2:
                    v = _num(parts[1])
                    if v is not None:
                        return {"currency": major, "rate_pct": v, "as_of": parts[0], "source": "oecd",
                                "label": f"OECD 10Y government yield ({sid}, monthly)"}
        except Exception:
            pass
    if major in _REFERENCE_10Y:
        return {"currency": major, "rate_pct": _REFERENCE_10Y[major], "as_of": "2025-06", "source": "reference",
                "label": "reference 10Y yield (mid-2025) — check the current rate"}
    return _failure(major, "no_data", f"No government yield on file for {major}")


# ── entry point ──────────────────────────────────────────────────────────────


def main(args=None):
    if args is None:
        args = sys.argv[1:]
    if not args:
        return fincept_json.dumps({"error": "Usage: python equity_research_data.py <command> <args>", "code": "unknown"})
    cmd = args[0]
    try:
        # yfinance prints some failures to stdout, which carries only the JSON line.
        with fincept_json.stdout_to_stderr():
            if cmd == "events" and len(args) >= 2:
                result = get_events(args[1])
            elif cmd == "peers_compare" and len(args) >= 2:
                result = get_peers_compare(args[1].split(","))
            elif cmd == "peers_suggest" and len(args) >= 2:
                result = get_peers_suggest(args[1], int(args[2]) if len(args) > 2 else 6)
            elif cmd == "risk_free" and len(args) >= 2:
                result = get_risk_free(args[1])
            else:
                result = {"error": f"Unknown command or missing argument: {' '.join(args)}", "code": "unknown"}
    except Exception as e:
        result = {"error": str(e), "code": _classify(e), "symbol": args[1] if len(args) > 1 else ""}
    return fincept_json.dumps(result)


if __name__ == "__main__":
    print(main())
