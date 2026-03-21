"""
skfolio Risk Analysis & Management
=================================

This module provides comprehensive risk analysis and management capabilities
for portfolio optimization. It includes all major risk measures, stress testing,
scenario analysis, and advanced Monte Carlo simulation methods.

Key Features:
- Complete suite of risk measures (VaR, CVaR, drawdown, etc.)
- Risk attribution and decomposition
- Stress testing and scenario analysis
- Monte Carlo simulation with copulas
- Risk budgeting and constraints
- Portfolio risk monitoring
- Coherent risk measures
- Dynamic risk analysis

Usage:
    from skfolio_risk import RiskAnalyzer

    analyzer = RiskAnalyzer()
    risk_metrics = analyzer.calculate_risk_metrics(portfolio_returns)
    stress_results = analyzer.stress_test(portfolio_weights, scenarios)
"""

import numpy as np
import pandas as pd
import warnings
from typing import Dict, List, Optional, Union, Tuple, Any, Callable
from dataclasses import dataclass, field
from datetime import datetime, timedelta
from scipy import stats
from scipy.optimize import minimize
import json
import logging

# skfolio imports
from skfolio import RiskMeasure
from skfolio.distribution import VineCopula
from skfolio.portfolio import Portfolio

warnings.filterwarnings('ignore')
logger = logging.getLogger(__name__)

@dataclass
class RiskMetrics:
    """Container for risk metrics"""

    # Traditional Risk Measures
    volatility: float
    variance: float
    semivariance: float
    mean_absolute_deviation: float

    # Downside Risk Measures
    var_95: float  # Value at Risk at 95% confidence
    var_99: float  # Value at Risk at 99% confidence
    cvar_95: float  # Conditional Value at Risk at 95% confidence
    cvar_99: float  # Conditional Value at Risk at 99% confidence
    evar: float  # Entropic Value at Risk
    worst_realization: float

    # Drawdown Measures
    max_drawdown: float
    average_drawdown: float
    cdar_95: float  # Conditional Drawdown at Risk
    ulcer_index: float
    pain_index: float
    calmar_ratio: float

    # Higher Moments
    skewness: float
    kurtosis: float
    fourth_central_moment: float
    fourth_lower_partial_moment: float

    # Tail Risk Measures
    tail_ratio: float
    expected_shortfall: float
    conditional_tail_expectation: float

    # Coherent Risk Measures
    gini_mean_difference: float
    entropic_risk_measure: float

    # Risk-Adjusted Performance
    sharpe_ratio: float
    sortino_ratio: float
    treynor_ratio: float
    information_ratio: float

    # Additional Metrics
    tracking_error: Optional[float] = None
    beta: Optional[float] = None
    alpha: Optional[float] = None
    r_squared: Optional[float] = None

    def to_dict(self) -> Dict:
        """Convert to dictionary"""
        return {
            "traditional_risk": {
                "volatility": self.volatility,
                "variance": self.variance,
                "semivariance": self.semivariance,
                "mean_absolute_deviation": self.mean_absolute_deviation
            },
            "downside_risk": {
                "var_95": self.var_95,
                "var_99": self.var_99,
                "cvar_95": self.cvar_95,
                "cvar_99": self.cvar_99,
                "evar": self.evar,
                "worst_realization": self.worst_realization
            },
            "drawdown_risk": {
                "max_drawdown": self.max_drawdown,
                "average_drawdown": self.average_drawdown,
                "cdar_95": self.cdar_95,
                "ulcer_index": self.ulcer_index,
                "pain_index": self.pain_index
            },
            "higher_moments": {
                "skewness": self.skewness,
                "kurtosis": self.kurtosis,
                "fourth_central_moment": self.fourth_central_moment,
                "fourth_lower_partial_moment": self.fourth_lower_partial_moment
            },
            "tail_risk": {
                "tail_ratio": self.tail_ratio,
                "expected_shortfall": self.expected_shortfall,
                "conditional_tail_expectation": self.conditional_tail_expectation
            },
            "coherent_risk": {
                "gini_mean_difference": self.gini_mean_difference,
                "entropic_risk_measure": self.entropic_risk_measure
            },
            "risk_adjusted_performance": {
                "sharpe_ratio": self.sharpe_ratio,
                "sortino_ratio": self.sortino_ratio,
                "treynor_ratio": self.treynor_ratio,
                "information_ratio": self.information_ratio
            },
            "market_measures": {
                "tracking_error": self.tracking_error,
                "beta": self.beta,
                "alpha": self.alpha,
                "r_squared": self.r_squared
            }
        }

@dataclass
class StressTestScenario:
    """Stress test scenario definition"""

    name: str
    description: str
    shocks: Dict[str, float]  # Asset name -> shock percentage
    correlation_changes: Optional[Dict[Tuple[str, str], float]] = None
    volatility_changes: Optional[Dict[str, float]] = None
    probability: float = 1.0  # Scenario probability

    def to_dict(self) -> Dict:
        """Convert to dictionary"""
        return {
            "name": self.name,
            "description": self.description,
            "shocks": self.shocks,
            "correlation_changes": self.correlation_changes,
            "volatility_changes": self.volatility_changes,
            "probability": self.probability
        }

