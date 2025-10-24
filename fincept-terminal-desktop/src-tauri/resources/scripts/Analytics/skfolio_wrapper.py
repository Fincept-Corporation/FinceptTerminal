"""
Advanced Portfolio Analytics Module
===================================

Sophisticated portfolio optimization and analytics using skfolio library.
Provides state-of-the-art portfolio management techniques including hierarchical
risk parity, mean-variance optimization with advanced risk measures, factor-based
modeling, copula simulation, and comprehensive performance attribution.

===== DATA SOURCES REQUIRED =====
INPUT:
  - Pandas DataFrame with asset price/return data (datetime index)
  - Factor model data (optional, for enhanced analysis)
  - Market benchmarks and indices
  - Portfolio constraints and optimization parameters

OUTPUT:
  - Optimal portfolio weights using multiple optimization methods
  - Risk decomposition and performance attribution
  - Efficient frontier and optimization landscapes
  - Hyperparameter tuning and model validation results
  - Walk-forward backtest with performance metrics
  - Stress testing and scenario analysis outcomes

PARAMETERS:
  - optimization_method: Portfolio optimization algorithm (default: 'mean_risk')
  - objective_function: Optimization objective (default: 'maximize_ratio')
  - risk_measure: Risk metric for optimization (default: 'variance')
  - covariance_estimator: Covariance estimation method (default: 'empirical')
  - train_test_split_ratio: Data split ratio (default: 0.7)
  - lookback_window: Trading days for analysis (default: 252)
  - rebalance_frequency: Rebalancing frequency in days (default: 21)
  - confidence_level: Statistical confidence level (default: 0.95)
  - l1_coef: L1 regularization strength (default: 0.0)
  - l2_coef: L2 regularization strength (default: 0.0)
"""

import pandas as pd
import numpy as np
import matplotlib.pyplot as plt
import seaborn as sns
import plotly.graph_objects as go
import plotly.express as px
from plotly.subplots import make_subplots
import warnings
from typing import Dict, List, Optional, Union, Tuple, Any
from dataclasses import dataclass, field
from datetime import datetime, timedelta
import json

# skfolio imports
from sklearn.model_selection import train_test_split, GridSearchCV, RandomizedSearchCV, KFold
from sklearn.pipeline import Pipeline
from scipy.stats import loguniform

from skfolio import RatioMeasure, RiskMeasure
from skfolio.datasets import load_sp500_dataset, load_factors_dataset
from skfolio.distribution import VineCopula
from skfolio.model_selection import CombinatorialPurgedCV, WalkForward, cross_val_predict
from skfolio.moments import (
    DenoiseCovariance, DetoneCovariance, EWMu, GerberCovariance,
    ShrunkMu, LedoitWolf
)
from skfolio.optimization import (
    MeanRisk, HierarchicalRiskParity, NestedClustersOptimization,
    ObjectiveFunction, RiskBudgeting, MaximumDiversification,
    EqualWeighted, InverseVolatility, Random
)
from skfolio.pre_selection import SelectKExtremes
from skfolio.preprocessing import prices_to_returns
from skfolio.prior import (
    BlackLitterman, EmpiricalPrior, EntropyPooling, FactorModel,
    OpinionPooling, SyntheticData
)
from skfolio.uncertainty_set import BootstrapMuUncertaintySet

warnings.filterwarnings('ignore')

@dataclass
class PortfolioConfig:
    """Configuration class for portfolio optimization parameters"""

    # Basic Settings
    optimization_method: str = "mean_risk"  # mean_risk, risk_parity, hrp, max_div, equal_weight
    objective_function: str = "minimize_risk"  # minimize_risk, maximize_return, maximize_ratio, maximize_utility
    risk_measure: str = "variance"  # variance, cvar, semi_variance, max_drawdown, etc.

    # Data Settings
    train_test_split_ratio: float = 0.7
    lookback_window: int = 252  # trading days
    rebalance_frequency: int = 21  # days

    # Risk Management
    min_weights: Optional[Dict[str, float]] = None
    max_weights: Union[float, Dict[str, float]] = 1.0
    max_weight_single_asset: float = 0.3
    transaction_costs: Optional[Dict[str, float]] = None

    # Advanced Parameters
    l1_coef: float = 0.0  # L1 regularization
    l2_coef: float = 0.0  # L2 regularization
    risk_aversion: float = 1.0
    confidence_level: float = 0.95

    # Covariance Estimation
    covariance_estimator: str = "empirical"  # empirical, ledoit_wolf, gerber, denoise
    mu_estimator: str = "empirical"  # empirical, shrunk, ew

    # Cross-validation
    cv_method: str = "kfold"  # kfold, walk_forward, combinatorial_purged
    cv_folds: int = 5

    # Black-Litterman
    views: Optional[List[str]] = None
    tau: float = 0.025

    # Factor Model
    use_factor_model: bool = False
    factor_views: Optional[List[str]] = None

    # Clustering (for HRP)
    clustering_method: str = "hierarchical"
    linkage_method: str = "ward"

    # Uncertainty Sets
    use_uncertainty_set: bool = False
    bootstrap_samples: int = 1000

    # Constraints
    group_constraints: Optional[List[str]] = None
    turnover_constraint: Optional[float] = None
    tracking_error_constraint: Optional[float] = None

    def __post_init__(self):
        """Validate configuration parameters"""
        if not 0 < self.train_test_split_ratio < 1:
            raise ValueError("train_test_split_ratio must be between 0 and 1")
        if self.confidence_level <= 0 or self.confidence_level >= 1:
            raise ValueError("confidence_level must be between 0 and 1")


