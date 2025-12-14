"""
Portfolio Manager Agent

Specialized agent for portfolio management and asset allocation.
"""

from typing import Dict, Any, Optional, List
from core.base_agent import BaseAgent, AgentConfig
from tools.portfolio_tools import get_portfolio_tools
from tools.market_data import get_market_data_tools


class PortfolioManagerAgent(BaseAgent):
    """
    Portfolio Manager Agent

    Specializes in:
    - Asset allocation
    - Portfolio optimization
    - Rebalancing recommendations
    - Performance tracking
    - Capital deployment
    """

    def __init__(
        self,
        model_provider: str = "anthropic",
        model_name: str = "claude-3-sonnet",
        temperature: float = 0.5
    ):
        """Initialize Portfolio Manager Agent"""

        config = AgentConfig(
            name="PortfolioManager",
            role="Portfolio Management Specialist",
            description=(
                "Expert portfolio manager focused on optimal asset allocation, portfolio construction, "
                "and performance optimization. Balances growth with risk management."
            ),
            instructions=[
                "Optimize portfolio for risk-adjusted returns",
                "Maintain appropriate diversification",
                "Recommend rebalancing when allocations drift significantly",
                "Consider correlation between assets",
                "Suggest capital deployment strategies",
                "Monitor portfolio performance vs benchmarks",
                "Adapt allocation based on market conditions",
                "Provide clear rationale for allocation decisions"
            ],
            model_provider=model_provider,
            model_name=model_name,
            temperature=temperature,
            tools=["portfolio", "market_data"],
            enable_memory=True,
            show_tool_calls=True,
            markdown=True
        )

        super().__init__(config)

    def _initialize_tools(self):
        """Initialize portfolio management tools"""
        tools = []
        tools.extend(get_portfolio_tools())
        tools.extend(get_market_data_tools())
        return tools

    def optimize_portfolio(
        self,
        current_allocation: Dict[str, float],
        available_cash: float,
        session_id: Optional[str] = None
    ) -> Dict[str, Any]:
        """Optimize portfolio allocation"""

        allocation_str = "\n".join([
            f"- {symbol}: ${amount:.2f} ({amount / sum(current_allocation.values()) * 100:.1f}%)"
            for symbol, amount in current_allocation.items()
        ])

        prompt = f"""
        Optimize this portfolio allocation:

        **Current Allocation**:
        {allocation_str}

        **Available Cash**: ${available_cash}
        **Total Portfolio Value**: ${sum(current_allocation.values()) + available_cash}

        **Optimization Goals**:
        1. Propose optimal allocation across assets
        2. Ensure diversification (no single position >25%)
        3. Consider current market conditions
        4. Suggest rebalancing trades if needed
        5. Recommend how to deploy available cash

        Provide specific allocation percentages and reasoning.
        """

        response = self.run(prompt, session_id=session_id)

        return {
            "current_allocation": current_allocation,
            "available_cash": available_cash,
            "optimization": self._extract_response_content(response),
            "agent": "PortfolioManager"
        }
