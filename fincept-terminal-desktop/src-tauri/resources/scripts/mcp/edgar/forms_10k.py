"""
10-K Annual Report Extraction
==============================

Complete extraction of 10-K annual reports including:
- All text sections (Items 1-16)
- Business description (Item 1)
- Risk factors (Item 1A)
- MD&A (Item 7)
- Financial statements (Item 8)
- Controls and procedures (Item 9A)
- Executive compensation (Item 11)
- Directors and governance (Item 10)
- Exhibits and attachments
- Full text search and extraction
"""

from typing import Dict, Any, Optional, List
import traceback

try:
    from edgar import Company
    EDGAR_AVAILABLE = True
except ImportError:
    EDGAR_AVAILABLE = False

from .base import EdgarError, check_edgar_available


def get_latest_10k(ticker: str) -> Dict[str, Any]:
    """
    Get latest 10-K filing for a company

    Args:
        ticker: Stock ticker symbol

    Returns:
        10-K filing object and metadata
    """
    try:
        check_edgar_available()
        company = Company(ticker)
        filings = company.get_filings(form="10-K")

        if not filings or len(filings) == 0:
            return {
                "error": EdgarError("get_latest_10k",
                                  f"No 10-K filings found for {ticker}").to_dict()
            }

        latest = filings.latest(1)

        return {
            "success": True,
            "data": {
                "accession_number": latest.accession_number,
                "filing_date": str(latest.filing_date),
                "period_of_report": str(latest.period_of_report) if hasattr(latest, 'period_of_report') else None,
                "form": latest.form,
                "company": latest.company if hasattr(latest, 'company') else ticker,
                "cik": latest.cik if hasattr(latest, 'cik') else None,
                "filing": latest
            }
        }
    except Exception as e:
        return {"error": EdgarError("get_latest_10k", str(e), traceback.format_exc()).to_dict()}


