"""
Portfolio analytics: mean-variance statistics, efficient frontier, CAPM alpha / beta, systematic vs specific
risk, VaR / ES and drawdown for a multi-asset book.

CLI (one JSON object on stdout; {"error": ...} and exit 1 on failure). The params object is the JSON
argument, or stdin when no argument is given:
  comprehensive_analysis {"returns_data": {asset: [daily returns]}, "weights": [...], "market_returns": [...],
                          "risk_free_rate": 0.03, "frontier_points": 25}   (also positional: <returns> [w] [mkt])
  calculate_portfolio_metrics {"holdings": [{"symbol", "weight"}]}       1y of Yahoo prices, SPY as market
  tax_report {"holdings": [{"symbol", "quantity", "avg_price", "current_price", "purchase_date"}],
              "st_rate": 0.22, "lt_rate": 0.15}                          tax if everything were sold today
  pme_analysis {"symbols": [...], "weights": [...], "benchmark": "SPY"}  1y relative performance vs a benchmark
  allocation_analysis {"holdings": [{"symbol", "value" | "weight"}]}     concentration (HHI, top-N)
Returns are simple daily fractions; annualised with 252 periods; VaR / ES are positive 1-day loss fractions.
"""

import dataclasses
import sys

import numpy as np
from typing import Dict, List, Optional, Union, Tuple
import warnings

from config import (
    AssetClass, MathConstants, PortfolioParameters,
    DEFAULT_PORTFOLIO_PARAMS, validate_weights, validate_covariance_matrix, ERROR_MESSAGES,
    CliArgs, CliError, return_series, run_cli
)
from math_engine import (
    StatisticalCalculations, PortfolioMath, PerformanceCalculations,
    RiskCalculations, OptimizationEngine
)


def _fincept_yahoo():
    """fincept_yahoo (the shared Yahoo layer: bar cache, limiter); scripts/ is three levels up."""
    try:
        import fincept_yahoo
    except ImportError:
        import os
        sys.path.append(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))
        import fincept_yahoo
    return fincept_yahoo


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

        # Objective: maximize utility U = w'μ - (A/2)w'Σw (raw products: the validating helpers raise on
        # the slightly infeasible iterates SLSQP may visit)
        def objective(weights):
            portfolio_return = float(weights @ expected_returns)
            portfolio_variance = float(weights @ cov_matrix @ weights)
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
            weights = np.clip(result.x, 0.0, 1.0)
            weights = weights / weights.sum()
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
        """Split between the risk-free asset and the risky portfolio on the CAL.

        y = risky weight; return = rf + y (Rp - rf), std = y sigma_p. y is floored at 0 (no short
        position in the risky portfolio) and the reported return / std are recomputed from the
        floored y (they used to echo the unreachable target).
        """
        if target_return is not None:
            if risky_portfolio_return == risk_free_rate:
                raise ValueError("risky portfolio return equals the risk-free rate: no allocation reaches a target")
            y = (target_return - risk_free_rate) / (risky_portfolio_return - risk_free_rate)
        elif target_std is not None:
            if not risky_portfolio_std:
                raise ValueError("risky portfolio standard deviation must be positive")
            y = target_std / risky_portfolio_std
        else:
            raise ValueError("Either target_return or target_std must be specified")

        clipped = y < 0
        y = max(0.0, y)  # No short selling of the risky portfolio

        return {
            "risky_asset_weight": y,
            "risk_free_weight": 1 - y,
            "portfolio_return": risk_free_rate + y * (risky_portfolio_return - risk_free_rate),
            "portfolio_std": y * risky_portfolio_std,
            "leveraged": y > 1.0,
            "target_reachable": not clipped
        }


