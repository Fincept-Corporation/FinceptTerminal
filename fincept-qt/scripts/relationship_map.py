#!/usr/bin/env python3
"""
Relationship Map data fetcher (Fincept Terminal, Relationship Map tab) - schema 2.

Usage: python relationship_map.py <TICKER> [--sections id,id,...]

stdout: exactly ONE JSON line, at the end: the whole payload, or {"error": "<sentence>"}.
stderr: while the run goes, progress lines (each on its own line, after a newline, so library
        chatter never shares the line):
          @@RELMAP {"event":"plan","ticker":T,"sections":[ids]}
          @@RELMAP {"event":"section","ticker":T,"id":id,"status":{...},"data":{...}}
        one "section" line per planned section, as each finishes.

The contract (keys, units, section ids, which top-level keys belong to which section) is the
one documented in src/services/relationship_map/RelationshipMapParse.h. Every schema-1 key keeps
its name and meaning. Units: holder / insider "percentage" and "change_percent" are PERCENT; every
other ratio is a FRACTION; currencies are never assumed to be USD ("" when unknown).

Sections (status per section: ok | empty | timeout | skipped | error | unavailable | not_applicable):
  profile       Yahoo Finance Ticker.info (first; everything else needs it)
  management    the profile's companyOfficers
  holders       Yahoo major / institutional / fund holders
  insiders      Yahoo insider roster, transactions, 6-month purchase summary
  analysts      Yahoo price targets, recommendation trend, upgrades / downgrades
  calendar      Yahoo earnings and dividend dates
  peers         equity_research_data.get_peers_suggest (the Equity Research Peers service:
                industry screen on the company's own market), curated table as a fallback
  correlation   computed from one Yahoo price download (company, peers, sector ETF, index)
  fund          ETFs / mutual funds: Yahoo funds_data (holdings, sector weights, asset classes)
  subsidiaries  SEC EDGAR Exhibit 21 (US SEC filers only)
  supply_chain  no free source: always unavailable, never invented

--sections a,b fetches the profile plus those sections only (same output shape); the terminal
folds the reply into the remembered payload.

Yahoo goes through fincept_yahoo (shared session, limiter, caches shared by every process):
profiles (the company's and the peers') via fy.info, a day; live price / day change / market cap
via fy.quotes; the correlation prices from the shared bar cache (fy.download); the calendar via
equity_research_data.calendar (a day, shared with Equity Research); the holders, insiders,
analysts and fund sections are kept a day per (ticker, section) once complete. A section served
from the cache still emits its @@RELMAP line.
"""

import concurrent.futures
import os
import re
import sys
import threading
import time
from datetime import datetime, timedelta, timezone

try:
    import fincept_json
except ImportError:  # scripts dir not on sys.path (unusual launcher)
    sys.path.append(os.path.dirname(os.path.abspath(__file__)))
    import fincept_json

import fincept_yahoo as fy  # noqa: E402  (guards every yfinance request of the process)
import relationship_map_sources as srcs  # noqa: E402

SCHEMA = 2

# Sections whose complete outcome is kept a day per (ticker, section) for every process.
_CACHED_SECTIONS = ("holders", "insiders", "analysts", "fund")
_SECTION_TTL_S = 86400

SECTION_IDS = ("profile", "holders", "insiders", "management", "analysts", "calendar", "peers",
               "correlation", "fund", "subsidiaries", "supply_chain")

# Top-level payload keys of each section (RelationshipMapParse.cpp section_keys()).
SECTION_KEYS = {
    "profile": ("company", "governance", "technicals", "short_interest", "enterprise", "margins"),
    "holders": ("major_holders", "institutional_holders", "mutualfund_holders"),
    "insiders": ("insider_holders", "insider_transactions", "insider_summary"),
    "management": ("officers",),
    "analysts": ("analyst_targets", "recommendations_summary", "upgrades_downgrades"),
    "calendar": ("calendar",),
    "peers": ("peers", "peer_source"),
    "correlation": ("correlation",),
    "fund": ("fund",),
    "subsidiaries": ("subsidiaries", "subsidiaries_filing"),
    "supply_chain": ("supply_chain",),
}

_LIST_KEYS = {"recommendations_summary", "upgrades_downgrades", "officers", "institutional_holders",
              "mutualfund_holders", "insider_holders", "insider_transactions", "peers", "subsidiaries",
              "supply_chain"}

# Labels a failed section puts in "missing_sections": release-1 labels where the screen knows
# them (RelationshipMapFormat.h missing_for_kind), else the section id. merge_retry_json() maps
# every one of them back to its section.
_SECTION_LABELS = {
    "profile": ["company profile"],
    "holders": ["institutional holders", "fund holders"],
    "insiders": ["insiders", "insider transactions"],
    "analysts": ["analyst targets", "recommendations", "upgrades/downgrades"],
}

_SOURCES = {"correlation": "Computed from Yahoo prices", "subsidiaries": "SEC EDGAR", "supply_chain": ""}

_FAILED = ("timeout", "skipped", "error")

# Instruments without shareholders, officers, filings or peers.
_NO_RELATIONSHIPS = {"CRYPTOCURRENCY": "a cryptocurrency", "CURRENCY": "a currency pair", "INDEX": "an index",
                     "FUTURE": "a futures contract", "OPTION": "an option", "COMMODITY": "a commodity"}
_FUNDS = {"ETF": "an ETF", "MUTUALFUND": "a mutual fund", "MONEYMARKET": "a money-market fund"}
_NOT_FOR_FUNDS = ("holders", "insiders", "management", "analysts", "peers", "subsidiaries", "supply_chain")

SUPPLY_CHAIN_NOTE = "Customers and suppliers are not available from free sources"


# ── Small helpers ────────────────────────────────────────────────────────────


def _safe_float(val, default=0.0):
    try:
        f = float(val) if val is not None else default
    except (TypeError, ValueError):
        return default
    # NaN / inf (common in yfinance frames) would be written as bare NaN / Infinity tokens,
    # which are not JSON and make the whole result unreadable.
    if f != f or f in (float("inf"), float("-inf")):
        return default
    return f


def _safe_int(val, default=0):
    try:
        return int(val) if val is not None else default
    except (TypeError, ValueError, OverflowError):
        return default


def _safe_str(val, default=""):
    return str(val) if val is not None else default


def _clean(val):
    """A text cell: "" for None / NaN / NA."""
    if val is None:
        return ""
    try:
        if val != val:  # NaN
            return ""
    except Exception:
        pass
    s = str(val).strip()
    return "" if s.lower() in ("nan", "none", "<na>", "nat") else s


def _day(val):
    """A date-like value (Timestamp, datetime, date, ISO text, epoch seconds) as YYYY-MM-DD; ""."""
    if val is None:
        return ""
    try:
        if isinstance(val, (int, float)):
            if val != val or val <= 0:
                return ""
            return datetime.fromtimestamp(float(val), tz=timezone.utc).strftime("%Y-%m-%d")
        if hasattr(val, "strftime"):
            return val.strftime("%Y-%m-%d")
        s = _clean(val)
        return s[:10] if re.match(r"^\d{4}-\d{2}-\d{2}", s) else ""
    except Exception:
        return ""


