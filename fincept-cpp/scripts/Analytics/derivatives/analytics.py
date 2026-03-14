"""
Derivatives Portfolio Analytics Module
===================================

Advanced portfolio analytics and risk metrics framework for derivative portfolios including options, forwards, futures, and swaps. Provides comprehensive risk measurement, scenario analysis, stress testing, and performance attribution capabilities.

===== DATA SOURCES REQUIRED =====
INPUT:
  - Derivative portfolio positions and holdings
  - Market data for underlying assets (spot prices, volatilities, interest rates)
  - Historical price series for risk calculations
  - Option pricing models and parameters
  - Counterparty and market risk factors
  - Benchmark return series for performance comparison

OUTPUT:
  - Portfolio risk metrics (VaR, CVaR, maximum drawdown, volatility)
  - Scenario analysis and stress testing results
  - Sensitivity analysis and Greeks aggregation
  - Performance attribution and P&L decomposition
  - Monte Carlo simulation outcomes
  - Portfolio optimization recommendations

PARAMETERS:
  - confidence_levels: VaR confidence levels - default: [0.95, 0.99]
  - num_simulations: Number of Monte Carlo simulations - default: 10000
  - time_horizon: Analysis time horizon in days - default: 30
  - risk_free_rate: Risk-free rate for calculations - default: 0.02
  - objective: Portfolio optimization objective - default: "sharpe_ratio"
  - constraints: Portfolio optimization constraints
"""

import numpy as np
import pandas as pd
from typing import Dict, List, Tuple, Optional, Union, Any
from datetime import datetime, timedelta
from dataclasses import dataclass, field
from enum import Enum
import logging
from scipy import stats
from scipy.optimize import minimize

from .core import (
    DerivativeInstrument, MarketData, PricingResult,
    ValidationError, ModelValidator, Constants
)
from .options import VanillaOption, OptionGreeks, BlackScholesPricingEngine
from .forward_commitments import ForwardCommitmentPricingEngine

logger = logging.getLogger(__name__)


class RiskMeasure(Enum):
    """Types of risk measures"""
    VALUE_AT_RISK = "var"
    CONDITIONAL_VAR = "cvar"
    EXPECTED_SHORTFALL = "expected_shortfall"
    MAXIMUM_DRAWDOWN = "max_drawdown"
    VOLATILITY = "volatility"
    BETA = "beta"
    SHARPE_RATIO = "sharpe_ratio"


class ScenarioType(Enum):
    """Types of scenario analysis"""
    STRESS_TEST = "stress_test"
    MONTE_CARLO = "monte_carlo"
    HISTORICAL_SIMULATION = "historical_simulation"
    SENSITIVITY_ANALYSIS = "sensitivity_analysis"


@dataclass
class PortfolioPosition:
    """Individual position in derivatives portfolio"""
    instrument: DerivativeInstrument
    quantity: float
    entry_price: float
    entry_date: datetime
    current_value: float = 0.0
    unrealized_pnl: float = 0.0

    def __post_init__(self):
        if self.quantity == 0:
            raise ValidationError("Position quantity cannot be zero")


@dataclass
class RiskMetrics:
    """Container for portfolio risk metrics"""
    portfolio_value: float
    var_95: float = 0.0
    var_99: float = 0.0
    cvar_95: float = 0.0
    cvar_99: float = 0.0
    volatility: float = 0.0
    maximum_drawdown: float = 0.0
    sharpe_ratio: float = 0.0
    sortino_ratio: float = 0.0
    calmar_ratio: float = 0.0
    beta: float = 0.0

    # Greeks aggregation
    total_delta: float = 0.0
    total_gamma: float = 0.0
    total_theta: float = 0.0
    total_vega: float = 0.0
    total_rho: float = 0.0


@dataclass
class ScenarioResult:
    """Results from scenario analysis"""
    scenario_name: str
    scenario_type: ScenarioType
    base_portfolio_value: float
    scenario_portfolio_value: float
    pnl_change: float
    percentage_change: float
    probability: Optional[float] = None
    scenario_parameters: Dict[str, Any] = field(default_factory=dict)


@dataclass
class SensitivityResult:
    """Results from sensitivity analysis"""
    parameter_name: str
    base_value: float
    shock_size: float
    portfolio_value_change: float
    sensitivity: float  # Change per unit of parameter
    elasticity: float  # Percentage change per percentage change in parameter


