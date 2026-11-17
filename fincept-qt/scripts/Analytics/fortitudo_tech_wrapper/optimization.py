"""
Fortitudo.tech Portfolio Optimization Wrapper
==============================================
Mean-Variance and Mean-CVaR optimization.

* Mean-variance problems are quadratic programs. They are solved with SLSQP on a
  SCALED objective (variance divided by the average asset variance) with analytic
  gradients and a tight tolerance. Unscaled, a daily variance (~1e-4) sits below
  SLSQP's default ftol (1e-6) and the solver stops at its starting point (equal
  weights) and reports success.
* The highest attainable return under the weight bounds (the top of the efficient
  frontier) is a linear programme (scipy.optimize.linprog / HiGHS).
* Mean-CVaR problems are solved exactly as the Rockafellar-Uryasev linear programme
  (min z + 1/alpha * sum_s p_s * max(-r_s.w - z, 0)) instead of running a smooth
  optimiser on the non-smooth empirical CVaR.

`alpha` in this module is the TAIL probability (0.05 = 95 % CVaR).

Usage:
    python optimization.py
"""

import numpy as np
import pandas as pd
from typing import Dict, Any, Optional, Union
import warnings

warnings.filterwarnings('ignore')

# Try to import optimization backends
try:
    from scipy.optimize import minimize, linprog, Bounds
    from scipy import sparse as _sparse
    SCIPY_AVAILABLE = True
except ImportError:
    SCIPY_AVAILABLE = False

try:
    import fortitudo.tech as ft
    FORTITUDO_AVAILABLE = True
except ImportError:
    FORTITUDO_AVAILABLE = False
    ft = None


_TOL = 1e-7  # feasibility tolerance used to accept a solver result


def _weight_bounds(n: int, long_only: bool, max_weight: Optional[float] = None,
                   min_weight: Optional[float] = None):
    """Per-asset (lower, upper) bounds shared by every optimiser.

    max_weight <= 0 / None means "no cap" (the UI only sends a cap when > 0).
    Returns (lower, upper) or raises ValueError with a plain message when the
    bounds cannot hold a fully-invested portfolio.
    """
    upper = 1.0 if (max_weight is None or float(max_weight) <= 0) else float(max_weight)
    if long_only:
        lower = 0.0 if min_weight is None else max(0.0, float(min_weight))
    else:
        lower = -1.0 if min_weight is None else float(min_weight)
    if lower > upper:
        raise ValueError(f"min_weight ({lower}) is above max_weight ({upper})")
    if n * upper < 1.0 - 1e-12:
        raise ValueError(
            f"max_weight {upper:g} is too small for {n} assets: a fully invested portfolio "
            f"needs max_weight >= 1/{n} = {1.0 / n:.4f}")
    if n * lower > 1.0 + 1e-12:
        raise ValueError(
            f"min_weight {lower:g} is too large for {n} assets: it needs min_weight <= 1/{n}")
    return lower, upper


def _feasible(w: np.ndarray, lower: float, upper: float) -> bool:
    return (abs(float(np.sum(w)) - 1.0) < 1e-6
            and float(np.min(w)) >= lower - _TOL
            and float(np.max(w)) <= upper + _TOL)


def _polish(w: np.ndarray, lower: float, upper: float) -> np.ndarray:
    """Remove solver dust (|w| ~ 1e-12 outside the bounds) and re-normalise."""
    w = np.clip(np.asarray(w, dtype=float), lower, upper)
    s = w.sum()
    return w / s if abs(s) > 1e-12 else w


def _max_return_lp(mu: np.ndarray, lower: float, upper: float):
    """Highest attainable mean return under sum(w)=1 and the bounds (LP)."""
    n = len(mu)
    res = linprog(-mu, A_eq=np.ones((1, n)), b_eq=[1.0],
                  bounds=[(lower, upper)] * n, method="highs")
    if res.status != 0:
        return None, None
    w = _polish(res.x, lower, upper)
    return w, float(w @ mu)