def _default(key):
    if key in _LIST_KEYS:
        return []
    if key == "peer_source":
        return "none"
    return {}


def _no_data(exc):
    """yfinance's "there is nothing" exceptions (no upgrade history, no fund data): empty, not an error."""
    text = str(exc).lower()
    return ("no " in text and "found" in text) or "no data" in text


def _short_error(exc):
    text = re.sub(r"\s+", " ", str(exc) or type(exc).__name__).strip()
    if "too many requests" in text.lower() or "rate limit" in text.lower():
        return "rate-limited"
    return text[:160]


def _info_attempts(symbol):
    """fn() for _Budget.call_retry: the shared daily profile (fy.info, a new Ticker per fetch).
    The retry joins the request still in flight instead of sending a second one: an abandoned
    attempt is mostly one queued behind the shared request limiter, and a duplicate would only
    lengthen that queue."""
    return lambda: dict(fy.info(symbol))


def _quote_overlay(target, quote, fields):
    """Copy the live quote's values over the day-old profile's: fields = {target key: quote key}.
    Only finite numbers (prices / caps > 0) replace a value."""
    if not quote:
        return
    for key, qkey in fields.items():
        v = quote.get(qkey)
        if v is None or isinstance(v, bool):
            continue
        f = _safe_float(v, None)
        if f is None or (key != "day_change_pct" and f <= 0):
            continue
        target[key] = f


# ── Streaming ────────────────────────────────────────────────────────────────

_EMIT_LOCK = threading.Lock()
_STREAM = sys.stderr


def _emit(event):
    """One "@@RELMAP {json}" line on stderr, on its own line, written in one piece."""
    line = "\n@@RELMAP " + fincept_json.dumps(event, ensure_ascii=True, separators=(",", ":")) + "\n"
    with _EMIT_LOCK:
        try:
            _STREAM.write(line)
            _STREAM.flush()
        except Exception:
            pass


# ── Time budget ──────────────────────────────────────────────────────────────


class _Skipped(Exception):
    """A call that was not made (kind "skipped": the budget is used up) or did not answer
    inside its limit (kind "timeout")."""

    def __init__(self, label, kind):
        super().__init__(label)
        self.label = label
        self.kind = kind


class _Budget:
    """Total time budget for one run plus a per-call limit.

    One stalled Yahoo / SEC call used to stall the whole run; now it is abandoned after
    `per_call` seconds (the worker thread is a daemon and dies with the process) and the run
    continues with what it has. Once the total budget is spent, calls not yet made are skipped.
    Environment: FINCEPT_RELMAP_BUDGET_S (default 45), FINCEPT_RELMAP_CALL_TIMEOUT_S (12).
    """

    def __init__(self):
        self.total = self._env("FINCEPT_RELMAP_BUDGET_S", 45.0)
        self.per_call = self._env("FINCEPT_RELMAP_CALL_TIMEOUT_S", 12.0)
        self.start = time.monotonic()

    @staticmethod
    def _env(name, default):
        try:
            value = float(os.environ.get(name, ""))
            return value if value > 0 else default
        except ValueError:
            return default

    def remaining(self):
        return self.total - (time.monotonic() - self.start)

    def call(self, label, fn, timeout=None):
        """fn() with the limits applied (`timeout` replaces the per-call limit for a composite
        call). Raises _Skipped on time-out / exhausted budget; re-raises what fn raised."""
        left = self.remaining()
        if left <= 0.5:
            raise _Skipped(label, "skipped")
        box = {}

        def work():
            try:
                box["value"] = fn()
            except BaseException as exc:  # handed back to the caller's thread
                box["error"] = exc

        t = threading.Thread(target=work, name="relmap-" + label, daemon=True)
        t.start()
        t.join(min(timeout or self.per_call, left))
        if t.is_alive():
            raise _Skipped(label, "timeout")
        if "error" in box:
            raise box["error"]
        return box.get("value")

    def call_retry(self, label, fn, first, second):
        """call() with one more attempt after a time-out: a stalled Yahoo request usually answers
        at once on a new one. `fn` must start from scratch (a new yf.Ticker: a Ticker whose info
        fetch was abandoned answers None from then on)."""
        try:
            return self.call(label, fn, first)
        except _Skipped as exc:
            if exc.kind != "timeout" or self.remaining() < 2.0:
                raise
        return self.call(label, fn, second)


class _TimedTicker:
    """yfinance.Ticker whose data properties are fetched through the budget, each at most once.
    A request that stalls past half the per-call limit is asked once more on a fresh Ticker
    (which then serves the later properties; the abandoned one keeps waiting on its own).
    The holder / insider properties share one Yahoo request: they are fetched under one lock,
    and when that request timed out the others fail at once instead of asking again."""

    LABELS = {
        "analyst_price_targets": "analyst targets", "recommendations_summary": "recommendations",
        "upgrades_downgrades": "upgrades/downgrades", "calendar": "calendar",
        "major_holders": "institutional holders", "institutional_holders": "institutional holders",
        "mutualfund_holders": "fund holders", "insider_roster_holders": "insiders",
        "insider_transactions": "insider transactions", "insider_purchases": "insider transactions",
        "info": "company profile", "funds_data": "fund",
    }
    _GROUP = {"major_holders": "holders", "institutional_holders": "holders", "mutualfund_holders": "holders",
              "insider_roster_holders": "holders", "insider_transactions": "holders", "insider_purchases": "holders"}

    def __init__(self, factory, budget):
        self._factory = factory  # () -> a new yf.Ticker
        self._t = factory()
        self._b = budget
        self._cache = {}
        self._locks = {}
        self._group_timeout = {}
        self._guard = threading.Lock()

    def _fetch(self, name, label, fn):
        b = self._b

        def attempt(t):
            return (lambda: fn(t)) if fn else (lambda: getattr(t, name))

        try:
            return b.call(label, attempt(self._t), 0.5 * b.per_call)
        except _Skipped as exc:
            if exc.kind != "timeout" or b.remaining() < 2.0:
                raise
        fresh = self._factory()
        self._t = fresh
        return b.call(label, attempt(fresh), 0.75 * b.per_call)

    def get(self, name, fn=None):
        """The property `name` (or fn(ticker) for a composite read)."""
        group = self._GROUP.get(name, name)
        with self._guard:
            lock = self._locks.setdefault(group, threading.Lock())
        label = self.LABELS.get(name, name)
        with lock:
            if name in self._cache:
                kind, value = self._cache[name]
                if kind == "error":
                    raise value
                return value
            if group in self._group_timeout:
                raise _Skipped(label, "timeout")
            try:
                value = self._fetch(name, label, fn)
            except _Skipped as exc:
                if exc.kind == "timeout" and name in self._GROUP:
                    self._group_timeout[group] = True
                raise
            except Exception as exc:
                self._cache[name] = ("error", exc)
                raise
            self._cache[name] = ("value", value)
            return value


# ── Run state ────────────────────────────────────────────────────────────────


