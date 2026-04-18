"""
EdgarTools — Agno-compatible toolkit wrapping the internal Fincept Edgar MCP.

Exposes SEC EDGAR data as LLM-callable tools:
  - Company search and info
  - 10-K annual reports (sections, full text, exhibits, search)
  - 10-Q quarterly reports
  - 8-K corporate events
  - Form 4 insider transactions
  - 13F institutional holdings
  - XBRL financial data

Usage in agent config:
  "tools": ["edgar"]

The LLM can then call e.g.:
  get_10k_sections(ticker="AAPL", sections="business,risk_factors")
  get_insider_transactions(ticker="TSLA")
  get_financials(ticker="NVDA")
"""

import json
import logging
import sys
from pathlib import Path
from typing import Optional

logger = logging.getLogger(__name__)

# Ensure the scripts root is on sys.path so mcp.edgar is importable
_SCRIPTS_ROOT = str(Path(__file__).parent.parent.parent.parent)
if _SCRIPTS_ROOT not in sys.path:
    sys.path.insert(0, _SCRIPTS_ROOT)


def _call(command: str, *args) -> str:
    """
    Call execute_command from the internal Edgar MCP and return a compact
    JSON string suitable for passing back to the LLM.
    """
    try:
        from mcp.edgar import main as edgar_main
        from mcp.edgar import base as edgar_base
        # Auto-initialize identity on first call
        edgar_base.initialize_edgar()
        result = edgar_main.execute_command(command, list(str(a) for a in args))
        # Return compact JSON; cap at 8 000 chars so the LLM isn't overwhelmed
        text = json.dumps(result, default=str)
        if len(text) > 8_000:
            text = text[:8_000] + "... [truncated]"
        return text
    except ImportError:
        return json.dumps({"error": "edgartools library not installed. Run: pip install edgartools"})
    except Exception as e:
        return json.dumps({"error": str(e)})


# ---------------------------------------------------------------------------
# Agno Toolkit
# ---------------------------------------------------------------------------

