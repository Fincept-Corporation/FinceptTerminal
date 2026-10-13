"""
Form 4 Insider Transaction Extraction
======================================

Insider trading data from Form 4 filings (MCP edgar_insider_* tools):
- owner name / role (director, officer, 10% owner)
- non-derivative transactions with code, shares, price, value, shares owned after
- open-market buy vs sell summary

Parsing is shared with research.py (Equity Research FILINGS tab). Only transaction
codes P (purchase) and S (sale) are counted as buys and sells: option exercises (M),
tax withholding (F), grants (A) and gifts (G) are not trading decisions, and counting
them as buys/sells — as this module used to — reported insiders "buying" on every
vesting date.
"""

from typing import Dict, Any
import traceback

from .base import EdgarError
from .research import _company, _NotFound, load_form4s, summarize_form4s


def _guard(command: str, fn, *args) -> Dict[str, Any]:
    try:
        return fn(*args)
    except _NotFound as e:
        return {"error": EdgarError(command, str(e)).to_dict()}
    except Exception as e:
        return {"error": EdgarError(command, str(e), traceback.format_exc()).to_dict()}


def _transactions(ticker: str, limit: int) -> Dict[str, Any]:
    parsed, _failed = load_form4s(_company(ticker), limit)
    rows = [{
        "filing_date": p["filed"],
        "accession_number": p["accession"],
        "owner": p["insider"],
        "role": p["role"],
        "is_director": p["is_director"],
        "is_officer": p["is_officer"],
        "transaction_codes": sorted({t["code"] for t in p["transactions"]}),
    } for p in parsed]
    return {"success": True, "data": rows, "count": len(rows)}


def get_insider_transactions(ticker: str, limit: int = 25) -> Dict[str, Any]:
    """Recent Form 4 filings with owner and role."""
    return _guard("get_insider_transactions", _transactions, ticker, limit)


def _detailed(ticker: str, limit: int) -> Dict[str, Any]:
    parsed, _failed = load_form4s(_company(ticker), limit)
    rows = []
    for p in parsed:
        rows.append({
            "filing_date": p["filed"],
            "accession_number": p["accession"],
            "url": p.get("url"),
            "owner": {
                "name": p["insider"],
                "is_director": p["is_director"],
                "is_officer": p["is_officer"],
                "is_ten_percent_owner": p["is_ten_pct_owner"],
                "officer_title": p["officer_title"] or p["role"],
            },
            "transactions": [{
                "transaction_date": p["trade_date"],
                "transaction_code": t["code"],
                "transaction_type": t["code_label"],
                "shares": t["shares"],
                "price_per_share": t["price"],
                "value": t["value"],
                "shares_owned_after": p["owned_after"],
                "rule_10b5_1_plan": t["plan_10b5_1"],
            } for t in p["transactions"]],
            "transaction_count": len(p["transactions"]),
        })
    return {"success": True, "data": rows, "count": len(rows)}


def get_insider_transactions_detailed(ticker: str, limit: int = 25) -> Dict[str, Any]:
    """Form 4 filings with every non-derivative transaction (code, shares, price, value)."""
    return _guard("get_insider_transactions_detailed", _detailed, ticker, limit)


def _summary(ticker: str, limit: int) -> Dict[str, Any]:
    parsed, failed = load_form4s(_company(ticker), limit)
    s = summarize_form4s(parsed)
    dates = sorted(p["filed"] for p in parsed if p.get("filed"))
    return {"success": True, "data": {
        "filings_analyzed": len(parsed),
        "filings_unparsed": failed,
        "window_from": dates[0] if dates else None,
        "window_to": dates[-1] if dates else None,
        "total_transactions": s["buys"]["count"] + s["sells"]["count"],
        "buys": s["buys"]["count"],
        "sells": s["sells"]["count"],
        "buy_volume": s["buys"]["shares"],
        "sell_volume": s["sells"]["shares"],
        "net_volume": s["net_shares"],
        "buy_value": s["buys"]["value"],
        "sell_value": s["sells"]["value"],
        "net_value": s["net_value"],
        "sells_under_10b5_1": s["sells_under_10b5_1"],
        "other_transactions": s["other_codes"],
        "insiders": s["insiders"],
        "note": "buys/sells count open-market codes P and S only",
    }}


def get_insider_summary(ticker: str, limit: int = 50) -> Dict[str, Any]:
    """Open-market buying vs selling over the most recent `limit` Form 4 filings."""
    return _guard("get_insider_summary", _summary, ticker, min(int(limit), 60))
