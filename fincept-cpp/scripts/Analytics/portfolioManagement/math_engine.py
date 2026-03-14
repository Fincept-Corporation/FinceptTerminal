
"""Portfolio Math Engine Module
===============================

Mathematical engine for portfolio calculations

===== DATA SOURCES REQUIRED =====
INPUT:
  - Portfolio holdings and transaction history
  - Asset price data and market returns
  - Benchmark indices and market data
  - Investment policy statements and constraints
  - Risk tolerance and preference parameters

OUTPUT:
  - Portfolio performance metrics and attribution
  - Risk analysis and diversification metrics
  - Rebalancing recommendations and optimization
  - Portfolio analytics reports and visualizations
  - Investment strategy recommendations

PARAMETERS:
  - optimization_method: Portfolio optimization method (default: 'mean_variance')
  - risk_free_rate: Risk-free rate for calculations (default: 0.02)
  - rebalance_frequency: Portfolio rebalancing frequency (default: 'quarterly')
  - max_weight: Maximum single asset weight (default: 0.10)
  - benchmark: Portfolio benchmark index (default: 'market_index')
"""



import numpy as np
import pandas as pd
from typing import Dict, List, Optional, Union, Tuple, Callable
from scipy import optimize, stats
from scipy.linalg import sqrtm, inv, LinAlgError
import warnings

from config import (
    MathConstants, PerformanceMetric, RiskMetric,
    validate_weights, validate_returns, validate_covariance_matrix,
    ERROR_MESSAGES
)


class StatisticalCalculations:
    """Core statistical calculations for financial analysis"""

    @staticmethod
    def calculate_mean(data: Union[np.ndarray, pd.Series]) -> float:
        """Calculate arithmetic mean"""
        return np.mean(data)

    @staticmethod
    def calculate_variance(data: Union[np.ndarray, pd.Series], ddof: int = 1) -> float:
        """Calculate variance with degrees of freedom correction"""
        return np.var(data, ddof=ddof)

    @staticmethod
    def calculate_std(data: Union[np.ndarray, pd.Series], ddof: int = 1) -> float:
        """Calculate standard deviation"""
        return np.std(data, ddof=ddof)

    @staticmethod
    def calculate_covariance(x: Union[np.ndarray, pd.Series],
                             y: Union[np.ndarray, pd.Series], ddof: int = 1) -> float:
        """Calculate covariance between two series"""
        return np.cov(x, y, ddof=ddof)[0, 1]

    @staticmethod
    def calculate_correlation(x: Union[np.ndarray, pd.Series],
                              y: Union[np.ndarray, pd.Series]) -> float:
        """Calculate correlation coefficient"""
        return np.corrcoef(x, y)[0, 1]

    @staticmethod
    def calculate_beta(asset_returns: Union[np.ndarray, pd.Series],
                       market_returns: Union[np.ndarray, pd.Series]) -> float:
        """Calculate beta coefficient"""
        covariance = StatisticalCalculations.calculate_covariance(asset_returns, market_returns)
        market_variance = StatisticalCalculations.calculate_variance(market_returns)
        return covariance / market_variance

    @staticmethod
    def calculate_tracking_error(portfolio_returns: Union[np.ndarray, pd.Series],
                                 benchmark_returns: Union[np.ndarray, pd.Series],
                                 annualize: bool = True) -> float:
        """Calculate tracking error"""
        excess_returns = np.array(portfolio_returns) - np.array(benchmark_returns)
        tracking_error = np.std(excess_returns, ddof=1)

        if annualize:
            tracking_error *= MathConstants.SQRT_TRADING_DAYS

        return tracking_error

    @staticmethod
    def calculate_downside_deviation(returns: Union[np.ndarray, pd.Series],
                                     target_return: float = 0.0,
                                     annualize: bool = True) -> float:
        """Calculate downside deviation"""
        returns_array = np.array(returns)
        downside_returns = returns_array[returns_array < target_return]

        if len(downside_returns) == 0:
            return 0.0

        downside_dev = np.sqrt(np.mean((downside_returns - target_return) ** 2))

        if annualize:
            downside_dev *= MathConstants.SQRT_TRADING_DAYS

        return downside_dev


