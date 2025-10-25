"""
Portfolio Analytics Module
==========================

Comprehensive portfolio risk and return analysis compliant with CFA Institute standards.
Provides modern portfolio theory implementation, performance attribution, risk metrics,
and portfolio optimization capabilities for multi-asset investment portfolios with
advanced analytics and reporting features.

===== DATA SOURCES REQUIRED =====
INPUT:
  - Portfolio holdings and transaction data
  - Asset price series and market returns
  - Benchmark indices and market data
  - Risk-free rates and economic indicators
  - Portfolio constraints and investment policy

OUTPUT:
  - Portfolio performance metrics (returns, volatility, Sharpe ratio)
  - Risk decomposition and attribution analysis
  - Efficient frontier and optimization results
  - Performance attribution vs benchmarks
  - Portfolio analytics reports and visualizations

PARAMETERS:
  - risk_free_rate: Risk-free rate for calculations (default: 0.02)
  - confidence_level: VaR confidence level (default: 0.95)
  - benchmark: Portfolio benchmark index (default: 'SPY')
  - rebalance_frequency: Rebalancing frequency (default: 'monthly')
  - optimization_method: Portfolio optimization method (default: 'mean_variance')
  - lookback_period: Historical lookback window (default: 252)
  - currency: Portfolio base currency (default: 'USD')
"""

import numpy as np
import pandas as pd
from typing import Dict, List, Optional, Union, Tuple
import warnings

from config import (
    AssetClass, MathConstants, PortfolioParameters,
    DEFAULT_PORTFOLIO_PARAMS, validate_weights, ERROR_MESSAGES
)
from math_engine import (
    StatisticalCalculations, PortfolioMath, PerformanceCalculations,
    RiskCalculations, OptimizationEngine
)


class AssetClassAnalysis:
    """Major asset class characteristics and analysis"""

    ASSET_CLASS_PROPERTIES = {
        AssetClass.EQUITY: {
            "expected_return_range": (0.06, 0.12),
            "volatility_range": (0.15, 0.25),
            "liquidity": "high",
            "inflation_hedge": "moderate",
            "correlation_with_bonds": -0.2
        },
        AssetClass.FIXED_INCOME: {
            "expected_return_range": (0.02, 0.06),
            "volatility_range": (0.03, 0.08),
            "liquidity": "high",
            "inflation_hedge": "poor",
            "correlation_with_equity": -0.2
        },
        AssetClass.COMMODITIES: {
            "expected_return_range": (0.03, 0.08),
            "volatility_range": (0.20, 0.35),
            "liquidity": "moderate",
            "inflation_hedge": "good",
            "correlation_with_equity": 0.3
        },
        AssetClass.REAL_ESTATE: {
            "expected_return_range": (0.04, 0.10),
            "volatility_range": (0.12, 0.20),
            "liquidity": "low",
            "inflation_hedge": "good",
            "correlation_with_equity": 0.6
        }
    }

    @classmethod
    def get_asset_characteristics(cls, asset_class: AssetClass) -> Dict:
        """Get characteristics of major asset classes"""
        return cls.ASSET_CLASS_PROPERTIES.get(asset_class, {})

    @classmethod
    def analyze_diversification_benefits(cls, asset_classes: List[AssetClass]) -> Dict:
        """Analyze diversification benefits across asset classes"""
        correlations = {}
        total_pairs = 0
        sum_correlations = 0

        for i, asset1 in enumerate(asset_classes):
            for j, asset2 in enumerate(asset_classes[i + 1:], i + 1):
                # Get correlation estimates between asset classes
                if asset1 == AssetClass.EQUITY and asset2 == AssetClass.FIXED_INCOME:
                    corr = cls.ASSET_CLASS_PROPERTIES[AssetClass.EQUITY]["correlation_with_bonds"]
                elif asset1 == AssetClass.FIXED_INCOME and asset2 == AssetClass.EQUITY:
                    corr = cls.ASSET_CLASS_PROPERTIES[AssetClass.FIXED_INCOME]["correlation_with_equity"]
                elif asset1 == AssetClass.COMMODITIES and asset2 == AssetClass.EQUITY:
                    corr = cls.ASSET_CLASS_PROPERTIES[AssetClass.COMMODITIES]["correlation_with_equity"]
                elif asset1 == AssetClass.REAL_ESTATE and asset2 == AssetClass.EQUITY:
                    corr = cls.ASSET_CLASS_PROPERTIES[AssetClass.REAL_ESTATE]["correlation_with_equity"]
                else:
                    corr = 0.5  # Default moderate correlation

                correlations[f"{asset1.value}_{asset2.value}"] = corr
                sum_correlations += abs(corr)
                total_pairs += 1

        avg_correlation = sum_correlations / total_pairs if total_pairs > 0 else 0
        diversification_ratio = 1 - avg_correlation

        return {
            "pairwise_correlations": correlations,
            "average_correlation": avg_correlation,
            "diversification_ratio": diversification_ratio,
            "diversification_benefit": "High" if diversification_ratio > 0.7 else
            "Moderate" if diversification_ratio > 0.4 else "Low"
        }


