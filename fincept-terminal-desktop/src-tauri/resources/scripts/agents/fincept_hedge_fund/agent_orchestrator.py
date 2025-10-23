# -*- coding: utf-8 -*-
# agent_orchestrator.py

"""
===== DATA SOURCES REQUIRED =====
# INPUT:
#   - ticker symbols (array)
#   - end_date (string)
#   - FINANCIAL_DATASETS_API_KEY
#
# OUTPUT:
#   - Financial metrics: ROE, D/E, Current Ratio
#   - Financial line items: Revenue, Net Income, FCF, Debt, Equity, Retained Earnings, Operating Margin, R&D, Shares
#   - Market Cap
#
# PARAMETERS:
#   - period="annual"
#   - limit=10 years
"""

import asyncio
import json
import logging
import numpy as np
from datetime import datetime
from typing import Dict, List, Any, Optional
from dataclasses import dataclass
import google.generativeai as genai

# Import all agents
from fincept_terminal.Agents.fincept_hedge_fund.macro_cycle_agent import MacroCycleAgent
from fincept_terminal.Agents.fincept_hedge_fund.central_bank_agent import CentralBankAgent
from fincept_terminal.Agents.fincept_hedge_fund.geopolitical_agent import GeopoliticalAgent
from fincept_terminal.Agents.fincept_hedge_fund.regulatory_agent import RegulatoryAgent
from fincept_terminal.Agents.fincept_hedge_fund.sentiment_agent import SentimentAgent
from fincept_terminal.Agents.fincept_hedge_fund.institutional_flow_agent import InstitutionalFlowAgent
from fincept_terminal.Agents.fincept_hedge_fund.supply_chain_agent import SupplyChainAgent
from fincept_terminal.Agents.fincept_hedge_fund.innovation_agent import InnovationAgent
from fincept_terminal.Agents.fincept_hedge_fund.currency_agent import CurrencyAgent
from fincept_terminal.Agents.fincept_hedge_fund.behavioral_agent import BehavioralAgent
from config import CONFIG


@dataclass
class AgentDecision:
    """Standardized agent decision structure"""
    agent_name: str
    timestamp: datetime
    signal_strength: float  # 0-1
    direction: str  # bullish, bearish, neutral
    confidence: float
    key_insights: List[str]
    market_implications: Dict[str, float]
    time_horizon: str
    raw_analysis: Dict[str, Any]


@dataclass
class ConsensusDecision:
    """Final consensus decision from all agents"""
    timestamp: datetime
    overall_signal: str  # strong_buy, buy, hold, sell, strong_sell
    conviction_level: float  # 0-1
    asset_allocation: Dict[str, float]
    sector_weights: Dict[str, float]
    risk_level: float
    consensus_factors: List[str]
    dissenting_views: List[str]
    execution_priority: str


