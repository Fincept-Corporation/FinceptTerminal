"""
Hedge Fund Analytics Module
==========================

Comprehensive analysis framework for hedge fund investments across all major strategies including long/short equity, market neutral, event-driven, relative value, and opportunistic approaches. Provides performance measurement, risk assessment, and fee structure analysis.

===== DATA SOURCES REQUIRED =====
INPUT:
  - Historical NAV and performance data for hedge funds
  - Strategy classification and exposure information
  - Gross and net exposure levels
  - Leverage and portfolio composition data
  - Management and performance fee structures
  - Redemption terms and liquidity constraints

OUTPUT:
  - Strategy-specific performance metrics and analytics
  - Risk-adjusted return calculations (Sharpe, Sortino, etc.)
  - Factor exposure analysis and attribution
  - Fee structure impact analysis
  - Liquidity risk assessment
  - Portfolio diversification analysis

PARAMETERS:
  - strategy: Hedge fund strategy type (LONG_SHORT_EQUITY, MARKET_NEUTRAL, etc.)
  - gross_exposure: Total gross exposure percentage
  - net_exposure: Net market exposure percentage
  - leverage: Leverage multiple - default: 1.0
  - management_fee: Annual management fee rate
  - performance_fee: Performance fee rate (typically 20%)
  - hurdle_rate: Performance hurdle rate - default: Config.HF_HURDLE_RATE_DEFAULT
  - high_water_mark: High water mark level - default: 100
  - redemption_frequency: Redemption frequency (daily, weekly, monthly, quarterly, annual)
  - lock_up_period: Lock-up period in months - default: 12
"""

import numpy as np
import pandas as pd
from decimal import Decimal, getcontext
from typing import List, Dict, Optional, Any, Tuple
from datetime import datetime, timedelta
import logging

from config import (
    MarketData, CashFlow, Performance, AssetParameters, AssetClass,
    Constants, Config, HedgeFundStrategy
)
from base_analytics import AlternativeInvestmentBase, FinancialMath

logger = logging.getLogger(__name__)