class RiskAversionAnalysis:
    """Risk aversion modeling and utility analysis"""

    @staticmethod
    def utility_function(return_value: float, variance: float,
                         risk_aversion: float, function_type: str = "quadratic") -> float:
        """Calculate utility based on return and risk"""
        if function_type == "quadratic":
            # U = E(R) - (A/2) * Var(R)
            return return_value - (risk_aversion / 2) * variance
        elif function_type == "exponential":
            # U = -exp(-A * E(R))
            return -np.exp(-risk_aversion * return_value)
        elif function_type == "log":
            # U = ln(1 + R) for positive returns
            if return_value > -1:
                return np.log(1 + return_value)
            else:
                return -np.inf
        else:
            raise ValueError("Function type must be 'quadratic', 'exponential', or 'log'")

    @staticmethod
    def certainty_equivalent(expected_return: float, variance: float,
                             risk_aversion: float) -> float:
        """Calculate certainty equivalent return"""
        return expected_return - (risk_aversion / 2) * variance

    @staticmethod
    def risk_premium(expected_return: float, risk_free_rate: float,
                     variance: float, risk_aversion: float) -> float:
        """Calculate required risk premium"""
        return (risk_aversion / 2) * variance

    @classmethod
    def optimal_portfolio_selection(cls, expected_returns: np.ndarray,
                                    cov_matrix: np.ndarray,
                                    risk_aversion: float,
                                    risk_free_rate: float = MathConstants.DEFAULT_RISK_FREE_RATE) -> Dict:
        """Find optimal portfolio for given risk aversion"""
        n_assets = len(expected_returns)

        # Objective: maximize utility U = w'μ - (A/2)w'Σw
        def objective(weights):
            portfolio_return = PortfolioMath.calculate_portfolio_return(weights, expected_returns)
            portfolio_variance = PortfolioMath.calculate_portfolio_variance(weights, cov_matrix)
            utility = cls.utility_function(portfolio_return, portfolio_variance, risk_aversion)
            return -utility  # Minimize negative utility

        from scipy import optimize

        # Constraints
        cons = [{'type': 'eq', 'fun': lambda w: np.sum(w) - 1.0}]
        bounds = tuple((0, 1) for _ in range(n_assets))
        x0 = np.ones(n_assets) / n_assets

        result = optimize.minimize(objective, x0, method='SLSQP',
                                   bounds=bounds, constraints=cons)

        if result.success:
            weights = result.x
            portfolio_return = PortfolioMath.calculate_portfolio_return(weights, expected_returns)
            portfolio_std = PortfolioMath.calculate_portfolio_std(weights, cov_matrix)
            portfolio_variance = portfolio_std ** 2
            utility = cls.utility_function(portfolio_return, portfolio_variance, risk_aversion)
            ce_return = cls.certainty_equivalent(portfolio_return, portfolio_variance, risk_aversion)

            return {
                "optimal_weights": weights,
                "expected_return": portfolio_return,
                "standard_deviation": portfolio_std,
                "variance": portfolio_variance,
                "utility": utility,
                "certainty_equivalent": ce_return,
                "risk_premium": portfolio_return - risk_free_rate
            }
        else:
            raise ValueError(ERROR_MESSAGES["optimization_failed"])