class _Section:
    def __init__(self, sid):
        self.id = sid
        self.source = _SOURCES.get(sid, "Yahoo Finance")
        self.state = ""
        self.as_of = ""
        self.ms = 0
        self.count = 0
        self.note = ""
        self.data = {}
        self.failure = ""
        self.failed_labels = []
        self.errors = []
        self.extra = []  # sentences added to the note

    def fail(self, kind, label, exc=None):
        rank = {"timeout": 3, "skipped": 2, "error": 1}
        if rank.get(kind, 0) > rank.get(self.failure, 0):
            self.failure = kind
        if label and label not in self.failed_labels:
            self.failed_labels.append(label)
        if exc is not None:
            self.errors.append(_short_error(exc))

    def status(self):
        return {"state": self.state, "source": self.source, "as_of": self.as_of, "ms": int(self.ms),
                "count": int(self.count), "note": self.note}


class _Run:
    def __init__(self, ticker, wanted):
        self.ticker = ticker
        self.wanted = [s for s in SECTION_IDS[1:] if wanted is None or s in wanted]
        self.budget = _Budget()
        self.fetched_at = srcs.utc_now_iso()
        self.yf = None
        self.stock = None
        self.info = {}
        self.company = {}
        self.quote_type = ""
        self.sections = {}
        self.peer_rows = None
        self.peer_source = ""

    # A Yahoo / SEC call for a section: its value, or None when it failed (recorded on the section).
    def call(self, sec, label, fn, timeout=None):
        try:
            return self.budget.call(label, fn, timeout)
        except _Skipped as exc:
            sec.fail(exc.kind, label)
        except Exception as exc:
            if not _no_data(exc):
                sec.fail("error", label, exc)
        return None

    def attr(self, sec, name, fn=None):
        try:
            return self.stock.get(name, fn)
        except _Skipped as exc:
            sec.fail(exc.kind, exc.label)
        except AttributeError:
            pass  # this yfinance release does not have it
        except Exception as exc:
            if not _no_data(exc):
                sec.fail("error", _TimedTicker.LABELS.get(name, name), exc)
        return None

    def settle(self, sec, has_rows, empty_note=""):
        if sec.failure:
            sec.state = sec.failure
            src = sec.source or "The source"
            if sec.failure == "timeout":
                text = "%s did not answer in time" % src
            elif sec.failure == "skipped":
                text = "Not fetched: the %d s time budget was used up" % self.budget.total
            else:
                text = "%s failed (%s)" % (src, sec.errors[0] if sec.errors else "unknown error")
            sec.note = " ".join([text + "."] + sec.extra).strip()
        elif has_rows:
            sec.state = "ok"
            sec.note = " ".join(sec.extra).strip()
        else:
            sec.state = "empty"
            sec.note = " ".join([empty_note] + sec.extra).strip() if empty_note else " ".join(sec.extra).strip()


def _emit_section(run, sec):
    keys = SECTION_KEYS[sec.id]
    if sec.state in _FAILED:
        data = {k: sec.data[k] for k in keys if k in sec.data}  # what did arrive
    else:
        data = {k: sec.data.get(k, _default(k)) for k in keys}
    _emit({"event": "section", "ticker": run.ticker, "id": sec.id, "status": sec.status(), "data": data})


# ── Profile ──────────────────────────────────────────────────────────────────


