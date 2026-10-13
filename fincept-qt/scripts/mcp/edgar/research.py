"""
Equity Research → FILINGS tab data
==================================

Compact, display-ready SEC EDGAR views for one issuer. Each command is one
section of the Equity Research FILINGS tab (and is also reachable from the MCP
EDGAR tools through main.py):

  research_filings    <ticker> [limit]  issuer profile, recent filings (insider
                                        forms excluded), 8-K events with item labels
  research_financials <ticker>          latest annual (10-K/20-F/40-F) and quarterly
                                        (10-Q) XBRL key metrics with YoY comparisons
  research_insiders   <ticker> [limit]  Form 4 open-market buy/sell summary and the
                                        recent non-derivative transactions
  research_holders    <ticker> [top_n]  top institutional holders (13F, as aggregated
                                        by Yahoo Finance) and, when the issuer is itself
                                        a 13F-HR filer, its own top 13F positions

Every command returns {"success": true, "data": {...}} or
{"success": false, "error": "<message>", "code": "<not_found|no_data|error>"}.
Numbers are raw (USD, shares, per-share) — formatting is the caller's job.
"""

from __future__ import annotations

import datetime as _dt
import traceback
from concurrent.futures import ThreadPoolExecutor
from typing import Any, Dict, Iterable, List, Optional, Tuple

from .base import check_edgar_available

try:
    from edgar import Company
except ImportError:  # check_edgar_available() reports it
    Company = None  # type: ignore[assignment]

SEC_ARCHIVES = "https://www.sec.gov/Archives/edgar/data"

# Forms filed by insiders about their own holdings — they swamp an issuer's filing
# list (AAPL: ~1,350 of ~2,250) and have their own section, so the filings list
# leaves them out.
_INSIDER_FORMS = {"3", "4", "5", "3/A", "4/A", "5/A", "144", "144/A"}

# Form 8-K item numbers → plain-English labels (SEC Form 8-K General Instructions).
_8K_ITEMS = {
    "1.01": "Entry into a Material Definitive Agreement",
    "1.02": "Termination of a Material Definitive Agreement",
    "1.03": "Bankruptcy or Receivership",
    "1.04": "Mine Safety — Shutdowns and Patterns of Violations",
    "1.05": "Material Cybersecurity Incidents",
    "2.01": "Completion of Acquisition or Disposition of Assets",
    "2.02": "Results of Operations and Financial Condition",
    "2.03": "Creation of a Direct Financial Obligation",
    "2.04": "Triggering Events That Accelerate a Financial Obligation",
    "2.05": "Costs Associated with Exit or Disposal Activities",
    "2.06": "Material Impairments",
    "3.01": "Notice of Delisting or Failure to Satisfy a Listing Rule",
    "3.02": "Unregistered Sales of Equity Securities",
    "3.03": "Material Modification to Rights of Security Holders",
    "4.01": "Changes in Registrant's Certifying Accountant",
    "4.02": "Non-Reliance on Previously Issued Financial Statements",
    "5.01": "Changes in Control of Registrant",
    "5.02": "Departure/Election of Directors or Officers; Compensation",
    "5.03": "Amendments to Articles of Incorporation or Bylaws",
    "5.04": "Temporary Suspension of Trading Under Employee Benefit Plans",
    "5.05": "Amendments to the Code of Ethics",
    "5.06": "Change in Shell Company Status",
    "5.07": "Submission of Matters to a Vote of Security Holders",
    "5.08": "Shareholder Director Nominations",
    "6.01": "ABS Informational and Computational Material",
    "6.02": "Change of Servicer or Trustee",
    "6.03": "Change in Credit Enhancement",
    "6.04": "Failure to Make a Required Distribution",
    "6.05": "Securities Act Updating Disclosure",
    "7.01": "Regulation FD Disclosure",
    "8.01": "Other Events",
    "9.01": "Financial Statements and Exhibits",
}

