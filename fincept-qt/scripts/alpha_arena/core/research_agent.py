"""
Research Agent for Alpha Arena

Provides research capabilities using:
- SEC filings via edgar_tools.py
- News analysis
- Company fundamentals

This agent doesn't trade but provides research context to trading agents.
"""

import sys
from pathlib import Path
from typing import Dict, List, Optional, Any, AsyncGenerator
from dataclasses import dataclass, field
from datetime import datetime

# Add scripts directory to path for imports
scripts_dir = Path(__file__).parent.parent.parent
if str(scripts_dir) not in sys.path:
    sys.path.insert(0, str(scripts_dir))

from alpha_arena.core.base_agent import BaseAgent
from alpha_arena.types.responses import StreamResponse, NotifyResponse
from alpha_arena.utils.logging import get_logger

logger = get_logger("research_agent")


@dataclass
class CompanyInfo:
    """Company information from SEC"""
    cik: str
    name: str
    ticker: Optional[str] = None
    sic: Optional[str] = None
    industry: Optional[str] = None
    fiscal_year_end: Optional[str] = None
    state: Optional[str] = None

    def to_dict(self) -> Dict[str, Any]:
        return {
            "cik": self.cik,
            "name": self.name,
            "ticker": self.ticker,
            "sic": self.sic,
            "industry": self.industry,
            "fiscal_year_end": self.fiscal_year_end,
            "state": self.state,
        }


@dataclass
class FilingSummary:
    """Summary of an SEC filing"""
    accession_number: str
    form_type: str
    filing_date: str
    description: Optional[str] = None
    primary_document: Optional[str] = None

    def to_dict(self) -> Dict[str, Any]:
        return {
            "accession_number": self.accession_number,
            "form_type": self.form_type,
            "filing_date": self.filing_date,
            "description": self.description,
            "primary_document": self.primary_document,
        }


@dataclass
class FinancialData:
    """Financial data from SEC filings"""
    period: str
    revenue: Optional[float] = None
    net_income: Optional[float] = None
    total_assets: Optional[float] = None
    total_liabilities: Optional[float] = None
    total_equity: Optional[float] = None
    eps: Optional[float] = None
    cash_flow_operations: Optional[float] = None

    def to_dict(self) -> Dict[str, Any]:
        return {
            "period": self.period,
            "revenue": self.revenue,
            "net_income": self.net_income,
            "total_assets": self.total_assets,
            "total_liabilities": self.total_liabilities,
            "total_equity": self.total_equity,
            "eps": self.eps,
            "cash_flow_operations": self.cash_flow_operations,
        }

    def to_prompt_context(self) -> str:
        """Format for LLM context"""
        lines = [f"Financial Data ({self.period}):"]
        if self.revenue:
            lines.append(f"  Revenue: ${self.revenue:,.0f}")
        if self.net_income:
            lines.append(f"  Net Income: ${self.net_income:,.0f}")
        if self.total_assets:
            lines.append(f"  Total Assets: ${self.total_assets:,.0f}")
        if self.eps:
            lines.append(f"  EPS: ${self.eps:.2f}")
        return "\n".join(lines)