def _profile(run):
    """Fetch Ticker.info and fill the profile section. Returns an {"error"} envelope or None."""
    sec = _Section("profile")
    run.sections["profile"] = sec
    t0 = time.monotonic()
    T = run.ticker
    b = run.budget
    try:
        # Everything else needs the profile: a stalled first request gets a second chance.
        info = b.call_retry("company profile", _info_attempts(T), 0.6 * b.per_call, b.per_call)
    except _Skipped:
        return {"error": "Yahoo Finance did not answer in time for %s - try again in a moment" % T}
    except Exception as exc:
        text = _short_error(exc)
        if text == "rate-limited":
            return {"error": "Yahoo Finance is limiting requests right now - try again in a minute"}
        if any(m in text.lower() for m in ("connect", "timed out", "resolve", "network", "curl", "ssl")):
            return {"error": "Could not reach Yahoo Finance for %s: %s" % (T, text)}
        # yfinance fails on its own empty answer for an unknown symbol.
        return {"error": "No market data found for %s - check the symbol" % T}
    sec.ms = (time.monotonic() - t0) * 1000.0
    run.info = info = dict(info)

    company = {
        "ticker": T,
        "name": info.get("longName") or info.get("shortName") or T,
        "sector": info.get("sector") or "",
        "industry": info.get("industry") or "",
        "website": info.get("website") or "",
        "description": (info.get("longBusinessSummary") or "")[:300],
        "employees": _safe_int(info.get("fullTimeEmployees")),
        "country": info.get("country") or "",
        "exchange": info.get("exchange") or "",
        # Yahoo quoteType: EQUITY, ETF, MUTUALFUND, CRYPTOCURRENCY, CURRENCY, INDEX, FUTURE, ...
        "quote_type": _clean(info.get("quoteType")).upper(),
        # Quote currency as Yahoo reports it (minor units kept: "GBp"). Empty when Yahoo
        # gives none - never assumed to be USD.
        "currency": info.get("currency") or "",
        # Currency of the financial statements (revenue, EPS, pay); a GBp-quoted company
        # may report in USD. Empty when unknown.
        "financial_currency": info.get("financialCurrency") or "",
        "financialCurrency": info.get("financialCurrency") or "",
        "market_cap": _safe_float(info.get("marketCap")),
        "current_price": _safe_float(info.get("currentPrice") or info.get("regularMarketPrice")),
        "previous_close": _safe_float(info.get("previousClose")),
        "day_change_pct": _safe_float(info.get("regularMarketChangePercent")),
        "pe_ratio": _safe_float(info.get("trailingPE")),
        "forward_pe": _safe_float(info.get("forwardPE")),
        "price_to_book": _safe_float(info.get("priceToBook")),
        "price_to_sales": _safe_float(info.get("priceToSalesTrailing12Months")),
        "roe": _safe_float(info.get("returnOnEquity")),
        "roa": _safe_float(info.get("returnOnAssets")),
        "revenue_growth": _safe_float(info.get("revenueGrowth")),
        "earnings_growth": _safe_float(info.get("earningsGrowth")),
        "profit_margins": _safe_float(info.get("profitMargins")),
        "revenue": _safe_float(info.get("totalRevenue")),
        "ebitda": _safe_float(info.get("ebitda")),
        "free_cashflow": _safe_float(info.get("freeCashflow")),
        "operating_cashflow": _safe_float(info.get("operatingCashflow")),
        "total_cash": _safe_float(info.get("totalCash")),
        "total_debt": _safe_float(info.get("totalDebt")),
        "insider_percent": _safe_float(info.get("heldPercentInsiders")),
        "institutional_percent": _safe_float(info.get("heldPercentInstitutions")),
        "recommendation": info.get("recommendationKey") or "",
        "recommendation_mean": _safe_float(info.get("recommendationMean")),
        "target_high": _safe_float(info.get("targetHighPrice")),
        "target_low": _safe_float(info.get("targetLowPrice")),
        "target_mean": _safe_float(info.get("targetMeanPrice")),
        "target_median": _safe_float(info.get("targetMedianPrice")),
        "analyst_count": _safe_int(info.get("numberOfAnalystOpinions")),
        "dividend_yield": _safe_float(info.get("dividendYield")),
        "payout_ratio": _safe_float(info.get("payoutRatio")),
        "trailing_eps": _safe_float(info.get("trailingEps")),
        "forward_eps": _safe_float(info.get("forwardEps")),
        "shares_outstanding": _safe_float(info.get("sharesOutstanding")),
        "float_shares": _safe_float(info.get("floatShares")),
    }
    # The profile is the day's (shared cache); price, day change and market cap come from the
    # live quote (fy.quotes: cached per market session, one batched request).
    try:
        quote = b.call("quote", lambda: fy.quotes([T]).get(T), 0.5 * b.per_call)
    except Exception:
        quote = None
    _quote_overlay(company, quote, {"current_price": "regularMarketPrice", "previous_close": "regularMarketPreviousClose",
                                    "day_change_pct": "regularMarketChangePercent", "market_cap": "marketCap"})
    # yfinance answers an unknown / delisted symbol with an empty record. Say so now (same
    # test and wording the host applies) instead of fetching every section for an empty shell.
    if company["current_price"] <= 0 and company["market_cap"] <= 0:
        return {"error": "No market data found for %s - check the symbol" % T}
    run.company = company
    run.quote_type = company["quote_type"]

    data = {"company": company, "governance": {}, "short_interest": {}}
    overall = _safe_int(info.get("overallRisk"))
    if overall > 0:
        data["governance"] = {
            "audit_risk": _safe_int(info.get("auditRisk")),
            "board_risk": _safe_int(info.get("boardRisk")),
            "compensation_risk": _safe_int(info.get("compensationRisk")),
            "shareholder_rights_risk": _safe_int(info.get("shareHolderRightsRisk")),
            "overall_risk": overall,
        }
    data["technicals"] = {
        "fifty_two_week_high": _safe_float(info.get("fiftyTwoWeekHigh")),
        "fifty_two_week_low": _safe_float(info.get("fiftyTwoWeekLow")),
        "fifty_day_avg": _safe_float(info.get("fiftyDayAverage")),
        "two_hundred_day_avg": _safe_float(info.get("twoHundredDayAverage")),
        "beta": _safe_float(info.get("beta")),
        "week52_change_pct": _safe_float(info.get("52WeekChange")),
        "sp500_52wk_change": _safe_float(info.get("SandP52WeekChange")),
        "avg_volume": _safe_int(info.get("averageVolume")),
        "avg_volume_10d": _safe_int(info.get("averageDailyVolume10Day")),
    }
    shares_short = _safe_float(info.get("sharesShort"))
    if shares_short > 0:
        data["short_interest"] = {
            "shares_short": shares_short,
            "short_ratio": _safe_float(info.get("shortRatio")),
            "short_pct_float": _safe_float(info.get("shortPercentOfFloat")),
            "float_shares": _safe_float(info.get("floatShares")),
        }
    data["enterprise"] = {
        "enterprise_value": _safe_float(info.get("enterpriseValue")),
        "ev_to_revenue": _safe_float(info.get("enterpriseToRevenue")),
        "ev_to_ebitda": _safe_float(info.get("enterpriseToEbitda")),
        "peg_ratio": _safe_float(info.get("trailingPegRatio")),
        "price_to_sales": _safe_float(info.get("priceToSalesTrailing12Months")),
        "book_value": _safe_float(info.get("bookValue")),
    }
    data["margins"] = {
        "gross": _safe_float(info.get("grossMargins")),
        "operating": _safe_float(info.get("operatingMargins")),
        "ebitda": _safe_float(info.get("ebitdaMargins")),
        "net": _safe_float(info.get("profitMargins")),
        "debt_to_equity": _safe_float(info.get("debtToEquity")),
        "current_ratio": _safe_float(info.get("currentRatio")),
        "quick_ratio": _safe_float(info.get("quickRatio")),
    }
    sec.data = data
    sec.count = 1
    run.settle(sec, True)
    return None


# ── Sections ─────────────────────────────────────────────────────────────────


def _section_management(run, sec):
    officers = []
    for o in run.info.get("companyOfficers") or []:
        if not isinstance(o, dict):
            continue
        name = _clean(o.get("name"))
        if not name:
            continue
        officers.append({
            "name": name,
            "title": _clean(o.get("title")),
            "total_pay": _safe_int(_safe_float(o.get("totalPay"))),
            "year_born": _safe_int(o.get("yearBorn")),
        })
    sec.data["officers"] = officers
    sec.count = len(officers)
    sec.ms = run.sections["profile"].ms
    run.settle(sec, bool(officers), "Yahoo lists no officers for this company")


def _holder_rows(df, kind, limit=25):
    rows = []
    if df is None or getattr(df, "empty", True):
        return rows
    for _, row in df.head(limit).iterrows():
        name = _clean(row.get("Holder"))
        if not name:
            continue
        # pctHeld (older releases: "% Out") and pctChange are fractions; the contract is percent.
        pct = row.get("pctHeld") if "pctHeld" in row.index else row.get("% Out")
        rows.append({
            "name": name,
            "shares": _safe_float(row.get("Shares")),
            "value": _safe_float(row.get("Value")),
            "percentage": _safe_float(pct) * 100.0,
            "change_percent": _safe_float(row.get("pctChange")) * 100.0,
            "date_reported": _day(row.get("Date Reported")),
            "fund_family": "",
            "type": kind,
        })
    return rows


def _section_holders(run, sec):
    major = run.attr(sec, "major_holders")
    if sec.failure == "timeout":
        run.settle(sec, False)
        return
    inst = run.attr(sec, "institutional_holders")
    mf = run.attr(sec, "mutualfund_holders")

    mh = {}
    if major is not None and not getattr(major, "empty", True):
        vals = {}
        for idx, row in major.iterrows():
            vals[str(idx)] = row.get("Value") if "Value" in row.index else row.iloc[0]
        mh = {
            "insiders_pct": _safe_float(vals.get("insidersPercentHeld")),
            "institutions_pct": _safe_float(vals.get("institutionsPercentHeld")),
            "institutions_float_pct": _safe_float(vals.get("institutionsFloatPercentHeld")),
            "institutions_count": _safe_int(_safe_float(vals.get("institutionsCount"))),
        }
        if not any(mh.values()):
            mh = {}
    inst_rows = _holder_rows(inst, "institutional")
    mf_rows = _holder_rows(mf, "mutualfund")
    sec.data.update({"major_holders": mh, "institutional_holders": inst_rows, "mutualfund_holders": mf_rows})
    sec.count = len(inst_rows) + len(mf_rows)
    dates = [r["date_reported"] for r in inst_rows + mf_rows if r["date_reported"]]
    sec.as_of = max(dates) if dates else ""
    if mh and not inst_rows and not mf_rows and not sec.failure:
        sec.extra.append("Yahoo lists no institutional holders for this market.")
    run.settle(sec, bool(mh or inst_rows or mf_rows), "Yahoo lists no holders for this company")


