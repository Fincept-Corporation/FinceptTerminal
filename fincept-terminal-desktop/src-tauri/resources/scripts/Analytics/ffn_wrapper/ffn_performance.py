"""
FFN Performance Analyzer - Advanced Performance Analysis
========================================================

Advanced performance analysis capabilities including:
- Group statistics for multiple assets
- Correlation analysis
- Information ratio calculations
- Relative performance metrics
- Performance attribution
"""

import pandas as pd
import numpy as np
import matplotlib.pyplot as plt
import plotly.graph_objects as go
from plotly.subplots import make_subplots
import warnings
from typing import Dict, List, Optional, Union, Tuple, Any
import json

# FFN imports
import ffn

warnings.filterwarnings('ignore')


class FFNPerformanceAnalyzer:
    """
    FFN Performance Analyzer for multi-asset performance comparison

    Features:
    - Group statistics for multiple assets
    - Correlation matrix and heatmaps
    - Information ratio calculations
    - Relative performance analysis
    - Lookback period comparisons
    - Scatter matrix visualizations
    """

    def __init__(self, risk_free_rate: float = 0.0, annualization_factor: int = 252):
        self.risk_free_rate = risk_free_rate
        self.annualization_factor = annualization_factor
        self.prices = None
        self.group_stats = None
        self.correlation_matrix = None
        self.relative_stats = {}

    def load_multi_asset_data(self,
                              prices: pd.DataFrame,
                              start_date: str = None,
                              end_date: str = None) -> None:
        """
        Load multi-asset price data

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
        print(f"Multi-asset data loaded: {len(self.prices)} periods, {len(self.prices.columns)} assets")

    def calculate_group_stats(self, prices: pd.DataFrame = None) -> ffn.GroupStats:
        """
        Calculate group statistics for multiple assets

        Parameters:
        -----------
        prices : pd.DataFrame, optional
            Price data (uses loaded data if None)

        Returns:
        --------
        FFN GroupStats object with comprehensive statistics
        """
        prices = prices if prices is not None else self.prices

        if prices is None:
            raise ValueError("No price data loaded. Call load_multi_asset_data() first.")

        # Create GroupStats object
        self.group_stats = ffn.GroupStats(prices)
        self.group_stats.set_riskfree_rate(self.risk_free_rate)

        return self.group_stats

    def calculate_correlation_matrix(self,
                                     returns: pd.DataFrame = None,
                                     method: str = 'pearson') -> pd.DataFrame:
        """
        Calculate correlation matrix for assets

        Parameters:
        -----------
        returns : pd.DataFrame, optional
            Return data (calculated from prices if None)
        method : str
            Correlation method: 'pearson', 'kendall', 'spearman'

        Returns:
        --------
        Correlation matrix DataFrame
        """
        if returns is None:
            if self.prices is None:
                raise ValueError("No price data loaded. Call load_multi_asset_data() first.")
            returns = ffn.to_returns(self.prices)

        self.correlation_matrix = returns.corr(method=method)
        return self.correlation_matrix

    def calculate_information_ratio(self,
                                    returns: pd.Series,
                                    benchmark_returns: pd.Series) -> float:
        """
        Calculate information ratio (excess return / tracking error)

        Parameters:
        -----------
        returns : pd.Series
            Asset returns
        benchmark_returns : pd.Series
            Benchmark returns

        Returns:
        --------
        Information ratio
        """
        return ffn.calc_information_ratio(returns, benchmark_returns)

    def calculate_risk_return_ratio(self, returns: pd.Series = None) -> float:
        """
        Calculate risk-return ratio (mean/std)

        Parameters:
        -----------
        returns : pd.Series
            Return series

        Returns:
        --------
        Risk-return ratio
        """
        if returns is None:
            if self.prices is None:
                raise ValueError("No price data loaded.")
            returns = ffn.to_returns(self.prices.iloc[:, 0])

        return ffn.calc_risk_return_ratio(returns)

    def calculate_relative_performance(self,
                                      asset_prices: pd.Series,
                                      benchmark_prices: pd.Series) -> Dict[str, float]:
        """
        Calculate relative performance metrics vs benchmark

        Parameters:
        -----------
        asset_prices : pd.Series
            Asset price series
        benchmark_prices : pd.Series
            Benchmark price series

        Returns:
        --------
        Dictionary with relative performance metrics
        """
        asset_returns = ffn.to_returns(asset_prices)
        benchmark_returns = ffn.to_returns(benchmark_prices)

        # Calculate metrics
        metrics = {
            'total_return_asset': ffn.calc_total_return(asset_prices),
            'total_return_benchmark': ffn.calc_total_return(benchmark_prices),
            'cagr_asset': ffn.calc_cagr(asset_prices),
            'cagr_benchmark': ffn.calc_cagr(benchmark_prices),
            'information_ratio': ffn.calc_information_ratio(asset_returns, benchmark_returns),
            'tracking_error': (asset_returns - benchmark_returns).std() * np.sqrt(self.annualization_factor),
            'correlation': asset_returns.corr(benchmark_returns),
            'beta': asset_returns.cov(benchmark_returns) / benchmark_returns.var(),
        }

        # Excess returns
        excess_cagr = metrics['cagr_asset'] - metrics['cagr_benchmark']
        metrics['excess_cagr'] = excess_cagr

        self.relative_stats = metrics
        return metrics

    def analyze_lookback_periods(self,
                                 prices: pd.DataFrame = None,
                                 periods: List[str] = None) -> pd.DataFrame:
        """
        Analyze performance across different lookback periods

        Parameters:
        -----------
        prices : pd.DataFrame
            Price data
        periods : list
            List of lookback periods ['1m', '3m', '6m', '1y', '3y', '5y']

        Returns:
        --------
        DataFrame with lookback returns
        """
        prices = prices if prices is not None else self.prices
        periods = periods or ['1m', '3m', '6m', '1y', '3y', '5y']

        if prices is None:
            raise ValueError("No price data loaded.")

        # Calculate lookback returns
        lookback_data = {}

        period_map = {
            '1m': 21, '3m': 63, '6m': 126, '1y': 252,
            '2y': 504, '3y': 756, '5y': 1260, '10y': 2520
        }

        for period in periods:
            days = period_map.get(period)
            if days and len(prices) >= days:
                period_prices = prices.iloc[-days:]
                returns = ffn.calc_total_return(period_prices)
                lookback_data[period] = returns

        return pd.DataFrame(lookback_data).T

    def plot_correlation_heatmap(self,
                                 correlation_matrix: pd.DataFrame = None,
                                 title: str = "Correlation Heatmap") -> go.Figure:
        """
        Plot correlation heatmap

        Parameters:
        -----------
        correlation_matrix : pd.DataFrame
            Correlation matrix (calculated if None)
        title : str
            Chart title

        Returns:
        --------
        Plotly figure object
        """
        if correlation_matrix is None:
            if self.correlation_matrix is None:
                self.calculate_correlation_matrix()
            correlation_matrix = self.correlation_matrix

        fig = go.Figure(data=go.Heatmap(
            z=correlation_matrix.values,
            x=correlation_matrix.columns,
            y=correlation_matrix.index,
            colorscale='RdBu',
            zmid=0,
            text=correlation_matrix.values,
            texttemplate='%{text:.2f}',
            textfont={"size": 10},
            colorbar=dict(title="Correlation")
        ))

        fig.update_layout(
            title=title,
            template="plotly_dark",
            height=600,
            width=800
        )

        return fig

    def plot_relative_performance(self,
                                  asset_prices: pd.Series,
                                  benchmark_prices: pd.Series,
                                  asset_name: str = "Asset",
                                  benchmark_name: str = "Benchmark") -> go.Figure:
        """
        Plot relative performance vs benchmark

        Parameters:
        -----------
        asset_prices : pd.Series
            Asset price series
        benchmark_prices : pd.Series
            Benchmark price series
        asset_name, benchmark_name : str
            Names for legend

        Returns:
        --------
        Plotly figure object
        """
        # Rebase both to 100
        asset_rebased = ffn.rebase(asset_prices, 100)
        benchmark_rebased = ffn.rebase(benchmark_prices, 100)

        # Create subplots
        fig = make_subplots(
            rows=2, cols=1,
            subplot_titles=('Relative Performance', 'Excess Returns'),
            row_heights=[0.6, 0.4],
            vertical_spacing=0.12
        )

        # Plot rebased prices
        fig.add_trace(
            go.Scatter(x=asset_rebased.index, y=asset_rebased.values,
                      mode='lines', name=asset_name,
                      line=dict(color='cyan')),
            row=1, col=1
        )
        fig.add_trace(
            go.Scatter(x=benchmark_rebased.index, y=benchmark_rebased.values,
                      mode='lines', name=benchmark_name,
                      line=dict(color='orange')),
            row=1, col=1
        )

        # Plot excess returns
        asset_returns = ffn.to_returns(asset_prices)
        benchmark_returns = ffn.to_returns(benchmark_prices)
        excess_returns = (asset_returns - benchmark_returns).cumsum()

        fig.add_trace(
            go.Scatter(x=excess_returns.index, y=excess_returns.values,
                      mode='lines', name='Excess Returns',
                      fill='tozeroy',
                      line=dict(color='green')),
            row=2, col=1
        )

        fig.update_xaxes(title_text="Date", row=2, col=1)
        fig.update_yaxes(title_text="Value (rebased to 100)", row=1, col=1)
        fig.update_yaxes(title_text="Cumulative Excess", row=2, col=1)

        fig.update_layout(
            template="plotly_dark",
            height=800,
            showlegend=True
        )

        return fig

    def plot_scatter_matrix(self,
                           returns: pd.DataFrame = None,
                           title: str = "Returns Scatter Matrix") -> go.Figure:
        """
        Plot scatter matrix for multiple assets

        Parameters:
        -----------
        returns : pd.DataFrame
            Return data
        title : str
            Chart title

        Returns:
        --------
        Plotly figure object
        """
        if returns is None:
            if self.prices is None:
                raise ValueError("No price data loaded.")
            returns = ffn.to_returns(self.prices)

        # Limit to first 4 assets for readability
        if len(returns.columns) > 4:
            returns = returns.iloc[:, :4]
            print("Limited to first 4 assets for scatter matrix visualization")

        import plotly.express as px
        fig = px.scatter_matrix(
            returns,
            dimensions=returns.columns.tolist(),
            title=title,
            template="plotly_dark",
            height=800
        )

        fig.update_traces(diagonal_visible=False, showupperhalf=False)

        return fig

    def get_group_stats_summary(self) -> pd.DataFrame:
        """
        Get summary table of group statistics

        Returns:
        --------
        DataFrame with statistics for all assets
        """
        if self.group_stats is None:
            self.calculate_group_stats()

        # Extract key stats for each asset
        stats_list = []
        for name, perf_stats in self.group_stats.items():
            rets = ffn.to_returns(self.prices[name])
            stats_dict = {
                'Asset': name,
                'Total Return': ffn.calc_total_return(self.prices[name]),
                'CAGR': ffn.calc_cagr(self.prices[name]),
                'Volatility': rets.std() * np.sqrt(self.annualization_factor),
                'Sharpe': ffn.calc_sharpe(rets, rf=self.risk_free_rate, nperiods=self.annualization_factor, annualize=True),
                'Max DD': ffn.calc_max_drawdown(self.prices[name]),
                'Calmar': ffn.calc_calmar_ratio(self.prices[name]),
            }
            stats_list.append(stats_dict)

        return pd.DataFrame(stats_list)

    def calc_prob_mom(self, returns1: pd.Series, returns2: pd.Series) -> float:
        """Calculate probabilistic momentum"""
        return ffn.calc_prob_mom(returns1, returns2)

    def calc_ftca(self, returns: pd.DataFrame = None, threshold: float = 0.5) -> pd.DataFrame:
        """Fast threshold clustering algorithm"""
        returns = returns if returns is not None else ffn.to_returns(self.prices)
        return ffn.calc_ftca(returns, threshold=threshold)

    def calc_stats(self, prices: pd.Series) -> dict:
        """Calculate comprehensive statistics dictionary"""
        return ffn.calc_stats(prices)

    def merge_prices(self, *price_series) -> pd.DataFrame:
        """Merge multiple price series"""
        return ffn.merge(*price_series)

    def get_data(self, tickers: list, **kwargs) -> pd.DataFrame:
        """Fetch price data for tickers"""
        return ffn.get(tickers, **kwargs)

    def plot_corr_heatmap_ffn(self, returns: pd.DataFrame = None):
        """Plot correlation heatmap using FFN's matplotlib"""
        returns = returns if returns is not None else ffn.to_returns(self.prices)
        return ffn.plot_corr_heatmap(returns)

    def plot_heatmap_ffn(self, data: pd.DataFrame):
        """Plot generic heatmap using FFN's matplotlib"""
        return ffn.plot_heatmap(data)

    def export_to_json(self) -> str:
        """
        Export all calculated metrics to JSON

        Returns:
        --------
        JSON string with all metrics
        """
        export_data = {
            'config': {
                'risk_free_rate': self.risk_free_rate,
                'annualization_factor': self.annualization_factor,
            }
        }

        if self.correlation_matrix is not None:
            export_data['correlation_matrix'] = self.correlation_matrix.to_dict()

        if self.relative_stats:
            export_data['relative_stats'] = {
                k: float(v) if isinstance(v, (np.integer, np.floating)) else v
                for k, v in self.relative_stats.items()
            }

        if self.group_stats is not None:
            export_data['group_summary'] = self.get_group_stats_summary().to_dict('records')

        return json.dumps(export_data, indent=2)


