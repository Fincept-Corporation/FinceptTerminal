"""SEC EDGAR company facts -> a small financial snapshot of one US filer.

The terminal's company snapshot (src/services/company_snapshot/CompanySnapshotService) asks Yahoo
Finance first and uses this script only to fill what Yahoo did not give, for companies that file
with the SEC. Every figure is a value the company reported in XBRL (us-gaap / dei taxonomies); the
only derived figures are labelled: last-twelve-months flows (latest annual + current year-to-date
- prior year-to-date), EBITDA = operating income + D&A of the same period, the effective tax rate
(income tax / pre-tax income) and total debt (the reported debt tags added without overlap). A
figure the filings do not carry is left out, never estimated.

CLI:
    python sec_company_facts.py snapshot AAPL
    -> {"success": true, "data": {"symbol", "cik", "name", "currency": "USD", "ltm_label",
         "fields": {"revenue_ltm": {"value", "period_end", "label", "tags": [...]}, ...},
         "missing": [...], "as_of"}}
    python sec_company_facts.py selftest     (offline checks of the period arithmetic)

SEC fair-access policy: a User-Agent that identifies the requester and at most 10 requests per
second. This script sends "Fincept Terminal support@fincept.in", spaces its own requests at least
125 ms apart (<= 8 per second) and makes two requests for an uncached company (ticker map, company
facts). The ticker map is cached for 7 days and each company's extracted snapshot for 12 hours in
<FINCEPT_DATA_DIR>/sec_cache/.
"""

from __future__ import annotations

import datetime as _dt
import json
import os
import sys
import time
import urllib.error
import urllib.request

_SCRIPTS_ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "..", ".."))
if _SCRIPTS_ROOT not in sys.path:
    sys.path.insert(0, _SCRIPTS_ROOT)

import fincept_json  # noqa: E402

USER_AGENT = "Fincept Terminal support@fincept.in"
TICKERS_URL = "https://www.sec.gov/files/company_tickers.json"
FACTS_URL = "https://data.sec.gov/api/xbrl/companyfacts/CIK{cik:010d}.json"
MIN_INTERVAL_S = 0.125  # <= 8 requests per second
TICKER_MAP_TTL_S = 7 * 24 * 3600
SNAPSHOT_TTL_S = 12 * 3600
TIMEOUT_S = 25

_last_request = [0.0]


def _cache_dir() -> str:
    # <FINCEPT_DATA_DIR>/sec_cache; run by hand, under the edition's default data root.
    import fincept_paths
    d = os.path.join(str(fincept_paths.data_root()), "sec_cache")
    os.makedirs(d, exist_ok=True)
    return d


def _get_json(url: str):
    wait = MIN_INTERVAL_S - (time.monotonic() - _last_request[0])
    if wait > 0:
        time.sleep(wait)
    req = urllib.request.Request(url, headers={"User-Agent": USER_AGENT, "Accept-Encoding": "identity",
                                               "Accept": "application/json"})
    try:
        with urllib.request.urlopen(req, timeout=TIMEOUT_S) as resp:
            body = resp.read()
    finally:
        _last_request[0] = time.monotonic()
    return json.loads(body.decode("utf-8"))


def _read_cache(name: str, ttl_s: int):
    path = os.path.join(_cache_dir(), name)
    try:
        if time.time() - os.path.getmtime(path) > ttl_s:
            return None
        with open(path, "r", encoding="utf-8") as f:
            return json.load(f)
    except (OSError, ValueError):
        return None


def _write_cache(name: str, obj) -> None:
    path = os.path.join(_cache_dir(), name)
    tmp = path + ".tmp"
    try:
        with open(tmp, "w", encoding="utf-8") as f:
            json.dump(obj, f)
        os.replace(tmp, path)
    except OSError:
        pass


def lookup_cik(ticker: str):
    """(cik, title) for a ticker from SEC's ticker map, or (None, None)."""
    tmap = _read_cache("company_tickers.json", TICKER_MAP_TTL_S)
    if tmap is None:
        tmap = _get_json(TICKERS_URL)
        _write_cache("company_tickers.json", tmap)
    want = ticker.upper().replace(".", "-")
    for row in tmap.values():
        t = str(row.get("ticker", "")).upper().replace(".", "-")
        if t == want:
            return int(row["cik_str"]), row.get("title", "")
    return None, None


# ── Period arithmetic ─────────────────────────────────────────────────────────

def _date(s: str):
    return _dt.date.fromisoformat(s)


def _duration_days(f) -> int:
    return (_date(f["end"]) - _date(f["start"])).days


