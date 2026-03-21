"""
Fortitudo.tech Portfolio Optimization Wrapper
==============================================
Mean-Variance and Mean-CVaR optimization using convex optimization.

Implements portfolio optimization with constraints using scipy or cvxopt.

Usage:
    python optimization.py
"""

import numpy as np
import pandas as pd
from typing import Dict, Any, Optional, List, Tuple, Union
import warnings

warnings.filterwarnings('ignore')

# Try to import optimization backends
try:
    from scipy.optimize import minimize, LinearConstraint, Bounds
    SCIPY_AVAILABLE = True
except ImportError:
    SCIPY_AVAILABLE = False

try:
    import cvxopt
    from cvxopt import matrix, solvers
    solvers.options['show_progress'] = False
    CVXOPT_AVAILABLE = True
except ImportError:
    CVXOPT_AVAILABLE = False

try:
    import fortitudo.tech as ft
    FORTITUDO_AVAILABLE = True
except ImportError:
    FORTITUDO_AVAILABLE = False
    ft = None


class MeanVarianceOptimizer:
    """
    Mean-Variance Portfolio Optimization

    Minimizes portfolio variance for a given target return,
    or maximizes return for a given risk level.

    Methods:
        - minimum_variance(): Find the global minimum variance portfolio
        - efficient_frontier(): Generate efficient frontier points
        - max_sharpe(): Find the maximum Sharpe ratio portfolio
        - target_return(): Find portfolio with target return and minimum variance
        - target_risk(): Find portfolio with target risk and maximum return
    """

    def __init__(
        self,
        returns: Union[pd.DataFrame, np.ndarray],
        probabilities: Optional[np.ndarray] = None,
        risk_free_rate: float = 0.0
    ):
        """
        Initialize optimizer with return data.

        Args:
            returns: Asset returns matrix (n_scenarios x n_assets)
            probabilities: Scenario probabilities (optional)
            risk_free_rate: Risk-free rate for Sharpe ratio calculation
        """
        if isinstance(returns, pd.DataFrame):
            self.asset_names = list(returns.columns)
            self.returns = returns.values
        else:
            self.returns = returns
            self.asset_names = [f'Asset_{i}' for i in range(returns.shape[1])]

        self.n_scenarios, self.n_assets = self.returns.shape
        self.probabilities = probabilities
        self.risk_free_rate = risk_free_rate

        # Calculate mean returns and covariance matrix
        if probabilities is not None:
            p = probabilities.flatten() if probabilities.ndim == 2 else probabilities
            self.mean_returns = self.returns.T @ p
            centered = self.returns - self.mean_returns
            self.cov_matrix = (centered.T * p) @ centered
        else:
            self.mean_returns = np.mean(self.returns, axis=0)
            self.cov_matrix = np.cov(self.returns, rowvar=False)

    def _portfolio_variance(self, weights: np.ndarray) -> float:
        """Calculate portfolio variance."""
        return float(weights @ self.cov_matrix @ weights)

    def _portfolio_volatility(self, weights: np.ndarray) -> float:
        """Calculate portfolio volatility."""
        return np.sqrt(self._portfolio_variance(weights))

    def _portfolio_return(self, weights: np.ndarray) -> float:
        """Calculate portfolio expected return."""
        return float(weights @ self.mean_returns)

    def _sharpe_ratio(self, weights: np.ndarray) -> float:
        """Calculate portfolio Sharpe ratio."""
        port_return = self._portfolio_return(weights)
        port_vol = self._portfolio_volatility(weights)
        if port_vol == 0:
            return 0.0
        return (port_return - self.risk_free_rate) / port_vol

    def minimum_variance(
        self,
        long_only: bool = True,
        max_weight: Optional[float] = None,
        min_weight: Optional[float] = None
    ) -> Dict[str, Any]:
        """
        Find the global minimum variance portfolio.

        Args:
            long_only: If True, enforce non-negative weights
            max_weight: Maximum weight per asset
            min_weight: Minimum weight per asset

        Returns:
            Dictionary with optimal weights and metrics
        """
        if not SCIPY_AVAILABLE:
            return {"success": False, "error": "scipy not available"}

        n = self.n_assets

        # Initial guess: equal weights
        x0 = np.ones(n) / n

        # Bounds
        if long_only:
            lower = min_weight if min_weight is not None else 0.0
            upper = max_weight if max_weight is not None else 1.0
            bounds = Bounds(np.full(n, lower), np.full(n, upper))
        else:
            lower = min_weight if min_weight is not None else -1.0
            upper = max_weight if max_weight is not None else 1.0
            bounds = Bounds(np.full(n, lower), np.full(n, upper))

        # Sum to 1 constraint
        constraints = [{'type': 'eq', 'fun': lambda w: np.sum(w) - 1.0}]

        # Minimize variance
        result = minimize(
            self._portfolio_variance,
            x0,
            method='SLSQP',
            bounds=bounds,
            constraints=constraints,
            options={'maxiter': 1000}
        )

        if result.success:
            weights = result.x
            return {
                "success": True,
                "weights": dict(zip(self.asset_names, weights.tolist())),
                "expected_return": self._portfolio_return(weights),
                "volatility": self._portfolio_volatility(weights),
                "sharpe_ratio": self._sharpe_ratio(weights),
                "variance": self._portfolio_variance(weights)
            }
        else:
            return {"success": False, "error": result.message}

    def max_sharpe(
        self,
        long_only: bool = True,
        max_weight: Optional[float] = None
    ) -> Dict[str, Any]:
        """
        Find the maximum Sharpe ratio portfolio.

        Args:
            long_only: If True, enforce non-negative weights
            max_weight: Maximum weight per asset

        Returns:
            Dictionary with optimal weights and metrics
        """
        if not SCIPY_AVAILABLE:
            return {"success": False, "error": "scipy not available"}

        n = self.n_assets
        x0 = np.ones(n) / n

        # Negative Sharpe ratio for minimization
        def neg_sharpe(weights):
            return -self._sharpe_ratio(weights)

        # Bounds
        if long_only:
            bounds = Bounds(np.zeros(n), np.full(n, max_weight if max_weight else 1.0))
        else:
            bounds = Bounds(np.full(n, -1.0), np.full(n, max_weight if max_weight else 1.0))

        constraints = [{'type': 'eq', 'fun': lambda w: np.sum(w) - 1.0}]

        result = minimize(
            neg_sharpe,
            x0,
            method='SLSQP',
            bounds=bounds,
            constraints=constraints,
            options={'maxiter': 1000}
        )

        if result.success:
            weights = result.x
            return {
                "success": True,
                "weights": dict(zip(self.asset_names, weights.tolist())),
                "expected_return": self._portfolio_return(weights),
                "volatility": self._portfolio_volatility(weights),
                "sharpe_ratio": self._sharpe_ratio(weights),
                "variance": self._portfolio_variance(weights)
            }
        else:
            return {"success": False, "error": result.message}

    def target_return(
        self,
        target: float,
        long_only: bool = True,
        max_weight: Optional[float] = None
    ) -> Dict[str, Any]:
        """
        Find minimum variance portfolio for a target return.

        Args:
            target: Target expected return
            long_only: If True, enforce non-negative weights
            max_weight: Maximum weight per asset

        Returns:
            Dictionary with optimal weights and metrics
        """
        if not SCIPY_AVAILABLE:
            return {"success": False, "error": "scipy not available"}

        n = self.n_assets
        x0 = np.ones(n) / n

        if long_only:
            bounds = Bounds(np.zeros(n), np.full(n, max_weight if max_weight else 1.0))
        else:
            bounds = Bounds(np.full(n, -1.0), np.full(n, max_weight if max_weight else 1.0))

        constraints = [
            {'type': 'eq', 'fun': lambda w: np.sum(w) - 1.0},
            {'type': 'eq', 'fun': lambda w: self._portfolio_return(w) - target}
        ]

        result = minimize(
            self._portfolio_variance,
            x0,
            method='SLSQP',
            bounds=bounds,
            constraints=constraints,
            options={'maxiter': 1000}
        )

        if result.success:
            weights = result.x
            return {
                "success": True,
                "weights": dict(zip(self.asset_names, weights.tolist())),
                "expected_return": self._portfolio_return(weights),
                "volatility": self._portfolio_volatility(weights),
                "sharpe_ratio": self._sharpe_ratio(weights),
                "target_return": target
            }
        else:
            return {"success": False, "error": result.message}

    def efficient_frontier(
        self,
        n_points: int = 20,
        long_only: bool = True,
        max_weight: Optional[float] = None
    ) -> Dict[str, Any]:
        """
        Generate the efficient frontier.

        Args:
            n_points: Number of points on the frontier
            long_only: If True, enforce non-negative weights
            max_weight: Maximum weight per asset

        Returns:
            Dictionary with frontier points
        """
        # Get min and max returns
        min_var_result = self.minimum_variance(long_only, max_weight)
        if not min_var_result["success"]:
            return min_var_result

        min_return = min_var_result["expected_return"]

        # Find max return portfolio (100% in highest return asset if long_only)
        if long_only:
            max_return = np.max(self.mean_returns)
        else:
            max_return = min_return + 3 * min_var_result["volatility"]

        # Guard against degenerate case where min == max return
        if np.isclose(min_return, max_return):
            return {
                "success": True,
                "frontier": [{
                    "expected_return": min_var_result["expected_return"],
                    "volatility": min_var_result["volatility"],
                    "sharpe_ratio": min_var_result["sharpe_ratio"],
                    "weights": min_var_result["weights"]
                }],
                "n_points": 1,
                "min_variance_portfolio": min_var_result,
                "note": "Degenerate frontier: all assets have identical expected returns"
            }

        # Generate target returns
        target_returns = np.linspace(min_return, max_return, n_points)

        frontier = []
        for target in target_returns:
            result = self.target_return(target, long_only, max_weight)
            if result["success"]:
                frontier.append({
                    "expected_return": result["expected_return"],
                    "volatility": result["volatility"],
                    "sharpe_ratio": result["sharpe_ratio"],
                    "weights": result["weights"]
                })

        return {
            "success": True,
            "frontier": frontier,
            "n_points": len(frontier),
            "min_variance_portfolio": min_var_result
        }


