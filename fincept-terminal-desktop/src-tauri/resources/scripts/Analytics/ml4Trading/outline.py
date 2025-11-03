
"""Machine Learning Outline Module
======================================

ML trading framework and architecture

===== DATA SOURCES REQUIRED =====
INPUT:
  - Historical market data and price time series
  - Technical indicators and market features
  - Alternative data sources (news, sentiment, etc.)
  - Market microstructure data and order flow
  - Economic indicators and macro data

OUTPUT:
  - Machine learning models for price prediction
  - Trading signals and strategy recommendations
  - Risk forecasts and position sizing suggestions
  - Performance metrics and backtest results
  - Feature importance and model interpretability

PARAMETERS:
  - model_type: Machine learning algorithm (default: 'random_forest')
  - training_window: Training data window size (default: 252 days)
  - prediction_horizon: Prediction horizon (default: 1 day)
  - feature_selection: Feature selection method (default: 'recursive')
  - validation_method: Model validation approach (default: 'time_series_split')
"""



import numpy as np
import pandas as pd
import warnings
warnings.filterwarnings('ignore')

# Core libraries
from typing import Dict, List, Tuple, Optional, Union, Any
from dataclasses import dataclass
from abc import ABC, abstractmethod
from datetime import datetime, timedelta
import logging
import pickle
import json

# Data and preprocessing
from sklearn.preprocessing import StandardScaler, MinMaxScaler, RobustScaler
from sklearn.model_selection import TimeSeriesSplit, cross_val_score
from sklearn.metrics import accuracy_score, precision_score, recall_score, f1_score
from sklearn.metrics import mean_squared_error, mean_absolute_error, r2_score

# Traditional ML models
from sklearn.linear_model import LinearRegression, Ridge, Lasso, LogisticRegression
from sklearn.ensemble import RandomForestRegressor, RandomForestClassifier
from sklearn.ensemble import GradientBoostingRegressor, GradientBoostingClassifier
from sklearn.tree import DecisionTreeRegressor, DecisionTreeClassifier
from sklearn.naive_bayes import MultinomialNB, GaussianNB
from sklearn.svm import SVR, SVC

# Dimensionality reduction and clustering
from sklearn.decomposition import PCA, FastICA, FactorAnalysis
from sklearn.manifold import TSNE
from sklearn.cluster import KMeans, AgglomerativeClustering, DBSCAN

# Time series analysis
from statsmodels.tsa.arima.model import ARIMA
from statsmodels.tsa.vector_ar.var_model import VAR
from statsmodels.tsa.stattools import coint, adfuller
from statsmodels.stats.stattools import jarque_bera

# Deep learning (conceptual - would need actual implementations)
# import tensorflow as tf
# import torch
# import torch.nn as nn

# Visualization
import matplotlib.pyplot as plt
import seaborn as sns
import plotly.graph_objects as go
import plotly.express as px
from plotly.subplots import make_subplots

# ============================================================================
# CONFIGURATION AND LOGGING
# ============================================================================

@dataclass
class ML4TConfig:
    """Configuration settings for ML4T module"""
    data_path: str = "data/"
    models_path: str = "models/"
    results_path: str = "results/"
    log_level: str = "INFO"
    random_state: int = 42
    test_size: float = 0.2
    validation_size: float = 0.2
    n_jobs: int = -1

    # Trading specific
    trading_costs: float = 0.001  # 0.1% per trade
    slippage: float = 0.0005     # 0.05% slippage
    initial_capital: float = 100000.0

    # Risk management
    max_position_size: float = 0.1  # 10% max position
    stop_loss: float = -0.05        # 5% stop loss
    take_profit: float = 0.15       # 15% take profit

class ML4TLogger:
    """Centralized logging for ML4T module"""
    def __init__(self, name: str = "ML4T", level: str = "INFO"):
        self.logger = logging.getLogger(name)
        self.logger.setLevel(getattr(logging, level))

        if not self.logger.handlers:
            handler = logging.StreamHandler()
            formatter = logging.Formatter(
                '%(asctime)s - %(name)s - %(levelname)s - %(message)s'
            )
            handler.setFormatter(formatter)
            self.logger.addHandler(handler)

    def info(self, msg): self.logger.info(msg)
    def warning(self, msg): self.logger.warning(msg)
    def error(self, msg): self.logger.error(msg)
    def debug(self, msg): self.logger.debug(msg)

# ============================================================================
# DATA MANAGEMENT AND ACQUISITION
# ============================================================================