class PortfolioAnalyticsEngine:
    """
    Advanced Portfolio Analytics Engine using skfolio

    Features:
    - Multiple optimization methods
    - Risk management and analysis
    - Stress testing and scenario analysis
    - Performance attribution
    - Interactive visualizations
    - Backtesting capabilities
    """

    def __init__(self, config: PortfolioConfig = None):
        self.config = config or PortfolioConfig()
        self.prices = None
        self.returns = None
        self.factors = None
        self.factor_returns = None
        self.model = None
        self.portfolio = None
        self.backtest_results = {}
        self.optimization_history = []

    def load_data(self,
                  prices: pd.DataFrame,
                  factors: pd.DataFrame = None,
                  start_date: str = None,
                  end_date: str = None) -> None:
        """
        Load price and factor data

        Parameters:
        -----------
        prices : pd.DataFrame
            Asset prices with datetime index and asset columns
        factors : pd.DataFrame, optional
            Factor data with datetime index
        start_date, end_date : str, optional
            Date range for analysis
        """
        # Filter by date range if provided
        if start_date or end_date:
            if start_date:
                prices = prices[prices.index >= start_date]
                if factors is not None:
                    factors = factors[factors.index >= start_date]
            if end_date:
                prices = prices[prices.index <= end_date]
                if factors is not None:
                    factors = factors[factors.index <= end_date]

        self.prices = prices
        self.returns = prices_to_returns(prices)

        if factors is not None:
            self.factors = factors
            self.factor_returns = prices_to_returns(factors) if 'price' in str(factors.columns).lower() else factors

        print(f"Data loaded: {len(self.prices)} periods, {len(self.prices.columns)} assets")
        if factors is not None:
            print(f"Factor data: {len(self.factors.columns)} factors")

    def _get_estimators(self) -> Tuple[Any, Any]:
        """Get covariance and mu estimators based on config"""

        # Covariance estimators
        covariance_estimators = {
            "empirical": None,
            "ledoit_wolf": LedoitWolf(),
            "gerber": GerberCovariance(),
            "denoise": DenoiseCovariance(),
            "detone": DetoneCovariance()
        }

        # Mu estimators
        mu_estimators = {
            "empirical": None,
            "shrunk": ShrunkMu(),
            "ew": EWMu(alpha=0.2)
        }

        cov_est = covariance_estimators.get(self.config.covariance_estimator)
        mu_est = mu_estimators.get(self.config.mu_estimator)

        return cov_est, mu_est

    def _build_prior_estimator(self) -> Any:
        """Build prior estimator based on configuration"""
        cov_est, mu_est = self._get_estimators()

        # Base empirical prior
        empirical_prior = EmpiricalPrior(
            mu_estimator=mu_est,
            covariance_estimator=cov_est
        )

        # Black-Litterman
        if self.config.views:
            return BlackLitterman(
                views=self.config.views,
                tau=self.config.tau,
                prior_estimator=empirical_prior
            )

        # Factor Model
        if self.config.use_factor_model and self.factor_returns is not None:
            factor_prior = BlackLitterman(
                views=self.config.factor_views,
                tau=self.config.tau
            ) if self.config.factor_views else empirical_prior

            return FactorModel(
                factor_prior_estimator=factor_prior
            )

        return empirical_prior

    def _build_model(self) -> Any:
        """Build optimization model based on configuration"""

        # Get objective function
        obj_functions = {
            "minimize_risk": ObjectiveFunction.MINIMIZE_RISK,
            "maximize_return": ObjectiveFunction.MAXIMIZE_RETURN,
            "maximize_ratio": ObjectiveFunction.MAXIMIZE_RATIO,
            "maximize_utility": ObjectiveFunction.MAXIMIZE_UTILITY
        }

        # Get risk measure
        risk_measures = {
            "variance": RiskMeasure.VARIANCE,
            "semi_variance": RiskMeasure.SEMI_VARIANCE,
            "cvar": RiskMeasure.CVAR,
            "evar": RiskMeasure.EVAR,
            "max_drawdown": RiskMeasure.MAX_DRAWDOWN,
            "cdar": RiskMeasure.CDAR,
            "ulcer_index": RiskMeasure.ULCER_INDEX
        }

        prior_estimator = self._build_prior_estimator()
        uncertainty_set = BootstrapMuUncertaintySet() if self.config.use_uncertainty_set else None

        # Build model based on optimization method
        if self.config.optimization_method == "mean_risk":
            model = MeanRisk(
                objective_function=obj_functions[self.config.objective_function],
                risk_measure=risk_measures[self.config.risk_measure],
                prior_estimator=prior_estimator,
                mu_uncertainty_set_estimator=uncertainty_set,
                min_weights=self.config.min_weights,
                max_weights=self.config.max_weights,
                transaction_costs=self.config.transaction_costs,
                l1_coef=self.config.l1_coef,
                l2_coef=self.config.l2_coef,
                risk_aversion=self.config.risk_aversion
            )

        elif self.config.optimization_method == "risk_parity":
            model = RiskBudgeting(
                risk_measure=risk_measures[self.config.risk_measure],
                prior_estimator=prior_estimator,
                min_weights=self.config.min_weights,
                max_weights=self.config.max_weights
            )

        elif self.config.optimization_method == "hrp":
            model = HierarchicalRiskParity(
                risk_measure=risk_measures[self.config.risk_measure],
                prior_estimator=prior_estimator,
                linkage_method=self.config.linkage_method
            )

        elif self.config.optimization_method == "max_div":
            model = MaximumDiversification(
                prior_estimator=prior_estimator,
                min_weights=self.config.min_weights,
                max_weights=self.config.max_weights
            )

        elif self.config.optimization_method == "equal_weight":
            model = EqualWeighted()

        elif self.config.optimization_method == "inverse_vol":
            model = InverseVolatility(prior_estimator=prior_estimator)

        else:
            raise ValueError(f"Unknown optimization method: {self.config.optimization_method}")

        return model

    def optimize_portfolio(self,
                          train_size: float = None,
                          verbose: bool = True) -> Dict[str, Any]:
        """
        Optimize portfolio using configured parameters

        Returns:
        --------
        Dict with optimization results
        """
        if self.returns is None:
            raise ValueError("No data loaded. Call load_data() first.")

        train_size = train_size or self.config.train_test_split_ratio

        # Split data
        if self.factor_returns is not None:
            X_train, X_test, factors_train, factors_test = train_test_split(
                self.returns, self.factor_returns,
                test_size=1-train_size, shuffle=False
            )
        else:
            X_train, X_test = train_test_split(
                self.returns, test_size=1-train_size, shuffle=False
            )
            factors_train = factors_test = None

        # Build and fit model
        self.model = self._build_model()

        if factors_train is not None:
            self.model.fit(X_train, factors_train)
        else:
            self.model.fit(X_train)

        # Generate portfolio
        self.portfolio = self.model.predict(X_test)

        # Store results
        results = {
            'weights': dict(zip(self.returns.columns, self.model.weights_)),
            'train_period': (X_train.index[0], X_train.index[-1]),
            'test_period': (X_test.index[0], X_test.index[-1]),
            'model_type': self.config.optimization_method,
            'objective': self.config.objective_function,
            'risk_measure': self.config.risk_measure
        }

        # Performance metrics
        if hasattr(self.portfolio, 'sharpe_ratio'):
            results.update({
                'sharpe_ratio': self.portfolio.sharpe_ratio,
                'sortino_ratio': getattr(self.portfolio, 'sortino_ratio', None),
                'calmar_ratio': getattr(self.portfolio, 'calmar_ratio', None),
                'max_drawdown': getattr(self.portfolio, 'max_drawdown', None),
                'volatility': getattr(self.portfolio, 'annualized_volatility', None),
                'return': getattr(self.portfolio, 'annualized_mean', None)
            })

        self.optimization_history.append(results)

        if verbose:
            print(f"\nPortfolio Optimization Complete:")
            print(f"Method: {self.config.optimization_method}")
            print(f"Objective: {self.config.objective_function}")
            print(f"Risk Measure: {self.config.risk_measure}")
            print(f"Training Period: {results['train_period'][0]} to {results['train_period'][1]}")
            print(f"Test Period: {results['test_period'][0]} to {results['test_period'][1]}")
            if 'sharpe_ratio' in results:
                print(f"Sharpe Ratio: {results['sharpe_ratio']:.4f}")

        return results

    def hyperparameter_tuning(self,
                             param_grid: Dict = None,
                             cv_method: str = None,
                             scoring = None,
                             n_jobs: int = -1) -> Dict[str, Any]:
        """
        Perform hyperparameter tuning using grid search or random search

        Parameters:
        -----------
        param_grid : dict
            Parameter grid for tuning
        cv_method : str
            Cross-validation method
        scoring : str
            Scoring metric
        n_jobs : int
            Number of parallel jobs
        """
        if param_grid is None:
            param_grid = {
                'l1_coef': [0.0, 0.001, 0.01, 0.1],
                'l2_coef': [0.0, 0.001, 0.01, 0.1],
                'risk_aversion': [0.5, 1.0, 2.0, 5.0]
            }

        # Cross-validation strategy
        cv_method = cv_method or self.config.cv_method
        if cv_method == "walk_forward":
            cv = WalkForward(train_size=self.config.lookback_window,
                           test_size=self.config.rebalance_frequency)
        elif cv_method == "combinatorial_purged":
            cv = CombinatorialPurgedCV(n_folds=self.config.cv_folds)
        else:
            cv = KFold(n_splits=self.config.cv_folds, shuffle=False)

        # Grid search
        grid_search = GridSearchCV(
            estimator=self._build_model(),
            param_grid=param_grid,
            cv=cv,
            n_jobs=n_jobs,
            verbose=1
        )

        if self.factor_returns is not None:
            grid_search.fit(self.returns, self.factor_returns)
        else:
            grid_search.fit(self.returns)

        # Update model with best parameters
        self.model = grid_search.best_estimator_

        return {
            'best_params': grid_search.best_params_,
            'best_score': grid_search.best_score_,
            'cv_results': grid_search.cv_results_
        }

    def backtest_strategy(self,
                         rebalance_freq: int = None,
                         window_size: int = None,
                         start_date: str = None,
                         end_date: str = None) -> pd.DataFrame:
        """
        Backtest the portfolio strategy using walk-forward analysis

        Parameters:
        -----------
        rebalance_freq : int
            Rebalancing frequency in days
        window_size : int
            Rolling window size for optimization
        start_date, end_date : str
            Backtest date range

        Returns:
        --------
        DataFrame with backtest results
        """
        rebalance_freq = rebalance_freq or self.config.rebalance_frequency
        window_size = window_size or self.config.lookback_window

        # Filter data for backtest period
        returns_data = self.returns.copy()
        if start_date:
            returns_data = returns_data[returns_data.index >= start_date]
        if end_date:
            returns_data = returns_data[returns_data.index <= end_date]

        # Walk-forward backtest
        backtest_dates = []
        portfolio_returns = []
        weights_history = []

        for i in range(window_size, len(returns_data), rebalance_freq):
            # Training window
            train_data = returns_data.iloc[i-window_size:i]

            # Build and fit model
            model = self._build_model()

            try:
                if self.factor_returns is not None:
                    factor_data = self.factor_returns.iloc[i-window_size:i]
                    model.fit(train_data, factor_data)
                else:
                    model.fit(train_data)

                # Get weights
                weights = pd.Series(model.weights_, index=train_data.columns)
                weights_history.append(weights)

                # Calculate forward returns
                end_idx = min(i + rebalance_freq, len(returns_data))
                forward_returns = returns_data.iloc[i:end_idx]

                # Portfolio returns
                ptf_returns = (forward_returns * weights).sum(axis=1)
                portfolio_returns.extend(ptf_returns.values)
                backtest_dates.extend(forward_returns.index)

            except Exception as e:
                print(f"Error at period {i}: {e}")
                continue

        # Create backtest results DataFrame
        backtest_df = pd.DataFrame({
            'date': backtest_dates,
            'portfolio_return': portfolio_returns
        }).set_index('date')

        # Calculate performance metrics
        backtest_df['cumulative_return'] = (1 + backtest_df['portfolio_return']).cumprod()
        backtest_df['drawdown'] = (backtest_df['cumulative_return'] /
                                  backtest_df['cumulative_return'].expanding().max() - 1)

        self.backtest_results = {
            'returns': backtest_df,
            'weights_history': weights_history,
            'metrics': self._calculate_performance_metrics(backtest_df['portfolio_return'])
        }

        return backtest_df

    def stress_test(self,
                   scenarios: Dict[str, Dict] = None,
                   n_simulations: int = 10000) -> Dict[str, Any]:
        """
        Perform stress testing using various scenarios

        Parameters:
        -----------
        scenarios : dict
            Stress test scenarios
        n_simulations : int
            Number of Monte Carlo simulations
        """
        if self.model is None:
            raise ValueError("No model fitted. Run optimize_portfolio() first.")

        stress_results = {}

        # Default stress scenarios
        if scenarios is None:
            scenarios = {
                'market_crash': {'market_shock': -0.20},
                'high_volatility': {'volatility_mult': 2.0},
                'recession': {'gdp_shock': -0.05},
                'inflation_spike': {'inflation_shock': 0.10}
            }

        # Monte Carlo stress testing
        if hasattr(self.model, 'prior_estimator_'):
            prior = self.model.prior_estimator_

            # Generate synthetic returns using vine copula
            vine = VineCopula(log_transform=True, n_jobs=-1)
            vine.fit(self.returns)

            for scenario_name, scenario_params in scenarios.items():
                try:
                    # Generate stressed scenarios
                    conditioning = scenario_params if 'market_shock' in scenario_params else None
                    synthetic_returns = vine.sample(n_samples=n_simulations,
                                                  conditioning=conditioning)

                    # Calculate portfolio performance under stress
                    stressed_portfolio = self.model.predict(synthetic_returns)

                    stress_results[scenario_name] = {
                        'mean_return': synthetic_returns.mean().mean(),
                        'volatility': synthetic_returns.std().mean(),
                        'portfolio_var': np.percentile(stressed_portfolio.returns, 5),
                        'portfolio_cvar': stressed_portfolio.returns[
                            stressed_portfolio.returns <= np.percentile(stressed_portfolio.returns, 5)
                        ].mean()
                    }

                except Exception as e:
                    print(f"Error in scenario {scenario_name}: {e}")
                    continue

        return stress_results

    def _calculate_performance_metrics(self, returns: pd.Series) -> Dict[str, float]:
        """Calculate comprehensive performance metrics"""
        returns_annual = returns.mean() * 252
        volatility_annual = returns.std() * np.sqrt(252)

        # Drawdown calculation
        cumulative = (1 + returns).cumprod()
        rolling_max = cumulative.expanding().max()
        drawdown = (cumulative - rolling_max) / rolling_max

        metrics = {
            'annual_return': returns_annual,
            'annual_volatility': volatility_annual,
            'sharpe_ratio': returns_annual / volatility_annual if volatility_annual > 0 else 0,
            'max_drawdown': drawdown.min(),
            'calmar_ratio': returns_annual / abs(drawdown.min()) if drawdown.min() < 0 else 0,
            'sortino_ratio': returns_annual / (returns[returns < 0].std() * np.sqrt(252)) if len(returns[returns < 0]) > 0 else 0,
            'skewness': returns.skew(),
            'kurtosis': returns.kurt(),
            'var_95': np.percentile(returns, 5),
            'cvar_95': returns[returns <= np.percentile(returns, 5)].mean(),
            'win_rate': (returns > 0).sum() / len(returns)
        }

        return metrics

    def plot_weights(self, top_n: int = 15, figsize: Tuple[int, int] = (12, 8)) -> go.Figure:
        """Plot portfolio weights"""
        if self.model is None:
            raise ValueError("No model fitted. Run optimize_portfolio() first.")

        weights = pd.Series(self.model.weights_, index=self.returns.columns)
        weights = weights.sort_values(key=abs, ascending=False)[:top_n]

        # Create plotly bar chart
        fig = go.Figure(data=[
            go.Bar(
                x=weights.index,
                y=weights.values,
                marker_color=['red' if x < 0 else 'blue' for x in weights.values],
                text=[f'{x:.2%}' for x in weights.values],
                textposition='outside'
            )
        ])

        fig.update_layout(
            title=f'Portfolio Weights - {self.config.optimization_method.title()}',
            xaxis_title='Assets',
            yaxis_title='Weight',
            yaxis_tickformat='.1%',
            height=500
        )

        return fig

    def plot_efficient_frontier(self, n_portfolios: int = 100) -> go.Figure:
        """Plot efficient frontier"""
        if self.returns is None:
            raise ValueError("No data loaded. Call load_data() first.")

        # Generate efficient frontier
        returns_range = np.linspace(self.returns.mean().min() * 252,
                                   self.returns.mean().max() * 252,
                                   n_portfolios)

        risks = []
        returns_list = []

        for target_return in returns_range:
            try:
                model = MeanRisk(
                    objective_function=ObjectiveFunction.MINIMIZE_RISK,
                    risk_measure=RiskMeasure.VARIANCE,
                    min_return=target_return/252
                )
                model.fit(self.returns)

                portfolio_risk = np.sqrt(model.weights_ @ self.returns.cov() @ model.weights_) * np.sqrt(252)
                risks.append(portfolio_risk)
                returns_list.append(target_return)

            except:
                continue

        # Plot
        fig = go.Figure()

        fig.add_trace(go.Scatter(
            x=risks, y=returns_list,
            mode='lines+markers',
            name='Efficient Frontier',
            line=dict(color='blue', width=2)
        ))

        # Add current portfolio point if available
        if self.model is not None:
            current_return = self.portfolio.annualized_mean if hasattr(self.portfolio, 'annualized_mean') else 0
            current_risk = self.portfolio.annualized_volatility if hasattr(self.portfolio, 'annualized_volatility') else 0

            fig.add_trace(go.Scatter(
                x=[current_risk], y=[current_return],
                mode='markers',
                name='Current Portfolio',
                marker=dict(color='red', size=10, symbol='star')
            ))

        fig.update_layout(
            title='Efficient Frontier',
            xaxis_title='Risk (Volatility)',
            yaxis_title='Expected Return',
            xaxis_tickformat='.1%',
            yaxis_tickformat='.1%'
        )

        return fig

    def plot_backtest_results(self) -> go.Figure:
        """Plot backtest results"""
        if not self.backtest_results:
            raise ValueError("No backtest results. Run backtest_strategy() first.")

        df = self.backtest_results['returns']

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
            go.Scatter(x=df.index, y=df['drawdown'],
                      name='Drawdown', fill='tonexty',
                      line=dict(color='red')),
            row=2, col=1
        )

        fig.update_layout(height=600, title='Backtest Results')
        fig.update_yaxes(tickformat='.1%', row=1, col=1)
        fig.update_yaxes(tickformat='.1%', row=2, col=1)

        return fig

    def generate_report(self) -> Dict[str, Any]:
        """Generate comprehensive portfolio analytics report"""
        if self.model is None:
            raise ValueError("No model fitted. Run optimize_portfolio() first.")

        report = {
            'timestamp': datetime.now().isoformat(),
            'configuration': {
                'optimization_method': self.config.optimization_method,
                'objective_function': self.config.objective_function,
                'risk_measure': self.config.risk_measure,
                'covariance_estimator': self.config.covariance_estimator,
                'mu_estimator': self.config.mu_estimator
            },
            'portfolio_weights': dict(zip(self.returns.columns, self.model.weights_)),
            'top_10_positions': dict(
                pd.Series(self.model.weights_, index=self.returns.columns)
                .sort_values(key=abs, ascending=False)[:10]
            ),
            'performance_metrics': {},
            'risk_analysis': {},
            'optimization_history': self.optimization_history
        }

        # Performance metrics
        if hasattr(self.portfolio, 'sharpe_ratio'):
            report['performance_metrics'] = {
                'sharpe_ratio': self.portfolio.sharpe_ratio,
                'sortino_ratio': getattr(self.portfolio, 'sortino_ratio', None),
                'calmar_ratio': getattr(self.portfolio, 'calmar_ratio', None),
                'max_drawdown': getattr(self.portfolio, 'max_drawdown', None),
                'annual_volatility': getattr(self.portfolio, 'annualized_volatility', None),
                'annual_return': getattr(self.portfolio, 'annualized_mean', None)
            }

        # Risk analysis
        portfolio_returns = self.portfolio.returns if hasattr(self.portfolio, 'returns') else None
        if portfolio_returns is not None:
            report['risk_analysis'] = {
                'var_95': np.percentile(portfolio_returns, 5),
                'cvar_95': portfolio_returns[portfolio_returns <= np.percentile(portfolio_returns, 5)].mean(),
                'skewness': portfolio_returns.skew() if hasattr(portfolio_returns, 'skew') else None,
                'kurtosis': portfolio_returns.kurtosis() if hasattr(portfolio_returns, 'kurtosis') else None,
                'volatility': portfolio_returns.std() * np.sqrt(252)
            }

        # Backtest results
        if self.backtest_results:
            report['backtest_results'] = self.backtest_results['metrics']

        return report

    def save_report(self, filename: str = None) -> str:
        """Save portfolio analytics report to JSON file"""
        if filename is None:
            timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
            filename = f"portfolio_report_{timestamp}.json"

        report = self.generate_report()

        # Convert numpy types to native Python types for JSON serialization
        def convert_numpy(obj):
            if isinstance(obj, np.ndarray):
                return obj.tolist()
            elif isinstance(obj, np.integer):
                return int(obj)
            elif isinstance(obj, np.floating):
                return float(obj)
            elif isinstance(obj, pd.Series):
                return obj.to_dict()
            elif isinstance(obj, pd.DataFrame):
                return obj.to_dict('records')
            return obj

        # Recursively convert numpy types
        def recursive_convert(obj):
            if isinstance(obj, dict):
                return {k: recursive_convert(v) for k, v in obj.items()}
            elif isinstance(obj, list):
                return [recursive_convert(v) for v in obj]
            else:
                return convert_numpy(obj)

        report_serializable = recursive_convert(report)

        with open(filename, 'w') as f:
            json.dump(report_serializable, f, indent=2, default=str)

        print(f"Report saved to: {filename}")
        return filename

    def export_weights_to_csv(self, filename: str = None) -> str:
        """Export portfolio weights to CSV file"""
        if self.model is None:
            raise ValueError("No model fitted. Run optimize_portfolio() first.")

        if filename is None:
            timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
            filename = f"portfolio_weights_{timestamp}.csv"

        weights_df = pd.DataFrame({
            'Asset': self.returns.columns,
            'Weight': self.model.weights_,
            'Weight_Percent': self.model.weights_ * 100
        }).sort_values('Weight', key=abs, ascending=False)

        weights_df.to_csv(filename, index=False)
        print(f"Weights exported to: {filename}")
        return filename

    def compare_strategies(self,
                          strategies: List[Dict[str, Any]],
                          metric: str = 'sharpe_ratio') -> pd.DataFrame:
        """
        Compare multiple optimization strategies

        Parameters:
        -----------
        strategies : List[Dict]
            List of strategy configurations
        metric : str
            Comparison metric

        Returns:
        --------
        DataFrame with strategy comparison results
        """
        results = []

        for i, strategy_config in enumerate(strategies):
            try:
                # Create temporary config
                temp_config = PortfolioConfig(**strategy_config)
                temp_engine = PortfolioAnalyticsEngine(temp_config)
                temp_engine.load_data(self.prices, self.factors)

                # Optimize
                result = temp_engine.optimize_portfolio(verbose=False)
                result['strategy_id'] = f"Strategy_{i+1}"
                result['config'] = strategy_config
                results.append(result)

            except Exception as e:
                print(f"Error in strategy {i+1}: {e}")
                continue

        # Create comparison DataFrame
        comparison_df = pd.DataFrame(results)

        if metric in comparison_df.columns:
            comparison_df = comparison_df.sort_values(metric, ascending=False)

        return comparison_df

    def risk_attribution(self) -> Dict[str, pd.DataFrame]:
        """
        Perform risk attribution analysis

        Returns:
        --------
        Dictionary with risk attribution results
        """
        if self.model is None:
            raise ValueError("No model fitted. Run optimize_portfolio() first.")

        weights = pd.Series(self.model.weights_, index=self.returns.columns)
        cov_matrix = self.returns.cov() * 252  # Annualized

        # Portfolio variance
        portfolio_var = weights.T @ cov_matrix @ weights
        portfolio_vol = np.sqrt(portfolio_var)

        # Marginal contribution to risk
        marginal_contrib = (cov_matrix @ weights) / portfolio_vol

        # Component contribution to risk
        component_contrib = weights * marginal_contrib

        # Percentage contribution to risk
        pct_contrib = component_contrib / portfolio_vol * 100

        # Risk attribution DataFrame
        risk_attrib_df = pd.DataFrame({
            'Asset': self.returns.columns,
            'Weight': weights.values,
            'Weight_Pct': weights.values * 100,
            'Marginal_Risk': marginal_contrib.values,
            'Component_Risk': component_contrib.values,
            'Risk_Contribution_Pct': pct_contrib.values,
            'Individual_Vol': np.sqrt(np.diag(cov_matrix)) * 100
        }).sort_values('Risk_Contribution_Pct', key=abs, ascending=False)

        # Sector attribution if available
        sector_attrib = None
        if hasattr(self, 'sector_mapping') and self.sector_mapping:
            risk_attrib_df['Sector'] = risk_attrib_df['Asset'].map(self.sector_mapping)
            sector_attrib = risk_attrib_df.groupby('Sector').agg({
                'Weight_Pct': 'sum',
                'Risk_Contribution_Pct': 'sum',
                'Component_Risk': 'sum'
            }).sort_values('Risk_Contribution_Pct', ascending=False)

        return {
            'asset_attribution': risk_attrib_df,
            'sector_attribution': sector_attrib,
            'portfolio_volatility': portfolio_vol,
            'portfolio_variance': portfolio_var
        }

    def scenario_analysis(self, scenarios: Dict[str, pd.DataFrame]) -> Dict[str, Any]:
        """
        Perform scenario analysis with custom return scenarios

        Parameters:
        -----------
        scenarios : Dict[str, pd.DataFrame]
            Dictionary of scenario names and their return DataFrames

        Returns:
        --------
        Dictionary with scenario analysis results
        """
        if self.model is None:
            raise ValueError("No model fitted. Run optimize_portfolio() first.")

        scenario_results = {}

        for scenario_name, scenario_returns in scenarios.items():
            try:
                # Ensure scenario returns have same columns as training data
                common_assets = scenario_returns.columns.intersection(self.returns.columns)
                scenario_subset = scenario_returns[common_assets]

                # Get weights for common assets
                weights_subset = pd.Series(self.model.weights_, index=self.returns.columns)[common_assets]
                weights_subset = weights_subset / weights_subset.sum()  # Renormalize

                # Calculate portfolio returns under scenario
                portfolio_returns = (scenario_subset * weights_subset).sum(axis=1)

                # Calculate metrics
                scenario_results[scenario_name] = {
                    'total_return': (1 + portfolio_returns).prod() - 1,
                    'annualized_return': portfolio_returns.mean() * 252,
                    'volatility': portfolio_returns.std() * np.sqrt(252),
                    'sharpe_ratio': (portfolio_returns.mean() * 252) / (portfolio_returns.std() * np.sqrt(252)),
                    'max_drawdown': ((1 + portfolio_returns).cumprod() /
                                   (1 + portfolio_returns).cumprod().expanding().max() - 1).min(),
                    'var_95': np.percentile(portfolio_returns, 5),
                    'cvar_95': portfolio_returns[portfolio_returns <= np.percentile(portfolio_returns, 5)].mean(),
                    'worst_day': portfolio_returns.min(),
                    'best_day': portfolio_returns.max(),
                    'n_periods': len(portfolio_returns)
                }

            except Exception as e:
                print(f"Error in scenario {scenario_name}: {e}")
                scenario_results[scenario_name] = {'error': str(e)}

        return scenario_results

    def set_sector_mapping(self, sector_mapping: Dict[str, str]):
        """Set sector mapping for assets for sector-level analysis"""
        self.sector_mapping = sector_mapping


