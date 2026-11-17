
"""Portfolio maths library for Analytics/portfolioManagement (no CLI; used by risk_management,
portfolio_analytics and active_management).

Conventions: simple per-period (daily) returns; annualised with 252 periods (mean x 252, stdev x
sqrt(252)); risk_free_rate is ANNUAL and converted with rf / 252; VaR / ES are POSITIVE loss
fractions at the given confidence level (0.95 -> the 5 % left tail). A ratio whose denominator is
zero is NaN (the CLIs print it as null), never a stand-in 0.
"""

from __future__ import annotations  # pandas only appears in annotations: not imported at run time

from typing import Dict, List, Optional, Union, Tuple, Callable

import numpy as np

from config import (
    MathConstants, PerformanceMetric, RiskMetric,
    validate_weights, validate_returns, validate_covariance_matrix,
    ERROR_MESSAGES
)


def _stats():
    from scipy import stats  # lazy: scipy costs ~0.5 s and most commands never need it
    return stats


def _optimize():
    from scipy import optimize
    return optimize


def _check_confidence(confidence_level: float) -> float:
    if not 0.0 < confidence_level < 1.0:
        raise ValueError(ERROR_MESSAGES["invalid_confidence_level"])
    return confidence_level


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
        """Calculate correlation coefficient (NaN when either series is constant)"""
        x, y = np.asarray(x, dtype=float), np.asarray(y, dtype=float)
        if np.std(x) == 0 or np.std(y) == 0:
            return float("nan")
        return np.corrcoef(x, y)[0, 1]

    @staticmethod
    def calculate_beta(asset_returns: Union[np.ndarray, pd.Series],
                       market_returns: Union[np.ndarray, pd.Series]) -> float:
        """Beta = cov(asset, market) / var(market), both with ddof=1 (NaN for a constant market)"""
        covariance = StatisticalCalculations.calculate_covariance(asset_returns, market_returns)
        market_variance = StatisticalCalculations.calculate_variance(market_returns)
        if market_variance == 0:
            return float("nan")
        return covariance / market_variance

    @staticmethod
    def calculate_tracking_error(portfolio_returns: Union[np.ndarray, pd.Series],
                                 benchmark_returns: Union[np.ndarray, pd.Series],
                                 annualize: bool = True) -> float:
        """Tracking error = stdev(portfolio - benchmark, ddof=1), x sqrt(252) when annualised"""
        excess_returns = np.array(portfolio_returns) - np.array(benchmark_returns)
        tracking_error = np.std(excess_returns, ddof=1)

        if annualize:
            tracking_error *= MathConstants.SQRT_TRADING_DAYS

        return tracking_error

    @staticmethod
    def calculate_downside_deviation(returns: Union[np.ndarray, pd.Series],
                                     target_return: float = 0.0,
                                     annualize: bool = True) -> float:
        """Target semideviation: sqrt(mean(min(r - target, 0)^2)) over ALL N observations.

        Periods above the target count as zero shortfall (they are NOT dropped - dividing by the
        number of below-target periods overstated the figure by sqrt(N / N_below)). target_return is
        per period. Annualised by x sqrt(252), the same square-root-of-time rule as volatility.
        """
        returns_array = np.asarray(returns, dtype=float)
        if returns_array.size == 0:
            return float("nan")

        shortfall = np.minimum(returns_array - target_return, 0.0)
        downside_dev = float(np.sqrt(np.mean(shortfall ** 2)))

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
        if variance < -1e-12:
            raise ValueError(ERROR_MESSAGES["negative_variance"])
        return max(float(variance), 0.0)

    @staticmethod
    def calculate_portfolio_std(weights: np.ndarray, cov_matrix: np.ndarray) -> float:
        """Calculate portfolio standard deviation"""
        variance = PortfolioMath.calculate_portfolio_variance(weights, cov_matrix)
        return np.sqrt(variance)

    @staticmethod
    def calculate_diversification_ratio(weights: np.ndarray,
                                        individual_stds: np.ndarray,
                                        portfolio_std: float) -> float:
        """Diversification ratio = (w . sigma_i) / sigma_p (NaN for a zero-risk portfolio)"""
        weighted_avg_std = np.dot(weights, individual_stds)
        if portfolio_std == 0:
            return float("nan")
        return weighted_avg_std / portfolio_std

    @staticmethod
    def find_minimum_variance_portfolio(cov_matrix: np.ndarray,
                                        constraints: Optional[Dict] = None) -> Dict:
        """Long-only global minimum-variance portfolio (weights sum to 1, optional min/max weight)"""
        cov_matrix = np.atleast_2d(np.asarray(cov_matrix, dtype=float))
        n = cov_matrix.shape[0]
        lo, hi = 0.0, 1.0
        if constraints:
            lo = max(lo, float(constraints.get('min_weight', lo)))
            hi = min(hi, float(constraints.get('max_weight', hi)))

        # Raw quadratic form: the validating helpers raise on the slightly infeasible iterates
        # SLSQP is allowed to visit, which used to abort the optimisation.
        def objective(weights):
            return float(weights @ cov_matrix @ weights)

        cons = [{'type': 'eq', 'fun': lambda w: np.sum(w) - 1.0}]
        bounds = tuple((lo, hi) for _ in range(n))
        x0 = np.ones(n) / n

        result = _optimize().minimize(objective, x0, method='SLSQP',
                                      bounds=bounds, constraints=cons,
                                      options={'maxiter': MathConstants.MAX_ITERATIONS, 'ftol': 1e-12})

        if not result.success:
            raise ValueError(ERROR_MESSAGES["optimization_failed"])

        optimal_weights = np.clip(result.x, lo, hi)
        optimal_weights = optimal_weights / optimal_weights.sum()
        min_variance = float(optimal_weights @ cov_matrix @ optimal_weights)

        return {
            'weights': optimal_weights,
            'variance': min_variance,
            'std': float(np.sqrt(max(min_variance, 0.0))),
            'converged': bool(result.success),
        }


