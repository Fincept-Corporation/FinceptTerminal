# -*- coding: utf-8 -*-
# portfolio_optimizer.py

"""
Complete Portfolio Optimization Module using PyPortfolioOpt
Implements all major features from the PyPortfolioOpt library including:
- Mean-Variance Optimization
- Risk Models and Expected Returns
- Black-Litterman Allocation
- Hierarchical Risk Parity
- Efficient Frontier calculations
- Post-processing and plotting
"""

import numpy as np
import pandas as pd
import matplotlib.pyplot as plt
import seaborn as sns
import warnings
from datetime import datetime, timedelta
from typing import Dict, List, Tuple, Optional, Union
import json
import threading
import time
from pathlib import Path

# PyPortfolioOpt imports
try:
    # Core optimization
    from pypfopt import EfficientFrontier, EfficientSemivariance, EfficientCVaR, EfficientCDaR
    from pypfopt import CLA, HRPOpt, BlackLittermanModel

    # Expected returns and risk models
    from pypfopt import expected_returns, risk_models
    from pypfopt.risk_models import CovarianceShrinkage

    # Objective functions and constraints
    from pypfopt import objective_functions

    # Post-processing
    from pypfopt.discrete_allocation import DiscreteAllocation, get_latest_prices

    # Plotting
    from pypfopt import plotting

    PYPFOPT_AVAILABLE = True
except ImportError as e:
    print(f"PyPortfolioOpt not available: {e}")
    print("Install with: pip install PyPortfolioOpt")
    PYPFOPT_AVAILABLE = False

# Import logging
from fincept_terminal.Utils.Logging.logger import logger, operation, monitor_performance

# Suppress warnings for cleaner output
warnings.filterwarnings('ignore')


