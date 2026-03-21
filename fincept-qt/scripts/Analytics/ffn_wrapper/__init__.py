"""
FFN (Financial Functions for Python) Wrapper Module
===================================================

Advanced financial performance analytics and portfolio statistics using the ffn library.
Provides comprehensive performance analysis, risk metrics, portfolio optimization,
and visualization capabilities.

===== DATA SOURCES REQUIRED =====
INPUT:
  - Pandas DataFrame or Series with price/return data (datetime index)
  - Multiple asset price series for portfolio analysis
  - Benchmark data for relative performance analysis
  - Risk-free rate for Sharpe/Sortino calculations

OUTPUT:
  - Performance statistics (returns, volatility, Sharpe ratio, etc.)
  - Drawdown analysis and maximum drawdown
  - Rolling performance metrics
  - Portfolio weights (ERC, minimum variance, inverse volatility)
  - Correlation matrices and heatmaps
  - Performance visualizations
  - Group statistics for multi-asset comparison

PARAMETERS:
  - risk_free_rate: Risk-free rate for Sharpe/Sortino calculations (default: 0.0)
  - annualization_factor: Days per year for annualization (default: 252)
  - rebase_value: Starting value for price rebasing (default: 100)
  - weight_bounds: Min/max weight constraints (default: (0.0, 1.0))
  - covar_method: Covariance estimation method (default: 'ledoit-wolf')
"""

from .ffn_analytics import FFNAnalyticsEngine, FFNConfig
from .ffn_performance import FFNPerformanceAnalyzer
from .ffn_portfolio import FFNPortfolioOptimizer

__all__ = [
    'FFNAnalyticsEngine',
    'FFNConfig',
    'FFNPerformanceAnalyzer',
    'FFNPortfolioOptimizer',
]