def extract_10k_sections(ticker: str, sections: Optional[List[str]] = None) -> Dict[str, Any]:
    """
    Extract specific sections from 10-K filing

    Args:
        ticker: Stock ticker symbol
        sections: List of sections to extract (None = all)
                 Options: 'business', 'risk_factors', 'legal_proceedings',
                         'md&a', 'financials', 'controls', 'compensation',
                         'governance', 'all_items'

    Returns:
        Dictionary with requested sections
    """
    try:
        check_edgar_available()
        company = Company(ticker)
        filing = company.get_filings(form="10-K").latest(1)

        if not filing:
            return {
                "error": EdgarError("extract_10k_sections",
                                  f"No 10-K filing found for {ticker}").to_dict()
            }

        # Get 10-K object
        tenk = filing.obj()

        if not tenk:
            return {
                "error": EdgarError("extract_10k_sections",
                                  "Could not parse 10-K object").to_dict()
            }

        # Extract all sections if none specified
        if sections is None or 'all' in sections:
            sections = ['business', 'risk_factors', 'legal_proceedings', 'md&a',
                       'controls', 'compensation', 'governance', 'all_items']

        result = {
            "ticker": ticker,
            "filing_date": str(filing.filing_date),
            "period_of_report": str(filing.period_of_report) if hasattr(filing, 'period_of_report') else None,
            "accession_number": filing.accession_number,
            "sections": {}
        }

        # Extract Item 1: Business
        if 'business' in sections:
            try:
                business = tenk.business if hasattr(tenk, 'business') else None
                result["sections"]["business"] = {
                    "item": "Item 1",
                    "title": "Business",
                    "text": business,
                    "length": len(business) if business else 0
                }
            except Exception as e:
                result["sections"]["business"] = {"error": str(e)}

        # Extract Item 1A: Risk Factors
        if 'risk_factors' in sections:
            try:
                risk_factors = tenk.risk_factors if hasattr(tenk, 'risk_factors') else None
                result["sections"]["risk_factors"] = {
                    "item": "Item 1A",
                    "title": "Risk Factors",
                    "text": risk_factors,
                    "length": len(risk_factors) if risk_factors else 0
                }
            except Exception as e:
                result["sections"]["risk_factors"] = {"error": str(e)}

        # Extract Item 3: Legal Proceedings
        if 'legal_proceedings' in sections:
            try:
                # Try to get Item 3 via get_item_with_part
                if hasattr(tenk, 'get_item_with_part'):
                    legal = tenk.get_item_with_part('Item 3', 'Item 3')
                    result["sections"]["legal_proceedings"] = {
                        "item": "Item 3",
                        "title": "Legal Proceedings",
                        "text": str(legal) if legal else None,
                        "length": len(str(legal)) if legal else 0
                    }
                else:
                    result["sections"]["legal_proceedings"] = {"error": "Method not available"}
            except Exception as e:
                result["sections"]["legal_proceedings"] = {"error": str(e)}

        # Extract Item 7: MD&A
        if 'md&a' in sections or 'mda' in sections:
            try:
                mda = tenk.management_discussion if hasattr(tenk, 'management_discussion') else None
                result["sections"]["management_discussion"] = {
                    "item": "Item 7",
                    "title": "Management's Discussion and Analysis",
                    "text": mda,
                    "length": len(mda) if mda else 0
                }
            except Exception as e:
                result["sections"]["management_discussion"] = {"error": str(e)}

        # Extract Item 9A: Controls and Procedures
        if 'controls' in sections:
            try:
                if hasattr(tenk, 'get_item_with_part'):
                    controls = tenk.get_item_with_part('Item 9A', 'Item 9A')
                    result["sections"]["controls_procedures"] = {
                        "item": "Item 9A",
                        "title": "Controls and Procedures",
                        "text": str(controls) if controls else None,
                        "length": len(str(controls)) if controls else 0
                    }
                else:
                    result["sections"]["controls_procedures"] = {"error": "Method not available"}
            except Exception as e:
                result["sections"]["controls_procedures"] = {"error": str(e)}

        # Extract Item 10: Directors and Governance
        if 'governance' in sections:
            try:
                governance = tenk.directors_officers_and_governance if hasattr(tenk, 'directors_officers_and_governance') else None
                result["sections"]["directors_governance"] = {
                    "item": "Item 10",
                    "title": "Directors, Executive Officers and Corporate Governance",
                    "text": governance,
                    "length": len(governance) if governance else 0
                }
            except Exception as e:
                result["sections"]["directors_governance"] = {"error": str(e)}

        # Extract Item 11: Executive Compensation
        if 'compensation' in sections:
            try:
                if hasattr(tenk, 'get_item_with_part'):
                    compensation = tenk.get_item_with_part('Item 11', 'Item 11')
                    result["sections"]["executive_compensation"] = {
                        "item": "Item 11",
                        "title": "Executive Compensation",
                        "text": str(compensation) if compensation else None,
                        "length": len(str(compensation)) if compensation else 0
                    }
                else:
                    result["sections"]["executive_compensation"] = {"error": "Method not available"}
            except Exception as e:
                result["sections"]["executive_compensation"] = {"error": str(e)}

        # Extract all items list
        if 'all_items' in sections:
            try:
                items = tenk.items if hasattr(tenk, 'items') else []
                result["sections"]["all_items"] = {
                    "items": items,
                    "count": len(items) if items else 0
                }
            except Exception as e:
                result["sections"]["all_items"] = {"error": str(e)}

        return {
            "success": True,
            "data": result
        }

    except Exception as e:
        return {"error": EdgarError("extract_10k_sections", str(e), traceback.format_exc()).to_dict()}


def get_10k_full_text(ticker: str, max_length: Optional[int] = None) -> Dict[str, Any]:
    """
    Get full text content of 10-K filing

    Args:
        ticker: Stock ticker symbol
        max_length: Maximum text length (None = unlimited)

    Returns:
        Full text content of the filing
    """
    try:
        check_edgar_available()
        company = Company(ticker)
        filing = company.get_filings(form="10-K").latest(1)

        if not filing:
            return {
                "error": EdgarError("get_10k_full_text",
                                  f"No 10-K filing found for {ticker}").to_dict()
            }

        # Get text content
        text = filing.text() if hasattr(filing, 'text') else str(filing)

        if max_length and len(text) > max_length:
            text = text[:max_length] + "... (truncated)"

        return {
            "success": True,
            "data": {
                "ticker": ticker,
                "accession_number": filing.accession_number,
                "filing_date": str(filing.filing_date),
                "text": text,
                "text_length": len(text),
                "truncated": max_length is not None and len(text) > max_length
            }
        }
    except Exception as e:
        return {"error": EdgarError("get_10k_full_text", str(e), traceback.format_exc()).to_dict()}


def get_10k_markdown(ticker: str) -> Dict[str, Any]:
    """
    Get 10-K content in markdown format

    Args:
        ticker: Stock ticker symbol

    Returns:
        Markdown formatted content
    """
    try:
        check_edgar_available()
        company = Company(ticker)
        filing = company.get_filings(form="10-K").latest(1)

        if not filing:
            return {
                "error": EdgarError("get_10k_markdown",
                                  f"No 10-K filing found for {ticker}").to_dict()
            }

        # Get markdown content
        markdown = filing.markdown() if hasattr(filing, 'markdown') else filing.text()

        return {
            "success": True,
            "data": {
                "ticker": ticker,
                "accession_number": filing.accession_number,
                "filing_date": str(filing.filing_date),
                "markdown": markdown,
                "length": len(markdown)
            }
        }
    except Exception as e:
        return {"error": EdgarError("get_10k_markdown", str(e), traceback.format_exc()).to_dict()}