# Form 4 transaction codes (Form 4 General Instructions, Item 8).
_TXN_CODES = {
    "P": "Open-market or private purchase",
    "S": "Open-market or private sale",
    "A": "Grant or award",
    "D": "Disposition to the issuer",
    "F": "Tax / exercise-price withholding",
    "I": "Discretionary transaction",
    "M": "Option exercise / conversion",
    "C": "Conversion of derivative security",
    "E": "Expiration of short derivative position",
    "H": "Expiration of long derivative position",
    "O": "Exercise of out-of-the-money derivative",
    "X": "Exercise of in-the-money derivative",
    "G": "Gift",
    "L": "Small acquisition",
    "W": "Acquisition or disposition by will",
    "Z": "Deposit into / withdrawal from voting trust",
    "J": "Other acquisition or disposition",
    "K": "Equity swap or similar",
    "U": "Disposition due to tender of shares",
    "V": "Voluntarily reported earlier",
}

_ANNUAL_FORMS = {"10-K", "10-K/A", "10-KT", "10-KT/A", "20-F", "20-F/A", "40-F", "40-F/A"}
_QUARTERLY_FORMS = {"10-Q", "10-Q/A"}

# (key, label, kind, statement, {taxonomy: [concepts in preference order]})
_METRICS = [
    ("revenue", "Revenue", "duration", "is", {
        "us-gaap": ["RevenueFromContractWithCustomerExcludingAssessedTax", "Revenues", "SalesRevenueNet",
                    "RevenueFromContractWithCustomerIncludingAssessedTax", "RevenuesNetOfInterestExpense"],
        "ifrs-full": ["Revenue", "RevenueFromContractsWithCustomers"]}),
    ("operating_income", "Operating income", "duration", "is", {
        "us-gaap": ["OperatingIncomeLoss"],
        "ifrs-full": ["ProfitLossFromOperatingActivities"]}),
    ("net_income", "Net income", "duration", "is", {
        "us-gaap": ["NetIncomeLoss", "ProfitLoss", "NetIncomeLossAvailableToCommonStockholdersBasic"],
        "ifrs-full": ["ProfitLossAttributableToOwnersOfParent", "ProfitLoss"]}),
    ("eps_diluted", "EPS (diluted)", "duration", "is", {
        "us-gaap": ["EarningsPerShareDiluted", "EarningsPerShareBasicAndDiluted", "EarningsPerShareBasic"],
        "ifrs-full": ["DilutedEarningsLossPerShare", "BasicEarningsLossPerShare"]}),
    ("operating_cash_flow", "Operating cash flow", "duration", "cf", {
        "us-gaap": ["NetCashProvidedByUsedInOperatingActivities",
                    "NetCashProvidedByUsedInOperatingActivitiesContinuingOperations"],
        "ifrs-full": ["CashFlowsFromUsedInOperatingActivities"]}),
    ("capex", "Capital expenditure", "duration", "cf", {
        "us-gaap": ["PaymentsToAcquirePropertyPlantAndEquipment", "PaymentsToAcquireProductiveAssets"],
        "ifrs-full": ["PurchaseOfPropertyPlantAndEquipmentClassifiedAsInvestingActivities"]}),
    ("total_assets", "Total assets", "instant", "bs", {
        "us-gaap": ["Assets"], "ifrs-full": ["Assets"]}),
    ("total_liabilities", "Total liabilities", "instant", "bs", {
        "us-gaap": ["Liabilities"], "ifrs-full": ["Liabilities"]}),
    ("equity", "Shareholders' equity", "instant", "bs", {
        "us-gaap": ["StockholdersEquity", "StockholdersEquityIncludingPortionAttributableToNoncontrollingInterest"],
        "ifrs-full": ["EquityAttributableToOwnersOfParent", "Equity"]}),
    ("cash", "Cash & equivalents", "instant", "bs", {
        "us-gaap": ["CashAndCashEquivalentsAtCarryingValue",
                    "CashCashEquivalentsRestrictedCashAndRestrictedCashEquivalents", "Cash"],
        "ifrs-full": ["CashAndCashEquivalents"]}),
    # Non-current debt only. LongTermDebt (incl. current maturities) and IFRS LongtermBorrowings
    # (bank loans, excluding bonds issued — TSM tags its TWD 900bn of bonds separately) would
    # silently change the definition from one issuer to the next.
    ("long_term_debt", "Long-term debt (non-current)", "instant", "bs", {
        "us-gaap": ["LongTermDebtNoncurrent", "LongTermDebtAndCapitalLeaseObligations"],
        "ifrs-full": ["NoncurrentPortionOfNoncurrentBorrowings"]}),
]