class DerivativesPortfolio:
    """Derivatives portfolio management and analytics"""

    def __init__(self, portfolio_id: str = None):
        self.portfolio_id = portfolio_id or f"portfolio_{datetime.now().strftime('%Y%m%d_%H%M%S')}"
        self.positions: List[PortfolioPosition] = []
        self.creation_date = datetime.now()
        self.last_update = None

        # Pricing engines
        self.options_engine = BlackScholesPricingEngine()
        self.forwards_engine = ForwardCommitmentPricingEngine()

    def add_position(self, instrument: DerivativeInstrument, quantity: float,
                     entry_price: float, entry_date: datetime = None):
        """Add position to portfolio"""
        if entry_date is None:
            entry_date = datetime.now()

        position = PortfolioPosition(
            instrument=instrument,
            quantity=quantity,
            entry_price=entry_price,
            entry_date=entry_date
        )

        self.positions.append(position)
        self.last_update = datetime.now()
        logger.info(f"Added position: {quantity} units of {instrument}")

    def remove_position(self, position_index: int):
        """Remove position from portfolio"""
        if 0 <= position_index < len(self.positions):
            removed_position = self.positions.pop(position_index)
            self.last_update = datetime.now()
            logger.info(f"Removed position: {removed_position.instrument}")
        else:
            raise ValueError("Invalid position index")

    def update_positions(self, market_data: MarketData) -> float:
        """Update all position values and calculate portfolio value"""
        total_value = 0.0

        for position in self.positions:
            try:
                # Price the instrument
                if hasattr(position.instrument, 'option_type'):  # Options
                    pricing_result = self.options_engine.price(position.instrument, market_data)
                else:  # Forward commitments
                    pricing_result = self.forwards_engine.price(position.instrument, market_data)

                # Update position values
                position.current_value = pricing_result.fair_value * position.quantity
                position.unrealized_pnl = position.current_value - (position.entry_price * position.quantity)

                total_value += position.current_value

            except Exception as e:
                logger.error(f"Error pricing position {position.instrument}: {e}")
                # Use entry price as fallback
                position.current_value = position.entry_price * position.quantity
                total_value += position.current_value

        self.last_update = datetime.now()
        return total_value

    def get_portfolio_greeks(self, market_data: MarketData) -> Dict[str, float]:
        """Calculate aggregated portfolio Greeks"""
        total_greeks = {
            'delta': 0.0, 'gamma': 0.0, 'theta': 0.0,
            'vega': 0.0, 'rho': 0.0
        }

        for position in self.positions:
            if hasattr(position.instrument, 'option_type'):  # Options only
                try:
                    greeks = self.options_engine.calculate_greeks(position.instrument, market_data)

                    total_greeks['delta'] += greeks.delta * position.quantity
                    total_greeks['gamma'] += greeks.gamma * position.quantity
                    total_greeks['theta'] += greeks.theta * position.quantity
                    total_greeks['vega'] += greeks.vega * position.quantity
                    total_greeks['rho'] += greeks.rho * position.quantity

                except Exception as e:
                    logger.warning(f"Could not calculate Greeks for {position.instrument}: {e}")

        return total_greeks

    def get_portfolio_summary(self, market_data: MarketData) -> Dict[str, Any]:
        """Get comprehensive portfolio summary"""
        portfolio_value = self.update_positions(market_data)
        greeks = self.get_portfolio_greeks(market_data)

        total_pnl = sum(pos.unrealized_pnl for pos in self.positions)

        return {
            'portfolio_id': self.portfolio_id,
            'portfolio_value': portfolio_value,
            'total_unrealized_pnl': total_pnl,
            'number_of_positions': len(self.positions),
            'last_update': self.last_update,
            'greeks': greeks,
            'positions_summary': [
                {
                    'instrument_type': pos.instrument.derivative_type.value,
                    'quantity': pos.quantity,
                    'current_value': pos.current_value,
                    'unrealized_pnl': pos.unrealized_pnl,
                    'entry_date': pos.entry_date
                }
                for pos in self.positions
            ]
        }