class HedgeFundAnalyzer(AlternativeInvestmentBase):
    """
    Comprehensive hedge fund analysis across all strategies
    CFA Standards: Performance analysis, risk metrics, factor models
    """

    def __init__(self, parameters: AssetParameters):
        super().__init__(parameters)
        self.strategy = getattr(parameters, 'strategy', HedgeFundStrategy.LONG_SHORT_EQUITY)
        self.gross_exposure = getattr(parameters, 'gross_exposure', None)
        self.net_exposure = getattr(parameters, 'net_exposure', None)
        self.leverage = getattr(parameters, 'leverage', Decimal('1.0'))
        self.high_water_mark = getattr(parameters, 'high_water_mark', Decimal('100'))
        self.hurdle_rate = getattr(parameters, 'hurdle_rate', Config.HF_HURDLE_RATE_DEFAULT)
        self.redemption_frequency = getattr(parameters, 'redemption_frequency', 'quarterly')
        self.lock_up_period = getattr(parameters, 'lock_up_period', 12)  # months

    def calculate_strategy_metrics(self) -> Dict[str, Any]:
        """
        Calculate strategy-specific metrics based on hedge fund type
        CFA Standards: Strategy classification and risk metrics
        """
        metrics = {}
        returns = self.calculate_simple_returns()

        if not returns:
            return {"error": "Insufficient return data"}

        # Base metrics for all strategies
        metrics.update(self._calculate_base_metrics(returns))

        # Strategy-specific analysis
        if self.strategy in [HedgeFundStrategy.LONG_SHORT_EQUITY,
                             HedgeFundStrategy.EQUITY_MARKET_NEUTRAL,
                             HedgeFundStrategy.DEDICATED_SHORT_BIAS]:
            metrics.update(self._analyze_equity_related_strategy(returns))

        elif self.strategy in [HedgeFundStrategy.MERGER_ARBITRAGE,
                               HedgeFundStrategy.DISTRESSED_SECURITIES,
                               HedgeFundStrategy.ACTIVIST]:
            metrics.update(self._analyze_event_driven_strategy(returns))

        elif self.strategy in [HedgeFundStrategy.FIXED_INCOME_ARBITRAGE,
                               HedgeFundStrategy.CONVERTIBLE_ARBITRAGE,
                               HedgeFundStrategy.VOLATILITY_ARBITRAGE]:
            metrics.update(self._analyze_relative_value_strategy(returns))

        elif self.strategy in [HedgeFundStrategy.GLOBAL_MACRO,
                               HedgeFundStrategy.CTA_MANAGED_FUTURES]:
            metrics.update(self._analyze_opportunistic_strategy(returns))

        return metrics

    def _calculate_base_metrics(self, returns: List[Decimal]) -> Dict[str, Any]:
        """Calculate base metrics common to all hedge fund strategies"""
        metrics = {}

        # Performance metrics
        total_return = sum(returns)
        avg_return = total_return / len(returns)
        volatility = self.calculate_volatility(returns, annualized=False)

        metrics['total_return'] = float(total_return)
        metrics['average_return'] = float(avg_return)
        metrics['volatility'] = float(volatility)

        # Risk-adjusted metrics
        sharpe = self.math.sharpe_ratio(returns)
        sortino = self.math.sortino_ratio(returns)

        metrics['sharpe_ratio'] = float(sharpe)
        metrics['sortino_ratio'] = float(sortino)

        # Drawdown analysis
        prices = [md.price for md in self.market_data]
        if prices:
            max_dd, peak_idx, trough_idx = self.math.maximum_drawdown(prices)
            metrics['maximum_drawdown'] = float(max_dd)

        # Exposure metrics
        if self.gross_exposure and self.net_exposure:
            metrics['gross_exposure'] = float(self.gross_exposure)
            metrics['net_exposure'] = float(self.net_exposure)
            metrics['leverage'] = float(self.leverage)

        return metrics

    def _analyze_equity_related_strategy(self, returns: List[Decimal]) -> Dict[str, Any]:
        """Analyze equity-related hedge fund strategies"""
        metrics = {}

        # Market exposure analysis
        if self.strategy == HedgeFundStrategy.LONG_SHORT_EQUITY:
            # Long/short specific metrics
            if self.gross_exposure and self.net_exposure:
                long_exposure = (self.gross_exposure + self.net_exposure) / Decimal('2')
                short_exposure = (self.gross_exposure - self.net_exposure) / Decimal('2')

                metrics['long_exposure'] = float(long_exposure)
                metrics['short_exposure'] = float(short_exposure)
                metrics['long_short_ratio'] = float(long_exposure / short_exposure) if short_exposure != 0 else None

        elif self.strategy == HedgeFundStrategy.EQUITY_MARKET_NEUTRAL:
            # Market neutrality metrics
            metrics['target_beta'] = 0.0
            metrics['expected_market_correlation'] = 'Low'

        elif self.strategy == HedgeFundStrategy.DEDICATED_SHORT_BIAS:
            # Short bias metrics
            metrics['short_bias'] = True
            metrics['expected_market_correlation'] = 'Negative'

        return metrics

    def _analyze_event_driven_strategy(self, returns: List[Decimal]) -> Dict[str, Any]:
        """Analyze event-driven hedge fund strategies"""
        metrics = {}

        # Event-driven characteristics
        if self.strategy == HedgeFundStrategy.MERGER_ARBITRAGE:
            # Merger arbitrage specific
            metrics['return_profile'] = 'steady_positive_with_tail_risk'
            metrics['market_correlation'] = 'low_in_normal_times'

        elif self.strategy == HedgeFundStrategy.DISTRESSED_SECURITIES:
            # Distressed specific
            metrics['return_profile'] = 'illiquid_with_high_returns'
            metrics['credit_sensitivity'] = 'high'

        elif self.strategy == HedgeFundStrategy.ACTIVIST:
            # Activist specific
            metrics['holding_period'] = 'long_term'
            metrics['concentration'] = 'high'

        # Calculate event-driven specific metrics
        win_rate = self._calculate_win_rate(returns)
        metrics['win_rate'] = win_rate

        return metrics

    def _analyze_relative_value_strategy(self, returns: List[Decimal]) -> Dict[str, Any]:
        """Analyze relative value hedge fund strategies"""
        metrics = {}

        if self.strategy == HedgeFundStrategy.FIXED_INCOME_ARBITRAGE:
            # Fixed income arbitrage
            metrics['duration_risk'] = 'managed'
            metrics['credit_risk'] = 'varies'
            metrics['leverage_typical'] = 'high'

        elif self.strategy == HedgeFundStrategy.CONVERTIBLE_ARBITRAGE:
            # Convertible arbitrage
            metrics['delta_hedging'] = True
            metrics['volatility_exposure'] = 'positive'

        elif self.strategy == HedgeFundStrategy.VOLATILITY_ARBITRAGE:
            # Volatility arbitrage
            metrics['volatility_exposure'] = 'primary_driver'
            metrics['gamma_trading'] = True

        # Relative value metrics
        consistency_ratio = self._calculate_consistency_ratio(returns)
        metrics['consistency_ratio'] = consistency_ratio

        return metrics

    def _analyze_opportunistic_strategy(self, returns: List[Decimal]) -> Dict[str, Any]:
        """Analyze opportunistic hedge fund strategies"""
        metrics = {}

        if self.strategy == HedgeFundStrategy.GLOBAL_MACRO:
            # Global macro
            metrics['investment_universe'] = 'global'
            metrics['asset_classes'] = 'multiple'
            metrics['leverage'] = 'variable'

        elif self.strategy == HedgeFundStrategy.CTA_MANAGED_FUTURES:
            # CTA/Managed futures
            metrics['approach'] = 'systematic_trend_following'
            metrics['diversification'] = 'high'
            metrics['crisis_alpha'] = 'potential'

        # Opportunistic metrics
        volatility = self.calculate_volatility(returns)
        metrics['return_volatility'] = float(volatility)

        return metrics

    def _calculate_win_rate(self, returns: List[Decimal]) -> float:
        """Calculate percentage of positive return periods"""
        if not returns:
            return 0.0

        positive_periods = sum(1 for r in returns if r > 0)
        return positive_periods / len(returns)

    def _calculate_consistency_ratio(self, returns: List[Decimal]) -> float:
        """Calculate return consistency (lower volatility relative to mean)"""
        if len(returns) < 2:
            return 0.0

        mean_return = sum(returns) / len(returns)
        volatility = self.calculate_volatility(returns, annualized=False)

        if volatility == 0:
            return float('inf')

        return float(abs(mean_return) / volatility)

    def fee_calculation(self, gross_returns: List[Decimal], nav_values: List[Decimal]) -> Dict[str, Any]:
        """
        Calculate hedge fund fees with high water mark
        CFA Standard: 2 and 20 fee structure with high water mark
        """
        if not all([gross_returns, nav_values, self.parameters.management_fee]):
            return {"error": "Insufficient data for fee calculation"}

        management_fee_rate = self.parameters.management_fee
        performance_fee_rate = self.parameters.performance_fee or Decimal('0.20')

        total_mgmt_fees = Decimal('0')
        total_perf_fees = Decimal('0')
        net_returns = []
        current_hwm = self.high_water_mark

        for i, (gross_ret, nav) in enumerate(zip(gross_returns, nav_values)):
            # Management fee (typically calculated on beginning NAV)
            beginning_nav = nav_values[i - 1] if i > 0 else nav
            mgmt_fee = beginning_nav * management_fee_rate / Constants.MONTHS_IN_YEAR
            total_mgmt_fees += mgmt_fee

            # Performance fee calculation
            perf_fee = Decimal('0')
            nav_after_mgmt_fee = nav - mgmt_fee

            # Check if above high water mark
            if nav_after_mgmt_fee > current_hwm:
                # Performance above hurdle rate
                hurdle_amount = beginning_nav * self.hurdle_rate / Constants.MONTHS_IN_YEAR

                if nav_after_mgmt_fee > (beginning_nav + hurdle_amount):
                    excess_return = nav_after_mgmt_fee - max(current_hwm, beginning_nav + hurdle_amount)
                    perf_fee = excess_return * performance_fee_rate

                    # Update high water mark
                    current_hwm = nav_after_mgmt_fee

            total_perf_fees += perf_fee

            # Net return after fees
            net_nav = nav_after_mgmt_fee - perf_fee
            net_return = (net_nav - beginning_nav) / beginning_nav if beginning_nav > 0 else Decimal('0')
            net_returns.append(net_return)

        return {
            "total_management_fees": float(total_mgmt_fees),
            "total_performance_fees": float(total_perf_fees),
            "total_fees": float(total_mgmt_fees + total_perf_fees),
            "net_returns": [float(r) for r in net_returns],
            "final_high_water_mark": float(current_hwm),
            "fee_drag": float((sum(gross_returns) - sum(net_returns)) / sum(gross_returns)) if sum(
                gross_returns) != 0 else 0
        }

    def factor_model_analysis(self, market_returns: List[Decimal],
                              factor_returns: Dict[str, List[Decimal]] = None) -> Dict[str, Any]:
        """
        Multi-factor model analysis for hedge fund returns
        CFA Standard: Factor model decomposition
        """
        fund_returns = self.calculate_simple_returns()

        if not fund_returns or not market_returns:
            return {"error": "Insufficient return data"}

        if len(fund_returns) != len(market_returns):
            return {"error": "Return series length mismatch"}

        analysis = {}

        # Single factor (market) model
        market_beta = self._calculate_beta(fund_returns, market_returns)
        market_alpha = self._calculate_alpha(fund_returns, market_returns, market_beta)

        analysis['market_beta'] = float(market_beta)
        analysis['market_alpha'] = float(market_alpha)

        # Multi-factor model if additional factors provided
        if factor_returns:
            factor_exposures = self._multi_factor_regression(fund_returns, market_returns, factor_returns)
            analysis['factor_exposures'] = factor_exposures

        # Strategy-specific factor analysis
        strategy_factors = self._get_strategy_factors()
        analysis['relevant_factors'] = strategy_factors

        return analysis

    def _calculate_beta(self, fund_returns: List[Decimal], market_returns: List[Decimal]) -> Decimal:
        """Calculate beta relative to market"""
        if len(fund_returns) != len(market_returns) or len(fund_returns) < 2:
            return Decimal('1')

        # Calculate covariance and market variance
        fund_mean = sum(fund_returns) / len(fund_returns)
        market_mean = sum(market_returns) / len(market_returns)

        covariance = sum((f - fund_mean) * (m - market_mean)
                         for f, m in zip(fund_returns, market_returns)) / (len(fund_returns) - 1)

        market_variance = sum((m - market_mean) ** 2 for m in market_returns) / (len(market_returns) - 1)

        if market_variance == 0:
            return Decimal('1')

        return Decimal(str(covariance)) / Decimal(str(market_variance))

    def _calculate_alpha(self, fund_returns: List[Decimal], market_returns: List[Decimal], beta: Decimal) -> Decimal:
        """Calculate Jensen's alpha"""
        fund_mean = sum(fund_returns) / len(fund_returns)
        market_mean = sum(market_returns) / len(market_returns)
        risk_free = Config.RISK_FREE_RATE / Constants.MONTHS_IN_YEAR

        # Alpha = Fund Return - Risk Free - Beta * (Market Return - Risk Free)
        alpha = fund_mean - risk_free - beta * (market_mean - risk_free)
        return alpha

    def _multi_factor_regression(self, fund_returns: List[Decimal],
                                 market_returns: List[Decimal],
                                 factor_returns: Dict[str, List[Decimal]]) -> Dict[str, float]:
        """Perform multi-factor regression analysis"""
        try:
            # Prepare data for regression
            y = np.array([float(r) for r in fund_returns])

            # Create factor matrix
            factors = [market_returns]
            factor_names = ['market']

            for factor_name, returns in factor_returns.items():
                if len(returns) == len(fund_returns):
                    factors.append(returns)
                    factor_names.append(factor_name)

            X = np.array([[float(factors[j][i]) for j in range(len(factors))]
                          for i in range(len(fund_returns))])

            # Add intercept
            X = np.column_stack([np.ones(len(y)), X])

            # Perform regression
            coefficients = np.linalg.lstsq(X, y, rcond=None)[0]

            # Format results
            exposures = {'alpha': float(coefficients[0])}
            for i, factor_name in enumerate(factor_names):
                exposures[f'{factor_name}_beta'] = float(coefficients[i + 1])

            # Calculate R-squared
            y_pred = X @ coefficients
            ss_res = np.sum((y - y_pred) ** 2)
            ss_tot = np.sum((y - np.mean(y)) ** 2)
            r_squared = 1 - (ss_res / ss_tot) if ss_tot != 0 else 0
            exposures['r_squared'] = float(r_squared)

            return exposures

        except Exception as e:
            logger.error(f"Error in multi-factor regression: {str(e)}")
            return {}

    def _get_strategy_factors(self) -> List[str]:
        """Get relevant factors for each strategy type"""
        strategy_factors = {
            HedgeFundStrategy.LONG_SHORT_EQUITY: ['market', 'size', 'value', 'momentum'],
            HedgeFundStrategy.EQUITY_MARKET_NEUTRAL: ['size', 'value', 'momentum', 'quality'],
            HedgeFundStrategy.MERGER_ARBITRAGE: ['volatility', 'credit_spreads'],
            HedgeFundStrategy.DISTRESSED_SECURITIES: ['credit_spreads', 'high_yield'],
            HedgeFundStrategy.FIXED_INCOME_ARBITRAGE: ['term_structure', 'credit_spreads'],
            HedgeFundStrategy.CONVERTIBLE_ARBITRAGE: ['volatility', 'credit_spreads', 'equity'],
            HedgeFundStrategy.GLOBAL_MACRO: ['currencies', 'commodities', 'bonds', 'equity'],
            HedgeFundStrategy.CTA_MANAGED_FUTURES: ['momentum', 'commodities', 'currencies']
        }

        return strategy_factors.get(self.strategy, ['market'])

    def liquidity_analysis(self) -> Dict[str, Any]:
        """
        Analyze hedge fund liquidity characteristics
        CFA Standard: Liquidity risk assessment
        """
        analysis = {
            "redemption_frequency": self.redemption_frequency,
            "lock_up_period_months": self.lock_up_period,
            "strategy": self.strategy.value
        }

        # Strategy-specific liquidity characteristics
        liquidity_profiles = {
            HedgeFundStrategy.EQUITY_MARKET_NEUTRAL: "high",
            HedgeFundStrategy.LONG_SHORT_EQUITY: "medium_high",
            HedgeFundStrategy.MERGER_ARBITRAGE: "medium",
            HedgeFundStrategy.CONVERTIBLE_ARBITRAGE: "medium",
            HedgeFundStrategy.DISTRESSED_SECURITIES: "low",
            HedgeFundStrategy.ACTIVIST: "low",
            HedgeFundStrategy.GLOBAL_MACRO: "high",
            HedgeFundStrategy.CTA_MANAGED_FUTURES: "high"
        }

        analysis["expected_liquidity"] = liquidity_profiles.get(self.strategy, "medium")

        # Liquidity risk score (1-5, 5 being highest risk)
        risk_scores = {
            "daily": 1,
            "weekly": 2,
            "monthly": 3,
            "quarterly": 4,
            "annual": 5
        }

        analysis["liquidity_risk_score"] = risk_scores.get(self.redemption_frequency, 3)

        return analysis

    def calculate_nav(self) -> Decimal:
        """Calculate hedge fund NAV"""
        latest_price = self.get_latest_price()
        return latest_price or Decimal('100')  # Default to 100 if no price data

    def calculate_key_metrics(self) -> Dict[str, Any]:
        """Calculate comprehensive hedge fund metrics"""
        metrics = {}

        # Strategy-specific metrics
        strategy_metrics = self.calculate_strategy_metrics()
        metrics.update(strategy_metrics)

        # Liquidity analysis
        liquidity_metrics = self.liquidity_analysis()
        metrics.update(liquidity_metrics)

        # Add strategy classification
        metrics['strategy'] = self.strategy.value
        metrics['leverage'] = float(self.leverage)

        return metrics

    def valuation_summary(self) -> Dict[str, Any]:
        """Comprehensive hedge fund analysis summary"""
        return {
            "fund_overview": {
                "strategy": self.strategy.value,
                "management_fee": float(self.parameters.management_fee) if self.parameters.management_fee else None,
                "performance_fee": float(self.parameters.performance_fee) if self.parameters.performance_fee else None,
                "hurdle_rate": float(self.hurdle_rate),
                "high_water_mark": float(self.high_water_mark),
                "leverage": float(self.leverage)
            },
            "performance_analysis": self.calculate_key_metrics(),
            "liquidity_profile": self.liquidity_analysis()
        }