class PerformanceCalculations:
    """Performance and risk-adjusted return calculations"""

    @staticmethod
    def sharpe_ratio(returns: Union[np.ndarray, pd.Series],
                     risk_free_rate: float = MathConstants.DEFAULT_RISK_FREE_RATE,
                     annualize: bool = True) -> float:
        """Sharpe = mean(r - rf/252) / stdev(r - rf/252, ddof=1), x sqrt(252) when annualised"""
        if not validate_returns(returns):
            raise ValueError(ERROR_MESSAGES["insufficient_data"])

        returns_array = np.array(returns, dtype=float)
        excess_returns = returns_array - risk_free_rate / MathConstants.TRADING_DAYS_YEAR

        mean_excess = np.mean(excess_returns)
        std_excess = np.std(excess_returns, ddof=1)

        if std_excess == 0:
            return float("nan")

        sharpe = mean_excess / std_excess

        if annualize:
            sharpe *= MathConstants.SQRT_TRADING_DAYS

        return sharpe

    @staticmethod
    def treynor_ratio(returns: Union[np.ndarray, pd.Series],
                      beta: float,
                      risk_free_rate: float = MathConstants.DEFAULT_RISK_FREE_RATE,
                      annualize: bool = True) -> float:
        """Treynor = (mean return - rf) / beta (annualised arithmetic mean x 252 when annualize)"""
        if not validate_returns(returns):
            raise ValueError(ERROR_MESSAGES["insufficient_data"])

        returns_array = np.array(returns, dtype=float)
        mean_return = np.mean(returns_array)

        if annualize:
            mean_return *= MathConstants.TRADING_DAYS_YEAR
            risk_free_rate_annual = risk_free_rate
        else:
            risk_free_rate_annual = risk_free_rate / MathConstants.TRADING_DAYS_YEAR

        if beta == 0 or not np.isfinite(beta):
            return float("nan")

        return (mean_return - risk_free_rate_annual) / beta

    @staticmethod
    def information_ratio(portfolio_returns: Union[np.ndarray, pd.Series],
                          benchmark_returns: Union[np.ndarray, pd.Series]) -> float:
        """IR = mean(active) / stdev(active, ddof=1) x sqrt(252)"""
        excess_returns = np.array(portfolio_returns) - np.array(benchmark_returns)

        if len(excess_returns) < 2:
            raise ValueError(ERROR_MESSAGES["insufficient_data"])

        mean_excess = np.mean(excess_returns)
        std_excess = np.std(excess_returns, ddof=1)

        if std_excess == 0:
            return float("nan")

        return mean_excess / std_excess * MathConstants.SQRT_TRADING_DAYS

    @staticmethod
    def jensen_alpha(portfolio_returns: Union[np.ndarray, pd.Series],
                     market_returns: Union[np.ndarray, pd.Series],
                     beta: float,
                     risk_free_rate: float = MathConstants.DEFAULT_RISK_FREE_RATE) -> float:
        """Jensen's alpha = Rp - [rf + beta (Rm - rf)] with arithmetic means x 252 (daily input)"""
        portfolio_mean = np.mean(portfolio_returns) * MathConstants.TRADING_DAYS_YEAR
        market_mean = np.mean(market_returns) * MathConstants.TRADING_DAYS_YEAR

        expected_return = risk_free_rate + beta * (market_mean - risk_free_rate)
        alpha = portfolio_mean - expected_return

        return alpha

    @staticmethod
    def m_squared(portfolio_returns: Union[np.ndarray, pd.Series],
                  market_returns: Union[np.ndarray, pd.Series],
                  risk_free_rate: float = MathConstants.DEFAULT_RISK_FREE_RATE) -> float:
        """M-squared EXCESS over the market ("M2 alpha") = (Sharpe_p - Sharpe_m) x sigma_m (annual).

        The M2 return itself is rf + Sharpe_p x sigma_m = this value + the market's mean return.
        """
        portfolio_sharpe = PerformanceCalculations.sharpe_ratio(portfolio_returns, risk_free_rate)
        market_sharpe = PerformanceCalculations.sharpe_ratio(market_returns, risk_free_rate)
        market_std = np.std(market_returns, ddof=1) * MathConstants.SQRT_TRADING_DAYS

        return (portfolio_sharpe - market_sharpe) * market_std

    @staticmethod
    def sortino_ratio(returns: Union[np.ndarray, pd.Series],
                      target_return: Optional[float] = None,
                      risk_free_rate: float = MathConstants.DEFAULT_RISK_FREE_RATE,
                      annualize: bool = True) -> float:
        """Sortino = mean(r - MAR) / target semideviation at the same MAR, x sqrt(252).

        MAR (per period) = target_return when given, else risk_free_rate / 252. The numerator and the
        downside deviation now use the SAME threshold (they used rf and 0 respectively).
        """
        returns_array = np.array(returns, dtype=float)
        mar = target_return if target_return is not None else risk_free_rate / MathConstants.TRADING_DAYS_YEAR
        mean_excess = np.mean(returns_array - mar)

        downside_dev = StatisticalCalculations.calculate_downside_deviation(
            returns_array, mar, annualize=False
        )

        if not downside_dev or not np.isfinite(downside_dev):
            return float("nan")

        sortino = mean_excess / downside_dev

        if annualize:
            sortino *= MathConstants.SQRT_TRADING_DAYS

        return sortino