# ─────────────────────────────────────────────────────────────────────────────
# Shared helpers
# ─────────────────────────────────────────────────────────────────────────────

class _NotFound(Exception):
    pass


def _fail(message: str, code: str = "error") -> Dict[str, Any]:
    return {"success": False, "error": message, "code": code}


def _company(ticker: Optional[str]):
    check_edgar_available()
    t = (ticker or "").strip().upper()
    if not t:
        raise ValueError("ticker is required")
    try:
        c = Company(t)
    except Exception as exc:  # edgartools raises CompanyNotFoundError for unknown tickers
        raise _NotFound(f"{t} is not registered with the SEC (no EDGAR CIK for this ticker)") from exc
    if c is None or getattr(c, "not_found", False) or not getattr(c, "cik", None):
        raise _NotFound(f"{t} is not registered with the SEC (no EDGAR CIK for this ticker)")
    return c


def _acc_nodash(accession: str) -> str:
    return (accession or "").replace("-", "")


def _doc_url(cik: int, accession: str, primary_doc: str) -> str:
    if not accession or not primary_doc:
        return _index_url(cik, accession)
    return f"{SEC_ARCHIVES}/{int(cik)}/{_acc_nodash(accession)}/{primary_doc}"


def _index_url(cik: int, accession: str) -> str:
    if not accession:
        return ""
    return f"{SEC_ARCHIVES}/{int(cik)}/{_acc_nodash(accession)}/{accession}-index.htm"


def _iso(value: Any) -> str:
    if value is None:
        return ""
    if isinstance(value, (_dt.date, _dt.datetime)):
        return value.isoformat()[:10]
    return str(value)[:10]


def _num(value: Any) -> Optional[float]:
    """float(value), or None for missing / non-numeric / non-finite cells."""
    if value is None or isinstance(value, bool):
        return None
    try:
        f = float(value)
    except (TypeError, ValueError):
        return None
    if f != f or f in (float("inf"), float("-inf")):
        return None
    return f


def _run(command: str, fn, *args) -> Dict[str, Any]:
    try:
        return fn(*args)
    except _NotFound as exc:
        return _fail(str(exc), "not_found")
    except Exception as exc:  # never let a traceback replace the JSON envelope
        return {"success": False, "error": f"{command}: {exc}", "code": "error",
                "details": traceback.format_exc(limit=4)}


# ─────────────────────────────────────────────────────────────────────────────
# research_filings
# ─────────────────────────────────────────────────────────────────────────────

def _8k_items(raw: str) -> List[Dict[str, str]]:
    out = []
    for code in [p.strip() for p in (raw or "").split(",") if p.strip()]:
        out.append({"code": code, "label": _8K_ITEMS.get(code, "Item " + code)})
    return out


def _filings(ticker: str, limit: int) -> Dict[str, Any]:
    c = _company(ticker)
    cik = int(c.cik)
    table = c.get_filings().data  # pyarrow Table, newest first
    rows = table.to_pylist()

    filings: List[Dict[str, Any]] = []
    events: List[Dict[str, Any]] = []
    latest: Dict[str, Dict[str, Any]] = {}
    insider_count = 0
    is_13f_filer = False
    for r in rows:
        form = str(r.get("form") or "")
        if form.startswith("13F-HR"):
            is_13f_filer = True
        if form in _INSIDER_FORMS:
            insider_count += 1
            continue
        acc = str(r.get("accession_number") or "")
        entry = {
            "form": form,
            "filed": _iso(r.get("filing_date")),
            "period": _iso(r.get("reportDate")),
            "accession": acc,
            "description": str(r.get("primaryDocDescription") or ""),
            "items": str(r.get("items") or ""),
            "url": _doc_url(cik, acc, str(r.get("primaryDocument") or "")),
            "index_url": _index_url(cik, acc),
        }
        base_form = form.split("/")[0]
        if base_form in ("10-K", "10-Q", "20-F", "40-F", "DEF 14A") and base_form not in latest:
            latest[base_form] = entry
        if form in ("8-K", "8-K/A") and len(events) < 25:
            events.append({**entry, "item_labels": _8k_items(entry["items"])})
        if len(filings) < limit:
            filings.append(entry)

    exchanges = []
    try:
        exchanges = list(c.get_exchanges() or [])
    except Exception:
        pass
    return {"success": True, "data": {
        "ticker": ticker.strip().upper(),
        "cik": cik,
        "name": str(getattr(c, "name", "") or ""),
        "sic": str(getattr(c, "sic", "") or ""),
        "industry": str(getattr(c, "industry", "") or ""),
        "fiscal_year_end": str(getattr(c, "fiscal_year_end", "") or ""),
        "exchanges": exchanges,
        "is_13f_filer": is_13f_filer,
        "filings": filings,
        "events": events,
        "latest": latest,
        "total_filings": len(rows),
        "insider_filings": insider_count,
        "edgar_url": f"https://www.sec.gov/cgi-bin/browse-edgar?action=getcompany&CIK={cik:010d}",
    }}