class MeanVarianceOptimizer:
    """
    Mean-Variance Portfolio Optimization

    Methods:
        - minimum_variance(): global minimum variance portfolio
        - efficient_frontier(): efficient frontier points
        - max_sharpe(): maximum Sharpe ratio portfolio
        - target_return(): minimum variance portfolio with return >= target
    All returns / volatilities / Sharpe ratios are PER PERIOD of the input data
    (daily for daily returns); annualisation is the caller's job.
    """

    def __init__(
        self,
        returns: Union[pd.DataFrame, np.ndarray],
        probabilities: Optional[np.ndarray] = None,
        risk_free_rate: float = 0.0
    ):
        """
        Args:
            returns: Asset returns matrix (n_scenarios x n_assets)
            probabilities: Scenario probabilities (optional)
            risk_free_rate: Risk-free rate PER PERIOD (same unit as the returns)
        """
        if isinstance(returns, pd.DataFrame):
            self.asset_names = list(returns.columns)
            self.returns = returns.values.astype(float)
        else:
            self.returns = np.asarray(returns, dtype=float)
            self.asset_names = [f'Asset_{i}' for i in range(self.returns.shape[1])]

        self.n_scenarios, self.n_assets = self.returns.shape
        self.probabilities = probabilities
        self.risk_free_rate = float(risk_free_rate or 0.0)

        if probabilities is not None:
            p = probabilities.flatten() if probabilities.ndim == 2 else probabilities
            self.mean_returns = self.returns.T @ p
            centered = self.returns - self.mean_returns
            self.cov_matrix = (centered.T * p) @ centered
        else:
            self.mean_returns = np.mean(self.returns, axis=0)
            self.cov_matrix = np.cov(self.returns, rowvar=False)
        self.cov_matrix = np.atleast_2d(self.cov_matrix)

        # Scale factors so the solver works on O(1) numbers.
        avg_var = float(np.mean(np.diag(self.cov_matrix)))
        self._var_scale = 1.0 / avg_var if avg_var > 0 else 1.0
        mu_abs = float(np.max(np.abs(self.mean_returns))) if self.n_assets else 0.0
        self._ret_scale = 1.0 / mu_abs if mu_abs > 0 else 1.0

    # ---- metrics -----------------------------------------------------------
    def _portfolio_variance(self, weights: np.ndarray) -> float:
        return float(weights @ self.cov_matrix @ weights)

    def _portfolio_volatility(self, weights: np.ndarray) -> float:
        return float(np.sqrt(max(self._portfolio_variance(weights), 0.0)))

    def _portfolio_return(self, weights: np.ndarray) -> float:
        return float(weights @ self.mean_returns)

    def _sharpe_ratio(self, weights: np.ndarray) -> Optional[float]:
        """Per-period Sharpe ratio; None when volatility is zero (undefined)."""
        vol = self._portfolio_volatility(weights)
        if vol <= 0:
            return None
        return (self._portfolio_return(weights) - self.risk_free_rate) / vol

    def _result(self, weights: np.ndarray, **extra) -> Dict[str, Any]:
        out = {
            "success": True,
            "weights": dict(zip(self.asset_names, [float(x) for x in weights])),
            "expected_return": self._portfolio_return(weights),
            "volatility": self._portfolio_volatility(weights),
            "sharpe_ratio": self._sharpe_ratio(weights),
            "variance": self._portfolio_variance(weights),
        }
        out.update(extra)
        return out

    # ---- core QP -----------------------------------------------------------
    def _min_variance_qp(self, lower: float, upper: float,
                         min_return: Optional[float] = None):
        """min w'Cw s.t. sum(w)=1, lower<=w<=upper [, w.mu >= min_return]."""
        n = self.n_assets
        C = self.cov_matrix * self._var_scale
        cons = [{'type': 'eq', 'fun': lambda w: np.sum(w) - 1.0,
                 'jac': lambda w: np.ones(n)}]
        if min_return is not None:
            mu_s = self.mean_returns * self._ret_scale
            tgt_s = float(min_return) * self._ret_scale
            cons.append({'type': 'ineq', 'fun': lambda w: w @ mu_s - tgt_s,
                         'jac': lambda w: mu_s})
        x0 = np.full(n, 1.0 / n)
        res = minimize(lambda w: float(w @ C @ w), x0, jac=lambda w: 2.0 * (C @ w),
                       method='SLSQP', bounds=Bounds(np.full(n, lower), np.full(n, upper)),
                       constraints=cons, options={'ftol': 1e-14, 'maxiter': 2000})
        w = np.asarray(res.x, dtype=float)
        ok = _feasible(w, lower, upper)
        if ok and min_return is not None:
            ok = self._portfolio_return(w) >= float(min_return) - 1e-6 * max(1.0, abs(float(min_return)))
        if not ok:
            return None, str(res.message)
        return _polish(w, lower, upper), None

    def minimum_variance(
        self,
        long_only: bool = True,
        max_weight: Optional[float] = None,
        min_weight: Optional[float] = None
    ) -> Dict[str, Any]:
        """Global minimum variance portfolio under the bounds."""
        if not SCIPY_AVAILABLE:
            return {"success": False, "error": "scipy not available"}
        try:
            lower, upper = _weight_bounds(self.n_assets, long_only, max_weight, min_weight)
        except ValueError as e:
            return {"success": False, "error": str(e)}
        w, err = self._min_variance_qp(lower, upper)
        if w is None:
            return {"success": False, "error": f"minimum-variance solver did not converge: {err}"}
        return self._result(w)

    def max_sharpe(
        self,
        long_only: bool = True,
        max_weight: Optional[float] = None,
        min_weight: Optional[float] = None
    ) -> Dict[str, Any]:
        """Maximum Sharpe ratio portfolio (per-period excess return / volatility)."""
        if not SCIPY_AVAILABLE:
            return {"success": False, "error": "scipy not available"}
        n = self.n_assets
        try:
            lower, upper = _weight_bounds(n, long_only, max_weight, min_weight)
        except ValueError as e:
            return {"success": False, "error": str(e)}
        C = self.cov_matrix
        mu_e = self.mean_returns - self.risk_free_rate
        k = np.sqrt(252.0)  # objective on an annual scale -> O(0.1..3)

        def neg_sharpe(w):
            v = float(w @ C @ w)
            if v <= 0:
                return 0.0
            return -k * float(w @ mu_e) / np.sqrt(v)

        def neg_sharpe_grad(w):
            v = float(w @ C @ w)
            if v <= 0:
                return np.zeros(n)
            s = np.sqrt(v)
            return -k * (mu_e / s - float(w @ mu_e) * (C @ w) / s ** 3)

        starts = [np.full(n, 1.0 / n)]
        gmv, _ = self._min_variance_qp(lower, upper)
        if gmv is not None:
            starts.append(gmv)
        w_max, _ = _max_return_lp(self.mean_returns, lower, upper)
        if w_max is not None:
            starts.append(w_max)

        best, best_f = None, None
        cons = [{'type': 'eq', 'fun': lambda w: np.sum(w) - 1.0, 'jac': lambda w: np.ones(n)}]
        for x0 in starts:
            res = minimize(neg_sharpe, x0, jac=neg_sharpe_grad, method='SLSQP',
                           bounds=Bounds(np.full(n, lower), np.full(n, upper)),
                           constraints=cons, options={'ftol': 1e-14, 'maxiter': 2000})
            w = np.asarray(res.x, dtype=float)
            if not _feasible(w, lower, upper):
                continue
            w = _polish(w, lower, upper)
            f = neg_sharpe(w)
            if best is None or f < best_f:
                best, best_f = w, f
        if best is None:
            return {"success": False, "error": "max-Sharpe solver did not converge"}
        return self._result(best)

    def max_return(self, long_only: bool = True, max_weight: Optional[float] = None,
                   min_weight: Optional[float] = None) -> Dict[str, Any]:
        """Highest attainable mean return under the bounds (LP)."""
        try:
            lower, upper = _weight_bounds(self.n_assets, long_only, max_weight, min_weight)
        except ValueError as e:
            return {"success": False, "error": str(e)}
        w, r = _max_return_lp(self.mean_returns, lower, upper)
        if w is None:
            return {"success": False, "error": "maximum-return LP failed"}
        return self._result(w)

    def target_return(
        self,
        target: float,
        long_only: bool = True,
        max_weight: Optional[float] = None,
        min_weight: Optional[float] = None
    ) -> Dict[str, Any]:
        """Minimum variance portfolio whose expected return is at least `target`.

        A target below the global-minimum-variance return gives the GMV portfolio
        (lower targets are dominated); a target above the attainable maximum is an error.
        """
        if not SCIPY_AVAILABLE:
            return {"success": False, "error": "scipy not available"}
        try:
            lower, upper = _weight_bounds(self.n_assets, long_only, max_weight, min_weight)
        except ValueError as e:
            return {"success": False, "error": str(e)}
        _, r_max = _max_return_lp(self.mean_returns, lower, upper)
        if r_max is not None and float(target) > r_max + 1e-12:
            return {"success": False,
                    "error": (f"target return {float(target):.6g} per period is above the highest "
                              f"attainable {r_max:.6g} under these weight limits")}
        w, err = self._min_variance_qp(lower, upper, min_return=float(target))
        if w is None:
            return {"success": False, "error": f"target-return solver did not converge: {err}"}
        return self._result(w, target_return=float(target),
                            target_binding=bool(self._portfolio_return(w) <= float(target) + 1e-9))

    def efficient_frontier(
        self,
        n_points: int = 20,
        long_only: bool = True,
        max_weight: Optional[float] = None,
        min_weight: Optional[float] = None
    ) -> Dict[str, Any]:
        """Efficient frontier from the minimum-variance portfolio to the maximum-return one."""
        min_var_result = self.minimum_variance(long_only, max_weight, min_weight)
        if not min_var_result["success"]:
            return min_var_result
        lower, upper = _weight_bounds(self.n_assets, long_only, max_weight, min_weight)

        min_return = min_var_result["expected_return"]
        w_top, max_return = _max_return_lp(self.mean_returns, lower, upper)
        if w_top is None:
            return {"success": False, "error": "maximum-return LP failed"}

        def _pt(res):
            return {"expected_return": res["expected_return"], "volatility": res["volatility"],
                    "sharpe_ratio": res["sharpe_ratio"], "weights": res["weights"]}

        if max_return <= min_return + 1e-12 * max(1.0, abs(min_return)):
            return {
                "success": True,
                "frontier": [_pt(min_var_result)],
                "n_points": 1,
                "requested_points": int(n_points),
                "failed_points": 0,
                "min_variance_portfolio": min_var_result,
                "note": "Degenerate frontier: the minimum-variance portfolio already has the highest attainable return"
            }

        frontier = [_pt(min_var_result)]
        failed = 0
        for target in np.linspace(min_return, max_return, n_points)[1:]:
            # The top target sits on the boundary of the feasible set; back off by a
            # hair so the inequality is numerically satisfiable.
            t = min(float(target), max_return - 1e-10 * max(1.0, abs(max_return)))
            res = self.target_return(t, long_only, max_weight, min_weight)
            if res["success"]:
                frontier.append(_pt(res))
            else:
                failed += 1

        return {
            "success": True,
            "frontier": frontier,
            "n_points": len(frontier),
            "requested_points": int(n_points),
            "failed_points": failed,
            "min_variance_portfolio": min_var_result
        }