class RiskCalculations:
    """Risk measurement and VaR calculations (positive numbers are losses)"""

    @staticmethod
    def value_at_risk_parametric(returns: Union[np.ndarray, pd.Series],
                                 confidence_level: float = 0.95,
                                 holding_period: int = 1) -> float:
        """Normal VaR over h periods: -(mu h + z sigma sqrt(h)), z = Phi^-1(1 - confidence).

        The mean scales with h and the volatility with sqrt(h) (the mean used to be scaled by
        sqrt(h) as well).
        """
        _check_confidence(confidence_level)
        returns_array = np.array(returns, dtype=float)
        mean_return = np.mean(returns_array)
        std_return = np.std(returns_array, ddof=1)

        z_score = _stats().norm.ppf(1 - confidence_level)

        return -(mean_return * holding_period + z_score * std_return * np.sqrt(holding_period))

    @staticmethod
    def value_at_risk_historical(returns: Union[np.ndarray, pd.Series],
                                 confidence_level: float = 0.95,
                                 holding_period: int = 1) -> float:
        """Historical VaR = minus the k-th worst return, k = ceil(N x (1 - confidence)).

        That is the lower empirical quantile (inverse of the empirical CDF): N=100 at 95 % -> the 5th
        worst. N x (1 - c) is rounded to 9 decimals first: 1 - 0.95 is 0.0500000000000000444 in
        floating point, which made ceil() pick the 6th worst. holding_period > 1 applies the
        square-root-of-time rule to the 1-period figure.
        """
        _check_confidence(confidence_level)
        returns_array = np.array(returns, dtype=float)
        if returns_array.size == 0:
            return float("nan")

        if holding_period > 1:
            returns_array = returns_array * np.sqrt(holding_period)

        sorted_returns = np.sort(returns_array)
        n_tail = int(np.ceil(round(len(sorted_returns) * (1 - confidence_level), 9)))
        var_index = max(0, min(n_tail - 1, len(sorted_returns) - 1))

        return -sorted_returns[var_index]

    @staticmethod
    def conditional_value_at_risk(returns: Union[np.ndarray, pd.Series],
                                  confidence_level: float = 0.95,
                                  method: str = "historical") -> float:
        """Expected shortfall (CVaR): mean loss in the tail beyond VaR.

        historical: -mean(returns <= -VaR_hist); parametric (normal):
        -(mu - sigma phi(z) / (1 - c)), z = Phi^-1(1 - c).
        """
        _check_confidence(confidence_level)
        returns_array = np.array(returns, dtype=float)

        if method == "historical":
            var = RiskCalculations.value_at_risk_historical(returns_array, confidence_level)
            tail_returns = returns_array[returns_array <= -var]
            return -np.mean(tail_returns) if len(tail_returns) > 0 else var
        else:
            stats = _stats()
            z_score = stats.norm.ppf(1 - confidence_level)
            mean_return = np.mean(returns_array)
            std_return = np.std(returns_array, ddof=1)

            return -(mean_return - std_return * stats.norm.pdf(z_score) / (1 - confidence_level))


