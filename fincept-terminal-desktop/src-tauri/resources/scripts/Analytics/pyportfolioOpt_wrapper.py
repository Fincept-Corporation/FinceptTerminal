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
  - rebalance_frequency: Days between rebalancing (default: 21)
  - confidence_level: Statistical confidence level (default: 0.95)
  - num_simulations: Monte Carlo simulations (default: 1000)
"""


import pandas as pd
import numpy as np
import matplotlib.pyplot as plt
import plotly.graph_objects as go
import plotly.express as px
from plotly.subplots import make_subplots
import seaborn as sns
import warnings
from typing import Dict, List, Optional, Union, Tuple, Any
from dataclasses import dataclass, field
from datetime import datetime, timedelta
import json
import cvxpy as cp

# PyPortfolioOpt imports
from pypfopt import EfficientFrontier, EfficientSemivariance, EfficientCVaR, EfficientCDaR
from pypfopt import expected_returns, risk_models, objective_functions
from pypfopt import HRPOpt, CLA, BlackLittermanModel
from pypfopt.discrete_allocation import DiscreteAllocation, get_latest_prices
from pypfopt.plotting import plot_covariance, plot_dendrogram, plot_efficient_frontier
from pypfopt import base_optimizer
from pypfopt.utils import portfolio_performance

warnings.filterwarnings('ignore')


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

    # Constraints
    weight_bounds: Tuple[float, float] = (0, 1)  # (min_weight, max_weight)
    sector_constraints: Optional[Dict[str, Tuple[float, float]]] = None

    # L2 Regularization
    gamma: float = 0  # L2 regularization parameter

    # Expected returns parameters
    span: int = 500  # Span for EMA
    frequency: int = 252  # Trading days per year

    # Risk model parameters
    delta: float = 0.95  # Exponential decay for exp_cov
    shrinkage_target: str = "constant_variance"  # For shrunk_covariance

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
        self.discrete_allocation = None
        self.performance_metrics = {}
        self.efficient_frontier_data = {}
        self.backtest_results = {}

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
        # Filter by date range
        if start_date or end_date:
            if start_date:
                prices = prices[prices.index >= start_date]
            if end_date:
                prices = prices[prices.index <= end_date]

        self.prices = prices
        self.returns = prices.pct_change().dropna()

        print(f"Data loaded: {len(self.prices)} periods, {len(self.prices.columns)} assets")

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
            # Requires market data - using mean as fallback
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
            self.risk_model = risk_models.CovarianceShrinkage(
                self.prices, frequency=self.config.frequency
            ).shrunk_covariance(shrinkage_target=self.config.shrinkage_target)
        elif method == "ledoit_wolf":
            self.risk_model = risk_models.CovarianceShrinkage(
                self.prices, frequency=self.config.frequency
            ).ledoit_wolf()
        else:
            raise ValueError(f"Unknown risk model method: {method}")

        return self.risk_model

    def black_litterman_optimization(self,
                                     market_caps: pd.Series = None,
                                     views: Dict[str, float] = None,
                                     view_confidences: List[float] = None,
                                     tau: float = None) -> pd.Series:
        """
        Perform Black-Litterman optimization

        Parameters:
        -----------
        market_caps : pd.Series
            Market capitalizations
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
        market_caps = market_caps or self.config.market_caps
        views = views or self.config.views
        view_confidences = view_confidences or self.config.view_confidences
        tau = tau or self.config.tau

        # If no market caps provided, use equal weights
        if market_caps is None:
            market_caps = pd.Series(1, index=self.prices.columns)

        # Calculate risk model if not done
        if self.risk_model is None:
            self.calculate_risk_model()

        # Create Black-Litterman model
        bl = BlackLittermanModel(
            cov_matrix=self.risk_model,
            pi="market",  # Use market-implied returns
            market_caps=market_caps,
            tau=tau
        )

        # Add views if provided
        if views:
            view_dict = {}
            for asset, view_return in views.items():
                if asset in self.prices.columns:
                    view_dict[asset] = view_return

            if view_dict:
                bl.add_views(view_dict, view_confidences)

        # Get posterior estimates
        ret_bl = bl.bl_returns()
        cov_bl = bl.bl_cov()

        # Optimize portfolio
        ef = EfficientFrontier(ret_bl, cov_bl, weight_bounds=self.config.weight_bounds)

        if self.config.objective == "max_sharpe":
            self.weights = ef.max_sharpe(risk_free_rate=self.config.risk_free_rate)
        elif self.config.objective == "min_volatility":
            self.weights = ef.min_volatility()
        else:
            raise ValueError(f"Objective {self.config.objective} not supported for Black-Litterman")

        self.optimizer = ef
        return pd.Series(self.weights)


