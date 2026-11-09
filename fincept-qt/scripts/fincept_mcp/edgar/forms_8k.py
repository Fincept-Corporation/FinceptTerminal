"""
8-K Corporate Events Extraction
================================

Extract corporate events from 8-K filings including:
- Item 1.01: Entry into Material Agreement
- Item 2.02: Results of Operations and Financial Condition (Earnings)
- Item 5.02: Departure/Election of Directors or Officers
- Item 7.01: Regulation FD Disclosure
- Item 8.01: Other Events
- Full text and categorized events
"""

from typing import Dict, Any, Optional, List
import traceback

try:
    from edgar import Company
    EDGAR_AVAILABLE = True
except ImportError:
    EDGAR_AVAILABLE = False

from .base import EdgarError, check_edgar_available


def get_latest_8k(ticker: str) -> Dict[str, Any]:
    """Get latest 8-K filing"""
    try:
        check_edgar_available()
        company = Company(ticker)
        filings = company.get_filings(form="8-K")

        if not filings or len(filings) == 0:
            return {
                "error": EdgarError("get_latest_8k",
                                  f"No 8-K filings found for {ticker}").to_dict()
            }

        latest = filings.latest(1)

        return {
            "success": True,
            "data": {
                "accession_number": latest.accession_number,
                "filing_date": str(latest.filing_date),
                "form": latest.form,
                "company": latest.company if hasattr(latest, 'company') else ticker,
                "cik": latest.cik if hasattr(latest, 'cik') else None,
                "items": latest.items if hasattr(latest, 'items') else [],
                "filing": latest
            }
        }
    except Exception as e:
        return {"error": EdgarError("get_latest_8k", str(e), traceback.format_exc()).to_dict()}


def get_8k_events(ticker: str, limit: int = 30) -> Dict[str, Any]:
    """
    Get recent 8-K events with categorization

    Args:
        ticker: Stock ticker symbol
        limit: Number of filings to retrieve

    Returns:
        List of 8-K events with items and dates
    """
    try:
        check_edgar_available()
        company = Company(ticker)
        filings = company.get_filings(form="8-K")

        if not filings or len(filings) == 0:
            return {
                "success": True,
                "data": [],
                "count": 0
            }

        events = []
        count = 0

        for filing in filings:
            if count >= limit:
                break

            event = {
                "accession_number": filing.accession_number,
                "filing_date": str(filing.filing_date),
                "items": filing.items if hasattr(filing, 'items') else [],
                "form": filing.form
            }

            events.append(event)
            count += 1

        return {
            "success": True,
            "data": events,
            "count": len(events)
        }
    except Exception as e:
        return {"error": EdgarError("get_8k_events", str(e), traceback.format_exc()).to_dict()}


def get_8k_events_categorized(ticker: str, limit: int = 30) -> Dict[str, Any]:
    """
    Get 8-K events categorized by type

    Args:
        ticker: Stock ticker symbol
        limit: Number of filings to retrieve

    Returns:
        Events grouped by category
    """
    try:
        check_edgar_available()
        company = Company(ticker)
        filings = company.get_filings(form="8-K")

        if not filings or len(filings) == 0:
            return {
                "success": True,
                "data": {
                    "earnings": [],
                    "management_changes": [],
                    "agreements": [],
                    "other": []
                },
                "count": 0
            }

        categories = {
            "earnings": [],
            "management_changes": [],
            "agreements": [],
            "mergers_acquisitions": [],
            "other": []
        }

        count = 0
        for filing in filings:
            if count >= limit:
                break

            items = filing.items if hasattr(filing, 'items') else []
            items_str = ','.join(items) if items else ''

            event = {
                "accession_number": filing.accession_number,
                "filing_date": str(filing.filing_date),
                "items": items
            }

            # Categorize by item number
            if '2.02' in items_str:  # Results of Operations
                categories["earnings"].append(event)
            elif any(x in items_str for x in ['5.02', '5.03']):  # Management changes
                categories["management_changes"].append(event)
            elif '1.01' in items_str:  # Material Agreement
                categories["agreements"].append(event)
            elif any(x in items_str for x in ['1.02', '2.01']):  # M&A
                categories["mergers_acquisitions"].append(event)
            else:
                categories["other"].append(event)

            count += 1

        return {
            "success": True,
            "data": categories,
            "count": count
        }
    except Exception as e:
        return {"error": EdgarError("get_8k_events_categorized", str(e), traceback.format_exc()).to_dict()}


def get_8k_full_text(ticker: str, max_length: Optional[int] = None) -> Dict[str, Any]:
    """Get full text of latest 8-K"""
    try:
        check_edgar_available()
        company = Company(ticker)
        filing = company.get_filings(form="8-K").latest(1)

        if not filing:
            return {
                "error": EdgarError("get_8k_full_text",
                                  f"No 8-K filing found for {ticker}").to_dict()
            }

        text = filing.text() if hasattr(filing, 'text') else str(filing)

        if max_length and len(text) > max_length:
            text = text[:max_length] + "... (truncated)"

        return {
            "success": True,
            "data": {
                "ticker": ticker,
                "accession_number": filing.accession_number,
                "filing_date": str(filing.filing_date),
                "items": filing.items if hasattr(filing, 'items') else [],
                "text": text,
                "text_length": len(text),
                "truncated": max_length is not None and len(text) > max_length
            }
        }
    except Exception as e:
        return {"error": EdgarError("get_8k_full_text", str(e), traceback.format_exc()).to_dict()}


def search_8k(ticker: str, query: str, max_results: int = 10) -> Dict[str, Any]:
    """Search within latest 8-K filing"""
    try:
        check_edgar_available()
        company = Company(ticker)
        filing = company.get_filings(form="8-K").latest(1)

        if not filing:
            return {
                "error": EdgarError("search_8k",
                                  f"No 8-K filing found for {ticker}").to_dict()
            }

        text = filing.text()
        query_lower = query.lower()
        text_lower = text.lower()

        results = []
        start = 0
        while len(results) < max_results:
            pos = text_lower.find(query_lower, start)
            if pos == -1:
                break

            context_start = max(0, pos - 100)
            context_end = min(len(text), pos + len(query) + 100)
            context = text[context_start:context_end]

            results.append({
                "position": pos,
                "context": context,
                "match": text[pos:pos + len(query)]
            })

            start = pos + len(query)

        return {
            "success": True,
            "data": {
                "ticker": ticker,
                "query": query,
                "results": results,
                "count": len(results)
            }
        }
    except Exception as e:
        return {"error": EdgarError("search_8k", str(e), traceback.format_exc()).to_dict()}