class AgentOrchestrator:
    """Orchestrates multi-agent analysis with inter-agent communication"""

    def __init__(self, gemini_api_key: str):
        # Configure Gemini AI
        genai.configure(api_key=gemini_api_key)
        self.gemini_model = genai.GenerativeModel('gemini-pro')

        # Initialize all agents
        self.agents = {
            'macro': MacroCycleAgent(),
            'fed': CentralBankAgent(),
            'geo': GeopoliticalAgent(),
            'regulatory': RegulatoryAgent(),
            'sentiment': SentimentAgent(),
            'flows': InstitutionalFlowAgent(),
            'supply': SupplyChainAgent(),
            'innovation': InnovationAgent(),
            'fx': CurrencyAgent(),
            'behavioral': BehavioralAgent()
        }

        # Agent communication weights for decision making
        self.agent_weights = {
            'macro': 0.20,  # Highest weight - drives overall allocation
            'fed': 0.18,  # Critical for duration and rates
            'geo': 0.15,  # Major risk factor
            'flows': 0.12,  # Smart money indicator
            'sentiment': 0.10,  # Contrarian signals
            'regulatory': 0.08,  # Policy impacts
            'supply': 0.07,  # Sector-specific risks
            'fx': 0.05,  # Portfolio hedging
            'behavioral': 0.03,  # Market psychology
            'innovation': 0.02  # Long-term trends
        }

        # Setup logging
        logging.basicConfig(level=logging.INFO,
                            format='%(asctime)s - %(name)s - %(levelname)s - %(message)s')
        self.logger = logging.getLogger("AgentOrchestrator")

    async def run_comprehensive_analysis(self, target_assets: List[str] = None) -> ConsensusDecision:
        """Run comprehensive analysis with all agents and generate consensus decision"""
        self.logger.info("🚀 Starting comprehensive multi-agent analysis...")

        # Step 1: Run all agents concurrently
        agent_decisions = await self._execute_all_agents()

        # Step 2: Enable agent-to-agent communication
        enriched_decisions = await self._facilitate_agent_communication(agent_decisions)

        # Step 3: Generate consensus using Gemini AI
        consensus = await self._generate_consensus_decision(enriched_decisions, target_assets)

        # Step 4: Validate and refine decision
        final_decision = await self._validate_and_refine_decision(consensus, enriched_decisions)

        self.logger.info(f"✅ Analysis complete. Final signal: {final_decision.overall_signal}")
        return final_decision

    async def _execute_all_agents(self) -> Dict[str, AgentDecision]:
        """Execute all agents concurrently and standardize outputs"""
        self.logger.info("📊 Executing all agents concurrently...")

        async def run_single_agent(name: str, agent: Any) -> AgentDecision:
            try:
                # Get agent report
                if hasattr(agent, 'get_cycle_report'):
                    report = await agent.get_cycle_report()
                elif hasattr(agent, 'get_policy_report'):
                    report = await agent.get_policy_report()
                elif hasattr(agent, 'get_geopolitical_report'):
                    report = await agent.get_geopolitical_report()
                elif hasattr(agent, 'get_regulatory_report'):
                    report = await agent.get_regulatory_report()
                elif hasattr(agent, 'get_sentiment_report'):
                    report = await agent.get_sentiment_report()
                elif hasattr(agent, 'get_flow_report'):
                    report = await agent.get_flow_report()
                elif hasattr(agent, 'get_supply_chain_report'):
                    report = await agent.get_supply_chain_report()
                elif hasattr(agent, 'get_innovation_report'):
                    report = await agent.get_innovation_report()
                elif hasattr(agent, 'get_currency_report'):
                    report = await agent.get_currency_report()
                elif hasattr(agent, 'get_behavioral_report'):
                    report = await agent.get_behavioral_report()
                else:
                    report = {"error": f"No report method found for {name}"}

                # Standardize the output
                return self._standardize_agent_output(name, report)

            except Exception as e:
                self.logger.error(f"❌ Error in agent {name}: {e}")
                return self._create_error_decision(name, str(e))

        # Run all agents concurrently
        tasks = [run_single_agent(name, agent) for name, agent in self.agents.items()]
        results = await asyncio.gather(*tasks, return_exceptions=True)

        # Combine results
        agent_decisions = {}
        for i, (name, agent) in enumerate(self.agents.items()):
            if i < len(results) and isinstance(results[i], AgentDecision):
                agent_decisions[name] = results[i]
            else:
                agent_decisions[name] = self._create_error_decision(name, "Execution failed")

        self.logger.info(f"📈 Completed analysis from {len(agent_decisions)} agents")
        return agent_decisions

    def _standardize_agent_output(self, agent_name: str, report: Dict[str, Any]) -> AgentDecision:
        """Standardize agent outputs into common decision format"""
        try:
            # Extract key metrics based on agent type
            signal_strength, direction, confidence, insights, implications = self._extract_agent_metrics(agent_name,
                                                                                                         report)

            return AgentDecision(
                agent_name=agent_name,
                timestamp=datetime.now(),
                signal_strength=signal_strength,
                direction=direction,
                confidence=confidence,
                key_insights=insights,
                market_implications=implications,
                time_horizon=self._determine_time_horizon(agent_name, report),
                raw_analysis=report
            )

        except Exception as e:
            self.logger.error(f"Error standardizing {agent_name} output: {e}")
            return self._create_error_decision(agent_name, str(e))

    def _extract_agent_metrics(self, agent_name: str, report: Dict) -> tuple:
        """Extract standardized metrics from agent reports"""
        # Default values
        signal_strength = 0.5
        direction = "neutral"
        confidence = 0.5
        insights = []
        implications = {}

        try:
            if agent_name == 'macro':
                cycle_analysis = report.get('cycle_analysis', {})
                phase = cycle_analysis.get('current_phase', 'expansion')
                signal_strength = cycle_analysis.get('strength', 0.5)
                direction = "bullish" if phase in ['expansion', 'trough'] else "bearish"
                confidence = cycle_analysis.get('confidence', 0.5)
                insights = [f"Business cycle: {phase}"]
                implications = report.get('investment_implications', {})

            elif agent_name == 'fed':
                fed_analysis = report.get('fed_analysis', {})
                stance = fed_analysis.get('policy_stance', 'neutral')
                signal_strength = fed_analysis.get('probability', 0.5)
                direction = "bearish" if stance == 'hawkish' else "bullish" if stance == 'dovish' else "neutral"
                confidence = fed_analysis.get('confidence', 0.5)
                insights = [f"Fed stance: {stance}", f"Rate direction: {fed_analysis.get('rate_direction', 'holding')}"]
                implications = report.get('market_impact', {})

            elif agent_name == 'geo':
                risk_assessment = report.get('global_risk_assessment', {})
                risk_level = risk_assessment.get('overall_risk_level', 5)
                signal_strength = min(risk_level / 10, 1.0)
                direction = "bearish" if risk_level > 6 else "neutral"
                confidence = 0.7
                top_risks = risk_assessment.get('top_risks', [])
                insights = [f"Geopolitical risk: {risk_level}/10"] + [risk.get('type', '') for risk in top_risks[:2]]
                implications = report.get('investment_implications', {})

            elif agent_name == 'sentiment':
                sentiment_analysis = report.get('sentiment_analysis', {})
                overall_sentiment = sentiment_analysis.get('overall_sentiment', 0)
                signal_strength = abs(overall_sentiment)
                direction = "bullish" if overall_sentiment > 0.2 else "bearish" if overall_sentiment < -0.2 else "neutral"
                confidence = 0.6
                insights = [f"Market sentiment: {sentiment_analysis.get('sentiment_classification', 'neutral')}"]
                contrarian_signals = report.get('contrarian_signals', [])
                if contrarian_signals:
                    insights.append(f"Contrarian opportunities: {len(contrarian_signals)}")
                implications = {"sentiment_momentum": overall_sentiment}

            elif agent_name == 'flows':
                flow_analysis = report.get('flow_analysis', {})
                flow_sentiment = flow_analysis.get('overall_flow_sentiment', 0)
                signal_strength = abs(flow_sentiment)
                direction = "bullish" if flow_sentiment > 0.1 else "bearish" if flow_sentiment < -0.1 else "neutral"
                confidence = 0.8
                insights = [f"Institutional flows: {flow_analysis.get('flow_classification', 'balanced')}"]
                implications = {"flow_momentum": flow_sentiment}

            # Add similar extraction logic for other agents...
            else:
                # Generic extraction for other agents
                insights = ["Analysis completed"]
                implications = {"general_impact": 0.0}

        except Exception as e:
            self.logger.error(f"Error extracting metrics for {agent_name}: {e}")

        return signal_strength, direction, confidence, insights, implications

    def _determine_time_horizon(self, agent_name: str, report: Dict) -> str:
        """Determine time horizon based on agent type"""
        horizon_map = {
            'macro': 'long_term',
            'fed': 'medium_term',
            'geo': 'medium_term',
            'regulatory': 'long_term',
            'sentiment': 'short_term',
            'flows': 'short_term',
            'supply': 'medium_term',
            'innovation': 'long_term',
            'fx': 'short_term',
            'behavioral': 'short_term'
        }
        return horizon_map.get(agent_name, 'medium_term')

    async def _facilitate_agent_communication(self, decisions: Dict[str, AgentDecision]) -> Dict[str, AgentDecision]:
        """Enable agents to communicate and refine their decisions based on peer insights"""
        self.logger.info("🤝 Facilitating inter-agent communication...")

        # Create communication summary for each agent
        for agent_name, decision in decisions.items():
            # Get peer insights relevant to this agent
            peer_insights = self._get_relevant_peer_insights(agent_name, decisions)

            # Use Gemini to refine the decision based on peer insights
            if peer_insights:
                refined_decision = await self._refine_decision_with_peers(decision, peer_insights)
                decisions[agent_name] = refined_decision

        return decisions

    def _get_relevant_peer_insights(self, agent_name: str, decisions: Dict[str, AgentDecision]) -> Dict[str, str]:
        """Get insights from peer agents relevant to the current agent"""
        peer_insights = {}

        # Define agent relationships and relevant insights
        agent_relationships = {
            'macro': ['fed', 'geo', 'flows'],  # Macro affected by policy, geopolitics, flows
            'fed': ['macro', 'sentiment', 'flows'],  # Fed policy affects sentiment, flows
            'geo': ['supply', 'fx', 'flows'],  # Geopolitics affects supply chains, currency, flows
            'sentiment': ['behavioral', 'flows', 'macro'],  # Sentiment relates to behavior, flows, macro
            'flows': ['sentiment', 'macro', 'fed'],  # Flows influenced by sentiment, macro, policy
            'supply': ['geo', 'innovation', 'regulatory'],  # Supply chains affected by geopolitics, innovation
            'regulatory': ['innovation', 'supply', 'flows'],  # Regulation affects innovation, supply, flows
            'fx': ['fed', 'geo', 'flows'],  # Currency affected by policy, geopolitics, flows
            'behavioral': ['sentiment', 'flows', 'macro'],  # Behavior relates to sentiment, flows, macro
            'innovation': ['regulatory', 'supply', 'macro']  # Innovation affected by regulation, supply, macro
        }

        relevant_agents = agent_relationships.get(agent_name, [])

        for peer_agent in relevant_agents:
            if peer_agent in decisions:
                peer_decision = decisions[peer_agent]
                insight_summary = f"{peer_decision.direction} signal ({peer_decision.signal_strength:.2f} strength) - {peer_decision.key_insights[0] if peer_decision.key_insights else 'No specific insights'}"
                peer_insights[peer_agent] = insight_summary

        return peer_insights

    async def _refine_decision_with_peers(self, original_decision: AgentDecision,
                                          peer_insights: Dict[str, str]) -> AgentDecision:
        """Use Gemini AI to refine agent decision based on peer insights"""
        try:
            peer_summary = "\n".join([f"{agent}: {insight}" for agent, insight in peer_insights.items()])

            prompt = f"""
            As the {original_decision.agent_name} agent, refine your analysis considering peer agent insights:

            Your Original Analysis:
            - Signal: {original_decision.direction} ({original_decision.signal_strength:.2f} strength)
            - Confidence: {original_decision.confidence:.2f}
            - Key Insights: {', '.join(original_decision.key_insights)}

            Peer Agent Insights:
            {peer_summary}

            Considering these peer insights, should you:
            1. Maintain your original assessment
            2. Increase/decrease signal strength
            3. Change direction
            4. Adjust confidence level

            Provide response in JSON format:
            {{
                "action": "maintain/strengthen/weaken/reverse",
                "new_signal_strength": 0.0-1.0,
                "new_direction": "bullish/bearish/neutral", 
                "new_confidence": 0.0-1.0,
                "reasoning": "Brief explanation",
                "key_factors": ["factor1", "factor2"]
            }}
            """

            response = self.gemini_model.generate_content(prompt)
            refinement = json.loads(response.text)

            # Update decision based on Gemini's analysis
            original_decision.signal_strength = refinement.get("new_signal_strength", original_decision.signal_strength)
            original_decision.direction = refinement.get("new_direction", original_decision.direction)
            original_decision.confidence = refinement.get("new_confidence", original_decision.confidence)
            original_decision.key_insights.append(f"Peer-refined: {refinement.get('reasoning', 'No reasoning')}")

            return original_decision

        except Exception as e:
            self.logger.error(f"Error refining decision with Gemini: {e}")
            return original_decision

    async def _generate_consensus_decision(self, decisions: Dict[str, AgentDecision],
                                           target_assets: List[str] = None) -> ConsensusDecision:
        """Generate final consensus decision using Gemini AI"""
        self.logger.info("🧠 Generating consensus decision with Gemini AI...")

        # Prepare decision summary
        decision_summary = self._prepare_decision_summary(decisions)

        prompt = f"""
        As a senior portfolio manager, analyze these AI agent insights and create a final investment decision:

        Agent Analysis Summary:
        {decision_summary}

        Agent Weights for Decision Making:
        {json.dumps(self.agent_weights, indent=2)}

        Target Assets: {target_assets or "Broad market exposure"}

        Generate a comprehensive investment decision with:
        1. Overall signal (strong_buy/buy/hold/sell/strong_sell)
        2. Conviction level (0-1)
        3. Asset allocation percentages
        4. Sector weight recommendations
        5. Risk level assessment (0-1)
        6. Key consensus factors
        7. Main dissenting views
        8. Execution priority (immediate/gradual/wait)

        Consider:
        - Agent reliability and track record
        - Signal consistency across agents
        - Risk-adjusted returns
        - Current market regime
        - Diversification principles

        Provide response in JSON format with all requested fields.
        """

        try:
            response = self.gemini_model.generate_content(prompt)
            consensus_data = json.loads(response.text)

            return ConsensusDecision(
                timestamp=datetime.now(),
                overall_signal=consensus_data.get("overall_signal", "hold"),
                conviction_level=consensus_data.get("conviction_level", 0.5),
                asset_allocation=consensus_data.get("asset_allocation", {"stocks": 0.6, "bonds": 0.4}),
                sector_weights=consensus_data.get("sector_weights", {}),
                risk_level=consensus_data.get("risk_level", 0.5),
                consensus_factors=consensus_data.get("consensus_factors", []),
                dissenting_views=consensus_data.get("dissenting_views", []),
                execution_priority=consensus_data.get("execution_priority", "gradual")
            )

        except Exception as e:
            self.logger.error(f"Error generating consensus with Gemini: {e}")
            return self._create_default_consensus(decisions)

    def _prepare_decision_summary(self, decisions: Dict[str, AgentDecision]) -> str:
        """Prepare summary of all agent decisions"""
        summary_lines = []

        for agent_name, decision in decisions.items():
            weight = self.agent_weights.get(agent_name, 0.0)
            summary_lines.append(
                f"{agent_name.upper()} (Weight: {weight:.1%}): "
                f"{decision.direction} signal, "
                f"Strength: {decision.signal_strength:.2f}, "
                f"Confidence: {decision.confidence:.2f}, "
                f"Horizon: {decision.time_horizon}, "
                f"Key: {decision.key_insights[0] if decision.key_insights else 'N/A'}"
            )

        return "\n".join(summary_lines)

    async def _validate_and_refine_decision(self, consensus: ConsensusDecision,
                                            decisions: Dict[str, AgentDecision]) -> ConsensusDecision:
        """Validate and refine the final decision"""
        self.logger.info("✅ Validating and refining final decision...")

        # Risk checks
        if consensus.risk_level > 0.8:
            self.logger.warning("⚠️  High risk level detected, adjusting position sizes...")
            # Reduce allocation to risky assets
            for asset in ["stocks", "growth", "emerging_markets"]:
                if asset in consensus.asset_allocation:
                    consensus.asset_allocation[asset] *= 0.8

        # Consistency checks
        bullish_agents = len([d for d in decisions.values() if d.direction == "bullish"])
        bearish_agents = len([d for d in decisions.values() if d.direction == "bearish"])

        if abs(bullish_agents - bearish_agents) <= 1:
            self.logger.info("📊 Mixed signals detected, reducing conviction...")
            consensus.conviction_level *= 0.8
            consensus.execution_priority = "gradual"

        return consensus

    def _create_error_decision(self, agent_name: str, error_msg: str) -> AgentDecision:
        """Create default decision for failed agents"""
        return AgentDecision(
            agent_name=agent_name,
            timestamp=datetime.now(),
            signal_strength=0.0,
            direction="neutral",
            confidence=0.0,
            key_insights=[f"Error: {error_msg}"],
            market_implications={},
            time_horizon="unknown",
            raw_analysis={"error": error_msg}
        )

    def _create_default_consensus(self, decisions: Dict[str, AgentDecision]) -> ConsensusDecision:
        """Create default consensus when Gemini fails"""
        # Simple weighted average approach
        total_weighted_signal = 0.0
        total_weight = 0.0

        for agent_name, decision in decisions.items():
            weight = self.agent_weights.get(agent_name, 0.0)
            signal_value = decision.signal_strength * (
                1 if decision.direction == "bullish" else -1 if decision.direction == "bearish" else 0)
            total_weighted_signal += signal_value * weight
            total_weight += weight

        avg_signal = total_weighted_signal / total_weight if total_weight > 0 else 0.0

        if avg_signal > 0.3:
            overall_signal = "buy"
        elif avg_signal < -0.3:
            overall_signal = "sell"
        else:
            overall_signal = "hold"

        return ConsensusDecision(
            timestamp=datetime.now(),
            overall_signal=overall_signal,
            conviction_level=abs(avg_signal),
            asset_allocation={"stocks": 0.6, "bonds": 0.3, "cash": 0.1},
            sector_weights={},
            risk_level=0.5,
            consensus_factors=["Default consensus due to system error"],
            dissenting_views=[],
            execution_priority="gradual"
        )


