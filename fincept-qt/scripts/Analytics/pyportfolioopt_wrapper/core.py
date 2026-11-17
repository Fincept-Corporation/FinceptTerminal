"""
Portfolio Optimization Analytics Module
======================================

Advanced portfolio optimization and risk management using PyPortfolioOpt library.
Provides sophisticated portfolio analytics including efficient frontier analysis,
multiple optimization methods, Black-Litterman modeling, and comprehensive
performance attribution capabilities.

===== DATA SOURCES REQUIRED =====
INPUT:
  - Pandas DataFrame with asset price data (datetime index, asset columns)
  - Market capitalizations (for Black-Litterman)
  - Factor data (optional, for factor models)
  - Portfolio constraints and parameters

OUTPUT:
  - Optimal portfolio weights
  - Performance metrics (returns, volatility, Sharpe ratio)
  - Risk decomposition and attribution
  - Efficient frontier data
  - Backtest results with drawdown analysis
  - Discrete allocation recommendations

PARAMETERS:
  - optimization_method: Portfolio optimization algorithm (default: 'efficient_frontier')
  - objective: Optimization objective (default: 'max_sharpe')
  - risk_free_rate: Risk-free rate for calculations (default: 0.02)
  - expected_returns_method: Return estimation method (default: 'mean_historical_return')
  - risk_model_method: Risk model for covariance (default: 'sample_cov')
  - target_volatility / target_return: targets for efficient_risk / efficient_return
    (defaults 0.15 / 0.12 when not given; the target actually used is reported)
  - sector_mapper + sector_constraints: {ticker: sector} and {sector: [lower, upper]}
  - rebalance_frequency: Days between rebalancing (default: 21)

Nothing in this module writes to stdout: the service prints exactly one JSON payload.
"""

from __future__ import annotations

import json
import sys
import warnings
from dataclasses import dataclass
from datetime import datetime
from typing import Any, Dict, List, Optional, Tuple

import cvxpy as cp
import numpy as np
import pandas as pd

# PyPortfolioOpt imports. pypfopt.plotting (matplotlib) and plotly are imported lazily inside
# the plot_* helpers: the service never plots, and a missing matplotlib must not break it.
from pypfopt import EfficientFrontier, EfficientSemivariance, EfficientCVaR, EfficientCDaR
from pypfopt import expected_returns, risk_models, objective_functions
from pypfopt import HRPOpt, CLA, BlackLittermanModel
from pypfopt.black_litterman import market_implied_prior_returns
from pypfopt.discrete_allocation import DiscreteAllocation, get_latest_prices
from pypfopt.base_optimizer import portfolio_performance
from pypfopt.exceptions import OptimizationError

warnings.filterwarnings('ignore')

# Used only when the caller does not give a target (the target used is always reported).
DEFAULT_TARGET_VOLATILITY = 0.15
DEFAULT_TARGET_RETURN = 0.12

OBJECTIVES = ("max_sharpe", "min_volatility", "max_quadratic_utility", "efficient_risk", "efficient_return")

# Mixed-integer solvers DiscreteAllocation.lp_portfolio can use, in order of preference.
# ECOS_BB is the library default; it only exists when the ecos package is installed.
_MIP_SOLVERS = ("ECOS_BB", "HIGHS", "SCIP", "GLPK_MI", "CBC", "SCIPY")

# Tried, in order, when cvxpy's default solver fails (e.g. OSQP stopping at its iteration limit on
# PyPortfolioOpt's rescaled max_sharpe problem with sector constraints).
_FALLBACK_SOLVERS = ("CLARABEL", "ECOS", "SCS")


def _log(message: str) -> None:
    """Diagnostics go to stderr: stdout carries only the service's JSON payload."""
    print(message, file=sys.stderr)


def solve_with_fallback(build, solve, notes: Optional[List[str]] = None):
    """Build and solve a PyPortfolioOpt cvxpy problem, retrying with other solvers if it fails.

    build(solver) -> a fresh optimiser (solver None = cvxpy's default); solve(optimiser) -> result.
    The default solver is always tried first, so results are unchanged whenever it succeeds.
    Returns (optimiser, result); re-raises the first solver failure if every solver fails.
    """
    installed = set(cp.installed_solvers())
    first_error = None
    for solver in [None] + [s for s in _FALLBACK_SOLVERS if s in installed]:
        opt = build(solver)
        try:
            result = solve(opt)
        except (OptimizationError, cp.error.SolverError) as exc:
            if first_error is None:
                first_error = exc
            _log(f"solver {solver or 'default'} failed: {' '.join(str(exc).split())}")
            continue
        if solver is not None and notes is not None:
            notes.append(f"the default solver failed ({' '.join(str(first_error).split())}); solved with {solver}")
        return opt, result
    raise first_error


def normalise_views(views, view_confidences, tickers) -> Tuple[Dict[str, float], Optional[List[float]]]:
    """Validate absolute Black-Litterman views and line their confidences up with them.

    views: {ticker: expected annual return}. view_confidences: list in the same order as
    `views`, or {ticker: confidence}; each between 0 and 1. Returns (views, confidences or None).
    """
    if views is None:
        return {}, None
    if isinstance(views, pd.Series):
        views = views.to_dict()
    if not isinstance(views, dict):
        raise ValueError("views must be an object {ticker: expected annual return}")
    tickers = [str(t) for t in tickers]
    clean: Dict[str, float] = {}
    for key, value in views.items():
        try:
            number = float(value)
        except (TypeError, ValueError):
            raise ValueError(f"view for {key} is not a number: {value!r}")
        if not np.isfinite(number):
            raise ValueError(f"view for {key} is not a finite number")
        clean[str(key)] = number
    unknown = [k for k in clean if k not in tickers]
    if unknown:
        raise ValueError("views reference tickers not in the price data: " + ", ".join(unknown))
    if view_confidences is None or not clean:
        return clean, None
    if isinstance(view_confidences, dict):
        conf_map = {str(k): v for k, v in view_confidences.items()}
        missing = [k for k in clean if k not in conf_map]
        if missing:
            raise ValueError("view_confidences missing for: " + ", ".join(missing))
        raw = [conf_map[k] for k in clean]
    else:
        raw = list(view_confidences)
        if len(raw) != len(clean):
            raise ValueError(f"view_confidences has {len(raw)} entries but there are {len(clean)} views "
                             "(give one confidence per view, in the same order, or an object {ticker: confidence})")
    confidences = []
    for value in raw:
        try:
            c = float(value)
        except (TypeError, ValueError):
            raise ValueError(f"view confidence is not a number: {value!r}")
        if not (0.0 <= c <= 1.0):
            raise ValueError(f"view confidences must be between 0 and 1 (got {c})")
        confidences.append(c)
    return clean, confidences


def normalise_market_caps(market_caps, tickers) -> pd.Series:
    """{ticker: cap} / Series -> positive float Series in `tickers` order (extra tickers are dropped)."""
    caps = market_caps if isinstance(market_caps, pd.Series) else pd.Series(market_caps, dtype=object)
    caps = caps.copy()
    caps.index = caps.index.map(str)
    caps = pd.to_numeric(caps, errors="coerce")
    tickers = [str(t) for t in tickers]
    missing = [t for t in tickers if t not in caps.index or not np.isfinite(caps.get(t, np.nan))]
    if missing:
        raise ValueError("market_caps missing or not numeric for: " + ", ".join(missing))
    caps = caps.reindex(tickers).astype(float)
    if (caps <= 0).any():
        raise ValueError("market_caps must be positive")
    return caps