def cla_optimization(self) -> pd.Series:
    """
    Perform Critical Line Algorithm optimization

    Returns:
    --------
    Optimal portfolio weights
    """
    # Calculate expected returns and risk model if not done
    if self.expected_returns is None:
        self.calculate_expected_returns()
    if self.risk_model is None:
        self.calculate_risk_model()

    # Create CLA optimizer
    cla = CLA(self.expected_returns, self.risk_model, weight_bounds=self.config.weight_bounds)

    if self.config.objective == "max_sharpe":
        self.weights = cla.max_sharpe(risk_free_rate=self.config.risk_free_rate)
    elif self.config.objective == "min_volatility":
        self.weights = cla.min_volatility()
    else:
        # For other objectives, use efficient frontier approach
        self.weights = cla.max_sharpe(risk_free_rate=self.config.risk_free_rate)

    self.optimizer = cla
    return pd.Series(self.weights)


def efficient_semivariance_optimization(self, target_return: float = None) -> pd.Series:
    """
    Perform efficient semivariance optimization

    Parameters:
    -----------
    target_return : float
        Target return for optimization

    Returns:
    --------
    Optimal portfolio weights
    """
    # Use historical returns for semivariance
    historical_rets = expected_returns.returns_from_prices(self.prices)

    # Create efficient semivariance optimizer
    es = EfficientSemivariance(self.expected_returns, historical_rets,
                               weight_bounds=self.config.weight_bounds)

    if self.config.objective == "min_volatility":
        if target_return:
            self.weights = es.efficient_semivariance(target_semivariance=target_return)
        else:
            # Minimize semivariance for target return of 10%
            self.weights = es.efficient_semivariance(target_semivariance=0.1)
    else:
        raise ValueError(f"Objective {self.config.objective} not supported for semivariance")

    self.optimizer = es
    return pd.Series(self.weights)


def efficient_cvar_optimization(self, target_return: float = None, beta: float = None) -> pd.Series:
    """
    Perform efficient CVaR optimization

    Parameters:
    -----------
    target_return : float
        Target return for optimization
    beta : float
        Confidence level for CVaR

    Returns:
    --------
    Optimal portfolio weights
    """
    beta = beta or self.config.beta
    historical_rets = expected_returns.returns_from_prices(self.prices)

    # Create efficient CVaR optimizer
    ec = EfficientCVaR(self.expected_returns, historical_rets, beta=beta,
                       weight_bounds=self.config.weight_bounds)

    if self.config.objective == "min_volatility":
        if target_return:
            self.weights = ec.efficient_return(target_return=target_return)
        else:
            self.weights = ec.min_cvar()
    elif self.config.objective == "max_sharpe":
        # CVaR doesn't have direct max_sharpe, use min_cvar
        self.weights = ec.min_cvar()
    else:
        self.weights = ec.min_cvar()

    self.optimizer = ec
    return pd.Series(self.weights)


