"""
EURASIAN BALKANS AGENT - Brzezinski's Framework (FinAgent Core)
Based on "The Grand Chessboard" by Zbigniew Brzezinski

This agent uses finagent_core for LLM execution.
All configuration is in GeopoliticsAgents/configs/agent_definitions.json

BRZEZINSKI THESIS: Central Asia/Caucasus as pivotal geopolitical space
where great powers compete for influence and resources.
"""

import sys
from pathlib import Path
from typing import Dict, Any

# Add parent directories to path
sys.path.insert(0, str(Path(__file__).parent.parent.parent.parent.parent))

from finagent_core.base_agent import AgentRegistry

class EurasianBalkansAgent:
    """
    Eurasian Balkans Agent using FinAgent Core

    Analyzes Central Asia through:
    - Pivotal geopolitical space concept
    - Great power competition (Russia, China, West)
    - Energy resources and pipeline politics
    - Ethnic/religious fault lines
    """

    def __init__(self, config_path: Path = None):
        if config_path is None:
            config_path = Path(__file__).parent.parent.parent / "configs" / "agent_definitions.json"

        registry = AgentRegistry(config_path)
        self.config = registry.get_agent("grand_chessboard_eurasian")

        if not self.config:
            raise ValueError("Eurasian balkans agent config not found in JSON")

        # Brzezinski's framework
        self.strategic_framework = {
            "pivotal_space": {
                "concept": "Region where control determines global power",
                "states": "Kazakhstan, Uzbekistan, Turkmenistan, Azerbaijan, Georgia",
                "importance": "Buffer between Russia, China, Middle East",
                "current_dynamics": "SCO, CSTO, Belt and Road competition"
            },
            "energy_politics": {
                "resources": "Oil, natural gas reserves",
                "pipelines": "Competing routes (Russia vs West vs China)",
                "strategic_value": "Energy independence from Russia",
                "current_dynamics": "Diversification efforts, Chinese investment"
            },
            "great_power_competition": {
                "russia": "Historical sphere of influence, reassertion",
                "china": "Economic penetration via BRI, SCO",
                "west": "Democracy promotion, energy access",
                "current_dynamics": "Russia-China alignment vs West"
            },
            "instability_factors": {
                "ethnic_tensions": "Multi-ethnic states, Russian minorities",
                "islamic_radicalism": "Taliban, ISIS spillover",
                "authoritarianism": "Weak institutions, strongmen",
                "current_dynamics": "Afghanistan collapse impact"
            }
        }

    def analyze(self, query: str) -> str:
        """
        Analyze query through Brzezinski's Grand Chessboard lens

        Note: LLM execution happens via finagent_core (tauri_bridge.py)
        """
        context = f"""
Grand Chessboard Framework (Brzezinski):
{self._format_framework()}

Query: {query}

Analyze through Eurasian Balkans strategic framework.
"""
        return context

    def _format_framework(self) -> str:
        """Format strategic framework for context"""
        formatted = []
        for name, data in self.strategic_framework.items():
            formatted.append(f"\n{name.upper()}:")
            formatted.append(f"  Concept: {data.get('concept', 'N/A')}")
            formatted.append(f"  Current: {data['current_dynamics']}")
        return "\n".join(formatted)


def main(argv=None):
    """CLI entry point for testing"""
    import argparse

    parser = argparse.ArgumentParser(description="Eurasian Balkans Agent")
    parser.add_argument("--query", required=True, help="Query to analyze")

    args = parser.parse_args(argv)

    agent = EurasianBalkansAgent()
    result = agent.analyze(args.query)
    print(result)


if __name__ == "__main__":
    main()