class CapitalAllocationLine:
    """Capital Allocation Line (CAL) and Capital Market Line (CML) analysis"""

    @staticmethod
    def calculate_cal(risky_portfolio_return: float,
                      risky_portfolio_std: float,
                      risk_free_rate: float = MathConstants.DEFAULT_RISK_FREE_RATE) -> Dict:
        """Calculate Capital Allocation Line parameters"""
        slope = (risky_portfolio_return - risk_free_rate) / risky_portfolio_std

        return {
            "intercept": risk_free_rate,
            "slope": slope,
            "sharpe_ratio": slope,
            "risky_return": risky_portfolio_return,
            "risky_std": risky_portfolio_std
        }

    @staticmethod
    def optimal_allocation(target_return: Optional[float] = None,
                           target_std: Optional[float] = None,
                           risky_portfolio_return: float = None,
                           risky_portfolio_std: float = None,
                           risk_free_rate: float = MathConstants.DEFAULT_RISK_FREE_RATE) -> Dict:
        """Find optimal allocation between risk-free and risky assets"""
        if target_return is not None:
            # Given target return, find allocation
            y = (target_return - risk_free_rate) / (risky_portfolio_return - risk_free_rate)
            portfolio_std = y * risky_portfolio_std

        elif target_std is not None:
            # Given target standard deviation, find allocation
            y = target_std / risky_portfolio_std
            portfolio_return = risk_free_rate + y * (risky_portfolio_return - risk_free_rate)
            target_return = portfolio_return

        else:
            raise ValueError("Either target_return or target_std must be specified")

        # Ensure valid allocation
        y = max(0, y)  # No short selling risk-free asset

        return {
            "risky_asset_weight": y,
            "risk_free_weight": 1 - y,
            "portfolio_return": target_return,
            "portfolio_std": target_std if target_std else portfolio_std,
            "leveraged": y > 1.0
        }


class SystematicRiskAnalysis:
    """Systematic vs. Nonsystematic risk analysis"""

    @staticmethod
    def decompose_risk(asset_returns: np.ndarray,
                       market_returns: np.ndarray) -> Dict:
        """Decompose total risk into systematic and nonsystematic components"""
        beta = StatisticalCalculations.calculate_beta(asset_returns, market_returns)

        # Total variance
        total_variance = StatisticalCalculations.calculate_variance(asset_returns)

        # Market variance
        market_variance = StatisticalCalculations.calculate_variance(market_returns)

        # Systematic variance = β² * σ²ₘ
        systematic_variance = (beta ** 2) * market_variance

        # Nonsystematic variance = Total - Systematic
        nonsystematic_variance = total_variance - systematic_variance

        # R-squared (proportion of variance explained by market)
        correlation = StatisticalCalculations.calculate_correlation(asset_returns, market_returns)
        r_squared = correlation ** 2

        return {
            "beta": beta,
            "total_variance": total_variance,
            "total_std": np.sqrt(total_variance),
            "systematic_variance": systematic_variance,
            "systematic_std": np.sqrt(systematic_variance),
            "nonsystematic_variance": max(0, nonsystematic_variance),
            "nonsystematic_std": np.sqrt(max(0, nonsystematic_variance)),
            "r_squared": r_squared,
            "systematic_risk_percentage": systematic_variance / total_variance * 100,
            "nonsystematic_risk_percentage": max(0, nonsystematic_variance) / total_variance * 100
        }

    @staticmethod
    def portfolio_beta(individual_betas: np.ndarray, weights: np.ndarray) -> float:
        """Calculate portfolio beta as weighted average of individual betas"""
        if not validate_weights(weights):
            raise ValueError(ERROR_MESSAGES["invalid_weights"])
        return np.dot(weights, individual_betas)