def efficient_cdar_optimization(self, target_return: float = None, beta: float = None) -> pd.Series:
    """
    Perform efficient CDaR optimization

    Parameters:
    -----------
    target_return : float
        Target return for optimization
    beta : float
        Confidence level for CDaR

    Returns:
    --------
    Optimal portfolio weights
    """
    beta = beta or self.config.beta
    historical_rets = expected_returns.returns_from_prices(self.prices)

    # Create efficient CDaR optimizer
    ecd = EfficientCDaR(self.expected_returns, historical_rets, beta=beta,
                        weight_bounds=self.config.weight_bounds)

    if self.config.objective == "min_volatility":
        if target_return:
            self.weights = ecd.efficient_return(target_return=target_return)
        else:
            self.weights = ecd.min_cdar()
    else:
        self.weights = ecd.min_cdar()

    self.optimizer = ecd
    return pd.Series(self.weights)


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
    # Calculate expected returns and risk model if not done
    if self.expected_returns is None:
        self.calculate_expected_returns()
    if self.risk_model is None:
        self.calculate_risk_model()

    # Create efficient frontier
    ef = EfficientFrontier(self.expected_returns, self.risk_model,
                           weight_bounds=self.config.weight_bounds, gamma=self.config.gamma)

    # Add sector constraints if provided
    if self.config.sector_constraints:
        for sector, (lower, upper) in self.config.sector_constraints.items():
            # This would require sector mapping - simplified for demo
            pass

    # Optimize based on objective
    if self.config.objective == "max_sharpe":
        self.weights = ef.max_sharpe(risk_free_rate=self.config.risk_free_rate)
    elif self.config.objective == "min_volatility":
        self.weights = ef.min_volatility()
    elif self.config.objective == "max_quadratic_utility":
        self.weights = ef.max_quadratic_utility(risk_aversion=self.config.risk_aversion,
                                                market_neutral=self.config.market_neutral)
    elif self.config.objective == "efficient_risk":
        # Requires target volatility - using 15% as default
        self.weights = ef.efficient_risk(target_volatility=0.15,
                                         market_neutral=self.config.market_neutral)
    elif self.config.objective == "efficient_return":
        # Requires target return - using 12% as default
        self.weights = ef.efficient_return(target_return=0.12,
                                           market_neutral=self.config.market_neutral)
    else:
        raise ValueError(f"Unknown objective: {self.config.objective}")

    self.optimizer = ef
    return pd.Series(self.weights)


