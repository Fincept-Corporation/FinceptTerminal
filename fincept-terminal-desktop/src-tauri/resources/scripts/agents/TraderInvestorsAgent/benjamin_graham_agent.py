"""
BENJAMIN GRAHAM AGENT - Deep Value Framework (FinAgent Core)
Based on Benjamin Graham's investment philosophy

This agent uses finagent_core for LLM execution.
All configuration is in TraderInvestorsAgent/configs/agent_definitions.json

GRAHAM PHILOSOPHY: Margin of safety principle - buy assets trading below intrinsic value
with significant discount for protection against downside risk.
"""

import sys
from pathlib import Path
from typing import Dict, Any

# Add parent directories to path
sys.path.insert(0, str(Path(__file__).parent.parent.parent))

from finagent_core.base_agent import AgentRegistry

class BenjaminGrahamAgent:
    """
    Benjamin Graham Agent using FinAgent Core

    Deep value principles analyzed:
    - Margin of safety → price significantly below intrinsic value
    - Quantitative rigor → statistical bargains
    - Net-net working capital → asset-based valuation
    - Defensive characteristics → financial safety
    - Mr. Market behavior → exploit market irrationality
    """

    def __init__(self, config_path: Path = None):
        if config_path is None:
            config_path = Path(__file__).parent / "configs" / "agent_definitions.json"

        registry = AgentRegistry(config_path)
        self.config = registry.get_agent("benjamin_graham_agent")

        if not self.config:
            raise ValueError("Benjamin Graham agent config not found in JSON")

        # Graham's investment criteria
        self.investment_criteria = {
            "margin_of_safety": {
                "indicator": "Price vs intrinsic value discount",
                "importance": "Critical - core principle",
                "evaluation": "P/B ratio, asset coverage",
                "threshold": "Price <67% of intrinsic value (50% discount)"
            },
            "net_net_working_capital": {
                "indicator": "Current assets - total liabilities",
                "importance": "High - statistical bargain",
                "evaluation": "Market cap vs NCAV",
                "threshold": "Price <NCAV (negative enterprise value)"
            },
            "defensive_characteristics": {
                "indicator": "Financial safety measures",
                "importance": "High - protect downside",
                "evaluation": "Current ratio, debt levels, earnings stability",
                "threshold": "Current ratio >2, D/E <0.5, profitable 5+ years"
            },
            "earnings_yield": {
                "indicator": "Earnings power relative to price",
                "importance": "Medium - valuation metric",
                "evaluation": "E/P ratio vs bond yields",
                "threshold": "E/P ratio >2x AAA bond yield"
            },
            "dividend_record": {
                "indicator": "Consistent dividend payments",
                "importance": "Low - income component",
                "evaluation": "Dividend history",
                "threshold": "Uninterrupted dividends 20+ years"
            }
        }

    def analyze(self, query: str) -> str:
        """
        Analyze query through Graham's deep value lens

        Note: LLM execution happens via finagent_core (tauri_bridge.py)
        This method is for direct Python usage if needed.
        """
        context = f"""
Investment Criteria (Benjamin Graham Framework):
{self._format_criteria()}

Query: {query}

Analyze this through Graham's margin of safety principle, seeking statistical bargains.
"""
        return context

    def _format_criteria(self) -> str:
        """Format investment criteria for context"""
        formatted = []
        for name, data in self.investment_criteria.items():
            formatted.append(f"\n{name.upper().replace('_', ' ')}:")
            formatted.append(f"  Indicator: {data['indicator']}")
            formatted.append(f"  Importance: {data['importance']}")
            formatted.append(f"  Threshold: {data['threshold']}")
        return "\n".join(formatted)


def main(argv=None):
    """CLI entry point for testing"""
    import argparse

    parser = argparse.ArgumentParser(description="Benjamin Graham Agent")
    parser.add_argument("--query", required=True, help="Investment query to analyze")

    args = parser.parse_args(argv)

    agent = BenjaminGrahamAgent()
    result = agent.analyze(args.query)
    print(result)


if __name__ == "__main__":
    main()
