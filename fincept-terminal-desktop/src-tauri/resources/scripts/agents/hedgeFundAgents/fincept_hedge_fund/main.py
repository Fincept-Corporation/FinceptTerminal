# -*- coding: utf-8 -*-
# main.py - AI Hedge Fund Orchestrator and Execution Engine

"""
===== DATA SOURCES REQUIRED =====
# INPUT:
#   - Agent reports from all hedge fund agents
#   - Market data feeds (real-time and historical)
#   - Portfolio current positions and allocations
#   - Risk management parameters and limits
#   - Trading signals and market regime data
#
# OUTPUT:
#   - Aggregated trading signals with conviction scores
#   - Portfolio recommendations with asset allocations
#   - Risk-adjusted position sizes using Kelly Criterion
#   - Execution results with trading costs and slippage
#   - Performance metrics and portfolio analytics
#
# PARAMETERS:
#   - max_position_size: Maximum single position limit (default 10%)
#   - max_sector_exposure: Maximum sector allocation limit (default 25%)
#   - var_limit: Value-at-Risk limit (default 5%)
#   - update_frequency: Analysis cycle frequency (default 60 seconds)
#   - agent_weights: Weight distribution for different agents
"""

import asyncio
import logging
import json
import pandas as pd
import numpy as np
from datetime import datetime, timedelta
from typing import Dict, List, Optional, Any
from dataclasses import dataclass
import signal
import sys
from concurrent.futures import ThreadPoolExecutor
import time

from config import CONFIG
from data_feeds import DataFeedManager

# Import all agents
from agents.macro_cycle_agent import MacroCycleAgent
from agents.central_bank_agent import CentralBankAgent
from agents.geopolitical_agent import GeopoliticalAgent
from agents.regulatory_agent import RegulatoryAgent
from agents.sentiment_agent import SentimentAgent
from agents.institutional_flow_agent import InstitutionalFlowAgent
from agents.supply_chain_agent import SupplyChainAgent
from agents.innovation_agent import InnovationAgent
from agents.currency_agent import CurrencyAgent
from agents.behavioral_agent import BehavioralAgent


@dataclass
class TradingSignal:
    """Aggregated trading signal from all agents"""
    timestamp: datetime
    asset_class: str
    direction: str  # long, short, neutral
    conviction: float  # 0-1
    position_size: float  # 0-1
    time_horizon: str  # short, medium, long
    risk_factors: List[str]
    supporting_agents: List[str]
    market_regime: str


@dataclass
class PortfolioRecommendation:
    """Portfolio-level recommendations"""
    timestamp: datetime
    asset_allocation: Dict[str, float]
    sector_weights: Dict[str, float]
    regional_weights: Dict[str, float]
    risk_budget: Dict[str, float]
    hedges: List[str]
    cash_allocation: float
    leverage: float