class CAPMAnalysis:
    """Capital Asset Pricing Model implementation"""

    @staticmethod
    def expected_return(beta: float,
                        risk_free_rate: float = MathConstants.DEFAULT_RISK_FREE_RATE,
                        market_return: float = 0.10) -> float:
        """Calculate expected return using CAPM"""
        return risk_free_rate + beta * (market_return - risk_free_rate)

    @staticmethod
    def security_market_line(betas: np.ndarray,
                             risk_free_rate: float = MathConstants.DEFAULT_RISK_FREE_RATE,
                             market_return: float = 0.10) -> np.ndarray:
        """Generate Security Market Line for given betas"""
        return risk_free_rate + betas * (market_return - risk_free_rate)

    @staticmethod
    def alpha_calculation(actual_returns: np.ndarray,
                          market_returns: np.ndarray,
                          risk_free_rate: float = MathConstants.DEFAULT_RISK_FREE_RATE) -> Dict:
        """Calculate Jensen's alpha and related metrics"""
        beta = StatisticalCalculations.calculate_beta(actual_returns, market_returns)

        # Average returns (annualized)
        avg_actual_return = np.mean(actual_returns) * MathConstants.TRADING_DAYS_YEAR
        avg_market_return = np.mean(market_returns) * MathConstants.TRADING_DAYS_YEAR

        # Expected return from CAPM
        expected_return = CAPMAnalysis.expected_return(beta, risk_free_rate, avg_market_return)

        # Alpha
        alpha = avg_actual_return - expected_return

        return {
            "alpha": alpha,
            "beta": beta,
            "actual_return": avg_actual_return,
            "expected_return": expected_return,
            "market_return": avg_market_return,
            "excess_return": avg_actual_return - risk_free_rate,
            "market_premium": avg_market_return - risk_free_rate
        }

    @staticmethod
    def capm_assumptions_check(returns_data: Dict[str, np.ndarray]) -> Dict:
        """Check CAPM assumptions validity"""
        assumptions = {
            "homogeneous_expectations": "Cannot verify with historical data",
            "single_period_model": "Using historical multi-period data",
            "risk_free_borrowing_lending": "Assumed available",
            "no_transaction_costs": "Assumed",
            "normal_distribution": {},
            "constant_correlations": {}
        }

        # Test normality
        from scipy import stats
        for asset, returns in returns_data.items():
            shapiro_stat, shapiro_p = stats.shapiro(returns[:min(5000, len(returns))])
            assumptions["normal_distribution"][asset] = {
                "shapiro_test_p_value": shapiro_p,
                "approximately_normal": shapiro_p > 0.05
            }

        # Test correlation stability (split sample)
        if len(returns_data) >= 2:
            assets = list(returns_data.keys())
            asset1_returns = returns_data[assets[0]]
            asset2_returns = returns_data[assets[1]]

            mid_point = len(asset1_returns) // 2
            corr1 = StatisticalCalculations.calculate_correlation(
                asset1_returns[:mid_point], asset2_returns[:mid_point]
            )
            corr2 = StatisticalCalculations.calculate_correlation(
                asset1_returns[mid_point:], asset2_returns[mid_point:]
            )

            assumptions["constant_correlations"] = {
                "first_half_correlation": corr1,
                "second_half_correlation": corr2,
                "correlation_difference": abs(corr1 - corr2),
                "stable_correlations": abs(corr1 - corr2) < 0.2
            }

        return assumptions


class EfficientFrontierAnalysis:
    """Efficient frontier construction and analysis"""

    def __init__(self, expected_returns: np.ndarray, cov_matrix: np.ndarray,
                 parameters: PortfolioParameters = DEFAULT_PORTFOLIO_PARAMS):
        self.expected_returns = expected_returns
        self.cov_matrix = cov_matrix
        self.parameters = parameters
        self.n_assets = len(expected_returns)

        if not validate_covariance_matrix(cov_matrix):
            raise ValueError(ERROR_MESSAGES["singular_matrix"])

    def generate_frontier(self) -> Dict:
        """Generate complete efficient frontier"""
        # Find minimum variance portfolio
        min_var_result = PortfolioMath.find_minimum_variance_portfolio(
            self.cov_matrix,
            {
                'min_weight': self.parameters.min_weight,
                'max_weight': self.parameters.max_weight
            }
        )

        # Generate efficient frontier
        frontier_result = OptimizationEngine.efficient_frontier(
            self.expected_returns,
            self.cov_matrix,
            self.parameters.num_frontier_points,
            {
                'min_weight': self.parameters.min_weight,
                'max_weight': self.parameters.max_weight
            }
        )

        # Find maximum Sharpe portfolio
        max_sharpe_result = OptimizationEngine.maximum_sharpe_portfolio(
            self.expected_returns,
            self.cov_matrix,
            self.parameters.risk_free_rate,
            {
                'min_weight': self.parameters.min_weight,
                'max_weight': self.parameters.max_weight
            }
        )

        return {
            "frontier_returns": frontier_result["returns"],
            "frontier_stds": frontier_result["stds"],
            "frontier_weights": frontier_result["weights"],
            "frontier_sharpe_ratios": frontier_result["sharpe_ratios"],
            "min_variance_portfolio": min_var_result,
            "max_sharpe_portfolio": max_sharpe_result,
            "capital_market_line": self._calculate_cml(max_sharpe_result)
        }

    def _calculate_cml(self, max_sharpe_portfolio: Dict) -> Dict:
        """Calculate Capital Market Line parameters"""
        return CapitalAllocationLine.calculate_cal(
            max_sharpe_portfolio["expected_return"],
            max_sharpe_portfolio["std"],
            self.parameters.risk_free_rate
        )

    def portfolio_on_frontier(self, target_return: float) -> Dict:
        """Find specific portfolio on efficient frontier"""
        from scipy import optimize

        def objective(weights):
            return PortfolioMath.calculate_portfolio_variance(weights, self.cov_matrix)

        # Constraints
        cons = [
            {'type': 'eq', 'fun': lambda w: np.sum(w) - 1.0},
            {'type': 'eq', 'fun': lambda w:
            PortfolioMath.calculate_portfolio_return(w, self.expected_returns) - target_return}
        ]

        bounds = tuple((self.parameters.min_weight, self.parameters.max_weight)
                       for _ in range(self.n_assets))
        x0 = np.ones(self.n_assets) / self.n_assets

        result = optimize.minimize(objective, x0, method='SLSQP',
                                   bounds=bounds, constraints=cons)

        if result.success:
            weights = result.x
            portfolio_std = PortfolioMath.calculate_portfolio_std(weights, self.cov_matrix)
            sharpe_ratio = (target_return - self.parameters.risk_free_rate) / portfolio_std

            return {
                "weights": weights,
                "expected_return": target_return,
                "standard_deviation": portfolio_std,
                "sharpe_ratio": sharpe_ratio,
                "on_efficient_frontier": True
            }
        else:
            return {"on_efficient_frontier": False, "error": "Optimization failed"}