def aligned_weights(weights, tickers) -> pd.Series:
    """Weights (dict / Series) -> float Series over `tickers` (unlisted tickers get 0).

    Unknown tickers and non-finite values are rejected rather than silently dropped.
    """
    if weights is None:
        raise ValueError("No weights available. Run optimization first.")
    series = weights.copy() if isinstance(weights, pd.Series) else pd.Series(dict(weights), dtype=object)
    if series.empty:
        raise ValueError("weights are empty")
    series.index = series.index.map(str)
    series = pd.to_numeric(series, errors="coerce")
    bad = [t for t, v in series.items() if not np.isfinite(v)]
    if bad:
        raise ValueError("weights must be finite numbers (bad: " + ", ".join(bad) + ")")
    tickers = [str(t) for t in tickers]
    unknown = [t for t in series.index if t not in tickers]
    if unknown:
        raise ValueError("weights reference tickers not in the price data: " + ", ".join(unknown))
    return series.reindex(tickers, fill_value=0.0).astype(float)


@dataclass
class PyPortfolioOptConfig:
    """Configuration class for PyPortfolioOpt parameters"""

    # Expected Returns Method
    expected_returns_method: str = "mean_historical_return"  # mean_historical_return, ema_historical_return, capm_return

    # Risk Model Method
    risk_model_method: str = "sample_cov"  # sample_cov, semicovariance, exp_cov, shrunk_covariance, ledoit_wolf

    # Optimization Method
    optimization_method: str = "efficient_frontier"  # efficient_frontier, hrp, cla, black_litterman, efficient_semivariance, efficient_cvar, efficient_cdar

    # Objective Function
    objective: str = "max_sharpe"  # max_sharpe, min_volatility, max_quadratic_utility, efficient_risk, efficient_return

    # Risk-free rate
    risk_free_rate: float = 0.02

    # Utility function parameters
    risk_aversion: float = 1
    market_neutral: bool = False

    # Targets for efficient_risk / efficient_return (None -> DEFAULT_TARGET_* and the result says so)
    target_volatility: Optional[float] = None
    target_return: Optional[float] = None

    # Constraints
    weight_bounds: Tuple[float, float] = (0, 1)  # (min_weight, max_weight)
    sector_constraints: Optional[Dict[str, Tuple[float, float]]] = None  # {sector: (lower, upper)}
    sector_mapper: Optional[Dict[str, str]] = None  # {ticker: sector}; required with sector_constraints

    # L2 Regularization
    gamma: float = 0  # L2 regularization parameter

    # Expected returns parameters
    span: int = 500  # Span for EMA
    frequency: int = 252  # Trading days per year

    # Risk model parameters
    delta: float = 0.95  # Exponential decay for exp_cov
    shrinkage_target: str = "constant_variance"  # ledoit_wolf target: constant_variance, single_factor, constant_correlation
    shrinkage_delta: float = 0.2  # fixed shrinkage intensity for shrunk_covariance

    # CVaR/CDaR parameters
    beta: float = 0.95  # Confidence level for CVaR/CDaR

    # Black-Litterman parameters
    tau: float = 0.1  # Uncertainty scaling factor
    market_caps: Optional[pd.Series] = None  # Market capitalizations
    views: Optional[Dict[str, float]] = None  # Absolute views
    view_confidences: Optional[List[float]] = None  # View confidences

    # HRP parameters
    linkage_method: str = "ward"  # ward, single, complete, average
    distance_metric: str = "euclidean"  # euclidean, correlation

    # Discrete allocation parameters
    total_portfolio_value: float = 10000

    # Advanced constraints
    turnover_constraint: Optional[float] = None  # Max turnover from previous weights
    tracking_error_constraint: Optional[float] = None  # Max tracking error vs benchmark