_RE_PRICE = re.compile(r"at price\s+([0-9][0-9,]*\.?[0-9]*)(?:\s*-\s*([0-9][0-9,]*\.?[0-9]*))?", re.I)


def _trade_kind(text, transaction=""):
    t = ("%s %s" % (text or "", transaction or "")).lower()
    if re.search(r"\bgift\b", t):
        return "gift"
    if re.search(r"\baward\b|\bgrant\b", t):
        return "grant"
    if "conversion of exercise" in t or re.search(r"\bexercise\b|\boption\b", t):
        return "option"
    if re.search(r"\bsale\b|\bsell\b|\bsold\b", t):
        return "sell"
    if re.search(r"\bpurchase\b|\bbuy\b|\bbought\b", t):
        return "buy"
    return "other"


def _roster_kind(text):
    t = (text or "").lower()
    if "purchase" in t or "buy" in t:
        return "buy"
    if "sale" in t or "sell" in t:
        return "sell"
    return text  # e.g. "Stock Gift", "Conversion of Exercise of derivative security"


def _trade_price(text, shares, value):
    if shares > 0 and value > 0:
        return value / shares
    m = _RE_PRICE.search(text or "")
    if not m:
        return 0.0
    lo = _safe_float(m.group(1).replace(",", ""))
    hi = _safe_float(m.group(2).replace(",", "")) if m.group(2) else lo
    return (lo + hi) / 2.0


def _section_insiders(run, sec):
    roster = run.attr(sec, "insider_roster_holders")
    # Older yfinance releases call the roster `insider_holders` (checked on the class: hasattr on
    # the instance would fetch it).
    if roster is None and sec.failure != "timeout" and hasattr(type(run.stock._t), "insider_holders"):
        roster = run.attr(sec, "insider_holders")
    if sec.failure == "timeout":
        run.settle(sec, False)
        return
    trades_df = run.attr(sec, "insider_transactions")
    purchases = run.attr(sec, "insider_purchases")

    co = run.company
    shares_out = co.get("shares_outstanding") or 0.0
    quote_ccy = co.get("currency") or ""
    price_now = co.get("current_price") or 0.0

    holders = []
    if roster is not None and not getattr(roster, "empty", True):
        for _, row in roster.head(40).iterrows():
            name = _clean(row.get("Name", row.get("Insider")))
            if not name:
                continue
            shares = _safe_float(row.get("Shares Owned Directly", row.get("Shares")))
            if shares <= 0:
                shares = _safe_float(row.get("Shares Owned Indirectly"))
            if shares <= 0:
                shares = _safe_float(row.get("positionSummary"))
            text = _clean(row.get("Most Recent Transaction"))
            holders.append({
                "name": name,
                "title": _clean(row.get("Position", row.get("Relation"))),
                "shares": shares,
                "percentage": (shares / shares_out * 100.0) if shares_out > 0 else 0.0,
                "last_transaction": _roster_kind(text),
                "last_transaction_text": text,
                "last_date": _day(row.get("Latest Transaction Date")),
            })

    trades = []
    if trades_df is not None and not getattr(trades_df, "empty", True):
        df = trades_df
        if "Start Date" in df.columns:
            df = df.sort_values("Start Date", ascending=False, kind="stable")
        for _, row in df.head(60).iterrows():
            date = _day(row.get("Start Date"))
            if not date:
                continue
            text = _clean(row.get("Text"))
            transaction = _clean(row.get("Transaction"))
            shares = _safe_float(row.get("Shares"))
            value = _safe_float(row.get("Value"))
            price = _trade_price(text, shares, value)
            # Yahoo states no currency. The quote currency only when the price is plausible in
            # it; non-US listings often carry USD values (Reliance at "15.69").
            ccy = ""
            if quote_ccy and price > 0 and price_now > 0 and 0.4 <= price / price_now <= 2.5:
                ccy = quote_ccy
            trades.append({
                "date": date,
                "insider": _clean(row.get("Insider")),
                "position": _clean(row.get("Position")),
                "text": text or transaction,
                "kind": _trade_kind(text, transaction),
                "shares": shares,
                "value": value,
                "price": round(price, 4),
                "currency": ccy,
                "ownership": _clean(row.get("Ownership")),
            })

    summary = {}
    if purchases is not None and not getattr(purchases, "empty", True):
        label_col = purchases.columns[0]
        window = str(label_col).replace("Insider Purchases Last", "").strip() or "6m"
        rows = {}
        for _, row in purchases.iterrows():
            rows[_clean(row.get(label_col))] = (_safe_float(row.get("Shares")), _safe_int(row.get("Trans")))
        if any(v[0] or v[1] for v in rows.values()):
            summary = {
                "window": window,
                "purchases_shares": rows.get("Purchases", (0.0, 0))[0],
                "purchases_count": rows.get("Purchases", (0.0, 0))[1],
                "sales_shares": rows.get("Sales", (0.0, 0))[0],
                "sales_count": rows.get("Sales", (0.0, 0))[1],
                "net_shares": rows.get("Net Shares Purchased (Sold)", (0.0, 0))[0],
                "net_pct": rows.get("% Net Shares Purchased (Sold)", (0.0, 0))[0],
                "total_insider_shares": rows.get("Total Insider Shares Held", (0.0, 0))[0],
            }
    if summary or trades:
        since = (datetime.now(timezone.utc) - timedelta(days=182)).strftime("%Y-%m-%d")
        recent = [t for t in trades if t["date"] >= since and t["currency"]]
        if not summary:
            summary = {"window": "6m", "purchases_shares": 0.0, "purchases_count": 0, "sales_shares": 0.0,
                       "sales_count": 0, "net_shares": 0.0, "net_pct": 0.0, "total_insider_shares": 0.0}
        summary["buy_value"] = sum((t["value"] for t in recent if t["kind"] == "buy"), 0.0)
        summary["sell_value"] = sum((t["value"] for t in recent if t["kind"] == "sell"), 0.0)
        summary["currency"] = recent[0]["currency"] if recent else ""

    sec.data.update({"insider_holders": holders, "insider_transactions": trades, "insider_summary": summary})
    sec.count = len(holders) if holders else len(trades)
    dates = [t["date"] for t in trades] + [h["last_date"] for h in holders if h["last_date"]]
    sec.as_of = max(dates) if dates else ""
    if trades and not any(t["currency"] for t in trades if t["price"] > 0):
        if any(t["price"] > 0 for t in trades):
            sec.extra.append("Yahoo states no currency for these transactions and the prices do not match "
                             "the quote currency, so amounts are shown without one.")
    run.settle(sec, bool(holders or trades or summary), "Yahoo lists no insiders for this company")


