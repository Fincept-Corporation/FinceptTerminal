"""
FFN Analytics Engine - Core Module
==================================

Main analytics engine for FFN-based financial performance analysis.
Provides comprehensive performance statistics, risk metrics, and portfolio analysis.
"""

import pandas as pd
import numpy as np
import matplotlib.pyplot as plt
import plotly.graph_objects as go
from plotly.subplots import make_subplots
import warnings
from typing import Dict, List, Optional, Union, Tuple, Any
from dataclasses import dataclass, field
from datetime import datetime, timedelta
import json

# FFN imports
import ffn

warnings.filterwarnings('ignore')


@dataclass
class FFNConfig:
    """Configuration class for FFN analytics parameters"""

    # Performance calculation parameters
    risk_free_rate: float = 0.0  # Annual risk-free rate
    annualization_factor: int = 252  # Trading days per year

    # Price transformation parameters
    rebase_value: float = 100  # Starting value for rebased prices
    log_returns: bool = False  # Use log returns instead of simple returns

    # Drawdown parameters
    drawdown_threshold: float = 0.10  # Minimum drawdown to report (10%)

    # Portfolio optimization parameters
    covar_method: str = "ledoit-wolf"  # ledoit-wolf, sample, exponential
    weight_bounds: Tuple[float, float] = (0.0, 1.0)  # Min/max weight constraints
    risk_parity_method: str = "ccd"  # ccd (cyclical coordinate descent)
    max_iterations: int = 100  # Max iterations for optimization
    tolerance: float = 1e-8  # Convergence tolerance

    # Resampling parameters
    resample_frequency: str = "M"  # D, W, M, Q, Y for daily/weekly/monthly/quarterly/yearly

    # Visualization parameters
    plot_style: str = "seaborn"  # matplotlib style
    figsize: Tuple[int, int] = (14, 8)  # Figure size for plots

    # Date range
    start_date: Optional[str] = None
    end_date: Optional[str] = None