def _usd_facts(facts: dict, tag: str, unit: str = "USD"):
    node = facts.get("us-gaap", {}).get(tag)
    if not node:
        return []
    return [f for f in node.get("units", {}).get(unit, []) if "end" in f and "val" in f]


def _dedupe(rows):
    """One value per (start, end): the latest filed."""
    best = {}
    for f in rows:
        key = (f.get("start"), f["end"])
        if key not in best or str(f.get("filed", "")) > str(best[key].get("filed", "")):
            best[key] = f
    return list(best.values())


def ltm_flow(rows):
    """Last-twelve-months value of a flow fact.

    Returns (value, period_end, label) or None. The latest annual (350-380 day) value is used; when a
    later year-to-date value of the next fiscal year exists together with the same-length year-to-date
    value one year earlier, LTM = annual + current YTD - prior YTD.
    """
    rows = _dedupe([f for f in rows if "start" in f])
    annual = [f for f in rows if 350 <= _duration_days(f) <= 380]
    if not annual:
        return None
    annual.sort(key=lambda f: f["end"])
    fy = annual[-1]
    fy_end = _date(fy["end"])
    ytd = [f for f in rows if 80 <= _duration_days(f) <= 300 and _date(f["start"]) > fy_end
           and (_date(f["start"]) - fy_end).days <= 7]
    if ytd:
        ytd.sort(key=lambda f: f["end"])
        cur = ytd[-1]
        dur = _duration_days(cur)
        cur_end = _date(cur["end"])
        prior = [f for f in rows if abs(_duration_days(f) - dur) <= 7
                 and abs((cur_end - _date(f["end"])).days - 365) <= 10]
        if prior:
            prior.sort(key=lambda f: f["end"])
            p = prior[-1]
            value = float(fy["val"]) + float(cur["val"]) - float(p["val"])
            return value, cur["end"], "TTM to " + cur["end"]
    return float(fy["val"]), fy["end"], "FY ending " + fy["end"]


def latest_instant(rows):
    """(value, end) of the latest instant (balance-sheet) fact, or None."""
    inst = [f for f in rows if "start" not in f]
    if not inst:
        return None
    inst.sort(key=lambda f: (f["end"], str(f.get("filed", ""))))
    f = inst[-1]
    return float(f["val"]), f["end"]


def _first_flow(facts, tags, unit="USD"):
    for tag in tags:
        r = ltm_flow(_usd_facts(facts, tag, unit))
        if r is not None:
            return r + (tag,)
    return None


def _first_instant(facts, tags, unit="USD"):
    for tag in tags:
        r = latest_instant(_usd_facts(facts, tag, unit))
        if r is not None:
            return r + (tag,)
    return None


REVENUE_TAGS = ["RevenueFromContractWithCustomerExcludingAssessedTax", "Revenues", "SalesRevenueNet",
                "RevenueFromContractWithCustomerIncludingAssessedTax"]
DA_TAGS = ["DepreciationDepletionAndAmortization", "DepreciationAndAmortization",
           "DepreciationAmortizationAndAccretionNet", "Depreciation"]
PRETAX_TAGS = ["IncomeLossFromContinuingOperationsBeforeIncomeTaxesExtraordinaryItemsNoncontrollingInterest",
               "IncomeLossFromContinuingOperationsBeforeIncomeTaxesMinorityInterestAndIncomeLossFromEquityMethodInvestments"]