class RiskManager:
    """Portfolio risk management and position sizing"""

    def __init__(self):
        self.max_position_size = CONFIG.risk.max_position_size
        self.max_sector_exposure = CONFIG.risk.max_sector_exposure
        self.var_limit = CONFIG.risk.var_limit
        self.correlation_limit = CONFIG.risk.correlation_limit

    def validate_signal(self, signal: TradingSignal, current_portfolio: Dict) -> bool:
        """Validate trading signal against risk limits"""
        # Position size check
        if signal.position_size > self.max_position_size:
            logging.warning(f"Signal exceeds max position size: {signal.position_size}")
            return False

        # Sector concentration check
        sector_exposure = current_portfolio.get('sectors', {})
        if signal.asset_class in sector_exposure:
            new_exposure = sector_exposure[signal.asset_class] + signal.position_size
            if new_exposure > self.max_sector_exposure:
                logging.warning(f"Signal would exceed sector limits: {new_exposure}")
                return False

        # Risk budget check
        total_risk = sum(current_portfolio.get('risk_budget', {}).values())
        if total_risk + signal.conviction * signal.position_size > 1.0:
            logging.warning("Signal would exceed total risk budget")
            return False

        return True

    def size_position(self, signal: TradingSignal, market_volatility: float) -> float:
        """Calculate optimal position size based on Kelly Criterion"""
        # Kelly formula: f = (bp - q) / b
        # where b = odds, p = probability of win, q = probability of loss

        win_probability = signal.conviction
        loss_probability = 1 - win_probability

        # Estimate expected return based on conviction and market regime
        expected_return = signal.conviction * 0.15  # 15% max expected return

        # Adjust for volatility
        volatility_adjusted_return = expected_return / max(market_volatility, 0.1)

        # Kelly sizing with maximum cap
        kelly_fraction = volatility_adjusted_return / (market_volatility ** 2)

        # Apply safety factor and caps
        safety_factor = 0.25  # Use 1/4 Kelly for safety
        optimal_size = kelly_fraction * safety_factor

        # Cap at configured maximum
        return min(optimal_size, signal.position_size, self.max_position_size)

    def calculate_portfolio_var(self, positions: Dict[str, float],
                                returns_covariance: np.ndarray) -> float:
        """Calculate portfolio Value-at-Risk"""
        if not positions or returns_covariance.size == 0:
            return 0.0

        position_vector = np.array(list(positions.values()))
        portfolio_variance = np.dot(position_vector, np.dot(returns_covariance, position_vector))
        portfolio_volatility = np.sqrt(portfolio_variance)

        # 95% VaR (1.645 standard deviations)
        var_95 = 1.645 * portfolio_volatility
        return min(var_95, 1.0)