@dataclass
class RiskBudget:
    """Risk budget allocation"""

    total_risk_budget: float
    asset_risk_contributions: Dict[str, float]
    risk_budget_allocations: Dict[str, float]
    risk_utilization: Dict[str, float]  # Actual budget usage
    marginal_risk_contributions: Dict[str, float]
    component_risk_contributions: Dict[str, float]

    def to_dict(self) -> Dict:
        """Convert to dictionary"""
        return {
            "total_risk_budget": self.total_risk_budget,
            "asset_risk_contributions": self.asset_risk_contributions,
            "risk_budget_allocations": self.risk_budget_allocations,
            "risk_utilization": self.risk_utilization,
            "marginal_risk_contributions": self.marginal_risk_contributions,
            "component_risk_contributions": self.component_risk_contributions
        }

class RiskAnalyzer:
    """
    Comprehensive risk analysis and management system

    Provides advanced risk measurement, stress testing, and risk budgeting
    capabilities for portfolio optimization workflows.
    """

    def __init__(self, risk_free_rate: float = 0.02):
        """
        Initialize risk analyzer

        Parameters:
        -----------
        risk_free_rate : float
            Risk-free rate for risk-adjusted metrics
        """

        self.risk_free_rate = risk_free_rate
        self.risk_measure_cache = {}

        # Risk measure mappings
        self.risk_measures = {
            "variance": RiskMeasure.VARIANCE,
            "semi_variance": RiskMeasure.SEMI_VARIANCE,
            "cvar": RiskMeasure.CVAR,
            "evar": RiskMeasure.EVAR,
            "max_drawdown": RiskMeasure.MAX_DRAWDOWN,
            "cdar": RiskMeasure.CDAR,
            "ulcer_index": RiskMeasure.ULCER_INDEX,
            "mean_absolute_deviation": RiskMeasure.MEAN_ABSOLUTE_DEVIATION,
            "worst_realization": RiskMeasure.WORST_REALIZATION,
            "standard_deviation": RiskMeasure.STANDARD_DEVIATION,
            "semi_deviation": RiskMeasure.SEMI_DEVIATION,
            "annualized_variance": RiskMeasure.ANNUALIZED_VARIANCE,
            "annualized_standard_deviation": RiskMeasure.ANNUALIZED_STANDARD_DEVIATION,
            "annualized_semi_variance": RiskMeasure.ANNUALIZED_SEMI_VARIANCE,
            "annualized_semi_deviation": RiskMeasure.ANNUALIZED_SEMI_DEVIATION,
            "average_drawdown": RiskMeasure.AVERAGE_DRAWDOWN,
            "edar": RiskMeasure.EDAR,
            "gini_mean_difference": RiskMeasure.GINI_MEAN_DIFFERENCE,
            "first_lower_partial_moment": RiskMeasure.FIRST_LOWER_PARTIAL_MOMENT,
            "value_at_risk": None  # Not available in RiskMeasure enum
        }

        logger.info("RiskAnalyzer initialized")

    def calculate_risk_metrics(self,
                             returns: pd.Series,
                             benchmark_returns: Optional[pd.Series] = None,
                             confidence_levels: List[float] = [0.95, 0.99]) -> RiskMetrics:
        """
        Calculate comprehensive risk metrics

        Parameters:
        -----------
        returns : pd.Series
            Portfolio returns
        benchmark_returns : pd.Series, optional
            Benchmark returns for market measures
        confidence_levels : List[float]
            Confidence levels for VaR/CVaR

        Returns:
        --------
        RiskMetrics object
        """

        try:
            # Basic statistics
            mean_return = returns.mean()
            std_return = returns.std()

            # Traditional Risk Measures
            variance = returns.var()
            semivariance = returns[returns < mean_return].var()
            mean_absolute_deviation = (returns - mean_return).abs().mean()

            # Downside Risk Measures
            var_95 = self._calculate_var(returns, 0.95)
            var_99 = self._calculate_var(returns, 0.99)
            cvar_95 = self._calculate_cvar(returns, 0.95)
            cvar_99 = self._calculate_cvar(returns, 0.99)
            evar = self._calculate_evar(returns)
            worst_realization = returns.min()

            # Drawdown Measures
            drawdowns = self._calculate_drawdowns(returns)
            max_drawdown = drawdowns.min()
            average_drawdown = drawdowns[drawdowns < 0].mean()
            cdar_95 = self._calculate_cdar(returns, 0.95)
            ulcer_index = self._calculate_ulcer_index(returns)
            pain_index = self._calculate_pain_index(returns)

            # Calmar Ratio
            annual_return = returns.mean() * 252
            calmar_ratio = annual_return / abs(max_drawdown) if max_drawdown < 0 else 0

            # Higher Moments
            skewness = returns.skew()
            kurtosis = returns.kurtosis()
            fourth_central_moment = self._calculate_fourth_central_moment(returns)
            fourth_lower_partial_moment = self._calculate_fourth_lower_partial_moment(returns)

            # Tail Risk Measures
            tail_ratio = self._calculate_tail_ratio(returns)
            expected_shortfall = cvar_95  # Using CVaR as expected shortfall
            conditional_tail_expectation = self._calculate_conditional_tail_expectation(returns, 0.05)

            # Coherent Risk Measures
            gini_mean_difference = self._calculate_gini_mean_difference(returns)
            entropic_risk_measure = self._calculate_entropic_risk_measure(returns)

            # Risk-Adjusted Performance
            sharpe_ratio = self._calculate_sharpe_ratio(returns)
            sortino_ratio = self._calculate_sortino_ratio(returns)
            treynor_ratio = None  # Requires beta
            information_ratio = None  # Requires benchmark

            # Market Measures (if benchmark provided)
            tracking_error = beta = alpha = r_squared = None
            if benchmark_returns is not None:
                tracking_error = self._calculate_tracking_error(returns, benchmark_returns)
                beta = self._calculate_beta(returns, benchmark_returns)
                alpha = self._calculate_alpha(returns, benchmark_returns, beta)
                r_squared = self._calculate_r_squared(returns, benchmark_returns)
                treynor_ratio = self._calculate_treynor_ratio(returns, beta)
                information_ratio = self._calculate_information_ratio(returns, benchmark_returns)

            return RiskMetrics(
                volatility=std_return * np.sqrt(252),  # Annualized
                variance=variance * 252,  # Annualized
                semivariance=semivariance * 252,  # Annualized
                mean_absolute_deviation=mean_absolute_deviation * np.sqrt(252),
                var_95=var_95,
                var_99=var_99,
                cvar_95=cvar_95,
                cvar_99=cvar_99,
                evar=evar,
                worst_realization=worst_realization,
                max_drawdown=max_drawdown,
                average_drawdown=average_drawdown,
                cdar_95=cdar_95,
                ulcer_index=ulcer_index,
                pain_index=pain_index,
                calmar_ratio=calmar_ratio,
                skewness=skewness,
                kurtosis=kurtosis,
                fourth_central_moment=fourth_central_moment,
                fourth_lower_partial_moment=fourth_lower_partial_moment,
                tail_ratio=tail_ratio,
                expected_shortfall=expected_shortfall,
                conditional_tail_expectation=conditional_tail_expectation,
                gini_mean_difference=gini_mean_difference,
                entropic_risk_measure=entropic_risk_measure,
                sharpe_ratio=sharpe_ratio,
                sortino_ratio=sortino_ratio,
                treynor_ratio=treynor_ratio,
                information_ratio=information_ratio,
                tracking_error=tracking_error,
                beta=beta,
                alpha=alpha,
                r_squared=r_squared
            )

        except Exception as e:
            logger.error(f"Error calculating risk metrics: {e}")
            raise

    def _calculate_var(self, returns: pd.Series, confidence_level: float) -> float:
        """Calculate Value at Risk"""
        return np.percentile(returns, (1 - confidence_level) * 100)

    def _calculate_cvar(self, returns: pd.Series, confidence_level: float) -> float:
        """Calculate Conditional Value at Risk"""
        var = self._calculate_var(returns, confidence_level)
        return returns[returns <= var].mean()

    def _calculate_evar(self, returns: pd.Series, risk_aversion: float = 1.0) -> float:
        """Calculate Entropic Value at Risk"""
        return (1 / risk_aversion) * np.log(np.mean(np.exp(-risk_aversion * returns)))

    def _calculate_drawdowns(self, returns: pd.Series) -> pd.Series:
        """Calculate drawdown series"""
        cumulative = (1 + returns).cumprod()
        running_max = cumulative.expanding().max()
        return (cumulative - running_max) / running_max

    def _calculate_cdar(self, returns: pd.Series, confidence_level: float) -> float:
        """Calculate Conditional Drawdown at Risk"""
        drawdowns = self._calculate_drawdowns(returns)
        return np.percentile(drawdowns, (1 - confidence_level) * 100)

    def _calculate_ulcer_index(self, returns: pd.Series) -> float:
        """Calculate Ulcer Index"""
        drawdowns = self._calculate_drawdowns(returns)
        return np.sqrt((drawdowns ** 2).sum() / len(drawdowns))

    def _calculate_pain_index(self, returns: pd.Series) -> float:
        """Calculate Pain Index"""
        drawdowns = self._calculate_drawdowns(returns)
        return drawdowns[drawdowns < 0].sum() / len(returns)

    def _calculate_fourth_central_moment(self, returns: pd.Series) -> float:
        """Calculate fourth central moment"""
        mean_return = returns.mean()
        return ((returns - mean_return) ** 4).mean()

    def _calculate_fourth_lower_partial_moment(self, returns: pd.Series, threshold: float = 0) -> float:
        """Calculate fourth lower partial moment"""
        below_threshold = returns[returns < threshold]
        if len(below_threshold) == 0:
            return 0
        return ((threshold - below_threshold) ** 4).mean()

    def _calculate_tail_ratio(self, returns: pd.Series, tail_pct: float = 0.05) -> float:
        """Calculate tail ratio"""
        lower_tail = np.percentile(returns, tail_pct * 100)
        upper_tail = np.percentile(returns, (1 - tail_pct) * 100)
        return abs(upper_tail / lower_tail) if lower_tail != 0 else float('inf')

    def _calculate_conditional_tail_expectation(self, returns: pd.Series, tail_pct: float) -> float:
        """Calculate conditional tail expectation"""
        var = np.percentile(returns, tail_pct * 100)
        tail_returns = returns[returns <= var]
        return tail_returns.mean() if len(tail_returns) > 0 else 0

    def _calculate_gini_mean_difference(self, returns: pd.Series) -> float:
        """Calculate Gini mean difference"""
        n = len(returns)
        if n == 0:
            return 0
        return (2 / (n ** 2)) * sum(abs(returns.iloc[i] - returns.iloc[j])
                                    for i in range(n) for j in range(n))

    def _calculate_entropic_risk_measure(self, returns: pd.Series, theta: float = 1.0) -> float:
        """Calculate entropic risk measure"""
        return theta * np.log(np.mean(np.exp(-returns / theta)))

    def _calculate_sharpe_ratio(self, returns: pd.Series) -> float:
        """Calculate Sharpe ratio"""
        annual_return = returns.mean() * 252
        annual_vol = returns.std() * np.sqrt(252)
        return (annual_return - self.risk_free_rate) / annual_vol if annual_vol > 0 else 0

    def _calculate_sortino_ratio(self, returns: pd.Series) -> float:
        """Calculate Sortino ratio"""
        annual_return = returns.mean() * 252
        downside_std = returns[returns < 0].std() * np.sqrt(252)
        return (annual_return - self.risk_free_rate) / downside_std if downside_std > 0 else 0

    def _calculate_treynor_ratio(self, returns: pd.Series, beta: float) -> float:
        """Calculate Treynor ratio"""
        annual_return = returns.mean() * 252
        return (annual_return - self.risk_free_rate) / beta if beta != 0 else 0

    def _calculate_information_ratio(self, returns: pd.Series, benchmark_returns: pd.Series) -> float:
        """Calculate Information ratio"""
        excess_returns = returns - benchmark_returns
        return excess_returns.mean() / excess_returns.std() if excess_returns.std() > 0 else 0

    def _calculate_tracking_error(self, returns: pd.Series, benchmark_returns: pd.Series) -> float:
        """Calculate tracking error"""
        excess_returns = returns - benchmark_returns
        return excess_returns.std() * np.sqrt(252)

    def _calculate_beta(self, returns: pd.Series, benchmark_returns: pd.Series) -> float:
        """Calculate beta"""
        if len(returns) != len(benchmark_returns):
            # Align data
            common_index = returns.index.intersection(benchmark_returns.index)
            returns = returns.loc[common_index]
            benchmark_returns = benchmark_returns.loc[common_index]

        covariance = np.cov(returns, benchmark_returns)[0, 1]
        benchmark_variance = benchmark_returns.var()
        return covariance / benchmark_variance if benchmark_variance > 0 else 0

    def _calculate_alpha(self, returns: pd.Series, benchmark_returns: pd.Series, beta: float) -> float:
        """Calculate alpha"""
        if len(returns) != len(benchmark_returns):
            common_index = returns.index.intersection(benchmark_returns.index)
            returns = returns.loc[common_index]
            benchmark_returns = benchmark_returns.loc[common_index]

        portfolio_return = returns.mean() * 252
        benchmark_return = benchmark_returns.mean() * 252
        return portfolio_return - (self.risk_free_rate + beta * (benchmark_return - self.risk_free_rate))

    def _calculate_r_squared(self, returns: pd.Series, benchmark_returns: pd.Series) -> float:
        """Calculate R-squared"""
        if len(returns) != len(benchmark_returns):
            common_index = returns.index.intersection(benchmark_returns.index)
            returns = returns.loc[common_index]
            benchmark_returns = benchmark_returns.loc[common_index]

        correlation = np.corrcoef(returns, benchmark_returns)[0, 1]
        return correlation ** 2 if not np.isnan(correlation) else 0

    def risk_attribution(self,
                        weights: np.ndarray,
                        asset_returns: pd.DataFrame,
                        cov_matrix: Optional[pd.DataFrame] = None) -> Dict:
        """
        Perform risk attribution analysis

        Parameters:
        -----------
        weights : np.ndarray
            Portfolio weights
        asset_returns : pd.DataFrame
            Asset returns data
        cov_matrix : pd.DataFrame, optional
            Covariance matrix

        Returns:
        --------
        Dict with risk attribution results
        """

        try:
            assets = asset_returns.columns.tolist()

            if cov_matrix is None:
                cov_matrix = asset_returns.cov() * 252  # Annualized

            # Portfolio risk
            portfolio_variance = weights.T @ cov_matrix @ weights
            portfolio_volatility = np.sqrt(portfolio_variance)

            # Marginal contribution to risk
            marginal_contrib = (cov_matrix @ weights) / portfolio_volatility

            # Component contribution to risk
            component_contrib = weights * marginal_contrib

            # Percentage contribution to risk
            pct_contrib = component_contrib / portfolio_volatility * 100

            # Risk attribution DataFrame
            risk_attrib_df = pd.DataFrame({
                'Asset': assets,
                'Weight': weights,
                'Weight_Pct': weights * 100,
                'Marginal_Risk': marginal_contrib,
                'Component_Risk': component_contrib,
                'Risk_Contribution_Pct': pct_contrib,
                'Individual_Vol': np.sqrt(np.diag(cov_matrix)) * 100
            }).sort_values('Risk_Contribution_Pct', key=abs, ascending=False)

            # Concentration measures
            concentration_ratio = (weights ** 2).sum()
            effective_n_assets = 1 / concentration_ratio if concentration_ratio > 0 else 0
            herfindahl_index = concentration_ratio

            return {
                "portfolio_risk": {
                    "volatility": float(portfolio_volatility),
                    "variance": float(portfolio_variance),
                    "concentration_ratio": float(concentration_ratio),
                    "effective_n_assets": float(effective_n_assets),
                    "herfindahl_index": float(herfindahl_index)
                },
                "asset_attribution": risk_attrib_df.to_dict('records'),
                "risk_contributions": {
                    "marginal": dict(zip(assets, marginal_contrib)),
                    "component": dict(zip(assets, component_contrib)),
                    "percentage": dict(zip(assets, pct_contrib))
                }
            }

        except Exception as e:
            logger.error(f"Error in risk attribution: {e}")
            raise

    def stress_test(self,
                   weights: np.ndarray,
                   asset_returns: pd.DataFrame,
                   scenarios: List[Union[StressTestScenario, Dict]],
                   n_simulations: int = 10000,
                   use_copula: bool = True,
                   progress_callback: Optional[Callable] = None) -> Dict:
        """
        Perform stress testing on portfolio

        Parameters:
        -----------
        weights : np.ndarray
            Portfolio weights
        asset_returns : pd.DataFrame
            Asset returns data
        scenarios : List[Union[StressTestScenario, Dict]]
            Stress test scenarios
        n_simulations : int
            Number of Monte Carlo simulations
        use_copula : bool
            Use copula for dependency modeling
        progress_callback : Callable, optional
            Progress callback

        Returns:
        --------
        Dict with stress test results
        """

        try:
            assets = asset_returns.columns.tolist()
            results = {}

            for i, scenario in enumerate(scenarios):
                if progress_callback:
                    progress_callback(f"Running scenario {i+1}/{len(scenarios)}: {scenario.name}")

                # Convert to StressTestScenario if needed
                if isinstance(scenario, dict):
                    scenario = StressTestScenario(**scenario)

                # Generate stressed returns
                stressed_returns = self._generate_stressed_returns(
                    asset_returns, scenario, n_simulations, use_copula
                )

                # Calculate portfolio returns under stress
                portfolio_returns = (stressed_returns * weights).sum(axis=1)

                # Calculate stress metrics
                stress_metrics = self._calculate_stress_metrics(portfolio_returns, scenario)

                results[scenario.name] = {
                    "scenario": scenario.to_dict(),
                    "portfolio_returns": {
                        "mean": float(portfolio_returns.mean()),
                        "volatility": float(portfolio_returns.std()),
                        "var_95": float(np.percentile(portfolio_returns, 5)),
                        "cvar_95": float(portfolio_returns[portfolio_returns <= np.percentile(portfolio_returns, 5)].mean()),
                        "worst": float(portfolio_returns.min()),
                        "best": float(portfolio_returns.max())
                    },
                    "stress_metrics": stress_metrics,
                    "impact_analysis": self._calculate_stress_impact(portfolio_returns, asset_returns, weights)
                }

            # Overall stress test summary
            summary = self._summarize_stress_results(results)

            return {
                "scenarios": results,
                "summary": summary,
                "methodology": {
                    "n_simulations": n_simulations,
                    "use_copula": use_copula,
                    "assets": assets,
                    "portfolio_weights": dict(zip(assets, weights))
                }
            }

        except Exception as e:
            logger.error(f"Error in stress testing: {e}")
            raise

    def _generate_stressed_returns(self,
                                asset_returns: pd.DataFrame,
                                scenario: StressTestScenario,
                                n_simulations: int,
                                use_copula: bool) -> pd.DataFrame:
        """Generate stressed returns for scenario"""

        # Apply shocks to historical returns
        shocked_returns = asset_returns.copy()

        for asset, shock in scenario.shocks.items():
            if asset in shocked_returns.columns:
                shocked_returns[asset] = shocked_returns[asset] + shock

        # Apply volatility changes
        if scenario.volatility_changes:
            for asset, vol_change in scenario.volatility_changes.items():
                if asset in shocked_returns.columns:
                    current_vol = shocked_returns[asset].std()
                    new_vol = current_vol * (1 + vol_change)
                    shocked_returns[asset] = shocked_returns[asset] * (new_vol / current_vol)

        # Generate synthetic returns
        if use_copula:
            try:
                # Use Vine Copula for dependency modeling
                vine = VineCopula(log_transform=True, n_jobs=-1)
                vine.fit(shocked_returns)

                # Apply scenario conditioning if specified
                conditioning = None
                if scenario.shocks:
                    # Use shocks as conditioning
                    conditioning = scenario.shocks

                synthetic_returns = vine.sample(n_samples=n_simulations, conditioning=conditioning)

            except Exception as e:
                logger.warning(f"Vine copula failed, using multivariate normal: {e}")
                # Fallback to multivariate normal
                synthetic_returns = self._generate_multivariate_normal_returns(
                    shocked_returns, n_simulations
                )
        else:
            synthetic_returns = self._generate_multivariate_normal_returns(
                shocked_returns, n_simulations
            )

        return synthetic_returns

    def _generate_multivariate_normal_returns(self,
                                           returns: pd.DataFrame,
                                           n_samples: int) -> pd.DataFrame:
        """Generate returns using multivariate normal distribution"""

        mean_returns = returns.mean()
        cov_matrix = returns.cov()

        synthetic_returns = np.random.multivariate_normal(
            mean_returns, cov_matrix, n_samples
        )

        return pd.DataFrame(synthetic_returns, columns=returns.columns)

    def _calculate_stress_metrics(self, portfolio_returns: np.ndarray, scenario: StressTestScenario) -> Dict:
        """Calculate stress-specific metrics"""

        metrics = {
            "stress_var": float(np.percentile(portfolio_returns, 5)),
            "stress_cvar": float(portfolio_returns[portfolio_returns <= np.percentile(portfolio_returns, 5)].mean()),
            "stress_volatility": float(portfolio_returns.std()),
            "stress_skewness": float(stats.skew(portfolio_returns)),
            "stress_kurtosis": float(stats.kurtosis(portfolio_returns)),
            "tail_loss": float(abs(np.percentile(portfolio_returns, 1))),  # 1% tail
            "max_consecutive_losses": self._calculate_max_consecutive_losses(portfolio_returns),
            "recovery_time": self._calculate_recovery_time(portfolio_returns)
        }

        return metrics

    def _calculate_max_consecutive_losses(self, returns: np.ndarray) -> int:
        """Calculate maximum consecutive losses"""
        losses = returns < 0
        max_consecutive = 0
        current_consecutive = 0

        for loss in losses:
            if loss:
                current_consecutive += 1
                max_consecutive = max(max_consecutive, current_consecutive)
            else:
                current_consecutive = 0

        return max_consecutive

    def _calculate_recovery_time(self, returns: np.ndarray) -> int:
        """Calculate recovery time from worst loss"""
        worst_idx = np.argmin(returns)
        cumulative = np.cumprod(1 + returns[worst_idx:])
        recovery_idx = np.argmax(cumulative >= 1) if np.any(cumulative >= 1) else -1
        return recovery_idx + 1 if recovery_idx >= 0 else len(returns) - worst_idx

    def _calculate_stress_impact(self,
                               stressed_portfolio_returns: np.ndarray,
                               historical_returns: pd.DataFrame,
                               weights: np.ndarray) -> Dict:
        """Calculate stress impact compared to historical performance"""

        # Historical portfolio returns
        historical_portfolio_returns = (historical_returns * weights).sum(axis=1)

        # Compare statistics
        historical_mean = historical_portfolio_returns.mean()
        stressed_mean = stressed_portfolio_returns.mean()

        historical_vol = historical_portfolio_returns.std()
        stressed_vol = stressed_portfolio_returns.std()

        impact = {
            "return_impact": stressed_mean - historical_mean,
            "volatility_impact": stressed_vol - historical_vol,
            "return_impact_pct": ((stressed_mean - historical_mean) / abs(historical_mean)) * 100 if historical_mean != 0 else 0,
            "volatility_impact_pct": ((stressed_vol - historical_vol) / historical_vol) * 100 if historical_vol != 0 else 0,
            "historical_var_95": float(np.percentile(historical_portfolio_returns, 5)),
            "stressed_var_95": float(np.percentile(stressed_portfolio_returns, 5)),
            "var_impact": float(np.percentile(stressed_portfolio_returns, 5) - np.percentile(historical_portfolio_returns, 5))
        }

        return impact

    def _summarize_stress_results(self, results: Dict) -> Dict:
        """Summarize stress test results across all scenarios"""

        summary = {
            "worst_case_scenario": None,
            "best_case_scenario": None,
            "average_impact": {},
            "risk_magnification": {}
        }

        all_var_95 = []
        all_cvar_95 = []
        all_worst_returns = []

        for scenario_name, scenario_results in results.items():
            var_95 = scenario_results["portfolio_returns"]["var_95"]
            cvar_95 = scenario_results["portfolio_returns"]["cvar_95"]
            worst_return = scenario_results["portfolio_returns"]["worst"]

            all_var_95.append(var_95)
            all_cvar_95.append(cvar_95)
            all_worst_returns.append(worst_return)

        if all_var_95:
            worst_idx = np.argmin(all_var_95)
            best_idx = np.argmax(all_var_95)

            scenario_names = list(results.keys())
            summary["worst_case_scenario"] = {
                "name": scenario_names[worst_idx],
                "var_95": all_var_95[worst_idx],
                "cvar_95": all_cvar_95[worst_idx]
            }
            summary["best_case_scenario"] = {
                "name": scenario_names[best_idx],
                "var_95": all_var_95[best_idx],
                "cvar_95": all_cvar_95[best_idx]
            }

            summary["average_impact"] = {
                "avg_var_95": float(np.mean(all_var_95)),
                "avg_cvar_95": float(np.mean(all_cvar_95)),
                "avg_worst_return": float(np.mean(all_worst_returns))
            }

        return summary

    def create_risk_budget(self,
                         weights: np.ndarray,
                         asset_returns: pd.DataFrame,
                         risk_budget_allocations: Optional[Dict[str, float]] = None) -> RiskBudget:
        """
        Create risk budget allocation

        Parameters:
        -----------
        weights : np.ndarray
            Portfolio weights
        asset_returns : pd.DataFrame
            Asset returns data
        risk_budget_allocations : Dict[str, float], optional
            Target risk budget allocations

        Returns:
        --------
        RiskBudget object
        """

        try:
            assets = asset_returns.columns.tolist()
            cov_matrix = asset_returns.cov() * 252  # Annualized

            # Calculate risk contributions
            portfolio_variance = weights.T @ cov_matrix @ weights
            portfolio_vol = np.sqrt(portfolio_variance)

            # Marginal contributions to risk
            marginal_contrib = (cov_matrix @ weights) / portfolio_vol

            # Component contributions to risk
            component_contrib = weights * marginal_contrib

            # Default risk budget allocations (equal risk contribution)
            if risk_budget_allocations is None:
                equal_risk_budget = 1.0 / len(assets)
                risk_budget_allocations = {asset: equal_risk_budget for asset in assets}

            # Calculate risk utilization
            risk_utilization = {}
            for i, asset in enumerate(assets):
                target_budget = risk_budget_allocations.get(asset, 0)
                actual_contribution = component_contrib[i] / portfolio_vol
                utilization = actual_contribution / target_budget if target_budget > 0 else 0
                risk_utilization[asset] = utilization

            return RiskBudget(
                total_risk_budget=portfolio_vol,
                asset_risk_contributions=dict(zip(assets, component_contrib)),
                risk_budget_allocations=risk_budget_allocations,
                risk_utilization=risk_utilization,
                marginal_risk_contributions=dict(zip(assets, marginal_contrib)),
                component_risk_contributions=dict(zip(assets, component_contrib))
            )

        except Exception as e:
            logger.error(f"Error creating risk budget: {e}")
            raise

    def monte_carlo_simulation(self,
                             asset_returns: pd.DataFrame,
                             weights: np.ndarray,
                             n_simulations: int = 10000,
                             time_horizon: int = 252,
                             use_copula: bool = True,
                             confidence_intervals: List[float] = [0.05, 0.95],
                             progress_callback: Optional[Callable] = None) -> Dict:
        """
        Perform Monte Carlo simulation

        Parameters:
        -----------
        asset_returns : pd.DataFrame
            Asset returns data
        weights : np.ndarray
            Portfolio weights
        n_simulations : int
            Number of simulations
        time_horizon : int
            Simulation time horizon (days)
        use_copula : bool
            Use copula for dependency modeling
        confidence_intervals : List[float]
            Confidence intervals for results
        progress_callback : Callable, optional
            Progress callback

        Returns:
        --------
        Dict with simulation results
        """

        try:
            assets = asset_returns.columns.tolist()

            if progress_callback:
                progress_callback("Setting up simulation parameters...")

            # Generate synthetic returns
            if use_copula:
                try:
                    vine = VineCopula(log_transform=True, n_jobs=-1)
                    vine.fit(asset_returns)
                    synthetic_returns = vine.sample(n_samples=n_simulations * time_horizon)
                except Exception as e:
                    logger.warning(f"Vine copula failed, using multivariate normal: {e}")
                    synthetic_returns = self._generate_multivariate_normal_returns(
                        asset_returns, n_simulations * time_horizon
                    )
            else:
                synthetic_returns = self._generate_multivariate_normal_returns(
                    asset_returns, n_simulations * time_horizon
                )

            if progress_callback:
                progress_callback("Running Monte Carlo simulation...")

            # Reshape for time series
            synthetic_returns = synthetic_returns.values.reshape(
                n_simulations, time_horizon, len(assets)
            )

            # Calculate portfolio returns for each simulation
            portfolio_returns = np.array([
                np.sum(synthetic_returns[i] * weights, axis=1)
                for i in range(n_simulations)
            ])

            # Calculate cumulative wealth paths
            wealth_paths = np.cumprod(1 + portfolio_returns, axis=1)

            # Calculate statistics
            final_wealth = wealth_paths[:, -1]
            total_returns = wealth_paths[:, -1] - 1

            # Confidence intervals
            confidence_results = {}
            for ci in confidence_intervals:
                lower = (1 - ci) / 2 * 100
                upper = (1 + ci) / 2 * 100
                confidence_results[f"ci_{ci}"] = {
                    "final_wealth_lower": float(np.percentile(final_wealth, lower)),
                    "final_wealth_upper": float(np.percentile(final_wealth, upper)),
                    "total_return_lower": float(np.percentile(total_returns, lower)),
                    "total_return_upper": float(np.percentile(total_returns, upper))
                }

            # Path statistics
            max_wealth = np.max(wealth_paths, axis=1)
            min_wealth = np.min(wealth_paths, axis=1)
            max_drawdown = np.max(1 - wealth_paths / np.maximum.accumulate(wealth_paths, axis=1), axis=1)

            if progress_callback:
                progress_callback("Monte Carlo simulation completed")

            return {
                "simulation_parameters": {
                    "n_simulations": n_simulations,
                    "time_horizon": time_horizon,
                    "use_copula": use_copula,
                    "assets": assets,
                    "weights": dict(zip(assets, weights))
                },
                "final_statistics": {
                    "mean_final_wealth": float(np.mean(final_wealth)),
                    "median_final_wealth": float(np.median(final_wealth)),
                    "std_final_wealth": float(np.std(final_wealth)),
                    "mean_total_return": float(np.mean(total_returns)),
                    "median_total_return": float(np.median(total_returns)),
                    "std_total_return": float(np.std(total_returns))
                },
                "path_statistics": {
                    "mean_max_wealth": float(np.mean(max_wealth)),
                    "mean_min_wealth": float(np.mean(min_wealth)),
                    "mean_max_drawdown": float(np.mean(max_drawdown)),
                    "median_max_drawdown": float(np.median(max_drawdown))
                },
                "confidence_intervals": confidence_results,
                "distribution": {
                    "percentiles": {
                        f"p{p}": float(np.percentile(total_returns, p))
                        for p in [1, 5, 10, 25, 50, 75, 90, 95, 99]
                    },
                    "probability_positive": float(np.mean(total_returns > 0)),
                    "probability_loss": float(np.mean(total_returns < 0)),
                    "probability_severe_loss": float(np.mean(total_returns < -0.2))  # 20% loss
                }
            }

        except Exception as e:
            logger.error(f"Error in Monte Carlo simulation: {e}")
            raise