def search_10k(ticker: str, query: str, max_results: int = 10) -> Dict[str, Any]:
    """
    Search for text within 10-K filing

    Args:
        ticker: Stock ticker symbol
        query: Search query string
        max_results: Maximum number of results

    Returns:
        Search results with context
    """
    try:
        check_edgar_available()
        company = Company(ticker)
        filing = company.get_filings(form="10-K").latest(1)

        if not filing:
            return {
                "error": EdgarError("search_10k",
                                  f"No 10-K filing found for {ticker}").to_dict()
            }

        # Perform search
        if hasattr(filing, 'search'):
            results = filing.search(query)
        else:
            # Fallback: manual search in text
            text = filing.text()
            query_lower = query.lower()
            text_lower = text.lower()

            results = []
            start = 0
            while len(results) < max_results:
                pos = text_lower.find(query_lower, start)
                if pos == -1:
                    break

                # Get context (100 chars before and after)
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
                "results": results[:max_results] if isinstance(results, list) else str(results),
                "count": len(results) if isinstance(results, list) else 1
            }
        }
    except Exception as e:
        return {"error": EdgarError("search_10k", str(e), traceback.format_exc()).to_dict()}


def get_10k_exhibits(ticker: str) -> Dict[str, Any]:
    """
    Get exhibits and attachments from 10-K filing

    Args:
        ticker: Stock ticker symbol

    Returns:
        List of exhibits with metadata
    """
    try:
        check_edgar_available()
        company = Company(ticker)
        filing = company.get_filings(form="10-K").latest(1)

        if not filing:
            return {
                "error": EdgarError("get_10k_exhibits",
                                  f"No 10-K filing found for {ticker}").to_dict()
            }

        exhibits_list = []
        attachments_list = []

        # Get exhibits
        if hasattr(filing, 'exhibits'):
            exhibits = filing.exhibits
            if hasattr(exhibits, 'documents'):
                for doc in exhibits.documents:
                    exhibits_list.append({
                        "sequence": doc.sequence if hasattr(doc, 'sequence') else None,
                        "description": doc.description if hasattr(doc, 'description') else None,
                        "document": doc.document if hasattr(doc, 'document') else str(doc),
                        "type": getattr(doc, 'type', None)
                    })

        # Get attachments
        if hasattr(filing, 'attachments'):
            attachments = filing.attachments
            if hasattr(attachments, '__len__'):
                attachments_list.append({
                    "count": len(attachments),
                    "available": True
                })

        return {
            "success": True,
            "data": {
                "ticker": ticker,
                "accession_number": filing.accession_number,
                "exhibits": exhibits_list,
                "exhibits_count": len(exhibits_list),
                "attachments": attachments_list,
                "total_attachments": len(filing.attachments) if hasattr(filing, 'attachments') else 0
            }
        }
    except Exception as e:
        return {"error": EdgarError("get_10k_exhibits", str(e), traceback.format_exc()).to_dict()}


def get_10k_metadata(ticker: str) -> Dict[str, Any]:
    """
    Get comprehensive metadata for 10-K filing

    Args:
        ticker: Stock ticker symbol

    Returns:
        Filing metadata
    """
    try:
        check_edgar_available()
        company = Company(ticker)
        filing = company.get_filings(form="10-K").latest(1)

        if not filing:
            return {
                "error": EdgarError("get_10k_metadata",
                                  f"No 10-K filing found for {ticker}").to_dict()
            }

        tenk = filing.obj()

        return {
            "success": True,
            "data": {
                "ticker": ticker,
                "form": filing.form,
                "filing_date": str(filing.filing_date),
                "period_of_report": str(filing.period_of_report) if hasattr(filing, 'period_of_report') else None,
                "accession_number": filing.accession_number,
                "cik": filing.cik if hasattr(filing, 'cik') else None,
                "company": filing.company if hasattr(filing, 'company') else ticker,
                "file_number": filing.file_number if hasattr(filing, 'file_number') else None,
                "is_xbrl": filing.is_xbrl if hasattr(filing, 'is_xbrl') else False,
                "size": filing.size if hasattr(filing, 'size') else None,
                "items": tenk.items if tenk and hasattr(tenk, 'items') else [],
                "filing_url": filing.filing_url if hasattr(filing, 'filing_url') else None,
            }
        }
    except Exception as e:
        return {"error": EdgarError("get_10k_metadata", str(e), traceback.format_exc()).to_dict()}