class DataManager:
    """Handles data acquisition, storage, and preprocessing"""

    def __init__(self, config: ML4TConfig):
        self.config = config
        self.logger = ML4TLogger("DataManager")
        self.data_cache = {}

    def load_market_data(self, symbols: List[str], start_date: str, end_date: str) -> pd.DataFrame:
        """Load market data for given symbols and date range"""
        # Placeholder implementation - would integrate with actual data providers
        self.logger.info(f"Loading market data for {len(symbols)} symbols from {start_date} to {end_date}")

        # Generate synthetic data for demonstration
        dates = pd.date_range(start=start_date, end=end_date, freq='D')
        np.random.seed(self.config.random_state)

        data = {}
        for symbol in symbols:
            n_days = len(dates)
            # Generate realistic price data with random walk
            returns = np.random.normal(0.0005, 0.02, n_days)  # Daily returns
            prices = 100 * np.exp(np.cumsum(returns))  # Price series

            volumes = np.random.lognormal(10, 1, n_days)

            symbol_data = pd.DataFrame({
                'open': prices * (1 + np.random.normal(0, 0.001, n_days)),
                'high': prices * (1 + np.abs(np.random.normal(0, 0.005, n_days))),
                'low': prices * (1 - np.abs(np.random.normal(0, 0.005, n_days))),
                'close': prices,
                'volume': volumes,
                'symbol': symbol
            }, index=dates)

            data[symbol] = symbol_data

        combined_data = pd.concat(data.values(), ignore_index=False)
        combined_data = combined_data.reset_index().rename(columns={'index': 'date'})

        self.data_cache['market_data'] = combined_data
        return combined_data

    def load_fundamental_data(self, symbols: List[str]) -> pd.DataFrame:
        """Load fundamental data from SEC filings"""
        self.logger.info(f"Loading fundamental data for {len(symbols)} symbols")

        # Generate synthetic fundamental data
        np.random.seed(self.config.random_state)

        fundamental_data = []
        for symbol in symbols:
            data = {
                'symbol': symbol,
                'market_cap': np.random.lognormal(15, 2),  # Market cap
                'pe_ratio': np.random.lognormal(3, 0.5),   # P/E ratio
                'pb_ratio': np.random.lognormal(1, 0.3),   # P/B ratio
                'debt_to_equity': np.random.exponential(0.5),  # D/E ratio
                'roe': np.random.normal(0.12, 0.05),       # Return on Equity
                'roa': np.random.normal(0.06, 0.03),       # Return on Assets
                'current_ratio': np.random.lognormal(1, 0.2),  # Current ratio
                'revenue_growth': np.random.normal(0.05, 0.15),  # Revenue growth
                'earnings_growth': np.random.normal(0.08, 0.20), # Earnings growth
            }
            fundamental_data.append(data)

        df = pd.DataFrame(fundamental_data)
        self.data_cache['fundamental_data'] = df
        return df

    def load_alternative_data(self, data_type: str) -> pd.DataFrame:
        """Load alternative data (sentiment, satellite, etc.)"""
        self.logger.info(f"Loading alternative data: {data_type}")

        if data_type == "sentiment":
            return self._generate_sentiment_data()
        elif data_type == "satellite":
            return self._generate_satellite_data()
        elif data_type == "social":
            return self._generate_social_data()
        else:
            raise ValueError(f"Unknown alternative data type: {data_type}")

    def _generate_sentiment_data(self) -> pd.DataFrame:
        """Generate synthetic sentiment data"""
        dates = pd.date_range(start='2020-01-01', end='2023-12-31', freq='D')
        np.random.seed(self.config.random_state)

        sentiment_data = pd.DataFrame({
            'date': dates,
            'news_sentiment': np.random.normal(0, 1, len(dates)),
            'social_sentiment': np.random.normal(0, 1.2, len(dates)),
            'analyst_sentiment': np.random.normal(0, 0.8, len(dates)),
            'earnings_sentiment': np.random.normal(0, 1.5, len(dates)),
        })

        return sentiment_data

    def _generate_satellite_data(self) -> pd.DataFrame:
        """Generate synthetic satellite data"""
        # Placeholder for satellite imagery analysis results
        return pd.DataFrame({
            'region': ['US_MIDWEST', 'BRAZIL_CERRADO', 'ARGENTINA_PAMPAS'],
            'crop_health_index': [0.85, 0.78, 0.82],
            'estimated_yield': [105.2, 89.7, 94.3],
            'weather_risk': [0.15, 0.25, 0.18]
        })

    def _generate_social_data(self) -> pd.DataFrame:
        """Generate synthetic social media data"""
        dates = pd.date_range(start='2020-01-01', end='2023-12-31', freq='D')
        np.random.seed(self.config.random_state)

        return pd.DataFrame({
            'date': dates,
            'twitter_mentions': np.random.poisson(100, len(dates)),
            'reddit_sentiment': np.random.normal(0, 1, len(dates)),
            'google_trends': np.random.uniform(0, 100, len(dates)),
            'news_volume': np.random.poisson(50, len(dates))
        })

# ============================================================================
# FEATURE ENGINEERING AND ALPHA FACTORS
# ============================================================================

class AlphaFactorEngine:
    """Comprehensive alpha factor generation and evaluation"""

    def __init__(self, config: ML4TConfig):
        self.config = config
        self.logger = ML4TLogger("AlphaEngine")
        self.factor_library = {}

    def calculate_technical_factors(self, data: pd.DataFrame, symbol: str) -> pd.DataFrame:
        """Calculate technical indicators and factors"""
        df = data[data['symbol'] == symbol].copy()
        df = df.sort_values('date')

        # Price-based factors
        df['returns'] = df['close'].pct_change()
        df['log_returns'] = np.log(df['close']).diff()

        # Moving averages
        for window in [5, 10, 20, 50, 200]:
            df[f'sma_{window}'] = df['close'].rolling(window).mean()
            df[f'ema_{window}'] = df['close'].ewm(span=window).mean()

        # Price momentum
        for window in [5, 10, 20]:
            df[f'momentum_{window}'] = df['close'] / df['close'].shift(window) - 1

        # Volatility measures
        for window in [5, 10, 20]:
            df[f'volatility_{window}'] = df['returns'].rolling(window).std()

        # RSI
        df['rsi_14'] = self._calculate_rsi(df['close'], 14)

        # Bollinger Bands
        df['bb_upper'], df['bb_lower'] = self._calculate_bollinger_bands(df['close'], 20)
        df['bb_position'] = (df['close'] - df['bb_lower']) / (df['bb_upper'] - df['bb_lower'])

        # MACD
        df['macd'], df['macd_signal'] = self._calculate_macd(df['close'])
        df['macd_histogram'] = df['macd'] - df['macd_signal']

        # Volume factors
        df['volume_sma_20'] = df['volume'].rolling(20).mean()
        df['volume_ratio'] = df['volume'] / df['volume_sma_20']

        # Price-Volume factors
        df['vwap'] = (df['close'] * df['volume']).rolling(20).sum() / df['volume'].rolling(20).sum()
        df['price_volume_trend'] = ((df['close'] - df['close'].shift(1)) / df['close'].shift(1) * df['volume']).rolling(20).sum()

        return df

    def calculate_fundamental_factors(self, market_data: pd.DataFrame, fundamental_data: pd.DataFrame) -> pd.DataFrame:
        """Calculate fundamental analysis factors"""
        merged_data = market_data.merge(fundamental_data, on='symbol', how='left')

        # Valuation ratios
        merged_data['price_to_book'] = merged_data['pb_ratio']
        merged_data['price_to_earnings'] = merged_data['pe_ratio']

        # Quality factors
        merged_data['return_on_equity'] = merged_data['roe']
        merged_data['return_on_assets'] = merged_data['roa']
        merged_data['debt_equity_ratio'] = merged_data['debt_to_equity']

        # Growth factors
        merged_data['revenue_growth_rate'] = merged_data['revenue_growth']
        merged_data['earnings_growth_rate'] = merged_data['earnings_growth']

        return merged_data

    def calculate_alternative_factors(self, data: pd.DataFrame, alt_data: Dict[str, pd.DataFrame]) -> pd.DataFrame:
        """Integrate alternative data factors"""
        result = data.copy()

        # Sentiment factors
        if 'sentiment' in alt_data:
            sentiment = alt_data['sentiment']
            result = result.merge(sentiment, on='date', how='left')

            # Create sentiment momentum
            result['sentiment_momentum'] = result['news_sentiment'].rolling(5).mean()
            result['sentiment_volatility'] = result['news_sentiment'].rolling(10).std()

        # Social media factors
        if 'social' in alt_data:
            social = alt_data['social']
            result = result.merge(social, on='date', how='left')

            # Social activity factors
            result['social_activity'] = result['twitter_mentions'] + result['news_volume']
            result['social_sentiment_trend'] = result['reddit_sentiment'].rolling(7).mean()

        return result

    def _calculate_rsi(self, prices: pd.Series, window: int = 14) -> pd.Series:
        """Calculate Relative Strength Index"""
        delta = prices.diff()
        gain = delta.where(delta > 0, 0).rolling(window=window).mean()
        loss = (-delta.where(delta < 0, 0)).rolling(window=window).mean()
        rs = gain / loss
        rsi = 100 - (100 / (1 + rs))
        return rsi

    def _calculate_bollinger_bands(self, prices: pd.Series, window: int = 20, num_std: float = 2) -> Tuple[pd.Series, pd.Series]:
        """Calculate Bollinger Bands"""
        sma = prices.rolling(window).mean()
        std = prices.rolling(window).std()
        upper = sma + (std * num_std)
        lower = sma - (std * num_std)
        return upper, lower

    def _calculate_macd(self, prices: pd.Series, fast: int = 12, slow: int = 26, signal: int = 9) -> Tuple[pd.Series, pd.Series]:
        """Calculate MACD"""
        ema_fast = prices.ewm(span=fast).mean()
        ema_slow = prices.ewm(span=slow).mean()
        macd = ema_fast - ema_slow
        macd_signal = macd.ewm(span=signal).mean()
        return macd, macd_signal

    def evaluate_factor_performance(self, data: pd.DataFrame, factor_name: str, target: str = 'returns') -> Dict:
        """Evaluate factor performance using various metrics"""
        # Information Coefficient (Spearman correlation)
        ic = data[factor_name].corr(data[target], method='spearman')

        # Factor returns by quintiles
        data['factor_quintile'] = pd.qcut(data[factor_name].rank(), 5, labels=['Q1', 'Q2', 'Q3', 'Q4', 'Q5'])
        quintile_returns = data.groupby('factor_quintile')[target].mean()

        # Long-short return (Q5 - Q1)
        long_short_return = quintile_returns['Q5'] - quintile_returns['Q1']

        return {
            'information_coefficient': ic,
            'quintile_returns': quintile_returns.to_dict(),
            'long_short_return': long_short_return,
            'factor_stats': {
                'mean': data[factor_name].mean(),
                'std': data[factor_name].std(),
                'skewness': data[factor_name].skew(),
                'kurtosis': data[factor_name].kurtosis()
            }
        }