class SystematicRiskAnalysis:
    """Systematic vs. Nonsystematic risk analysis"""

    @staticmethod
    def decompose_risk(asset_returns: np.ndarray,
                       market_returns: np.ndarray) -> Dict:
        """Single-index decomposition: total var = beta^2 var_m + residual var.

        The *_variance / *_std fields are PER PERIOD (daily for daily input); *_std_annualized
        multiply the std by sqrt(252). systematic share = R^2.
        """
        beta = StatisticalCalculations.calculate_beta(asset_returns, market_returns)
        total_variance = StatisticalCalculations.calculate_variance(asset_returns)
        market_variance = StatisticalCalculations.calculate_variance(market_returns)

        if not np.isfinite(beta) or total_variance == 0:
            return {"beta": beta if np.isfinite(beta) else None, "total_variance": total_variance,
                    "note": "a constant series has no risk to decompose"}

        # Systematic variance = beta^2 * var_m ; nonsystematic = total - systematic
        systematic_variance = (beta ** 2) * market_variance
        nonsystematic_variance = max(0.0, total_variance - systematic_variance)

        correlation = StatisticalCalculations.calculate_correlation(asset_returns, market_returns)
        r_squared = correlation ** 2
        sqrt_ppy = MathConstants.SQRT_TRADING_DAYS

        return {
            "beta": beta,
            "units": "per period (daily for daily input); *_std_annualized = std x sqrt(252)",
            "total_variance": total_variance,
            "total_std": np.sqrt(total_variance),
            "total_std_annualized": np.sqrt(total_variance) * sqrt_ppy,
            "systematic_variance": systematic_variance,
            "systematic_std": np.sqrt(systematic_variance),
            "systematic_std_annualized": np.sqrt(systematic_variance) * sqrt_ppy,
            "nonsystematic_variance": nonsystematic_variance,
            "nonsystematic_std": np.sqrt(nonsystematic_variance),
            "nonsystematic_std_annualized": np.sqrt(nonsystematic_variance) * sqrt_ppy,
            "r_squared": r_squared,
            "systematic_risk_percentage": systematic_variance / total_variance * 100,
            "nonsystematic_risk_percentage": nonsystematic_variance / total_variance * 100
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
        """Check CAPM assumptions validity (Shapiro-Wilk normality; split-sample correlation stability)"""
        assumptions = {
            "homogeneous_expectations": "Cannot verify with historical data",
            "single_period_model": "Using historical multi-period data",
            "risk_free_borrowing_lending": "Assumed available",
            "no_transaction_costs": "Assumed",
            "normal_distribution": {},
            "constant_correlations": {}
        }

        from scipy import stats
        for asset, returns in returns_data.items():
            returns = np.asarray(returns, dtype=float)
            if len(returns) < 3 or np.std(returns) == 0:
                assumptions["normal_distribution"][asset] = {
                    "shapiro_test_p_value": None, "approximately_normal": None,
                    "note": "needs at least 3 non-constant observations"}
                continue
            shapiro_stat, shapiro_p = stats.shapiro(returns[:min(5000, len(returns))])
            assumptions["normal_distribution"][asset] = {
                "shapiro_test_p_value": shapiro_p,
                "approximately_normal": bool(shapiro_p > 0.05)
            }

        if len(returns_data) >= 2:
            assets = list(returns_data.keys())
            asset1_returns = np.asarray(returns_data[assets[0]], dtype=float)
            asset2_returns = np.asarray(returns_data[assets[1]], dtype=float)

            mid_point = len(asset1_returns) // 2
            corr1 = StatisticalCalculations.calculate_correlation(
                asset1_returns[:mid_point], asset2_returns[:mid_point]
            )
            corr2 = StatisticalCalculations.calculate_correlation(
                asset1_returns[mid_point:], asset2_returns[mid_point:]
            )
            both = bool(np.isfinite(corr1) and np.isfinite(corr2))

            assumptions["constant_correlations"] = {
                "first_half_correlation": corr1,
                "second_half_correlation": corr2,
                "correlation_difference": abs(corr1 - corr2) if both else None,
                "stable_correlations": bool(abs(corr1 - corr2) < 0.2) if both else None
            }

        return assumptions


class EfficientFrontierAnalysis:
    """Efficient frontier construction and analysis (long-only, fully invested)"""

    def __init__(self, expected_returns: np.ndarray, cov_matrix: np.ndarray,
                 parameters: PortfolioParameters = DEFAULT_PORTFOLIO_PARAMS):
        self.expected_returns = np.asarray(expected_returns, dtype=float)
        self.cov_matrix = np.atleast_2d(np.asarray(cov_matrix, dtype=float))
        self.parameters = parameters
        self.n_assets = len(self.expected_returns)

        if not validate_covariance_matrix(self.cov_matrix):
            raise ValueError(ERROR_MESSAGES["singular_matrix"])

    def _bounds(self) -> Dict:
        return {'min_weight': self.parameters.min_weight, 'max_weight': self.parameters.max_weight}

    def generate_frontier(self) -> Dict:
        """Generate complete efficient frontier"""
        min_var_result = PortfolioMath.find_minimum_variance_portfolio(self.cov_matrix, self._bounds())

        frontier_result = OptimizationEngine.efficient_frontier(
            self.expected_returns,
            self.cov_matrix,
            self.parameters.num_frontier_points,
            self._bounds()
        )

        max_sharpe_result = OptimizationEngine.maximum_sharpe_portfolio(
            self.expected_returns,
            self.cov_matrix,
            self.parameters.risk_free_rate,
            self._bounds()
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

    def _calculate_cml(self, max_sharpe_portfolio: Dict) -> Optional[Dict]:
        """Calculate Capital Market Line parameters"""
        if not max_sharpe_portfolio["std"]:
            return None
        return CapitalAllocationLine.calculate_cal(
            max_sharpe_portfolio["expected_return"],
            max_sharpe_portfolio["std"],
            self.parameters.risk_free_rate
        )

    def portfolio_on_frontier(self, target_return: float) -> Dict:
        """Minimum-variance portfolio with exactly the target return"""
        from scipy import optimize

        cov, mu = self.cov_matrix, self.expected_returns
        cons = [
            {'type': 'eq', 'fun': lambda w: np.sum(w) - 1.0},
            {'type': 'eq', 'fun': lambda w: float(w @ mu) - target_return}
        ]

        bounds = tuple((self.parameters.min_weight, self.parameters.max_weight)
                       for _ in range(self.n_assets))
        x0 = np.ones(self.n_assets) / self.n_assets

        result = optimize.minimize(lambda w: float(w @ cov @ w), x0, method='SLSQP',
                                   bounds=bounds, constraints=cons)

        if result.success:
            weights = result.x
            portfolio_std = float(np.sqrt(max(weights @ cov @ weights, 0.0)))
            sharpe_ratio = ((target_return - self.parameters.risk_free_rate) / portfolio_std
                            if portfolio_std > 0 else float("nan"))

            return {
                "weights": weights,
                "expected_return": target_return,
                "standard_deviation": portfolio_std,
                "sharpe_ratio": sharpe_ratio,
                "on_efficient_frontier": True
            }
        else:
            return {"on_efficient_frontier": False, "error": "Optimization failed (target not reachable?)"}


class CorrelationEffectsAnalysis:
    """Analysis of correlation effects on portfolio risk"""

    @staticmethod
    def correlation_impact(weights: np.ndarray,
                           individual_stds: np.ndarray,
                           correlation_matrix: np.ndarray) -> Dict:
        """Portfolio std under the actual, perfect (rho = 1) and zero correlations.

        An undefined correlation (an asset with zero variance) does not affect any std - its
        covariance terms are zero - so it is treated as 0 in the arithmetic and left out of the
        average correlation.
        """
        weights = np.asarray(weights, dtype=float)
        individual_stds = np.asarray(individual_stds, dtype=float)
        corr = np.atleast_2d(np.asarray(correlation_matrix, dtype=float))
        corr_calc = np.nan_to_num(corr, nan=0.0)
        np.fill_diagonal(corr_calc, 1.0)
        outer = np.outer(individual_stds, individual_stds)

        def pstd(c):
            return float(np.sqrt(max(weights @ (outer * c) @ weights, 0.0)))

        actual_portfolio_std = pstd(corr_calc)
        perfect_corr_std = pstd(np.ones_like(corr_calc))
        zero_corr_std = pstd(np.eye(len(corr_calc)))

        weighted_avg_std = float(np.dot(weights, individual_stds))
        upper = corr[np.triu_indices(len(corr), k=1)]
        upper = upper[np.isfinite(upper)]

        def share(a, b):
            return (a - b) / a if a > 0 else float("nan")

        return {
            "actual_portfolio_std": actual_portfolio_std,
            "perfect_correlation_std": perfect_corr_std,
            "zero_correlation_std": zero_corr_std,
            "weighted_average_std": weighted_avg_std,
            "diversification_ratio": (weighted_avg_std / actual_portfolio_std
                                      if actual_portfolio_std > 0 else float("nan")),
            "risk_reduction_vs_perfect_corr": share(perfect_corr_std, actual_portfolio_std),
            "risk_reduction_vs_weighted_avg": share(weighted_avg_std, actual_portfolio_std),
            "average_correlation": float(np.mean(upper)) if upper.size else None
        }

    @staticmethod
    def optimal_correlation_for_risk_target(weights: np.ndarray,
                                            individual_stds: np.ndarray,
                                            target_portfolio_std: float) -> float:
        """Find the common pairwise correlation that gives the target portfolio standard deviation"""

        def objective(avg_correlation):
            n = len(weights)
            corr_matrix = np.full((n, n), avg_correlation)
            np.fill_diagonal(corr_matrix, 1.0)
            cov_matrix = np.outer(individual_stds, individual_stds) * corr_matrix
            portfolio_std = np.sqrt(max(weights @ cov_matrix @ weights, 0.0))
            return (portfolio_std - target_portfolio_std) ** 2

        from scipy import optimize

        result = optimize.minimize_scalar(objective, bounds=(-1, 1), method='bounded')

        return result.x if result.success else None


def _max_drawdown(returns: np.ndarray) -> Dict:
    """Drawdown from the running peak of wealth, starting wealth 1.0 included (a loss on the first
    observation counts). Indices count observations: 0 = before the first return."""
    wealth = np.concatenate(([1.0], np.cumprod(1.0 + np.asarray(returns, dtype=float))))
    drawdown = wealth / np.maximum.accumulate(wealth) - 1.0
    trough = int(np.argmin(drawdown))
    peak = int(np.argmax(wealth[:trough + 1]))
    back = np.nonzero(wealth[trough:] >= wealth[peak])[0]
    recovered = bool(trough > peak and back.size)
    return {
        "max_drawdown": float(drawdown.min()),
        "peak_index": peak,
        "trough_index": trough,
        "recovery_time": int(back[0]) if recovered else None,
        "recovered": recovered if trough > peak else None,
    }


class PortfolioAnalytics:
    """Main portfolio analytics interface"""

    def __init__(self, parameters: PortfolioParameters = DEFAULT_PORTFOLIO_PARAMS):
        self.parameters = parameters

    def comprehensive_analysis(self, returns_data: Dict[str, np.ndarray],
                               weights: Optional[np.ndarray] = None,
                               market_returns: Optional[np.ndarray] = None,
                               frontier_points: int = 25) -> Dict:
        """Perform comprehensive portfolio analysis on daily return series.

        Series of different lengths (and the market series) are aligned on their most recent common
        observations; weights are normalised to sum to 1 (so percent weights work).
        """
        notes: List[str] = []
        asset_names = list(returns_data.keys())
        if not asset_names:
            raise ValueError("returns_data is empty")
        series = [np.asarray(returns_data[a], dtype=float) for a in asset_names]
        n_obs = min(len(s) for s in series)
        if any(len(s) != n_obs for s in series):
            notes.append(f"series had different lengths; aligned on the most recent {n_obs} observations "
                         "(assumes they all end on the same date)")
        returns_matrix = np.column_stack([s[len(s) - n_obs:] for s in series])

        if market_returns is not None:
            market_returns = np.asarray(market_returns, dtype=float)
            n_common = min(n_obs, len(market_returns))
            if len(market_returns) != n_obs:
                notes.append(f"market_returns has {len(market_returns)} observations; aligned on the most "
                             f"recent {n_common}")
            returns_matrix = returns_matrix[len(returns_matrix) - n_common:]
            market_returns = market_returns[len(market_returns) - n_common:]
            n_obs = n_common
        if n_obs < 2:
            raise ValueError(f"At least 2 common observations are required (got {n_obs})")
        if n_obs < 30:
            notes.append(f"only {n_obs} observations: estimates are very uncertain")

        n_assets = len(asset_names)
        if weights is None:
            weights = np.ones(n_assets) / n_assets
        weights = np.asarray(weights, dtype=float).reshape(-1)
        if weights.size != n_assets:
            raise ValueError(f"weights has {weights.size} entries for {n_assets} assets")
        if weights.sum() <= 0:
            raise ValueError("weights must sum to a positive number")
        if abs(weights.sum() - 1.0) > 1e-6:
            notes.append(f"weights summed to {weights.sum():g}; normalised to 1")
            weights = weights / weights.sum()

        expected_returns = np.mean(returns_matrix, axis=0) * MathConstants.TRADING_DAYS_YEAR
        cov_matrix = np.atleast_2d(np.cov(returns_matrix, rowvar=False, ddof=1)) * MathConstants.TRADING_DAYS_YEAR
        individual_stds = np.sqrt(np.clip(np.diag(cov_matrix), 0.0, None))
        with np.errstate(divide="ignore", invalid="ignore"):
            corr_matrix = cov_matrix / np.outer(individual_stds, individual_stds)
        flat = individual_stds == 0
        corr_matrix[flat, :] = np.nan
        corr_matrix[:, flat] = np.nan
        np.fill_diagonal(corr_matrix, np.where(flat, np.nan, 1.0))
        if flat.any():
            notes.append("zero-variance series (correlations undefined, shown as null): "
                         + ", ".join(a for a, f in zip(asset_names, flat) if f))

        portfolio_series = returns_matrix @ weights
        portfolio_return = float(weights @ expected_returns)
        portfolio_std = float(np.sqrt(max(weights @ cov_matrix @ weights, 0.0)))
        portfolio_sharpe = PerformanceCalculations.sharpe_ratio(portfolio_series, self.parameters.risk_free_rate)

        results = {
            "observations": n_obs,
            "risk_free_rate": self.parameters.risk_free_rate,
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

        results["correlation_analysis"] = CorrelationEffectsAnalysis.correlation_impact(
            weights, individual_stds, corr_matrix
        )

        if n_assets < 2:
            results["efficient_frontier"] = None
            notes.append("efficient frontier needs at least 2 assets")
        else:
            try:
                params = dataclasses.replace(self.parameters, num_frontier_points=int(frontier_points))
                frontier_analyzer = EfficientFrontierAnalysis(expected_returns, cov_matrix, params)
                results["efficient_frontier"] = frontier_analyzer.generate_frontier()
            except ValueError as e:
                results["efficient_frontier"] = {"error": str(e)}

        if market_returns is not None:
            results["capm_analysis"] = CAPMAnalysis.alpha_calculation(
                portfolio_series, market_returns, self.parameters.risk_free_rate
            )
            individual_betas = [StatisticalCalculations.calculate_beta(returns_matrix[:, i], market_returns)
                                for i in range(n_assets)]
            results["individual_betas"] = dict(zip(asset_names, individual_betas))
            results["systematic_risk_analysis"] = SystematicRiskAnalysis.decompose_risk(
                portfolio_series, market_returns
            )

        results["risk_metrics"] = {
            "value_at_risk_95": RiskCalculations.value_at_risk_historical(portfolio_series, 0.95),
            "value_at_risk_99": RiskCalculations.value_at_risk_historical(portfolio_series, 0.99),
            "conditional_var_95": RiskCalculations.conditional_value_at_risk(portfolio_series, 0.95),
            "maximum_drawdown": self._calculate_max_drawdown(portfolio_series),
            "basis": "historical 1-day VaR / ES as positive loss fractions"
        }
        results["notes"] = notes

        return results

    def _calculate_max_drawdown(self, returns: np.ndarray) -> Dict:
        """Calculate maximum drawdown (see _max_drawdown)"""
        return _max_drawdown(returns)

# ============================================================================
# CLI Interface
# ============================================================================


def _rate(value, default: float, name: str) -> float:
    if value is None:
        return default
    try:
        out = float(value)
    except (TypeError, ValueError):
        raise CliError(f"{name} must be a number")
    if not 0.0 <= out < 1.0:
        raise CliError(f"{name} must be a fraction between 0 and 1 (e.g. 0.22)")
    return out


def cmd_calculate_portfolio_metrics(params):
    """Comprehensive analysis of holdings from 1y of Yahoo prices (SPY as the market).

    Input: {"holdings": [{"symbol": "AAPL", "weight": 0.5, ...}, ...]}
    One holding goes through the same analysis as several (it used to return a different contract).
    """
    try:
        fy = _fincept_yahoo()
        fy.yf()
        import pandas as pd
    except ImportError:
        return {"error": "yfinance not installed"}

    holdings = params.get("holdings", [])
    if not holdings:
        return {"error": "No holdings provided"}

    symbols = [str(h["symbol"]).strip().upper() for h in holdings if h.get("symbol")]
    if len(symbols) != len(holdings):
        return {"error": "every holding needs a symbol"}
    weights_arr = np.array([h.get("weight", 1.0 / len(holdings)) for h in holdings], dtype=float)
    if not np.all(np.isfinite(weights_arr)) or weights_arr.sum() <= 0:
        return {"error": "holding weights must be numbers that sum to a positive value"}
    weights_arr = weights_arr / weights_arr.sum()

    try:
        all_tickers = list(dict.fromkeys(symbols + ["SPY"]))
        data = fy.download(all_tickers, period="1y", auto_adjust=True)  # the shared bar cache
        if data is None or data.empty:
            return {"error": "No price data available from yfinance"}

        if isinstance(data.columns, pd.MultiIndex):
            close = data["Close"]
        else:
            close = data[["Close"]]
            close.columns = all_tickers[:1]
        if isinstance(close, pd.Series):
            close = close.to_frame(name=all_tickers[0])

        returns_all = close.pct_change(fill_method=None).dropna()
        if returns_all.empty:
            return {"error": "Insufficient return data after computing returns"}

        available = [s for s in symbols if s in returns_all.columns]
        if not available:
            return {"error": "No return data for portfolio symbols: " + ", ".join(symbols)}

        returns = returns_all[available]
        market = returns_all["SPY"].values if "SPY" in returns_all.columns else None

        w = weights_arr[[symbols.index(s) for s in available]]
        w = w / w.sum()

        returns_dict = {col: returns[col].values for col in returns.columns}
        result = PortfolioAnalytics().comprehensive_analysis(returns_dict, w, market)
        missing = [s for s in symbols if s not in available]
        if missing:
            result["notes"].append("no price history (left out): " + ", ".join(missing))
        return result

    except Exception as e:  # network / pandas failures come back as JSON
        import traceback
        traceback.print_exc(file=sys.stderr)
        return {"error": f"{type(e).__name__}: {e}"}


def cmd_tax_report(params):
    """Capital-gains tax if every position were sold today (unrealised gains), US-style.

    Holding period from purchase_date (YYYY-MM-DD; held more than 365 days = long-term; no date =
    treated as short-term and listed). Losses are netted against gains: within each term first,
    then a net loss in one term offsets a net gain in the other. Rates: st_rate / lt_rate
    (defaults 22 % / 15 %, stated in the output as assumptions).
    """
    from datetime import date, datetime as _dt

    holdings = params.get("holdings", [])
    if not holdings:
        return {"error": "No holdings provided"}
    st_rate = _rate(params.get("st_rate"), 0.22, "st_rate")
    lt_rate = _rate(params.get("lt_rate"), 0.15, "lt_rate")
    try:
        as_of = _dt.strptime(str(params["as_of"]), "%Y-%m-%d").date() if params.get("as_of") else date.today()
    except ValueError:
        return {"error": "as_of must be a date YYYY-MM-DD"}

    total_cost_basis = total_market_value = 0.0
    gross = {"st_gain": 0.0, "st_loss": 0.0, "lt_gain": 0.0, "lt_loss": 0.0}
    tax_loss_harvest, unknown_period = [], []

    for h in holdings:
        symbol = str(h.get("symbol", ""))
        try:
            qty = float(h.get("quantity", 0))
            avg_price = float(h.get("avg_price", 0))
            current = float(h.get("current_price", avg_price))
        except (TypeError, ValueError):
            return {"error": f"{symbol}: quantity, avg_price and current_price must be numbers"}
        if not all(np.isfinite([qty, avg_price, current])):
            return {"error": f"{symbol}: quantity, avg_price and current_price must be finite"}

        cost_basis = qty * avg_price
        market_val = qty * current
        gain = market_val - cost_basis
        total_cost_basis += cost_basis
        total_market_value += market_val

        long_term = False
        if h.get("purchase_date"):
            try:
                held = (as_of - _dt.strptime(str(h["purchase_date"])[:10], "%Y-%m-%d").date()).days
            except ValueError:
                return {"error": f"{symbol}: purchase_date must be YYYY-MM-DD"}
            long_term = held > 365
        else:
            unknown_period.append(symbol)
        term = "lt" if long_term else "st"
        if gain >= 0:
            gross[term + "_gain"] += gain
        else:
            gross[term + "_loss"] += gain
            tax_loss_harvest.append({"symbol": symbol, "unrealized_loss": round(gain, 2),
                                     "term": "long" if long_term else "short"})

    net_st = gross["st_gain"] + gross["st_loss"]
    net_lt = gross["lt_gain"] + gross["lt_loss"]
    if net_st < 0 < net_lt:
        offset = min(-net_st, net_lt)
        net_st, net_lt = net_st + offset, net_lt - offset
    elif net_lt < 0 < net_st:
        offset = min(-net_lt, net_st)
        net_st, net_lt = net_st - offset, net_lt + offset

    est_short_term_tax = max(net_st, 0.0) * st_rate
    est_long_term_tax = max(net_lt, 0.0) * lt_rate
    total_tax = est_short_term_tax + est_long_term_tax
    tax_without_losses = gross["st_gain"] * st_rate + gross["lt_gain"] * lt_rate
    total_unrealized_gain = total_market_value - total_cost_basis

    result = {
        "basis": "tax if every position were sold today (unrealised gains), US-style short/long-term netting",
        "total_cost_basis": round(total_cost_basis, 2),
        "total_market_value": round(total_market_value, 2),
        "total_unrealized_gain": round(total_unrealized_gain, 2),
        "total_gain_pct": round(total_unrealized_gain / total_cost_basis * 100, 2) if total_cost_basis > 0 else None,
        "short_term_gains": round(max(net_st, 0.0), 2),
        "long_term_gains": round(max(net_lt, 0.0), 2),
        "short_term_gross_gains": round(gross["st_gain"], 2),
        "short_term_gross_losses": round(gross["st_loss"], 2),
        "long_term_gross_gains": round(gross["lt_gain"], 2),
        "long_term_gross_losses": round(gross["lt_loss"], 2),
        "net_capital_loss": round(min(net_st, 0.0) + min(net_lt, 0.0), 2),
        "estimated_short_term_tax": round(est_short_term_tax, 2),
        "estimated_long_term_tax": round(est_long_term_tax, 2),
        "estimated_total_tax": round(total_tax, 2),
        "rates_used": {"short_term": st_rate, "long_term": lt_rate,
                       "assumed": params.get("st_rate") is None or params.get("lt_rate") is None},
        "holding_period_unknown": unknown_period,
        "num_positions": len(holdings),
        "positions_with_losses": len(tax_loss_harvest),
        "as_of": as_of.isoformat(),
    }

    if tax_loss_harvest:
        result["tax_loss_harvest"] = tax_loss_harvest
        result["tax_loss_harvest_potential"] = round(sum(t["unrealized_loss"] for t in tax_loss_harvest), 2)
        # what realising the losses saves against the gains realised at the same time
        result["tax_savings_potential"] = round(tax_without_losses - total_tax, 2)

    return result


def cmd_pme_analysis(params):
    """One-year relative performance of the holdings vs a benchmark (default SPY).

    Not a Kaplan-Schoar / Long-Nickels PME - that needs dated cash flows. "pme_ratio" is kept for
    compatibility and equals relative_wealth_ratio = (1 + portfolio return) / (1 + benchmark return).
    The portfolio is the weighted daily return (rebalanced daily to the weights; equal by default).
    """
    symbols = list(dict.fromkeys(str(s).strip().upper() for s in params.get("symbols", []) if str(s).strip()))
    if not symbols:
        return {"error": "No symbols provided"}
    benchmark = str(params.get("benchmark") or "SPY").strip().upper()

    raw_w = params.get("weights")
    try:
        if isinstance(raw_w, dict):
            w = np.array([float(raw_w.get(s, 0.0)) for s in symbols])
        elif isinstance(raw_w, list) and len(raw_w) == len(symbols):
            w = np.array(raw_w, dtype=float)
        else:
            w = np.ones(len(symbols))
    except (TypeError, ValueError):
        return {"error": "weights must be numbers"}
    if not np.all(np.isfinite(w)) or (w < 0).any() or w.sum() <= 0:
        return {"error": "weights must be non-negative numbers that sum to a positive value"}

    try:
        import pandas as pd
        fy = _fincept_yahoo()

        tickers = list(dict.fromkeys(symbols + [benchmark]))
        data = fy.download(tickers, period="1y", interval="1d", auto_adjust=True)  # the shared bar cache
        if data is None or data.empty:
            return {"error": "Could not fetch price data"}
        close = data["Close"] if isinstance(data.columns, pd.MultiIndex) else data[["Close"]]
        if isinstance(close, pd.Series):
            close = close.to_frame(name=tickers[0])
        if not isinstance(data.columns, pd.MultiIndex):
            close.columns = tickers[:1]

        have = [t for t in tickers if t in close.columns and close[t].notna().sum() > 1]
        if benchmark not in have:
            return {"error": f"No price data for the benchmark {benchmark}"}
        avail = [s for s in symbols if s in have]
        if not avail:
            return {"error": "No price data for any symbol"}
        wa = np.array([w[symbols.index(s)] for s in avail], dtype=float)
        if wa.sum() <= 0:
            return {"error": "the symbols with price data all have zero weight"}
        wa = wa / wa.sum()

        rets = close[list(dict.fromkeys(avail + [benchmark]))].pct_change(fill_method=None).dropna()
        port_returns = rets[avail] @ wa
        bench_returns = rets[benchmark]
        n_days = len(rets)
        if n_days < 2:
            return {"error": "Not enough overlapping price history"}

        port_cum = float((1 + port_returns).prod() - 1)
        bench_cum = float((1 + bench_returns).prod() - 1)
        ann_factor = 252
        port_ann = float((1 + port_cum) ** (ann_factor / n_days) - 1)
        bench_ann = float((1 + bench_cum) ** (ann_factor / n_days) - 1)
        relative_wealth = float((1 + port_cum) / (1 + bench_cum)) if (1 + bench_cum) != 0 else None

        excess = port_returns - bench_returns
        alpha_ann = float(excess.mean() * ann_factor)
        tracking_error = float(excess.std(ddof=1) * np.sqrt(ann_factor))
        info_ratio = float(alpha_ann / tracking_error) if tracking_error > 0 else None

        up_mask = bench_returns > 0
        down_mask = bench_returns < 0
        up_capture = (float(port_returns[up_mask].mean() / bench_returns[up_mask].mean())
                      if up_mask.sum() > 0 else None)
        down_capture = (float(port_returns[down_mask].mean() / bench_returns[down_mask].mean())
                        if down_mask.sum() > 0 else None)

        def r4(v):
            return round(v, 4) if v is not None else None

        return {
            "pme_ratio": r4(relative_wealth),
            "relative_wealth_ratio": r4(relative_wealth),
            "portfolio_total_return": r4(port_cum),
            "benchmark_total_return": r4(bench_cum),
            "portfolio_ann_return": r4(port_ann),
            "benchmark_ann_return": r4(bench_ann),
            "annualized_alpha": r4(alpha_ann),
            "tracking_error": r4(tracking_error),
            "information_ratio": r4(info_ratio),
            "up_capture_ratio": r4(up_capture),
            "down_capture_ratio": r4(down_capture),
            "outperformance": r4(port_cum - bench_cum),
            "trading_days": n_days,
            "benchmark": benchmark,
            "weights": dict(zip(avail, wa.round(6).tolist())),
            "symbols_missing": [s for s in symbols if s not in avail],
            "note": "Relative performance over one year, not a cash-flow PME: pme_ratio = relative_wealth_ratio "
                    "= (1 + portfolio return) / (1 + benchmark return); alpha = mean daily excess x 252; capture "
                    "ratios = mean portfolio / mean benchmark return on up / down benchmark days."
        }

    except Exception as e:  # network / pandas failures come back as JSON
        import traceback
        traceback.print_exc(file=sys.stderr)
        return {"error": f"{type(e).__name__}: {e}"}


def cmd_allocation_analysis(params):
    """Concentration of an allocation: HHI, effective number of holdings, top-3 / top-5 share.

    Weights come from "value" when every holding has one (w = value / total), else from "weight",
    always normalised to sum 1 (they used to be normalised only when they summed to more than 1.5).
    """
    holdings = params.get("holdings", [])
    if not holdings:
        return {"error": "No holdings provided"}

    symbols = [str(h.get("symbol", "")) for h in holdings]

    def numbers(key):
        vals = [h.get(key) for h in holdings]
        if any(v is None for v in vals):
            return None
        try:
            arr = np.array(vals, dtype=float)
        except (TypeError, ValueError):
            return None
        return arr if np.all(np.isfinite(arr)) else None

    values = numbers("value")
    weights = numbers("weight")
    if values is not None and (values >= 0).all() and values.sum() > 0:
        w, basis = values / values.sum(), "value"
    elif weights is not None and (weights >= 0).all() and weights.sum() > 0:
        w, basis = weights / weights.sum(), "weight"
    else:
        return {"error": "every holding needs a non-negative 'value' or 'weight', and they must not all be 0"}

    n = len(symbols)
    weights_norm = w.tolist()
    hhi = float(np.sum(w ** 2))  # Herfindahl-Hirschman Index (1/n .. 1)
    sorted_w = sorted(weights_norm, reverse=True)
    top3 = sum(sorted_w[:3])
    top5 = sum(sorted_w[:5])
    equal_weight = 1.0 / n
    max_deviation = max(abs(wt - equal_weight) for wt in weights_norm)

    return {
        "num_holdings": n,
        "weights_from": basis,
        "weights": dict(zip(symbols, [round(x, 6) for x in weights_norm])),
        "total_value": round(float(values.sum()), 2) if values is not None else None,
        "hhi_concentration": round(hhi, 4),
        "effective_num_stocks": round(1.0 / hhi, 2),
        "top_3_concentration_pct": round(top3 * 100, 2),
        "top_5_concentration_pct": round(top5 * 100, 2),
        "equal_weight_target": round(equal_weight, 4),
        "max_deviation_from_equal": round(max_deviation, 4),
        "is_concentrated": "Yes" if hhi > 0.25 else "No",
        "diversification_score": round((1 - hhi) * 100, 1),
    }


_ARGMAP = {"comprehensive_analysis": ["returns_data", "weights", "market_returns", "risk_free_rate",
                                      "frontier_points"]}


def _params(args) -> Dict:
    """The params object: the JSON argument, else stdin (read only when there is no argument - a
    caller that leaves stdin open without writing would otherwise block)."""
    params = args.get_json(0, "params")
    if params is None and len(sys.argv) <= 2:
        try:
            if not sys.stdin.isatty():
                data = sys.stdin.read()
                if data.strip():
                    import json
                    params = json.loads(data)
        except ValueError:
            raise CliError("stdin is not valid JSON")
        except OSError:
            params = None
    if params is None:
        raise CliError("No input data provided (pass a JSON object as the argument or on stdin)")
    if not isinstance(params, dict):
        raise CliError("the input must be a JSON object")
    return params


def _cmd_comprehensive_analysis(args):
    if len(sys.argv) <= 2:  # no argument: the params object comes on stdin
        import json
        args = CliArgs([sys.argv[0], "comprehensive_analysis", json.dumps(_params(args))], _ARGMAP)
    returns_data = args.get_json(0, "returns_data")
    if isinstance(returns_data, list):
        returns_data = {"Portfolio": returns_data}
    if not isinstance(returns_data, dict) or not returns_data:
        raise CliError("returns_data is required: {asset: [daily returns]}")
    data = {str(k): return_series(v, f"returns_data[{k}]") for k, v in returns_data.items()}
    weights = args.get_json(1, "weights")
    weights = return_series(weights, "weights", min_len=1) if weights is not None else None
    market = args.get_json(2, "market_returns")
    market = return_series(market, "market_returns") if market is not None else None
    rf = args.get_float(3, "risk_free_rate", MathConstants.DEFAULT_RISK_FREE_RATE)
    if not -0.5 < rf < 1.0:
        raise CliError("risk_free_rate must be an annual fraction (e.g. 0.03)")
    points = int(args.get_float(4, "frontier_points", 25))
    if not 2 <= points <= 200:
        raise CliError("frontier_points must be between 2 and 200")
    params = dataclasses.replace(DEFAULT_PORTFOLIO_PARAMS, risk_free_rate=rf)
    return PortfolioAnalytics(params).comprehensive_analysis(data, weights, market, points)


if __name__ == "__main__":
    run_cli({
        "comprehensive_analysis": _cmd_comprehensive_analysis,
        "calculate_portfolio_metrics": lambda a: cmd_calculate_portfolio_metrics(_params(a)),
        "tax_report": lambda a: cmd_tax_report(_params(a)),
        "pme_analysis": lambda a: cmd_pme_analysis(_params(a)),
        "allocation_analysis": lambda a: cmd_allocation_analysis(_params(a)),
    }, _ARGMAP)