def _section_analysts(run, sec):
    apt = run.attr(sec, "analyst_price_targets")
    rec = run.attr(sec, "recommendations_summary")
    ud = run.attr(sec, "upgrades_downgrades")

    targets = {}
    if isinstance(apt, dict) and apt:
        targets = {k: _safe_float(apt.get(k)) for k in ("current", "high", "low", "mean", "median")}
    recs = []
    if rec is not None and not getattr(rec, "empty", True):
        for _, row in rec.iterrows():
            recs.append({
                "period": _safe_str(row.get("period")),
                "strong_buy": _safe_int(row.get("strongBuy")),
                "buy": _safe_int(row.get("buy")),
                "hold": _safe_int(row.get("hold")),
                "sell": _safe_int(row.get("sell")),
                "strong_sell": _safe_int(row.get("strongSell")),
            })
    ups = []
    if ud is not None and not getattr(ud, "empty", True):
        for idx, row in ud.head(12).iterrows():
            ups.append({
                "date": _day(idx) or _safe_str(idx)[:10],
                "firm": _clean(row.get("Firm")),
                "to_grade": _clean(row.get("ToGrade")),
                "from_grade": _clean(row.get("FromGrade")),
                "action": _clean(row.get("Action")),
                "price_target": _safe_float(row.get("currentPriceTarget")),
                "prior_target": _safe_float(row.get("priorPriceTarget")),
            })
    sec.data.update({"analyst_targets": targets, "recommendations_summary": recs, "upgrades_downgrades": ups})
    has_cover = (targets.get("mean", 0) > 0 or any(r["strong_buy"] + r["buy"] + r["hold"] + r["sell"] + r["strong_sell"]
                                                   for r in recs) or bool(ups))
    sec.count = run.company.get("analyst_count") or len(ups)
    sec.as_of = max((u["date"] for u in ups if u["date"]), default="")
    run.settle(sec, has_cover, "No analyst coverage on Yahoo Finance")


def _section_calendar(run, sec):
    import equity_research_data as erd

    # The calendar Equity Research reads too (a day, shared): dates come back as YYYY-MM-DD.
    cal = run.attr(sec, "calendar", lambda t: erd.calendar(run.ticker, t))
    out = {}
    if isinstance(cal, dict) and cal:
        ed = cal.get("Earnings Date")
        if isinstance(ed, (list, tuple)):
            ed = ed[0] if ed else None
        out = {
            "earnings_date": _day(ed),
            "earnings_avg": _safe_float(cal.get("Earnings Average")),
            "earnings_low": _safe_float(cal.get("Earnings Low")),
            "earnings_high": _safe_float(cal.get("Earnings High")),
            "revenue_avg": _safe_float(cal.get("Revenue Average")),
            "revenue_low": _safe_float(cal.get("Revenue Low")),
            "revenue_high": _safe_float(cal.get("Revenue High")),
            "ex_dividend_date": _day(cal.get("Ex-Dividend Date")),
            "dividend_date": _day(cal.get("Dividend Date")),
        }
    sec.data["calendar"] = out
    sec.count = sum(1 for k in ("earnings_date", "ex_dividend_date", "dividend_date") if out.get(k))
    run.settle(sec, sec.count > 0, "Yahoo lists no upcoming earnings or dividend dates")


def _peer_symbols(run, sec):
    """(symbols, peer_source): Yahoo's industry screen (the Equity Research peers service), else
    the curated table (industry match before sector), else the flagged mega-cap fallback for a
    US listing, else nothing."""
    import equity_research_data as erd

    co = run.company
    syms = []
    # One composite call (a profile look-up, screens, the industry's leaders): a longer limit.
    res = run.call(sec, "peers", lambda: erd.get_peers_suggest(run.ticker, 8), timeout=2 * run.budget.per_call)
    if isinstance(res, dict):
        if res.get("error"):
            sec.fail("error", "peers", Exception(res.get("error")))
        else:
            syms = [str(s).upper() for s in res.get("peers") or [] if s and str(s).upper() != run.ticker]
    if syms:
        return syms[:8], "screen:industry"
    curated, key = srcs.curated_peers(co.get("industry"), co.get("sector"), run.ticker)
    if curated:
        if sec.failure:
            sec.extra.append("Yahoo's industry screen did not answer; the curated %s list is shown." % key)
        return curated[:8], "industry:" + key
    if not sec.failure and srcs.is_us_listing(run.ticker):
        return [p for p in srcs.GENERIC_PEERS if p != run.ticker][:5], "generic_fallback"
    return [], "none"


def _peer_row(pt, pi):
    return {
        "ticker": pt,
        "name": pi.get("longName") or pi.get("shortName") or pt,
        "market_cap": _safe_float(pi.get("marketCap")),
        "pe_ratio": _safe_float(pi.get("trailingPE")),
        "forward_pe": _safe_float(pi.get("forwardPE")),
        "roe": _safe_float(pi.get("returnOnEquity")),
        "revenue_growth": _safe_float(pi.get("revenueGrowth")),
        "profit_margins": _safe_float(pi.get("profitMargins")),
        "gross_margins": _safe_float(pi.get("grossMargins")),
        "operating_margins": _safe_float(pi.get("operatingMargins")),
        "revenue": _safe_float(pi.get("totalRevenue")),
        "current_price": _safe_float(pi.get("currentPrice") or pi.get("regularMarketPrice")),
        "sector": pi.get("sector") or "",
        "industry": pi.get("industry") or "",
        "country": pi.get("country") or "",
        "beta": _safe_float(pi.get("beta")),
        "ev_to_ebitda": _safe_float(pi.get("enterpriseToEbitda")),
        "price_to_book": _safe_float(pi.get("priceToBook")),
        "week52_change": _safe_float(pi.get("52WeekChange")),
        "recommendation": pi.get("recommendationKey") or "",
        "currency": pi.get("currency") or "",
        "financial_currency": pi.get("financialCurrency") or "",
    }


def _section_peers(run, sec):
    symbols, source = _peer_symbols(run, sec)

    half = 0.5 * run.budget.per_call

    def one(pt):
        try:
            return pt, run.budget.call_retry("peer " + pt, _info_attempts(pt), half, half), ""
        except _Skipped as exc:
            return pt, None, exc.kind
        except Exception:
            return pt, None, "error"

    rows, missed, kinds = [], [], []
    if symbols:
        # Every peer's live price / market cap in one batched quote request (profiles are a day old).
        try:
            quotes = run.budget.call("peer quotes", lambda: fy.quotes(symbols), half) or {}
        except Exception:
            quotes = {}
        with concurrent.futures.ThreadPoolExecutor(max_workers=min(6, len(symbols)),
                                                   thread_name_prefix="relmap-peer") as pool:
            for pt, pi, kind in pool.map(one, symbols):
                if pi is None:
                    if kind in ("timeout", "skipped"):
                        missed.append(pt)
                        kinds.append(kind)
                    continue
                row = _peer_row(pt, pi)
                _quote_overlay(row, quotes.get(pt), {"current_price": "regularMarketPrice", "market_cap": "marketCap"})
                if row["current_price"] > 0 or row["market_cap"] > 0:
                    rows.append(row)
    if missed:
        if rows:
            sec.extra.append("No answer in time for %d of %d peers (%s)." % (len(missed), len(symbols), ", ".join(missed)))
        else:
            sec.fail("timeout" if "timeout" in kinds else "skipped", "peers")
    run.peer_rows = rows
    run.peer_source = source if rows else "none"
    sec.data.update({"peers": rows, "peer_source": run.peer_source})
    sec.count = len(rows)
    if source == "generic_fallback" and rows:
        sec.extra.append("No comparable companies found; large caps shown for context only.")
    run.settle(sec, bool(rows), "Yahoo found no comparable companies")