class CorrelationEffectsAnalysis:
    """Analysis of correlation effects on portfolio risk"""

    @staticmethod
    def correlation_impact(weights: np.ndarray,
                           individual_stds: np.ndarray,
                           correlation_matrix: np.ndarray) -> Dict:
        """Analyze impact of correlations on portfolio risk"""
        # Portfolio standard deviation with actual correlations
        cov_matrix = np.outer(individual_stds, individual_stds) * correlation_matrix
        actual_portfolio_std = PortfolioMath.calculate_portfolio_std(weights, cov_matrix)

        # Portfolio standard deviation if perfectly correlated (ρ = 1)
        perfect_corr_matrix = np.ones_like(correlation_matrix)
        perfect_cov_matrix = np.outer(individual_stds, individual_stds) * perfect_corr_matrix
        perfect_corr_std = PortfolioMath.calculate_portfolio_std(weights, perfect_cov_matrix)

        # Portfolio standard deviation if uncorrelated (ρ = 0)
        zero_corr_matrix = np.eye(len(correlation_matrix))
        zero_cov_matrix = np.outer(individual_stds, individual_stds) * zero_corr_matrix
        zero_corr_std = PortfolioMath.calculate_portfolio_std(weights, zero_cov_matrix)

        # Diversification benefit
        weighted_avg_std = np.dot(weights, individual_stds)
        diversification_ratio = PortfolioMath.calculate_diversification_ratio(
            weights, individual_stds, actual_portfolio_std
        )

        return {
            "actual_portfolio_std": actual_portfolio_std,
            "perfect_correlation_std": perfect_corr_std,
            "zero_correlation_std": zero_corr_std,
            "weighted_average_std": weighted_avg_std,
            "diversification_ratio": diversification_ratio,
            "risk_reduction_vs_perfect_corr": (perfect_corr_std - actual_portfolio_std) / perfect_corr_std,
            "risk_reduction_vs_weighted_avg": (weighted_avg_std - actual_portfolio_std) / weighted_avg_std,
            "average_correlation": np.mean(correlation_matrix[correlation_matrix != 1.0])
        }

    @staticmethod
    def optimal_correlation_for_risk_target(weights: np.ndarray,
                                            individual_stds: np.ndarray,
                                            target_portfolio_std: float) -> float:
        """Find correlation needed to achieve target portfolio standard deviation"""

        # This assumes equal correlation between all pairs
        def objective(avg_correlation):
            # Create correlation matrix with average correlation
            n = len(weights)
            corr_matrix = np.full((n, n), avg_correlation)
            np.fill_diagonal(corr_matrix, 1.0)

            # Calculate portfolio std
            cov_matrix = np.outer(individual_stds, individual_stds) * corr_matrix
            portfolio_std = PortfolioMath.calculate_portfolio_std(weights, cov_matrix)

            return (portfolio_std - target_portfolio_std) ** 2

        from scipy import optimize

        result = optimize.minimize_scalar(objective, bounds=(-1, 1), method='bounded')

        return result.x if result.success else None