# Convenience functions
def quick_risk_analysis(returns: pd.Series, benchmark_returns: Optional[pd.Series] = None) -> Dict:
    """
    Quick risk analysis for returns series

    Parameters:
    -----------
    returns : pd.Series
        Portfolio returns
    benchmark_returns : pd.Series, optional
        Benchmark returns

    Returns:
    --------
    Dict with risk metrics
    """

    analyzer = RiskAnalyzer()
    risk_metrics = analyzer.calculate_risk_metrics(returns, benchmark_returns)
    return risk_metrics.to_dict()

def create_default_stress_scenarios() -> List[StressTestScenario]:
    """Create default stress test scenarios"""

    scenarios = [
        StressTestScenario(
            name="market_crash",
            description="Severe market downturn",
            shocks={"market": -0.30},
            probability=0.05
        ),
        StressTestScenario(
            name="volatility_spike",
            description="Extreme volatility increase",
            volatility_changes={"market": 2.0},
            probability=0.10
        ),
        StressTestScenario(
            name="sector_rotation",
            description="Major sector rotation",
            shocks={"technology": -0.20, "utilities": 0.15},
            probability=0.15
        ),
        StressTestScenario(
            name="interest_rate_shock",
            description="Unexpected interest rate change",
            shocks={"bonds": -0.10, "real_estate": -0.15},
            probability=0.20
        )
    ]

    return scenarios