# ============================================================================
# MACHINE LEARNING MODELS
# ============================================================================

class BaseMLModel(ABC):
    """Abstract base class for all ML models"""

    def __init__(self, config: ML4TConfig):
        self.config = config
        self.logger = ML4TLogger(self.__class__.__name__)
        self.model = None
        self.is_fitted = False
        self.feature_importance_ = None

    @abstractmethod
    def fit(self, X: pd.DataFrame, y: pd.Series) -> 'BaseMLModel':
        pass

    @abstractmethod
    def predict(self, X: pd.DataFrame) -> np.ndarray:
        pass

    def evaluate(self, X_test: pd.DataFrame, y_test: pd.Series) -> Dict:
        """Evaluate model performance"""
        predictions = self.predict(X_test)

        if len(np.unique(y_test)) == 2:  # Binary classification
            metrics = {
                'accuracy': accuracy_score(y_test, predictions),
                'precision': precision_score(y_test, predictions),
                'recall': recall_score(y_test, predictions),
                'f1_score': f1_score(y_test, predictions)
            }
        else:  # Regression
            metrics = {
                'mse': mean_squared_error(y_test, predictions),
                'mae': mean_absolute_error(y_test, predictions),
                'r2': r2_score(y_test, predictions),
                'rmse': np.sqrt(mean_squared_error(y_test, predictions))
            }

        return metrics

class LinearModel(BaseMLModel):
    """Linear regression and classification models"""

    def __init__(self, config: ML4TConfig, model_type: str = 'ridge', alpha: float = 1.0):
        super().__init__(config)
        self.model_type = model_type
        self.alpha = alpha

        if model_type == 'linear':
            self.model = LinearRegression()
        elif model_type == 'ridge':
            self.model = Ridge(alpha=alpha, random_state=config.random_state)
        elif model_type == 'lasso':
            self.model = Lasso(alpha=alpha, random_state=config.random_state)
        elif model_type == 'logistic':
            self.model = LogisticRegression(random_state=config.random_state)
        else:
            raise ValueError(f"Unknown model type: {model_type}")

    def fit(self, X: pd.DataFrame, y: pd.Series) -> 'LinearModel':
        self.model.fit(X, y)
        self.is_fitted = True

        # Feature importance for linear models (coefficients)
        if hasattr(self.model, 'coef_'):
            self.feature_importance_ = pd.Series(
                np.abs(self.model.coef_),
                index=X.columns
            ).sort_values(ascending=False)

        return self

    def predict(self, X: pd.DataFrame) -> np.ndarray:
        if not self.is_fitted:
            raise ValueError("Model must be fitted before making predictions")
        return self.model.predict(X)