def generate_efficient_frontier(self, num_points: int = 100) -> Dict[str, Any]:
    """
    Generate efficient frontier data

    Parameters:
    -----------
    num_points : int
        Number of points to generate on frontier

    Returns:
    --------
    Dictionary with frontier data
    """
    # Calculate expected returns and risk model if not done
    if self.expected_returns is None:
        self.calculate_expected_returns()
    if self.risk_model is None:
        self.calculate_risk_model()

    # Generate frontier
    ef = EfficientFrontier(self.expected_returns, self.risk_model,
                           weight_bounds=self.config.weight_bounds)

    # Get min and max return bounds
    min_ret = self.expected_returns.min()
    max_ret = self.expected_returns.max()

    # Generate target returns
    target_returns = np.linspace(min_ret, max_ret * 0.95, num_points)

    frontier_volatility = []
    frontier_returns = []
    frontier_sharpe = []

    for target_ret in target_returns:
        try:
            ef_temp = EfficientFrontier(self.expected_returns, self.risk_model,
                                        weight_bounds=self.config.weight_bounds)
            weights = ef_temp.efficient_return(target_return=target_ret,
                                               market_neutral=self.config.market_neutral)
            ret, vol, sharpe = ef_temp.portfolio_performance(risk_free_rate=self.config.risk_free_rate)

            frontier_returns.append(ret)
            frontier_volatility.append(vol)
            frontier_sharpe.append(sharpe)

        except Exception:
            continue

    self.efficient_frontier_data = {
        'returns': frontier_returns,
        'volatility': frontier_volatility,
        'sharpe_ratios': frontier_sharpe,
        'num_points': len(frontier_returns)
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

    total_portfolio_value = total_portfolio_value or self.config.total_portfolio_value

    # Get latest prices if not provided
    if latest_prices is None:
        latest_prices = get_latest_prices(self.prices)

    # Perform discrete allocation
    da = DiscreteAllocation(self.weights, latest_prices, total_portfolio_value=total_portfolio_value)
    allocation, leftover = da.lp_portfolio()

    self.discrete_allocation = {
        'allocation': allocation,
        'leftover_cash': leftover,
        'total_value': total_portfolio_value,
        'allocated_value': total_portfolio_value - leftover
    }

    return self.discrete_allocation


def portfolio_performance(self, weights: pd.Series = None) -> Tuple[float, float, float]:
    """
    Calculate portfolio performance metrics

    Parameters:
    -----------
    weights : pd.Series
        Portfolio weights (uses optimized weights if None)

    Returns:
    --------
    Tuple of (expected_return, volatility, sharpe_ratio)
    """
    weights = weights if weights is not None else pd.Series(self.weights)

    if self.expected_returns is None:
        self.calculate_expected_returns()
    if self.risk_model is None:
        self.calculate_risk_model()

    # Calculate performance
    ret, vol, sharpe = portfolio_performance(
        weights, self.expected_returns, self.risk_model,
        risk_free_rate=self.config.risk_free_rate
    )

    self.performance_metrics = {
        'expected_return': ret,
        'volatility': vol,
        'sharpe_ratio': sharpe
    }

    return ret, vol, sharpe


def backtest_strategy(self,
                      rebalance_frequency: int = 21,
                      lookback_period: int = 252,
                      start_date: str = None) -> pd.DataFrame:
    """
    Backtest the optimization strategy using rolling windows

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
    if start_date:
        prices = self.prices[self.prices.index >= start_date]
    else:
        prices = self.prices.copy()

    # Ensure we have enough data
    if len(prices) < lookback_period + rebalance_frequency:
        raise ValueError("Insufficient data for backtesting")

    backtest_dates = []
    portfolio_returns = []
    weights_history = []

    # Rolling backtest
    for i in range(lookback_period, len(prices) - rebalance_frequency, rebalance_frequency):
        try:
            # Training window
            train_prices = prices.iloc[i - lookback_period:i]

            # Create temporary engine for this window
            temp_engine = PyPortfolioOptAnalyticsEngine(self.config)
            temp_engine.load_data(train_prices)

            # Optimize portfolio
            weights = temp_engine.optimize_portfolio()
            weights_series = pd.Series(weights)
            weights_history.append(weights_series)

            # Forward period for evaluation
            forward_prices = prices.iloc[i:i + rebalance_frequency]
            forward_returns = forward_prices.pct_change().dropna()

            # Calculate portfolio returns
            portfolio_rets = (forward_returns * weights_series).sum(axis=1)
            portfolio_returns.extend(portfolio_rets.values)
            backtest_dates.extend(forward_returns.index)

        except Exception as e:
            print(f"Error at period {i}: {e}")
            continue

    # Create backtest DataFrame
    backtest_df = pd.DataFrame({
        'date': backtest_dates,
        'portfolio_return': portfolio_returns
    }).set_index('date')

    # Calculate performance metrics
    backtest_df['cumulative_return'] = (1 + backtest_df['portfolio_return']).cumprod()
    backtest_df['drawdown'] = (backtest_df['cumulative_return'] /
                               backtest_df['cumulative_return'].expanding().max() - 1)

    # Store results
    annual_return = backtest_df['portfolio_return'].mean() * 252
    annual_vol = backtest_df['portfolio_return'].std() * np.sqrt(252)
    max_drawdown = backtest_df['drawdown'].min()
    sharpe_ratio = (annual_return - self.config.risk_free_rate) / annual_vol if annual_vol > 0 else 0

    self.backtest_results = {
        'returns_df': backtest_df,
        'weights_history': weights_history,
        'annual_return': annual_return,
        'annual_volatility': annual_vol,
        'sharpe_ratio': sharpe_ratio,
        'max_drawdown': max_drawdown,
        'calmar_ratio': annual_return / abs(max_drawdown) if max_drawdown < 0 else 0
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
    weights = weights if weights is not None else pd.Series(self.weights)

    if self.risk_model is None:
        self.calculate_risk_model()

    # Portfolio volatility
    portfolio_vol = np.sqrt(weights.T @ self.risk_model @ weights)

    # Marginal contribution to risk
    marginal_contrib = self.risk_model @ weights / portfolio_vol

    # Component contribution to risk
    component_contrib = weights * marginal_contrib

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
    DataFrame with sensitivity results
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

    results = []
    original_value = getattr(self.config, parameter)

    for value in values:
        try:
            # Set parameter value
            setattr(self.config, parameter, value)

            # Optimize
            weights = self.optimize_portfolio()
            ret, vol, sharpe = self.portfolio_performance()

            results.append({
                parameter: value,
                'expected_return': ret,
                'volatility': vol,
                'sharpe_ratio': sharpe,
                'max_weight': max(weights.values()),
                'min_weight': min(weights.values()),
                'num_active_assets': sum(1 for w in weights.values() if abs(w) > 0.001)
            })

        except Exception as e:
            print(f"Error for {parameter}={value}: {e}")

    # Restore original value
    setattr(self.config, parameter, original_value)

    return pd.DataFrame(results)


def plot_weights(self, weights: pd.Series = None, top_n: int = 15) -> go.Figure:
    """Plot portfolio weights"""
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
    if not self.efficient_frontier_data:
        print("Generating efficient frontier...")
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

    fig.update_layout(
        height=800,
        title=f'Backtest Results - Sharpe: {self.backtest_results["sharpe_ratio"]:.2f}'
    )
    fig.update_yaxes(tickformat='.1%', row=1, col=1)
    fig.update_yaxes(title_text="Drawdown (%)", row=2, col=1)

    return fig


def plot_correlation_matrix(self) -> go.Figure:
    """Plot correlation matrix heatmap"""
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

    report = {
        'timestamp': datetime.now().isoformat(),
        'configuration': {
            'optimization_method': self.config.optimization_method,
            'objective': self.config.objective,
            'expected_returns_method': self.config.expected_returns_method,
            'risk_model_method': self.config.risk_model_method,
            'risk_free_rate': self.config.risk_free_rate
        },
        'portfolio_weights': dict(self.weights),
        'performance_metrics': {
            'expected_annual_return': ret,
            'annual_volatility': vol,
            'sharpe_ratio': sharpe
        },
        'portfolio_composition': {
            'number_of_assets': len([w for w in self.weights.values() if abs(w) > 0.001]),
            'max_weight': max(self.weights.values()),
            'min_weight': min(self.weights.values()),
            'weight_sum': sum(self.weights.values()),
            'long_positions': len([w for w in self.weights.values() if w > 0.001]),
            'short_positions': len([w for w in self.weights.values() if w < -0.001])
        }
    }

    # Add discrete allocation if available
    if self.discrete_allocation:
        report['discrete_allocation'] = self.discrete_allocation

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

    print(f"Report saved to: {filename}")
    return filename


def export_weights(self, filename: str = None) -> str:
    """Export weights to CSV"""
    if self.weights is None:
        raise ValueError("No weights available to export")

    if filename is None:
        timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
        filename = f"pypfopt_weights_{timestamp}.csv"

    weights_df = pd.DataFrame({
        'Asset': list(self.weights.keys()),
        'Weight': list(self.weights.values()),
        'Weight_Percent': [w * 100 for w in self.weights.values()]
    }).sort_values('Weight', key=abs, ascending=False)

    weights_df.to_csv(filename, index=False)
    print(f"Weights exported to: {filename}")
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
    """Demonstration of PyPortfolioOpt analytics engine"""
    print("=== PyPortfolioOpt Portfolio Analytics Demo ===\n")

    # Create sample data
    import yfinance as yf

    # Sample assets - diversified portfolio
    assets = ['AAPL', 'GOOGL', 'MSFT', 'AMZN', 'TSLA', 'JPM', 'JNJ', 'PG', 'XOM', 'GLD']

    try:
        # Download data
        print("Downloading sample data...")
        prices = yf.download(assets, start='2018-01-01', end='2023-12-31')['Adj Close']

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
