"""
Market Data Knowledge

Knowledge base for market data including:
- Historical price patterns
- Volume profiles
- Sector correlations
- Market microstructure insights
- Alternative data patterns
"""

from typing import Optional, List, Dict, Any
from datetime import datetime
import uuid

from .base import (
    RenTechKnowledge,
    KnowledgeDocument,
    KnowledgeCategory,
    get_knowledge_base,
)


class MarketDataKnowledge:
    """
    Market Data Knowledge Manager.

    Provides structured access to market data knowledge including
    price patterns, volume analysis, and microstructure insights.
    """

    def __init__(self, knowledge_base: Optional[RenTechKnowledge] = None):
        self.kb = knowledge_base or get_knowledge_base()
        self.category = KnowledgeCategory.MARKET_DATA

    def add_price_pattern(
        self,
        ticker: str,
        pattern_name: str,
        description: str,
        statistics: Dict[str, Any],
        tags: Optional[List[str]] = None,
    ) -> str:
        """
        Add a discovered price pattern to knowledge base.

        Args:
            ticker: Stock ticker
            pattern_name: Name of the pattern
            description: Description of the pattern
            statistics: Statistical metrics for the pattern
            tags: Optional tags

        Returns:
            Document ID
        """
        content = f"""
# Price Pattern: {pattern_name}

## Ticker: {ticker}

## Description
{description}

## Statistics
- Mean Return: {statistics.get('mean_return', 'N/A')}
- Std Return: {statistics.get('std_return', 'N/A')}
- Sharpe Ratio: {statistics.get('sharpe_ratio', 'N/A')}
- Win Rate: {statistics.get('win_rate', 'N/A')}
- P-value: {statistics.get('p_value', 'N/A')}
- Sample Size: {statistics.get('sample_size', 'N/A')}

## Optimal Parameters
- Lookback: {statistics.get('lookback', 'N/A')} days
- Holding Period: {statistics.get('holding_period', 'N/A')} days
- Entry Threshold: {statistics.get('entry_threshold', 'N/A')}
- Exit Threshold: {statistics.get('exit_threshold', 'N/A')}

## Regime Performance
- Bull Market Sharpe: {statistics.get('bull_sharpe', 'N/A')}
- Bear Market Sharpe: {statistics.get('bear_sharpe', 'N/A')}
- High Vol Sharpe: {statistics.get('high_vol_sharpe', 'N/A')}
"""

        doc = KnowledgeDocument(
            id=f"pattern_{ticker}_{pattern_name}_{uuid.uuid4().hex[:8]}",
            category=self.category,
            title=f"{ticker} - {pattern_name}",
            content=content,
            metadata={
                "ticker": ticker,
                "pattern_name": pattern_name,
                "statistics": statistics,
                "doc_type": "price_pattern",
            },
            tags=tags or [ticker, pattern_name, "price_pattern"],
            importance=0.7,
        )

        return self.kb.add_document(doc)

    def add_volume_profile(
        self,
        ticker: str,
        profile_data: Dict[str, Any],
    ) -> str:
        """Add volume profile knowledge for a ticker"""
        content = f"""
# Volume Profile: {ticker}

## Average Daily Volume
- ADV (20-day): {profile_data.get('adv_20', 'N/A'):,} shares
- ADV (60-day): {profile_data.get('adv_60', 'N/A'):,} shares

## Intraday Pattern
- Open Auction: {profile_data.get('open_pct', 'N/A')}% of volume
- Morning (9:30-12:00): {profile_data.get('morning_pct', 'N/A')}%
- Midday (12:00-14:00): {profile_data.get('midday_pct', 'N/A')}%
- Afternoon (14:00-15:30): {profile_data.get('afternoon_pct', 'N/A')}%
- Close Auction: {profile_data.get('close_pct', 'N/A')}%

## Liquidity Metrics
- Typical Spread: {profile_data.get('spread_bps', 'N/A')} bps
- Market Depth (1%): ${profile_data.get('depth_1pct', 'N/A'):,}
- Kyle's Lambda: {profile_data.get('kyle_lambda', 'N/A')}

## Execution Recommendations
- Optimal Participation: {profile_data.get('opt_participation', 'N/A')}%
- Best Time to Trade: {profile_data.get('best_time', 'N/A')}
- Avoid: {profile_data.get('avoid_time', 'N/A')}
"""

        doc = KnowledgeDocument(
            id=f"volume_{ticker}_{datetime.utcnow().strftime('%Y%m%d')}",
            category=self.category,
            title=f"{ticker} - Volume Profile",
            content=content,
            metadata={
                "ticker": ticker,
                "profile_data": profile_data,
                "doc_type": "volume_profile",
            },
            tags=[ticker, "volume", "liquidity", "execution"],
            importance=0.6,
            freshness_weight=0.8,  # Volume profiles need to be fresh
        )

        return self.kb.add_document(doc)

    def add_correlation_insight(
        self,
        tickers: List[str],
        correlation: float,
        insight: str,
        period: str,
    ) -> str:
        """Add correlation insight between assets"""
        ticker_str = " / ".join(tickers)

        content = f"""
# Correlation Insight: {ticker_str}

## Period: {period}

## Correlation: {correlation:.3f}

## Insight
{insight}

## Trading Implications
- Diversification benefit: {'Low' if abs(correlation) > 0.7 else 'Medium' if abs(correlation) > 0.4 else 'High'}
- Pair trading opportunity: {'Yes' if abs(correlation) > 0.8 else 'Maybe' if abs(correlation) > 0.6 else 'No'}
- Risk consideration: {'High co-movement' if correlation > 0.7 else 'Moderate co-movement' if correlation > 0.4 else 'Good diversification'}

## Historical Context
This correlation has been {'stable' if 'stable' in insight.lower() else 'variable'} over time.
"""

        doc = KnowledgeDocument(
            id=f"corr_{'_'.join(tickers)}_{uuid.uuid4().hex[:8]}",
            category=self.category,
            title=f"Correlation: {ticker_str}",
            content=content,
            metadata={
                "tickers": tickers,
                "correlation": correlation,
                "period": period,
                "doc_type": "correlation",
            },
            tags=tickers + ["correlation", "risk"],
            importance=0.5,
        )

        return self.kb.add_document(doc)

    def add_sector_analysis(
        self,
        sector: str,
        analysis: Dict[str, Any],
    ) -> str:
        """Add sector-level market analysis"""
        content = f"""
# Sector Analysis: {sector}

## Current Characteristics
- Average Beta: {analysis.get('avg_beta', 'N/A')}
- Average Volatility: {analysis.get('avg_vol', 'N/A')}%
- YTD Performance: {analysis.get('ytd_return', 'N/A')}%

## Correlation with Market
- SPY Correlation: {analysis.get('spy_corr', 'N/A')}
- Sector Beta: {analysis.get('sector_beta', 'N/A')}

## Top Holdings Impact
{chr(10).join([f"- {h['ticker']}: {h['weight']}%" for h in analysis.get('top_holdings', [])])}

## Regime Behavior
- In Risk-Off: {analysis.get('risk_off_behavior', 'N/A')}
- In Risk-On: {analysis.get('risk_on_behavior', 'N/A')}

## Current Assessment
{analysis.get('assessment', 'No assessment available.')}

## Signal Implications
- Mean reversion effectiveness: {analysis.get('mean_rev_score', 'N/A')}/10
- Momentum effectiveness: {analysis.get('momentum_score', 'N/A')}/10
"""

        doc = KnowledgeDocument(
            id=f"sector_{sector}_{datetime.utcnow().strftime('%Y%m%d')}",
            category=self.category,
            title=f"Sector Analysis: {sector}",
            content=content,
            metadata={
                "sector": sector,
                "analysis": analysis,
                "doc_type": "sector_analysis",
            },
            tags=[sector, "sector", "market_analysis"],
            importance=0.6,
            freshness_weight=0.7,
        )

        return self.kb.add_document(doc)

    def add_microstructure_insight(
        self,
        topic: str,
        insight: str,
        evidence: Dict[str, Any],
    ) -> str:
        """Add market microstructure insight"""
        content = f"""
# Microstructure Insight: {topic}

## Insight
{insight}

## Evidence
- Sample Period: {evidence.get('period', 'N/A')}
- Sample Size: {evidence.get('sample_size', 'N/A')}
- Statistical Significance: {evidence.get('significance', 'N/A')}

## Key Findings
{chr(10).join([f"- {f}" for f in evidence.get('findings', [])])}

## Trading Implications
{evidence.get('implications', 'No specific implications noted.')}

## Recommended Actions
{chr(10).join([f"- {a}" for a in evidence.get('actions', [])])}
"""

        doc = KnowledgeDocument(
            id=f"micro_{topic.replace(' ', '_')}_{uuid.uuid4().hex[:8]}",
            category=self.category,
            title=f"Microstructure: {topic}",
            content=content,
            metadata={
                "topic": topic,
                "evidence": evidence,
                "doc_type": "microstructure",
            },
            tags=["microstructure", "execution", topic.lower()],
            importance=0.7,
        )

        return self.kb.add_document(doc)

    def search_patterns(
        self,
        query: str,
        ticker: Optional[str] = None,
        limit: int = 10,
    ) -> List[Dict[str, Any]]:
        """Search for price patterns"""
        results = self.kb.search(
            query=query,
            category=self.category,
            limit=limit,
        )

        if ticker:
            results = [r for r in results if r.get("metadata", {}).get("ticker") == ticker]

        return results

    def get_ticker_knowledge(self, ticker: str) -> List[KnowledgeDocument]:
        """Get all knowledge for a specific ticker"""
        docs = self.kb.get_documents_by_category(self.category)
        return [d for d in docs if d.metadata.get("ticker") == ticker or ticker in d.tags]


# Global instance
_market_data_knowledge: Optional[MarketDataKnowledge] = None


def get_market_data_knowledge() -> MarketDataKnowledge:
    """Get the global market data knowledge instance"""
    global _market_data_knowledge
    if _market_data_knowledge is None:
        _market_data_knowledge = MarketDataKnowledge()
    return _market_data_knowledge