class TreeBasedModel(BaseMLModel):
    """Random Forest and Gradient Boosting models"""

    def __init__(self, config: ML4TConfig, model_type: str = 'random_forest', **kwargs):
        super().__init__(config)
        self.model_type = model_type

        # Base parameters for all models
        base_params = {
            'n_estimators': 100,
            'random_state': config.random_state
        }

        # Add n_jobs only for models that support it (Random Forest)
        if model_type in ['random_forest', 'random_forest_classifier']:
            base_params['n_jobs'] = config.n_jobs

        # Update with user-provided parameters
        base_params.update(kwargs)

        if model_type == 'random_forest':
            self.model = RandomForestRegressor(**base_params)
        elif model_type == 'random_forest_classifier':
            self.model = RandomForestClassifier(**base_params)
        elif model_type == 'gradient_boosting':
            self.model = GradientBoostingRegressor(**base_params)
        elif model_type == 'gradient_boosting_classifier':
            self.model = GradientBoostingClassifier(**base_params)
        else:
            raise ValueError(f"Unknown model type: {model_type}")

    def fit(self, X: pd.DataFrame, y: pd.Series) -> 'TreeBasedModel':
        self.model.fit(X, y)
        self.is_fitted = True

        # Feature importance
        self.feature_importance_ = pd.Series(
            self.model.feature_importances_,
            index=X.columns
        ).sort_values(ascending=False)

        return self

    def predict(self, X: pd.DataFrame) -> np.ndarray:
        if not self.is_fitted:
            raise ValueError("Model must be fitted before making predictions")
        return self.model.predict(X)

class TimeSeriesModel(BaseMLModel):
    """Time series models (ARIMA, VAR, etc.)"""

    def __init__(self, config: ML4TConfig, model_type: str = 'arima', **kwargs):
        super().__init__(config)
        self.model_type = model_type
        self.model_params = kwargs

    def fit(self, X: pd.DataFrame, y: pd.Series) -> 'TimeSeriesModel':
        if self.model_type == 'arima':
            order = self.model_params.get('order', (1, 1, 1))
            self.model = ARIMA(y, order=order)
            self.model = self.model.fit()

        self.is_fitted = True
        return self

    def predict(self, X: pd.DataFrame) -> np.ndarray:
        if not self.is_fitted:
            raise ValueError("Model must be fitted before making predictions")

        if self.model_type == 'arima':
            forecast = self.model.forecast(steps=len(X))
            return forecast.values

        return np.array([])

# ============================================================================
# PORTFOLIO OPTIMIZATION AND RISK MANAGEMENT
# ============================================================================

class PortfolioOptimizer:
    """Modern Portfolio Theory and risk-based optimization"""

    def __init__(self, config: ML4TConfig):
        self.config = config
        self.logger = ML4TLogger("PortfolioOptimizer")

    def mean_variance_optimization(self, returns: pd.DataFrame, risk_aversion: float = 1.0) -> pd.Series:
        """Classic Markowitz mean-variance optimization"""
        mu = returns.mean()
        cov_matrix = returns.cov()

        n = len(mu)
        # Objective: maximize mu'w - 0.5 * risk_aversion * w'Σw
        # Subject to: sum(w) = 1

        # Simplified solution (equal risk contribution)
        inv_cov = np.linalg.pinv(cov_matrix)
        ones = np.ones((n, 1))

        # Calculate optimal weights
        weights = inv_cov @ mu
        weights = weights / np.sum(weights)

        return pd.Series(weights, index=returns.columns)

    def risk_parity_optimization(self, returns: pd.DataFrame) -> pd.Series:
        """Risk parity portfolio optimization"""
        cov_matrix = returns.cov()

        # Start with equal weights
        n = len(returns.columns)
        weights = np.ones(n) / n

        # Iterative risk budgeting (simplified)
        for _ in range(50):  # Max iterations
            portfolio_vol = np.sqrt(weights.T @ cov_matrix @ weights)
            marginal_contrib = cov_matrix @ weights / portfolio_vol
            risk_contrib = weights * marginal_contrib

            # Adjust weights to equalize risk contributions
            target_risk = np.mean(risk_contrib)
            adjustment = target_risk / risk_contrib
            weights = weights * adjustment
            weights = weights / np.sum(weights)  # Normalize

        return pd.Series(weights, index=returns.columns)

    def black_litterman_optimization(self, returns: pd.DataFrame, views: Dict = None) -> pd.Series:
        """Black-Litterman model with investor views"""
        # Simplified implementation
        mu = returns.mean()
        cov_matrix = returns.cov()

        if views is None:
            # No views, revert to market capitalization weighting
            weights = np.ones(len(returns.columns)) / len(returns.columns)
        else:
            # Incorporate views (simplified)
            weights = self.mean_variance_optimization(returns)

        return pd.Series(weights, index=returns.columns)

    def calculate_portfolio_metrics(self, returns: pd.Series, benchmark_returns: pd.Series = None) -> Dict:
        """Calculate comprehensive portfolio performance metrics"""

        # Basic metrics
        total_return = (1 + returns).prod() - 1
        annualized_return = (1 + returns.mean()) ** 252 - 1
        annualized_volatility = returns.std() * np.sqrt(252)
        sharpe_ratio = annualized_return / annualized_volatility if annualized_volatility > 0 else 0

        # Downside metrics
        downside_returns = returns[returns < 0]
        downside_deviation = downside_returns.std() * np.sqrt(252)
        sortino_ratio = annualized_return / downside_deviation if downside_deviation > 0 else 0

        # Drawdown analysis
        cumulative_returns = (1 + returns).cumprod()
        rolling_max = cumulative_returns.expanding().max()
        drawdown = (cumulative_returns - rolling_max) / rolling_max
        max_drawdown = drawdown.min()

        # VaR and CVaR
        var_95 = returns.quantile(0.05)
        cvar_95 = returns[returns <= var_95].mean()

        metrics = {
            'total_return': total_return,
            'annualized_return': annualized_return,
            'annualized_volatility': annualized_volatility,
            'sharpe_ratio': sharpe_ratio,
            'sortino_ratio': sortino_ratio,
            'max_drawdown': max_drawdown,
            'var_95': var_95,
            'cvar_95': cvar_95,
            'calmar_ratio': annualized_return / abs(max_drawdown) if max_drawdown != 0 else 0
        }

        # Benchmark comparison
        if benchmark_returns is not None:
            benchmark_return = (1 + benchmark_returns.mean()) ** 252 - 1
            alpha = annualized_return - benchmark_return

            # Beta calculation
            covariance = returns.cov(benchmark_returns)
            benchmark_variance = benchmark_returns.var()
            beta = covariance / benchmark_variance if benchmark_variance > 0 else 0

            # Information ratio
            active_returns = returns - benchmark_returns
            tracking_error = active_returns.std() * np.sqrt(252)
            information_ratio = (active_returns.mean() * 252) / tracking_error if tracking_error > 0 else 0

            metrics.update({
                'alpha': alpha,
                'beta': beta,
                'information_ratio': information_ratio,
                'tracking_error': tracking_error
            })

        return metrics