class RiskAnalyzer:
    """Advanced risk analysis for derivatives portfolios"""

    def __init__(self, confidence_levels: List[float] = None):
        self.confidence_levels = confidence_levels or [0.95, 0.99]
        self.simulation_results = []

    def calculate_var(self, returns: np.ndarray, confidence_level: float = 0.95) -> float:
        """Calculate Value at Risk"""
        if len(returns) == 0:
            return 0.0

        return np.percentile(returns, (1 - confidence_level) * 100)

    def calculate_cvar(self, returns: np.ndarray, confidence_level: float = 0.95) -> float:
        """Calculate Conditional Value at Risk (Expected Shortfall)"""
        if len(returns) == 0:
            return 0.0

        var_threshold = self.calculate_var(returns, confidence_level)
        tail_losses = returns[returns <= var_threshold]

        return np.mean(tail_losses) if len(tail_losses) > 0 else var_threshold

    def calculate_maximum_drawdown(self, portfolio_values: np.ndarray) -> float:
        """Calculate maximum drawdown"""
        if len(portfolio_values) < 2:
            return 0.0

        # Calculate running maximum
        running_max = np.maximum.accumulate(portfolio_values)

        # Calculate drawdowns
        drawdowns = (portfolio_values - running_max) / running_max

        return np.min(drawdowns)

    def calculate_portfolio_risk_metrics(self, portfolio: DerivativesPortfolio,
                                         historical_returns: np.ndarray = None,
                                         benchmark_returns: np.ndarray = None,
                                         risk_free_rate: float = 0.02) -> RiskMetrics:
        """Calculate comprehensive risk metrics"""

        if historical_returns is None:
            # Generate sample returns for demonstration
            historical_returns = np.random.normal(0.001, 0.02, 252)  # Daily returns for 1 year

        portfolio_value = sum(pos.current_value for pos in portfolio.positions)

        # Basic risk metrics
        volatility = np.std(historical_returns) * np.sqrt(252)  # Annualized
        var_95 = self.calculate_var(historical_returns, 0.95)
        var_99 = self.calculate_var(historical_returns, 0.99)
        cvar_95 = self.calculate_cvar(historical_returns, 0.95)
        cvar_99 = self.calculate_cvar(historical_returns, 0.99)

        # Calculate portfolio values for drawdown
        portfolio_values = portfolio_value * (1 + np.cumsum(historical_returns))
        max_drawdown = self.calculate_maximum_drawdown(portfolio_values)

        # Performance ratios
        excess_returns = historical_returns - risk_free_rate / 252
        sharpe_ratio = np.mean(excess_returns) / np.std(historical_returns) * np.sqrt(252) if np.std(
            historical_returns) > 0 else 0

        # Sortino ratio (using downside deviation)
        downside_returns = historical_returns[historical_returns < 0]
        downside_deviation = np.std(downside_returns) if len(downside_returns) > 0 else np.std(historical_returns)
        sortino_ratio = np.mean(excess_returns) / downside_deviation * np.sqrt(252) if downside_deviation > 0 else 0

        # Calmar ratio
        calmar_ratio = np.mean(excess_returns) * 252 / abs(max_drawdown) if max_drawdown != 0 else 0

        # Beta calculation
        beta = 0.0
        if benchmark_returns is not None and len(benchmark_returns) == len(historical_returns):
            covariance = np.cov(historical_returns, benchmark_returns)[0, 1]
            benchmark_variance = np.var(benchmark_returns)
            beta = covariance / benchmark_variance if benchmark_variance > 0 else 0

        return RiskMetrics(
            portfolio_value=portfolio_value,
            var_95=var_95 * portfolio_value,
            var_99=var_99 * portfolio_value,
            cvar_95=cvar_95 * portfolio_value,
            cvar_99=cvar_99 * portfolio_value,
            volatility=volatility,
            maximum_drawdown=max_drawdown,
            sharpe_ratio=sharpe_ratio,
            sortino_ratio=sortino_ratio,
            calmar_ratio=calmar_ratio,
            beta=beta
        )