class DecisionEngine:
    """Aggregates agent signals and makes final trading decisions"""

    def __init__(self):
        self.agent_weights = CONFIG.agent_weights
        self.risk_manager = RiskManager()

    def aggregate_signals(self, agent_reports: Dict[str, Dict]) -> List[TradingSignal]:
        """Aggregate signals from all agents into trading recommendations"""
        signals = []

        # Combine macro and policy signals for asset allocation
        macro_signal = self._extract_macro_signal(agent_reports)
        if macro_signal:
            signals.append(macro_signal)

        # Generate sector rotation signals
        sector_signals = self._extract_sector_signals(agent_reports)
        signals.extend(sector_signals)

        # Generate currency signals
        currency_signal = self._extract_currency_signal(agent_reports)
        if currency_signal:
            signals.append(currency_signal)

        # Generate defensive/risk-on signals
        risk_signal = self._extract_risk_signal(agent_reports)
        if risk_signal:
            signals.append(risk_signal)

        return signals

    def _extract_macro_signal(self, reports: Dict[str, Dict]) -> Optional[TradingSignal]:
        """Extract macro asset allocation signal"""
        macro_report = reports.get('macro_cycle', {})
        fed_report = reports.get('central_bank', {})

        if not macro_report or not fed_report:
            return None

        # Combine cycle and policy analysis
        cycle_phase = macro_report.get('cycle_analysis', {}).get('current_phase', 'expansion')
        policy_stance = fed_report.get('fed_analysis', {}).get('policy_stance', 'neutral')

        # Determine asset allocation bias
        if cycle_phase == 'expansion' and policy_stance in ['neutral', 'dovish']:
            direction = 'long'
            asset_class = 'equities'
            conviction = 0.7
        elif cycle_phase == 'contraction' or policy_stance == 'hawkish':
            direction = 'long'
            asset_class = 'bonds'
            conviction = 0.6
        elif cycle_phase == 'peak':
            direction = 'short'
            asset_class = 'equities'
            conviction = 0.5
        else:
            return None

        supporting_agents = ['macro_cycle', 'central_bank']

        return TradingSignal(
            timestamp=datetime.now(),
            asset_class=asset_class,
            direction=direction,
            conviction=conviction,
            position_size=0.4,  # 40% allocation
            time_horizon='medium',
            risk_factors=['cycle_timing', 'policy_error'],
            supporting_agents=supporting_agents,
            market_regime=f"{cycle_phase}_{policy_stance}"
        )

    def _extract_sector_signals(self, reports: Dict[str, Dict]) -> List[TradingSignal]:
        """Extract sector rotation signals"""
        signals = []

        # Get sector implications from multiple agents
        sector_scores = {}

        # Macro cycle implications
        macro_report = reports.get('macro_cycle', {})
        if macro_report:
            implications = macro_report.get('investment_implications', {})
            for sector, weight in implications.items():
                sector_scores[sector] = sector_scores.get(sector, 0) + weight * self.agent_weights.get('macro_cycle', 0)

        # Geopolitical implications
        geo_report = reports.get('geopolitical', {})
        if geo_report:
            sector_rotation = geo_report.get('investment_implications', {}).get('sector_rotation', {})
            for sector, recommendation in sector_rotation.items():
                weight = 0.2 if recommendation == 'overweight' else -0.2 if recommendation == 'underweight' else 0
                sector_scores[sector] = sector_scores.get(sector, 0) + weight * self.agent_weights.get('geopolitical',
                                                                                                       0)

        # Convert scores to signals
        for sector, score in sector_scores.items():
            if abs(score) > 0.1:  # Minimum threshold
                direction = 'long' if score > 0 else 'short'
                conviction = min(abs(score) * 2, 1.0)  # Scale to 0-1

                signals.append(TradingSignal(
                    timestamp=datetime.now(),
                    asset_class=sector,
                    direction=direction,
                    conviction=conviction,
                    position_size=min(conviction * 0.15, 0.1),  # Max 10% per sector
                    time_horizon='medium',
                    risk_factors=['sector_rotation'],
                    supporting_agents=['macro_cycle', 'geopolitical'],
                    market_regime='sector_rotation'
                ))

        return signals

    def _extract_currency_signal(self, reports: Dict[str, Dict]) -> Optional[TradingSignal]:
        """Extract currency trading signal"""
        fed_report = reports.get('central_bank', {})
        geo_report = reports.get('geopolitical', {})

        if not fed_report:
            return None

        # Fed policy impact on USD
        policy_stance = fed_report.get('fed_analysis', {}).get('policy_stance', 'neutral')

        if policy_stance == 'hawkish':
            direction = 'long'
            conviction = 0.6
        elif policy_stance == 'dovish':
            direction = 'short'
            conviction = 0.5
        else:
            return None

        # Adjust for geopolitical factors
        if geo_report:
            defensive_score = geo_report.get('investment_implications', {}).get('defensive_positioning', 0)
            if defensive_score > 0.5:
                # High geopolitical risk favors USD as safe haven
                direction = 'long'
                conviction = min(conviction + defensive_score * 0.3, 1.0)

        return TradingSignal(
            timestamp=datetime.now(),
            asset_class='USD',
            direction=direction,
            conviction=conviction,
            position_size=0.2,  # 20% currency allocation
            time_horizon='short',
            risk_factors=['policy_divergence', 'geopolitical_events'],
            supporting_agents=['central_bank', 'geopolitical'],
            market_regime=f"currency_{policy_stance}"
        )

    def _extract_risk_signal(self, reports: Dict[str, Dict]) -> Optional[TradingSignal]:
        """Extract overall risk-on/risk-off signal"""
        risk_factors = []
        risk_score = 0.0

        # Aggregate risk from all agents
        for agent_name, report in reports.items():
            agent_weight = self.agent_weights.get(agent_name, 0)

            if agent_name == 'geopolitical':
                geo_risk = report.get('global_risk_assessment', {}).get('overall_risk_level', 5)
                risk_score += (geo_risk - 5) / 5 * agent_weight  # Normalize to -1 to 1

            elif agent_name == 'sentiment':
                sentiment_score = report.get('sentiment_analysis', {}).get('overall_sentiment', 0)
                risk_score += sentiment_score * agent_weight

            elif agent_name == 'institutional_flow':
                flow_signal = report.get('flow_analysis', {}).get('risk_sentiment', 0)
                risk_score += flow_signal * agent_weight

        # Convert to trading signal
        if abs(risk_score) > 0.2:  # Minimum threshold
            if risk_score > 0:
                # Risk-on environment
                asset_class = 'risk_assets'
                direction = 'long'
            else:
                # Risk-off environment
                asset_class = 'safe_havens'
                direction = 'long'

            conviction = min(abs(risk_score), 1.0)

            return TradingSignal(
                timestamp=datetime.now(),
                asset_class=asset_class,
                direction=direction,
                conviction=conviction,
                position_size=conviction * 0.3,  # Scale position size
                time_horizon='short',
                risk_factors=risk_factors,
                supporting_agents=list(reports.keys()),
                market_regime='risk_on' if risk_score > 0 else 'risk_off'
            )

        return None

    def generate_portfolio_recommendation(self, signals: List[TradingSignal],
                                          current_portfolio: Dict) -> PortfolioRecommendation:
        """Generate comprehensive portfolio recommendation"""

        # Initialize allocations
        asset_allocation = {
            'equities': 0.6,
            'bonds': 0.3,
            'commodities': 0.05,
            'cash': 0.05
        }

        sector_weights = {}
        regional_weights = CONFIG.regional_weights.copy()
        hedges = []

        # Apply signals to modify allocations
        for signal in signals:
            validated = self.risk_manager.validate_signal(signal, current_portfolio)

            if validated:
                if signal.asset_class in asset_allocation:
                    # Adjust asset class allocation
                    adjustment = signal.position_size * (1 if signal.direction == 'long' else -1)
                    asset_allocation[signal.asset_class] += adjustment

                elif signal.asset_class in ['risk_assets', 'safe_havens']:
                    # Risk-on/off adjustment
                    if signal.asset_class == 'risk_assets' and signal.direction == 'long':
                        asset_allocation['equities'] += signal.position_size * 0.5
                        asset_allocation['bonds'] -= signal.position_size * 0.3
                    elif signal.asset_class == 'safe_havens' and signal.direction == 'long':
                        asset_allocation['bonds'] += signal.position_size * 0.4
                        asset_allocation['cash'] += signal.position_size * 0.2
                        asset_allocation['equities'] -= signal.position_size * 0.4

                # Collect sector weights
                if signal.asset_class not in ['USD', 'risk_assets', 'safe_havens']:
                    sector_weights[signal.asset_class] = signal.position_size * (
                        1 if signal.direction == 'long' else -1)

                # Collect hedging requirements
                hedges.extend(signal.risk_factors)

        # Normalize allocations
        total_allocation = sum(asset_allocation.values())
        if total_allocation != 1.0:
            for asset in asset_allocation:
                asset_allocation[asset] /= total_allocation

        # Set risk budget based on conviction levels
        risk_budget = {}
        for signal in signals:
            risk_budget[signal.asset_class] = signal.conviction * signal.position_size

        return PortfolioRecommendation(
            timestamp=datetime.now(),
            asset_allocation=asset_allocation,
            sector_weights=sector_weights,
            regional_weights=regional_weights,
            risk_budget=risk_budget,
            hedges=list(set(hedges)),  # Remove duplicates
            cash_allocation=asset_allocation.get('cash', 0.05),
            leverage=1.0  # No leverage for now
        )


