"""
FFN Portfolio Optimizer - Portfolio Construction and Optimization
================================================================

Portfolio optimization capabilities including:
- Equal Risk Contribution (ERC) weights
- Inverse volatility weights
- Mean-variance optimization
- Portfolio clustering
- Weight constraints and limits
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


class FFNPortfolioOptimizer:
    """
    FFN Portfolio Optimizer for portfolio construction

    Features:
    - Equal Risk Contribution (ERC) weights
    - Inverse volatility weights
    - Mean-variance optimization
    - Portfolio clustering analysis
    - Weight constraints and bounds
    - Portfolio rebalancing
    - Performance attribution
    """

    def __init__(self,
                 weight_bounds: Tuple[float, float] = (0.0, 1.0),
                 covar_method: str = 'ledoit-wolf',
                 risk_parity_method: str = 'ccd',
                 max_iterations: int = 100,
                 tolerance: float = 1e-8):
        """
        Initialize portfolio optimizer

        Parameters:
        -----------
        weight_bounds : tuple
            (min_weight, max_weight) for asset weights
        covar_method : str
            Covariance estimation method: 'ledoit-wolf', 'sample', 'exponential'
        risk_parity_method : str
            Risk parity method: 'ccd' (cyclical coordinate descent)
        max_iterations : int
            Maximum iterations for optimization
        tolerance : float
            Convergence tolerance
        """
        self.weight_bounds = weight_bounds
        self.covar_method = covar_method
        self.risk_parity_method = risk_parity_method
        self.max_iterations = max_iterations
        self.tolerance = tolerance

        self.returns = None
        self.prices = None
        self.weights = None
        self.portfolio_returns = None
        self.portfolio_prices = None

    def load_data(self,
                  prices: pd.DataFrame,
                  start_date: str = None,
                  end_date: str = None) -> None:
        """
        Load price data for optimization

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
        self.returns = ffn.to_returns(prices).dropna()  # Remove NaN from first row

        print(f"Portfolio data loaded: {len(self.prices)} periods, {len(self.prices.columns)} assets")

    def calculate_erc_weights(self,
                             returns: pd.DataFrame = None,
                             initial_weights: np.ndarray = None,
                             risk_weights: np.ndarray = None) -> pd.Series:
        """
        Calculate Equal Risk Contribution (ERC) portfolio weights

        Parameters:
        -----------
        returns : pd.DataFrame
            Return data (uses loaded data if None)
        initial_weights : np.ndarray
            Initial guess for weights
        risk_weights : np.ndarray
            Risk budget for each asset

        Returns:
        --------
        Series with optimal weights
        """
        returns = returns if returns is not None else self.returns

        if returns is None:
            raise ValueError("No return data loaded. Call load_data() first.")

        # Calculate ERC weights using ffn
        weights = ffn.calc_erc_weights(
            returns,
            initial_weights=initial_weights,
            risk_weights=risk_weights,
            covar_method=self.covar_method,
            risk_parity_method=self.risk_parity_method,
            maximum_iterations=self.max_iterations,
            tolerance=self.tolerance
        )

        # Apply weight bounds
        weights = self.apply_weight_bounds(weights)

        self.weights = pd.Series(weights, index=returns.columns, name='ERC Weights')
        return self.weights

    def calculate_inv_vol_weights(self, returns: pd.DataFrame = None) -> pd.Series:
        """
        Calculate inverse volatility weights (1/vol weighting)

        Parameters:
        -----------
        returns : pd.DataFrame
            Return data (uses loaded data if None)

        Returns:
        --------
        Series with inverse volatility weights
        """
        returns = returns if returns is not None else self.returns

        if returns is None:
            raise ValueError("No return data loaded. Call load_data() first.")

        # Calculate inverse volatility weights using ffn
        weights = ffn.calc_inv_vol_weights(returns)

        # Apply weight bounds
        weights = self.apply_weight_bounds(weights)

        self.weights = pd.Series(weights, index=returns.columns, name='Inv Vol Weights')
        return self.weights

    def calculate_mean_var_weights(self,
                                   returns: pd.DataFrame = None,
                                   rf: float = 0.0,
                                   weight_bounds: Tuple[float, float] = None) -> pd.Series:
        """
        Calculate mean-variance optimal weights (maximum Sharpe ratio)

        Parameters:
        -----------
        returns : pd.DataFrame
            Return data (uses loaded data if None)
        rf : float
            Risk-free rate
        weight_bounds : tuple
            Weight bounds (uses instance bounds if None)

        Returns:
        --------
        Series with optimal weights
        """
        returns = returns if returns is not None else self.returns
        weight_bounds = weight_bounds or self.weight_bounds

        if returns is None:
            raise ValueError("No return data loaded. Call load_data() first.")

        # Calculate mean-variance weights using ffn
        weights = ffn.calc_mean_var_weights(
            returns,
            weight_bounds=weight_bounds,
            rf=rf,
            covar_method=self.covar_method
        )

        self.weights = pd.Series(weights, index=returns.columns, name='Mean-Var Weights')
        return self.weights

    def calculate_clusters(self,
                          returns: pd.DataFrame = None,
                          n_clusters: int = None,
                          plot: bool = False) -> np.ndarray:
        """
        Calculate asset clusters using hierarchical clustering

        Parameters:
        -----------
        returns : pd.DataFrame
            Return data
        n_clusters : int
            Number of clusters (auto-determined if None)
        plot : bool
            Whether to plot dendrogram

        Returns:
        --------
        Array of cluster labels
        """
        returns = returns if returns is not None else self.returns

        if returns is None:
            raise ValueError("No return data loaded. Call load_data() first.")

        clusters = ffn.calc_clusters(returns, n=n_clusters, plot=plot)
        return clusters

    def apply_weight_bounds(self, weights: np.ndarray) -> np.ndarray:
        """
        Apply weight bounds and normalize

        Parameters:
        -----------
        weights : np.ndarray
            Portfolio weights

        Returns:
        --------
        Bounded and normalized weights
        """
        min_w, max_w = self.weight_bounds

        # Clip weights
        weights = np.clip(weights, min_w, max_w)

        # Renormalize to sum to 1
        weights = weights / weights.sum()

        return weights

    def limit_weights(self,
                     weights: Union[pd.Series, np.ndarray],
                     limit: float = 0.1) -> np.ndarray:
        """
        Limit maximum weight for any single asset

        Parameters:
        -----------
        weights : pd.Series or np.ndarray
            Portfolio weights
        limit : float
            Maximum weight for any asset

        Returns:
        --------
        Limited and renormalized weights
        """
        if isinstance(weights, pd.Series):
            weights = weights.values

        limited_weights = ffn.limit_weights(weights, limit)
        return limited_weights

    def generate_random_weights(self, n_assets: int) -> np.ndarray:
        """Generate random portfolio weights"""
        return ffn.random_weights(n_assets)

    def resample_returns(self, returns: pd.DataFrame = None, freq: str = 'M') -> pd.DataFrame:
        """Resample returns to different frequency"""
        returns = returns if returns is not None else self.returns
        return ffn.resample_returns(returns, freq)

    def create_portfolio(self,
                        weights: Union[pd.Series, Dict] = None,
                        prices: pd.DataFrame = None) -> pd.Series:
        """
        Create portfolio returns/prices from weights

        Parameters:
        -----------
        weights : pd.Series or dict
            Portfolio weights (uses calculated weights if None)
        prices : pd.DataFrame
            Price data (uses loaded prices if None)

        Returns:
        --------
        Portfolio price series
        """
        weights = weights if weights is not None else self.weights
        prices = prices if prices is not None else self.prices

        if weights is None:
            raise ValueError("No weights available. Calculate weights first.")
        if prices is None:
            raise ValueError("No price data loaded. Call load_data() first.")

        # Convert weights to dict if Series
        if isinstance(weights, pd.Series):
            weights = weights.to_dict()

        # Calculate portfolio returns
        returns = ffn.to_returns(prices)
        self.portfolio_returns = (returns * pd.Series(weights)).sum(axis=1)

        # Calculate portfolio prices
        self.portfolio_prices = ffn.to_price_index(self.portfolio_returns, start=100)

        return self.portfolio_prices

    def calculate_portfolio_stats(self,
                                  portfolio_prices: pd.Series = None,
                                  rf: float = 0.0) -> Dict[str, float]:
        """
        Calculate portfolio performance statistics

        Parameters:
        -----------
        portfolio_prices : pd.Series
            Portfolio price series (uses created portfolio if None)
        rf : float
            Risk-free rate

        Returns:
        --------
        Dictionary with portfolio statistics
        """
        portfolio_prices = portfolio_prices if portfolio_prices is not None else self.portfolio_prices

        if portfolio_prices is None:
            raise ValueError("No portfolio created. Call create_portfolio() first.")

        portfolio_returns = ffn.to_returns(portfolio_prices)

        stats = {
            'total_return': ffn.calc_total_return(portfolio_prices),
            'cagr': ffn.calc_cagr(portfolio_prices),
            'volatility': portfolio_returns.std() * np.sqrt(252),
            'sharpe_ratio': ffn.calc_sharpe(portfolio_returns, rf=rf, nperiods=252, annualize=True),
            'sortino_ratio': ffn.calc_sortino_ratio(portfolio_returns, rf=rf, nperiods=252, annualize=True),
            'max_drawdown': ffn.calc_max_drawdown(portfolio_prices),
            'calmar_ratio': ffn.calc_calmar_ratio(portfolio_prices),
        }

        return stats

    def rebalance_portfolio(self,
                           rebalance_freq: str = 'Q',
                           weight_method: str = 'erc') -> pd.DataFrame:
        """
        Simulate portfolio with periodic rebalancing

        Parameters:
        -----------
        rebalance_freq : str
            Rebalancing frequency: 'M' (monthly), 'Q' (quarterly), 'Y' (yearly)
        weight_method : str
            Weight calculation method: 'erc', 'inv_vol', 'mean_var'

        Returns:
        --------
        DataFrame with portfolio prices and rebalancing dates
        """
        if self.prices is None:
            raise ValueError("No price data loaded. Call load_data() first.")

        # Get rebalancing dates
        rebal_dates = self.prices.resample(rebalance_freq).last().index

        portfolio_values = []
        current_value = 100

        for i in range(len(rebal_dates) - 1):
            # Get period data
            start_date = rebal_dates[i]
            end_date = rebal_dates[i + 1]

            period_prices = self.prices.loc[start_date:end_date]
            period_returns = ffn.to_returns(period_prices)

            # Calculate weights for this period
            if weight_method == 'erc':
                weights = self.calculate_erc_weights(period_returns)
            elif weight_method == 'inv_vol':
                weights = self.calculate_inv_vol_weights(period_returns)
            elif weight_method == 'mean_var':
                weights = self.calculate_mean_var_weights(period_returns)
            else:
                raise ValueError(f"Unknown weight method: {weight_method}")

            # Calculate period portfolio returns
            period_portfolio_returns = (period_returns * weights).sum(axis=1)

            # Update portfolio value
            period_portfolio_prices = ffn.to_price_index(period_portfolio_returns, start=current_value)
            portfolio_values.extend(period_portfolio_prices.values)

            current_value = period_portfolio_prices.iloc[-1]

        portfolio_series = pd.Series(
            portfolio_values,
            index=self.prices.loc[rebal_dates[0]:rebal_dates[-1]].index,
            name='Rebalanced Portfolio'
        )

        return portfolio_series

    def plot_weights(self,
                    weights: pd.Series = None,
                    title: str = "Portfolio Weights") -> go.Figure:
        """
        Plot portfolio weights as bar chart

        Parameters:
        -----------
        weights : pd.Series
            Portfolio weights (uses calculated weights if None)
        title : str
            Chart title

        Returns:
        --------
        Plotly figure object
        """
        weights = weights if weights is not None else self.weights

        if weights is None:
            raise ValueError("No weights available. Calculate weights first.")

        fig = go.Figure(data=[
            go.Bar(
                x=weights.index,
                y=weights.values * 100,  # Convert to percentage
                text=[f'{w*100:.1f}%' for w in weights.values],
                textposition='outside',
                marker_color='cyan'
            )
        ])

        fig.update_layout(
            title=title,
            xaxis_title="Asset",
            yaxis_title="Weight (%)",
            template="plotly_dark",
            height=500
        )

        return fig

    def plot_portfolio_comparison(self,
                                  weights_dict: Dict[str, pd.Series],
                                  prices: pd.DataFrame = None) -> go.Figure:
        """
        Plot comparison of different portfolio strategies

        Parameters:
        -----------
        weights_dict : dict
            Dictionary of {strategy_name: weights}
        prices : pd.DataFrame
            Price data

        Returns:
        --------
        Plotly figure object
        """
        prices = prices if prices is not None else self.prices

        if prices is None:
            raise ValueError("No price data loaded.")

        fig = go.Figure()

        for strategy_name, weights in weights_dict.items():
            # Create portfolio
            returns = ffn.to_returns(prices)
            portfolio_returns = (returns * weights).sum(axis=1)
            portfolio_prices = ffn.to_price_index(portfolio_returns, start=100)

            fig.add_trace(go.Scatter(
                x=portfolio_prices.index,
                y=portfolio_prices.values,
                mode='lines',
                name=strategy_name
            ))

        fig.update_layout(
            title="Portfolio Strategy Comparison",
            xaxis_title="Date",
            yaxis_title="Portfolio Value (rebased to 100)",
            template="plotly_dark",
            height=600
        )

        return fig

    def export_to_json(self) -> str:
        """
        Export weights and portfolio stats to JSON

        Returns:
        --------
        JSON string
        """
        export_data = {
            'config': {
                'weight_bounds': self.weight_bounds,
                'covar_method': self.covar_method,
                'risk_parity_method': self.risk_parity_method,
            }
        }

        if self.weights is not None:
            export_data['weights'] = self.weights.to_dict()

        if self.portfolio_prices is not None:
            stats = self.calculate_portfolio_stats()
            export_data['portfolio_stats'] = {
                k: float(v) if isinstance(v, (np.integer, np.floating)) else v
                for k, v in stats.items()
            }

        return json.dumps(export_data, indent=2)