class PyPortfolioOptAnalyticsEngine:
    """
    Advanced PyPortfolioOpt Portfolio Analytics Engine

    Features:
    - Multiple optimization methods (EF, HRP, CLA, Black-Litterman)
    - Various risk models and expected return estimators
    - Advanced constraints and regularization
    - Efficient frontier generation
    - Discrete allocation
    - Performance analysis and plotting
    - Comprehensive backtesting
    """

    def __init__(self, config: PyPortfolioOptConfig = None):
        self.config = config or PyPortfolioOptConfig()
        self.prices = None
        self.returns = None
        self.expected_returns = None
        self.risk_model = None
        self.optimizer = None
        self.weights = None
        # Result of discrete_allocation(). (It used to be stored as self.discrete_allocation, which
        # replaced the method of the same name with None, so the method could never be called.)
        self.discrete_allocation_result = None
        self.performance_metrics = {}
        self.efficient_frontier_data = {}
        self.backtest_results = {}
        self._begin_run()

    def _begin_run(self) -> None:
        """Reset what describes the most recent optimisation (notes, objective, targets, BL state)."""
        self.notes: List[str] = []
        self.objective_used: Optional[str] = None
        self.targets_used: Dict[str, float] = {}
        self.bl_details: Dict[str, Any] = {}
        # Estimates the last optimisation was solved on, when they differ from the historical ones
        # (Black-Litterman posterior). portfolio_performance() reports against these.
        self._perf_mu = None
        self._perf_cov = None

    def load_data(self,
                  prices: pd.DataFrame,
                  start_date: str = None,
                  end_date: str = None) -> None:
        """
        Load price data and calculate returns

        Parameters:
        -----------
        prices : pd.DataFrame
            Price data with datetime index and asset columns
        start_date, end_date : str
            Date range for filtering
        """
        prices = prices.copy()  # never mutate the caller's frame (the backtest passes slices)
        prices.columns = prices.columns.map(str)

        # Convert all columns to numeric (handle string data from frontend)
        for col in prices.columns:
            prices[col] = pd.to_numeric(prices[col], errors='coerce')

        empty = [c for c in prices.columns if prices[c].isna().all()]
        if empty:
            raise ValueError("No numeric prices for: " + ", ".join(empty))

        # Drop any rows with NaN values after conversion
        prices = prices.dropna()

        # Ensure index is datetime
        if not isinstance(prices.index, pd.DatetimeIndex):
            prices.index = pd.to_datetime(prices.index)
        prices = prices.sort_index()

        # Filter by date range
        if start_date or end_date:
            if start_date:
                prices = prices[prices.index >= start_date]
            if end_date:
                prices = prices[prices.index <= end_date]

        if len(prices.columns) < 2:
            raise ValueError("At least two assets are needed for portfolio optimisation")
        if len(prices) < 3:
            raise ValueError(f"Not enough price history: {len(prices)} complete rows (need at least 3)")
        if (prices <= 0).any().any():
            bad = [c for c in prices.columns if (prices[c] <= 0).any()]
            raise ValueError("Prices must be positive (zero or negative values in: " + ", ".join(bad) + ")")

        self.prices = prices
        self.returns = prices.pct_change().dropna()
        self.expected_returns = None
        self.risk_model = None

    def calculate_expected_returns(self, method: str = None) -> pd.Series:
        """
        Calculate expected returns using specified method

        Parameters:
        -----------
        method : str
            Method for calculating expected returns

        Returns:
        --------
        Expected returns series
        """
        method = method or self.config.expected_returns_method

        if method == "mean_historical_return":
            self.expected_returns = expected_returns.mean_historical_return(
                self.prices, frequency=self.config.frequency
            )
        elif method == "ema_historical_return":
            self.expected_returns = expected_returns.ema_historical_return(
                self.prices, span=self.config.span, frequency=self.config.frequency
            )
        elif method == "capm_return":
            # No market series is passed, so PyPortfolioOpt uses the equal-weighted universe as the market
            self.expected_returns = expected_returns.capm_return(
                self.prices, frequency=self.config.frequency
            )
        else:
            raise ValueError(f"Unknown expected returns method: {method}")

        return self.expected_returns

    def calculate_risk_model(self, method: str = None) -> pd.DataFrame:
        """
        Calculate risk model (covariance matrix) using specified method

        Parameters:
        -----------
        method : str
            Method for calculating risk model

        Returns:
        --------
        Covariance matrix
        """
        method = method or self.config.risk_model_method

        if method == "sample_cov":
            self.risk_model = risk_models.sample_cov(
                self.prices, frequency=self.config.frequency
            )
        elif method == "semicovariance":
            self.risk_model = risk_models.semicovariance(
                self.prices, frequency=self.config.frequency
            )
        elif method == "exp_cov":
            self.risk_model = risk_models.exp_cov(
                self.prices, span=self.config.span, frequency=self.config.frequency
            )
        elif method == "shrunk_covariance":
            # Fixed-intensity shrinkage towards the scaled identity (the library's argument is `delta`).
            self.risk_model = risk_models.CovarianceShrinkage(
                self.prices, frequency=self.config.frequency
            ).shrunk_covariance(delta=self.config.shrinkage_delta)
        elif method == "ledoit_wolf":
            # shrinkage_target belongs to ledoit_wolf, not shrunk_covariance.
            self.risk_model = risk_models.CovarianceShrinkage(
                self.prices, frequency=self.config.frequency
            ).ledoit_wolf(shrinkage_target=self.config.shrinkage_target)
        else:
            raise ValueError(f"Unknown risk model method: {method}")

        return self.risk_model

    # ------------------------------------------------------------------
    # Shared helpers for the optimisers
    # ------------------------------------------------------------------

    def _ensure_estimates(self) -> None:
        if self.expected_returns is None:
            self.calculate_expected_returns()
        if self.risk_model is None:
            self.calculate_risk_model()

    def _note(self, text: str) -> None:
        """Record a note once (a solver retry runs the same objective code again)."""
        if text not in self.notes:
            self.notes.append(text)

    def _historical_returns(self) -> pd.DataFrame:
        return expected_returns.returns_from_prices(self.prices).dropna(how="any")

    def _target(self, name: str, default: float, override: float = None) -> float:
        """The caller's target (config field `name`, or an explicit override), else `default` with a note."""
        value = override if override is not None else getattr(self.config, name, None)
        if value is None:
            value = default
            self._note(f"{name} not given; used the default {default:g}")
        try:
            value = float(value)
        except (TypeError, ValueError):
            raise ValueError(f"{name} must be a number")
        if not np.isfinite(value):
            raise ValueError(f"{name} must be a finite number")
        self.targets_used[name] = value
        return value

    def _sector_bounds(self) -> Optional[Tuple[Dict[str, str], Dict[str, float], Dict[str, float]]]:
        """(mapper, lower, upper) from config.sector_constraints, or None when there are none."""
        constraints = self.config.sector_constraints
        if not constraints:
            return None
        mapper = self.config.sector_mapper
        if not mapper or not isinstance(mapper, dict):
            raise ValueError("sector_constraints need config.sector_mapper ({ticker: sector}) "
                             "to know which assets belong to each sector")
        mapper = {str(k): str(v) for k, v in mapper.items()}
        lower, upper = {}, {}
        for sector, bounds in constraints.items():
            try:
                lo, hi = bounds
            except (TypeError, ValueError):
                raise ValueError(f"sector_constraints[{sector!r}] must be [lower, upper]")
            if lo is not None:
                lower[str(sector)] = float(lo)
            if hi is not None:
                upper[str(sector)] = float(hi)
        known = set(mapper.values())
        unknown = [str(s) for s in constraints if str(s) not in known]
        if unknown:
            raise ValueError("sector_constraints name sectors with no assets in sector_mapper: " + ", ".join(unknown))
        return mapper, lower, upper

    def _apply_common_constraints(self, opt, l2: bool = True) -> None:
        """L2 regularisation (mean-variance only) and sector constraints for a cvxpy-based optimiser."""
        if l2 and self.config.gamma:
            # L2 regularisation is an extra objective in PyPortfolioOpt, not a constructor argument.
            opt.add_objective(objective_functions.L2_reg, gamma=self.config.gamma)
        sectors = self._sector_bounds()
        if sectors is not None:
            mapper, lower, upper = sectors
            opt.add_sector_constraints(mapper, lower, upper)

    def _reject_sector_constraints(self, method: str) -> None:
        if self.config.sector_constraints:
            raise ValueError(f"sector_constraints are not supported by {method}; use efficient_frontier, "
                             "black_litterman, efficient_semivariance, efficient_cvar or efficient_cdar")

    def _solve_mean_variance(self, ef: EfficientFrontier):
        """Run config.objective on a mean-variance EfficientFrontier."""
        objective = self.config.objective
        market_neutral = bool(self.config.market_neutral)
        if market_neutral and objective in ("max_sharpe", "min_volatility"):
            raise ValueError(f"market_neutral is not supported with {objective}: PyPortfolioOpt only offers "
                             "market-neutral max_quadratic_utility, efficient_risk and efficient_return")
        if objective == "max_sharpe":
            weights = ef.max_sharpe(risk_free_rate=self.config.risk_free_rate)
        elif objective == "min_volatility":
            weights = ef.min_volatility()
        elif objective == "max_quadratic_utility":
            weights = ef.max_quadratic_utility(risk_aversion=self.config.risk_aversion,
                                               market_neutral=market_neutral)
        elif objective == "efficient_risk":
            weights = ef.efficient_risk(
                target_volatility=self._target("target_volatility", DEFAULT_TARGET_VOLATILITY),
                market_neutral=market_neutral)
        elif objective == "efficient_return":
            weights = ef.efficient_return(
                target_return=self._target("target_return", DEFAULT_TARGET_RETURN),
                market_neutral=market_neutral)
        else:
            raise ValueError(f"Unknown objective: {objective} (expected one of {', '.join(OBJECTIVES)})")
        self.objective_used = objective
        return weights

    # ------------------------------------------------------------------
    # Optimisers
    # ------------------------------------------------------------------

    def hrp_optimization(self) -> pd.Series:
        """
        Perform Hierarchical Risk Parity optimization

        Returns:
        --------
        Optimal portfolio weights
        """
        self._reject_sector_constraints("hrp")
        # Create HRP optimizer
        hrp = HRPOpt(self.returns)

        # Optimize
        self.weights = hrp.optimize(linkage_method=self.config.linkage_method)
        self.objective_used = "hierarchical_risk_parity"

        self.optimizer = hrp
        return pd.Series(self.weights)

    def black_litterman_optimization(self,
                                     market_caps: pd.Series = None,
                                     views: Dict[str, float] = None,
                                     view_confidences: List[float] = None,
                                     tau: float = None) -> pd.Series:
        """
        Perform Black-Litterman optimization

        The prior is the market-implied return vector
        pi = risk_aversion * cov * market_weights + risk_free_rate. Views are absolute
        ({ticker: expected annual return}); with view_confidences (0-1, one per view) the view
        uncertainty uses Idzorek's method, otherwise He-Litterman's default omega. With no views
        the posterior is the prior (returns pi, covariance (1 + tau) * cov), so an unconstrained
        max_sharpe recovers the market-cap portfolio.

        Parameters:
        -----------
        market_caps : pd.Series
            Market capitalizations (equal caps assumed when not given)
        views : Dict[str, float]
            Absolute views on expected returns
        view_confidences : List[float]
            Confidence in each view (0-1)
        tau : float
            Uncertainty scaling factor

        Returns:
        --------
        Optimal portfolio weights
        """
        self._begin_run()
        # `x or default` is wrong for a pandas Series (ambiguous truth value) and for 0 / {}.
        market_caps = market_caps if market_caps is not None else self.config.market_caps
        views = views if views is not None else self.config.views
        view_confidences = view_confidences if view_confidences is not None else self.config.view_confidences
        tau = tau if tau is not None else self.config.tau
        try:
            tau = float(tau)
        except (TypeError, ValueError):
            raise ValueError("tau must be a number")
        if not (0 < tau <= 1):
            raise ValueError("tau should be between 0 and 1")

        if self.risk_model is None:
            self.calculate_risk_model()
        cov = self.risk_model
        tickers = list(cov.columns)

        if market_caps is None:
            caps = pd.Series(1.0, index=tickers)
            caps_source = "equal (market_caps not given)"
            self.notes.append("market_caps not given: equal caps assumed, so the prior is the "
                              "equal-weight market's implied returns")
        else:
            caps = normalise_market_caps(market_caps, tickers)
            caps_source = "given"

        risk_aversion = float(self.config.risk_aversion)
        if risk_aversion <= 0:
            raise ValueError("risk_aversion must be positive for the market-implied prior")
        risk_free_rate = float(self.config.risk_free_rate)
        prior = market_implied_prior_returns(caps, risk_aversion, cov, risk_free_rate=risk_free_rate)

        views_clean, confidences = normalise_views(views, view_confidences, tickers)
        if views_clean:
            bl = BlackLittermanModel(
                cov,
                pi=prior,
                absolute_views=views_clean,
                omega="idzorek" if confidences is not None else None,
                view_confidences=confidences,
                tau=tau,
                risk_aversion=risk_aversion,
            )
            ret_bl = bl.bl_returns()
            cov_bl = bl.bl_cov()
            omega_method = "idzorek" if confidences is not None else "he_litterman_default"
        else:
            ret_bl = prior.copy()
            cov_bl = cov * (1.0 + tau)
            omega_method = None
            self.notes.append("No views given: the posterior is the market-implied prior "
                              "(returns = risk_aversion * cov * market weights + risk_free_rate, "
                              "covariance = (1 + tau) * cov), so an unconstrained max_sharpe "
                              "returns the market-cap weights")

        # Optimize portfolio on the posterior estimates
        def build(solver):
            ef = EfficientFrontier(ret_bl, cov_bl, weight_bounds=self.config.weight_bounds, solver=solver)
            self._apply_common_constraints(ef)
            return ef

        ef, self.weights = solve_with_fallback(build, self._solve_mean_variance, self.notes)

        self.optimizer = ef
        self._perf_mu, self._perf_cov = ret_bl, cov_bl
        self.bl_details = {
            "prior_returns": prior,
            "posterior_returns": ret_bl,
            "market_weights": caps / caps.sum(),
            "market_caps_source": caps_source,
            "views_applied": views_clean,
            "view_confidences": confidences,
            "omega_method": omega_method,
            "tau": tau,
            "risk_aversion": risk_aversion,
        }
        return pd.Series(self.weights)

    def cla_optimization(self) -> pd.Series:
        """
        Perform Critical Line Algorithm optimization

        Returns:
        --------
        Optimal portfolio weights
        """
        self._reject_sector_constraints("cla")
        self._ensure_estimates()

        objective = self.config.objective
        try:
            if objective == "max_sharpe":
                # CLA.max_sharpe() takes no risk-free rate (it maximises mean / volatility). CLA weights
                # sum to one, so w'(mu - rf) = w'mu - rf: running it on returns shifted by rf gives the
                # tangency portfolio for that rate.
                rf = float(self.config.risk_free_rate)
                if float(self.expected_returns.max()) <= rf:
                    raise ValueError("at least one of the assets must have an expected return exceeding the risk-free rate")
                cla = CLA(self.expected_returns - rf, self.risk_model, weight_bounds=self.config.weight_bounds)
                self.weights = cla.max_sharpe()
            elif objective == "min_volatility":
                cla = CLA(self.expected_returns, self.risk_model, weight_bounds=self.config.weight_bounds)
                self.weights = cla.min_volatility()
            else:
                raise ValueError(f"CLA supports the max_sharpe and min_volatility objectives, not {objective}")
        except TypeError as exc:
            # PyPortfolioOpt 1.5.6's CLA calls float() on 1-element arrays, which NumPy 2 rejects.
            if int(np.__version__.split(".")[0]) >= 2 and "0-dimensional" in str(exc):
                raise ValueError("CLA in this PyPortfolioOpt version does not work with NumPy "
                                 f"{np.__version__}; it runs in the NumPy 1.x environment (venv-numpy1)") from exc
            raise
        self.objective_used = objective

        self.optimizer = cla
        return pd.Series(self.weights)

    def efficient_semivariance_optimization(self, target_return: float = None) -> pd.Series:
        """
        Perform efficient semivariance optimization

        Objective mapping: min_volatility -> min_semivariance; max_quadratic_utility;
        efficient_risk (target_volatility is the target annual semideviation); efficient_return.
        PyPortfolioOpt has no max_sharpe here: it falls back to min_semivariance and says so.

        Parameters:
        -----------
        target_return : float
            Target return for efficient_return (overrides config.target_return)

        Returns:
        --------
        Optimal portfolio weights
        """
        if self.expected_returns is None:
            self.calculate_expected_returns()
        historical = self._historical_returns()

        objective = self.config.objective
        market_neutral = bool(self.config.market_neutral)
        if objective not in OBJECTIVES:
            raise ValueError(f"Unknown objective: {objective} (expected one of {', '.join(OBJECTIVES)})")

        def build(solver):
            es = EfficientSemivariance(self.expected_returns, historical,
                                       frequency=self.config.frequency,
                                       weight_bounds=self.config.weight_bounds, solver=solver)
            self._apply_common_constraints(es, l2=False)
            return es

        def solve(es):
            if objective in ("min_volatility", "max_sharpe"):
                if objective == "max_sharpe":
                    self._note("efficient_semivariance has no max_sharpe; minimised semivariance instead")
                self.objective_used = "min_semivariance"
                return es.min_semivariance(market_neutral=market_neutral)
            self.objective_used = objective
            if objective == "max_quadratic_utility":
                return es.max_quadratic_utility(risk_aversion=self.config.risk_aversion,
                                                market_neutral=market_neutral)
            if objective == "efficient_risk":
                target = self._target("target_volatility", DEFAULT_TARGET_VOLATILITY)
                self._note("efficient_semivariance: target_volatility is used as the target annual semideviation")
                return es.efficient_risk(target_semideviation=target, market_neutral=market_neutral)
            target = self._target("target_return", DEFAULT_TARGET_RETURN, override=target_return)
            return es.efficient_return(target_return=target, market_neutral=market_neutral)

        self.optimizer, self.weights = solve_with_fallback(build, solve, self.notes)
        return pd.Series(self.weights)

    def _tail_risk_optimization(self, kind: str, target_return: float = None, beta: float = None) -> pd.Series:
        """Shared body of the CVaR / CDaR optimisers (kind = 'cvar' or 'cdar')."""
        beta = beta if beta is not None else self.config.beta
        if self.expected_returns is None:
            # Without expected returns the optimiser has no ticker labels (weights came back as 0..n-1).
            self.calculate_expected_returns()

        cls = EfficientCVaR if kind == "cvar" else EfficientCDaR
        historical = self._historical_returns()

        objective = self.config.objective
        market_neutral = bool(self.config.market_neutral)
        if objective not in ("min_volatility", "max_sharpe", "efficient_return"):
            raise ValueError(f"efficient_{kind} supports the min_volatility (minimum {kind.upper()}), "
                             f"max_sharpe (falls back to minimum {kind.upper()}) and efficient_return "
                             f"objectives, not {objective}")

        def build(solver):
            opt = cls(self.expected_returns, historical, beta=beta,
                      weight_bounds=self.config.weight_bounds, solver=solver)
            self._apply_common_constraints(opt, l2=False)
            return opt

        def solve(opt):
            if objective == "efficient_return":
                self.objective_used = objective
                target = self._target("target_return", DEFAULT_TARGET_RETURN, override=target_return)
                return opt.efficient_return(target_return=target, market_neutral=market_neutral)
            if objective == "max_sharpe":
                self._note(f"efficient_{kind} has no max_sharpe; minimised {kind.upper()} instead")
            self.objective_used = f"min_{kind}"
            minimiser = opt.min_cvar if kind == "cvar" else opt.min_cdar
            return minimiser(market_neutral=market_neutral)

        self.optimizer, self.weights = solve_with_fallback(build, solve, self.notes)
        return pd.Series(self.weights)

    def efficient_cvar_optimization(self, target_return: float = None, beta: float = None) -> pd.Series:
        """
        Perform efficient CVaR optimization

        Parameters:
        -----------
        target_return : float
            Target return for efficient_return (overrides config.target_return)
        beta : float
            Confidence level for CVaR

        Returns:
        --------
        Optimal portfolio weights
        """
        return self._tail_risk_optimization("cvar", target_return=target_return, beta=beta)

    def efficient_cdar_optimization(self, target_return: float = None, beta: float = None) -> pd.Series:
        """
        Perform efficient CDaR optimization

        Parameters:
        -----------
        target_return : float
            Target return for efficient_return (overrides config.target_return)
        beta : float
            Confidence level for CDaR

        Returns:
        --------
        Optimal portfolio weights
        """
        return self._tail_risk_optimization("cdar", target_return=target_return, beta=beta)

    def optimize_portfolio(self,
                           method: str = None,
                           objective: str = None) -> pd.Series:
        """
        Main portfolio optimization method

        Parameters:
        -----------
        method : str
            Optimization method to use
        objective : str
            Optimization objective

        Returns:
        --------
        Optimal portfolio weights
        """
        method = method or self.config.optimization_method
        objective = objective or self.config.objective

        # Update config
        self.config.objective = objective
        self._begin_run()

        # Accepted by the config but never wired into any optimiser (they need current / benchmark
        # weights the engine does not have): refuse rather than silently ignore.
        for field_name in ("turnover_constraint", "tracking_error_constraint"):
            if getattr(self.config, field_name) is not None:
                raise ValueError(f"{field_name} is not supported by optimize; use min_tracking_error, or "
                                 "advanced_objectives.add_turnover_constraint / add_tracking_error_constraint")

        if method == "efficient_frontier":
            return self._efficient_frontier_optimization()
        elif method == "hrp":
            return self.hrp_optimization()
        elif method == "cla":
            return self.cla_optimization()
        elif method == "black_litterman":
            return self.black_litterman_optimization()
        elif method == "efficient_semivariance":
            return self.efficient_semivariance_optimization()
        elif method == "efficient_cvar":
            return self.efficient_cvar_optimization()
        elif method == "efficient_cdar":
            return self.efficient_cdar_optimization()
        else:
            raise ValueError(f"Unknown optimization method: {method}")

    def _efficient_frontier_optimization(self) -> pd.Series:
        """Internal method for standard efficient frontier optimization"""
        self._ensure_estimates()

        def build(solver):
            # Create efficient frontier
            ef = EfficientFrontier(self.expected_returns, self.risk_model,
                                   weight_bounds=self.config.weight_bounds, solver=solver)
            # L2 regularisation and sector constraints (sector constraints need config.sector_mapper)
            self._apply_common_constraints(ef)
            return ef

        self.optimizer, self.weights = solve_with_fallback(build, self._solve_mean_variance, self.notes)
        return pd.Series(self.weights)

    def generate_efficient_frontier(self, num_points: int = 100) -> Dict[str, Any]:
        """
        Generate efficient frontier data

        The frontier runs from the minimum-volatility portfolio's return (every target below it
        returns that same portfolio) to the highest return the constraints allow.

        Parameters:
        -----------
        num_points : int
            Number of points to generate on frontier

        Returns:
        --------
        Dictionary with frontier data
        """
        try:
            num_points = int(num_points)
        except (TypeError, ValueError):
            raise ValueError("num_points must be an integer")
        if num_points < 2:
            raise ValueError("num_points must be at least 2")
        self._ensure_estimates()
        market_neutral = bool(self.config.market_neutral)

        def new_frontier(solver=None) -> EfficientFrontier:
            ef = EfficientFrontier(self.expected_returns, self.risk_model,
                                   weight_bounds=self.config.weight_bounds, solver=solver)
            self._apply_common_constraints(ef)
            return ef

        if market_neutral:
            low = 0.0  # the least-risk dollar-neutral book holds nothing and earns nothing
        else:
            ef_min, _ = solve_with_fallback(new_frontier, lambda ef: ef.min_volatility())
            low = float(ef_min.portfolio_performance()[0])
        try:
            high = float(new_frontier()._max_return())  # what efficient_return() itself checks against
        except Exception:
            high = float(self.expected_returns.max())
        if high < low:
            high = low
        # Stop a hair short of the maximum: a target equal to it leaves a single feasible point.
        target_returns = np.linspace(low, high - 1e-6 * max(high - low, 1e-12), num_points)

        frontier_volatility = []
        frontier_returns = []
        frontier_sharpe = []
        skipped = 0

        for target_ret in target_returns:
            try:
                ef_temp, _ = solve_with_fallback(
                    new_frontier,
                    lambda ef: ef.efficient_return(target_return=float(target_ret), market_neutral=market_neutral))
                ret, vol, sharpe = ef_temp.portfolio_performance(risk_free_rate=self.config.risk_free_rate)

                frontier_returns.append(ret)
                frontier_volatility.append(vol)
                frontier_sharpe.append(sharpe)

            except Exception as exc:
                skipped += 1
                _log(f"efficient frontier: target return {target_ret:.6g} skipped: {exc}")

        if not frontier_returns:
            raise ValueError("No efficient frontier point could be solved")

        self.efficient_frontier_data = {
            'returns': frontier_returns,
            'volatility': frontier_volatility,
            'sharpe_ratios': frontier_sharpe,
            'num_points': len(frontier_returns),
            'skipped_points': skipped,
        }

        return self.efficient_frontier_data

    def discrete_allocation(self,
                            latest_prices: pd.Series = None,
                            total_portfolio_value: float = None) -> Dict[str, Any]:
        """
        Convert optimal weights to discrete share allocation

        Parameters:
        -----------
        latest_prices : pd.Series
            Latest prices for assets
        total_portfolio_value : float
            Total value to allocate

        Returns:
        --------
        Dictionary with discrete allocation
        """
        if self.weights is None:
            raise ValueError("No weights available. Run optimization first.")

        total_portfolio_value = (total_portfolio_value if total_portfolio_value is not None
                                 else self.config.total_portfolio_value)
        try:
            total_portfolio_value = float(total_portfolio_value)
        except (TypeError, ValueError):
            raise ValueError("total_portfolio_value must be a number")
        if not (np.isfinite(total_portfolio_value) and total_portfolio_value > 0):
            raise ValueError("total_portfolio_value must be greater than zero")

        given = list(dict(self.weights).keys()) if not isinstance(self.weights, pd.Series) else list(self.weights.index)
        given = [str(t) for t in given]
        weights = aligned_weights(self.weights, self.prices.columns).reindex(given)
        weights = {t: float(w) for t, w in weights.items()}

        # Get latest prices if not provided
        if latest_prices is None:
            latest_prices = get_latest_prices(self.prices)
        # lp_portfolio pairs weights and prices by POSITION, so the prices must follow the weights' order.
        latest_prices = latest_prices.reindex(list(weights)).astype(float)

        allocation, leftover, method = None, None, None
        installed = set(cp.installed_solvers())
        for solver in [s for s in _MIP_SOLVERS if s in installed]:
            try:
                da = DiscreteAllocation(weights, latest_prices, total_portfolio_value=total_portfolio_value)
                allocation, leftover = da.lp_portfolio(solver=solver)
                method = f"lp_portfolio ({solver})"
                break
            except Exception as exc:
                _log(f"discrete allocation: lp_portfolio with {solver} failed: {exc}")
        if allocation is None:
            da = DiscreteAllocation(weights, latest_prices, total_portfolio_value=total_portfolio_value)
            allocation, leftover = da.greedy_portfolio()
            method = "greedy_portfolio (no mixed-integer solver succeeded)"

        leftover = float(leftover)
        self.discrete_allocation_result = {
            'allocation': {str(k): int(v) for k, v in allocation.items()},
            'leftover_cash': leftover,
            'total_value': total_portfolio_value,
            'allocated_value': total_portfolio_value - leftover,
            'allocation_method': method,
        }

        return self.discrete_allocation_result

    def _historical_estimates(self) -> Tuple[pd.Series, pd.DataFrame]:
        self._ensure_estimates()
        return self.expected_returns, self.risk_model

    def _performance(self, weights, mu: pd.Series, cov: pd.DataFrame) -> Tuple[float, float, float]:
        w = aligned_weights(weights, mu.index)
        ret, vol, sharpe = portfolio_performance(
            w.values, mu, cov, risk_free_rate=self.config.risk_free_rate
        )
        return float(ret), float(vol), (float(sharpe) if vol > 0 else None)

    def portfolio_performance(self, weights: pd.Series = None) -> Tuple[float, float, float]:
        """
        Calculate portfolio performance metrics

        Uses the estimates the last optimisation was solved on (the Black-Litterman posterior
        after black_litterman_optimization, the historical estimates otherwise).

        Parameters:
        -----------
        weights : pd.Series
            Portfolio weights (uses optimized weights if None)

        Returns:
        --------
        Tuple of (expected_return, volatility, sharpe_ratio)
        """
        weights = weights if weights is not None else self.weights
        mu, cov = self._historical_estimates()
        if self._perf_mu is not None:
            mu, cov = self._perf_mu, self._perf_cov

        ret, vol, sharpe = self._performance(weights, mu, cov)

        self.performance_metrics = {
            'expected_return': ret,
            'volatility': vol,
            'sharpe_ratio': sharpe
        }

        return ret, vol, sharpe

    def historical_performance(self, weights: pd.Series = None) -> Tuple[float, float, float]:
        """Performance under the historical estimates (expected_returns_method / risk_model_method)."""
        weights = weights if weights is not None else self.weights
        mu, cov = self._historical_estimates()
        return self._performance(weights, mu, cov)

    def backtest_strategy(self,
                          rebalance_frequency: int = 21,
                          lookback_period: int = 252,
                          start_date: str = None) -> pd.DataFrame:
        """
        Backtest the optimization strategy using rolling windows

        At each rebalance the optimiser sees only the `lookback_period` closes up to the previous
        day; the weights are bought at that close and held (drifting with prices) until the next
        rebalance, so every day after the first rebalance has exactly one return. If an
        optimisation fails, the previous weights are kept (the failure is reported).

        Parameters:
        -----------
        rebalance_frequency : int
            Rebalancing frequency in days
        lookback_period : int
            Lookback period for optimization
        start_date : str
            Start date for backtest

        Returns:
        --------
        DataFrame with backtest results
        """
        try:
            rebalance_frequency = int(rebalance_frequency)
            lookback_period = int(lookback_period)
        except (TypeError, ValueError):
            raise ValueError("rebalance_frequency and lookback_period must be integers")
        if rebalance_frequency < 1:
            raise ValueError("rebalance_frequency must be at least 1")
        if lookback_period < 3:
            raise ValueError("lookback_period must be at least 3")

        if start_date:
            prices = self.prices[self.prices.index >= start_date]
        else:
            prices = self.prices.copy()

        # Ensure we have enough data
        if len(prices) < lookback_period + rebalance_frequency:
            raise ValueError(f"Insufficient data for backtesting: need lookback_period + rebalance_frequency = "
                             f"{lookback_period + rebalance_frequency} price rows, have {len(prices)}")

        backtest_dates = []
        portfolio_returns = []
        weights_history = []
        failures = []
        current = None

        # Rolling backtest
        for i in range(lookback_period, len(prices), rebalance_frequency):
            rebalance_date = prices.index[i - 1]  # weights are set at this close
            try:
                # Training window: the lookback_period closes up to and including day i-1
                train_prices = prices.iloc[i - lookback_period:i]

                # Create temporary engine for this window
                temp_engine = PyPortfolioOptAnalyticsEngine(self.config)
                temp_engine.load_data(train_prices)

                # Optimize portfolio
                weights = temp_engine.optimize_portfolio()
                current = aligned_weights(weights, prices.columns)
                weights_history.append(current.rename(rebalance_date))
            except Exception as exc:
                failures.append({
                    'date': rebalance_date.isoformat(),
                    'error': " ".join(str(exc).split()) or type(exc).__name__,
                    'action': 'kept previous weights' if current is not None else 'skipped (no weights yet)',
                })
                if current is None:
                    continue

            # Holding period: from the close of day i-1 (entry) to the close before the next rebalance
            window = prices.iloc[i - 1:i + rebalance_frequency]
            growth = window / window.iloc[0]
            # Buy-and-hold value of 1 unit invested at the rebalance; any weight not invested is cash at 0 %.
            value = growth.mul(current, axis=1).sum(axis=1) + (1.0 - current.sum())
            window_returns = value.pct_change().iloc[1:]

            portfolio_returns.extend(window_returns.values)
            backtest_dates.extend(window_returns.index)

        if not portfolio_returns:
            first = failures[0]['error'] if failures else 'no rebalance dates'
            raise ValueError(f"Backtest produced no returns: every rebalance failed (first error: {first})")

        # Create backtest DataFrame
        backtest_df = pd.DataFrame({
            'date': backtest_dates,
            'portfolio_return': portfolio_returns
        }).set_index('date')

        # Calculate performance metrics
        backtest_df['cumulative_return'] = (1 + backtest_df['portfolio_return']).cumprod()
        # Peak includes the starting capital (1.0): a fall straight after the start is a drawdown too.
        running_peak = backtest_df['cumulative_return'].cummax().clip(lower=1.0)
        backtest_df['drawdown'] = backtest_df['cumulative_return'] / running_peak - 1

        # Store results
        periods = self.config.frequency
        annual_return = float(backtest_df['portfolio_return'].mean() * periods)
        annual_vol = float(backtest_df['portfolio_return'].std() * np.sqrt(periods))
        max_drawdown = float(backtest_df['drawdown'].min())
        sharpe_ratio = ((annual_return - self.config.risk_free_rate) / annual_vol
                        if np.isfinite(annual_vol) and annual_vol > 0 else None)

        self.backtest_results = {
            'returns_df': backtest_df,
            'weights_history': weights_history,
            'annual_return': annual_return,
            'annual_volatility': annual_vol,
            'sharpe_ratio': sharpe_ratio,
            'max_drawdown': max_drawdown,
            'calmar_ratio': annual_return / abs(max_drawdown) if max_drawdown < 0 else None,
            'num_rebalances': len(weights_history),
            'failed_rebalances': failures,
        }

        return backtest_df

    def risk_decomposition(self, weights: pd.Series = None) -> Dict[str, pd.Series]:
        """
        Decompose portfolio risk by asset contribution

        Parameters:
        -----------
        weights : pd.Series
            Portfolio weights

        Returns:
        --------
        Dictionary with risk decomposition
        """
        weights = weights if weights is not None else self.weights

        if self.risk_model is None:
            self.calculate_risk_model()
        cov = self._perf_cov if self._perf_cov is not None else self.risk_model

        # Weights over the covariance's tickers, in its order (a request may list fewer, in any order)
        w = aligned_weights(weights, cov.columns)

        # Portfolio volatility
        variance = float(w.values @ cov.values @ w.values)
        if not variance > 0:
            raise ValueError("Portfolio volatility is zero: risk contributions are undefined")
        portfolio_vol = float(np.sqrt(variance))

        # Marginal contribution to risk
        marginal_contrib = pd.Series(cov.values @ w.values / portfolio_vol, index=cov.columns)

        # Component contribution to risk
        component_contrib = w * marginal_contrib

        # Percentage contribution
        pct_contrib = component_contrib / portfolio_vol * 100

        return {
            'marginal_contribution': marginal_contrib,
            'component_contribution': component_contrib,
            'percentage_contribution': pct_contrib,
            'portfolio_volatility': portfolio_vol
        }

    def sensitivity_analysis(self, parameter: str = "risk_free_rate",
                             values: List[float] = None) -> pd.DataFrame:
        """
        Perform sensitivity analysis on parameters

        Parameters:
        -----------
        parameter : str
            Parameter to vary
        values : List[float]
            Values to test

        Returns:
        --------
        DataFrame with sensitivity results (a failed value has nulls and an 'error' entry)
        """
        if values is None:
            if parameter == "risk_free_rate":
                values = [0.0, 0.01, 0.02, 0.03, 0.04, 0.05]
            elif parameter == "risk_aversion":
                values = [0.5, 1.0, 2.0, 3.0, 5.0]
            elif parameter == "gamma":
                values = [0.0, 0.1, 0.5, 1.0, 2.0]
            else:
                raise ValueError(f"Unknown parameter: {parameter}")
        if not hasattr(self.config, parameter):
            raise ValueError(f"Unknown parameter: {parameter}")
        if not isinstance(values, (list, tuple)) or not values:
            raise ValueError("values must be a non-empty list")

        results = []
        original_value = getattr(self.config, parameter)
        saved = (self.weights, self.optimizer, self.notes, self.objective_used, self.targets_used,
                 self.bl_details, self._perf_mu, self._perf_cov)

        try:
            for value in values:
                try:
                    # Set parameter value
                    setattr(self.config, parameter, value)

                    # Optimize (optimize_portfolio returns a pd.Series)
                    weights = pd.Series(self.optimize_portfolio(), dtype=float)
                    ret, vol, sharpe = self.portfolio_performance()

                    results.append({
                        parameter: value,
                        'expected_return': ret,
                        'volatility': vol,
                        'sharpe_ratio': sharpe,
                        'max_weight': float(weights.max()),
                        'min_weight': float(weights.min()),
                        'num_active_assets': int((weights.abs() > 0.001).sum())
                    })

                except Exception as exc:
                    _log(f"sensitivity analysis: {parameter}={value} failed: {exc}")
                    results.append({
                        parameter: value,
                        'expected_return': None,
                        'volatility': None,
                        'sharpe_ratio': None,
                        'max_weight': None,
                        'min_weight': None,
                        'num_active_assets': None,
                        'error': " ".join(str(exc).split()) or type(exc).__name__,
                    })
        finally:
            # Restore original value and the state of the run the caller made before
            setattr(self.config, parameter, original_value)
            (self.weights, self.optimizer, self.notes, self.objective_used, self.targets_used,
             self.bl_details, self._perf_mu, self._perf_cov) = saved

        return pd.DataFrame(results, dtype=object)

    def plot_weights(self, weights: pd.Series = None, top_n: int = 15) -> go.Figure:
        """Plot portfolio weights"""
        import plotly.graph_objects as go
        weights = weights if weights is not None else pd.Series(self.weights)

        # Sort by absolute weight
        weights_sorted = weights.reindex(weights.abs().sort_values(ascending=False).index)[:top_n]

        # Create bar chart
        fig = go.Figure()

        colors = ['red' if w < 0 else 'blue' for w in weights_sorted.values]

        fig.add_trace(go.Bar(
            x=weights_sorted.index,
            y=weights_sorted.values,
            marker_color=colors,
            text=[f'{w:.2%}' for w in weights_sorted.values],
            textposition='outside'
        ))

        fig.update_layout(
            title=f'Portfolio Weights - {self.config.optimization_method.title()}',
            xaxis_title='Assets',
            yaxis_title='Weight',
            yaxis_tickformat='.1%',
            height=600
        )

        return fig

    def plot_efficient_frontier(self) -> go.Figure:
        """Plot efficient frontier"""
        import plotly.graph_objects as go
        if not self.efficient_frontier_data:
            self.generate_efficient_frontier()

        frontier_data = self.efficient_frontier_data

        fig = go.Figure()

        # Plot efficient frontier
        fig.add_trace(go.Scatter(
            x=frontier_data['volatility'],
            y=frontier_data['returns'],
            mode='lines+markers',
            name='Efficient Frontier',
            line=dict(color='blue', width=2),
            marker=dict(size=4)
        ))

        # Add optimal portfolio if available
        if self.weights is not None:
            ret, vol, sharpe = self.portfolio_performance()

            fig.add_trace(go.Scatter(
                x=[vol],
                y=[ret],
                mode='markers',
                name=f'Optimal Portfolio (Sharpe: {sharpe:.2f})',
                marker=dict(color='red', size=12, symbol='star')
            ))

        fig.update_layout(
            title='Efficient Frontier',
            xaxis_title='Volatility (Risk)',
            yaxis_title='Expected Return',
            xaxis_tickformat='.1%',
            yaxis_tickformat='.1%',
            height=600
        )

        return fig

    def plot_risk_decomposition(self) -> go.Figure:
        """Plot risk decomposition"""
        import plotly.graph_objects as go
        if self.weights is None:
            raise ValueError("No weights available. Run optimization first.")

        risk_decomp = self.risk_decomposition()
        pct_contrib = risk_decomp['percentage_contribution']

        # Sort by contribution
        pct_contrib_sorted = pct_contrib.reindex(pct_contrib.abs().sort_values(ascending=False).index)

        fig = go.Figure()

        colors = ['red' if c < 0 else 'green' for c in pct_contrib_sorted.values]

        fig.add_trace(go.Bar(
            x=pct_contrib_sorted.index,
            y=pct_contrib_sorted.values,
            marker_color=colors,
            text=[f'{c:.1f}%' for c in pct_contrib_sorted.values],
            textposition='outside'
        ))

        fig.update_layout(
            title='Risk Contribution by Asset',
            xaxis_title='Assets',
            yaxis_title='Risk Contribution (%)',
            height=600
        )

        return fig

    def plot_backtest_results(self) -> go.Figure:
        """Plot backtest results"""
        import plotly.graph_objects as go
        from plotly.subplots import make_subplots
        if not self.backtest_results:
            raise ValueError("No backtest results. Run backtest_strategy() first.")

        df = self.backtest_results['returns_df']

        # Create subplots
        fig = make_subplots(
            rows=2, cols=1,
            subplot_titles=['Cumulative Returns', 'Drawdown'],
            vertical_spacing=0.1
        )

        # Cumulative returns
        fig.add_trace(
            go.Scatter(x=df.index, y=df['cumulative_return'],
                       name='Portfolio', line=dict(color='blue')),
            row=1, col=1
        )

        # Drawdown
        fig.add_trace(
            go.Scatter(x=df.index, y=df['drawdown'] * 100,
                       name='Drawdown', fill='tonexty',
                       line=dict(color='red')),
            row=2, col=1
        )

        sharpe = self.backtest_results["sharpe_ratio"]
        fig.update_layout(
            height=800,
            title='Backtest Results - Sharpe: ' + (f'{sharpe:.2f}' if sharpe is not None else 'n/a')
        )
        fig.update_yaxes(tickformat='.1%', row=1, col=1)
        fig.update_yaxes(title_text="Drawdown (%)", row=2, col=1)

        return fig

    def plot_correlation_matrix(self) -> go.Figure:
        """Plot correlation matrix heatmap"""
        import plotly.graph_objects as go
        correlation_matrix = self.returns.corr()

        fig = go.Figure(data=go.Heatmap(
            z=correlation_matrix.values,
            x=correlation_matrix.columns,
            y=correlation_matrix.columns,
            colorscale='RdBu',
            zmid=0,
            text=correlation_matrix.round(2).values,
            texttemplate='%{text}',
            textfont={'size': 10}
        ))

        fig.update_layout(
            title='Asset Correlation Matrix',
            height=800,
            width=800
        )

        return fig

    def generate_report(self) -> Dict[str, Any]:
        """Generate comprehensive portfolio report"""
        if self.weights is None:
            raise ValueError("No optimization results available")

        # Get performance metrics
        ret, vol, sharpe = self.portfolio_performance()
        weights = pd.Series(self.weights, dtype=float)

        report = {
            'timestamp': datetime.now().isoformat(),
            'configuration': {
                'optimization_method': self.config.optimization_method,
                'objective': self.config.objective,
                'expected_returns_method': self.config.expected_returns_method,
                'risk_model_method': self.config.risk_model_method,
                'risk_free_rate': self.config.risk_free_rate
            },
            'portfolio_weights': {str(k): float(v) for k, v in weights.items()},
            'performance_metrics': {
                'expected_annual_return': ret,
                'annual_volatility': vol,
                'sharpe_ratio': sharpe
            },
            'portfolio_composition': {
                'number_of_assets': int((weights.abs() > 0.001).sum()),
                'max_weight': float(weights.max()),
                'min_weight': float(weights.min()),
                'weight_sum': float(weights.sum()),
                'long_positions': int((weights > 0.001).sum()),
                'short_positions': int((weights < -0.001).sum())
            }
        }
        if self.objective_used:
            report['objective_used'] = self.objective_used
        if self.targets_used:
            report['targets_used'] = dict(self.targets_used)
        if self.notes:
            report['notes'] = list(self.notes)

        # Add discrete allocation if available
        if self.discrete_allocation_result:
            report['discrete_allocation'] = self.discrete_allocation_result

        # Add backtest results if available
        if self.backtest_results:
            report['backtest_performance'] = {
                'annual_return': self.backtest_results['annual_return'],
                'annual_volatility': self.backtest_results['annual_volatility'],
                'sharpe_ratio': self.backtest_results['sharpe_ratio'],
                'max_drawdown': self.backtest_results['max_drawdown'],
                'calmar_ratio': self.backtest_results['calmar_ratio']
            }

        return report

    def save_report(self, filename: str = None) -> str:
        """Save report to JSON file"""
        if filename is None:
            timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
            filename = f"pypfopt_report_{timestamp}.json"

        report = self.generate_report()

        with open(filename, 'w') as f:
            json.dump(report, f, indent=2, default=str)

        _log(f"Report saved to: {filename}")
        return filename

    def export_weights(self, filename: str = None) -> str:
        """Export weights to CSV"""
        if self.weights is None:
            raise ValueError("No weights available to export")

        if filename is None:
            timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
            filename = f"pypfopt_weights_{timestamp}.csv"

        weights = pd.Series(self.weights, dtype=float)
        weights_df = pd.DataFrame({
            'Asset': list(weights.index),
            'Weight': list(weights.values),
            'Weight_Percent': [w * 100 for w in weights.values]
        }).sort_values('Weight', key=abs, ascending=False)

        weights_df.to_csv(filename, index=False)
        _log(f"Weights exported to: {filename}")
        return filename