def research_filings(ticker: str, limit: int = 150) -> Dict[str, Any]:
    return _run("research_filings", _filings, ticker, max(1, min(int(limit), 500)))


# ─────────────────────────────────────────────────────────────────────────────
# research_financials — XBRL company facts
# ─────────────────────────────────────────────────────────────────────────────

def _date(s: Any) -> Optional[_dt.date]:
    try:
        return _dt.date.fromisoformat(str(s)[:10])
    except (TypeError, ValueError):
        return None


def _span_days(e: Dict[str, Any]) -> Optional[int]:
    a, b = _date(e.get("start")), _date(e.get("end"))
    if a is None or b is None:
        return None
    return (b - a).days


def _pick_unit(units: Dict[str, List[Dict[str, Any]]]) -> Tuple[str, List[Dict[str, Any]]]:
    # Foreign filers tag both the reporting currency and a USD convenience
    # translation (latest year only); the reporting currency has the long history.
    best = max(units.items(), key=lambda kv: (len(kv[1]), kv[0] == "USD"))
    return best[0], best[1]


def _series(facts: Dict[str, Any], concepts: Dict[str, List[str]]):
    """Yield (taxonomy, concept, unit, entries) for every concept the issuer tags, in preference order."""
    for taxonomy, names in concepts.items():
        tx = facts.get(taxonomy) or {}
        for name in names:
            node = tx.get(name)
            if not node or not node.get("units"):
                continue
            unit, entries = _pick_unit(node["units"])
            yield taxonomy, name, unit, entries


def _anchor(facts: Dict[str, Any], forms: set, annual: bool) -> Optional[Dict[str, Any]]:
    """The newest filing of `forms` and the period it reports."""
    best = None
    for _key, _label, kind, _stmt, concepts in _METRICS:
        if kind != "duration":
            continue
        for _tx, _name, _unit, entries in _series(facts, concepts):
            for e in entries:
                if e.get("form") not in forms:
                    continue
                days = _span_days(e)
                if days is None:
                    continue
                if annual and not (340 <= days <= 380):
                    continue
                if not annual and not (80 <= days <= 100):
                    continue
                k = (str(e.get("filed", "")), str(e.get("end", "")))
                if best is None or k > best[0]:
                    best = (k, e)
    if best is None:
        return None
    e = best[1]
    return {"accession": e.get("accn"), "form": e.get("form"), "filed": e.get("filed"), "fy": e.get("fy"),
            "fp": e.get("fp"), "period_end": e.get("end")}


def _choose(entries: List[Dict[str, Any]], end: str, span: Optional[Tuple[int, int]],
            accession: Optional[str]) -> Optional[Dict[str, Any]]:
    """Entry for period `end` (± span window in days for durations); the anchor filing's own value wins,
    otherwise the most recently filed (restated) one."""
    cands = []
    for e in entries:
        if e.get("end") != end:
            continue
        if span is not None:
            d = _span_days(e)
            if d is None or not (span[0] <= d <= span[1]):
                continue
        elif e.get("start"):
            continue  # an instant metric must not pick up a duration fact
        cands.append(e)
    if not cands:
        return None
    own = [e for e in cands if e.get("accn") == accession]
    pool = own or cands
    return max(pool, key=lambda e: (str(e.get("filed", "")), str(e.get("accn", ""))))