# ============================================================================
# BACKTESTING ENGINE
# ============================================================================

class BacktestEngine:
    """Comprehensive backtesting framework"""

    def __init__(self, config: ML4TConfig):
        self.config = config
        self.logger = ML4TLogger("BacktestEngine")
        self.results = {}

    def run_backtest(self, strategy_signals: pd.DataFrame, price_data: pd.DataFrame,
                    initial_capital: float = None) -> Dict:
        """Execute complete backtest simulation"""

        if initial_capital is None:
            initial_capital = self.config.initial_capital

        # Initialize portfolio
        portfolio = {
            'cash': initial_capital,
            'positions': {},
            'portfolio_value': [initial_capital],
            'dates': [],
            'trades': []
        }

        # Sort by date
        signals = strategy_signals.sort_values('date')
        prices = price_data.sort_values('date')

        # Execute backtest day by day
        for date in signals['date'].unique():
            daily_signals = signals[signals['date'] == date]
            daily_prices = prices[prices['date'] == date]

            portfolio = self._process_daily_signals(portfolio, daily_signals, daily_prices, date)

            # Calculate portfolio value
            portfolio_value = self._calculate_portfolio_value(portfolio, daily_prices)
            portfolio['portfolio_value'].append(portfolio_value)
            portfolio['dates'].append(date)

        # Calculate performance metrics
        returns = pd.Series(portfolio['portfolio_value']).pct_change().dropna()
        portfolio_optimizer = PortfolioOptimizer(self.config)
        metrics = portfolio_optimizer.calculate_portfolio_metrics(returns)

        return {
            'portfolio_values': portfolio['portfolio_value'],
            'dates': portfolio['dates'],
            'trades': portfolio['trades'],
            'positions': portfolio['positions'],
            'metrics': metrics,
            'returns': returns
        }

    def _process_daily_signals(self, portfolio: Dict, signals: pd.DataFrame,
                              prices: pd.DataFrame, date) -> Dict:
        """Process trading signals for a single day"""

        for _, signal in signals.iterrows():
            symbol = signal['symbol']
            signal_strength = signal.get('signal', 0)

            # Get current price
            price_row = prices[prices['symbol'] == symbol]
            if price_row.empty:
                continue

            current_price = price_row['close'].iloc[0]

            # Calculate position size based on signal strength
            target_position_value = portfolio['cash'] * self.config.max_position_size * signal_strength
            target_shares = int(target_position_value / current_price)

            # Current position
            current_shares = portfolio['positions'].get(symbol, 0)
            shares_to_trade = target_shares - current_shares

            if abs(shares_to_trade) > 0:
                trade_value = shares_to_trade * current_price
                trading_cost = abs(trade_value) * self.config.trading_costs

                # Check if we have enough cash for the trade
                if shares_to_trade > 0:  # Buying
                    total_cost = trade_value + trading_cost
                    if total_cost <= portfolio['cash']:
                        portfolio['cash'] -= total_cost
                        portfolio['positions'][symbol] = current_shares + shares_to_trade

                        # Record trade
                        portfolio['trades'].append({
                            'date': date,
                            'symbol': symbol,
                            'shares': shares_to_trade,
                            'price': current_price,
                            'value': trade_value,
                            'cost': trading_cost,
                            'type': 'BUY'
                        })

                else:  # Selling
                    portfolio['cash'] += abs(trade_value) - trading_cost
                    portfolio['positions'][symbol] = current_shares + shares_to_trade

                    # Record trade
                    portfolio['trades'].append({
                        'date': date,
                        'symbol': symbol,
                        'shares': shares_to_trade,
                        'price': current_price,
                        'value': trade_value,
                        'cost': trading_cost,
                        'type': 'SELL'
                    })

        return portfolio

    def _calculate_portfolio_value(self, portfolio: Dict, prices: pd.DataFrame) -> float:
        """Calculate total portfolio value"""
        total_value = portfolio['cash']

        for symbol, shares in portfolio['positions'].items():
            if shares != 0:
                price_row = prices[prices['symbol'] == symbol]
                if not price_row.empty:
                    current_price = price_row['close'].iloc[0]
                    total_value += shares * current_price

        return total_value

    def run_walk_forward_analysis(self, model, data: pd.DataFrame, features: List[str],
                                 target: str, train_window: int = 252,
                                 test_window: int = 21) -> Dict:
        """Run walk-forward analysis for model validation"""

        results = []
        data = data.sort_values('date')

        start_idx = train_window
        while start_idx + test_window < len(data):
            # Training data
            train_data = data.iloc[start_idx - train_window:start_idx]
            test_data = data.iloc[start_idx:start_idx + test_window]

            # Prepare features
            X_train = train_data[features].fillna(0)
            y_train = train_data[target].fillna(0)
            X_test = test_data[features].fillna(0)
            y_test = test_data[target].fillna(0)

            # Train model
            model.fit(X_train, y_train)

            # Make predictions
            predictions = model.predict(X_test)

            # Evaluate
            metrics = model.evaluate(X_test, y_test)

            results.append({
                'train_start': train_data['date'].iloc[0],
                'train_end': train_data['date'].iloc[-1],
                'test_start': test_data['date'].iloc[0],
                'test_end': test_data['date'].iloc[-1],
                'metrics': metrics,
                'predictions': predictions,
                'actual': y_test.values
            })

            start_idx += test_window

        return {
            'results': results,
            'summary_metrics': self._summarize_walk_forward_results(results)
        }

    def _summarize_walk_forward_results(self, results: List[Dict]) -> Dict:
        """Summarize walk-forward analysis results"""
        metrics_list = [r['metrics'] for r in results]

        summary = {}
        for metric in metrics_list[0].keys():
            values = [m[metric] for m in metrics_list]
            summary[f'{metric}_mean'] = np.mean(values)
            summary[f'{metric}_std'] = np.std(values)
            summary[f'{metric}_min'] = np.min(values)
            summary[f'{metric}_max'] = np.max(values)

        return summary

# ============================================================================
# STRATEGY DEVELOPMENT FRAMEWORK
# ============================================================================