class MeanCVaROptimizer:
    """
    Mean-CVaR Portfolio Optimization (Rockafellar-Uryasev linear programme).

    CVaR / VaR here are POSITIVE LOSSES per period at tail probability `alpha`.
    """

    def __init__(
        self,
        returns: Union[pd.DataFrame, np.ndarray],
        probabilities: Optional[np.ndarray] = None,
        alpha: float = 0.05,
        risk_free_rate: float = 0.0
    ):
        """
        Args:
            returns: Asset returns matrix (n_scenarios x n_assets)
            probabilities: Scenario probabilities (optional)
            alpha: Tail probability for CVaR (0.05 = 95 % CVaR)
            risk_free_rate: Risk-free rate per period
        """
        if isinstance(returns, pd.DataFrame):
            self.asset_names = list(returns.columns)
            self.returns = returns.values.astype(float)
        else:
            self.returns = np.asarray(returns, dtype=float)
            self.asset_names = [f'Asset_{i}' for i in range(self.returns.shape[1])]

        self.n_scenarios, self.n_assets = self.returns.shape
        self.alpha = float(alpha)
        self.risk_free_rate = float(risk_free_rate or 0.0)

        if probabilities is not None:
            p = probabilities.flatten() if probabilities.ndim == 2 else probabilities
            self.probabilities = np.asarray(p, dtype=float) / float(np.sum(p))
        else:
            self.probabilities = np.ones(self.n_scenarios) / self.n_scenarios

        self.mean_returns = self.returns.T @ self.probabilities

    def _portfolio_return(self, weights: np.ndarray) -> float:
        return float(weights @ self.mean_returns)

    def _portfolio_cvar(self, weights: np.ndarray) -> float:
        """Rockafellar-Uryasev CVaR (positive loss) of the portfolio."""
        loss = -(self.returns @ weights)
        var = self._portfolio_var(weights)
        excess = np.maximum(loss - var, 0.0)
        return float(var + (self.probabilities @ excess) / self.alpha)

    def _portfolio_var(self, weights: np.ndarray) -> float:
        """VaR (positive loss): the smallest loss level l with P(loss > l) <= alpha."""
        loss = -(self.returns @ weights)
        if len(loss) == 0:
            return 0.0
        order = np.argsort(loss)[::-1]          # largest losses first
        cum = np.cumsum(self.probabilities[order])
        k = int(np.searchsorted(cum, self.alpha - 1e-12))  # first index where tail mass >= alpha
        return float(loss[order[min(k, len(loss) - 1)]])

    def _solve_lp(self, lower: float, upper: float, min_return: Optional[float] = None):
        S, n = self.returns.shape
        p = self.probabilities
        # variables: w (n), z (1), u (S)
        c = np.concatenate([np.zeros(n), [1.0], p / self.alpha])
        A_ub = _sparse.hstack([_sparse.csr_matrix(-self.returns),
                               _sparse.csr_matrix(-np.ones((S, 1))),
                               -_sparse.identity(S, format="csr")], format="csr")
        b_ub = np.zeros(S)
        if min_return is not None:
            row = _sparse.csr_matrix(np.concatenate([-self.mean_returns, [0.0], np.zeros(S)]))
            A_ub = _sparse.vstack([A_ub, row], format="csr")
            b_ub = np.concatenate([b_ub, [-float(min_return)]])
        A_eq = _sparse.csr_matrix(np.concatenate([np.ones(n), [0.0], np.zeros(S)]))
        bounds = [(lower, upper)] * n + [(None, None)] + [(0.0, None)] * S
        res = linprog(c, A_ub=A_ub, b_ub=b_ub, A_eq=A_eq, b_eq=[1.0],
                      bounds=bounds, method="highs")
        if res.status != 0:
            return None, res.message
        return _polish(res.x[:n], lower, upper), None

    def _result(self, w, **extra):
        out = {
            "success": True,
            "weights": dict(zip(self.asset_names, [float(x) for x in w])),
            "expected_return": self._portfolio_return(w),
            "cvar": self._portfolio_cvar(w),
            "var": self._portfolio_var(w),
            "alpha": self.alpha
        }
        out.update(extra)
        return out

    def minimum_cvar(
        self,
        long_only: bool = True,
        max_weight: Optional[float] = None,
        min_weight: Optional[float] = None
    ) -> Dict[str, Any]:
        """Minimum CVaR portfolio."""
        if not SCIPY_AVAILABLE:
            return {"success": False, "error": "scipy not available"}
        try:
            lower, upper = _weight_bounds(self.n_assets, long_only, max_weight, min_weight)
        except ValueError as e:
            return {"success": False, "error": str(e)}
        w, err = self._solve_lp(lower, upper)
        if w is None:
            return {"success": False, "error": f"minimum-CVaR LP failed: {err}"}
        return self._result(w)

    def target_return(
        self,
        target: float,
        long_only: bool = True,
        max_weight: Optional[float] = None,
        min_weight: Optional[float] = None
    ) -> Dict[str, Any]:
        """Minimum CVaR portfolio with expected return >= target."""
        if not SCIPY_AVAILABLE:
            return {"success": False, "error": "scipy not available"}
        try:
            lower, upper = _weight_bounds(self.n_assets, long_only, max_weight, min_weight)
        except ValueError as e:
            return {"success": False, "error": str(e)}
        _, r_max = _max_return_lp(self.mean_returns, lower, upper)
        if r_max is not None and float(target) > r_max + 1e-12:
            return {"success": False,
                    "error": (f"target return {float(target):.6g} per period is above the highest "
                              f"attainable {r_max:.6g} under these weight limits")}
        w, err = self._solve_lp(lower, upper, min_return=float(target))
        if w is None:
            return {"success": False, "error": f"target-return CVaR LP failed: {err}"}
        return self._result(w, target_return=float(target))

    def efficient_frontier(
        self,
        n_points: int = 20,
        long_only: bool = True,
        max_weight: Optional[float] = None
    ) -> Dict[str, Any]:
        """CVaR-efficient frontier from the minimum-CVaR portfolio to the maximum-return one."""
        min_cvar_result = self.minimum_cvar(long_only, max_weight)
        if not min_cvar_result["success"]:
            return min_cvar_result
        lower, upper = _weight_bounds(self.n_assets, long_only, max_weight)
        min_return = min_cvar_result["expected_return"]
        _, max_return = _max_return_lp(self.mean_returns, lower, upper)

        def _pt(res):
            return {"expected_return": res["expected_return"], "cvar": res["cvar"],
                    "var": res["var"], "weights": res["weights"]}

        if max_return is None or max_return <= min_return + 1e-12 * max(1.0, abs(min_return)):
            return {"success": True, "frontier": [_pt(min_cvar_result)], "n_points": 1,
                    "min_cvar_portfolio": min_cvar_result, "alpha": self.alpha,
                    "note": "Degenerate frontier: the minimum-CVaR portfolio already has the highest attainable return"}

        frontier = [_pt(min_cvar_result)]
        failed = 0
        for target in np.linspace(min_return, max_return, n_points)[1:]:
            t = min(float(target), max_return - 1e-10 * max(1.0, abs(max_return)))
            result = self.target_return(t, long_only, max_weight)
            if result["success"]:
                frontier.append(_pt(result))
            else:
                failed += 1

        return {
            "success": True,
            "frontier": frontier,
            "n_points": len(frontier),
            "requested_points": int(n_points),
            "failed_points": failed,
            "min_cvar_portfolio": min_cvar_result,
            "alpha": self.alpha
        }


