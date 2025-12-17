"""
RUSSIA GEOGRAPHY AGENT - Tim Marshall's Framework (FinAgent Core)
Based on "Prisoners of Geography" Chapter 1

This agent now uses finagent_core for LLM execution.
All configuration is in GeopoliticsAgents/configs/agent_definitions.json

MARSHALL THESIS: Russia is a prisoner of its geography - the North European Plain's
vulnerability forces expansion, warm-water port obsession drives foreign policy,
and vast territory creates perpetual security challenges.
"""

import sys
from pathlib import Path
from typing import Dict, Any

# Add parent directories to path
sys.path.insert(0, str(Path(__file__).parent.parent.parent.parent))

from finagent_core.base_agent import AgentRegistry

class RussiaGeographyAgent:
    """
    Russia Geography Agent using FinAgent Core

    Geographic constraints analyzed:
    - North European Plain vulnerability → buffer state creation
    - Warm-water port obsession → Crimea, Syria policies
    - Vast territory → logistical challenges
    - Climate limitations → agricultural constraints
    """

    def __init__(self, config_path: Path = None):
        if config_path is None:
            config_path = Path(__file__).parent.parent.parent / "configs" / "agent_definitions.json"

        registry = AgentRegistry(config_path)
        self.config = registry.get_agent("prisoners_geography_russia")

        if not self.config:
            raise ValueError("Russia geography agent config not found in JSON")

        # Geographic constraints from Marshall's framework
        self.geographic_prisons = {
            "north_european_plain": {
                "constraint": "Flat invasion route from Europe",
                "historical_vulnerability": "Napoleon, Hitler, NATO expansion",
                "strategic_imperative": "Buffer state creation",
                "current_implications": "Ukraine, Belarus, Baltic states"
            },
            "warm_water_ports": {
                "constraint": "Most ports frozen year-round",
                "historical_obsession": "Black Sea, Baltic, Pacific access",
                "strategic_imperative": "Year-round naval presence",
                "current_implications": "Crimea annexation, Syria presence"
            },
            "vast_territory": {
                "constraint": "17 million km², 11 time zones",
                "logistical_challenges": "Infrastructure costs",
                "strategic_imperative": "Control of transport corridors",
                "current_implications": "Rail networks, pipeline control"
            },
            "climate_agriculture": {
                "constraint": "Only 12% arable land, permafrost",
                "food_security_issue": "Limited agricultural heartland",
                "strategic_imperative": "Ukraine grain belt control",
                "current_implications": "Southern expansion"
            }
        }

    def analyze(self, query: str) -> str:
        """
        Analyze query through Marshall's geographic determinism lens

        Note: LLM execution happens via finagent_core (tauri_bridge.py)
        This method is for direct Python usage if needed.
        """
        context = f"""
Geographic Constraints for Russia:
{self._format_constraints()}

Query: {query}

Analyze this through Marshall's framework, connecting to geographic determinism.
"""
        return context

    def _format_constraints(self) -> str:
        """Format geographic constraints for context"""
        formatted = []
        for name, data in self.geographic_prisons.items():
            formatted.append(f"\n{name.upper()}:")
            formatted.append(f"  Constraint: {data['constraint']}")
            formatted.append(f"  Imperative: {data['strategic_imperative']}")
            formatted.append(f"  Current: {data['current_implications']}")
        return "\n".join(formatted)


def main():
    """CLI entry point for testing"""
    import argparse

    parser = argparse.ArgumentParser(description="Russia Geography Agent")
    parser.add_argument("--query", required=True, help="Query to analyze")

    args = parser.parse_args()

    agent = RussiaGeographyAgent()
    result = agent.analyze(args.query)
    print(result)


if __name__ == "__main__":
    main()