class TradingStrategy(ABC):
    """Abstract base class for trading strategies"""

    def __init__(self, name: str, config: ML4TConfig):
        self.name = name
        self.config = config
        self.logger = ML4TLogger(f"Strategy_{name}")
        self.model = None
        self.features = []

    @abstractmethod
    def generate_signals(self, data: pd.DataFrame) -> pd.DataFrame:
        """Generate trading signals"""
        pass

    def set_model(self, model: BaseMLModel):
        """Set the ML model for the strategy"""
        self.model = model

    def set_features(self, features: List[str]):
        """Set feature list for the strategy"""
        self.features = features

class MeanReversionStrategy(TradingStrategy):
    """Mean reversion strategy implementation"""

    def __init__(self, config: ML4TConfig, lookback_window: int = 20,
                 entry_threshold: float = 2.0, exit_threshold: float = 0.5):
        super().__init__("MeanReversion", config)
        self.lookback_window = lookback_window
        self.entry_threshold = entry_threshold
        self.exit_threshold = exit_threshold

    def generate_signals(self, data: pd.DataFrame) -> pd.DataFrame:
        """Generate mean reversion signals"""
        signals = []

        for symbol in data['symbol'].unique():
            symbol_data = data[data['symbol'] == symbol].copy()
            symbol_data = symbol_data.sort_values('date')

            # Calculate z-score
            symbol_data['rolling_mean'] = symbol_data['close'].rolling(self.lookback_window).mean()
            symbol_data['rolling_std'] = symbol_data['close'].rolling(self.lookback_window).std()
            symbol_data['z_score'] = (symbol_data['close'] - symbol_data['rolling_mean']) / symbol_data['rolling_std']

            # Generate signals
            symbol_data['signal'] = 0

            # Long signal when price is oversold (negative z-score)
            symbol_data.loc[symbol_data['z_score'] < -self.entry_threshold, 'signal'] = 1

            # Short signal when price is overbought (positive z-score)
            symbol_data.loc[symbol_data['z_score'] > self.entry_threshold, 'signal'] = -1

            # Exit signals
            symbol_data.loc[abs(symbol_data['z_score']) < self.exit_threshold, 'signal'] = 0

            signals.append(symbol_data[['date', 'symbol', 'signal']])

        return pd.concat(signals, ignore_index=True)

class MomentumStrategy(TradingStrategy):
    """Momentum strategy implementation"""

    def __init__(self, config: ML4TConfig, lookback_window: int = 12,
                 holding_period: int = 3):
        super().__init__("Momentum", config)
        self.lookback_window = lookback_window
        self.holding_period = holding_period

    def generate_signals(self, data: pd.DataFrame) -> pd.DataFrame:
        """Generate momentum signals"""
        signals = []

        for symbol in data['symbol'].unique():
            symbol_data = data[data['symbol'] == symbol].copy()
            symbol_data = symbol_data.sort_values('date')

            # Calculate momentum
            symbol_data['momentum'] = symbol_data['close'].pct_change(self.lookback_window)

            # Rank-based signals
            symbol_data['momentum_rank'] = symbol_data['momentum'].rank(pct=True)

            # Generate signals based on momentum quintiles
            symbol_data['signal'] = 0
            symbol_data.loc[symbol_data['momentum_rank'] > 0.8, 'signal'] = 1  # Top quintile
            symbol_data.loc[symbol_data['momentum_rank'] < 0.2, 'signal'] = -1  # Bottom quintile

            signals.append(symbol_data[['date', 'symbol', 'signal']])

        return pd.concat(signals, ignore_index=True)

class MLStrategy(TradingStrategy):
    """Machine Learning-based strategy"""

    def __init__(self, config: ML4TConfig, model: BaseMLModel,
                 features: List[str], target: str = 'future_returns'):
        super().__init__("MLStrategy", config)
        self.model = model
        self.features = features
        self.target = target

    def generate_signals(self, data: pd.DataFrame) -> pd.DataFrame:
        """Generate ML-based signals"""

        # Prepare features
        feature_data = data[self.features].fillna(0)

        # Make predictions
        predictions = self.model.predict(feature_data)

        # Convert predictions to signals
        signals_df = data[['date', 'symbol']].copy()
        signals_df['prediction'] = predictions

        # Signal generation logic
        prediction_threshold = np.std(predictions) * 0.5
        signals_df['signal'] = 0
        signals_df.loc[predictions > prediction_threshold, 'signal'] = 1
        signals_df.loc[predictions < -prediction_threshold, 'signal'] = -1

        return signals_df[['date', 'symbol', 'signal']]

# ============================================================================
# VISUALIZATION AND REPORTING
# ============================================================================