def main():
    """Test optimization functions."""
    print("=== Portfolio Optimization Test ===\n")

    # Generate test data
    np.random.seed(42)
    n_scenarios = 200
    asset_names = ['Stocks', 'Bonds', 'Commodities', 'Cash']

    # Different return characteristics for each asset
    returns = np.column_stack([
        np.random.randn(n_scenarios) * 0.02 + 0.0003,  # Stocks: high vol, high return
        np.random.randn(n_scenarios) * 0.005 + 0.0001,  # Bonds: low vol, low return
        np.random.randn(n_scenarios) * 0.015 + 0.0002,  # Commodities: med vol
        np.random.randn(n_scenarios) * 0.001 + 0.0001,  # Cash: very low vol
    ])

    returns_df = pd.DataFrame(returns, columns=asset_names)

    print("1. Mean-Variance Optimization")
    print("-" * 50)

    mv_optimizer = MeanVarianceOptimizer(returns_df, risk_free_rate=0.0001)

    min_var = mv_optimizer.minimum_variance()
    if min_var["success"]:
        print("  Minimum Variance Portfolio:")
        print(f"    Return: {min_var['expected_return']:.6f}")
        print(f"    Volatility: {min_var['volatility']:.6f}")
        print(f"    Weights: {min_var['weights']}")

    max_sharpe = mv_optimizer.max_sharpe()
    if max_sharpe["success"]:
        print("\n  Maximum Sharpe Portfolio:")
        print(f"    Return: {max_sharpe['expected_return']:.6f}")
        print(f"    Volatility: {max_sharpe['volatility']:.6f}")
        print(f"    Sharpe (per period): {max_sharpe['sharpe_ratio']:.4f}")
        print(f"    Weights: {max_sharpe['weights']}")

    print("\n2. Mean-CVaR Optimization")
    print("-" * 50)

    cvar_optimizer = MeanCVaROptimizer(returns_df, alpha=0.05)

    min_cvar = cvar_optimizer.minimum_cvar()
    if min_cvar["success"]:
        print("  Minimum CVaR Portfolio (95%):")
        print(f"    Return: {min_cvar['expected_return']:.6f}")
        print(f"    CVaR: {min_cvar['cvar']:.6f}")
        print(f"    VaR: {min_cvar['var']:.6f}")
        print(f"    Weights: {min_cvar['weights']}")

    print("\n3. Efficient Frontier (5 points)")
    print("-" * 50)
    frontier = mv_optimizer.efficient_frontier(n_points=5)
    if frontier["success"]:
        print("  Return    Volatility")
        for pt in frontier["frontier"]:
            print(f"  {pt['expected_return']:.6f}  {pt['volatility']:.6f}")

    print("\n=== Test: PASSED ===")


if __name__ == "__main__":
    main()