def _prior(entries: List[Dict[str, Any]], end: str, span: Optional[Tuple[int, int]]) -> Optional[Dict[str, Any]]:
    """Same-length period ending ~one year before `end` (closest to 365 days; latest filing on a tie)."""
    target = _date(end)
    if target is None:
        return None
    best = None  # (distance from 365 days, filed, entry)
    for e in entries:
        d_end = _date(e.get("end"))
        if d_end is None:
            continue
        gap = (target - d_end).days
        if not (350 <= gap <= 380):
            continue
        if span is not None:
            d = _span_days(e)
            if d is None or not (span[0] <= d <= span[1]):
                continue
        elif e.get("start"):
            continue
        score = abs(gap - 365)
        filed = str(e.get("filed", ""))
        if best is None or score < best[0] or (score == best[0] and filed > best[1]):
            best = (score, filed, e)
    return best[2] if best else None


def _yoy(value: Optional[float], prior: Optional[float]) -> Optional[float]:
    # Growth is only meaningful off a positive base; a swing from a loss is "n/m".
    if value is None or prior is None or prior <= 0:
        return None
    return (value - prior) / prior


def _group(facts: Dict[str, Any], forms: set, annual: bool, cik: int) -> Optional[Dict[str, Any]]:
    anchor = _anchor(facts, forms, annual)
    if anchor is None:
        return None
    end = anchor["period_end"]
    acc = anchor["accession"]
    metrics = []
    by_key: Dict[str, Dict[str, Any]] = {}
    for key, label, kind, stmt, concepts in _METRICS:
        series = list(_series(facts, concepts))
        if kind == "instant":
            spans = [None]
        elif annual:
            spans = [(340, 380)]
        elif stmt == "cf":
            # 10-Q cash-flow statements are year-to-date: 3, 6 or 9 months.
            spans = [(250, 285), (165, 195), (80, 100)]
        else:
            spans = [(80, 100), (165, 195), (250, 285)]
        chosen = None
        for taxonomy, name, unit, entries in series:
            for span in spans:
                e = _choose(entries, end, span, acc)
                if e is None:
                    continue
                # The comparative may sit under an older concept (issuers re-tag revenue, e.g.
                # Revenues → RevenueFromContractWithCustomer…): search every concept of the
                # metric in the same unit, the chosen concept first.
                p = _prior(entries, end, span)
                if p is None:
                    pool = [x for _t, _n, u, es in series if u == unit for x in es]
                    p = _prior(pool, end, span)
                chosen = (taxonomy, name, unit, e, p, span)
                break
            if chosen:
                break
        if not chosen:
            m = {"key": key, "label": label, "statement": stmt, "value": None, "prior": None, "yoy": None,
                 "unit": None, "months": None, "concept": None}
        else:
            taxonomy, name, unit, e, p, span = chosen
            v = _num(e.get("val"))
            pv = _num(p.get("val")) if p else None
            days = _span_days(e) if span is not None else None
            m = {"key": key, "label": label, "statement": stmt, "value": v, "prior": pv,
                 "prior_period_end": p.get("end") if p else None, "yoy": _yoy(v, pv), "unit": unit,
                 "months": (round(days / 30.44) if days else None), "concept": f"{taxonomy}:{name}"}
        metrics.append(m)
        by_key[key] = m

    # Free cash flow = operating cash flow − capex, only when both cover the same span.
    ocf, capex = by_key.get("operating_cash_flow"), by_key.get("capex")
    if ocf and capex and ocf.get("value") is not None and capex.get("value") is not None \
            and ocf.get("months") == capex.get("months") and ocf.get("unit") == capex.get("unit"):
        fcf = ocf["value"] - capex["value"]
        pf = None
        if ocf.get("prior") is not None and capex.get("prior") is not None:
            pf = ocf["prior"] - capex["prior"]
        metrics.insert(metrics.index(capex) + 1, {
            "key": "free_cash_flow", "label": "Free cash flow (OCF − capex)", "statement": "cf", "value": fcf,
            "prior": pf, "yoy": _yoy(fcf, pf), "unit": ocf.get("unit"), "months": ocf.get("months"),
            "concept": "derived"})

    return {**anchor, "index_url": _index_url(cik, acc), "metrics": metrics}