class HedgeFundOrchestrator:
    """Main orchestrator for the AI hedge fund system"""

    def __init__(self):
        self.data_manager = DataFeedManager()
        self.decision_engine = DecisionEngine()
        self.risk_manager = RiskManager()

        # Initialize all agents
        self.agents = {
            'macro_cycle': MacroCycleAgent(),
            'central_bank': CentralBankAgent(),
            'geopolitical': GeopoliticalAgent(),
            'regulatory': RegulatoryAgent(),
            'sentiment': SentimentAgent(),
            'institutional_flow': InstitutionalFlowAgent(),
            'supply_chain': SupplyChainAgent(),
            'innovation': InnovationAgent(),
            'currency': CurrencyAgent(),
            'behavioral': BehavioralAgent()
        }

        self.current_portfolio = {
            'positions': {},
            'sectors': {},
            'risk_budget': {},
            'last_rebalance': datetime.now()
        }

        self.is_running = False
        self.update_interval = CONFIG.agent.update_frequency  # seconds

        # Setup logging
        self._setup_logging()

    def _setup_logging(self):
        """Setup comprehensive logging"""
        logging.basicConfig(
            level=CONFIG.log_level,
            format=CONFIG.log_format,
            handlers=[
                logging.FileHandler(CONFIG.logs_dir / f"hedge_fund_{datetime.now().strftime('%Y%m%d')}.log"),
                logging.StreamHandler(sys.stdout)
            ]
        )

        # Create separate loggers for different components
        self.logger = logging.getLogger("HedgeFundOrchestrator")
        self.trade_logger = logging.getLogger("TradingSignals")
        self.risk_logger = logging.getLogger("RiskManagement")

    async def run_analysis_cycle(self) -> Dict[str, Any]:
        """Run one complete analysis cycle"""
        cycle_start = time.time()
        self.logger.info("Starting analysis cycle")

        try:
            # Run all agents concurrently
            agent_reports = await self._run_all_agents()

            # Generate trading signals
            signals = self.decision_engine.aggregate_signals(agent_reports)

            # Generate portfolio recommendation
            portfolio_rec = self.decision_engine.generate_portfolio_recommendation(
                signals, self.current_portfolio
            )

            # Execute trades (simulated)
            execution_results = await self._execute_trades(signals, portfolio_rec)

            # Update portfolio state
            self._update_portfolio_state(execution_results, portfolio_rec)

            # Log performance
            cycle_time = time.time() - cycle_start
            self.logger.info(f"Analysis cycle completed in {cycle_time:.2f} seconds")

            return {
                'timestamp': datetime.now().isoformat(),
                'cycle_time': cycle_time,
                'agent_reports': agent_reports,
                'trading_signals': [self._signal_to_dict(s) for s in signals],
                'portfolio_recommendation': self._portfolio_to_dict(portfolio_rec),
                'execution_results': execution_results,
                'current_portfolio': self.current_portfolio
            }

        except Exception as e:
            self.logger.error(f"Error in analysis cycle: {e}")
            return {'error': str(e), 'timestamp': datetime.now().isoformat()}

    async def _run_all_agents(self) -> Dict[str, Dict]:
        """Run all agents concurrently"""
        self.logger.info("Running agent analysis")

        async def run_agent(name: str, agent):
            try:
                if hasattr(agent, 'get_cycle_report'):
                    return await agent.get_cycle_report()
                elif hasattr(agent, 'get_policy_report'):
                    return await agent.get_policy_report()
                elif hasattr(agent, 'get_geopolitical_report'):
                    return await agent.get_geopolitical_report()
                else:
                    # Generic report method
                    return await agent.generate_report()
            except Exception as e:
                self.logger.error(f"Error running agent {name}: {e}")
                return {'error': str(e), 'agent': name}

        # Run agents concurrently
        tasks = [run_agent(name, agent) for name, agent in self.agents.items()]
        results = await asyncio.gather(*tasks, return_exceptions=True)

        # Combine results
        agent_reports = {}
        for i, (name, agent) in enumerate(self.agents.items()):
            if i < len(results) and not isinstance(results[i], Exception):
                agent_reports[name] = results[i]
            else:
                self.logger.error(f"Agent {name} failed: {results[i] if i < len(results) else 'Unknown error'}")
                agent_reports[name] = {'error': 'Agent failed', 'agent': name}

        return agent_reports

    async def _execute_trades(self, signals: List[TradingSignal],
                              portfolio_rec: PortfolioRecommendation) -> Dict[str, Any]:
        """Execute trading signals (simulated)"""
        self.trade_logger.info(f"Executing {len(signals)} trading signals")

        executed_trades = []
        total_trading_cost = 0.0

        for signal in signals:
            # Validate signal
            if not self.risk_manager.validate_signal(signal, self.current_portfolio):
                self.trade_logger.warning(f"Signal rejected by risk manager: {signal.asset_class}")
                continue

            # Calculate position size
            market_volatility = self._estimate_market_volatility(signal.asset_class)
            position_size = self.risk_manager.size_position(signal, market_volatility)

            # Simulate execution
            execution_price = self._get_execution_price(signal.asset_class)
            trading_cost = position_size * CONFIG.trading.commission_rate
            slippage_cost = position_size * CONFIG.trading.slippage_assumption

            executed_trade = {
                'timestamp': datetime.now().isoformat(),
                'asset_class': signal.asset_class,
                'direction': signal.direction,
                'position_size': position_size,
                'execution_price': execution_price,
                'trading_cost': trading_cost,
                'slippage_cost': slippage_cost,
                'signal_conviction': signal.conviction
            }

            executed_trades.append(executed_trade)
            total_trading_cost += trading_cost + slippage_cost

            self.trade_logger.info(f"Executed: {signal.direction} {position_size:.3f} {signal.asset_class}")

        return {
            'executed_trades': executed_trades,
            'total_trades': len(executed_trades),
            'total_trading_cost': total_trading_cost,
            'rejected_signals': len(signals) - len(executed_trades)
        }

    def _estimate_market_volatility(self, asset_class: str) -> float:
        """Estimate market volatility for asset class"""
        # Simplified volatility estimates
        volatility_map = {
            'equities': 0.16,
            'bonds': 0.08,
            'commodities': 0.25,
            'USD': 0.12,
            'technology': 0.22,
            'energy': 0.30,
            'financials': 0.18
        }

        return volatility_map.get(asset_class, 0.15)

    def _get_execution_price(self, asset_class: str) -> float:
        """Get simulated execution price"""
        # Simplified pricing - in production would use real market data
        base_prices = {
            'equities': 100.0,
            'bonds': 98.5,
            'commodities': 85.2,
            'USD': 1.0,
            'technology': 105.3,
            'energy': 92.1
        }

        base_price = base_prices.get(asset_class, 100.0)
        # Add small random variation
        return base_price * (1 + np.random.normal(0, 0.002))

    def _update_portfolio_state(self, execution_results: Dict,
                                portfolio_rec: PortfolioRecommendation):
        """Update current portfolio state"""
        # Update positions
        for trade in execution_results.get('executed_trades', []):
            asset_class = trade['asset_class']
            position_change = trade['position_size'] * (1 if trade['direction'] == 'long' else -1)

            self.current_portfolio['positions'][asset_class] = (
                    self.current_portfolio['positions'].get(asset_class, 0) + position_change
            )

        # Update sector allocations
        self.current_portfolio['sectors'] = portfolio_rec.sector_weights

        # Update risk budget
        self.current_portfolio['risk_budget'] = portfolio_rec.risk_budget

        # Update last rebalance time
        self.current_portfolio['last_rebalance'] = datetime.now()

        self.logger.info("Portfolio state updated")

    def _signal_to_dict(self, signal: TradingSignal) -> Dict:
        """Convert trading signal to dictionary"""
        return {
            'timestamp': signal.timestamp.isoformat(),
            'asset_class': signal.asset_class,
            'direction': signal.direction,
            'conviction': signal.conviction,
            'position_size': signal.position_size,
            'time_horizon': signal.time_horizon,
            'risk_factors': signal.risk_factors,
            'supporting_agents': signal.supporting_agents,
            'market_regime': signal.market_regime
        }

    def _portfolio_to_dict(self, portfolio: PortfolioRecommendation) -> Dict:
        """Convert portfolio recommendation to dictionary"""
        return {
            'timestamp': portfolio.timestamp.isoformat(),
            'asset_allocation': portfolio.asset_allocation,
            'sector_weights': portfolio.sector_weights,
            'regional_weights': portfolio.regional_weights,
            'risk_budget': portfolio.risk_budget,
            'hedges': portfolio.hedges,
            'cash_allocation': portfolio.cash_allocation,
            'leverage': portfolio.leverage
        }

    async def start_continuous_operation(self):
        """Start continuous hedge fund operation"""
        self.is_running = True
        self.logger.info("Starting continuous hedge fund operation")

        try:
            while self.is_running:
                # Run analysis cycle
                cycle_results = await self.run_analysis_cycle()

                # Save results
                await self._save_cycle_results(cycle_results)

                # Wait for next cycle
                await asyncio.sleep(self.update_interval)

        except KeyboardInterrupt:
            self.logger.info("Received shutdown signal")
        except Exception as e:
            self.logger.error(f"Error in continuous operation: {e}")
        finally:
            self.is_running = False
            self.logger.info("Hedge fund operation stopped")

    async def _save_cycle_results(self, results: Dict):
        """Save cycle results for analysis"""
        timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
        filename = CONFIG.data_dir / f"cycle_results_{timestamp}.json"

        try:
            with open(filename, 'w') as f:
                json.dump(results, f, indent=2, default=str)
        except Exception as e:
            self.logger.error(f"Error saving results: {e}")

    def stop(self):
        """Stop the hedge fund operation"""
        self.is_running = False
        self.logger.info("Stopping hedge fund operation")

    async def get_status(self) -> Dict[str, Any]:
        """Get current system status"""
        return {
            'timestamp': datetime.now().isoformat(),
            'is_running': self.is_running,
            'agents_count': len(self.agents),
            'current_portfolio': self.current_portfolio,
            'last_update': self.current_portfolio.get('last_rebalance', datetime.now()).isoformat(),
            'system_health': await self._check_system_health()
        }

    async def _check_system_health(self) -> Dict[str, str]:
        """Check system health status"""
        health = {}

        # Check data feeds
        try:
            data_summary = self.data_manager.get_latest_data_summary()
            health['data_feeds'] = 'healthy'
        except Exception:
            health['data_feeds'] = 'degraded'

        # Check agent status
        healthy_agents = sum(1 for agent in self.agents.values() if hasattr(agent, 'name'))
        health['agents'] = 'healthy' if healthy_agents == len(self.agents) else 'degraded'

        # Check configuration
        try:
            CONFIG.validate_config()
            health['configuration'] = 'healthy'
        except Exception:
            health['configuration'] = 'error'

        return health