class ML4TVisualizer:
    """Comprehensive visualization toolkit for ML4T"""

    def __init__(self, config: ML4TConfig):
        self.config = config
        self.logger = ML4TLogger("Visualizer")

    def plot_portfolio_performance(self, backtest_results: Dict, benchmark_data: pd.DataFrame = None):
        """Plot portfolio performance with benchmarks"""

        fig = make_subplots(
            rows=3, cols=2,
            subplot_titles=['Portfolio Value', 'Returns Distribution',
                          'Drawdown', 'Rolling Sharpe Ratio',
                          'Trade Analysis', 'Feature Importance'],
            specs=[[{"secondary_y": False}, {"secondary_y": False}],
                   [{"secondary_y": False}, {"secondary_y": False}],
                   [{"secondary_y": False}, {"secondary_y": False}]]
        )

        # Portfolio value
        dates = pd.to_datetime(backtest_results['dates'])
        portfolio_values = backtest_results['portfolio_values'][1:]  # Remove initial value

        fig.add_trace(
            go.Scatter(x=dates, y=portfolio_values, name='Portfolio', line=dict(color='blue')),
            row=1, col=1
        )

        # Returns distribution
        returns = backtest_results['returns']
        fig.add_trace(
            go.Histogram(x=returns, name='Returns', nbinsx=50, opacity=0.7),
            row=1, col=2
        )

        # Drawdown
        cumulative_returns = (1 + returns).cumprod()
        rolling_max = cumulative_returns.expanding().max()
        drawdown = (cumulative_returns - rolling_max) / rolling_max

        fig.add_trace(
            go.Scatter(x=dates[1:], y=drawdown * 100, name='Drawdown %',
                      fill='tonexty', line=dict(color='red')),
            row=2, col=1
        )

        # Rolling Sharpe ratio
        rolling_sharpe = returns.rolling(window=252).mean() / returns.rolling(window=252).std() * np.sqrt(252)
        fig.add_trace(
            go.Scatter(x=dates[1:], y=rolling_sharpe, name='Rolling Sharpe', line=dict(color='green')),
            row=2, col=2
        )

        # Trade analysis
        trades_df = pd.DataFrame(backtest_results['trades'])
        if not trades_df.empty:
            trade_pnl = trades_df.groupby('date')['value'].sum()
            fig.add_trace(
                go.Bar(x=trade_pnl.index, y=trade_pnl.values, name='Daily P&L'),
                row=3, col=1
            )

        fig.update_layout(height=1200, title_text="Portfolio Performance Dashboard")
        fig.show()

    def plot_factor_analysis(self, factor_performance: Dict):
        """Plot factor analysis results"""

        fig = make_subplots(
            rows=2, cols=2,
            subplot_titles=['Factor Returns by Quintile', 'Information Coefficient Over Time',
                          'Factor Distribution', 'Cumulative Factor Returns']
        )

        # Factor returns by quintile
        quintiles = list(factor_performance['quintile_returns'].keys())
        returns = list(factor_performance['quintile_returns'].values())

        fig.add_trace(
            go.Bar(x=quintiles, y=returns, name='Quintile Returns'),
            row=1, col=1
        )

        # Add other factor analysis plots here...

        fig.update_layout(height=800, title_text="Factor Analysis Dashboard")
        fig.show()

    def plot_model_performance(self, walk_forward_results: Dict):
        """Plot model performance over time"""

        results = walk_forward_results['results']

        # Extract metrics over time
        dates = [r['test_start'] for r in results]
        metrics = [r['metrics'] for r in results]

        fig = make_subplots(
            rows=2, cols=2,
            subplot_titles=['Model Accuracy Over Time', 'Prediction vs Actual',
                          'Feature Importance', 'Residual Analysis']
        )

        # Model performance over time
        if 'accuracy' in metrics[0]:
            accuracy_scores = [m['accuracy'] for m in metrics]
            fig.add_trace(
                go.Scatter(x=dates, y=accuracy_scores, name='Accuracy', line=dict(color='blue')),
                row=1, col=1
            )
        elif 'r2' in metrics[0]:
            r2_scores = [m['r2'] for m in metrics]
            fig.add_trace(
                go.Scatter(x=dates, y=r2_scores, name='R²', line=dict(color='blue')),
                row=1, col=1
            )

        # Prediction vs Actual scatter plot
        all_predictions = np.concatenate([r['predictions'] for r in results])
        all_actual = np.concatenate([r['actual'] for r in results])

        fig.add_trace(
            go.Scatter(x=all_actual, y=all_predictions, mode='markers',
                      name='Pred vs Actual', opacity=0.6),
            row=1, col=2
        )

        # Add diagonal line for perfect predictions
        min_val, max_val = min(all_actual.min(), all_predictions.min()), max(all_actual.max(), all_predictions.max())
        fig.add_trace(
            go.Scatter(x=[min_val, max_val], y=[min_val, max_val],
                      mode='lines', name='Perfect Prediction', line=dict(color='red', dash='dash')),
            row=1, col=2
        )

        fig.update_layout(height=800, title_text="Model Performance Analysis")
        fig.show()

# ============================================================================
# MAIN ML4T ORCHESTRATOR
# ============================================================================