def _financials(ticker: str) -> Dict[str, Any]:
    c = _company(ticker)
    cik = int(c.cik)
    from edgar.httprequests import download_json
    raw = download_json(f"https://data.sec.gov/api/xbrl/companyfacts/CIK{cik:010d}.json")
    facts = (raw or {}).get("facts") or {}
    if not facts.get("us-gaap") and not facts.get("ifrs-full"):
        return _fail(f"{ticker.upper()} has no XBRL financial statements on EDGAR "
                     f"(funds, trusts and some foreign issuers file none)", "no_data")

    shares = None
    dei = (facts.get("dei") or {}).get("EntityCommonStockSharesOutstanding")
    if dei and dei.get("units"):
        _u, entries = _pick_unit(dei["units"])
        if entries:
            e = max(entries, key=lambda x: (str(x.get("end", "")), str(x.get("filed", ""))))
            shares = {"value": _num(e.get("val")), "as_of": e.get("end"), "form": e.get("form")}

    annual = _group(facts, _ANNUAL_FORMS, True, cik)
    quarterly = _group(facts, _QUARTERLY_FORMS, False, cik)
    if annual is None and quarterly is None:
        return _fail(f"No 10-K / 10-Q / 20-F XBRL periods found for {ticker.upper()}", "no_data")
    return {"success": True, "data": {
        "ticker": ticker.strip().upper(),
        "cik": cik,
        "name": str(raw.get("entityName") or getattr(c, "name", "") or ""),
        "taxonomy": "us-gaap" if facts.get("us-gaap") else "ifrs-full",
        "annual": annual,
        "quarterly": quarterly,
        "shares_outstanding": shares,
    }}


def research_financials(ticker: str) -> Dict[str, Any]:
    return _run("research_financials", _financials, ticker)


# ─────────────────────────────────────────────────────────────────────────────
# Form 4 insider transactions (shared with forms_insider.py)
# ─────────────────────────────────────────────────────────────────────────────

def parse_form4(filing) -> Optional[Dict[str, Any]]:
    """One Form 4 → {filed, trade_date, insider, role, flags…, transactions[]} (non-derivative only)."""
    form4 = filing.obj()
    if form4 is None:
        return None
    summary = form4.get_ownership_summary()
    owner = None
    try:
        owners = getattr(getattr(form4, "reporting_owners", None), "owners", None) or []
        owner = owners[0] if owners else None
    except Exception:
        owner = None
    plan = bool(getattr(summary, "aff10b5_one", False) or getattr(summary, "has_10b5_1_plan", False))
    txns = []
    for t in getattr(summary, "transactions", None) or []:
        if getattr(t, "is_derivative", False) or str(getattr(t, "security_type", "")) == "derivative":
            continue  # the option leg mirrors the M/X row on the common-stock table
        code = str(getattr(t, "code", "") or "").strip().upper()
        shares = _num(getattr(t, "shares_numeric", None))
        if shares is None:
            shares = _num(getattr(t, "shares", None))
        price = _num(getattr(t, "price_numeric", None))
        if price is None:
            price = _num(getattr(t, "price_per_share", None))
        value = _num(getattr(t, "value_numeric", None))
        if value is None and shares is not None and price is not None:
            value = shares * price
        txns.append({
            "code": code,
            "code_label": _TXN_CODES.get(code, str(getattr(t, "code_description", "") or code)),
            "type": str(getattr(t, "transaction_type", "") or ""),
            "shares": shares,
            "price": price if (price or 0) > 0 else None,
            "value": value if (value or 0) > 0 else None,
            "plan_10b5_1": bool(getattr(t, "is_10b5_1_plan", False)) or plan,
        })
    return {
        "filed": _iso(getattr(filing, "filing_date", None)),
        "trade_date": _iso(getattr(summary, "reporting_date", None)),
        "accession": str(getattr(filing, "accession_no", "") or getattr(filing, "accession_number", "")),
        "insider": str(getattr(summary, "insider_name", "") or (owner.name if owner else "")),
        "role": str(getattr(summary, "position", "") or ""),
        "is_director": bool(getattr(owner, "is_director", False)) if owner else False,
        "is_officer": bool(getattr(owner, "is_officer", False)) if owner else False,
        "is_ten_pct_owner": bool(getattr(owner, "is_ten_pct_owner", False)) if owner else False,
        "officer_title": str(getattr(owner, "officer_title", "") or "") if owner else "",
        "owned_after": _num(getattr(summary, "remaining_shares", None)),
        "plan_10b5_1": plan,
        "transactions": txns,
    }


