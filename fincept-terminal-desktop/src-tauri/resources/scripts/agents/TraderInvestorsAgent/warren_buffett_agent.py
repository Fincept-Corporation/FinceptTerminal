"""
WARREN BUFFETT AGENT - Value Investing Framework (FinAgent Core)
Based on Warren Buffett's investment philosophy

This agent uses finagent_core for LLM execution.
All configuration is in TraderInvestorsAgent/configs/agent_definitions.json

BUFFETT PHILOSOPHY: Buy wonderful companies at fair prices - focus on economic moats,
predictable earnings, strong management, and long-term value creation.
"""

import sys
from pathlib import Path
from typing import Dict, Any

# Add parent directories to path
sys.path.insert(0, str(Path(__file__).parent.parent.parent))

from finagent_core.base_agent import AgentRegistry

class WarrenBuffettAgent:
    """
    Warren Buffett Agent using FinAgent Core

    Investment principles analyzed:
    - Economic moats → sustainable competitive advantages
    - Predictable earnings → consistent profitability
    - Financial fortress → strong balance sheets
    - Quality management → efficient capital allocation
    - Fair price → owner earnings yield
    """

    def __init__(self, config_path: Path = None):
        if config_path is None:
            config_path = Path(__file__).parent / "configs" / "agent_definitions.json"

        registry = AgentRegistry(config_path)
        self.config = registry.get_agent("warren_buffett_agent")

        if not self.config:
            raise ValueError("Warren Buffett agent config not found in JSON")

        # Buffett's investment criteria
        self.investment_criteria = {
            "economic_moat": {
                "indicator": "High ROE (15%+), pricing power",
                "importance": "Critical - sustainable competitive advantage",
                "evaluation": "Brand strength, network effects, cost advantages",
                "threshold": "ROE >15% for 7+ years"
            },
            "predictable_earnings": {
                "indicator": "Consistent profitability over decade",
                "importance": "High - avoid cyclical businesses",
                "evaluation": "Earnings stability, growth trend",
                "threshold": "Positive earnings 8+ of 10 years"
            },
            "financial_fortress": {
                "indicator": "Low debt, strong cash flow",
                "importance": "High - financial safety",
                "evaluation": "D/E ratio, current ratio, FCF",
                "threshold": "D/E <0.3, consistent positive FCF"
            },
            "quality_management": {
                "indicator": "Capital allocation skill",
                "importance": "Medium - effective reinvestment",
                "evaluation": "Share buybacks, retained earnings growth",
                "threshold": "Shareholder-friendly decisions"
            },
            "fair_valuation": {
                "indicator": "Owner earnings yield",
                "importance": "Medium - buy at reasonable price",
                "evaluation": "FCF yield, P/E ratio",
                "threshold": "FCF yield >5%, P/E <25"
            }
        }

    def analyze(self, query: str) -> str:
        """
        Analyze query through Buffett's value investing lens

        Note: LLM execution happens via finagent_core (tauri_bridge.py)
        This method is for direct Python usage if needed.
        """
        context = f"""
Investment Criteria (Warren Buffett Framework):
{self._format_criteria()}

Query: {query}

Analyze this through Buffett's philosophy, focusing on long-term business quality.
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

    parser = argparse.ArgumentParser(description="Warren Buffett Agent")
    parser.add_argument("--query", required=True, help="Investment query to analyze")

    args = parser.parse_args(argv)

    agent = WarrenBuffettAgent()
    result = agent.analyze(args.query)
    print(result)


if __name__ == "__main__":
    main()
