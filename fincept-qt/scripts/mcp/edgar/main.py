"""
Edgar MCP - Main CLI Entry Point
==================================

Unified command-line interface for all Edgar MCP modules.

Usage:
    python -m mcp.edgar.main <command> [args...]

Commands are organized by module:
- base: Configuration and ticker search
- forms_10k: 10-K annual report extraction
- forms_10q: 10-Q quarterly report extraction
- financials: XBRL financial data

Example:
    python -m mcp.edgar.main 10k_sections AAPL business risk_factors
"""

import os
import sys
import json
import traceback
from typing import Dict, Any

# Import all modules (base first: it repairs the SSL context before edgartools loads)
from . import base
from . import forms_10k
from . import forms_10q
from . import forms_8k
from . import forms_insider
from . import forms_13f
from . import financials
from . import research

try:
    import fincept_json  # NaN/Infinity-safe JSON boundary shared by the app's scripts
except ImportError:  # scripts/ not on sys.path (unusual launcher)
    sys.path.append(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))
    import fincept_json


def execute_command(command: str, args: list) -> Dict[str, Any]:
    """
    Execute a command with arguments

    Args:
        command: Command name
        args: Command arguments

    Returns:
        Result dictionary
    """

    # =========================================================================
    # BASE COMMANDS - Configuration and Search
    # =========================================================================

    if command == "set_identity":
        identity = args[0] if len(args) > 0 else None
        return base.set_user_identity(identity)

    elif command == "get_identity":
        return base.get_user_identity()

    elif command == "find_company":
        query = args[0] if len(args) > 0 else None
        top_n = int(args[1]) if len(args) > 1 else 10
        return base.find_company_by_name(query, top_n)

    elif command == "get_company":
        ticker = args[0] if len(args) > 0 else None
        return base.get_company_info(ticker)

    elif command == "resolve_ticker":
        query = args[0] if len(args) > 0 else None
        ticker = base.resolve_ticker(query)
        return {"success": True, "ticker": ticker}

    # =========================================================================
    # 10-K COMMANDS - Annual Reports
    # =========================================================================

    elif command == "10k_latest":
        ticker = args[0] if len(args) > 0 else None
        return forms_10k.get_latest_10k(ticker)

    elif command == "10k_sections":
        ticker = args[0] if len(args) > 0 else None
        sections = args[1:] if len(args) > 1 else None
        return forms_10k.extract_10k_sections(ticker, sections)

    elif command == "10k_full_text":
        ticker = args[0] if len(args) > 0 else None
        max_length = int(args[1]) if len(args) > 1 else None
        return forms_10k.get_10k_full_text(ticker, max_length)

    elif command == "10k_markdown":
        ticker = args[0] if len(args) > 0 else None
        return forms_10k.get_10k_markdown(ticker)

    elif command == "10k_search":
        ticker = args[0] if len(args) > 0 else None
        query = args[1] if len(args) > 1 else ""
        max_results = int(args[2]) if len(args) > 2 else 10
        return forms_10k.search_10k(ticker, query, max_results)

    elif command == "10k_exhibits":
        ticker = args[0] if len(args) > 0 else None
        return forms_10k.get_10k_exhibits(ticker)

    elif command == "10k_metadata":
        ticker = args[0] if len(args) > 0 else None
        return forms_10k.get_10k_metadata(ticker)

    # =========================================================================
    # 10-Q COMMANDS - Quarterly Reports
    # =========================================================================

    elif command == "10q_latest":
        ticker = args[0] if len(args) > 0 else None
        return forms_10q.get_latest_10q(ticker)

    elif command == "10q_sections":
        ticker = args[0] if len(args) > 0 else None
        sections = args[1:] if len(args) > 1 else None
        return forms_10q.extract_10q_sections(ticker, sections)

    elif command == "10q_full_text":
        ticker = args[0] if len(args) > 0 else None
        max_length = int(args[1]) if len(args) > 1 else None
        return forms_10q.get_10q_full_text(ticker, max_length)

    elif command == "10q_markdown":
        ticker = args[0] if len(args) > 0 else None
        return forms_10q.get_10q_markdown(ticker)

    elif command == "10q_metadata":
        ticker = args[0] if len(args) > 0 else None
        return forms_10q.get_10q_metadata(ticker)

    elif command == "10q_search":
        ticker = args[0] if len(args) > 0 else None
        query = args[1] if len(args) > 1 else ""
        max_results = int(args[2]) if len(args) > 2 else 10
        return forms_10q.search_10q(ticker, query, max_results)

    # =========================================================================
    # FINANCIALS COMMANDS - XBRL Data
    # =========================================================================

    elif command == "get_financials":
        ticker = args[0] if len(args) > 0 else None
        periods = int(args[1]) if len(args) > 1 else 4
        annual = args[2].lower() != "false" if len(args) > 2 else True
        return financials.get_financials(ticker, periods, annual)

    elif command == "get_financial_metrics":
        ticker = args[0] if len(args) > 0 else None
        return financials.get_financial_metrics(ticker)

    elif command == "get_xbrl_statements":
        ticker = args[0] if len(args) > 0 else None
        form = args[1] if len(args) > 1 else "10-K"
        return financials.get_xbrl_statements(ticker, form)

    elif command == "get_company_facts":
        cik = args[0] if len(args) > 0 else None
        return financials.get_company_facts(cik)

    # =========================================================================
    # 8-K COMMANDS - Corporate Events
    # =========================================================================

    elif command == "8k_latest":
        ticker = args[0] if len(args) > 0 else None
        return forms_8k.get_latest_8k(ticker)

    elif command == "8k_events":
        ticker = args[0] if len(args) > 0 else None
        limit = int(args[1]) if len(args) > 1 else 30
        return forms_8k.get_8k_events(ticker, limit)

    elif command == "8k_events_categorized":
        ticker = args[0] if len(args) > 0 else None
        limit = int(args[1]) if len(args) > 1 else 30
        return forms_8k.get_8k_events_categorized(ticker, limit)

    elif command == "8k_full_text":
        ticker = args[0] if len(args) > 0 else None
        max_length = int(args[1]) if len(args) > 1 else None
        return forms_8k.get_8k_full_text(ticker, max_length)

    elif command == "8k_search":
        ticker = args[0] if len(args) > 0 else None
        query = args[1] if len(args) > 1 else ""
        max_results = int(args[2]) if len(args) > 2 else 10
        return forms_8k.search_8k(ticker, query, max_results)

    # =========================================================================
    # FORM 4 COMMANDS - Insider Transactions
    # =========================================================================

    elif command == "insider_transactions":
        ticker = args[0] if len(args) > 0 else None
        limit = int(args[1]) if len(args) > 1 else 25
        return forms_insider.get_insider_transactions(ticker, limit)

    elif command == "insider_transactions_detailed":
        ticker = args[0] if len(args) > 0 else None
        limit = int(args[1]) if len(args) > 1 else 25
        return forms_insider.get_insider_transactions_detailed(ticker, limit)

    elif command == "insider_summary":
        ticker = args[0] if len(args) > 0 else None
        limit = int(args[1]) if len(args) > 1 else 50
        return forms_insider.get_insider_summary(ticker, limit)

    # =========================================================================
    # 13F COMMANDS - Institutional Holdings
    # =========================================================================

    elif command == "13f_holdings":
        ticker = args[0] if len(args) > 0 else None
        quarters = int(args[1]) if len(args) > 1 else 2
        return forms_13f.get_13f_holdings(ticker, quarters)

    elif command == "13f_top_holdings":
        ticker = args[0] if len(args) > 0 else None
        top_n = int(args[1]) if len(args) > 1 else 20
        return forms_13f.get_13f_top_holdings(ticker, top_n)

    elif command == "13f_manager_info":
        ticker = args[0] if len(args) > 0 else None
        return forms_13f.get_13f_manager_info(ticker)

    elif command == "13f_summary":
        ticker = args[0] if len(args) > 0 else None
        return forms_13f.get_13f_summary(ticker)

    # =========================================================================
    # DIRECT SEC ACCESS — CIK resolution, filing text, multiples
    # =========================================================================

    elif command == "resolve_cik":
        query = args[0] if args else None
        return base.resolve_cik(query)

    elif command == "get_filing_text":
        ticker_or_cik = args[0] if args else None
        form = args[1] if len(args) > 1 else "10-K"
        return base.get_filing_text(ticker_or_cik, form)

    elif command == "search_filings":
        ticker_or_cik = args[0] if args else None
        form = args[1] if len(args) > 1 else "8-K"
        months_back = int(args[2]) if len(args) > 2 else 12
        return base.search_filings(ticker_or_cik, form, months_back)

    elif command == "get_filing_documents":
        ticker_or_cik = args[0] if args else None
        form = args[1] if len(args) > 1 else "10-K"
        return base.get_filing_documents(ticker_or_cik, form)

    elif command == "calc_multiples":
        ticker_or_cik = args[0] if args else None
        deal_value = float(args[1]) if len(args) > 1 else 0.0
        return base.calc_multiples(ticker_or_cik, deal_value)

    # =========================================================================
    # RESEARCH VIEWS — Equity Research FILINGS tab (compact, display-ready)
    # =========================================================================

    elif command == "research_filings":
        limit = int(args[1]) if len(args) > 1 else 150
        return research.research_filings(args[0] if args else None, limit)

    elif command == "research_financials":
        return research.research_financials(args[0] if args else None)

    elif command == "research_insiders":
        limit = int(args[1]) if len(args) > 1 else 25
        return research.research_insiders(args[0] if args else None, limit)

    elif command == "research_holders":
        top_n = int(args[1]) if len(args) > 1 else 10
        return research.research_holders(args[0] if args else None, top_n)

    # Unknown command
    else:
        return {
            "error": {
                "command": "unknown",
                "error": f"Unknown command: {command}",
                "available_commands": {
                    "base": ["set_identity", "get_identity", "find_company", "get_company", "resolve_ticker"],
                    "10k": ["10k_latest", "10k_sections", "10k_full_text", "10k_markdown", "10k_search", "10k_exhibits", "10k_metadata"],
                    "10q": ["10q_latest", "10q_sections", "10q_full_text", "10q_markdown", "10q_metadata", "10q_search"],
                    "8k": ["8k_latest", "8k_events", "8k_events_categorized", "8k_full_text", "8k_search"],
                    "insider": ["insider_transactions", "insider_transactions_detailed", "insider_summary"],
                    "13f": ["13f_holdings", "13f_top_holdings", "13f_manager_info", "13f_summary"],
                    "financials": ["get_financials", "get_financial_metrics", "get_xbrl_statements", "get_company_facts"],
                    "research": ["research_filings", "research_financials", "research_insiders", "research_holders"]
                }
            }
        }