def load_form4s(company, limit: int) -> Tuple[List[Dict[str, Any]], int]:
    """Parse the `limit` most recent Form 4s concurrently (edgartools rate-limits to SEC's 9 req/s)."""
    filings = company.get_filings(form="4")
    if filings is None or len(filings) == 0:
        return [], 0
    n = min(limit, len(filings))
    items = [filings[i] for i in range(n)]
    # Primary-document paths come from the submissions table (a Filing's primary_document is an
    # attachment object, not the path the archive URL needs).
    try:
        docs = filings.data.column("primaryDocument").to_pylist()[:n]
    except Exception:
        docs = [""] * n

    def work(f):
        try:
            return parse_form4(f)
        except Exception:
            return None

    with ThreadPoolExecutor(max_workers=6) as pool:
        parsed = list(pool.map(work, items))
    cik = int(company.cik)
    ok = []
    for doc, p in zip(docs, parsed):
        if p is None:
            continue
        p["url"] = _doc_url(cik, p["accession"], str(doc or ""))
        ok.append(p)
    return ok, len(items) - len(ok)


def summarize_form4s(parsed: Iterable[Dict[str, Any]]) -> Dict[str, Any]:
    """Open-market activity only counts codes P (buy) and S (sell). Grants, option exercises,
    tax withholding and gifts are not trading decisions and are tallied separately."""
    buys = {"count": 0, "shares": 0.0, "value": 0.0}
    sells = {"count": 0, "shares": 0.0, "value": 0.0}
    sells_plan = 0
    other: Dict[str, int] = {}
    insiders = set()
    for p in parsed:
        insiders.add(p.get("insider"))
        for t in p.get("transactions", []):
            code = t.get("code")
            if code == "P":
                bucket = buys
            elif code == "S":
                bucket = sells
                if t.get("plan_10b5_1"):
                    sells_plan += 1
            else:
                other[code] = other.get(code, 0) + 1
                continue
            bucket["count"] += 1
            bucket["shares"] += t.get("shares") or 0.0
            bucket["value"] += t.get("value") or 0.0
    return {
        "buys": buys,
        "sells": sells,
        "net_shares": buys["shares"] - sells["shares"],
        "net_value": buys["value"] - sells["value"],
        "sells_under_10b5_1": sells_plan,
        "other_codes": {k: {"count": v, "label": _TXN_CODES.get(k, k)} for k, v in sorted(other.items())},
        "insiders": len([i for i in insiders if i]),
    }


def _insiders(ticker: str, limit: int) -> Dict[str, Any]:
    c = _company(ticker)
    parsed, failed = load_form4s(c, limit)
    rows = []
    for p in parsed:
        for t in p["transactions"]:
            rows.append({
                "filed": p["filed"], "trade_date": p["trade_date"], "insider": p["insider"], "role": p["role"],
                "code": t["code"], "code_label": t["code_label"], "shares": t["shares"], "price": t["price"],
                "value": t["value"], "owned_after": p["owned_after"], "plan_10b5_1": t["plan_10b5_1"],
                "url": p["url"],
            })
    rows.sort(key=lambda r: (r["trade_date"] or r["filed"] or ""), reverse=True)
    dates = sorted(p["filed"] for p in parsed if p.get("filed"))
    return {"success": True, "data": {
        "ticker": ticker.strip().upper(),
        "cik": int(c.cik),
        "name": str(getattr(c, "name", "") or ""),
        "window": {"filings_requested": limit, "filings_parsed": len(parsed), "filings_failed": failed,
                   "from": dates[0] if dates else None, "to": dates[-1] if dates else None},
        "summary": summarize_form4s(parsed),
        "transactions": rows[:120],
        "transactions_total": len(rows),
    }}