def build_snapshot(facts: dict) -> dict:
    fields = {}

    def put(key, value, period_end, label, tags):
        fields[key] = {"value": value, "period_end": period_end, "label": label, "tags": tags}

    ltm_label = ""
    rev = _first_flow(facts, REVENUE_TAGS)
    if rev:
        put("revenue_ltm", rev[0], rev[1], rev[2], [rev[3]])
        ltm_label = rev[2]
    ebit = _first_flow(facts, ["OperatingIncomeLoss"])
    if ebit:
        put("ebit_ltm", ebit[0], ebit[1], ebit[2], [ebit[3]])
        ltm_label = ltm_label or ebit[2]
    ni = _first_flow(facts, ["NetIncomeLoss", "ProfitLoss"])
    if ni:
        put("net_income_ltm", ni[0], ni[1], ni[2], [ni[3]])
    da = _first_flow(facts, DA_TAGS)
    if da:
        put("depreciation_ltm", da[0], da[1], da[2], [da[3]])
    if ebit and da and ebit[1] == da[1]:
        put("ebitda_ltm", ebit[0] + da[0], ebit[1], ebit[2] + " (operating income + D&A)", [ebit[3], da[3]])
    intr = _first_flow(facts, ["InterestExpense", "InterestExpenseNonoperating", "InterestExpenseDebt"])
    if intr:
        put("interest_expense_ltm", intr[0], intr[1], intr[2], [intr[3]])
    tax = _first_flow(facts, ["IncomeTaxExpenseBenefit"])
    pretax = _first_flow(facts, PRETAX_TAGS)
    if tax and pretax and pretax[0] > 0 and tax[1] == pretax[1]:
        put("effective_tax_rate", tax[0] / pretax[0], tax[1], tax[2] + " (income tax / pre-tax income)",
            [tax[3], pretax[3]])
    eps = _first_flow(facts, ["EarningsPerShareDiluted"], unit="USD/shares")
    if eps:
        put("eps_ltm", eps[0], eps[1], eps[2], [eps[3]])

    # Shares outstanding: the cover-page count (dei) is the most recent.
    sh_rows = [f for f in facts.get("dei", {}).get("EntityCommonStockSharesOutstanding", {})
               .get("units", {}).get("shares", []) if "end" in f]
    sh = latest_instant(sh_rows) if sh_rows else None
    if sh:
        put("shares_outstanding", sh[0], sh[1], "Cover page, " + sh[1], ["dei:EntityCommonStockSharesOutstanding"])
    else:
        s2 = _first_instant(facts, ["CommonStockSharesOutstanding"], unit="shares")
        if s2:
            put("shares_outstanding", s2[0], s2[1], "Balance sheet, " + s2[1], [s2[2]])

    cash = _first_instant(facts, ["CashAndCashEquivalentsAtCarryingValue",
                                  "CashCashEquivalentsRestrictedCashAndRestrictedCashEquivalents"])
    if cash:
        put("cash", cash[0], cash[1], "Balance sheet, " + cash[1], [cash[2]])

    # Total debt without double counting: LongTermDebt already holds its current portion.
    def inst(tag):
        r = latest_instant(_usd_facts(facts, tag))
        return r  # (value, end) or None

    ltd = inst("LongTermDebt")
    ltd_nc = inst("LongTermDebtNoncurrent")
    ltd_c = inst("LongTermDebtCurrent")
    debt_c = inst("DebtCurrent")
    stb = inst("ShortTermBorrowings")
    cp = inst("CommercialPaper")
    parts, tags, end = [], [], None
    if ltd:
        parts.append(ltd[0]); tags.append("LongTermDebt"); end = ltd[1]
        for r, t in ((stb, "ShortTermBorrowings"), (cp, "CommercialPaper")):
            if r and r[1] == ltd[1]:
                parts.append(r[0]); tags.append(t)
    elif ltd_nc:
        parts.append(ltd_nc[0]); tags.append("LongTermDebtNoncurrent"); end = ltd_nc[1]
        if debt_c and debt_c[1] == ltd_nc[1]:
            parts.append(debt_c[0]); tags.append("DebtCurrent")
        else:
            for r, t in ((ltd_c, "LongTermDebtCurrent"), (stb, "ShortTermBorrowings"), (cp, "CommercialPaper")):
                if r and r[1] == ltd_nc[1]:
                    parts.append(r[0]); tags.append(t)
    if parts:
        put("total_debt", float(sum(parts)), end, "Balance sheet, " + end + " (" + " + ".join(tags) + ")", tags)

    # A tag the company stopped using leaves its last value years behind the rest (Apple's
    # InterestExpense ends in 2023): such a figure is not current, so it is dropped, not shown.
    stale = []
    ends = [_date(v["period_end"]) for v in fields.values()]
    if ends:
        ref = max(ends)
        for k in list(fields):
            if (ref - _date(fields[k]["period_end"])).days > 400:
                stale.append(k)
                del fields[k]

    want = ["revenue_ltm", "ebitda_ltm", "ebit_ltm", "net_income_ltm", "eps_ltm", "depreciation_ltm",
            "interest_expense_ltm", "effective_tax_rate", "shares_outstanding", "cash", "total_debt"]
    return {"fields": fields, "ltm_label": ltm_label, "missing": [k for k in want if k not in fields],
            "stale_dropped": stale}