class MeanCVaROptimizer:
    """
    Mean-CVaR Portfolio Optimization

    Minimizes Conditional Value-at-Risk (Expected Shortfall) for a given target return,
    or maximizes return for a given CVaR level.

    CVaR is a coherent risk measure that considers the expected loss in the worst cases.
    """

    def __init__(
        self,
        returns: Union[pd.DataFrame, np.ndarray],
        probabilities: Optional[np.ndarray] = None,
        alpha: float = 0.05,
        risk_free_rate: float = 0.0
    ):
        """
        Initialize optimizer.

        Args:
            returns: Asset returns matrix (n_scenarios x n_assets)
            probabilities: Scenario probabilities (optional)
            alpha: Significance level for CVaR (e.g., 0.05 for 95% CVaR)
            risk_free_rate: Risk-free rate
        """
        if isinstance(returns, pd.DataFrame):
            self.asset_names = list(returns.columns)
            self.returns = returns.values
        else:
            self.returns = returns
            self.asset_names = [f'Asset_{i}' for i in range(returns.shape[1])]

        self.n_scenarios, self.n_assets = self.returns.shape
        self.alpha = alpha
        self.risk_free_rate = risk_free_rate

        if probabilities is not None:
            self.probabilities = probabilities.flatten() if probabilities.ndim == 2 else probabilities
        else:
            self.probabilities = np.ones(self.n_scenarios) / self.n_scenarios

        # Mean returns
        self.mean_returns = self.returns.T @ self.probabilities

    def _portfolio_return(self, weights: np.ndarray) -> float:
        """Calculate portfolio expected return."""
        return float(weights @ self.mean_returns)

    def _portfolio_cvar(self, weights: np.ndarray) -> float:
        """Calculate portfolio CVaR."""
        port_returns = self.returns @ weights

        # Sort returns and probabilities
        sorted_idx = np.argsort(port_returns)
        sorted_returns = port_returns[sorted_idx]
        sorted_probs = self.probabilities[sorted_idx]

        # Find CVaR
        cumsum = np.cumsum(sorted_probs)
        tail_mask = cumsum <= self.alpha
        if np.any(tail_mask):
            tail_probs = sorted_probs[tail_mask]
            tail_returns = sorted_returns[tail_mask]
            cvar = -np.sum(tail_returns * tail_probs) / np.sum(tail_probs)
        else:
            cvar = -sorted_returns[0]

        return float(cvar)

    def _portfolio_var(self, weights: np.ndarray) -> float:
        """Calculate portfolio VaR."""
        port_returns = self.returns @ weights
        if len(port_returns) == 0:
            return 0.0
        sorted_idx = np.argsort(port_returns)
        sorted_returns = port_returns[sorted_idx]
        sorted_probs = self.probabilities[sorted_idx]
        cumsum = np.cumsum(sorted_probs)
        var_idx = np.searchsorted(cumsum, self.alpha)
        return -sorted_returns[min(var_idx, len(sorted_returns) - 1)]

    def minimum_cvar(
        self,
        long_only: bool = True,
        max_weight: Optional[float] = None,
        min_weight: Optional[float] = None
    ) -> Dict[str, Any]:
        """
        Find the minimum CVaR portfolio.

        Args:
            long_only: If True, enforce non-negative weights
            max_weight: Maximum weight per asset
            min_weight: Minimum weight per asset

        Returns:
            Dictionary with optimal weights and metrics
        """
        if not SCIPY_AVAILABLE:
            return {"success": False, "error": "scipy not available"}

        n = self.n_assets
        x0 = np.ones(n) / n

        # Bounds
        if long_only:
            lower = min_weight if min_weight is not None else 0.0
            upper = max_weight if max_weight is not None else 1.0
            bounds = Bounds(np.full(n, lower), np.full(n, upper))
        else:
            lower = min_weight if min_weight is not None else -1.0
            upper = max_weight if max_weight is not None else 1.0
            bounds = Bounds(np.full(n, lower), np.full(n, upper))

        constraints = [{'type': 'eq', 'fun': lambda w: np.sum(w) - 1.0}]

        result = minimize(
            self._portfolio_cvar,
            x0,
            method='SLSQP',
            bounds=bounds,
            constraints=constraints,
            options={'maxiter': 1000}
        )

        if result.success:
            weights = result.x
            return {
                "success": True,
                "weights": dict(zip(self.asset_names, weights.tolist())),
                "expected_return": self._portfolio_return(weights),
                "cvar": self._portfolio_cvar(weights),
                "var": self._portfolio_var(weights),
                "alpha": self.alpha
            }
        else:
            return {"success": False, "error": result.message}

    def target_return(
        self,
        target: float,
        long_only: bool = True,
        max_weight: Optional[float] = None
    ) -> Dict[str, Any]:
        """
        Find minimum CVaR portfolio for a target return.

        Args:
            target: Target expected return
            long_only: If True, enforce non-negative weights
            max_weight: Maximum weight per asset

        Returns:
            Dictionary with optimal weights and metrics
        """
        if not SCIPY_AVAILABLE:
            return {"success": False, "error": "scipy not available"}

        n = self.n_assets
        x0 = np.ones(n) / n

        if long_only:
            bounds = Bounds(np.zeros(n), np.full(n, max_weight if max_weight else 1.0))
        else:
            bounds = Bounds(np.full(n, -1.0), np.full(n, max_weight if max_weight else 1.0))

        constraints = [
            {'type': 'eq', 'fun': lambda w: np.sum(w) - 1.0},
            {'type': 'eq', 'fun': lambda w: self._portfolio_return(w) - target}
        ]

        result = minimize(
            self._portfolio_cvar,
            x0,
            method='SLSQP',
            bounds=bounds,
            constraints=constraints,
            options={'maxiter': 1000}
        )

        if result.success:
            weights = result.x
            return {
                "success": True,
                "weights": dict(zip(self.asset_names, weights.tolist())),
                "expected_return": self._portfolio_return(weights),
                "cvar": self._portfolio_cvar(weights),
                "var": self._portfolio_var(weights),
                "target_return": target,
                "alpha": self.alpha
            }
        else:
            return {"success": False, "error": result.message}

    def efficient_frontier(
        self,
        n_points: int = 20,
        long_only: bool = True,
        max_weight: Optional[float] = None
    ) -> Dict[str, Any]:
        """
        Generate the CVaR-efficient frontier.

        Args:
            n_points: Number of points on the frontier
            long_only: If True, enforce non-negative weights
            max_weight: Maximum weight per asset

        Returns:
            Dictionary with frontier points
        """
        # Get min CVaR portfolio
        min_cvar_result = self.minimum_cvar(long_only, max_weight)
        if not min_cvar_result["success"]:
            return min_cvar_result

        min_return = min_cvar_result["expected_return"]

        # Find max return
        if long_only:
            max_return = np.max(self.mean_returns)
        else:
            max_return = min_return + 3 * min_cvar_result["cvar"]

        # Guard against degenerate case where min == max return
        if np.isclose(min_return, max_return):
            return {
                "success": True,
                "frontier": [{
                    "expected_return": min_cvar_result["expected_return"],
                    "cvar": min_cvar_result["cvar"],
                    "var": min_cvar_result["var"],
                    "weights": min_cvar_result["weights"]
                }],
                "n_points": 1,
                "min_cvar_portfolio": min_cvar_result,
                "alpha": self.alpha,
                "note": "Degenerate frontier: all assets have identical expected returns"
            }

        # Generate target returns
        target_returns = np.linspace(min_return, max_return, n_points)

        frontier = []
        for target in target_returns:
            result = self.target_return(target, long_only, max_weight)
            if result["success"]:
                frontier.append({
                    "expected_return": result["expected_return"],
                    "cvar": result["cvar"],
                    "var": result["var"],
                    "weights": result["weights"]
                })

        return {
            "success": True,
            "frontier": frontier,
            "n_points": len(frontier),
            "min_cvar_portfolio": min_cvar_result,
            "alpha": self.alpha
        }