async def run_hedge_fund_analysis(gemini_api_key: str, target_assets: List[str] = None):
    """Main function to run complete hedge fund analysis"""
    orchestrator = AgentOrchestrator(gemini_api_key)

    print("🏦 AI HEDGE FUND - MULTI-AGENT ANALYSIS")
    print("=" * 50)

    # Run comprehensive analysis
    decision = await orchestrator.run_comprehensive_analysis(target_assets)

    # Display results
    print(f"\n📈 FINAL INVESTMENT DECISION")
    print(f"Signal: {decision.overall_signal.upper()}")
    print(f"Conviction: {decision.conviction_level:.1%}")
    print(f"Risk Level: {decision.risk_level:.1%}")
    print(f"Execution: {decision.execution_priority}")

    print(f"\n💼 ASSET ALLOCATION:")
    for asset, weight in decision.asset_allocation.items():
        print(f"  {asset.capitalize()}: {weight:.1%}")

    print(f"\n🔍 KEY CONSENSUS FACTORS:")
    for factor in decision.consensus_factors[:3]:
        print(f"  • {factor}")

    if decision.dissenting_views:
        print(f"\n⚠️  DISSENTING VIEWS:")
        for view in decision.dissenting_views[:2]:
            print(f"  • {view}")

    return decision


# Example usage
if __name__ == "__main__":
    import os

    # Set your Gemini API key
    GEMINI_API_KEY = os.getenv("GEMINI_API_KEY", "your-gemini-api-key-here")

    # Define target assets (optional)
    TARGET_ASSETS = ["SPY", "QQQ", "TLT", "GLD", "VXX"]

    # Run the analysis
    asyncio.run(run_hedge_fund_analysis(GEMINI_API_KEY, TARGET_ASSETS))