def _section_correlation(run, sec):
    co = run.company
    entries = [(run.ticker, "self", co.get("name") or run.ticker)]
    if run.quote_type not in _FUNDS:
        peers, source = run.peer_rows, run.peer_source
        if peers is None:
            # A retry of the correlation alone: the peer list without the peers' profiles.
            scratch = _Section("peers")
            syms, source = _peer_symbols(run, scratch)
            peers = [{"ticker": s, "name": s} for s in syms]
        if source != "generic_fallback":  # mega-caps for context are not peers
            entries += [(p["ticker"], "peer", p.get("name") or p["ticker"]) for p in peers]
        etf = srcs.sector_etf(run.ticker, co.get("sector"), co.get("currency"))
        if etf:
            entries.append((etf[0], "sector_etf", etf[1]))
    idx = srcs.benchmark_index(run.ticker)
    entries.append((idx[0], "index", idx[1]))
    seen, uniq = set(), []
    for e in entries:
        if e[0].upper() not in seen:
            seen.add(e[0].upper())
            uniq.append((e[0].upper(), e[1], e[2]))
    symbols = [e[0] for e in uniq]
    if len(symbols) < 2:
        run.settle(sec, False, "Nothing to compare this instrument with")
        return

    # The shared bar cache: the index and sector ETF (and any symbol a screen already loaded)
    # cost nothing; a cold symbol is one request. Cold downloads run one after another, so the
    # call gets twice the per-call limit.
    df = run.call(sec, "correlation", lambda: fy.download(symbols, period="1y", interval="1d", auto_adjust=True,
                                                          group_by="column", max_age=12 * 3600),
                  timeout=2 * run.budget.per_call)
    corr = None
    if df is not None:
        corr = srcs.compute_correlation(srcs.close_frame(df, symbols), uniq)
    if corr:
        sec.data["correlation"] = corr
        sec.count = len(corr["symbols"])
        sec.as_of = corr["as_of"]
        lost = [s for s in symbols if s not in corr["symbols"]]
        if lost:
            sec.extra.append("No price history for %s." % ", ".join(lost))
    run.settle(sec, bool(corr and len(corr["symbols"]) >= 2), "Yahoo returned no price history to compare")


def _section_fund(run, sec):
    info = run.info

    def load(ticker):
        fd = ticker.funds_data
        out = {}
        for name in ("top_holdings", "sector_weightings", "asset_classes", "fund_overview"):
            try:
                out[name] = getattr(fd, name)
            except Exception as exc:
                if not _no_data(exc):
                    raise
                out[name] = None
        return out

    fd = run.attr(sec, "funds_data", load) or {}
    overview = fd.get("fund_overview") if isinstance(fd.get("fund_overview"), dict) else {}
    holdings = []
    th = fd.get("top_holdings")
    if th is not None and not getattr(th, "empty", True):
        for sym, row in th.iterrows():
            holdings.append({"symbol": _clean(sym).upper(), "name": _clean(row.get("Name")),
                             "weight": _safe_float(row.get("Holding Percent"))})
    weights = []
    sw = fd.get("sector_weightings")
    if isinstance(sw, dict):
        weights = [{"sector": str(k), "weight": _safe_float(v)} for k, v in sw.items() if _safe_float(v) > 0]
        weights.sort(key=lambda w: w["weight"], reverse=True)
    classes = {}
    ac = fd.get("asset_classes")
    if isinstance(ac, dict):
        classes = {str(k): _safe_float(v) for k, v in ac.items()}
    fund = {
        "family": info.get("fundFamily") or overview.get("family") or "",
        "category": info.get("category") or overview.get("categoryName") or "",
        "legal_type": info.get("legalType") or overview.get("legalType") or "",
        "total_assets": _safe_float(info.get("totalAssets") or info.get("netAssets")),
        "holdings": holdings,
        "sector_weights": weights,
        "asset_classes": classes,
    }
    sec.data["fund"] = fund
    sec.count = len(holdings)
    run.settle(sec, bool(holdings or weights), "Yahoo lists no holdings for this fund")


def _section_subsidiaries(run, sec):
    res = srcs.fetch_subsidiaries(run.ticker, run.budget.call, run.budget.per_call)
    if res["state"] == "not_applicable":
        sec.state = "not_applicable"
        sec.note = res["note"]
        return
    sec.data.update({"subsidiaries": res["subsidiaries"], "subsidiaries_filing": res["filing"]})
    sec.count = len(res["subsidiaries"])
    sec.as_of = res["as_of"]
    if res["state"] == "ok" and res["note"]:
        sec.extra.append(res["note"])
    run.settle(sec, bool(res["subsidiaries"]), res["note"] if res["state"] != "ok" else "")


_RUNNERS = {
    "holders": _section_holders,
    "insiders": _section_insiders,
    "analysts": _section_analysts,
    "calendar": _section_calendar,
    "peers": _section_peers,
    "correlation": _section_correlation,
    "fund": _section_fund,
    "subsidiaries": _section_subsidiaries,
}

# Start order: the slow ones first (peers also gates the correlation).
_NETWORK_ORDER = ("peers", "subsidiaries", "holders", "insiders", "analysts", "calendar", "fund", "correlation")


class _Incomplete(Exception):
    """The section did not complete (a call failed / timed out): its outcome is not cached."""


def _run_cached(run, sec, runner):
    """runner(run, sec), its complete outcome kept per (ticker, section) for every process: a day
    when it has rows, an hour when it is empty (yfinance answers a failed holders / analysts
    request with empty frames, so "empty" may be a hidden failure); a section that failed in part
    is shown as it is and not kept."""
    def fetch():
        runner(run, sec)
        if sec.failure:
            raise _Incomplete(sec.id)
        kept = {"data": sec.data, "count": sec.count, "as_of": sec.as_of, "state": sec.state, "note": sec.note}
        return fy.Partial(kept, 3600) if sec.state == "empty" else kept

    try:
        kept = fy.cached("relmap_" + sec.id, run.ticker, fetch, max_age=_SECTION_TTL_S, stale_on_error=False)
    except _Incomplete:
        return
    if isinstance(kept, dict) and kept.get("state") in ("ok", "empty"):
        sec.data = dict(kept.get("data") or {})
        sec.count = _safe_int(kept.get("count"))
        sec.as_of = kept.get("as_of") or ""
        sec.state = kept["state"]
        sec.note = kept.get("note") or ""


def _run_section(run, sid):
    sec = run.sections[sid]
    t0 = time.monotonic()
    try:
        if sid in _CACHED_SECTIONS:
            _run_cached(run, sec, _RUNNERS[sid])
        else:
            _RUNNERS[sid](run, sec)
    except _Skipped as exc:
        sec.fail(exc.kind, exc.label)
    except Exception as exc:
        sec.fail("error", sid, exc)
    finally:
        sec.ms = (time.monotonic() - t0) * 1000.0
        if not sec.state or (sec.failure and sec.state not in _FAILED):
            run.settle(sec, sec.count > 0)
    return sid