@dataclass
class ResearchReport:
    """A comprehensive research report"""
    symbol: str
    generated_at: datetime = field(default_factory=datetime.now)
    company_info: Optional[CompanyInfo] = None
    recent_filings: List[FilingSummary] = field(default_factory=list)
    financials: List[FinancialData] = field(default_factory=list)
    insider_activity: List[Dict[str, Any]] = field(default_factory=list)
    summary: str = ""
    risk_factors: List[str] = field(default_factory=list)

    def to_dict(self) -> Dict[str, Any]:
        return {
            "symbol": self.symbol,
            "generated_at": self.generated_at.isoformat(),
            "company_info": self.company_info.to_dict() if self.company_info else None,
            "recent_filings": [f.to_dict() for f in self.recent_filings],
            "financials": [f.to_dict() for f in self.financials],
            "insider_activity": self.insider_activity,
            "summary": self.summary,
            "risk_factors": self.risk_factors,
        }

    def to_prompt_context(self) -> str:
        """Format entire report for LLM context"""
        lines = [f"RESEARCH REPORT: {self.symbol}"]
        lines.append(f"Generated: {self.generated_at.strftime('%Y-%m-%d %H:%M')}")
        lines.append("")

        if self.company_info:
            lines.append(f"Company: {self.company_info.name}")
            if self.company_info.industry:
                lines.append(f"Industry: {self.company_info.industry}")
            lines.append("")

        if self.financials:
            lines.append("FINANCIALS:")
            for fin in self.financials[:2]:  # Last 2 periods
                lines.append(fin.to_prompt_context())
            lines.append("")

        if self.recent_filings:
            lines.append(f"RECENT FILINGS ({len(self.recent_filings)}):")
            for filing in self.recent_filings[:5]:
                lines.append(f"  - {filing.form_type} ({filing.filing_date})")
            lines.append("")

        if self.insider_activity:
            lines.append(f"INSIDER ACTIVITY: {len(self.insider_activity)} transactions")
            lines.append("")

        if self.risk_factors:
            lines.append("KEY RISKS:")
            for risk in self.risk_factors[:3]:
                lines.append(f"  - {risk[:100]}")
            lines.append("")

        if self.summary:
            lines.append("SUMMARY:")
            lines.append(self.summary)

        return "\n".join(lines)