class HedgeFundPortfolio:
    """
    Portfolio-level hedge fund analysis
    CFA Standards: Multi-manager allocation and diversification
    """

    def __init__(self):
        self.hedge_funds: List[HedgeFundAnalyzer] = []

    def add_hedge_fund(self, hedge_fund: HedgeFundAnalyzer) -> None:
        """Add hedge fund to portfolio"""
        self.hedge_funds.append(hedge_fund)

    def strategy_diversification(self) -> Dict[str, Any]:
        """Analyze strategy diversification across portfolio"""
        strategy_allocation = {}
        total_nav = Decimal('0')

        for hf in self.hedge_funds:
            strategy = hf.strategy.value
            nav = hf.calculate_nav()

            if strategy not in strategy_allocation:
                strategy_allocation[strategy] = {
                    'count': 0,
                    'total_nav': Decimal('0')
                }

            strategy_allocation[strategy]['count'] += 1
            strategy_allocation[strategy]['total_nav'] += nav
            total_nav += nav

        # Convert to percentages
        for strategy in strategy_allocation:
            allocation = strategy_allocation[strategy]
            allocation['weight'] = float(allocation['total_nav'] / total_nav) if total_nav > 0 else 0
            allocation['total_nav'] = float(allocation['total_nav'])

        return {
            "strategy_allocation": strategy_allocation,
            "total_portfolio_nav": float(total_nav),
            "number_of_strategies": len(strategy_allocation),
            "number_of_funds": len(self.hedge_funds)
        }

    def portfolio_correlation_analysis(self) -> Dict[str, Any]:
        """Analyze correlations between hedge fund strategies"""
        if len(self.hedge_funds) < 2:
            return {"error": "Need at least 2 hedge funds for correlation analysis"}

        # Get returns for each fund
        fund_returns = {}
        for i, hf in enumerate(self.hedge_funds):
            returns = hf.calculate_simple_returns()
            if returns:
                fund_returns[f"fund_{i}_{hf.strategy.value}"] = returns

        if len(fund_returns) < 2:
            return {"error": "Insufficient return data"}

        # Calculate correlation matrix (simplified)
        correlations = {}
        fund_names = list(fund_returns.keys())

        for i, fund1 in enumerate(fund_names):
            for j, fund2 in enumerate(fund_names[i + 1:], i + 1):
                returns1 = fund_returns[fund1]
                returns2 = fund_returns[fund2]

                if len(returns1) == len(returns2) and len(returns1) > 1:
                    correlation = self._calculate_correlation(returns1, returns2)
                    correlations[f"{fund1}_vs_{fund2}"] = float(correlation)

        return {
            "pairwise_correlations": correlations,
            "funds_analyzed": fund_names
        }

    def _calculate_correlation(self, returns1: List[Decimal], returns2: List[Decimal]) -> Decimal:
        """Calculate correlation between two return series"""
        if len(returns1) != len(returns2) or len(returns1) < 2:
            return Decimal('0')

        mean1 = sum(returns1) / len(returns1)
        mean2 = sum(returns2) / len(returns2)

        numerator = sum((r1 - mean1) * (r2 - mean2) for r1, r2 in zip(returns1, returns2))

        sum_sq1 = sum((r1 - mean1) ** 2 for r1 in returns1)
        sum_sq2 = sum((r2 - mean2) ** 2 for r2 in returns2)

        denominator = (sum_sq1 * sum_sq2).sqrt()

        if denominator == 0:
            return Decimal('0')

        return numerator / denominator


# Export main components
__all__ = ['HedgeFundAnalyzer', 'HedgeFundPortfolio']