# Example usage and demonstration functions
def create_sample_pypfopt_config() -> PyPortfolioOptConfig:
    """Create sample configuration for PyPortfolioOpt"""
    return PyPortfolioOptConfig(
        optimization_method="efficient_frontier",
        objective="max_sharpe",
        expected_returns_method="mean_historical_return",
        risk_model_method="sample_cov",
        risk_free_rate=0.02,
        weight_bounds=(0, 0.4),  # Max 40% in any single asset
        gamma=0.1,  # Small L2 regularization
        total_portfolio_value=100000
    )


def demo_pypfopt_analytics():
    """Demonstration of PyPortfolioOpt analytics engine (interactive use only; prints to stdout)"""
    print("=== PyPortfolioOpt Portfolio Analytics Demo ===\n")

    # Create sample data
    import os
    try:
        import fincept_yahoo as fy  # the shared Yahoo layer (bar cache, limiter)
    except ImportError:  # scripts/ is three levels up from this file
        sys.path.append(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))
        import fincept_yahoo as fy

    # Sample assets - diversified portfolio
    assets = ['AAPL', 'GOOGL', 'MSFT', 'AMZN', 'TSLA', 'JPM', 'JNJ', 'PG', 'XOM', 'GLD']

    try:
        # Download data (auto_adjust=False: the frame then carries 'Adj Close')
        print("Downloading sample data...")
        prices = fy.download(assets, start='2018-01-01', end='2023-12-31', auto_adjust=False)['Adj Close']

    except:
        # Create synthetic data if yfinance fails
        print("Creating synthetic data...")
        np.random.seed(42)
        dates = pd.date_range('2018-01-01', '2023-12-31', freq='D')
        n_assets = len(assets)

        # Generate correlated returns
        correlation_matrix = np.random.uniform(0.1, 0.7, (n_assets, n_assets))
        np.fill_diagonal(correlation_matrix, 1.0)
        correlation_matrix = (correlation_matrix + correlation_matrix.T) / 2

        returns = pd.DataFrame(
            np.random.multivariate_normal(
                np.random.uniform(0.0005, 0.0015, n_assets),
                correlation_matrix * 0.0004,
                len(dates)
            ),
            index=dates,
            columns=assets
        )

        # Convert to prices
        prices = (1 + returns).cumprod() * 100

    # Create configuration
    config = create_sample_pypfopt_config()

    # Initialize engine
    engine = PyPortfolioOptAnalyticsEngine(config)
    engine.load_data(prices)

    print(f"Data loaded: {len(prices)} periods, {len(prices.columns)} assets\n")

    # Standard efficient frontier optimization
    print("Performing Max Sharpe optimization...")
    weights = engine.optimize_portfolio()
    ret, vol, sharpe = engine.portfolio_performance()