# Example Usage and Testing Functions
def create_sample_config() -> PortfolioConfig:
    """Create a sample configuration for testing"""
    return PortfolioConfig(
        optimization_method="mean_risk",
        objective_function="maximize_ratio",
        risk_measure="cvar",
        covariance_estimator="ledoit_wolf",
        mu_estimator="shrunk",
        max_weight_single_asset=0.15,
        l2_coef=0.01,
        cv_method="walk_forward",
        cv_folds=5,
        use_uncertainty_set=True
    )

def demo_portfolio_analytics():
    """Demonstration of the portfolio analytics engine"""

    # Load sample data
    print("Loading sample data...")
    prices = load_sp500_dataset()

    # Create configuration
    config = create_sample_config()

    # Initialize engine
    engine = PortfolioAnalyticsEngine(config)
    engine.load_data(prices)

    # Optimize portfolio
    print("\nOptimizing portfolio...")
    results = engine.optimize_portfolio()

    # Print top 10 weights
    weights = pd.Series(engine.model.weights_, index=engine.returns.columns)
    print("\nTop 10 Positions:")
    print(weights.sort_values(key=abs, ascending=False)[:10].to_string())

    # Hyperparameter tuning
    print("\nPerforming hyperparameter tuning...")
    tuning_results = engine.hyperparameter_tuning()
    print(f"Best parameters: {tuning_results['best_params']}")
    print(f"Best score: {tuning_results['best_score']:.4f}")

    # Backtest
    print("\nRunning backtest...")
    backtest_df = engine.backtest_strategy()
    print(f"Backtest performance metrics:")
    for metric, value in engine.backtest_results['metrics'].items():
        if isinstance(value, (int, float)):
            print(f"{metric}: {value:.4f}")

    # Risk attribution
    print("\nRisk attribution analysis...")
    risk_attrib = engine.risk_attribution()
    print("Top 5 risk contributors:")
    print(risk_attrib['asset_attribution'][['Asset', 'Weight_Pct', 'Risk_Contribution_Pct']].head().to_string(index=False))

    # Generate and save report
    print("\nGenerating comprehensive report...")
    report_file = engine.save_report()
    weights_file = engine.export_weights_to_csv()

    print(f"\nDemo completed successfully!")
    print(f"Files generated: {report_file}, {weights_file}")

    return engine

if __name__ == "__main__":
    # Run demonstration
    engine = demo_portfolio_analytics()