class ScenarioAnalyzer:
    """Scenario analysis and stress testing"""

    def __init__(self):
        self.scenarios = {}

    def add_scenario(self, name: str, market_shocks: Dict[str, float]):
        """Add predefined scenario"""
        self.scenarios[name] = market_shocks

    def stress_test(self, portfolio: DerivativesPortfolio,
                    base_market_data: MarketData,
                    stress_scenarios: Dict[str, Dict[str, float]] = None) -> List[ScenarioResult]:
        """Perform stress testing on portfolio"""

        if stress_scenarios is None:
            stress_scenarios = self._get_default_stress_scenarios()

        # Calculate base portfolio value
        base_value = portfolio.update_positions(base_market_data)

        results = []

        for scenario_name, shocks in stress_scenarios.items():
            try:
                # Apply shocks to market data
                stressed_market_data = self._apply_market_shocks(base_market_data, shocks)

                # Calculate portfolio value under stress
                stressed_value = portfolio.update_positions(stressed_market_data)

                pnl_change = stressed_value - base_value
                percentage_change = (pnl_change / base_value * 100) if base_value != 0 else 0

                result = ScenarioResult(
                    scenario_name=scenario_name,
                    scenario_type=ScenarioType.STRESS_TEST,
                    base_portfolio_value=base_value,
                    scenario_portfolio_value=stressed_value,
                    pnl_change=pnl_change,
                    percentage_change=percentage_change,
                    scenario_parameters=shocks
                )

                results.append(result)

            except Exception as e:
                logger.error(f"Error in stress test scenario {scenario_name}: {e}")

        return results

    def monte_carlo_simulation(self, portfolio: DerivativesPortfolio,
                               base_market_data: MarketData,
                               num_simulations: int = 10000,
                               time_horizon: int = 30) -> List[ScenarioResult]:
        """Monte Carlo simulation for portfolio"""

        base_value = portfolio.update_positions(base_market_data)
        results = []

        # Simulation parameters
        dt = 1 / 252  # Daily time step
        drift = 0.0001  # Small positive drift
        volatility = 0.02  # 2% daily volatility

        for i in range(num_simulations):
            try:
                # Generate random price path
                random_shocks = np.random.normal(0, 1, time_horizon)

                # Simulate final market conditions
                final_spot_multiplier = np.exp(np.sum(
                    (drift - 0.5 * volatility ** 2) * dt + volatility * np.sqrt(dt) * random_shocks
                ))

                simulated_market_data = MarketData(
                    spot_price=base_market_data.spot_price * final_spot_multiplier,
                    risk_free_rate=base_market_data.risk_free_rate,
                    dividend_yield=base_market_data.dividend_yield,
                    volatility=base_market_data.volatility,
                    time_to_expiry=max(0, base_market_data.time_to_expiry - time_horizon / 252)
                )

                simulated_value = portfolio.update_positions(simulated_market_data)
                pnl_change = simulated_value - base_value
                percentage_change = (pnl_change / base_value * 100) if base_value != 0 else 0

                result = ScenarioResult(
                    scenario_name=f"MC_Simulation_{i + 1}",
                    scenario_type=ScenarioType.MONTE_CARLO,
                    base_portfolio_value=base_value,
                    scenario_portfolio_value=simulated_value,
                    pnl_change=pnl_change,
                    percentage_change=percentage_change,
                    probability=1 / num_simulations,
                    scenario_parameters={
                        "final_spot_multiplier": final_spot_multiplier,
                        "time_horizon_days": time_horizon
                    }
                )

                results.append(result)

            except Exception as e:
                logger.warning(f"Error in MC simulation {i + 1}: {e}")

        return results

    def sensitivity_analysis(self, portfolio: DerivativesPortfolio,
                             base_market_data: MarketData,
                             parameters: List[str] = None,
                             shock_sizes: Dict[str, float] = None) -> List[SensitivityResult]:
        """Perform sensitivity analysis on key parameters"""

        if parameters is None:
            parameters = ['spot_price', 'volatility', 'risk_free_rate', 'time_to_expiry']

        if shock_sizes is None:
            shock_sizes = {
                'spot_price': 0.01,  # 1%
                'volatility': 0.01,  # 1 percentage point
                'risk_free_rate': 0.0025,  # 25 basis points
                'time_to_expiry': -1 / 365  # 1 day
            }

        base_value = portfolio.update_positions(base_market_data)
        results = []

        for param in parameters:
            if param not in shock_sizes:
                continue

            try:
                # Create shocked market data
                shocked_data = self._shock_parameter(base_market_data, param, shock_sizes[param])

                # Calculate portfolio value with shock
                shocked_value = portfolio.update_positions(shocked_data)

                # Calculate sensitivity metrics
                pnl_change = shocked_value - base_value
                base_param_value = getattr(base_market_data, param)

                # Linear sensitivity (change per unit)
                sensitivity = pnl_change / shock_sizes[param] if shock_sizes[param] != 0 else 0

                # Elasticity (percentage change per percentage change)
                if base_param_value != 0 and base_value != 0:
                    elasticity = (pnl_change / base_value) / (shock_sizes[param] / base_param_value)
                else:
                    elasticity = 0

                result = SensitivityResult(
                    parameter_name=param,
                    base_value=base_param_value,
                    shock_size=shock_sizes[param],
                    portfolio_value_change=pnl_change,
                    sensitivity=sensitivity,
                    elasticity=elasticity
                )

                results.append(result)

            except Exception as e:
                logger.error(f"Error in sensitivity analysis for {param}: {e}")

        return results

    def _get_default_stress_scenarios(self) -> Dict[str, Dict[str, float]]:
        """Get default stress test scenarios"""
        return {
            "Market_Crash": {
                "spot_price": -0.20,  # 20% stock drop
                "volatility": 0.15,  # Vol spike to 15%
                "risk_free_rate": -0.01  # Rate cut
            },
            "Vol_Spike": {
                "volatility": 0.20,  # Extreme volatility
                "spot_price": -0.05  # 5% stock drop
            },
            "Interest_Rate_Shock": {
                "risk_free_rate": 0.02,  # 200 bps rate increase
                "spot_price": -0.10  # 10% stock drop
            },
            "Time_Decay": {
                "time_to_expiry": -0.1,  # 10% time decay
                "volatility": -0.05  # Vol compression
            },
            "Bull_Market": {
                "spot_price": 0.15,  # 15% stock rally
                "volatility": -0.05  # Vol compression
            }
        }

    def _apply_market_shocks(self, base_data: MarketData, shocks: Dict[str, float]) -> MarketData:
        """Apply market shocks to create stressed market data"""
        return MarketData(
            spot_price=base_data.spot_price * (1 + shocks.get('spot_price', 0)),
            risk_free_rate=base_data.risk_free_rate + shocks.get('risk_free_rate', 0),
            dividend_yield=base_data.dividend_yield + shocks.get('dividend_yield', 0),
            volatility=max(0.001, base_data.volatility + shocks.get('volatility', 0)),
            time_to_expiry=max(0, base_data.time_to_expiry + shocks.get('time_to_expiry', 0))
        )

    def _shock_parameter(self, base_data: MarketData, parameter: str, shock: float) -> MarketData:
        """Apply shock to specific parameter"""
        data_dict = {
            'spot_price': base_data.spot_price,
            'risk_free_rate': base_data.risk_free_rate,
            'dividend_yield': base_data.dividend_yield,
            'volatility': base_data.volatility,
            'time_to_expiry': base_data.time_to_expiry
        }

        if parameter in data_dict:
            if parameter == 'spot_price':
                data_dict[parameter] *= (1 + shock)  # Percentage shock
            else:
                data_dict[parameter] += shock  # Absolute shock

        return MarketData(**data_dict)


