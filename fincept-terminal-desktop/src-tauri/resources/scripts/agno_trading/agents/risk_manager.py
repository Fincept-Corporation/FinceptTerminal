"""
Risk Manager Agent

Specialized agent for risk management and portfolio protection.
"""

from typing import Dict, Any, Optional, List
from core.base_agent import BaseAgent, AgentConfig
from tools.portfolio_tools import get_portfolio_tools


class RiskManagerAgent(BaseAgent):
    """
    Risk Manager Agent

    Specializes in:
    - Portfolio risk assessment
    - Position sizing
    - Stop loss recommendations
    - Drawdown management
    - Risk/reward analysis
    """

    def __init__(
        self,
        model_provider: str = "openai",
        model_name: str = "gpt-4",
        risk_tolerance: str = "moderate",
        temperature: float = 0.2  # Very analytical
    ):
        """Initialize Risk Manager Agent"""

        config = AgentConfig(
            name="RiskManager",
            role="Risk Management Specialist",
            description=(
                "Expert risk manager focused on capital preservation and risk-adjusted returns. "
                "Ensures all trades comply with risk management rules and portfolio safety."
            ),
            instructions=[
                "Never risk more than 2% of portfolio on a single trade (conservative)",
                "Ensure risk/reward ratio is at least 1:2",
                "Monitor overall portfolio exposure and correlation",
                "Recommend appropriate position sizes based on volatility",
                "Identify when portfolio is overextended",
                "Suggest protective measures during high volatility",
                "Calculate and monitor maximum drawdown",
                "Ensure diversification across uncorrelated assets",
                "Be conservative - when in doubt, reduce risk"
            ],
            model_provider=model_provider,
            model_name=model_name,
            temperature=temperature,
            tools=["portfolio"],
            risk_tolerance=risk_tolerance,
            enable_memory=True,
            show_tool_calls=True,
            markdown=True
        )

        super().__init__(config)

    def _initialize_tools(self):
        """Initialize risk management specific tools"""
        return get_portfolio_tools()

    def assess_trade_risk(
        self,
        trade: Dict[str, Any],
        portfolio_value: float,
        session_id: Optional[str] = None
    ) -> Dict[str, Any]:
        """Assess risk for a proposed trade"""

        prompt = f"""
        Assess the risk for this trade:

        **Trade Details**:
        - Symbol: {trade.get('symbol')}
        - Direction: {trade.get('direction')}
        - Entry: ${trade.get('entry_price')}
        - Stop Loss: ${trade.get('stop_loss')}
        - Take Profit: ${trade.get('take_profit')}
        - Proposed Position Size: {trade.get('position_size')} units

        **Portfolio**:
        - Total Value: ${portfolio_value}

        **Analysis Required**:
        1. Calculate risk amount and % of portfolio
        2. Calculate risk/reward ratio
        3. Assess if within risk parameters (max 2% risk per trade)
        4. Recommend optimal position size if adjustment needed
        5. Evaluate stop loss placement
        6. Overall risk assessment: APPROVED, APPROVED_WITH_CHANGES, or REJECTED

        Provide clear recommendation with reasoning.
        """

        response = self.run(prompt, session_id=session_id)

        return {
            "trade": trade,
            "portfolio_value": portfolio_value,
            "assessment": self._extract_response_content(response),
            "agent": "RiskManager"
        }

    def analyze_portfolio_risk(
        self,
        positions: List[Dict[str, Any]],
        portfolio_value: float,
        session_id: Optional[str] = None
    ) -> Dict[str, Any]:
        """Analyze overall portfolio risk"""

        positions_summary = "\n".join([
            f"- {p.get('symbol')}: {p.get('quantity')} units @ ${p.get('current_price')} "
            f"(Value: ${p.get('quantity', 0) * p.get('current_price', 0):.2f})"
            for p in positions
        ])

        prompt = f"""
        Analyze portfolio risk:

        **Portfolio Value**: ${portfolio_value}

        **Current Positions**:
        {positions_summary}

        **Risk Analysis Required**:
        1. Position concentration (% of portfolio in each position)
        2. Overall exposure (long vs short)
        3. Diversification assessment
        4. Correlation risk (if positions are related)
        5. Maximum drawdown potential
        6. Recommendations for risk reduction if needed

        Risk Tolerance: {self.config.risk_tolerance}

        Provide actionable recommendations to optimize risk.
        """

        response = self.run(prompt, session_id=session_id)

        return {
            "portfolio_value": portfolio_value,
            "position_count": len(positions),
            "risk_analysis": self._extract_response_content(response),
            "agent": "RiskManager"
        }