def main():
    """Test optimization functions."""
    print("=== Portfolio Optimization Test ===\n")

    # Generate test data
    np.random.seed(42)
    n_scenarios = 200
    n_assets = 4
    asset_names = ['Stocks', 'Bonds', 'Commodities', 'Cash']

    # Different return characteristics for each asset
    returns = np.column_stack([
        np.random.randn(n_scenarios) * 0.02 + 0.0003,  # Stocks: high vol, high return
        np.random.randn(n_scenarios) * 0.005 + 0.0001,  # Bonds: low vol, low return
        np.random.randn(n_scenarios) * 0.015 + 0.0002,  # Commodities: med vol
        np.random.randn(n_scenarios) * 0.001 + 0.0001,  # Cash: very low vol
    ])

    returns_df = pd.DataFrame(returns, columns=asset_names)

    # Test Mean-Variance Optimizer
    print("1. Mean-Variance Optimization")
    print("-" * 50)

    mv_optimizer = MeanVarianceOptimizer(returns_df, risk_free_rate=0.0001)

    # Minimum variance portfolio
    min_var = mv_optimizer.minimum_variance()
    if min_var["success"]:
        print("  Minimum Variance Portfolio:")
        print(f"    Return: {min_var['expected_return']:.6f}")
        print(f"    Volatility: {min_var['volatility']:.6f}")
        print(f"    Sharpe: {min_var['sharpe_ratio']:.4f}")
        print(f"    Weights: {min_var['weights']}")

    # Max Sharpe portfolio
    max_sharpe = mv_optimizer.max_sharpe()
    if max_sharpe["success"]:
        print("\n  Maximum Sharpe Portfolio:")
        print(f"    Return: {max_sharpe['expected_return']:.6f}")
        print(f"    Volatility: {max_sharpe['volatility']:.6f}")
        print(f"    Sharpe: {max_sharpe['sharpe_ratio']:.4f}")
        print(f"    Weights: {max_sharpe['weights']}")

    # Test Mean-CVaR Optimizer
    print("\n2. Mean-CVaR Optimization")
    print("-" * 50)

    cvar_optimizer = MeanCVaROptimizer(returns_df, alpha=0.05)

    # Minimum CVaR portfolio
    min_cvar = cvar_optimizer.minimum_cvar()
    if min_cvar["success"]:
        print("  Minimum CVaR Portfolio (95%):")
        print(f"    Return: {min_cvar['expected_return']:.6f}")
        print(f"    CVaR: {min_cvar['cvar']:.6f}")
        print(f"    VaR: {min_cvar['var']:.6f}")
        print(f"    Weights: {min_cvar['weights']}")

    # Efficient frontier
    print("\n3. Efficient Frontier (5 points)")
    print("-" * 50)
    frontier = mv_optimizer.efficient_frontier(n_points=5)
    if frontier["success"]:
        print("  Return    Volatility  Sharpe")
        for pt in frontier["frontier"]:
            print(f"  {pt['expected_return']:.6f}  {pt['volatility']:.6f}    {pt['sharpe_ratio']:.4f}")

    print("\n=== Test: PASSED ===")


if __name__ == "__main__":
    main()