class PerformanceAttribution:
    """Performance attribution analysis"""

    def __init__(self):
        self.attribution_factors = ['delta', 'gamma', 'theta', 'vega', 'rho']

    def attribute_pnl(self, portfolio: DerivativesPortfolio,
                      initial_market_data: MarketData,
                      final_market_data: MarketData) -> Dict[str, float]:
        """Attribute P&L to different risk factors"""

        # Calculate initial portfolio value and Greeks
        initial_value = portfolio.update_positions(initial_market_data)
        initial_greeks = portfolio.get_portfolio_greeks(initial_market_data)

        # Calculate final portfolio value
        final_value = portfolio.update_positions(final_market_data)
        total_pnl = final_value - initial_value

        # Calculate market moves
        spot_move = final_market_data.spot_price - initial_market_data.spot_price
        vol_move = final_market_data.volatility - initial_market_data.volatility
        rate_move = final_market_data.risk_free_rate - initial_market_data.risk_free_rate
        time_move = final_market_data.time_to_expiry - initial_market_data.time_to_expiry

        # Attribute P&L to Greeks
        delta_pnl = initial_greeks.get('delta', 0) * spot_move
        gamma_pnl = 0.5 * initial_greeks.get('gamma', 0) * (spot_move ** 2)
        theta_pnl = initial_greeks.get('theta', 0) * (-time_move * 365)  # Convert to days
        vega_pnl = initial_greeks.get('vega', 0) * vol_move * 100  # Vega per 1% vol
        rho_pnl = initial_greeks.get('rho', 0) * rate_move * 100  # Rho per 1% rate

        # Calculate unexplained P&L
        explained_pnl = delta_pnl + gamma_pnl + theta_pnl + vega_pnl + rho_pnl
        unexplained_pnl = total_pnl - explained_pnl

        return {
            'total_pnl': total_pnl,
            'delta_pnl': delta_pnl,
            'gamma_pnl': gamma_pnl,
            'theta_pnl': theta_pnl,
            'vega_pnl': vega_pnl,
            'rho_pnl': rho_pnl,
            'explained_pnl': explained_pnl,
            'unexplained_pnl': unexplained_pnl,
            'explanation_ratio': abs(explained_pnl / total_pnl) if total_pnl != 0 else 0
        }


