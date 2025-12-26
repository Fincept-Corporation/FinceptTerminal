"""
AMERICAN WORLD ORDER AGENT - Kissinger's Framework (FinAgent Core)
Based on "World Order" by Henry Kissinger

This agent uses finagent_core for LLM execution.
All configuration is in GeopoliticsAgents/configs/agent_definitions.json

KISSINGER THESIS: American world order based on liberal internationalism,
democratic values, and post-WWII institutional framework.
"""

import sys
from pathlib import Path
from typing import Dict, Any

# Add parent directories to path
sys.path.insert(0, str(Path(__file__).parent.parent.parent.parent))

from finagent_core.base_agent import AgentRegistry

class AmericanOrderAgent:
    """
    American World Order Agent using FinAgent Core

    Analyzes events through:
    - Liberal internationalist principles
    - Democratic value promotion
    - American exceptionalism
    - Post-WWII institutions (UN, NATO, IMF, World Bank)
    """

    def __init__(self, config_path: Path = None):
        if config_path is None:
            config_path = Path(__file__).parent.parent / "configs" / "agent_definitions.json"

        registry = AgentRegistry(config_path)
        self.config = registry.get_agent("world_order_american")

        if not self.config:
            raise ValueError("American world order agent config not found in JSON")

        # American order principles
        self.order_principles = {
            "liberal_internationalism": {
                "principle": "Rules-based international order",
                "institutions": "UN, WTO, IMF, World Bank",
                "values": "Democracy, human rights, free markets",
                "current_challenges": "Rising authoritarianism, trade wars"
            },
            "democratic_promotion": {
                "principle": "Democratic peace theory",
                "strategy": "Democracy promotion abroad",
                "tension": "Idealism vs realpolitik",
                "current_challenges": "Failed interventions, illiberal democracies"
            },
            "american_exceptionalism": {
                "principle": "Unique American role in world affairs",
                "manifest_destiny": "Leadership of free world",
                "burden": "Global policeman role",
                "current_challenges": "War fatigue, isolationism"
            },
            "institutional_framework": {
                "principle": "Multilateral institutions",
                "nato": "Collective security in Europe",
                "alliances": "Hub-and-spoke in Asia",
                "current_challenges": "Burden-sharing, relevance"
            }
        }

    def analyze(self, query: str) -> str:
        """
        Analyze query through Kissinger's American world order lens

        Note: LLM execution happens via finagent_core (tauri_bridge.py)
        """
        context = f"""
American World Order Principles (Kissinger):
{self._format_principles()}

Query: {query}

Analyze through American world order framework.
"""
        return context

    def _format_principles(self) -> str:
        """Format order principles for context"""
        formatted = []
        for name, data in self.order_principles.items():
            formatted.append(f"\n{name.upper()}:")
            formatted.append(f"  Principle: {data['principle']}")
            formatted.append(f"  Current: {data.get('current_challenges', 'N/A')}")
        return "\n".join(formatted)


def main(argv=None):
    """CLI entry point for testing"""
    import argparse

    parser = argparse.ArgumentParser(description="American World Order Agent")
    parser.add_argument("--query", required=True, help="Query to analyze")

    args = parser.parse_args(argv)

    agent = AmericanOrderAgent()
    result = agent.analyze(args.query)
    print(result)


if __name__ == "__main__":
    main()