def main():
    """Example usage of FFN Portfolio Optimizer"""

    # Example with sample data
    dates = pd.date_range('2020-01-01', '2023-12-31', freq='D')
    np.random.seed(42)

    # Create multi-asset data
    assets = {}
    for i, name in enumerate(['Stock_A', 'Stock_B', 'Stock_C', 'Stock_D']):
        returns_arr = np.random.normal(0.0005, 0.01 + i*0.002, len(dates))
        cumulative = np.cumprod(1 + returns_arr)
        prices = pd.Series(cumulative * 100, index=dates, name=name)
        assets[name] = prices

    prices_df = pd.DataFrame(assets)

    # Initialize optimizer
    optimizer = FFNPortfolioOptimizer(weight_bounds=(0.05, 0.50))

    # Load data
    optimizer.load_data(prices_df)

    # Calculate different weight strategies
    print("=== Equal Risk Contribution Weights ===")
    erc_weights = optimizer.calculate_erc_weights()
    print(erc_weights)

    print("\n=== Inverse Volatility Weights ===")
    inv_vol_weights = optimizer.calculate_inv_vol_weights()
    print(inv_vol_weights)

    print("\n=== Mean-Variance Weights ===")
    mv_weights = optimizer.calculate_mean_var_weights()
    print(mv_weights)

    # Create portfolio
    portfolio = optimizer.create_portfolio(erc_weights)
    print(f"\nPortfolio created with {len(portfolio)} periods")

    # Portfolio stats
    stats = optimizer.calculate_portfolio_stats()
    print(f"\nPortfolio Statistics:")
    for key, value in stats.items():
        print(f"  {key}: {value:.4f}")

    print("\n=== FFN Portfolio Optimizer Test: PASSED ===")


if __name__ == "__main__":
    main()