class PortfolioMath:
    """Portfolio mathematics and optimization"""

    @staticmethod
    def calculate_portfolio_return(weights: np.ndarray, expected_returns: np.ndarray) -> float:
        """Calculate expected portfolio return"""
        if not validate_weights(weights):
            raise ValueError(ERROR_MESSAGES["invalid_weights"])
        return np.dot(weights, expected_returns)

    @staticmethod
    def calculate_portfolio_variance(weights: np.ndarray, cov_matrix: np.ndarray) -> float:
        """Calculate portfolio variance"""
        if not validate_weights(weights):
            raise ValueError(ERROR_MESSAGES["invalid_weights"])
        if not validate_covariance_matrix(cov_matrix):
            raise ValueError(ERROR_MESSAGES["singular_matrix"])

        variance = np.dot(weights.T, np.dot(cov_matrix, weights))
        if variance < 0:
            raise ValueError(ERROR_MESSAGES["negative_variance"])
        return variance

    @staticmethod
    def calculate_portfolio_std(weights: np.ndarray, cov_matrix: np.ndarray) -> float:
        """Calculate portfolio standard deviation"""
        variance = PortfolioMath.calculate_portfolio_variance(weights, cov_matrix)
        return np.sqrt(variance)

    @staticmethod
    def calculate_diversification_ratio(weights: np.ndarray,
                                        individual_stds: np.ndarray,
                                        portfolio_std: float) -> float:
        """Calculate diversification ratio"""
        weighted_avg_std = np.dot(weights, individual_stds)
        return weighted_avg_std / portfolio_std

    @staticmethod
    def find_minimum_variance_portfolio(cov_matrix: np.ndarray,
                                        constraints: Optional[Dict] = None) -> Dict:
        """Find global minimum variance portfolio"""
        n = cov_matrix.shape[0]

        # Objective function: minimize portfolio variance
        def objective(weights):
            return PortfolioMath.calculate_portfolio_variance(weights, cov_matrix)

        # Constraints
        cons = [{'type': 'eq', 'fun': lambda w: np.sum(w) - 1.0}]  # weights sum to 1

        if constraints:
            if 'min_weight' in constraints:
                cons.append({'type': 'ineq', 'fun': lambda w: w - constraints['min_weight']})
            if 'max_weight' in constraints:
                cons.append({'type': 'ineq', 'fun': lambda w: constraints['max_weight'] - w})

        # Bounds
        bounds = tuple((0, 1) for _ in range(n))

        # Initial guess
        x0 = np.ones(n) / n

        # Optimize
        result = optimize.minimize(objective, x0, method='SLSQP',
                                   bounds=bounds, constraints=cons,
                                   options={'maxiter': MathConstants.MAX_ITERATIONS})

        if not result.success:
            raise ValueError(ERROR_MESSAGES["optimization_failed"])

        optimal_weights = result.x
        min_variance = result.fun
        min_std = np.sqrt(min_variance)

        return {
            'weights': optimal_weights,
            'variance': min_variance,
            'std': min_std,
            'optimization_result': result
        }


class PerformanceCalculations:
    """Performance and risk-adjusted return calculations"""

    @staticmethod
    def sharpe_ratio(returns: Union[np.ndarray, pd.Series],
                     risk_free_rate: float = MathConstants.DEFAULT_RISK_FREE_RATE,
                     annualize: bool = True) -> float:
        """Calculate Sharpe ratio"""
        if not validate_returns(returns):
            raise ValueError(ERROR_MESSAGES["insufficient_data"])

        returns_array = np.array(returns)
        excess_returns = returns_array - risk_free_rate / MathConstants.TRADING_DAYS_YEAR

        mean_excess = np.mean(excess_returns)
        std_excess = np.std(excess_returns, ddof=1)

        if std_excess == 0:
            return 0.0

        sharpe = mean_excess / std_excess

        if annualize:
            sharpe *= MathConstants.SQRT_TRADING_DAYS

        return sharpe

    @staticmethod
    def treynor_ratio(returns: Union[np.ndarray, pd.Series],
                      beta: float,
                      risk_free_rate: float = MathConstants.DEFAULT_RISK_FREE_RATE,
                      annualize: bool = True) -> float:
        """Calculate Treynor ratio"""
        if not validate_returns(returns):
            raise ValueError(ERROR_MESSAGES["insufficient_data"])

        returns_array = np.array(returns)
        mean_return = np.mean(returns_array)

        if annualize:
            mean_return *= MathConstants.TRADING_DAYS_YEAR
            risk_free_rate_annual = risk_free_rate
        else:
            risk_free_rate_annual = risk_free_rate / MathConstants.TRADING_DAYS_YEAR

        if beta == 0:
            return 0.0

        return (mean_return - risk_free_rate_annual) / beta

    @staticmethod
    def information_ratio(portfolio_returns: Union[np.ndarray, pd.Series],
                          benchmark_returns: Union[np.ndarray, pd.Series]) -> float:
        """Calculate information ratio"""
        excess_returns = np.array(portfolio_returns) - np.array(benchmark_returns)

        if len(excess_returns) < 2:
            raise ValueError(ERROR_MESSAGES["insufficient_data"])

        mean_excess = np.mean(excess_returns)
        std_excess = np.std(excess_returns, ddof=1)

        if std_excess == 0:
            return 0.0

        return mean_excess / std_excess * MathConstants.SQRT_TRADING_DAYS

    @staticmethod
    def jensen_alpha(portfolio_returns: Union[np.ndarray, pd.Series],
                     market_returns: Union[np.ndarray, pd.Series],
                     beta: float,
                     risk_free_rate: float = MathConstants.DEFAULT_RISK_FREE_RATE) -> float:
        """Calculate Jensen's alpha"""
        portfolio_mean = np.mean(portfolio_returns) * MathConstants.TRADING_DAYS_YEAR
        market_mean = np.mean(market_returns) * MathConstants.TRADING_DAYS_YEAR

        expected_return = risk_free_rate + beta * (market_mean - risk_free_rate)
        alpha = portfolio_mean - expected_return

        return alpha

    @staticmethod
    def m_squared(portfolio_returns: Union[np.ndarray, pd.Series],
                  market_returns: Union[np.ndarray, pd.Series],
                  risk_free_rate: float = MathConstants.DEFAULT_RISK_FREE_RATE) -> float:
        """Calculate M-squared (M²) measure"""
        portfolio_sharpe = PerformanceCalculations.sharpe_ratio(portfolio_returns, risk_free_rate)
        market_sharpe = PerformanceCalculations.sharpe_ratio(market_returns, risk_free_rate)
        market_std = np.std(market_returns, ddof=1) * MathConstants.SQRT_TRADING_DAYS

        return (portfolio_sharpe - market_sharpe) * market_std

    @staticmethod
    def sortino_ratio(returns: Union[np.ndarray, pd.Series],
                      target_return: float = 0.0,
                      risk_free_rate: float = MathConstants.DEFAULT_RISK_FREE_RATE,
                      annualize: bool = True) -> float:
        """Calculate Sortino ratio"""
        returns_array = np.array(returns)
        excess_returns = returns_array - risk_free_rate / MathConstants.TRADING_DAYS_YEAR
        mean_excess = np.mean(excess_returns)

        downside_dev = StatisticalCalculations.calculate_downside_deviation(
            returns_array, target_return, annualize=False
        )

        if downside_dev == 0:
            return 0.0

        sortino = mean_excess / downside_dev

        if annualize:
            sortino *= MathConstants.SQRT_TRADING_DAYS

        return sortino