class PortfolioAnalytics:
    """Main portfolio analytics interface"""

    def __init__(self, parameters: PortfolioParameters = DEFAULT_PORTFOLIO_PARAMS):
        self.parameters = parameters

    def comprehensive_analysis(self, returns_data: Dict[str, np.ndarray],
                               weights: Optional[np.ndarray] = None,
                               market_returns: Optional[np.ndarray] = None) -> Dict:
        """Perform comprehensive portfolio analysis"""
        # Convert returns to matrix format
        returns_df = pd.DataFrame(returns_data)
        returns_matrix = returns_df.dropna().values
        asset_names = list(returns_data.keys())

        # Calculate basic statistics
        expected_returns = np.mean(returns_matrix, axis=0) * MathConstants.TRADING_DAYS_YEAR
        cov_matrix = np.cov(returns_matrix.T) * MathConstants.TRADING_DAYS_YEAR
        corr_matrix = np.corrcoef(returns_matrix.T)
        individual_stds = np.sqrt(np.diag(cov_matrix))

        # Default equal weights if not provided
        if weights is None:
            weights = np.ones(len(asset_names)) / len(asset_names)

        # Portfolio statistics
        portfolio_return = PortfolioMath.calculate_portfolio_return(weights, expected_returns)
        portfolio_std = PortfolioMath.calculate_portfolio_std(weights, cov_matrix)
        portfolio_sharpe = PerformanceCalculations.sharpe_ratio(
            np.dot(returns_matrix, weights), self.parameters.risk_free_rate
        )

        results = {
            "basic_statistics": {
                "asset_names": asset_names,
                "expected_returns": expected_returns,
                "volatilities": individual_stds,
                "correlation_matrix": corr_matrix,
                "covariance_matrix": cov_matrix
            },
            "portfolio_metrics": {
                "weights": weights,
                "expected_return": portfolio_return,
                "standard_deviation": portfolio_std,
                "variance": portfolio_std ** 2,
                "sharpe_ratio": portfolio_sharpe
            }
        }

        # Correlation effects analysis
        correlation_analysis = CorrelationEffectsAnalysis.correlation_impact(
            weights, individual_stds, corr_matrix
        )
        results["correlation_analysis"] = correlation_analysis

        # Efficient frontier analysis
        try:
            frontier_analyzer = EfficientFrontierAnalysis(expected_returns, cov_matrix, self.parameters)
            frontier_results = frontier_analyzer.generate_frontier()
            results["efficient_frontier"] = frontier_results
        except Exception as e:
            results["efficient_frontier"] = {"error": str(e)}

        # CAPM analysis if market returns provided
        if market_returns is not None:
            portfolio_returns = np.dot(returns_matrix, weights)
            capm_results = CAPMAnalysis.alpha_calculation(
                portfolio_returns, market_returns, self.parameters.risk_free_rate
            )

            # Individual asset betas
            individual_betas = []
            for i in range(len(asset_names)):
                beta = StatisticalCalculations.calculate_beta(returns_matrix[:, i], market_returns)
                individual_betas.append(beta)

            # Systematic risk decomposition
            systematic_analysis = SystematicRiskAnalysis.decompose_risk(
                portfolio_returns, market_returns
            )

            results["capm_analysis"] = capm_results
            results["individual_betas"] = dict(zip(asset_names, individual_betas))
            results["systematic_risk_analysis"] = systematic_analysis

        # Risk metrics
        portfolio_returns = np.dot(returns_matrix, weights)
        var_95 = RiskCalculations.value_at_risk_historical(portfolio_returns, 0.95)
        var_99 = RiskCalculations.value_at_risk_historical(portfolio_returns, 0.99)
        cvar_95 = RiskCalculations.conditional_value_at_risk(portfolio_returns, 0.95)

        results["risk_metrics"] = {
            "value_at_risk_95": var_95,
            "value_at_risk_99": var_99,
            "conditional_var_95": cvar_95,
            "maximum_drawdown": self._calculate_max_drawdown(portfolio_returns)
        }

        return results

    def _calculate_max_drawdown(self, returns: np.ndarray) -> Dict:
        """Calculate maximum drawdown"""
        cumulative_returns = np.cumprod(1 + returns)
        running_max = np.maximum.accumulate(cumulative_returns)
        drawdown = (cumulative_returns - running_max) / running_max

        max_dd = np.min(drawdown)
        max_dd_idx = np.argmin(drawdown)

        # Find peak before max drawdown
        peak_idx = np.argmax(running_max[:max_dd_idx + 1])

        return {
            "max_drawdown": max_dd,
            "peak_index": peak_idx,
            "trough_index": max_dd_idx,
            "recovery_time": None  # Would need to calculate recovery
        }