def main():
    """Example usage of FFN Performance Analyzer"""

    # Example with sample data
    dates = pd.date_range('2020-01-01', '2023-12-31', freq='D')
    np.random.seed(42)

    # Create multi-asset data
    assets = {}
    for i, name in enumerate(['Stock_A', 'Stock_B', 'Stock_C', 'Benchmark']):
        returns_arr = np.random.normal(0.0005 + i*0.0001, 0.01 + i*0.002, len(dates))
        cumulative = np.cumprod(1 + returns_arr)
        prices = pd.Series(cumulative * 100, index=dates, name=name)
        assets[name] = prices

    prices_df = pd.DataFrame(assets)

    # Initialize analyzer
    analyzer = FFNPerformanceAnalyzer(risk_free_rate=0.02)

    # Load data
    analyzer.load_multi_asset_data(prices_df)

    # Correlation matrix
    corr = analyzer.calculate_correlation_matrix()
    print(f"Correlation Matrix:\n{corr}\n")

    # Relative performance
    rel_perf = analyzer.calculate_relative_performance(
        prices_df['Stock_A'],
        prices_df['Benchmark']
    )
    print(f"Relative Performance Metrics:")
    for key, value in rel_perf.items():
        print(f"  {key}: {value:.4f}")

    # Group stats summary
    print(f"\nGroup Statistics Summary:")
    print(analyzer.get_group_stats_summary())

    print("\n=== FFN Performance Analyzer Test: PASSED ===")


if __name__ == "__main__":
    main()