class ResearchAgent(BaseAgent):
    """
    Research Agent for gathering and analyzing company information.

    Uses edgar_tools.py for SEC filings data.
    """

    def __init__(self, name: str = "ResearchAgent"):
        super().__init__(name)
        self._edgar_available = False
        self._cache: Dict[str, ResearchReport] = {}
        self._cache_ttl = 3600  # 1 hour cache

    async def initialize(self) -> bool:
        """Initialize the research agent"""
        try:
            # Check if edgar_tools is available
            import edgar_tools
            self._edgar_available = True
            logger.info("Edgar tools available for research")
        except ImportError:
            logger.warning("Edgar tools not available - research capabilities limited")
            self._edgar_available = False

        self._initialized = True
        return True

    async def stream(
        self,
        query: str,
        competition_id: str,
        cycle_number: int,
        dependencies: Optional[Dict] = None,
    ) -> AsyncGenerator[StreamResponse, None]:
        """Stream research results"""
        yield StreamResponse(content="Research agent stream not implemented", event="info")

    async def get_company_info(self, ticker: str) -> Optional[CompanyInfo]:
        """Get company information from SEC"""
        if not self._edgar_available:
            return None

        try:
            import edgar_tools
            import json

            # Call edgar_tools
            result = edgar_tools.main(["company_info", ticker])
            data = json.loads(result)

            if data.get("success"):
                info = data.get("data", {})
                return CompanyInfo(
                    cik=info.get("cik", ""),
                    name=info.get("name", ""),
                    ticker=info.get("tickers", [ticker])[0] if info.get("tickers") else ticker,
                    sic=info.get("sic"),
                    industry=info.get("sic_description"),
                    fiscal_year_end=info.get("fiscal_year_end"),
                    state=info.get("state_of_incorporation"),
                )
        except Exception as e:
            logger.error(f"Error getting company info: {e}")

        return None

    async def get_recent_filings(
        self,
        ticker: str,
        form_types: Optional[List[str]] = None,
        limit: int = 10,
    ) -> List[FilingSummary]:
        """Get recent SEC filings"""
        if not self._edgar_available:
            return []

        try:
            import edgar_tools
            import json

            # Call edgar_tools
            args = ["get_filings", ticker, "--limit", str(limit)]
            if form_types:
                args.extend(["--form-type", ",".join(form_types)])

            result = edgar_tools.main(args)
            data = json.loads(result)

            if data.get("success"):
                filings = []
                for f in data.get("data", {}).get("filings", []):
                    filings.append(FilingSummary(
                        accession_number=f.get("accession_number", ""),
                        form_type=f.get("form_type", ""),
                        filing_date=f.get("filing_date", ""),
                        description=f.get("description"),
                        primary_document=f.get("primary_document"),
                    ))
                return filings
        except Exception as e:
            logger.error(f"Error getting filings: {e}")

        return []

    async def get_financials(self, ticker: str) -> List[FinancialData]:
        """Get financial statements"""
        if not self._edgar_available:
            return []

        try:
            import edgar_tools
            import json

            # Get income statement
            result = edgar_tools.main(["income_statement", ticker])
            data = json.loads(result)

            financials = []
            if data.get("success"):
                for period, values in data.get("data", {}).items():
                    if isinstance(values, dict):
                        financials.append(FinancialData(
                            period=period,
                            revenue=values.get("revenue") or values.get("total_revenue"),
                            net_income=values.get("net_income"),
                            eps=values.get("eps") or values.get("basic_eps"),
                        ))

            return financials[:4]  # Return last 4 periods
        except Exception as e:
            logger.error(f"Error getting financials: {e}")

        return []

    async def get_insider_transactions(self, ticker: str, limit: int = 10) -> List[Dict[str, Any]]:
        """Get insider transactions"""
        if not self._edgar_available:
            return []

        try:
            import edgar_tools
            import json

            result = edgar_tools.main(["insider_transactions", ticker, "--limit", str(limit)])
            data = json.loads(result)

            if data.get("success"):
                return data.get("data", {}).get("transactions", [])
        except Exception as e:
            logger.error(f"Error getting insider transactions: {e}")

        return []

    async def generate_report(self, ticker: str, use_cache: bool = True) -> ResearchReport:
        """
        Generate a comprehensive research report for a ticker.

        Args:
            ticker: Stock ticker symbol
            use_cache: Whether to use cached report if available

        Returns:
            ResearchReport with all available data
        """
        # Check cache
        if use_cache and ticker in self._cache:
            cached = self._cache[ticker]
            age = (datetime.now() - cached.generated_at).total_seconds()
            if age < self._cache_ttl:
                logger.info(f"Using cached research report for {ticker}")
                return cached

        logger.info(f"Generating research report for {ticker}")

        report = ResearchReport(symbol=ticker)

        # Gather data concurrently
        company_info = await self.get_company_info(ticker)
        if company_info:
            report.company_info = company_info

        filings = await self.get_recent_filings(ticker, limit=10)
        report.recent_filings = filings

        financials = await self.get_financials(ticker)
        report.financials = financials

        insider = await self.get_insider_transactions(ticker, limit=10)
        report.insider_activity = insider

        # Generate summary
        report.summary = self._generate_summary(report)

        # Cache the report
        self._cache[ticker] = report

        return report

    def _generate_summary(self, report: ResearchReport) -> str:
        """Generate a brief summary of the research"""
        parts = []

        if report.company_info:
            parts.append(f"{report.company_info.name}")
            if report.company_info.industry:
                parts.append(f"operates in {report.company_info.industry}")

        if report.financials:
            latest = report.financials[0]
            if latest.revenue:
                parts.append(f"Revenue: ${latest.revenue:,.0f}")
            if latest.net_income:
                profit_status = "profitable" if latest.net_income > 0 else "reporting losses"
                parts.append(f"Currently {profit_status}")

        if report.recent_filings:
            parts.append(f"{len(report.recent_filings)} recent SEC filings")

        if report.insider_activity:
            parts.append(f"{len(report.insider_activity)} insider transactions")

        return ". ".join(parts) + "." if parts else "Limited data available."

    def clear_cache(self, ticker: Optional[str] = None):
        """Clear research cache"""
        if ticker:
            self._cache.pop(ticker, None)
        else:
            self._cache.clear()


# Singleton instance
_research_agent: Optional[ResearchAgent] = None


async def get_research_agent() -> ResearchAgent:
    """Get the research agent singleton"""
    global _research_agent
    if _research_agent is None:
        _research_agent = ResearchAgent()
        await _research_agent.initialize()
    return _research_agent


async def get_research_context(ticker: str) -> str:
    """
    Get research context for a ticker.

    Convenience function for adding research to trading prompts.
    """
    agent = await get_research_agent()
    report = await agent.generate_report(ticker)
    return report.to_prompt_context()