class OptimizationEngine:
    """Portfolio optimization algorithms (long-only, fully invested)"""

    @staticmethod
    def efficient_frontier(expected_returns: np.ndarray,
                           cov_matrix: np.ndarray,
                           num_points: int = 100,
                           constraints: Optional[Dict] = None) -> Dict:
        """Minimum-variance portfolios for targets between the min-variance and max-return points"""
        expected_returns = np.asarray(expected_returns, dtype=float)
        cov_matrix = np.atleast_2d(np.asarray(cov_matrix, dtype=float))
        n_assets = len(expected_returns)
        optimize = _optimize()
        lo, hi = 0.0, 1.0
        if constraints:
            lo = max(lo, float(constraints.get('min_weight', lo)))
            hi = min(hi, float(constraints.get('max_weight', hi)))
        bounds = tuple((lo, hi) for _ in range(n_assets))
        x0 = np.ones(n_assets) / n_assets
        budget = {'type': 'eq', 'fun': lambda w: np.sum(w) - 1.0}

        min_var_portfolio = PortfolioMath.find_minimum_variance_portfolio(cov_matrix, constraints)
        min_return = float(min_var_portfolio['weights'] @ expected_returns)

        # Highest return reachable under the same bounds (not simply max(mu), which a weight cap
        # can make infeasible).
        max_res = optimize.minimize(lambda w: -float(w @ expected_returns), x0, method='SLSQP',
                                    bounds=bounds, constraints=[budget],
                                    options={'maxiter': MathConstants.MAX_ITERATIONS})
        max_return = float(max_res.x @ expected_returns) if max_res.success else float(np.max(expected_returns))

        frontier_weights = []
        frontier_returns = []
        frontier_stds = []
        frontier_sharpe = []

        if max_return <= min_return + 1e-12:
            target_returns = np.array([min_return])
        else:
            target_returns = np.linspace(min_return, max_return, max(int(num_points), 2))

        def objective(weights):
            return float(weights @ cov_matrix @ weights)

        for target_return in target_returns:
            try:
                cons = [budget, {'type': 'eq', 'fun': lambda w, tr=target_return: float(w @ expected_returns) - tr}]
                result = optimize.minimize(objective, x0, method='SLSQP',
                                           bounds=bounds, constraints=cons,
                                           options={'maxiter': MathConstants.MAX_ITERATIONS})
                if not result.success:
                    continue
                weights = np.clip(result.x, lo, hi)
                weights = weights / weights.sum()
                portfolio_return = float(weights @ expected_returns)
                portfolio_std = float(np.sqrt(max(weights @ cov_matrix @ weights, 0.0)))
                sharpe = ((portfolio_return - MathConstants.DEFAULT_RISK_FREE_RATE) / portfolio_std
                          if portfolio_std > 0 else float("nan"))

                frontier_weights.append(weights)
                frontier_returns.append(portfolio_return)
                frontier_stds.append(portfolio_std)
                frontier_sharpe.append(sharpe)
            except (ValueError, ArithmeticError):
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
        """Long-only tangency portfolio: maximise (w.mu - rf) / sqrt(w' S w), weights sum to 1"""
        expected_returns = np.asarray(expected_returns, dtype=float)
        cov_matrix = np.atleast_2d(np.asarray(cov_matrix, dtype=float))
        n_assets = len(expected_returns)
        lo, hi = 0.0, 1.0
        if constraints:
            lo = max(lo, float(constraints.get('min_weight', lo)))
            hi = min(hi, float(constraints.get('max_weight', hi)))

        def objective(weights):
            portfolio_return = float(weights @ expected_returns)
            portfolio_std = float(np.sqrt(max(weights @ cov_matrix @ weights, 0.0)))
            if portfolio_std < 1e-12:
                return 0.0  # undefined Sharpe: neutral, never the "best" point (-inf used to be)
            return -(portfolio_return - risk_free_rate) / portfolio_std

        cons = [{'type': 'eq', 'fun': lambda w: np.sum(w) - 1.0}]
        bounds = tuple((lo, hi) for _ in range(n_assets))
        x0 = np.ones(n_assets) / n_assets

        result = _optimize().minimize(objective, x0, method='SLSQP',
                                      bounds=bounds, constraints=cons,
                                      options={'maxiter': MathConstants.MAX_ITERATIONS})

        if not result.success:
            raise ValueError(ERROR_MESSAGES["optimization_failed"])

        optimal_weights = np.clip(result.x, lo, hi)
        optimal_weights = optimal_weights / optimal_weights.sum()
        portfolio_return = float(optimal_weights @ expected_returns)
        portfolio_std = float(np.sqrt(max(optimal_weights @ cov_matrix @ optimal_weights, 0.0)))
        sharpe = (portfolio_return - risk_free_rate) / portfolio_std if portfolio_std > 0 else float("nan")

        return {
            'weights': optimal_weights,
            'expected_return': portfolio_return,
            'std': portfolio_std,
            'sharpe_ratio': sharpe,
            'converged': bool(result.success),
        }