try:
    from agno.tools import Toolkit

    class EdgarTools(Toolkit):
        """
        SEC EDGAR toolkit for Fincept agents.

        Provides access to:
        - Company search and profile data
        - 10-K annual report sections
        - 10-Q quarterly report sections
        - 8-K corporate event filings
        - Form 4 insider transaction data
        - 13F institutional holdings
        - XBRL financial statements and metrics
        """

        def __init__(self, **kwargs):
            super().__init__(name="edgar", **kwargs)
            # Register all tools
            self.register(self.find_company)
            self.register(self.get_company_info)
            self.register(self.get_10k_sections)
            self.register(self.get_10k_full_text)
            self.register(self.search_10k)
            self.register(self.get_10q_sections)
            self.register(self.get_10q_full_text)
            self.register(self.get_8k_events)
            self.register(self.get_insider_transactions)
            self.register(self.get_insider_summary)
            self.register(self.get_13f_holdings)
            self.register(self.get_13f_top_holdings)
            self.register(self.get_financials)
            self.register(self.get_financial_metrics)

        # ── Company Search ────────────────────────────────────────────────

        def find_company(self, query: str) -> str:
            """
            Search for companies on SEC EDGAR by name.

            Args:
                query: Company name or partial name (e.g. "Apple", "Tesla").

            Returns:
                JSON list of matching companies with ticker, CIK, and name.
            """
            return _call("find_company", query)

        def get_company_info(self, ticker: str) -> str:
            """
            Get detailed company information from SEC EDGAR.

            Args:
                ticker: Stock ticker symbol (e.g. "AAPL", "MSFT").

            Returns:
                JSON with CIK, name, industry, SIC code, fiscal year end.
            """
            return _call("get_company", ticker)

        # ── 10-K Annual Reports ───────────────────────────────────────────

        def get_10k_sections(self, ticker: str, sections: str = "business") -> str:
            """
            Extract specific sections from a company's latest 10-K annual report.

            Args:
                ticker: Stock ticker symbol (e.g. "AAPL").
                sections: Comma-separated section names. Valid values:
                    business, risk_factors, mda (Management Discussion & Analysis),
                    financials, controls, compensation, governance, all_items.
                    Default: "business".

            Returns:
                JSON with the requested section texts.
            """
            section_list = [s.strip() for s in sections.split(",")]
            return _call("10k_sections", ticker, *section_list)

        def get_10k_full_text(self, ticker: str, max_length: int = 20000) -> str:
            """
            Get the full text of a company's latest 10-K filing.

            Args:
                ticker: Stock ticker symbol (e.g. "AAPL").
                max_length: Maximum characters to return (default 20000).

            Returns:
                JSON with the 10-K full text (truncated to max_length).
            """
            return _call("10k_full_text", ticker, str(max_length))

        def search_10k(self, ticker: str, query: str, max_results: int = 10) -> str:
            """
            Search within a company's 10-K filing for specific topics.

            Args:
                ticker: Stock ticker symbol (e.g. "AAPL").
                query: Search query (e.g. "revenue recognition", "litigation").
                max_results: Maximum number of results (default 10).

            Returns:
                JSON with matching passages and their context.
            """
            return _call("10k_search", ticker, query, str(max_results))

        # ── 10-Q Quarterly Reports ────────────────────────────────────────

        def get_10q_sections(self, ticker: str, sections: str = "part_i_item_2") -> str:
            """
            Extract specific sections from a company's latest 10-Q quarterly report.

            Args:
                ticker: Stock ticker symbol (e.g. "AAPL").
                sections: Comma-separated section names. Valid values:
                    part_i_item_1 (Financial Statements),
                    part_i_item_2 (MD&A),
                    part_i_item_3 (Quantitative Disclosures),
                    part_i_item_4 (Controls),
                    part_ii_item_1 (Legal Proceedings),
                    all_items.
                    Default: "part_i_item_2".

            Returns:
                JSON with the requested section texts.
            """
            section_list = [s.strip() for s in sections.split(",")]
            return _call("10q_sections", ticker, *section_list)

        def get_10q_full_text(self, ticker: str, max_length: int = 15000) -> str:
            """
            Get the full text of a company's latest 10-Q filing.

            Args:
                ticker: Stock ticker symbol (e.g. "AAPL").
                max_length: Maximum characters to return (default 15000).

            Returns:
                JSON with the 10-Q full text.
            """
            return _call("10q_full_text", ticker, str(max_length))

        # ── 8-K Corporate Events ──────────────────────────────────────────

        def get_8k_events(self, ticker: str, limit: int = 20) -> str:
            """
            Get recent 8-K corporate event filings for a company.
            8-K filings report material events: earnings releases, M&A,
            leadership changes, bankruptcy, etc.

            Args:
                ticker: Stock ticker symbol (e.g. "AAPL").
                limit: Number of recent events to retrieve (default 20).

            Returns:
                JSON list of 8-K events with date, item type, and description.
            """
            return _call("8k_events", ticker, str(limit))

        # ── Insider Transactions (Form 4) ─────────────────────────────────

        def get_insider_transactions(self, ticker: str, limit: int = 25) -> str:
            """
            Get recent insider transactions (Form 4 filings) for a company.
            Shows purchases and sales by executives, directors, and major shareholders.

            Args:
                ticker: Stock ticker symbol (e.g. "AAPL").
                limit: Number of recent transactions to retrieve (default 25).

            Returns:
                JSON list of insider transactions with date, insider name,
                transaction type (buy/sell), shares, and price.
            """
            return _call("insider_transactions", ticker, str(limit))

        def get_insider_summary(self, ticker: str, limit: int = 50) -> str:
            """
            Get a summary of insider buying vs selling activity for a company.

            Args:
                ticker: Stock ticker symbol (e.g. "AAPL").
                limit: Number of transactions to summarize (default 50).

            Returns:
                JSON summary with net buy/sell counts and total value.
            """
            return _call("insider_summary", ticker, str(limit))

        # ── 13F Institutional Holdings ────────────────────────────────────

        def get_13f_holdings(self, ticker: str, quarters: int = 2) -> str:
            """
            Get 13F institutional holdings data for a company across recent quarters.
            Shows which institutional investors (hedge funds, mutual funds) hold the stock.

            Args:
                ticker: Stock ticker symbol (e.g. "AAPL").
                quarters: Number of recent quarters to retrieve (default 2).

            Returns:
                JSON with institutional holdings per quarter including shares
                and market value.
            """
            return _call("13f_holdings", ticker, str(quarters))

        def get_13f_top_holdings(self, ticker: str, top_n: int = 20) -> str:
            """
            Get the top institutional holders of a company's stock.

            Args:
                ticker: Stock ticker symbol (e.g. "AAPL").
                top_n: Number of top holders to return (default 20).

            Returns:
                JSON list of top institutional holders with shares and percentage.
            """
            return _call("13f_top_holdings", ticker, str(top_n))

        # ── XBRL Financials ───────────────────────────────────────────────

        def get_financials(self, ticker: str, periods: int = 4, annual: bool = True) -> str:
            """
            Get XBRL financial statements (Balance Sheet, Income Statement,
            Cash Flow) for a company.

            Args:
                ticker: Stock ticker symbol (e.g. "AAPL").
                periods: Number of reporting periods to retrieve (default 4).
                annual: True for annual (10-K) data, False for quarterly (10-Q).

            Returns:
                JSON with Balance Sheet, Income Statement, and Cash Flow data.
            """
            return _call("get_financials", ticker, str(periods), str(annual).lower())

        def get_financial_metrics(self, ticker: str) -> str:
            """
            Get key financial metrics for a company (revenue, net income, EPS,
            profit margin, etc.) from SEC EDGAR XBRL data.

            Args:
                ticker: Stock ticker symbol (e.g. "AAPL").

            Returns:
                JSON with key financial metrics and their values.
            """
            return _call("get_financial_metrics", ticker)

except ImportError:
    # Agno not available — define a stub so import doesn't crash
    logger.warning("agno not installed; EdgarTools stub created")

    class EdgarTools:  # type: ignore
        """Stub when agno is not installed."""
        def __init__(self, **kwargs):
            pass