class PortfolioOptimizer:
    """Portfolio optimization for derivatives"""

    def __init__(self, objective: str = "sharpe_ratio"):
        self.objective = objective  # "sharpe_ratio", "min_variance", "max_return"

    def optimize_weights(self, expected_returns: np.ndarray,
                         covariance_matrix: np.ndarray,
                         constraints: Dict = None) -> Dict[str, Any]:
        """Optimize portfolio weights"""

        n_assets = len(expected_returns)

        # Default constraints
        if constraints is None:
            constraints = {
                'max_weight': 0.4,  # Max 40% in any single asset
                'min_weight': -0.2,  # Allow 20% short positions
                'target_return': None
            }

        # Objective function
        def objective_function(weights):
            portfolio_return = np.dot(weights, expected_returns)
            portfolio_variance = np.dot(weights.T, np.dot(covariance_matrix, weights))
            portfolio_std = np.sqrt(portfolio_variance)

            if self.objective == "sharpe_ratio":
                return -portfolio_return / portfolio_std if portfolio_std > 0 else 0
            elif self.objective == "min_variance":
                return portfolio_variance
            elif self.objective == "max_return":
                return -portfolio_return
            else:
                return portfolio_variance

        # Constraints
        constraint_list = [
            {'type': 'eq', 'fun': lambda w: np.sum(w) - 1}  # Weights sum to 1
        ]

        if constraints.get('target_return'):
            constraint_list.append({
                'type': 'eq',
                'fun': lambda w: np.dot(w, expected_returns) - constraints['target_return']
            })

        # Bounds for individual weights
        bounds = [
            (constraints.get('min_weight', -1), constraints.get('max_weight', 1))
            for _ in range(n_assets)
        ]

        # Initial guess (equal weights)
        initial_weights = np.array([1 / n_assets] * n_assets)

        try:
            result = minimize(
                objective_function,
                initial_weights,
                method='SLSQP',
                bounds=bounds,
                constraints=constraint_list
            )

            if result.success:
                optimal_weights = result.x
                optimal_return = np.dot(optimal_weights, expected_returns)
                optimal_variance = np.dot(optimal_weights.T, np.dot(covariance_matrix, optimal_weights))
                optimal_std = np.sqrt(optimal_variance)
                sharpe_ratio = optimal_return / optimal_std if optimal_std > 0 else 0

                return {
                    'success': True,
                    'optimal_weights': optimal_weights,
                    'expected_return': optimal_return,
                    'volatility': optimal_std,
                    'sharpe_ratio': sharpe_ratio,
                    'optimization_result': result
                }
            else:
                return {'success': False, 'message': result.message}

        except Exception as e:
            logger.error(f"Portfolio optimization failed: {e}")
            return {'success': False, 'message': str(e)}


# Export main classes
__all__ = [
    'RiskMeasure', 'ScenarioType', 'PortfolioPosition', 'RiskMetrics',
    'ScenarioResult', 'SensitivityResult', 'DerivativesPortfolio',
    'RiskAnalyzer', 'ScenarioAnalyzer', 'PerformanceAttribution', 'PortfolioOptimizer'
]