def snapshot(ticker: str) -> dict:
    ticker = (ticker or "").strip().upper()
    if not ticker:
        return {"success": False, "error": "No ticker given."}
    cik, title = lookup_cik(ticker)
    if cik is None:
        return {"success": False, "code": "not_found",
                "error": f"{ticker} is not in SEC EDGAR's list of filers (SEC data covers US-registered companies)."}
    cache_name = f"snapshot_{cik:010d}.json"
    cached = _read_cache(cache_name, SNAPSHOT_TTL_S)
    if cached is not None:
        cached["symbol"] = ticker
        return {"success": True, "data": cached}
    facts_doc = _get_json(FACTS_URL.format(cik=cik))
    facts = facts_doc.get("facts", {})
    if "us-gaap" not in facts:
        return {"success": False, "code": "no_data",
                "error": f"SEC has no US-GAAP financial facts for {ticker} (it may report under IFRS)."}
    snap = build_snapshot(facts)
    snap.update({"symbol": ticker, "cik": cik, "name": facts_doc.get("entityName") or title, "currency": "USD",
                 "source": "SEC EDGAR company facts",
                 "as_of": _dt.datetime.now(_dt.timezone.utc).isoformat(timespec="seconds")})
    _write_cache(cache_name, snap)
    return {"success": True, "data": snap}


def selftest() -> dict:
    """Offline checks of the period arithmetic on hand-made facts (values in USD)."""
    rows = [
        {"start": "2024-01-01", "end": "2024-12-31", "val": 1000, "filed": "2025-02-01"},  # FY2024
        {"start": "2024-01-01", "end": "2024-06-30", "val": 450, "filed": "2024-08-01"},   # H1 2024
        {"start": "2025-01-01", "end": "2025-06-30", "val": 520, "filed": "2025-08-01"},   # H1 2025
        {"start": "2025-04-01", "end": "2025-06-30", "val": 270, "filed": "2025-08-01"},   # Q2 2025 (ignored)
    ]
    v, end, label = ltm_flow(rows)
    checks = {"ltm_value": (v, 1070.0), "ltm_end": (end, "2025-06-30")}
    v2 = ltm_flow(rows[:1])
    checks["annual_only"] = (v2[0], 1000.0)
    inst = latest_instant([{"end": "2024-12-31", "val": 5}, {"end": "2025-06-30", "val": 7}])
    checks["latest_instant"] = (inst[0], 7.0)
    facts = {"us-gaap": {
        "OperatingIncomeLoss": {"units": {"USD": rows[:1]}},
        "DepreciationAndAmortization": {"units": {"USD": [{"start": "2024-01-01", "end": "2024-12-31", "val": 100}]}},
        "LongTermDebt": {"units": {"USD": [{"end": "2024-12-31", "val": 300}]}},
        "CommercialPaper": {"units": {"USD": [{"end": "2024-12-31", "val": 50}]}},
        "LongTermDebtCurrent": {"units": {"USD": [{"end": "2024-12-31", "val": 40}]}},  # inside LongTermDebt
        "InterestExpense": {"units": {"USD": [{"start": "2021-01-01", "end": "2021-12-31", "val": 9}]}},  # stale tag
    }}
    snap = build_snapshot(facts)
    checks["ebitda"] = (snap["fields"]["ebitda_ltm"]["value"], 1100.0)
    checks["total_debt_no_double_count"] = (snap["fields"]["total_debt"]["value"], 350.0)
    checks["stale_tag_dropped"] = ("interest_expense_ltm" in snap["fields"], False)
    failed = {k: v for k, v in checks.items() if v[0] != v[1]}
    return {"success": not failed, "data": {"checks": {k: {"got": v[0], "want": v[1]} for k, v in checks.items()},
                                             "failed": list(failed)}}


def main(argv) -> int:
    if len(argv) < 2 or argv[1] not in ("snapshot", "selftest"):
        print(fincept_json.dumps({"success": False, "error": "usage: sec_company_facts.py snapshot <TICKER> | selftest"}))
        return 1
    try:
        if argv[1] == "selftest":
            out = selftest()
        else:
            out = snapshot(argv[2] if len(argv) > 2 else "")
    except urllib.error.HTTPError as e:
        msg = "SEC did not answer - try again in a minute." if e.code in (403, 429) else f"SEC answered HTTP {e.code}."
        out = {"success": False, "code": "rate_limited" if e.code in (403, 429) else "network", "error": msg}
    except (urllib.error.URLError, TimeoutError, OSError) as e:
        out = {"success": False, "code": "network", "error": f"Could not reach SEC EDGAR ({type(e).__name__})."}
    except Exception as e:  # never a traceback on stdout
        out = {"success": False, "code": "unknown", "error": f"SEC company facts could not be read: {e}"}
    print(fincept_json.dumps(out))
    return 0 if out.get("success") else 1


if __name__ == "__main__":
    sys.exit(main(sys.argv))
