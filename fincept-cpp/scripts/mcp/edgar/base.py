"""
Edgar Base Utilities
====================

Core shared functionality for all Edgar MCP modules:
- EdgarTools library initialization
- User identity management
- Ticker symbol resolution and search
- Configuration and caching
- Error handling
"""

import os
import json
from typing import Dict, Any, Optional
from datetime import datetime
import traceback

# EdgarTools imports
try:
    from edgar import (
        Company, set_identity, get_identity,
        find_company, get_company_tickers,
        configure_http, clear_cache, storage_info
    )
    EDGAR_AVAILABLE = True
except ImportError as e:
    EDGAR_AVAILABLE = False
    IMPORT_ERROR = str(e)


class EdgarError:
    """Custom error class for Edgar API errors"""
    def __init__(self, command: str, error: str, details: Optional[str] = None):
        self.command = command
        self.error = error
        self.details = details
        self.timestamp = int(datetime.now().timestamp())

    def to_dict(self) -> Dict[str, Any]:
        result = {
            "command": self.command,
            "error": self.error,
            "timestamp": self.timestamp,
            "type": "EdgarError"
        }
        if self.details:
            result["details"] = self.details
        return result


def check_edgar_available():
    """Check if edgartools library is available"""
    if not EDGAR_AVAILABLE:
        raise ImportError(f"edgartools library not available: {IMPORT_ERROR}")


def initialize_edgar(identity: Optional[str] = None):
    """
    Initialize EdgarTools with user identity

    Args:
        identity: User identity string (name + email)
    """
    check_edgar_available()

    if identity:
        set_identity(identity)
    elif 'EDGAR_IDENTITY' not in os.environ:
        # Default identity for Fincept Terminal
        set_identity("Fincept Terminal nikultilak@gmail.com")


# ============================================================================
# CONFIGURATION
# ============================================================================

def set_user_identity(identity: str) -> Dict[str, Any]:
    """
    Set SEC user identity (required by SEC regulations)

    Args:
        identity: User identity string (should include name and email)

    Returns:
        Success status and configured identity
    """
    try:
        check_edgar_available()
        set_identity(identity)
        current = get_identity()
        return {
            "success": True,
            "identity": current,
            "message": "Identity set successfully"
        }
    except Exception as e:
        return {"error": EdgarError("set_identity", str(e)).to_dict()}


def get_user_identity() -> Dict[str, Any]:
    """Get currently configured SEC user identity"""
    try:
        check_edgar_available()
        identity = get_identity()
        return {
            "success": True,
            "identity": identity
        }
    except Exception as e:
        return {"error": EdgarError("get_identity", str(e)).to_dict()}


def configure_connection(verify_ssl: Optional[bool] = None,
                        proxy: Optional[str] = None,
                        timeout: Optional[float] = None) -> Dict[str, Any]:
    """Configure HTTP connection settings"""
    try:
        check_edgar_available()
        configure_http(verify_ssl=verify_ssl, proxy=proxy, timeout=timeout)
        return {
            "success": True,
            "message": "HTTP configuration updated",
            "settings": {
                "verify_ssl": verify_ssl,
                "proxy": proxy,
                "timeout": timeout
            }
        }
    except Exception as e:
        return {"error": EdgarError("configure_http", str(e)).to_dict()}


def clear_local_cache(dry_run: bool = True) -> Dict[str, Any]:
    """Clear local cache (dry_run=True for preview)"""
    try:
        check_edgar_available()
        result = clear_cache(dry_run=dry_run)
        return {
            "success": True,
            "dry_run": dry_run,
            "statistics": result
        }
    except Exception as e:
        return {"error": EdgarError("clear_cache", str(e)).to_dict()}


def get_storage_info() -> Dict[str, Any]:
    """Get storage and cache information"""
    try:
        check_edgar_available()
        info = storage_info()
        return {
            "success": True,
            "storage_info": str(info)
        }
    except Exception as e:
        return {"error": EdgarError("storage_info", str(e)).to_dict()}


# ============================================================================
# TICKER RESOLUTION & SEARCH
# ============================================================================

def find_company_by_name(query: str, top_n: int = 10) -> Dict[str, Any]:
    """
    Search for companies by name with fuzzy matching

    Args:
        query: Company name or partial name
        top_n: Number of top results to return

    Returns:
        List of matching companies with tickers and CIKs
    """
    try:
        check_edgar_available()
        results = find_company(query)

        if not results:
            return {
                "success": True,
                "data": [],
                "count": 0,
                "query": query
            }

        # Convert results to list
        companies = []
        for i, company in enumerate(results):
            if i >= top_n:
                break
            companies.append({
                "ticker": company.tickers[0] if company.tickers else None,
                "cik": company.cik,
                "name": company.name,
                "industry": company.industry if hasattr(company, 'industry') else None,
            })

        return {
            "success": True,
            "data": companies,
            "count": len(companies),
            "query": query
        }
    except Exception as e:
        return {"error": EdgarError("find_company", str(e), traceback.format_exc()).to_dict()}


def resolve_ticker(query: str) -> Optional[str]:
    """
    Resolve company name to ticker symbol

    Args:
        query: Company name or ticker

    Returns:
        Ticker symbol or None if not found
    """
    try:
        check_edgar_available()

        # First try direct Company lookup (in case it's already a ticker)
        try:
            company = Company(query)
            if company and company.tickers:
                return company.tickers[0]
        except:
            pass

        # Search by name
        results = find_company(query)
        if results:
            first = results[0] if hasattr(results, '__getitem__') else next(iter(results))
            if first.tickers:
                return first.tickers[0]

        return None
    except:
        return None


def get_company_info(ticker: str) -> Dict[str, Any]:
    """
    Get detailed company information by ticker

    Args:
        ticker: Stock ticker symbol

    Returns:
        Company information including CIK, name, industry, etc.
    """
    try:
        check_edgar_available()
        company = Company(ticker)

        return {
            "success": True,
            "data": {
                "ticker": ticker,
                "cik": company.cik,
                "name": company.name,
                "tickers": company.tickers,
                "industry": company.industry if hasattr(company, 'industry') else None,
                "sic": company.sic if hasattr(company, 'sic') else None,
                "fiscal_year_end": company.fiscal_year_end if hasattr(company, 'fiscal_year_end') else None,
                "business_address": str(company.business_address()) if hasattr(company, 'business_address') and company.business_address() else None,
            }
        }
    except Exception as e:
        return {"error": EdgarError("get_company", str(e), traceback.format_exc()).to_dict()}


def get_all_company_tickers(limit: int = 1000) -> Dict[str, Any]:
    """
    Get list of all company tickers from SEC

    Args:
        limit: Maximum number of tickers to return

    Returns:
        List of companies with tickers and CIKs
    """
    try:
        check_edgar_available()
        tickers = get_company_tickers()

        if not tickers:
            return {
                "success": True,
                "data": [],
                "count": 0
            }

        # Convert to list and limit
        companies_list = []
        for i, (cik, ticker_data) in enumerate(tickers.items()):
            if i >= limit:
                break
            companies_list.append({
                "cik": cik,
                "ticker": ticker_data.get('ticker'),
                "title": ticker_data.get('title'),
            })

        return {
            "success": True,
            "data": companies_list,
            "count": len(companies_list),
            "total": len(tickers)
        }
    except Exception as e:
        return {"error": EdgarError("get_company_tickers", str(e), traceback.format_exc()).to_dict()}