# Signal handlers for graceful shutdown
def signal_handler(orchestrator):
    def handler(signum, frame):
        logging.info(f"Received signal {signum}")
        orchestrator.stop()

    return handler


async def main():
    """Main entry point"""
    # Validate configuration
    try:
        CONFIG.validate_config()
        print("✓ Configuration validated")
    except Exception as e:
        print(f"✗ Configuration error: {e}")
        sys.exit(1)

    # Initialize orchestrator
    orchestrator = HedgeFundOrchestrator()

    # Setup signal handlers
    signal.signal(signal.SIGINT, signal_handler(orchestrator))
    signal.signal(signal.SIGTERM, signal_handler(orchestrator))

    print("\n🚀 AI Hedge Fund System Starting...")
    print("=" * 50)

    # Run single analysis cycle or continuous operation
    if len(sys.argv) > 1 and sys.argv[1] == "--single-cycle":
        print("Running single analysis cycle...")
        results = await orchestrator.run_analysis_cycle()
        print("\n📊 Analysis Results:")
        print(json.dumps(results, indent=2, default=str))
    else:
        print("Starting continuous operation...")
        print("Press Ctrl+C to stop")
        await orchestrator.start_continuous_operation()

    print("\n✅ System shutdown complete")


if __name__ == "__main__":
    asyncio.run(main())