class RiskCalculations:
    """Risk measurement and VaR calculations"""

    @staticmethod
    def value_at_risk_parametric(returns: Union[np.ndarray, pd.Series],
                                 confidence_level: float = 0.95,
                                 holding_period: int = 1) -> float:
        """Calculate parametric VaR"""
        returns_array = np.array(returns)
        mean_return = np.mean(returns_array)
        std_return = np.std(returns_array, ddof=1)

        # Z-score for confidence level
        z_score = stats.norm.ppf(1 - confidence_level)

        # VaR calculation
        var = -(mean_return + z_score * std_return) * np.sqrt(holding_period)

        return var

    @staticmethod
    def value_at_risk_historical(returns: Union[np.ndarray, pd.Series],
                                 confidence_level: float = 0.95,
                                 holding_period: int = 1) -> float:
        """Calculate historical simulation VaR"""
        returns_array = np.array(returns)

        # Scale for holding period
        if holding_period > 1:
            returns_array = returns_array * np.sqrt(holding_period)

        # Sort returns in ascending order
        sorted_returns = np.sort(returns_array)

        # Find percentile
        percentile = 1 - confidence_level
        var_index = int(np.ceil(len(sorted_returns) * percentile)) - 1
        var_index = max(0, min(var_index, len(sorted_returns) - 1))

        return -sorted_returns[var_index]

    @staticmethod
    def conditional_value_at_risk(returns: Union[np.ndarray, pd.Series],
                                  confidence_level: float = 0.95,
                                  method: str = "historical") -> float:
        """Calculate Conditional VaR (Expected Shortfall)"""
        returns_array = np.array(returns)

        if method == "historical":
            var = RiskCalculations.value_at_risk_historical(returns_array, confidence_level)
            tail_returns = returns_array[returns_array <= -var]
            return -np.mean(tail_returns) if len(tail_returns) > 0 else var
        else:
            var = RiskCalculations.value_at_risk_parametric(returns_array, confidence_level)
            # For parametric method, use analytical formula
            z_score = stats.norm.ppf(1 - confidence_level)
            mean_return = np.mean(returns_array)
            std_return = np.std(returns_array, ddof=1)

            cvar = -(mean_return - std_return * stats.norm.pdf(z_score) / (1 - confidence_level))
            return cvar