def _not_applicable(run, sid):
    """The plain sentence why `sid` does not apply to this instrument, or ""."""
    qt = run.quote_type
    if qt in _NO_RELATIONSHIPS:
        return "Not applicable to %s" % _NO_RELATIONSHIPS[qt]
    if qt in _FUNDS:
        if sid in _NOT_FOR_FUNDS:
            return "Not applicable to %s" % _FUNDS[qt]
        return ""
    if sid == "fund":
        return "Holdings apply to funds only"
    if sid == "subsidiaries" and not srcs.is_us_listing(run.ticker):
        return srcs.NOT_US_FILER_NOTE
    return ""


def _run_network(run, ids):
    if not ids:
        return
    chain = "correlation" in ids and "peers" in ids
    pool = concurrent.futures.ThreadPoolExecutor(max_workers=4, thread_name_prefix="relmap-section")
    pending = {}

    def submit(sid):
        pending[pool.submit(_run_section, run, sid)] = sid

    for sid in _NETWORK_ORDER:
        if sid in ids and not (sid == "correlation" and chain):
            submit(sid)
    try:
        while pending:
            done, _ = concurrent.futures.wait(list(pending), timeout=max(1.0, run.budget.remaining() + 20.0),
                                              return_when=concurrent.futures.FIRST_COMPLETED)
            if not done:
                break
            for fut in done:
                sid = pending.pop(fut)
                _emit_section(run, run.sections[sid])
                if sid == "peers" and chain:
                    submit("correlation")
    finally:
        for fut, sid in list(pending.items()):
            sec = run.sections[sid]
            if not sec.state:
                sec.fail("timeout", sid)
                run.settle(sec, False)
                _emit_section(run, sec)
        pool.shutdown(wait=False, cancel_futures=True)


def _missing_labels(sec):
    labels = _SECTION_LABELS.get(sec.id)
    if labels is None:
        return [sec.id]
    if not any(sec.data.get(k) for k in SECTION_KEYS[sec.id]):
        return list(labels)  # nothing arrived: every part of the section is missing
    picked = [lab for lab in sec.failed_labels if lab in labels]
    return picked or list(labels)


def _assemble(run):
    ids = ["profile"] + run.wanted
    out = {"schema": SCHEMA, "fetched_at": run.fetched_at}
    for sid in ids:
        sec = run.sections.get(sid)
        for key in SECTION_KEYS[sid]:
            out[key] = sec.data.get(key, _default(key)) if sec else _default(key)
    out["sections"] = {sid: run.sections[sid].status() for sid in ids if sid in run.sections}

    failed = [sid for sid in ids if sid in run.sections and run.sections[sid].state in _FAILED]
    missing = []
    for sid in failed:
        for lab in _missing_labels(run.sections[sid]):
            if lab not in missing:
                missing.append(lab)
    out["partial"] = bool(failed)
    out["missing_sections"] = missing
    if failed:
        by_state = {}
        for sid in failed:
            by_state.setdefault(run.sections[sid].state, []).append(sid)
        parts = []
        if by_state.get("timeout"):
            parts.append("no answer in time for " + ", ".join(by_state["timeout"]))
        if by_state.get("skipped"):
            parts.append("not fetched (the %d s time budget was used up): %s" % (run.budget.total,
                                                                                ", ".join(by_state["skipped"])))
        if by_state.get("error"):
            parts.append("failed: " + ", ".join(by_state["error"]))
        out["note"] = "Some sections did not load - " + "; ".join(parts)
    applicable = [sid for sid in ids if sid in run.sections
                  and run.sections[sid].state not in ("not_applicable", "unavailable")]
    ok = [sid for sid in applicable if run.sections[sid].state == "ok"]
    out["data_quality"] = int(round(100.0 * len(ok) / len(applicable))) if applicable else 0
    out["elapsed_s"] = round(time.monotonic() - run.budget.start, 1)
    return out


def fetch_relationship_map(ticker, wanted=None):
    run = _Run(ticker, wanted)
    _emit({"event": "plan", "ticker": ticker, "sections": ["profile"] + run.wanted})
    try:
        yf = fy.yf()
    except ImportError:
        return {"error": "yfinance is not installed in the terminal's Python environment"}
    run.yf = yf
    run.stock = _TimedTicker(lambda: yf.Ticker(ticker), run.budget)

    err = _profile(run)
    if err:
        return err
    _emit_section(run, run.sections["profile"])

    network = []
    for sid in run.wanted:
        sec = _Section(sid)
        run.sections[sid] = sec
        reason = _not_applicable(run, sid)
        if reason:
            sec.state, sec.note = "not_applicable", reason
            _emit_section(run, sec)
        elif sid == "management":
            _section_management(run, sec)
            _emit_section(run, sec)
        elif sid == "supply_chain":
            # No free source names customers or suppliers; never invent them.
            sec.state, sec.note = "unavailable", SUPPLY_CHAIN_NOTE
            sec.data["supply_chain"] = []
            _emit_section(run, sec)
        elif sid == "calendar" and run.quote_type in _FUNDS:
            # Yahoo answers a fund's calendar with HTTP 404, and any 4xx makes yfinance renew its
            # cookie and crumb under a global lock, stalling the requests running beside it.
            sec.state, sec.note = "unavailable", "Yahoo Finance has no earnings or dividend calendar for funds"
            sec.data["calendar"] = {}
            _emit_section(run, sec)
        else:
            network.append(sid)
    _run_network(run, network)
    return _assemble(run)


def _parse_args(argv):
    """(TICKER, [section ids] or None)."""
    if len(argv) < 2 or argv[1].startswith("--"):
        return "", None
    ticker = argv[1].strip().upper()
    raw = None
    rest = argv[2:]
    i = 0
    while i < len(rest):
        arg = rest[i]
        if arg == "--sections" and i + 1 < len(rest):
            raw = rest[i + 1]
            i += 2
            continue
        if arg.startswith("--sections="):
            raw = arg.split("=", 1)[1]
        i += 1
    if raw is None:
        return ticker, None
    return ticker, [s.strip().lower() for s in raw.split(",") if s.strip() and s.strip().lower() != "profile"]


if __name__ == "__main__":
    ticker, wanted = _parse_args(sys.argv)
    if not ticker:
        sys.stdout.write(fincept_json.dumps({"error": "Usage: relationship_map.py <TICKER> [--sections id,id]"}) + "\n")
        sys.exit(1)

    _STREAM = sys.stderr
    # yfinance prints some failures to stdout, which carries exactly one JSON line. A call
    # abandoned by the time budget keeps running in a daemon thread, so the redirect stays on
    # until the process ends and the JSON goes straight to the real stdout.
    real_stdout = sys.stdout
    with fincept_json.stdout_to_stderr():
        try:
            data = fetch_relationship_map(ticker, wanted)
        except Exception as exc:  # a bug, not a data gap: still one JSON line
            data = {"error": "The relationship map could not be built: %s" % _short_error(exc)}
        real_stdout.write(fincept_json.dumps(data, ensure_ascii=True) + "\n")
        real_stdout.flush()
        try:
            sys.stderr.flush()
        except Exception:
            pass
        # Abandoned calls (daemon threads, peer-screen pools) must not hold the process open.
        os._exit(0)
