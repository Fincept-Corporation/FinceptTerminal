"""
Research Papers Knowledge

Knowledge base for academic and internal research including:
- Academic papers on quantitative finance
- Internal research findings
- Factor research
- Strategy development notes
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


class ResearchKnowledge:
    """
    Research Knowledge Manager.

    Provides structured access to research papers, academic findings,
    and internal research documentation.
    """

    def __init__(self, knowledge_base: Optional[RenTechKnowledge] = None):
        self.kb = knowledge_base or get_knowledge_base()
        self.category = KnowledgeCategory.RESEARCH

    def add_academic_paper(
        self,
        title: str,
        authors: List[str],
        abstract: str,
        key_findings: List[str],
        methodology: str,
        relevance_to_trading: str,
        publication_year: int,
        tags: Optional[List[str]] = None,
    ) -> str:
        """
        Add an academic paper to the knowledge base.

        Args:
            title: Paper title
            authors: List of authors
            abstract: Paper abstract
            key_findings: Key findings from the paper
            methodology: Research methodology used
            relevance_to_trading: How this applies to our trading
            publication_year: Year of publication
            tags: Optional tags

        Returns:
            Document ID
        """
        content = f"""
# Research Paper: {title}

## Authors
{', '.join(authors)}

## Publication Year: {publication_year}

## Abstract
{abstract}

## Key Findings
{chr(10).join([f"- {f}" for f in key_findings])}

## Methodology
{methodology}

## Relevance to Trading
{relevance_to_trading}

## Implementation Notes
Consider the following when implementing:
- Data requirements
- Parameter sensitivity
- Transaction cost implications
- Capacity constraints
"""

        doc = KnowledgeDocument(
            id=f"paper_{title.replace(' ', '_')[:30]}_{uuid.uuid4().hex[:8]}",
            category=self.category,
            title=title,
            content=content,
            metadata={
                "authors": authors,
                "publication_year": publication_year,
                "doc_type": "academic_paper",
            },
            tags=tags or ["academic", "research"] + [a.split()[-1].lower() for a in authors[:3]],
            importance=0.8,
        )

        return self.kb.add_document(doc)

    def add_internal_research(
        self,
        title: str,
        researcher: str,
        summary: str,
        findings: List[str],
        data_used: List[str],
        statistical_results: Dict[str, Any],
        recommendations: List[str],
        status: str = "completed",
    ) -> str:
        """Add internal research finding"""
        content = f"""
# Internal Research: {title}

## Researcher: {researcher}
## Status: {status}
## Date: {datetime.utcnow().strftime('%Y-%m-%d')}

## Summary
{summary}

## Data Sources Used
{chr(10).join([f"- {d}" for d in data_used])}

## Key Findings
{chr(10).join([f"- {f}" for f in findings])}

## Statistical Results
- Sample Size: {statistical_results.get('sample_size', 'N/A')}
- P-value: {statistical_results.get('p_value', 'N/A')}
- T-statistic: {statistical_results.get('t_stat', 'N/A')}
- Sharpe Ratio: {statistical_results.get('sharpe', 'N/A')}
- Information Coefficient: {statistical_results.get('ic', 'N/A')}

## Recommendations
{chr(10).join([f"- {r}" for r in recommendations])}

## Next Steps
- Further validation needed: {statistical_results.get('needs_validation', False)}
- Ready for production: {statistical_results.get('production_ready', False)}
"""

        doc = KnowledgeDocument(
            id=f"internal_{title.replace(' ', '_')[:30]}_{uuid.uuid4().hex[:8]}",
            category=self.category,
            title=f"Internal: {title}",
            content=content,
            metadata={
                "researcher": researcher,
                "status": status,
                "statistical_results": statistical_results,
                "doc_type": "internal_research",
            },
            tags=["internal", "research", researcher.lower().replace(" ", "_")],
            importance=0.9,  # Internal research is highly important
        )

        return self.kb.add_document(doc)

    def add_factor_research(
        self,
        factor_name: str,
        description: str,
        construction: str,
        historical_performance: Dict[str, Any],
        correlation_with_other_factors: Dict[str, float],
        implementation_notes: str,
    ) -> str:
        """Add factor research documentation"""
        content = f"""
# Factor Research: {factor_name}

## Description
{description}

## Factor Construction
{construction}

