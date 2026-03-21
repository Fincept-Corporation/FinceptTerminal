"""
AI Quant Lab - Portfolio Optimization Module
Advanced portfolio optimization algorithms including:
- Mean-Variance Optimization (Markowitz)
- Black-Litterman Model
- Risk Parity
- Hierarchical Risk Parity (HRP)
- Minimum Variance Portfolio
- Maximum Sharpe Portfolio
- Maximum Diversification
"""

import json
import sys
from typing import Dict, List, Any, Optional, Union, Tuple
import warnings
warnings.filterwarnings('ignore')

try:
    import pandas as pd
    import numpy as np
    from scipy import optimize
    from scipy.cluster.hierarchy import linkage, dendrogram
    from scipy.spatial.distance import squareform
    SCIPY_AVAILABLE = True
except ImportError:
    SCIPY_AVAILABLE = False
    pd = None
    np = None
    optimize = None


class PortfolioOptimizationService:
    """
    Advanced portfolio optimization service.

    Implements state-of-the-art portfolio construction methods.
    """

    def __init__(self):
        self.portfolios = {}

    def black_litterman(self,
                       market_caps: pd.Series,
                       cov_matrix: pd.DataFrame,
                       views: Dict[str, float],
                       view_confidences: Dict[str, float],
                       risk_free_rate: float = 0.02,
                       tau: float = 0.025,
                       risk_aversion: float = 2.5) -> Dict[str, Any]:
        """
        Black-Litterman portfolio optimization.

        Combines market equilibrium with investor views.

        Args:
            market_caps: Market capitalizations (proxy for equilibrium weights)
            cov_matrix: Covariance matrix of returns
            views: Dict of asset -> expected return view
            view_confidences: Dict of asset -> confidence (0-1)
            risk_free_rate: Risk-free rate
            tau: Scaling factor for uncertainty in prior (typically 0.01-0.05)
            risk_aversion: Market risk aversion coefficient

        Returns:
            Optimal portfolio weights and expected returns
        """
        if not SCIPY_AVAILABLE:
            return {"success": False, "error": "SciPy not available"}

        try:
            # Step 1: Calculate market equilibrium returns (reverse optimization)
            market_weights = market_caps / market_caps.sum()
            equilibrium_returns = risk_aversion * cov_matrix @ market_weights

            # Step 2: Prepare views
            assets = list(cov_matrix.columns)
            n_assets = len(assets)
            n_views = len(views)

            # View matrix P (n_views x n_assets)
            P = np.zeros((n_views, n_assets))
            Q = np.zeros(n_views)  # View returns

            for i, (asset, view_return) in enumerate(views.items()):
                if asset in assets:
                    asset_idx = assets.index(asset)
                    P[i, asset_idx] = 1.0
                    Q[i] = view_return

            # Uncertainty in views (Omega matrix)
            Omega = np.zeros((n_views, n_views))
            for i, (asset, confidence) in enumerate(view_confidences.items()):
                if asset in assets:
                    asset_idx = assets.index(asset)
                    # Lower confidence = higher uncertainty
                    view_var = (1 - confidence) * tau * cov_matrix.iloc[asset_idx, asset_idx]
                    Omega[i, i] = view_var

            # Step 3: Black-Litterman formula
            # Posterior returns = equilibrium + adjustment from views
            tau_cov = tau * cov_matrix

            # M matrix
            M_inv = np.linalg.inv(tau_cov) + P.T @ np.linalg.inv(Omega) @ P
            M = np.linalg.inv(M_inv)

            # Posterior expected returns
            posterior_returns = M @ (
                np.linalg.inv(tau_cov) @ equilibrium_returns.values +
                P.T @ np.linalg.inv(Omega) @ Q
            )

            # Posterior covariance
            posterior_cov = cov_matrix + M

            # Step 4: Optimize portfolio with posterior estimates
            # Maximize utility: w'μ - (λ/2)w'Σw
            def neg_utility(weights):
                return -(weights @ posterior_returns - (risk_aversion/2) * weights @ posterior_cov @ weights)

            constraints = [{'type': 'eq', 'fun': lambda w: np.sum(w) - 1}]
            bounds = tuple((0, 1) for _ in range(n_assets))
            init_weights = market_weights.values

            result = optimize.minimize(
                neg_utility,
                init_weights,
                method='SLSQP',
                bounds=bounds,
                constraints=constraints
            )

            if not result.success:
                return {"success": False, "error": "Optimization failed"}

            optimal_weights = result.x

            # Calculate portfolio statistics
            port_return = optimal_weights @ posterior_returns
            port_vol = np.sqrt(optimal_weights @ posterior_cov @ optimal_weights)
            sharpe = (port_return - risk_free_rate) / port_vol

            return {
                "success": True,
                "method": "Black-Litterman",
                "weights": dict(zip(assets, optimal_weights)),
                "expected_return": float(port_return),
                "expected_volatility": float(port_vol),
                "sharpe_ratio": float(sharpe),
                "equilibrium_returns": dict(zip(assets, equilibrium_returns)),
                "posterior_returns": dict(zip(assets, posterior_returns)),
                "views_incorporated": len(views),
                "description": "Black-Litterman combines market equilibrium with investor views"
            }

        except Exception as e:
            return {"success": False, "error": f"Black-Litterman failed: {str(e)}"}

    def hierarchical_risk_parity(self,
                                 cov_matrix: pd.DataFrame) -> Dict[str, Any]:
        """
        Hierarchical Risk Parity (HRP) portfolio.

        Tree-based allocation using machine learning clustering.
        More stable than traditional optimization.

        Args:
            cov_matrix: Covariance matrix of returns

        Returns:
            HRP portfolio weights
        """
        if not SCIPY_AVAILABLE:
            return {"success": False, "error": "SciPy not available"}

        try:
            # Step 1: Tree clustering
            corr_matrix = cov_matrix.corr()
            dist_matrix = np.sqrt((1 - corr_matrix) / 2)  # Distance metric
            link = linkage(squareform(dist_matrix.values), method='single')

            # Step 2: Quasi-diagonalization (reorder assets)
            sorted_indices = self._get_quasi_diag(link)
            sorted_assets = cov_matrix.columns[sorted_indices].tolist()

            # Step 3: Recursive bisection
            weights = pd.Series(1.0, index=sorted_assets)
            cluster_items = [sorted_assets]

            while len(cluster_items) > 0:
                cluster_items = [
                    item[j:k]
                    for item in cluster_items
                    for j, k in ((0, len(item)//2), (len(item)//2, len(item)))
                    if len(item) > 1
                ]

                for i in range(0, len(cluster_items), 2):
                    cluster_0 = cluster_items[i]
                    cluster_1 = cluster_items[i+1] if i+1 < len(cluster_items) else cluster_0

                    # Calculate cluster variances
                    cov_0 = cov_matrix.loc[cluster_0, cluster_0]
                    cov_1 = cov_matrix.loc[cluster_1, cluster_1]

                    ivp_0 = 1.0 / np.diag(cov_0)  # Inverse variance portfolio
                    ivp_1 = 1.0 / np.diag(cov_1)

                    ivp_0 /= ivp_0.sum()
                    ivp_1 /= ivp_1.sum()

                    var_0 = ivp_0 @ cov_0 @ ivp_0
                    var_1 = ivp_1 @ cov_1 @ ivp_1

                    # Allocate by inverse variance
                    alpha = 1 - var_0 / (var_0 + var_1)

                    weights[cluster_0] *= alpha
                    weights[cluster_1] *= (1 - alpha)

            # Normalize
            weights /= weights.sum()

            # Calculate portfolio stats
            port_vol = np.sqrt(weights.values @ cov_matrix @ weights.values)

            return {
                "success": True,
                "method": "Hierarchical Risk Parity",
                "weights": weights.to_dict(),
                "expected_volatility": float(port_vol),
                "num_assets": len(weights),
                "description": "HRP uses machine learning clustering for robust allocation",
                "advantages": [
                    "No matrix inversion (numerically stable)",
                    "Handles multicollinearity well",
                    "Out-of-sample robustness"
                ]
            }

        except Exception as e:
            return {"success": False, "error": f"HRP failed: {str(e)}"}

    def minimum_variance_portfolio(self,
                                   cov_matrix: pd.DataFrame,
                                   constraints: Optional[Dict] = None) -> Dict[str, Any]:
        """
        Minimum Variance Portfolio.

        Minimizes portfolio volatility regardless of returns.

        Args:
            cov_matrix: Covariance matrix
            constraints: Optional constraints (max_weight, etc.)

        Returns:
            Minimum variance portfolio
        """
        if not SCIPY_AVAILABLE:
            return {"success": False, "error": "SciPy not available"}

        try:
            n_assets = cov_matrix.shape[0]
            constraints_dict = constraints or {}

            # Objective: minimize variance
            def portfolio_variance(weights):
                return weights @ cov_matrix @ weights

            # Constraints
            cons = [{'type': 'eq', 'fun': lambda w: np.sum(w) - 1}]

            # Bounds
            max_weight = constraints_dict.get("max_weight", 1.0)
            min_weight = constraints_dict.get("min_weight", 0.0)
            bounds = tuple((min_weight, max_weight) for _ in range(n_assets))

            # Initial guess
            init_weights = np.array([1.0/n_assets] * n_assets)

            # Optimize
            result = optimize.minimize(
                portfolio_variance,
                init_weights,
                method='SLSQP',
                bounds=bounds,
                constraints=cons
            )

            if not result.success:
                return {"success": False, "error": "Optimization failed"}

            optimal_weights = result.x
            port_vol = np.sqrt(portfolio_variance(optimal_weights))

            return {
                "success": True,
                "method": "Minimum Variance",
                "weights": dict(zip(cov_matrix.columns, optimal_weights)),
                "expected_volatility": float(port_vol),
                "description": "Minimum risk portfolio, ignores expected returns",
                "suitable_for": ["Risk-averse investors", "Low volatility mandates"]
            }

        except Exception as e:
            return {"success": False, "error": f"Minimum variance failed: {str(e)}"}

    def maximum_sharpe_portfolio(self,
                                 expected_returns: pd.Series,
                                 cov_matrix: pd.DataFrame,
                                 risk_free_rate: float = 0.02,
                                 constraints: Optional[Dict] = None) -> Dict[str, Any]:
        """
        Maximum Sharpe Ratio Portfolio.

        Finds the tangency portfolio on the efficient frontier.

        Args:
            expected_returns: Expected returns vector
            cov_matrix: Covariance matrix
            risk_free_rate: Risk-free rate
            constraints: Optional constraints

        Returns:
            Maximum Sharpe portfolio
        """
        if not SCIPY_AVAILABLE:
            return {"success": False, "error": "SciPy not available"}

        try:
            n_assets = len(expected_returns)
            constraints_dict = constraints or {}

            # Objective: maximize Sharpe ratio
            def neg_sharpe(weights):
                port_return = weights @ expected_returns
                port_vol = np.sqrt(weights @ cov_matrix @ weights)
                return -(port_return - risk_free_rate) / port_vol

            # Constraints
            cons = [{'type': 'eq', 'fun': lambda w: np.sum(w) - 1}]

            # Bounds
            max_weight = constraints_dict.get("max_weight", 1.0)
            min_weight = constraints_dict.get("min_weight", 0.0)
            bounds = tuple((min_weight, max_weight) for _ in range(n_assets))

            # Initial guess
            init_weights = np.array([1.0/n_assets] * n_assets)

            # Optimize
            result = optimize.minimize(
                neg_sharpe,
                init_weights,
                method='SLSQP',
                bounds=bounds,
                constraints=cons
            )

            if not result.success:
                return {"success": False, "error": "Optimization failed"}

            optimal_weights = result.x
            port_return = optimal_weights @ expected_returns
            port_vol = np.sqrt(optimal_weights @ cov_matrix @ optimal_weights)
            sharpe = (port_return - risk_free_rate) / port_vol

            return {
                "success": True,
                "method": "Maximum Sharpe Ratio",
                "weights": dict(zip(expected_returns.index, optimal_weights)),
                "expected_return": float(port_return),
                "expected_volatility": float(port_vol),
                "sharpe_ratio": float(sharpe),
                "risk_free_rate": risk_free_rate,
                "description": "Tangency portfolio with highest risk-adjusted returns",
                "suitable_for": ["Performance maximization", "Unconstrained mandates"]
            }

        except Exception as e:
            return {"success": False, "error": f"Maximum Sharpe failed: {str(e)}"}

    def efficient_frontier(self,
                          expected_returns: pd.Series,
                          cov_matrix: pd.DataFrame,
                          num_portfolios: int = 100) -> Dict[str, Any]:
        """
        Calculate the efficient frontier.

        Args:
            expected_returns: Expected returns
            cov_matrix: Covariance matrix
            num_portfolios: Number of portfolios to generate

        Returns:
            Efficient frontier portfolios
        """
        if not SCIPY_AVAILABLE:
            return {"success": False, "error": "SciPy not available"}

        try:
            n_assets = len(expected_returns)

            # Find min and max return portfolios
            min_var_port = self.minimum_variance_portfolio(cov_matrix)
            if not min_var_port["success"]:
                return min_var_port

            min_return = min(expected_returns)
            max_return = max(expected_returns)

            # Generate portfolios along frontier
            target_returns = np.linspace(min_return, max_return, num_portfolios)
            frontier_portfolios = []

            for target_ret in target_returns:
                # Optimize for minimum variance given target return
                def portfolio_variance(weights):
                    return weights @ cov_matrix @ weights

                cons = [
                    {'type': 'eq', 'fun': lambda w: np.sum(w) - 1},
                    {'type': 'eq', 'fun': lambda w: w @ expected_returns - target_ret}
                ]

                bounds = tuple((0, 1) for _ in range(n_assets))
                init_weights = np.array([1.0/n_assets] * n_assets)

                result = optimize.minimize(
                    portfolio_variance,
                    init_weights,
                    method='SLSQP',
                    bounds=bounds,
                    constraints=cons
                )

                if result.success:
                    weights = result.x
                    vol = np.sqrt(portfolio_variance(weights))
                    frontier_portfolios.append({
                        "return": float(target_ret),
                        "volatility": float(vol),
                        "sharpe": float((target_ret - 0.02) / vol),
                        "weights": dict(zip(expected_returns.index, weights))
                    })

            return {
                "success": True,
                "frontier": frontier_portfolios,
                "num_portfolios": len(frontier_portfolios),
                "description": "Efficient frontier: risk-return trade-off curve"
            }

        except Exception as e:
            return {"success": False, "error": f"Efficient frontier failed: {str(e)}"}

    def _get_quasi_diag(self, link):
        """Get quasi-diagonal order from linkage"""
        link = link.astype(int)
        sorted_items = pd.Series([link[-1, 0], link[-1, 1]])
        num_items = link[-1, 3]

        while sorted_items.max() >= num_items:
            sorted_items.index = range(0, sorted_items.shape[0] * 2, 2)
            df = sorted_items[sorted_items >= num_items]
            i = df.index
            j = df.values - num_items
            sorted_items[i] = link[j, 0]
            df = pd.Series(link[j, 1], index=i+1)
            sorted_items = sorted_items.append(df)
            sorted_items = sorted_items.sort_index()
            sorted_items.index = range(sorted_items.shape[0])

        return sorted_items.tolist()


def main():
    """CLI interface"""
    if len(sys.argv) < 2:
        print(json.dumps({"success": False, "error": "No command specified"}))
        sys.exit(1)

    command = sys.argv[1]
    service = PortfolioOptimizationService()

    try:
        if command == "check_status":
            result = {
                "success": True,
                "scipy_available": SCIPY_AVAILABLE,
                "methods_available": [
                    "black_litterman",
                    "hierarchical_risk_parity",
                    "minimum_variance",
                    "maximum_sharpe",
                    "efficient_frontier"
                ]
            }
            print(json.dumps(result))
        else:
            result = {"success": False, "error": f"Unknown command: {command}"}
            print(json.dumps(result))

    except Exception as e:
        print(json.dumps({"success": False, "error": str(e)}))
        sys.exit(1)


if __name__ == "__main__":
    main()
