"""
10-Q Quarterly Report Extraction
=================================

Complete extraction of 10-Q quarterly reports including:
- Part I: Financial Information
  - Item 1: Financial Statements
  - Item 2: Management's Discussion and Analysis
  - Item 3: Quantitative and Qualitative Disclosures About Market Risk
  - Item 4: Controls and Procedures
- Part II: Other Information
  - Item 1: Legal Proceedings
  - Item 1A: Risk Factors
  - Item 2: Unregistered Sales of Equity Securities
  - Item 5: Other Information
  - Item 6: Exhibits
- Full text and search capabilities
"""

from typing import Dict, Any, Optional, List
import traceback

try:
    from edgar import Company
    EDGAR_AVAILABLE = True
except ImportError:
    EDGAR_AVAILABLE = False

from .base import EdgarError, check_edgar_available


def get_latest_10q(ticker: str) -> Dict[str, Any]:
    """
    Get latest 10-Q filing for a company

    Args:
        ticker: Stock ticker symbol

    Returns:
        10-Q filing object and metadata
    """
    try:
        check_edgar_available()
        company = Company(ticker)
        filings = company.get_filings(form="10-Q")

        if not filings or len(filings) == 0:
            return {
                "error": EdgarError("get_latest_10q",
                                  f"No 10-Q filings found for {ticker}").to_dict()
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
        return {"error": EdgarError("get_latest_10q", str(e), traceback.format_exc()).to_dict()}


def extract_10q_sections(ticker: str, sections: Optional[List[str]] = None) -> Dict[str, Any]:
    """
    Extract specific sections from 10-Q filing

    Args:
        ticker: Stock ticker symbol
        sections: List of sections to extract (None = all)
                 Options: 'part_i_item_1' (Financial Statements),
                         'part_i_item_2' (MD&A),
                         'part_i_item_3' (Market Risk),
                         'part_i_item_4' (Controls),
                         'part_i_item_8' (Financial Statements - alternate),
                         'part_ii_item_1' (Legal Proceedings),
                         'part_ii_item_1a' (Risk Factors),
                         'part_ii_item_2' (Sales of Equity),
                         'part_ii_item_5' (Other Information),
                         'part_ii_item_6' (Exhibits)

    Returns:
        Dictionary with requested sections
    """
    try:
        check_edgar_available()
        company = Company(ticker)
        filing = company.get_filings(form="10-Q").latest(1)

        if not filing:
            return {
                "error": EdgarError("extract_10q_sections",
                                  f"No 10-Q filing found for {ticker}").to_dict()
            }

        # Get 10-Q object
        tenq = filing.obj()

        if not tenq:
            return {
                "error": EdgarError("extract_10q_sections",
                                  "Could not parse 10-Q object").to_dict()
            }

        # Extract all sections if none specified
        if sections is None or 'all' in sections:
            sections = ['part_i_item_1', 'part_i_item_2', 'part_i_item_3', 'part_i_item_4',
                       'part_i_item_8', 'part_ii_item_1', 'part_ii_item_1a', 'part_ii_item_2',
                       'part_ii_item_5', 'part_ii_item_6']

        result = {
            "ticker": ticker,
            "filing_date": str(filing.filing_date),
            "period_of_report": str(filing.period_of_report) if hasattr(filing, 'period_of_report') else None,
            "accession_number": filing.accession_number,
            "sections": {}
        }

        # Get sections object if available
        sections_obj = tenq.sections if hasattr(tenq, 'sections') else None

        # PART I: Financial Information

        # Part I, Item 1: Financial Statements
        if 'part_i_item_1' in sections:
            try:
                if sections_obj and hasattr(sections_obj, '__getitem__'):
                    financials = sections_obj.get('part_i_item_1') or sections_obj.get('part_i_item_8')
                    result["sections"]["part_i_item_1"] = {
                        "part": "Part I",
                        "item": "Item 1",
                        "title": "Financial Statements",
                        "text": financials,
                        "length": len(str(financials)) if financials else 0
                    }
                else:
                    result["sections"]["part_i_item_1"] = {"error": "Sections not accessible"}
            except Exception as e:
                result["sections"]["part_i_item_1"] = {"error": str(e)}

        # Part I, Item 2: Management's Discussion and Analysis
        if 'part_i_item_2' in sections:
            try:
                if sections_obj and hasattr(sections_obj, '__getitem__'):
                    mda = sections_obj.get('part_i_item_2')
                    result["sections"]["part_i_item_2"] = {
                        "part": "Part I",
                        "item": "Item 2",
                        "title": "Management's Discussion and Analysis of Financial Condition and Results of Operations",
                        "text": mda,
                        "length": len(str(mda)) if mda else 0
                    }
                else:
                    result["sections"]["part_i_item_2"] = {"error": "Sections not accessible"}
            except Exception as e:
                result["sections"]["part_i_item_2"] = {"error": str(e)}

        # Part I, Item 3: Market Risk
        if 'part_i_item_3' in sections:
            try:
                if sections_obj and hasattr(sections_obj, '__getitem__'):
                    market_risk = sections_obj.get('part_i_item_3')
                    result["sections"]["part_i_item_3"] = {
                        "part": "Part I",
                        "item": "Item 3",
                        "title": "Quantitative and Qualitative Disclosures About Market Risk",
                        "text": market_risk,
                        "length": len(str(market_risk)) if market_risk else 0
                    }
                else:
                    result["sections"]["part_i_item_3"] = {"error": "Sections not accessible"}
            except Exception as e:
                result["sections"]["part_i_item_3"] = {"error": str(e)}

        # Part I, Item 4: Controls and Procedures
        if 'part_i_item_4' in sections:
            try:
                if sections_obj and hasattr(sections_obj, '__getitem__'):
                    controls = sections_obj.get('part_i_item_4')
                    result["sections"]["part_i_item_4"] = {
                        "part": "Part I",
                        "item": "Item 4",
                        "title": "Controls and Procedures",
                        "text": controls,
                        "length": len(str(controls)) if controls else 0
                    }
                else:
                    result["sections"]["part_i_item_4"] = {"error": "Sections not accessible"}
            except Exception as e:
                result["sections"]["part_i_item_4"] = {"error": str(e)}

        # PART II: Other Information

        # Part II, Item 1: Legal Proceedings
        if 'part_ii_item_1' in sections:
            try:
                if sections_obj and hasattr(sections_obj, '__getitem__'):
                    legal = sections_obj.get('part_ii_item_1')
                    result["sections"]["part_ii_item_1"] = {
                        "part": "Part II",
                        "item": "Item 1",
                        "title": "Legal Proceedings",
                        "text": legal,
                        "length": len(str(legal)) if legal else 0
                    }
                else:
                    result["sections"]["part_ii_item_1"] = {"error": "Sections not accessible"}
            except Exception as e:
                result["sections"]["part_ii_item_1"] = {"error": str(e)}

        # Part II, Item 1A: Risk Factors
        if 'part_ii_item_1a' in sections:
            try:
                if sections_obj and hasattr(sections_obj, '__getitem__'):
                    risk_factors = sections_obj.get('part_ii_item_1a')
                    result["sections"]["part_ii_item_1a"] = {
                        "part": "Part II",
                        "item": "Item 1A",
                        "title": "Risk Factors",
                        "text": risk_factors,
                        "length": len(str(risk_factors)) if risk_factors else 0
                    }
                else:
                    result["sections"]["part_ii_item_1a"] = {"error": "Sections not accessible"}
            except Exception as e:
                result["sections"]["part_ii_item_1a"] = {"error": str(e)}

        # Part II, Item 2: Unregistered Sales of Equity Securities
        if 'part_ii_item_2' in sections:
            try:
                if sections_obj and hasattr(sections_obj, '__getitem__'):
                    sales = sections_obj.get('part_ii_item_2')
                    result["sections"]["part_ii_item_2"] = {
                        "part": "Part II",
                        "item": "Item 2",
                        "title": "Unregistered Sales of Equity Securities and Use of Proceeds",
                        "text": sales,
                        "length": len(str(sales)) if sales else 0
                    }
                else:
                    result["sections"]["part_ii_item_2"] = {"error": "Sections not accessible"}
            except Exception as e:
                result["sections"]["part_ii_item_2"] = {"error": str(e)}

        # Part I, Item 8: Financial Statements (alternate key)
        if 'part_i_item_8' in sections:
            try:
                if sections_obj and hasattr(sections_obj, '__getitem__'):
                    financials_alt = sections_obj.get('part_i_item_8')
                    result["sections"]["part_i_item_8"] = {
                        "part": "Part I",
                        "item": "Item 8",
                        "title": "Financial Statements (alternate)",
                        "text": financials_alt,
                        "length": len(str(financials_alt)) if financials_alt else 0
                    }
                else:
                    result["sections"]["part_i_item_8"] = {"error": "Sections not accessible"}
            except Exception as e:
                result["sections"]["part_i_item_8"] = {"error": str(e)}

        # Part II, Item 5: Other Information
        if 'part_ii_item_5' in sections:
            try:
                if sections_obj and hasattr(sections_obj, '__getitem__'):
                    other = sections_obj.get('part_ii_item_5')
                    result["sections"]["part_ii_item_5"] = {
                        "part": "Part II",
                        "item": "Item 5",
                        "title": "Other Information",
                        "text": other,
                        "length": len(str(other)) if other else 0
                    }
                else:
                    result["sections"]["part_ii_item_5"] = {"error": "Sections not accessible"}
            except Exception as e:
                result["sections"]["part_ii_item_5"] = {"error": str(e)}

        # Part II, Item 6: Exhibits
        if 'part_ii_item_6' in sections:
            try:
                if sections_obj and hasattr(sections_obj, '__getitem__'):
                    exhibits = sections_obj.get('part_ii_item_6')
                    result["sections"]["part_ii_item_6"] = {
                        "part": "Part II",
                        "item": "Item 6",
                        "title": "Exhibits",
                        "text": exhibits,
                        "length": len(str(exhibits)) if exhibits else 0
                    }
                else:
                    result["sections"]["part_ii_item_6"] = {"error": "Sections not accessible"}
            except Exception as e:
                result["sections"]["part_ii_item_6"] = {"error": str(e)}

        # Add available items list
        try:
            items = tenq.items if hasattr(tenq, 'items') else []
            result["available_items"] = items
            result["items_count"] = len(items) if items else 0
        except:
            pass

        return {
            "success": True,
            "data": result
        }

    except Exception as e:
        return {"error": EdgarError("extract_10q_sections", str(e), traceback.format_exc()).to_dict()}


def get_10q_full_text(ticker: str, max_length: Optional[int] = None) -> Dict[str, Any]:
    """
    Get full text content of 10-Q filing

    Args:
        ticker: Stock ticker symbol
        max_length: Maximum text length (None = unlimited)

    Returns:
        Full text content of the filing
    """
    try:
        check_edgar_available()
        company = Company(ticker)
        filing = company.get_filings(form="10-Q").latest(1)

        if not filing:
            return {
                "error": EdgarError("get_10q_full_text",
                                  f"No 10-Q filing found for {ticker}").to_dict()
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
        return {"error": EdgarError("get_10q_full_text", str(e), traceback.format_exc()).to_dict()}


def get_10q_markdown(ticker: str) -> Dict[str, Any]:
    """
    Get 10-Q content in markdown format

    Args:
        ticker: Stock ticker symbol

    Returns:
        Markdown formatted content
    """
    try:
        check_edgar_available()
        company = Company(ticker)
        filing = company.get_filings(form="10-Q").latest(1)

        if not filing:
            return {
                "error": EdgarError("get_10q_markdown",
                                  f"No 10-Q filing found for {ticker}").to_dict()
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
        return {"error": EdgarError("get_10q_markdown", str(e), traceback.format_exc()).to_dict()}


def get_10q_metadata(ticker: str) -> Dict[str, Any]:
    """
    Get comprehensive metadata for 10-Q filing

    Args:
        ticker: Stock ticker symbol

    Returns:
        Filing metadata
    """
    try:
        check_edgar_available()
        company = Company(ticker)
        filing = company.get_filings(form="10-Q").latest(1)

        if not filing:
            return {
                "error": EdgarError("get_10q_metadata",
                                  f"No 10-Q filing found for {ticker}").to_dict()
            }

        tenq = filing.obj()

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
                "items": tenq.items if tenq and hasattr(tenq, 'items') else [],
                "filing_url": filing.filing_url if hasattr(filing, 'filing_url') else None,
            }
        }
    except Exception as e:
        return {"error": EdgarError("get_10q_metadata", str(e), traceback.format_exc()).to_dict()}


def search_10q(ticker: str, query: str, max_results: int = 10) -> Dict[str, Any]:
    """
    Search for text within 10-Q filing

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
        filing = company.get_filings(form="10-Q").latest(1)

        if not filing:
            return {
                "error": EdgarError("search_10q",
                                  f"No 10-Q filing found for {ticker}").to_dict()
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
        return {"error": EdgarError("search_10q", str(e), traceback.format_exc()).to_dict()}