# Command line interface
def main():
    """Command line interface"""
    import sys
    import json

    if len(sys.argv) < 2:
        print(json.dumps({
            "error": "Usage: python skfolio_risk.py <command> <args>",
            "commands": ["analyze", "stress_test", "monte_carlo", "risk_budget"]
        }))
        return

    command = sys.argv[1]

    if command == "analyze":
        if len(sys.argv) < 3:
            print(json.dumps({"error": "Usage: analyze <returns_file> [benchmark_file]"}))
            return

        try:
            returns = pd.read_csv(sys.argv[2], index_col=0, parse_dates=True)
            returns = returns.iloc[:, 0]  # Use first column as returns

            benchmark_returns = None
            if len(sys.argv) > 3:
                benchmark = pd.read_csv(sys.argv[3], index_col=0, parse_dates=True)
                benchmark_returns = benchmark.iloc[:, 0]

            risk_metrics = quick_risk_analysis(returns, benchmark_returns)
            print(json.dumps(risk_metrics, indent=2, default=str))

        except Exception as e:
            print(json.dumps({"error": str(e)}))

    elif command == "stress_test":
        print(json.dumps({
            "message": "Stress testing requires Python integration",
            "usage": "Use the stress_test method from RiskAnalyzer class"
        }))

    else:
        print(json.dumps({"error": f"Unknown command: {command}"}))

if __name__ == "__main__":
    main()