class PortfolioOptimizer:
    """
    Comprehensive Portfolio Optimization Engine using PyPortfolioOpt

    Features:
    - Multiple optimization methods (Mean-Variance, CVaR, Semivariance, etc.)
    - Expected returns calculation methods
    - Risk models with shrinkage estimators
    - Black-Litterman model implementation
    - Hierarchical Risk Parity
    - Efficient frontier plotting
    - Discrete allocation and post-processing
    """

    def __init__(self, business_logic=None):
        """Initialize the portfolio optimizer"""
        if not PYPFOPT_AVAILABLE:
            raise ImportError("PyPortfolioOpt is required but not installed")

        self.business_logic = business_logic

        # Optimization parameters
        self.risk_free_rate = 0.02  # 2% risk-free rate
        self.confidence_level = 0.95  # For CVaR calculations
        self.lookback_days = 252  # 1 year of trading days

        # Cache for optimization results
        self.optimization_cache = {}
        self.cache_timeout = 3600  # 1 hour cache timeout

        # Supported optimization methods
        self.optimization_methods = {
            'mean_variance': 'Mean-Variance Optimization',
            'min_volatility': 'Minimum Volatility',
            'max_sharpe': 'Maximum Sharpe Ratio',
            'efficient_risk': 'Efficient Risk',
            'efficient_return': 'Efficient Return',
            'semivariance': 'Efficient Semivariance',
            'cvar': 'Conditional Value at Risk',
            'cdar': 'Conditional Drawdown at Risk',
            'hrp': 'Hierarchical Risk Parity',
            'black_litterman': 'Black-Litterman Model',
            'cla': 'Critical Line Algorithm'
        }

        # Expected returns methods
        self.expected_returns_methods = {
            'mean_historical_return': 'Mean Historical Return',
            'ema_historical_return': 'Exponentially Weighted Returns',
            'capm_return': 'CAPM Expected Returns',
            'james_stein': 'James-Stein Estimator'
        }

        # Risk model methods
        self.risk_model_methods = {
            'sample_cov': 'Sample Covariance',
            'semicovariance': 'Semicovariance',
            'exp_cov': 'Exponentially Weighted Covariance',
            'ledoit_wolf': 'Ledoit-Wolf Shrinkage',
            'oracle_approximating': 'Oracle Approximating Shrinkage'
        }

    def get_historical_data(self, symbols: List[str], lookback_days: int = None) -> pd.DataFrame:
        """
        Get historical price data for optimization

        Args:
            symbols: List of stock symbols
            lookback_days: Number of days to look back

        Returns:
            DataFrame with historical prices
        """
        try:
            if lookback_days is None:
                lookback_days = self.lookback_days

            # Get historical data from business logic or yfinance
            if self.business_logic:
                # Try to get from business logic first
                historical_data = {}

                for symbol in symbols:
                    try:
                        import yfinance as yf
                        ticker = yf.Ticker(symbol)

                        # Calculate start date
                        end_date = datetime.now()
                        start_date = end_date - timedelta(days=lookback_days + 50)  # Buffer for weekends/holidays

                        # Get historical data
                        hist = ticker.history(start=start_date, end=end_date)

                        if not hist.empty:
                            historical_data[symbol] = hist['Close']
                        else:
                            logger.warning(f"No historical data for {symbol}")

                    except Exception as e:
                        logger.error(f"Error fetching data for {symbol}: {e}")
                        continue

                if historical_data:
                    df = pd.DataFrame(historical_data)
                    # Remove rows with any NaN values
                    df = df.dropna()

                    # Ensure we have enough data
                    if len(df) < 30:  # Minimum 30 days
                        raise ValueError(f"Insufficient historical data. Only {len(df)} days available.")

                    return df.tail(lookback_days)  # Return exactly lookback_days
                else:
                    raise ValueError("No historical data could be retrieved")
            else:
                raise ValueError("No business logic provided for data retrieval")

        except Exception as e:
            logger.error(f"Error getting historical data: {e}")
            raise

    @monitor_performance
    def calculate_expected_returns(self, prices: pd.DataFrame, method: str = 'mean_historical_return',
                                   **kwargs) -> pd.Series:
        """
        Calculate expected returns using various methods

        Args:
            prices: Historical price data
            method: Method to calculate expected returns
            **kwargs: Additional parameters for specific methods

        Returns:
            Series of expected returns
        """
        try:
            with operation("calculate_expected_returns", context={'method': method}):
                if method == 'mean_historical_return':
                    frequency = kwargs.get('frequency', 252)
                    return expected_returns.mean_historical_return(prices, frequency=frequency)

                elif method == 'ema_historical_return':
                    frequency = kwargs.get('frequency', 252)
                    span = kwargs.get('span', 500)
                    return expected_returns.ema_historical_return(prices, frequency=frequency, span=span)

                elif method == 'capm_return':
                    market_prices = kwargs.get('market_prices')
                    frequency = kwargs.get('frequency', 252)
                    if market_prices is None:
                        # Use first stock as market proxy if no market data provided
                        market_prices = prices.iloc[:, 0]
                    return expected_returns.capm_return(prices, market_prices=market_prices, frequency=frequency)

                elif method == 'james_stein':
                    frequency = kwargs.get('frequency', 252)
                    mu = expected_returns.mean_historical_return(prices, frequency=frequency)
                    return expected_returns.james_stein_shrinkage(mu)

                else:
                    raise ValueError(f"Unknown expected returns method: {method}")

        except Exception as e:
            logger.error(f"Error calculating expected returns: {e}")
            raise

    @monitor_performance
    def calculate_risk_model(self, prices: pd.DataFrame, method: str = 'sample_cov',
                             **kwargs) -> pd.DataFrame:
        """
        Calculate risk model (covariance matrix) using various methods

        Args:
            prices: Historical price data
            method: Method to calculate risk model
            **kwargs: Additional parameters for specific methods

        Returns:
            Covariance matrix
        """
        try:
            with operation("calculate_risk_model", context={'method': method}):
                if method == 'sample_cov':
                    frequency = kwargs.get('frequency', 252)
                    return risk_models.sample_cov(prices, frequency=frequency)

                elif method == 'semicovariance':
                    frequency = kwargs.get('frequency', 252)
                    benchmark = kwargs.get('benchmark', 0)
                    return risk_models.semicovariance(prices, frequency=frequency, benchmark=benchmark)

                elif method == 'exp_cov':
                    frequency = kwargs.get('frequency', 252)
                    span = kwargs.get('span', 180)
                    return risk_models.exp_cov(prices, frequency=frequency, span=span)

                elif method == 'ledoit_wolf':
                    frequency = kwargs.get('frequency', 252)
                    cs = CovarianceShrinkage(prices, frequency=frequency)
                    return cs.ledoit_wolf()

                elif method == 'oracle_approximating':
                    frequency = kwargs.get('frequency', 252)
                    cs = CovarianceShrinkage(prices, frequency=frequency)
                    return cs.oracle_approximating()

                else:
                    raise ValueError(f"Unknown risk model method: {method}")

        except Exception as e:
            logger.error(f"Error calculating risk model: {e}")
            raise

    @monitor_performance
    def optimize_mean_variance(self, mu: pd.Series, S: pd.DataFrame,
                               optimization_target: str = 'max_sharpe',
                               target_return: float = None,
                               target_volatility: float = None,
                               weight_bounds: Tuple[float, float] = (0, 1),
                               sector_mapper: Dict = None,
                               sector_lower: Dict = None,
                               sector_upper: Dict = None,
                               **kwargs) -> Dict:
        """
        Perform mean-variance optimization

        Args:
            mu: Expected returns
            S: Covariance matrix
            optimization_target: Target for optimization
            target_return: Target return (for efficient_risk)
            target_volatility: Target volatility (for efficient_return)
            weight_bounds: Weight bounds for individual assets
            sector_mapper: Mapping of assets to sectors
            sector_lower: Lower bounds for sector weights
            sector_upper: Upper bounds for sector weights
            **kwargs: Additional parameters

        Returns:
            Dictionary with optimization results
        """
        try:
            with operation("optimize_mean_variance", context={'target': optimization_target}):
                # Create EfficientFrontier object
                ef = EfficientFrontier(mu, S, weight_bounds=weight_bounds)

                # Add sector constraints if provided
                if sector_mapper and (sector_lower or sector_upper):
                    ef.add_sector_constraints(sector_mapper, sector_lower, sector_upper)

                # Add L2 regularization if specified
                gamma = kwargs.get('gamma', 0)
                if gamma > 0:
                    ef.add_objective(objective_functions.L2_reg, gamma=gamma)

                # Perform optimization based on target
                if optimization_target == 'max_sharpe':
                    ef.max_sharpe(risk_free_rate=self.risk_free_rate)
                elif optimization_target == 'min_volatility':
                    ef.min_volatility()
                elif optimization_target == 'efficient_risk':
                    if target_return is None:
                        raise ValueError("target_return required for efficient_risk")
                    ef.efficient_risk(target_return)
                elif optimization_target == 'efficient_return':
                    if target_volatility is None:
                        raise ValueError("target_volatility required for efficient_return")
                    ef.efficient_return(target_volatility)
                else:
                    raise ValueError(f"Unknown optimization target: {optimization_target}")

                # Get raw weights
                raw_weights = ef.weights

                # Clean weights (remove tiny weights)
                cleaned_weights = ef.clean_weights()

                # Calculate performance
                performance = ef.portfolio_performance(
                    risk_free_rate=self.risk_free_rate,
                    verbose=False
                )

                return {
                    'raw_weights': dict(raw_weights),
                    'cleaned_weights': cleaned_weights,
                    'expected_return': performance[0],
                    'volatility': performance[1],
                    'sharpe_ratio': performance[2],
                    'optimization_target': optimization_target,
                    'ef_object': ef  # Store for further analysis
                }

        except Exception as e:
            logger.error(f"Error in mean-variance optimization: {e}")
            raise

    @monitor_performance
    def optimize_semivariance(self, prices: pd.DataFrame,
                              optimization_target: str = 'max_quadratic_utility',
                              benchmark: float = 0,
                              target_return: float = None,
                              market_neutral: bool = False,
                              **kwargs) -> Dict:
        """
        Perform semivariance optimization

        Args:
            prices: Historical price data
            optimization_target: Target for optimization
            benchmark: Benchmark return for semideviation
            target_return: Target return (for efficient_semivariance)
            market_neutral: Whether to make portfolio market neutral
            **kwargs: Additional parameters

        Returns:
            Dictionary with optimization results
        """
        try:
            with operation("optimize_semivariance", context={'target': optimization_target}):
                # Calculate returns
                returns = prices.pct_change().dropna()

                # Create EfficientSemivariance object
                es = EfficientSemivariance(returns, benchmark=benchmark)

                # Add market neutral constraint if specified
                if market_neutral:
                    es.add_constraint(lambda w: sum(w) == 0)

                # Perform optimization based on target
                if optimization_target == 'max_quadratic_utility':
                    risk_aversion = kwargs.get('risk_aversion', 1)
                    es.max_quadratic_utility(risk_aversion=risk_aversion)
                elif optimization_target == 'efficient_semivariance':
                    if target_return is None:
                        raise ValueError("target_return required for efficient_semivariance")
                    es.efficient_semivariance(target_return)
                elif optimization_target == 'min_semivariance':
                    es.min_semivariance()
                else:
                    raise ValueError(f"Unknown semivariance optimization target: {optimization_target}")

                # Get results
                raw_weights = es.weights
                cleaned_weights = es.clean_weights()

                # Calculate performance
                try:
                    performance = es.portfolio_performance(risk_free_rate=self.risk_free_rate)
                except:
                    # Fallback calculation if performance method fails
                    portfolio_return = sum(
                        raw_weights[i] * returns.mean().iloc[i] * 252 for i in range(len(raw_weights)))
                    performance = (portfolio_return, None, None)

                return {
                    'raw_weights': dict(raw_weights),
                    'cleaned_weights': cleaned_weights,
                    'expected_return': performance[0],
                    'semideviation': performance[1] if len(performance) > 1 else None,
                    'optimization_target': optimization_target,
                    'benchmark': benchmark
                }

        except Exception as e:
            logger.error(f"Error in semivariance optimization: {e}")
            raise

    @monitor_performance
    def optimize_cvar(self, prices: pd.DataFrame,
                      optimization_target: str = 'max_quadratic_utility',
                      beta: float = None,
                      target_return: float = None,
                      **kwargs) -> Dict:
        """
        Perform CVaR (Conditional Value at Risk) optimization

        Args:
            prices: Historical price data
            optimization_target: Target for optimization
            beta: Confidence level for CVaR (if None, uses self.confidence_level)
            target_return: Target return (for efficient_cvar)
            **kwargs: Additional parameters

        Returns:
            Dictionary with optimization results
        """
        try:
            with operation("optimize_cvar", context={'target': optimization_target}):
                if beta is None:
                    beta = self.confidence_level

                # Calculate returns
                returns = prices.pct_change().dropna()

                # Create EfficientCVaR object
                ec = EfficientCVaR(returns, beta=beta)

                # Perform optimization based on target
                if optimization_target == 'max_quadratic_utility':
                    risk_aversion = kwargs.get('risk_aversion', 1)
                    ec.max_quadratic_utility(risk_aversion=risk_aversion)
                elif optimization_target == 'efficient_cvar':
                    if target_return is None:
                        raise ValueError("target_return required for efficient_cvar")
                    ec.efficient_cvar(target_return)
                elif optimization_target == 'min_cvar':
                    ec.min_cvar()
                else:
                    raise ValueError(f"Unknown CVaR optimization target: {optimization_target}")

                # Get results
                raw_weights = ec.weights
                cleaned_weights = ec.clean_weights()

                # Calculate performance
                try:
                    performance = ec.portfolio_performance()
                except:
                    # Fallback calculation
                    portfolio_return = sum(
                        raw_weights[i] * returns.mean().iloc[i] * 252 for i in range(len(raw_weights)))
                    performance = (portfolio_return, None)

                return {
                    'raw_weights': dict(raw_weights),
                    'cleaned_weights': cleaned_weights,
                    'expected_return': performance[0],
                    'cvar': performance[1] if len(performance) > 1 else None,
                    'confidence_level': beta,
                    'optimization_target': optimization_target
                }

        except Exception as e:
            logger.error(f"Error in CVaR optimization: {e}")
            raise

    @monitor_performance
    def optimize_cdar(self, prices: pd.DataFrame,
                      optimization_target: str = 'max_quadratic_utility',
                      beta: float = None,
                      target_return: float = None,
                      **kwargs) -> Dict:
        """
        Perform CDaR (Conditional Drawdown at Risk) optimization

        Args:
            prices: Historical price data
            optimization_target: Target for optimization
            beta: Confidence level for CDaR (if None, uses self.confidence_level)
            target_return: Target return (for efficient_cdar)
            **kwargs: Additional parameters

        Returns:
            Dictionary with optimization results
        """
        try:
            with operation("optimize_cdar", context={'target': optimization_target}):
                if beta is None:
                    beta = self.confidence_level

                # Create EfficientCDaR object
                ec = EfficientCDaR(prices, beta=beta)

                # Perform optimization based on target
                if optimization_target == 'max_quadratic_utility':
                    risk_aversion = kwargs.get('risk_aversion', 1)
                    ec.max_quadratic_utility(risk_aversion=risk_aversion)
                elif optimization_target == 'efficient_cdar':
                    if target_return is None:
                        raise ValueError("target_return required for efficient_cdar")
                    ec.efficient_cdar(target_return)
                elif optimization_target == 'min_cdar':
                    ec.min_cdar()
                else:
                    raise ValueError(f"Unknown CDaR optimization target: {optimization_target}")

                # Get results
                raw_weights = ec.weights
                cleaned_weights = ec.clean_weights()

                # Calculate performance
                try:
                    performance = ec.portfolio_performance()
                except:
                    # Calculate returns for fallback
                    returns = prices.pct_change().dropna()
                    portfolio_return = sum(
                        raw_weights[i] * returns.mean().iloc[i] * 252 for i in range(len(raw_weights)))
                    performance = (portfolio_return, None)

                return {
                    'raw_weights': dict(raw_weights),
                    'cleaned_weights': cleaned_weights,
                    'expected_return': performance[0],
                    'cdar': performance[1] if len(performance) > 1 else None,
                    'confidence_level': beta,
                    'optimization_target': optimization_target
                }

        except Exception as e:
            logger.error(f"Error in CDaR optimization: {e}")
            raise

    @monitor_performance
    def optimize_hrp(self, prices: pd.DataFrame,
                     linkage_method: str = 'single',
                     max_cluster_size: int = None) -> Dict:
        """
        Perform Hierarchical Risk Parity optimization

        Args:
            prices: Historical price data
            linkage_method: Linkage method for clustering
            max_cluster_size: Maximum cluster size

        Returns:
            Dictionary with optimization results
        """
        try:
            with operation("optimize_hrp"):
                # Calculate returns
                returns = prices.pct_change().dropna()

                # Create HRPOpt object
                hrp = HRPOpt(returns)

                # Set parameters
                if max_cluster_size:
                    hrp.max_cluster_size = max_cluster_size

                # Optimize
                weights = hrp.optimize(linkage_method=linkage_method)

                # Clean weights
                cleaned_weights = hrp.clean_weights()

                # Calculate performance
                performance = hrp.portfolio_performance(risk_free_rate=self.risk_free_rate)

                return {
                    'raw_weights': dict(weights),
                    'cleaned_weights': cleaned_weights,
                    'expected_return': performance[0],
                    'volatility': performance[1],
                    'sharpe_ratio': performance[2],
                    'linkage_method': linkage_method,
                    'clustered_corr': hrp.clustered_corr,
                    'clusters': hrp.clusters
                }

        except Exception as e:
            logger.error(f"Error in HRP optimization: {e}")
            raise

    @monitor_performance
    def optimize_black_litterman(self, prices: pd.DataFrame,
                                 views: Dict[str, float] = None,
                                 view_confidences: List[float] = None,
                                 market_caps: Dict[str, float] = None,
                                 risk_aversion: float = 1,
                                 tau: float = 0.05,
                                 pi_method: str = 'market_cap',
                                 **kwargs) -> Dict:
        """
        Perform Black-Litterman optimization

        Args:
            prices: Historical price data
            views: Dictionary of views {asset: expected_return}
            view_confidences: List of confidence levels for views
            market_caps: Market capitalizations for assets
            risk_aversion: Risk aversion parameter
            tau: Tau parameter for Black-Litterman
            pi_method: Method to calculate prior returns
            **kwargs: Additional parameters

        Returns:
            Dictionary with optimization results
        """
        try:
            with operation("optimize_black_litterman"):
                # Calculate historical returns and covariance
                mu_hist = expected_returns.mean_historical_return(prices)
                S = risk_models.sample_cov(prices)

                # Set up market caps if not provided
                if market_caps is None:
                    # Use equal market caps as fallback
                    market_caps = {asset: 1.0 for asset in prices.columns}

                # Create Black-Litterman model
                bl = BlackLittermanModel(
                    S,
                    pi=pi_method,
                    market_caps=market_caps,
                    risk_aversion=risk_aversion,
                    tau=tau
                )

                # Add views if provided
                if views:
                    # Convert views to the format expected by PyPortfolioOpt
                    view_dict = {}
                    for asset, view_return in views.items():
                        if asset in prices.columns:
                            view_dict[asset] = view_return

                    if view_dict:
                        # Create P matrix (picking matrix)
                        P = pd.DataFrame(0, index=range(len(view_dict)), columns=S.index)
                        Q = []  # Views vector

                        for i, (asset, view_return) in enumerate(view_dict.items()):
                            P.iloc[i][asset] = 1
                            Q.append(view_return)

                        # Set confidence matrix
                        if view_confidences is None:
                            # Default confidence
                            omega = np.diag([1.0] * len(view_dict))
                        else:
                            omega = np.diag(view_confidences[:len(view_dict)])

                        # Add views to model
                        bl.bl_views(P, Q, omega)

                # Get Black-Litterman returns and covariance
                mu_bl = bl.bl_returns()
                S_bl = bl.bl_cov()

                # Optimize portfolio
                ef = EfficientFrontier(mu_bl, S_bl)
                ef.max_sharpe(risk_free_rate=self.risk_free_rate)

                # Get results
                raw_weights = ef.weights
                cleaned_weights = ef.clean_weights()
                performance = ef.portfolio_performance(risk_free_rate=self.risk_free_rate)

                return {
                    'raw_weights': dict(raw_weights),
                    'cleaned_weights': cleaned_weights,
                    'expected_return': performance[0],
                    'volatility': performance[1],
                    'sharpe_ratio': performance[2],
                    'bl_returns': mu_bl.to_dict(),
                    'prior_returns': bl.pi.to_dict() if hasattr(bl, 'pi') else {},
                    'views': views or {},
                    'tau': tau,
                    'risk_aversion': risk_aversion
                }

        except Exception as e:
            logger.error(f"Error in Black-Litterman optimization: {e}")
            raise

    @monitor_performance
    def optimize_cla(self, mu: pd.Series, S: pd.DataFrame) -> Dict:
        """
        Perform Critical Line Algorithm optimization

        Args:
            mu: Expected returns
            S: Covariance matrix

        Returns:
            Dictionary with optimization results
        """
        try:
            with operation("optimize_cla"):
                # Create CLA object
                cla = CLA(mu, S)

                # Optimize
                cla.max_sharpe()

                # Get results
                raw_weights = cla.weights
                cleaned_weights = cla.clean_weights()
                performance = cla.portfolio_performance(risk_free_rate=self.risk_free_rate)

                # Get efficient frontier
                ef_returns, ef_volatilities, ef_weights = cla.efficient_frontier()

                return {
                    'raw_weights': dict(raw_weights),
                    'cleaned_weights': cleaned_weights,
                    'expected_return': performance[0],
                    'volatility': performance[1],
                    'sharpe_ratio': performance[2],
                    'efficient_frontier': {
                        'returns': ef_returns,
                        'volatilities': ef_volatilities,
                        'weights': ef_weights
                    }
                }

        except Exception as e:
            logger.error(f"Error in CLA optimization: {e}")
            raise

    @monitor_performance
    def calculate_efficient_frontier(self, mu: pd.Series, S: pd.DataFrame,
                                     num_points: int = 100,
                                     risk_range: Tuple[float, float] = None) -> Dict:
        """
        Calculate the efficient frontier

        Args:
            mu: Expected returns
            S: Covariance matrix
            num_points: Number of points on the frontier
            risk_range: Risk range (min_vol, max_vol)

        Returns:
            Dictionary with frontier data
        """
        try:
            with operation("calculate_efficient_frontier"):
                ef = EfficientFrontier(mu, S)

                # Calculate risk range if not provided
                if risk_range is None:
                    # Min volatility portfolio
                    ef_temp = EfficientFrontier(mu, S)
                    ef_temp.min_volatility()
                    min_vol = ef_temp.portfolio_performance()[1]

                    # Max return portfolio (approximate)
                    max_return = mu.max()
                    ef_temp = EfficientFrontier(mu, S)
                    try:
                        ef_temp.efficient_return(max_return * 0.95)  # 95% of max return
                        max_vol = ef_temp.portfolio_performance()[1]
                    except:
                        max_vol = min_vol * 3  # Fallback

                    risk_range = (min_vol, max_vol)

                # Generate frontier points using both risk-targeting and return-targeting approaches
                frontier_returns = []
                frontier_volatilities = []
                frontier_weights = []

                # Method 1: Target different volatility levels
                target_vols = np.linspace(risk_range[0], risk_range[1], num_points // 2)

                for target_vol in target_vols:
                    try:
                        ef_temp = EfficientFrontier(mu, S)
                        # Use efficient_risk to target specific volatility
                        ef_temp.efficient_risk(target_vol ** 2)  # efficient_risk takes variance, not volatility
                        ret, vol, _ = ef_temp.portfolio_performance()

                        if min_vol <= vol <= risk_range[1] * 1.1:  # Validate volatility range
                            frontier_returns.append(ret)
                            frontier_volatilities.append(vol)
                            frontier_weights.append(dict(ef_temp.weights))
                    except Exception as e:
                        logger.debug(f"Failed to optimize for target volatility {target_vol:.4f}: {e}")
                        continue

                # Method 2: Target different return levels
                if frontier_returns:
                    min_return = min(frontier_returns)
                    max_return = max(frontier_returns)
                else:
                    min_return = mu.min()
                    max_return = mu.max() * 0.9

                target_returns = np.linspace(min_return, max_return, num_points // 2)

                for target_return in target_returns:
                    try:
                        ef_temp = EfficientFrontier(mu, S)
                        ef_temp.efficient_return(target_return)
                        ret, vol, _ = ef_temp.portfolio_performance()

                        # Check if this point is already covered and within reasonable bounds
                        if (vol >= min_vol and vol <= risk_range[1] * 1.2 and
                                not any(abs(existing_vol - vol) < 0.001 for existing_vol in frontier_volatilities)):
                            frontier_returns.append(ret)
                            frontier_volatilities.append(vol)
                            frontier_weights.append(dict(ef_temp.weights))
                    except Exception as e:
                        logger.debug(f"Failed to optimize for target return {target_return:.4f}: {e}")
                        continue

                # Remove duplicates and sort by volatility
                if frontier_returns:
                    # Combine data and remove near-duplicates
                    combined_data = list(zip(frontier_volatilities, frontier_returns, frontier_weights))

                    # Sort by volatility
                    combined_data.sort(key=lambda x: x[0])

                    # Remove near-duplicate points (within 0.1% volatility difference)
                    filtered_data = []
                    last_vol = -1

                    for vol, ret, weights in combined_data:
                        if abs(vol - last_vol) > 0.001:  # Keep points that are sufficiently different
                            filtered_data.append((vol, ret, weights))
                            last_vol = vol

                    # Unpack filtered data
                    if filtered_data:
                        frontier_volatilities, frontier_returns, frontier_weights = zip(*filtered_data)
                        frontier_volatilities = list(frontier_volatilities)
                        frontier_returns = list(frontier_returns)
                        frontier_weights = list(frontier_weights)
                    else:
                        # Fallback if no valid points found
                        frontier_returns = []
                        frontier_volatilities = []
                        frontier_weights = []

                # Ensure we have at least min volatility and max Sharpe portfolios
                if len(frontier_returns) < 2:
                    logger.warning("Insufficient frontier points generated, adding key portfolios")

                    # Add minimum volatility portfolio
                    try:
                        ef_min = EfficientFrontier(mu, S)
                        ef_min.min_volatility()
                        min_ret, min_vol, _ = ef_min.portfolio_performance()

                        frontier_returns.append(min_ret)
                        frontier_volatilities.append(min_vol)
                        frontier_weights.append(dict(ef_min.weights))
                    except Exception as e:
                        logger.warning(f"Could not add min volatility portfolio: {e}")

                    # Add maximum Sharpe portfolio
                    try:
                        ef_sharpe = EfficientFrontier(mu, S)
                        ef_sharpe.max_sharpe(risk_free_rate=self.risk_free_rate)
                        sharpe_ret, sharpe_vol, sharpe_ratio = ef_sharpe.portfolio_performance(
                            risk_free_rate=self.risk_free_rate)

                        # Only add if significantly different from existing points
                        if not any(abs(sharpe_vol - vol) < 0.001 for vol in frontier_volatilities):
                            frontier_returns.append(sharpe_ret)
                            frontier_volatilities.append(sharpe_vol)
                            frontier_weights.append(dict(ef_sharpe.weights))
                    except Exception as e:
                        logger.warning(f"Could not add max Sharpe portfolio: {e}")

                # Calculate max Sharpe portfolio for final results
                try:
                    ef_sharpe = EfficientFrontier(mu, S)
                    ef_sharpe.max_sharpe(risk_free_rate=self.risk_free_rate)
                    sharpe_performance = ef_sharpe.portfolio_performance(risk_free_rate=self.risk_free_rate)

                    max_sharpe_data = {
                        'return': sharpe_performance[0],
                        'volatility': sharpe_performance[1],
                        'sharpe_ratio': sharpe_performance[2],
                        'weights': dict(ef_sharpe.weights)
                    }
                except Exception as e:
                    logger.warning(f"Could not calculate max Sharpe portfolio: {e}")
                    max_sharpe_data = {
                        'return': 0,
                        'volatility': 0,
                        'sharpe_ratio': 0,
                        'weights': {}
                    }

                # Calculate additional portfolio statistics
                efficient_frontier_stats = {}
                if frontier_returns and frontier_volatilities:
                    efficient_frontier_stats = {
                        'num_points': len(frontier_returns),
                        'min_return': min(frontier_returns),
                        'max_return': max(frontier_returns),
                        'min_volatility': min(frontier_volatilities),
                        'max_volatility': max(frontier_volatilities),
                        'return_range': max(frontier_returns) - min(frontier_returns),
                        'volatility_range': max(frontier_volatilities) - min(frontier_volatilities)
                    }

                return {
                    'returns': frontier_returns,
                    'volatilities': frontier_volatilities,
                    'weights': frontier_weights,
                    'max_sharpe': max_sharpe_data,
                    'risk_range': risk_range,
                    'statistics': efficient_frontier_stats,
                    'success': len(frontier_returns) > 0
                }

        except Exception as e:
            logger.error(f"Error calculating efficient frontier: {e}")
            # Return empty results on error
            return {
                'returns': [],
                'volatilities': [],
                'weights': [],
                'max_sharpe': {
                    'return': 0,
                    'volatility': 0,
                    'sharpe_ratio': 0,
                    'weights': {}
                },
                'risk_range': (0, 0),
                'statistics': {},
                'success': False,
                'error': str(e)
            }

            # Calculate max Sharpe portfolio
            ef_sharpe = EfficientFrontier(mu, S)
            ef_sharpe.max_sharpe(risk_free_rate=self.risk_free_rate)
            sharpe_performance = ef_sharpe.portfolio_performance(risk_free_rate=self.risk_free_rate)

            return {
                'returns': frontier_returns,
                'volatilities': frontier_volatilities,
                'weights': frontier_weights,
                'max_sharpe': {
                    'return': sharpe_performance[0],
                    'volatility': sharpe_performance[1],
                    'sharpe_ratio': sharpe_performance[2],
                    'weights': dict(ef_sharpe.weights)
                },
                'risk_range': risk_range
            }


@monitor_performance
def discrete_allocation(self, weights: Dict[str, float],
                        latest_prices: Dict[str, float],
                        total_portfolio_value: float,
                        short_ratio: float = None) -> Dict:
    """
    Convert continuous weights to discrete share allocation

    Args:
        weights: Portfolio weights
        latest_prices: Latest prices for assets
        total_portfolio_value: Total value to allocate
        short_ratio: Ratio for shorting (if allowed)

    Returns:
        Dictionary with discrete allocation
    """
    try:
        with operation("discrete_allocation"):
            # Create DiscreteAllocation object
            da = DiscreteAllocation(
                weights,
                latest_prices,
                total_portfolio_value=total_portfolio_value,
                short_ratio=short_ratio
            )

            # Get allocation using greedy algorithm
            allocation, leftover = da.greedy_portfolio()

            # Calculate allocation summary
            allocated_value = sum(shares * latest_prices[asset] for asset, shares in allocation.items())

            return {
                'allocation': allocation,
                'leftover_cash': leftover,
                'allocated_value': allocated_value,
                'total_value': total_portfolio_value,
                'allocation_percentage': (allocated_value / total_portfolio_value) * 100
            }

    except Exception as e:
        logger.error(f"Error in discrete allocation: {e}")
        raise


@monitor_performance
def backtest_strategy(self, symbols: List[str],
                      optimization_method: str,
                      rebalance_frequency: str = 'monthly',
                      lookback_days: int = 252,
                      **optimization_params) -> Dict:
    """
    Backtest a portfolio optimization strategy

    Args:
        symbols: List of symbols to include
        optimization_method: Method to use for optimization
        rebalance_frequency: How often to rebalance
        lookback_days: Lookback period for optimization
        **optimization_params: Parameters for optimization method

    Returns:
        Dictionary with backtesting results
    """
    try:
        with operation("backtest_strategy"):
            # Get extended historical data for backtesting
            extended_lookback = lookback_days * 3  # 3x lookback for backtesting
            prices = self.get_historical_data(symbols, extended_lookback)

            if len(prices) < lookback_days * 2:
                raise ValueError("Insufficient data for backtesting")

            # Calculate rebalancing dates
            rebalance_dates = self._get_rebalance_dates(prices.index, rebalance_frequency)

            # Initialize tracking variables
            portfolio_values = []
            weights_history = []
            returns_history = []

            # Initial portfolio value
            initial_value = 100000  # $100k starting value
            current_value = initial_value

            for i, rebalance_date in enumerate(rebalance_dates[:-1]):
                try:
                    # Get data for optimization (lookback from rebalance date)
                    opt_end_date = rebalance_date
                    opt_start_idx = max(0, prices.index.get_loc(opt_end_date) - lookback_days)
                    opt_data = prices.iloc[opt_start_idx:prices.index.get_loc(opt_end_date)]

                    if len(opt_data) < 30:  # Minimum data requirement
                        continue

                    # Perform optimization
                    if optimization_method == 'mean_variance':
                        mu = self.calculate_expected_returns(opt_data)
                        S = self.calculate_risk_model(opt_data)
                        result = self.optimize_mean_variance(mu, S, **optimization_params)
                        weights = result['cleaned_weights']
                    elif optimization_method == 'hrp':
                        result = self.optimize_hrp(opt_data, **optimization_params)
                        weights = result['cleaned_weights']
                    elif optimization_method == 'black_litterman':
                        result = self.optimize_black_litterman(opt_data, **optimization_params)
                        weights = result['cleaned_weights']
                    else:
                        raise ValueError(f"Unsupported optimization method for backtesting: {optimization_method}")

                    weights_history.append({
                        'date': rebalance_date,
                        'weights': weights
                    })

                    # Calculate returns until next rebalance
                    next_rebalance = rebalance_dates[i + 1]
                    period_start_idx = prices.index.get_loc(rebalance_date)
                    period_end_idx = prices.index.get_loc(next_rebalance)

                    period_prices = prices.iloc[period_start_idx:period_end_idx + 1]
                    period_returns = period_prices.pct_change().dropna()

                    # Calculate portfolio returns
                    for date, returns in period_returns.iterrows():
                        portfolio_return = sum(weights.get(asset, 0) * returns[asset]
                                               for asset in returns.index if asset in weights)
                        current_value *= (1 + portfolio_return)

                        portfolio_values.append({
                            'date': date,
                            'value': current_value,
                            'return': portfolio_return
                        })

                        returns_history.append(portfolio_return)

                except Exception as e:
                    logger.warning(f"Error in backtest period {rebalance_date}: {e}")
                    continue

            # Calculate performance metrics
            if returns_history:
                returns_array = np.array(returns_history)

                total_return = (current_value - initial_value) / initial_value
                annualized_return = (current_value / initial_value) ** (252 / len(returns_history)) - 1
                volatility = np.std(returns_array) * np.sqrt(252)
                sharpe_ratio = (annualized_return - self.risk_free_rate) / volatility if volatility > 0 else 0

                # Calculate max drawdown
                values = [pv['value'] for pv in portfolio_values]
                peak = np.maximum.accumulate(values)
                drawdown = (values - peak) / peak
                max_drawdown = np.min(drawdown)

                return {
                    'portfolio_values': portfolio_values,
                    'weights_history': weights_history,
                    'performance': {
                        'total_return': total_return,
                        'annualized_return': annualized_return,
                        'volatility': volatility,
                        'sharpe_ratio': sharpe_ratio,
                        'max_drawdown': max_drawdown,
                        'final_value': current_value,
                        'initial_value': initial_value
                    },
                    'optimization_method': optimization_method,
                    'rebalance_frequency': rebalance_frequency
                }
            else:
                raise ValueError("No returns calculated during backtesting")

    except Exception as e:
        logger.error(f"Error in strategy backtesting: {e}")
        raise


def _get_rebalance_dates(self, date_index: pd.DatetimeIndex, frequency: str) -> List:
    """Get rebalancing dates based on frequency"""
    if frequency == 'daily':
        return date_index.tolist()
    elif frequency == 'weekly':
        return date_index[date_index.weekday == 0].tolist()  # Mondays
    elif frequency == 'monthly':
        return date_index.groupby([date_index.year, date_index.month]).first().tolist()
    elif frequency == 'quarterly':
        return date_index.groupby([date_index.year, date_index.quarter]).first().tolist()
    else:
        raise ValueError(f"Unsupported rebalance frequency: {frequency}")


@monitor_performance
def plot_efficient_frontier(self, mu: pd.Series, S: pd.DataFrame,
                            num_points: int = 100,
                            save_path: str = None,
                            show_assets: bool = True,
                            show_cml: bool = True) -> str:
    """
    Plot the efficient frontier

    Args:
        mu: Expected returns
        S: Covariance matrix
        num_points: Number of points on frontier
        save_path: Path to save plot
        show_assets: Whether to show individual assets
        show_cml: Whether to show Capital Market Line

    Returns:
        Path to saved plot or base64 string
    """
    try:
        with operation("plot_efficient_frontier"):
            # Calculate efficient frontier
            frontier_data = self.calculate_efficient_frontier(mu, S, num_points)

            # Create plot
            plt.figure(figsize=(12, 8))

            # Plot efficient frontier
            plt.scatter(frontier_data['volatilities'], frontier_data['returns'],
                        c=frontier_data['returns'], cmap='viridis', alpha=0.6, s=50)
            plt.colorbar(label='Expected Return')

            # Plot individual assets if requested
            if show_assets:
                asset_volatility = np.sqrt(np.diag(S))
                plt.scatter(asset_volatility, mu, alpha=0.8, s=100, c='red', marker='o')

                # Add asset labels
                for i, asset in enumerate(mu.index):
                    plt.annotate(asset, (asset_volatility[i], mu[i]),
                                 xytext=(5, 5), textcoords='offset points')

            # Plot max Sharpe portfolio
            max_sharpe = frontier_data['max_sharpe']
            plt.scatter(max_sharpe['volatility'], max_sharpe['return'],
                        marker='*', s=500, c='gold', edgecolors='black',
                        label=f'Max Sharpe (SR={max_sharpe["sharpe_ratio"]:.3f})')

            # Plot Capital Market Line if requested
            if show_cml:
                # CML: y = rf + (portfolio_return - rf) / portfolio_vol * x
                max_vol = max(frontier_data['volatilities'])
                cml_x = np.linspace(0, max_vol, 100)
                cml_slope = (max_sharpe['return'] - self.risk_free_rate) / max_sharpe['volatility']
                cml_y = self.risk_free_rate + cml_slope * cml_x
                plt.plot(cml_x, cml_y, 'r--', alpha=0.7, label='Capital Market Line')

            plt.xlabel('Volatility (Risk)')
            plt.ylabel('Expected Return')
            plt.title('Efficient Frontier')
            plt.legend()
            plt.grid(True, alpha=0.3)

            # Save or return plot
            if save_path:
                plt.savefig(save_path, dpi=300, bbox_inches='tight')
                plt.close()
                return save_path
            else:
                # Return as base64 for embedding in UI
                import io
                import base64

                buffer = io.BytesIO()
                plt.savefig(buffer, format='png', dpi=300, bbox_inches='tight')
                buffer.seek(0)
                plot_data = base64.b64encode(buffer.getvalue()).decode()
                plt.close()
                return plot_data

    except Exception as e:
        logger.error(f"Error plotting efficient frontier: {e}")
        raise


@monitor_performance
def plot_correlation_matrix(self, prices: pd.DataFrame,
                            method: str = 'pearson',
                            save_path: str = None) -> str:
    """
    Plot correlation matrix of assets

    Args:
        prices: Price data
        method: Correlation method ('pearson', 'spearman', 'kendall')
        save_path: Path to save plot

    Returns:
        Path to saved plot or base64 string
    """
    try:
        with operation("plot_correlation_matrix"):
            # Calculate returns and correlation
            returns = prices.pct_change().dropna()
            correlation_matrix = returns.corr(method=method)

            # Create plot
            plt.figure(figsize=(12, 10))

            # Create heatmap
            mask = np.triu(np.ones_like(correlation_matrix, dtype=bool))
            sns.heatmap(correlation_matrix, mask=mask, annot=True, cmap='coolwarm',
                        center=0, square=True, linewidths=0.5, cbar_kws={"shrink": .8})

            plt.title(f'Asset Correlation Matrix ({method.title()})')
            plt.tight_layout()

            # Save or return plot
            if save_path:
                plt.savefig(save_path, dpi=300, bbox_inches='tight')
                plt.close()
                return save_path
            else:
                import io
                import base64

                buffer = io.BytesIO()
                plt.savefig(buffer, format='png', dpi=300, bbox_inches='tight')
                buffer.seek(0)
                plot_data = base64.b64encode(buffer.getvalue()).decode()
                plt.close()
                return plot_data

    except Exception as e:
        logger.error(f"Error plotting correlation matrix: {e}")
        raise


@monitor_performance
def plot_weights_comparison(self, weights_dict: Dict[str, Dict[str, float]],
                            save_path: str = None) -> str:
    """
    Plot comparison of different portfolio weights

    Args:
        weights_dict: Dictionary of {method_name: weights}
        save_path: Path to save plot

    Returns:
        Path to saved plot or base64 string
    """
    try:
        with operation("plot_weights_comparison"):
            # Prepare data
            df_weights = pd.DataFrame(weights_dict).fillna(0)

            # Create plot
            fig, (ax1, ax2) = plt.subplots(2, 1, figsize=(14, 10))

            # Stacked bar chart
            df_weights.T.plot(kind='bar', stacked=True, ax=ax1,
                              colormap='tab20', alpha=0.8)
            ax1.set_title('Portfolio Weights Comparison (Stacked)')
            ax1.set_ylabel('Weight')
            ax1.legend(bbox_to_anchor=(1.05, 1), loc='upper left')
            ax1.grid(True, alpha=0.3)

            # Side-by-side comparison
            df_weights.plot(kind='bar', ax=ax2, alpha=0.8, colormap='tab10')
            ax2.set_title('Portfolio Weights Comparison (Side-by-side)')
            ax2.set_ylabel('Weight')
            ax2.set_xlabel('Assets')
            ax2.legend(bbox_to_anchor=(1.05, 1), loc='upper left')
            ax2.grid(True, alpha=0.3)

            plt.tight_layout()

            # Save or return plot
            if save_path:
                plt.savefig(save_path, dpi=300, bbox_inches='tight')
                plt.close()
                return save_path
            else:
                import io
                import base64

                buffer = io.BytesIO()
                plt.savefig(buffer, format='png', dpi=300, bbox_inches='tight')
                buffer.seek(0)
                plot_data = base64.b64encode(buffer.getvalue()).decode()
                plt.close()
                return plot_data

    except Exception as e:
        logger.error(f"Error plotting weights comparison: {e}")
        raise


@monitor_performance
def generate_optimization_report(self, symbols: List[str],
                                 optimization_methods: List[str] = None,
                                 save_path: str = None) -> Dict:
    """
    Generate comprehensive optimization report

    Args:
        symbols: List of symbols to optimize
        optimization_methods: Methods to compare
        save_path: Path to save report

    Returns:
        Dictionary with complete analysis
    """
    try:
        with operation("generate_optimization_report"):
            if optimization_methods is None:
                optimization_methods = ['mean_variance', 'hrp', 'min_volatility']

            # Get historical data
            prices = self.get_historical_data(symbols, self.lookback_days)

            # Calculate expected returns and risk model
            mu = self.calculate_expected_returns(prices)
            S = self.calculate_risk_model(prices)

            # Run optimizations
            results = {}
            weights_comparison = {}

            for method in optimization_methods:
                try:
                    if method == 'mean_variance':
                        result = self.optimize_mean_variance(mu, S, 'max_sharpe')
                    elif method == 'min_volatility':
                        result = self.optimize_mean_variance(mu, S, 'min_volatility')
                    elif method == 'hrp':
                        result = self.optimize_hrp(prices)
                    elif method == 'black_litterman':
                        result = self.optimize_black_litterman(prices)
                    else:
                        continue

                    results[method] = result
                    weights_comparison[method] = result['cleaned_weights']

                except Exception as e:
                    logger.warning(f"Failed to optimize using {method}: {e}")
                    continue

            # Generate plots
            plots = {}

            # Efficient frontier
            try:
                plots['efficient_frontier'] = self.plot_efficient_frontier(mu, S)
            except Exception as e:
                logger.warning(f"Failed to plot efficient frontier: {e}")

            # Correlation matrix
            try:
                plots['correlation_matrix'] = self.plot_correlation_matrix(prices)
            except Exception as e:
                logger.warning(f"Failed to plot correlation matrix: {e}")

            # Weights comparison
            if len(weights_comparison) > 1:
                try:
                    plots['weights_comparison'] = self.plot_weights_comparison(weights_comparison)
                except Exception as e:
                    logger.warning(f"Failed to plot weights comparison: {e}")

            # Compile report
            report = {
                'timestamp': datetime.now().isoformat(),
                'symbols': symbols,
                'optimization_methods': optimization_methods,
                'data_period': {
                    'start_date': prices.index[0].isoformat(),
                    'end_date': prices.index[-1].isoformat(),
                    'days': len(prices)
                },
                'results': results,
                'weights_comparison': weights_comparison,
                'plots': plots,
                'expected_returns': mu.to_dict(),
                'risk_metrics': {
                    'volatilities': np.sqrt(np.diag(S)).tolist(),
                    'correlations': prices.pct_change().corr().to_dict()
                }
            }

            # Save report if path provided
            if save_path:
                with open(save_path, 'w') as f:
                    # Remove plot data for JSON serialization
                    report_copy = report.copy()
                    report_copy['plots'] = {k: f"<plot_data_{k}>" for k in plots.keys()}
                    json.dump(report_copy, f, indent=2, default=str)

            return report

    except Exception as e:
        logger.error(f"Error generating optimization report: {e}")
        raise


def cleanup(self):
    """Clean up optimizer resources"""
    try:
        logger.info("🧹 Cleaning up portfolio optimizer...")

        # Clear caches
        self.optimization_cache.clear()

        # Close any matplotlib figures
        plt.close('all')

        logger.info("Portfolio optimizer cleanup complete")

    except Exception as e:
        logger.error(f"Error in optimizer cleanup: {e}")


# Utility functions for integration with main portfolio system

def get_optimization_summary(optimization_result: Dict) -> str:
    """
    Generate a human-readable summary of optimization results

    Args:
        optimization_result: Result from any optimization method

    Returns:
        Formatted summary string
    """
    try:
        summary_lines = []

        # Basic performance metrics
        if 'expected_return' in optimization_result:
            summary_lines.append(f"Expected Return: {optimization_result['expected_return']:.2%}")

        if 'volatility' in optimization_result:
            summary_lines.append(f"Volatility: {optimization_result['volatility']:.2%}")

        if 'sharpe_ratio' in optimization_result:
            summary_lines.append(f"Sharpe Ratio: {optimization_result['sharpe_ratio']:.3f}")

        # Top holdings
        weights = optimization_result.get('cleaned_weights', {})
        if weights:
            top_holdings = sorted(weights.items(), key=lambda x: x[1], reverse=True)[:5]
            summary_lines.append("\nTop Holdings:")
            for asset, weight in top_holdings:
                if weight > 0.001:  # Only show weights > 0.1%
                    summary_lines.append(f"  {asset}: {weight:.1%}")

        # Method-specific information
        method = optimization_result.get('optimization_target') or optimization_result.get('optimization_method',
                                                                                           'Unknown')
        summary_lines.insert(0, f"Optimization Method: {method}")

        return "\n".join(summary_lines)

    except Exception as e:
        return f"Error generating summary: {e}"


def validate_optimization_inputs(symbols: List[str], prices: pd.DataFrame = None) -> bool:
    """
    Validate inputs for optimization

    Args:
        symbols: List of symbols
        prices: Price data (optional)

    Returns:
        True if valid, raises ValueError if not
    """
    if not symbols or len(symbols) < 2:
        raise ValueError("At least 2 symbols required for optimization")

    if len(symbols) > 50:
        logger.warning("Large number of symbols may slow optimization")

    if prices is not None:
        if len(prices) < 30:
            raise ValueError("Insufficient price data for optimization (minimum 30 days)")

        # Check for missing data
        missing_data = prices.isnull().sum()
        if missing_data.any():
            missing_symbols = missing_data[missing_data > 0].index.tolist()
            logger.warning(f"Missing data for symbols: {missing_symbols}")

    return True


# Configuration class for optimization parameters
class OptimizationConfig:
    """Configuration class for optimization parameters"""

    def __init__(self):
        self.risk_free_rate = 0.02
        self.confidence_level = 0.95
        self.lookback_days = 252
        self.rebalance_frequency = 'monthly'
        self.weight_bounds = (0, 1)
        self.l2_gamma = 0.1
        self.tau = 0.05
        self.risk_aversion = 1.0

        # Method-specific defaults
        self.expected_returns_method = 'mean_historical_return'
        self.risk_model_method = 'sample_cov'
        self.optimization_target = 'max_sharpe'

        # HRP specific
        self.hrp_linkage_method = 'single'

        # Black-Litterman specific
        self.bl_pi_method = 'market_cap'

    def to_dict(self) -> Dict:
        """Convert config to dictionary"""
        return {
            'risk_free_rate': self.risk_free_rate,
            'confidence_level': self.confidence_level,
            'lookback_days': self.lookback_days,
            'rebalance_frequency': self.rebalance_frequency,
            'weight_bounds': self.weight_bounds,
            'l2_gamma': self.l2_gamma,
            'tau': self.tau,
            'risk_aversion': self.risk_aversion,
            'expected_returns_method': self.expected_returns_method,
            'risk_model_method': self.risk_model_method,
            'optimization_target': self.optimization_target,
            'hrp_linkage_method': self.hrp_linkage_method,
            'bl_pi_method': self.bl_pi_method
        }

    @classmethod
    def from_dict(cls, config_dict: Dict):
        """Create config from dictionary"""
        config = cls()
        for key, value in config_dict.items():
            if hasattr(config, key):
                setattr(config, key, value)
        return config

    @monitor_performance
    def calculate_efficient_frontier(self, mu: pd.Series, S: pd.DataFrame,
                                     num_points: int = 100,
                                     risk_range: Tuple[float, float] = None) -> Dict:
        """
        Calculate the efficient frontier

        Args:
            mu: Expected returns
            S: Covariance matrix
            num_points: Number of points on the frontier
            risk_range: Risk range (min_vol, max_vol)

        Returns:
            Dictionary with frontier data
        """
        try:
            with operation("calculate_efficient_frontier"):
                # Calculate risk range if not provided
                if risk_range is None:
                    # Min volatility portfolio
                    ef_temp = EfficientFrontier(mu, S)
                    ef_temp.min_volatility()
                    min_vol = ef_temp.portfolio_performance()[1]

                    # Max return portfolio (approximate)
                    max_return = mu.max()
                    ef_temp = EfficientFrontier(mu, S)
                    try:
                        ef_temp.efficient_return(max_return * 0.95)  # 95% of max return
                        max_vol = ef_temp.portfolio_performance()[1]
                    except:
                        max_vol = min_vol * 3  # Fallback

                    risk_range = (min_vol, max_vol)

                # Generate frontier points using return targets instead of risk targets
                min_return = mu.min()
                max_return = mu.max() * 0.95  # 95% of max return to avoid optimization issues

                target_returns = np.linspace(min_return, max_return, num_points)
                frontier_returns = []
                frontier_volatilities = []
                frontier_weights = []

                for target_return in target_returns:
                    try:
                        ef_temp = EfficientFrontier(mu, S)
                        ef_temp.efficient_risk(target_return)
                        ret, vol, _ = ef_temp.portfolio_performance()

                        # Only include points within reasonable bounds
                        if ret >= min_return and vol >= risk_range[0] and vol <= risk_range[1] * 1.2:
                            frontier_returns.append(ret)
                            frontier_volatilities.append(vol)
                            frontier_weights.append(dict(ef_temp.weights))

                    except Exception as e:
                        # Skip problematic points
                        logger.debug(f"Skipping target return {target_return:.4f}: {e}")
                        continue

                # Ensure we have some points
                if not frontier_returns:
                    raise ValueError("Unable to generate efficient frontier points")

                # Calculate max Sharpe portfolio
                ef_sharpe = EfficientFrontier(mu, S)
                ef_sharpe.max_sharpe(risk_free_rate=self.risk_free_rate)
                sharpe_performance = ef_sharpe.portfolio_performance(risk_free_rate=self.risk_free_rate)

                # Calculate min volatility portfolio
                ef_min_vol = EfficientFrontier(mu, S)
                ef_min_vol.min_volatility()
                min_vol_performance = ef_min_vol.portfolio_performance(risk_free_rate=self.risk_free_rate)

                # Sort frontier points by volatility for proper plotting
                frontier_data = list(zip(frontier_volatilities, frontier_returns, frontier_weights))
                frontier_data.sort(key=lambda x: x[0])  # Sort by volatility

                sorted_volatilities, sorted_returns, sorted_weights = zip(*frontier_data)

                return {
                    'returns': list(sorted_returns),
                    'volatilities': list(sorted_volatilities),
                    'weights': list(sorted_weights),
                    'max_sharpe': {
                        'return': sharpe_performance[0],
                        'volatility': sharpe_performance[1],
                        'sharpe_ratio': sharpe_performance[2],
                        'weights': dict(ef_sharpe.weights)
                    },
                    'min_volatility': {
                        'return': min_vol_performance[0],
                        'volatility': min_vol_performance[1],
                        'sharpe_ratio': min_vol_performance[2],
                        'weights': dict(ef_min_vol.weights)
                    },
                    'risk_range': risk_range,
                    'num_points': len(sorted_returns)
                }

        except Exception as e:
            logger.error(f"Error calculating efficient frontier: {e}")
            raise