class OptimizationEngine:
    """Portfolio optimization algorithms"""

    @staticmethod
    def efficient_frontier(expected_returns: np.ndarray,
                           cov_matrix: np.ndarray,
                           num_points: int = 100,
                           constraints: Optional[Dict] = None) -> Dict:
        """Generate efficient frontier"""
        n_assets = len(expected_returns)

        # Find minimum and maximum return portfolios
        min_var_portfolio = PortfolioMath.find_minimum_variance_portfolio(cov_matrix, constraints)
        min_return = PortfolioMath.calculate_portfolio_return(min_var_portfolio['weights'], expected_returns)

        # Maximum return portfolio (highest expected return asset)
        max_return = np.max(expected_returns)

        # Generate target returns
        target_returns = np.linspace(min_return, max_return, num_points)

        frontier_weights = []
        frontier_returns = []
        frontier_stds = []
        frontier_sharpe = []

        for target_return in target_returns:
            try:
                # Optimize for target return
                def objective(weights):
                    return PortfolioMath.calculate_portfolio_variance(weights, cov_matrix)

                # Constraints
                cons = [
                    {'type': 'eq', 'fun': lambda w: np.sum(w) - 1.0},  # weights sum to 1
                    {'type': 'eq', 'fun': lambda w, tr=target_return:
                    PortfolioMath.calculate_portfolio_return(w, expected_returns) - tr}
                ]

                if constraints:
                    if 'min_weight' in constraints:
                        cons.append({'type': 'ineq', 'fun': lambda w: w - constraints['min_weight']})
                    if 'max_weight' in constraints:
                        cons.append({'type': 'ineq', 'fun': lambda w: constraints['max_weight'] - w})

                bounds = tuple((0, 1) for _ in range(n_assets))
                x0 = np.ones(n_assets) / n_assets

                result = optimize.minimize(objective, x0, method='SLSQP',
                                           bounds=bounds, constraints=cons,
                                           options={'maxiter': MathConstants.MAX_ITERATIONS})

                if result.success:
                    weights = result.x
                    portfolio_return = PortfolioMath.calculate_portfolio_return(weights, expected_returns)
                    portfolio_std = PortfolioMath.calculate_portfolio_std(weights, cov_matrix)
                    sharpe = (portfolio_return - MathConstants.DEFAULT_RISK_FREE_RATE) / portfolio_std

                    frontier_weights.append(weights)
                    frontier_returns.append(portfolio_return)
                    frontier_stds.append(portfolio_std)
                    frontier_sharpe.append(sharpe)

            except:
                continue

        return {
            'returns': np.array(frontier_returns),
            'stds': np.array(frontier_stds),
            'weights': np.array(frontier_weights),
            'sharpe_ratios': np.array(frontier_sharpe),
            'min_var_portfolio': min_var_portfolio
        }

    @staticmethod
    def maximum_sharpe_portfolio(expected_returns: np.ndarray,
                                 cov_matrix: np.ndarray,
                                 risk_free_rate: float = MathConstants.DEFAULT_RISK_FREE_RATE,
                                 constraints: Optional[Dict] = None) -> Dict:
        """Find maximum Sharpe ratio portfolio"""
        n_assets = len(expected_returns)

        # Objective function: maximize Sharpe ratio (minimize negative Sharpe)
        def objective(weights):
            portfolio_return = PortfolioMath.calculate_portfolio_return(weights, expected_returns)
            portfolio_std = PortfolioMath.calculate_portfolio_std(weights, cov_matrix)

            if portfolio_std == 0:
                return -np.inf

            sharpe = (portfolio_return - risk_free_rate) / portfolio_std
            return -sharpe  # Minimize negative Sharpe

        # Constraints
        cons = [{'type': 'eq', 'fun': lambda w: np.sum(w) - 1.0}]

        if constraints:
            if 'min_weight' in constraints:
                cons.append({'type': 'ineq', 'fun': lambda w: w - constraints['min_weight']})
            if 'max_weight' in constraints:
                cons.append({'type': 'ineq', 'fun': lambda w: constraints['max_weight'] - w})

        bounds = tuple((0, 1) for _ in range(n_assets))
        x0 = np.ones(n_assets) / n_assets

        result = optimize.minimize(objective, x0, method='SLSQP',
                                   bounds=bounds, constraints=cons,
                                   options={'maxiter': MathConstants.MAX_ITERATIONS})

        if not result.success:
            raise ValueError(ERROR_MESSAGES["optimization_failed"])

        optimal_weights = result.x
        portfolio_return = PortfolioMath.calculate_portfolio_return(optimal_weights, expected_returns)
        portfolio_std = PortfolioMath.calculate_portfolio_std(optimal_weights, cov_matrix)
        sharpe = (portfolio_return - risk_free_rate) / portfolio_std

        return {
            'weights': optimal_weights,
            'expected_return': portfolio_return,
            'std': portfolio_std,
            'sharpe_ratio': sharpe,
            'optimization_result': result
        }