def main():
    """Main CLI entry point"""
    if len(sys.argv) < 2:
        print(json.dumps({
            "error": "Usage: python -m mcp.edgar.main <command> [args...]",
            "examples": [
                "python -m mcp.edgar.main find_company Apple",
                "python -m mcp.edgar.main 10k_sections AAPL business risk_factors",
                "python -m mcp.edgar.main 10q_sections MSFT part_i_item_2",
                "python -m mcp.edgar.main get_financials TSLA 4 true",
            ],
            "command_groups": {
                "base": "Configuration and company search",
                "10k": "10-K annual report extraction",
                "10q": "10-Q quarterly report extraction",
                "financials": "XBRL financial data"
            }
        }, indent=2))
        sys.exit(1)

    command = sys.argv[1]
    args = sys.argv[2:]

    # A bad integer argument or a missing edgartools install used to escape as a bare
    # traceback on stderr; the host then had no JSON envelope to read the failure from.
    try:
        base.initialize_edgar()
        result = execute_command(command, args)
    except Exception as exc:
        result = {"success": False, "error": f"{command}: {exc}", "details": traceback.format_exc(limit=4)}

    # One line, and never the bare NaN/Infinity tokens json.dumps emits by default —
    # QJsonDocument rejects the whole document on the first one.
    print(fincept_json.dumps(result))


if __name__ == "__main__":
    main()