class ML4TFramework:
    """Main orchestrator for the ML4T framework"""

    def __init__(self, config: ML4TConfig = None):
        self.config = config or ML4TConfig()
        self.logger = ML4TLogger("ML4TFramework")

        # Initialize components
        self.data_manager = DataManager(self.config)
        self.alpha_engine = AlphaFactorEngine(self.config)
        self.portfolio_optimizer = PortfolioOptimizer(self.config)
        self.backtest_engine = BacktestEngine(self.config)
        self.visualizer = ML4TVisualizer(self.config)

        # Storage for results
        self.data = {}
        self.factors = {}
        self.models = {}
        self.strategies = {}
        self.results = {}

    def load_data(self, symbols: List[str], start_date: str, end_date: str,
                  include_fundamental: bool = True, include_alternative: bool = True):
        """Load all required data"""

        self.logger.info("Loading market data...")
        self.data['market'] = self.data_manager.load_market_data(symbols, start_date, end_date)

        if include_fundamental:
            self.logger.info("Loading fundamental data...")
            self.data['fundamental'] = self.data_manager.load_fundamental_data(symbols)

        if include_alternative:
            self.logger.info("Loading alternative data...")
            self.data['alternative'] = {
                'sentiment': self.data_manager.load_alternative_data('sentiment'),
                'social': self.data_manager.load_alternative_data('social')
            }

    def engineer_features(self, symbols: List[str]):
        """Generate comprehensive feature set"""

        self.logger.info("Engineering features...")

        all_features = []

        for symbol in symbols:
            # Technical factors
            technical_data = self.alpha_engine.calculate_technical_factors(self.data['market'], symbol)

            # Fundamental factors
            if 'fundamental' in self.data:
                fundamental_data = self.alpha_engine.calculate_fundamental_factors(
                    technical_data, self.data['fundamental']
                )
            else:
                fundamental_data = technical_data

            # Alternative data factors
            if 'alternative' in self.data:
                final_data = self.alpha_engine.calculate_alternative_factors(
                    fundamental_data, self.data['alternative']
                )
            else:
                final_data = fundamental_data

            all_features.append(final_data)

        self.factors['engineered'] = pd.concat(all_features, ignore_index=True)

        # Create target variable (future returns)
        self.factors['engineered'] = self.factors['engineered'].sort_values(['symbol', 'date'])
        self.factors['engineered']['future_returns'] = self.factors['engineered'].groupby('symbol')['returns'].shift(-1)

        # Remove rows with missing target
        self.factors['engineered'] = self.factors['engineered'].dropna(subset=['future_returns'])

        return self.factors['engineered']

    def train_models(self, feature_data: pd.DataFrame, models_config: List[Dict]):
        """Train multiple ML models"""

        self.logger.info("Training models...")

        # Prepare features and target
        feature_columns = [col for col in feature_data.columns
                          if col not in ['date', 'symbol', 'future_returns', 'open', 'high', 'low', 'close', 'volume']]

        X = feature_data[feature_columns].fillna(0)
        y = feature_data['future_returns'].fillna(0)

        # Train each model
        for model_config in models_config:
            model_name = model_config['name']
            model_type = model_config['type']
            model_params = model_config.get('params', {})

            self.logger.info(f"Training {model_name} ({model_type})...")

            if model_type in ['linear', 'ridge', 'lasso', 'logistic']:
                model = LinearModel(self.config, model_type, **model_params)
            elif model_type in ['random_forest', 'gradient_boosting']:
                model = TreeBasedModel(self.config, model_type, **model_params)
            elif model_type in ['arima']:
                model = TimeSeriesModel(self.config, model_type, **model_params)
            else:
                self.logger.warning(f"Unknown model type: {model_type}")
                continue

            # Train model
            model.fit(X, y)
            self.models[model_name] = model

            self.logger.info(f"Model {model_name} trained successfully")

    def create_strategies(self, strategy_configs: List[Dict]):
        """Create trading strategies"""

        self.logger.info("Creating strategies...")

        for strategy_config in strategy_configs:
            strategy_name = strategy_config['name']
            strategy_type = strategy_config['type']
            strategy_params = strategy_config.get('params', {})

            if strategy_type == 'mean_reversion':
                strategy = MeanReversionStrategy(self.config, **strategy_params)
            elif strategy_type == 'momentum':
                strategy = MomentumStrategy(self.config, **strategy_params)
            elif strategy_type == 'ml_strategy':
                model_name = strategy_params.get('model_name')
                if model_name in self.models:
                    features = strategy_params.get('features', [])
                    strategy = MLStrategy(self.config, self.models[model_name], features)
                else:
                    self.logger.warning(f"Model {model_name} not found for ML strategy")
                    continue
            else:
                self.logger.warning(f"Unknown strategy type: {strategy_type}")
                continue

            self.strategies[strategy_name] = strategy
            self.logger.info(f"Strategy {strategy_name} created successfully")

    def run_backtests(self):
        """Run backtests for all strategies"""

        self.logger.info("Running backtests...")

        for strategy_name, strategy in self.strategies.items():
            self.logger.info(f"Backtesting strategy: {strategy_name}")

            # Generate signals
            signals = strategy.generate_signals(self.factors['engineered'])

            # Run backtest
            backtest_results = self.backtest_engine.run_backtest(
                signals, self.data['market'], self.config.initial_capital
            )

            self.results[strategy_name] = backtest_results

            # Log performance summary
            metrics = backtest_results['metrics']
            self.logger.info(f"Strategy {strategy_name} - Sharpe: {metrics['sharpe_ratio']:.3f}, "
                           f"Return: {metrics['annualized_return']:.3f}, "
                           f"MaxDD: {metrics['max_drawdown']:.3f}")

    def generate_report(self):
        """Generate comprehensive performance report"""

        self.logger.info("Generating performance report...")

        # Create summary report
        summary_data = []

        for strategy_name, results in self.results.items():
            metrics = results['metrics']
            summary_data.append({
                'Strategy': strategy_name,
                'Total Return': f"{metrics['total_return']:.2%}",
                'Annualized Return': f"{metrics['annualized_return']:.2%}",
                'Volatility': f"{metrics['annualized_volatility']:.2%}",
                'Sharpe Ratio': f"{metrics['sharpe_ratio']:.3f}",
                'Max Drawdown': f"{metrics['max_drawdown']:.2%}",
                'Calmar Ratio': f"{metrics['calmar_ratio']:.3f}"
            })

        summary_df = pd.DataFrame(summary_data)

        print("\n" + "="*80)
        print("ML4T FRAMEWORK - PERFORMANCE SUMMARY")
        print("="*80)
        print(summary_df.to_string(index=False))
        print("="*80)

        return summary_df

    def visualize_results(self, strategy_name: str = None):
        """Create visualizations for results"""

        if strategy_name is None:
            # Show all strategies
            for name in self.results.keys():
                self.visualizer.plot_portfolio_performance(self.results[name])
        else:
            if strategy_name in self.results:
                self.visualizer.plot_portfolio_performance(self.results[strategy_name])
            else:
                self.logger.warning(f"Strategy {strategy_name} not found in results")

# ============================================================================
# EXAMPLE USAGE AND DEMO
# ============================================================================

def run_ml4t_demo():
    """Demonstration of the ML4T framework"""

    print("Initializing ML4T Framework...")

    # Initialize framework
    config = ML4TConfig()
    ml4t = ML4TFramework(config)

    # Define universe
    symbols = ['AAPL', 'GOOGL', 'MSFT', 'AMZN', 'TSLA']
    start_date = '2020-01-01'
    end_date = '2023-12-31'

    # Load data
    ml4t.load_data(symbols, start_date, end_date)

    # Engineer features
    feature_data = ml4t.engineer_features(symbols)

    # Define models to train
    models_config = [
        {'name': 'ridge_model', 'type': 'ridge', 'params': {'alpha': 1.0}},
        {'name': 'random_forest', 'type': 'random_forest', 'params': {'n_estimators': 100}},
        {'name': 'gradient_boosting', 'type': 'gradient_boosting', 'params': {'n_estimators': 100}}
    ]

    # Train models
    ml4t.train_models(feature_data, models_config)

    # Define strategies
    feature_columns = [col for col in feature_data.columns
                      if col not in ['date', 'symbol', 'future_returns', 'open', 'high', 'low', 'close', 'volume']]

    strategies_config = [
        {'name': 'mean_reversion', 'type': 'mean_reversion', 'params': {'lookback_window': 20}},
        {'name': 'momentum', 'type': 'momentum', 'params': {'lookback_window': 12}},
        {'name': 'ml_ridge', 'type': 'ml_strategy', 'params': {'model_name': 'ridge_model', 'features': feature_columns[:10]}},
        {'name': 'ml_rf', 'type': 'ml_strategy', 'params': {'model_name': 'random_forest', 'features': feature_columns[:10]}}
    ]

    # Create strategies
    ml4t.create_strategies(strategies_config)

    # Run backtests
    ml4t.run_backtests()

    # Generate report
    summary = ml4t.generate_report()

    return ml4t, summary

if __name__ == "__main__":
    # Run demonstration
    framework, results = run_ml4t_demo()
    print("\nML4T Framework demonstration completed successfully!")
    print("Access the framework object to explore individual components and results.")