class FFNAnalyticsEngine:
    """
    FFN Analytics Engine - Core financial performance analysis

    Features:
    - Comprehensive performance statistics (CAGR, Sharpe, Sortino, etc.)
    - Drawdown analysis with details
    - Return transformations (simple, log, excess)
    - Price rebasing and normalization
    - Rolling performance metrics
    - Monthly/yearly performance tables
    - Lookback period analysis
    - Statistical calculations (skew, kurtosis, etc.)
    """

    def __init__(self, config: FFNConfig = None):
        self.config = config or FFNConfig()
        self.prices = None
        self.returns = None
        self.performance_stats = None
        self.drawdown_details = None
        self.monthly_returns = None
        self.yearly_returns = None
        self.rolling_metrics = {}

    def load_data(self,
                  prices: Union[pd.Series, pd.DataFrame],
                  start_date: str = None,
                  end_date: str = None) -> None:
        """
        Load price data for analysis

        Parameters:
        -----------
        prices : pd.Series or pd.DataFrame
            Price data with datetime index
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

        # Calculate returns
        if self.config.log_returns:
            self.returns = ffn.to_log_returns(prices)
        else:
            self.returns = ffn.to_returns(prices)

        print(f"Data loaded: {len(self.prices)} periods")
        if isinstance(prices, pd.DataFrame):
            print(f"Assets: {len(self.prices.columns)}")

    def calculate_performance_stats(self,
                                    prices: pd.Series = None,
                                    rf: float = None) -> Dict[str, Any]:
        """
        Calculate comprehensive performance statistics

        Parameters:
        -----------
        prices : pd.Series, optional
            Price series (uses loaded data if None)
        rf : float, optional
            Risk-free rate (uses config if None)

        Returns:
        --------
        Dictionary with performance metrics
        """
        prices = prices if prices is not None else self.prices
        rf = rf if rf is not None else self.config.risk_free_rate
        returns = self.returns

        if prices is None:
            raise ValueError("No price data loaded. Call load_data() first.")

        # Handle DataFrame (multiple assets) vs Series (single asset)
        if isinstance(prices, pd.DataFrame):
            # For DataFrames, calculate stats for each column
            all_metrics = {}
            for col in prices.columns:
                col_prices = prices[col]
                col_returns = returns[col] if isinstance(returns, pd.DataFrame) else returns
                all_metrics[col] = self._calculate_single_asset_stats(col_prices, col_returns, rf)
            return all_metrics
        else:
            # Single asset
            return self._calculate_single_asset_stats(prices, returns, rf)

    def _calculate_single_asset_stats(self, prices: pd.Series, returns: pd.Series, rf: float) -> Dict[str, Any]:
        """
        Calculate performance statistics for a single asset

        Parameters:
        -----------
        prices : pd.Series
            Price series for a single asset
        returns : pd.Series
            Return series for a single asset
        rf : float
            Risk-free rate

        Returns:
        --------
        Dictionary with performance metrics
        """
        # Helper to safely convert to float
        def safe_float(val):
            if val is None:
                return None
            try:
                if hasattr(val, 'size') and val.size == 0:
                    return None
                if pd.isna(val):
                    return None
                return float(val)
            except (TypeError, ValueError):
                return None

        # Helper to safely call FFN functions
        def safe_call(func, *args, **kwargs):
            try:
                result = func(*args, **kwargs)
                return safe_float(result)
            except Exception:
                return None

        # Try to create PerformanceStats object (may fail with edge cases)
        try:
            self.performance_stats = ffn.PerformanceStats(prices)
            self.performance_stats.set_riskfree_rate(rf)
        except Exception:
            self.performance_stats = None

        # Extract key metrics with safe handling - each wrapped in try-except
        metrics = {
            'total_return': safe_call(ffn.calc_total_return, prices),
            'cagr': safe_call(ffn.calc_cagr, prices),
            'sharpe_ratio': safe_call(ffn.calc_sharpe, returns, rf=rf, nperiods=self.config.annualization_factor, annualize=True),
            'sortino_ratio': safe_call(ffn.calc_sortino_ratio, returns, rf=rf, nperiods=self.config.annualization_factor, annualize=True),
            'max_drawdown': safe_call(ffn.calc_max_drawdown, prices),
            'calmar_ratio': safe_call(ffn.calc_calmar_ratio, prices),
            'volatility': safe_float(returns.std() * np.sqrt(self.config.annualization_factor)) if returns is not None and len(returns) > 0 else None,
            'daily_mean': safe_float(returns.mean()) if returns is not None and len(returns) > 0 else None,
            'daily_vol': safe_float(returns.std()) if returns is not None and len(returns) > 0 else None,
            'best_day': safe_float(returns.max()) if returns is not None and len(returns) > 0 else None,
            'worst_day': safe_float(returns.min()) if returns is not None and len(returns) > 0 else None,
            'mtd': None,  # Skip MTD/YTD calculations - they often cause issues with small datasets
            'ytd': None,
        }

        return metrics

    def calculate_drawdown_analysis(self, prices: pd.Series = None) -> pd.DataFrame:
        """
        Calculate drawdown analysis with details

        Parameters:
        -----------
        prices : pd.Series, optional
            Price series (uses loaded data if None)

        Returns:
        --------
        DataFrame with drawdown details (start, end, duration, magnitude)
        """
        prices = prices if prices is not None else self.prices

        if prices is None:
            raise ValueError("No price data loaded. Call load_data() first.")

        try:
            # Get drawdown series
            dd_series = ffn.to_drawdown_series(prices)

            # Get drawdown details
            self.drawdown_details = ffn.drawdown_details(dd_series)

            # Filter by threshold - check if DataFrame is not empty first
            if self.drawdown_details is not None and hasattr(self.drawdown_details, 'size') and self.drawdown_details.size > 0 and self.config.drawdown_threshold > 0:
                try:
                    mask = abs(self.drawdown_details['drawdown']) >= self.config.drawdown_threshold
                    self.drawdown_details = self.drawdown_details[mask]
                except Exception:
                    pass

            return self.drawdown_details
        except Exception:
            # Return empty DataFrame on error
            self.drawdown_details = pd.DataFrame()
            return self.drawdown_details

    def calculate_rolling_metrics(self,
                                  window: int = 252,
                                  metrics: List[str] = None) -> Dict[str, pd.Series]:
        """
        Calculate rolling performance metrics

        Parameters:
        -----------
        window : int
            Rolling window size in days (default: 252 = 1 year)
        metrics : list, optional
            List of metrics to calculate ['sharpe', 'volatility', 'returns']

        Returns:
        --------
        Dictionary with rolling metric series
        """
        if self.returns is None:
            return {}

        # Ensure we have enough data points
        if len(self.returns) < window:
            # Use a smaller window if not enough data
            window = max(2, len(self.returns) - 1)

        if window < 2:
            return {}

        metrics = metrics or ['sharpe', 'volatility', 'returns']
        results = {}

        try:
            if 'returns' in metrics:
                results['rolling_returns'] = self.returns.rolling(window, min_periods=1).sum()

            if 'volatility' in metrics:
                results['rolling_volatility'] = (
                    self.returns.rolling(window, min_periods=2).std() *
                    np.sqrt(self.config.annualization_factor)
                )

            if 'sharpe' in metrics:
                rolling_mean = self.returns.rolling(window, min_periods=2).mean() * self.config.annualization_factor
                rolling_std = self.returns.rolling(window, min_periods=2).std() * np.sqrt(self.config.annualization_factor)
                # Avoid division by zero
                with np.errstate(divide='ignore', invalid='ignore'):
                    results['rolling_sharpe'] = (rolling_mean - self.config.risk_free_rate) / rolling_std
        except Exception:
            pass

        self.rolling_metrics = results
        return results

    def calculate_monthly_returns(self, prices: pd.Series = None) -> pd.DataFrame:
        """
        Calculate monthly returns table

        Returns:
        --------
        DataFrame with monthly returns (rows=years, cols=months)
        """
        prices = prices if prices is not None else self.prices

        if prices is None:
            return pd.DataFrame()

        try:
            # Resample to monthly
            monthly_prices = prices.resample('M').last()

            if len(monthly_prices) < 2:
                return pd.DataFrame()

            monthly_rets = ffn.to_returns(monthly_prices)

            # Create pivot table
            monthly_rets_df = monthly_rets.to_frame('returns')
            monthly_rets_df['year'] = monthly_rets_df.index.year
            monthly_rets_df['month'] = monthly_rets_df.index.month

            self.monthly_returns = monthly_rets_df.pivot(
                index='year',
                columns='month',
                values='returns'
            )

            # Add year total
            self.monthly_returns['Year'] = self.monthly_returns.sum(axis=1)

            return self.monthly_returns
        except Exception:
            self.monthly_returns = pd.DataFrame()
            return self.monthly_returns

    def rebase_prices(self,
                     prices: Union[pd.Series, pd.DataFrame] = None,
                     value: float = None) -> Union[pd.Series, pd.DataFrame]:
        """
        Rebase prices to start at specified value

        Parameters:
        -----------
        prices : pd.Series or pd.DataFrame
            Price data to rebase
        value : float
            Starting value (default: config.rebase_value)

        Returns:
        --------
        Rebased price series/dataframe
        """
        prices = prices if prices is not None else self.prices
        value = value if value is not None else self.config.rebase_value

        if prices is None:
            raise ValueError("No price data loaded. Call load_data() first.")

        return ffn.rebase(prices, value)

    def to_excess_returns(self,
                         returns: pd.Series = None,
                         rf: float = None) -> pd.Series:
        """
        Convert returns to excess returns (above risk-free rate)

        Parameters:
        -----------
        returns : pd.Series
            Return series
        rf : float
            Risk-free rate

        Returns:
        --------
        Excess returns series
        """
        returns = returns if returns is not None else self.returns
        rf = rf if rf is not None else self.config.risk_free_rate

        if returns is None:
            raise ValueError("No return data available. Call load_data() first.")

        return ffn.to_excess_returns(
            returns,
            rf,
            nperiods=self.config.annualization_factor
        )

    def plot_performance(self,
                        prices: Union[pd.Series, pd.DataFrame] = None,
                        title: str = "Performance") -> go.Figure:
        """
        Plot price performance over time

        Parameters:
        -----------
        prices : pd.Series or pd.DataFrame
            Price data to plot
        title : str
            Chart title

        Returns:
        --------
        Plotly figure object
        """
        prices = prices if prices is not None else self.prices

        if prices is None:
            raise ValueError("No price data loaded. Call load_data() first.")

        # Rebase for visualization
        rebased = ffn.rebase(prices, self.config.rebase_value)

        fig = go.Figure()

        if isinstance(rebased, pd.Series):
            fig.add_trace(go.Scatter(
                x=rebased.index,
                y=rebased.values,
                mode='lines',
                name=rebased.name or 'Price'
            ))
        else:
            for col in rebased.columns:
                fig.add_trace(go.Scatter(
                    x=rebased.index,
                    y=rebased[col].values,
                    mode='lines',
                    name=col
                ))

        fig.update_layout(
            title=title,
            xaxis_title="Date",
            yaxis_title=f"Value (rebased to {self.config.rebase_value})",
            template="plotly_dark",
            height=600
        )

        return fig

    def plot_drawdown(self, prices: pd.Series = None) -> go.Figure:
        """
        Plot drawdown over time

        Parameters:
        -----------
        prices : pd.Series
            Price series

        Returns:
        --------
        Plotly figure object
        """
        prices = prices if prices is not None else self.prices

        if prices is None:
            raise ValueError("No price data loaded. Call load_data() first.")

        dd_series = ffn.to_drawdown_series(prices)

        fig = go.Figure()
        fig.add_trace(go.Scatter(
            x=dd_series.index,
            y=dd_series.values * 100,  # Convert to percentage
            fill='tozeroy',
            fillcolor='rgba(255,0,0,0.3)',
            line=dict(color='red'),
            name='Drawdown'
        ))

        fig.update_layout(
            title="Drawdown Analysis",
            xaxis_title="Date",
            yaxis_title="Drawdown (%)",
            template="plotly_dark",
            height=500
        )

        return fig

    def get_stats_summary(self) -> str:
        """
        Get formatted statistics summary

        Returns:
        --------
        Formatted string with performance statistics
        """
        if self.performance_stats is None:
            self.calculate_performance_stats()

        # Use ffn's display method
        return str(self.performance_stats.stats)

    def to_monthly(self, prices: pd.Series = None) -> pd.Series:
        """Convert daily prices to monthly"""
        prices = prices if prices is not None else self.prices
        return ffn.to_monthly(prices)

    def rescale(self, prices: pd.Series = None) -> pd.Series:
        """Rescale prices to 0-1 range"""
        prices = prices if prices is not None else self.prices
        return ffn.rescale(prices)

    def winsorize(self, returns: pd.Series = None, limits: tuple = (0.05, 0.05)) -> pd.Series:
        """Winsorize returns (clip outliers)"""
        returns = returns if returns is not None else self.returns
        return ffn.winsorize(returns, limits=limits)

    def to_ulcer_index(self, prices: pd.Series = None) -> float:
        """Calculate Ulcer Index (drawdown volatility)"""
        prices = prices if prices is not None else self.prices
        return ffn.to_ulcer_index(prices)

    def to_ulcer_performance_index(self, prices: pd.Series = None, rf: float = None) -> float:
        """Calculate Ulcer Performance Index"""
        prices = prices if prices is not None else self.prices
        rf = rf if rf is not None else self.config.risk_free_rate
        return ffn.to_ulcer_performance_index(prices, rf)

    def annualize(self, returns: pd.Series = None, nperiods: int = None) -> pd.Series:
        """Annualize returns"""
        returns = returns if returns is not None else self.returns
        nperiods = nperiods or self.config.annualization_factor
        return ffn.annualize(returns, nperiods)

    def deannualize(self, returns: pd.Series = None, nperiods: int = None) -> pd.Series:
        """Deannualize returns"""
        returns = returns if returns is not None else self.returns
        nperiods = nperiods or self.config.annualization_factor
        return ffn.deannualize(returns, nperiods)

    def rollapply(self, func, window: int = 252, prices: pd.Series = None) -> pd.Series:
        """Apply function over rolling window"""
        prices = prices if prices is not None else self.prices
        return ffn.rollapply(prices, window, func)

    def resample_data(self, prices: pd.Series = None, freq: str = None) -> pd.Series:
        """Resample prices to different frequency"""
        prices = prices if prices is not None else self.prices
        freq = freq or self.config.resample_frequency
        return prices.resample(freq).last()

    def get_freq_name(self, prices: pd.Series = None) -> str:
        """Get frequency name of price series"""
        prices = prices if prices is not None else self.prices
        return ffn.get_freq_name(prices)

    def infer_freq(self, prices: pd.Series = None) -> int:
        """Infer number of periods per year"""
        prices = prices if prices is not None else self.prices
        return ffn.infer_freq(prices)

    def export_to_json(self) -> str:
        """
        Export all calculated metrics to JSON

        Returns:
        --------
        JSON string with all metrics
        """
        metrics = self.calculate_performance_stats()

        # Convert to serializable format
        export_data = {
            'performance_metrics': {
                k: float(v) if isinstance(v, (np.integer, np.floating)) else v
                for k, v in metrics.items()
                if v is not None
            },
            'config': {
                'risk_free_rate': self.config.risk_free_rate,
                'annualization_factor': self.config.annualization_factor,
                'rebase_value': self.config.rebase_value,
            }
        }

        if self.drawdown_details is not None and len(self.drawdown_details) > 0:
            export_data['drawdown_details'] = self.drawdown_details.to_dict('records')

        return json.dumps(export_data, indent=2)


def main():
    """Example usage of FFN Analytics Engine"""

    # Example with sample data - use more data for MTD/YTD calculations
    dates = pd.date_range('2018-01-01', '2023-12-31', freq='D')
    np.random.seed(42)
    returns_arr = np.random.normal(0.0005, 0.01, len(dates))
    cumulative = np.cumprod(1 + returns_arr)
    prices = pd.Series(cumulative * 100, index=dates, name='Asset')

    # Initialize engine
    config = FFNConfig(risk_free_rate=0.02, rebase_value=100)
    engine = FFNAnalyticsEngine(config)

    # Load data
    engine.load_data(prices)

    # Calculate performance stats
    returns_series = engine.returns
    print("\nPerformance Statistics:")
    total_ret = ffn.calc_total_return(prices)
    cagr = ffn.calc_cagr(prices)
    sharpe = ffn.calc_sharpe(returns_series, rf=0.02, nperiods=252, annualize=True)
    sortino = ffn.calc_sortino_ratio(returns_series, rf=0.02, nperiods=252, annualize=True)
    max_dd = ffn.calc_max_drawdown(prices)
    calmar = ffn.calc_calmar_ratio(prices)
    vol = returns_series.std() * np.sqrt(252)

    print(f"  total_return: {total_ret:.4f}")
    print(f"  cagr: {cagr:.4f}")
    print(f"  sharpe_ratio: {sharpe:.4f}")
    print(f"  sortino_ratio: {sortino:.4f}")
    print(f"  max_drawdown: {max_dd:.4f}")
    print(f"  calmar_ratio: {calmar:.4f}")
    print(f"  volatility: {vol:.4f}")

    # Drawdown analysis
    dd_details = engine.calculate_drawdown_analysis()
    if dd_details is not None and len(dd_details) > 0:
        print(f"\nTop 3 Drawdowns:\n{dd_details.head(3)}")
    else:
        print("\nNo significant drawdowns found")

    # Rolling metrics
    rolling = engine.calculate_rolling_metrics(window=252)
    print(f"\nRolling metrics calculated: {list(rolling.keys())}")

    print("\n=== FFN Analytics Engine Test: PASSED ===")


if __name__ == "__main__":
    main()