## Historical Performance
- Annualized Return: {historical_performance.get('annual_return', 'N/A')}%
- Annualized Volatility: {historical_performance.get('annual_vol', 'N/A')}%
- Sharpe Ratio: {historical_performance.get('sharpe', 'N/A')}
- Max Drawdown: {historical_performance.get('max_dd', 'N/A')}%
- Sample Period: {historical_performance.get('period', 'N/A')}

## Factor Correlations
{chr(10).join([f"- {f}: {c:.2f}" for f, c in correlation_with_other_factors.items()])}

## Implementation Notes
{implementation_notes}

## Risk Considerations
- Factor crowding risk
- Regime dependency
- Capacity constraints
- Transaction cost sensitivity
"""

        doc = KnowledgeDocument(
            id=f"factor_{factor_name.replace(' ', '_')}_{uuid.uuid4().hex[:8]}",
            category=self.category,
            title=f"Factor: {factor_name}",
            content=content,
            metadata={
                "factor_name": factor_name,
                "historical_performance": historical_performance,
                "correlations": correlation_with_other_factors,
                "doc_type": "factor_research",
            },
            tags=["factor", factor_name.lower(), "research"],
            importance=0.85,
        )

        return self.kb.add_document(doc)

    def add_signal_documentation(
        self,
        signal_name: str,
        signal_type: str,
        description: str,
        parameters: Dict[str, Any],
        backtest_results: Dict[str, Any],
        live_results: Optional[Dict[str, Any]] = None,
        lessons_learned: Optional[List[str]] = None,
    ) -> str:
        """Add signal documentation"""
        content = f"""
# Signal Documentation: {signal_name}

## Signal Type: {signal_type}

## Description
{description}

## Parameters
{chr(10).join([f"- {k}: {v}" for k, v in parameters.items()])}

## Backtest Results
- Period: {backtest_results.get('period', 'N/A')}
- Sharpe Ratio: {backtest_results.get('sharpe', 'N/A')}
- Annual Return: {backtest_results.get('annual_return', 'N/A')}%
- Win Rate: {backtest_results.get('win_rate', 'N/A')}%
- Profit Factor: {backtest_results.get('profit_factor', 'N/A')}
- Max Drawdown: {backtest_results.get('max_dd', 'N/A')}%

## Live Results
{f'''
- Period: {live_results.get('period', 'N/A')}
- Sharpe Ratio: {live_results.get('sharpe', 'N/A')}
- Backtest vs Live Degradation: {live_results.get('degradation', 'N/A')}%
''' if live_results else 'Not yet traded live.'}

## Lessons Learned
{chr(10).join([f"- {l}" for l in (lessons_learned or ['No lessons documented yet.'])])}

## Current Status
- Active: {backtest_results.get('active', False)}
- Allocation: {backtest_results.get('allocation_pct', 0)}% of portfolio
"""

        doc = KnowledgeDocument(
            id=f"signal_{signal_name.replace(' ', '_')}_{uuid.uuid4().hex[:8]}",
            category=self.category,
            title=f"Signal: {signal_name}",
            content=content,
            metadata={
                "signal_name": signal_name,
                "signal_type": signal_type,
                "parameters": parameters,
                "backtest_results": backtest_results,
                "live_results": live_results,
                "doc_type": "signal_documentation",
            },
            tags=["signal", signal_type.lower(), signal_name.lower()],
            importance=0.9,
        )

        return self.kb.add_document(doc)

    def search_research(
        self,
        query: str,
        doc_type: Optional[str] = None,
        limit: int = 10,
    ) -> List[Dict[str, Any]]:
        """Search research documents"""
        results = self.kb.search(
            query=query,
            category=self.category,
            limit=limit,
        )

        if doc_type:
            results = [r for r in results if r.get("metadata", {}).get("doc_type") == doc_type]

        return results

    def get_factor_library(self) -> List[KnowledgeDocument]:
        """Get all documented factors"""
        docs = self.kb.get_documents_by_category(self.category)
        return [d for d in docs if d.metadata.get("doc_type") == "factor_research"]

    def get_signal_library(self) -> List[KnowledgeDocument]:
        """Get all documented signals"""
        docs = self.kb.get_documents_by_category(self.category)
        return [d for d in docs if d.metadata.get("doc_type") == "signal_documentation"]


# Global instance
_research_knowledge: Optional[ResearchKnowledge] = None


def get_research_knowledge() -> ResearchKnowledge:
    """Get the global research knowledge instance"""
    global _research_knowledge
    if _research_knowledge is None:
        _research_knowledge = ResearchKnowledge()
    return _research_knowledge