def research_insiders(ticker: str, limit: int = 25) -> Dict[str, Any]:
    return _run("research_insiders", _insiders, ticker, max(1, min(int(limit), 60)))


# ─────────────────────────────────────────────────────────────────────────────
# research_holders — institutional (13F) ownership
# ─────────────────────────────────────────────────────────────────────────────

def _yahoo_holders(ticker: str, top_n: int) -> Dict[str, Any]:
    import yfinance as yf

    t = yf.Ticker(ticker)
    out: Dict[str, Any] = {"holders": [], "summary": None, "error": None}
    try:
        df = t.institutional_holders
        if df is not None and not df.empty:
            for _, r in df.head(top_n).iterrows():
                out["holders"].append({
                    "holder": str(r.get("Holder") or ""),
                    "shares": _num(r.get("Shares")),
                    "pct_out": _num(r.get("pctHeld")),
                    "value": _num(r.get("Value")),
                    "pct_change": _num(r.get("pctChange")),
                    "reported": _iso(r.get("Date Reported")),
                })
    except Exception as exc:
        out["error"] = str(exc)
    try:
        mh = t.major_holders
        if mh is not None and not mh.empty and "Value" in mh.columns:
            vals = mh["Value"].to_dict()
            out["summary"] = {
                "insiders_pct": _num(vals.get("insidersPercentHeld")),
                "institutions_pct": _num(vals.get("institutionsPercentHeld")),
                "institutions_float_pct": _num(vals.get("institutionsFloatPercentHeld")),
                "institutions_count": _num(vals.get("institutionsCount")),
            }
    except Exception as exc:
        out["error"] = out["error"] or str(exc)
    return out


def _own_13f(company, top_n: int) -> Optional[Dict[str, Any]]:
    filings = company.get_filings(form="13F-HR")
    if filings is None or len(filings) == 0:
        return None
    filing = filings[0]
    report = filing.obj()
    if report is None:
        return None
    df = report.holdings
    positions = []
    if df is not None and hasattr(df, "sort_values") and "Value" in df.columns:
        for _, r in df.sort_values("Value", ascending=False).head(top_n).iterrows():
            positions.append({
                "issuer": str(r.get("Issuer") or ""),
                "ticker": str(r.get("Ticker") or ""),
                "class": str(r.get("Class") or ""),
                "shares": _num(r.get("SharesPrnAmount")),
                "value": _num(r.get("Value")),
                "put_call": str(r.get("PutCall") or ""),
            })
    cik = int(company.cik)
    acc = str(getattr(filing, "accession_no", "") or "")
    return {
        "report_period": _iso(getattr(report, "report_period", None)),
        "filed": _iso(getattr(filing, "filing_date", None)),
        "total_value": _num(getattr(report, "total_value", None)),
        "total_holdings": _num(getattr(report, "total_holdings", None)),
        "manager": str(getattr(report, "management_company_name", "") or ""),
        "positions": positions,
        "index_url": _index_url(cik, acc),
    }


def _holders(ticker: str, top_n: int) -> Dict[str, Any]:
    t = ticker.strip().upper()
    yahoo = _yahoo_holders(t, top_n)
    own = None
    own_error = None
    try:
        c = _company(t)
        own = _own_13f(c, top_n)
    except _NotFound:
        pass  # not an EDGAR registrant — the Yahoo aggregation can still exist
    except Exception as exc:
        own_error = str(exc)
    if not yahoo["holders"] and yahoo["summary"] is None and own is None:
        return _fail(f"No institutional (13F) ownership data for {t}"
                     + (f" — {yahoo['error']}" if yahoo.get("error") else ""), "no_data")
    return {"success": True, "data": {
        "ticker": t,
        "institutional": yahoo["holders"],
        "summary": yahoo["summary"],
        "source": "13F-HR filings as aggregated by Yahoo Finance (top holders, latest reported quarter)",
        "own_13f": own,
        "own_13f_error": own_error,
    }}


def research_holders(ticker: str, top_n: int = 10) -> Dict[str, Any]:
    return _run("research_holders", _holders, ticker, max(1, min(int(top_n), 50)))
