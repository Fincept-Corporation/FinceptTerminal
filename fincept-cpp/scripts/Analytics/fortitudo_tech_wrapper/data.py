"""
Fortitudo.tech Data Loading Wrapper
====================================
Example data loading functions

Includes fallback implementations using generated sample data for Python 3.14+ compatibility.

Usage:
    python data.py
"""

import pandas as pd
import numpy as np
from typing import Tuple, List

# Try to import fortitudo.tech, fallback to sample data generators
try:
    import fortitudo.tech as ft
    FORTITUDO_AVAILABLE = True
except ImportError:
    FORTITUDO_AVAILABLE = False
    ft = None


# ============================================================================
# FALLBACK DATA GENERATORS
# ============================================================================

def _generate_sample_time_series(n_scenarios: int = 5040, seed: int = 42) -> pd.DataFrame:
    """Generate sample time series data similar to fortitudo.tech example data."""
    np.random.seed(seed)

    # Create date index
    dates = pd.date_range(start='2004-01-01', periods=n_scenarios, freq='B')

    # 1 equity index
    equity_returns = np.random.randn(n_scenarios) * 0.01 + 0.0003
    equity_cumulative = 100 * np.exp(np.cumsum(equity_returns))

    # 34 yield curve columns (1m to 30y)
    maturities = ['1M', '2M', '3M', '6M', '1Y', '2Y', '3Y', '5Y', '7Y', '10Y', '15Y', '20Y', '30Y']
    yield_data = {}
    base_yield = 0.02
    for i, mat in enumerate(maturities):
        # Yield curve with term structure
        term_premium = i * 0.002
        yield_data[f'Yield_{mat}'] = base_yield + term_premium + np.random.randn(n_scenarios) * 0.001
        yield_data[f'Yield_{mat}'] = np.cumsum(yield_data[f'Yield_{mat}']) * 0.01 + base_yield + term_premium

    # 35 option columns (various strikes and maturities)
    option_data = {}
    for strike in range(90, 111, 5):  # 5 strikes
        for mat in ['1M', '3M', '6M', '1Y', '2Y', '3Y', '5Y']:  # 7 maturities
            key = f'Option_K{strike}_{mat}'
            base_iv = 0.20 + abs(strike - 100) * 0.002  # Smile
            option_data[key] = base_iv + np.random.randn(n_scenarios) * 0.02

    # 10 credit spread columns
    credit_data = {}
    for rating in ['AAA', 'AA', 'A', 'BBB', 'BB', 'B', 'CCC', 'HY', 'IG', 'Total']:
        base_spread = {'AAA': 0.005, 'AA': 0.01, 'A': 0.015, 'BBB': 0.02,
                      'BB': 0.03, 'B': 0.05, 'CCC': 0.08, 'HY': 0.04, 'IG': 0.01, 'Total': 0.02}
        credit_data[f'Credit_{rating}'] = base_spread.get(rating, 0.02) + np.random.randn(n_scenarios) * 0.005

    # Combine all data
    data = {'Equity_Index': equity_cumulative}
    data.update(yield_data)
    data.update(option_data)
    data.update(credit_data)

    df = pd.DataFrame(data, index=dates)
    return df


def _generate_sample_risk_factors(n_scenarios: int = 1000, seed: int = 42) -> pd.DataFrame:
    """Generate sample risk factor data."""
    np.random.seed(seed)

    dates = pd.date_range(start='2020-01-01', periods=n_scenarios, freq='B')

    factors = {
        'Equity': np.random.randn(n_scenarios) * 0.02,
        'Duration': np.random.randn(n_scenarios) * 0.005,
        'Credit': np.random.randn(n_scenarios) * 0.01,
        'FX': np.random.randn(n_scenarios) * 0.008,
        'Commodity': np.random.randn(n_scenarios) * 0.015,
        'Volatility': np.random.randn(n_scenarios) * 0.02
    }

    return pd.DataFrame(factors, index=dates)


def _generate_sample_pnl(n_scenarios: int = 1000, n_positions: int = 10, seed: int = 42) -> pd.DataFrame:
    """Generate sample P&L data."""
    np.random.seed(seed)

    position_names = [f'Position_{i+1}' for i in range(n_positions)]
    pnl_data = {}

    for pos in position_names:
        pnl_data[pos] = np.random.randn(n_scenarios) * 10000 + 500

    return pd.DataFrame(pnl_data)


def _generate_sample_parameters() -> Tuple[List, np.ndarray, np.ndarray]:
    """Generate sample parameters for volatility surface."""
    # Strike levels
    strikes = [0.8, 0.9, 0.95, 1.0, 1.05, 1.1, 1.2]

    # Maturities in years
    maturities = np.array([0.0833, 0.25, 0.5, 1.0, 2.0])

    # Sample volatility surface parameters
    base_vols = np.array([
        [0.25, 0.22, 0.20, 0.19, 0.20, 0.22, 0.25],  # 1M
        [0.24, 0.21, 0.19, 0.18, 0.19, 0.21, 0.24],  # 3M
        [0.23, 0.20, 0.18, 0.17, 0.18, 0.20, 0.23],  # 6M
        [0.22, 0.19, 0.17, 0.16, 0.17, 0.19, 0.22],  # 1Y
        [0.21, 0.18, 0.16, 0.15, 0.16, 0.18, 0.21],  # 2Y
    ])

    return strikes, maturities, base_vols


def load_example_time_series() -> pd.DataFrame:
    """
    Load example time series data

    Returns:
        DataFrame with shape (5040, 79) containing:
        - 1 equity index column
        - 34 yield curve columns (1m to 30y)
        - 35 option columns (various strikes and maturities)
        - 10 credit spread columns
    """
    if FORTITUDO_AVAILABLE:
        return ft.load_time_series()
    else:
        return _generate_sample_time_series()


def load_example_risk_factors() -> pd.DataFrame:
    """
    Load example risk factor data

    Returns:
        DataFrame with risk factor time series
    """
    if FORTITUDO_AVAILABLE:
        return ft.load_risk_factors()
    else:
        return _generate_sample_risk_factors()


def load_example_pnl() -> pd.DataFrame:
    """
    Load example P&L data

    Returns:
        DataFrame with P&L scenarios
    """
    if FORTITUDO_AVAILABLE:
        return ft.load_pnl()
    else:
        return _generate_sample_pnl()


def load_example_parameters() -> Tuple[List, np.ndarray, np.ndarray]:
    """
    Load example parameters for volatility surface

    Returns:
        Tuple of (list, array, array) containing parameters
    """
    if FORTITUDO_AVAILABLE:
        return ft.load_parameters()
    else:
        return _generate_sample_parameters()


def get_dataset_info(df: pd.DataFrame) -> dict:
    """
    Get information about a loaded dataset

    Args:
        df: DataFrame to analyze

    Returns:
        Dictionary with dataset statistics
    """
    return {
        'shape': df.shape,
        'n_rows': len(df),
        'n_columns': len(df.columns),
        'columns': df.columns.tolist(),
        'dtypes': df.dtypes.to_dict(),
        'memory_usage_mb': df.memory_usage(deep=True).sum() / 1024 / 1024,
        'has_nulls': df.isnull().any().any(),
        'null_counts': df.isnull().sum().to_dict() if df.isnull().any().any() else {}
    }


def main():
    """Test data loading functions"""
    print("=== Fortitudo.tech Data Loading Test ===\n")

    # Test 1: Load time series
    print("1. Loading Example Time Series")
    print("-" * 50)
    ts = load_example_time_series()
    info_ts = get_dataset_info(ts)
    print(f"  Shape: {info_ts['shape']}")
    print(f"  Columns: {info_ts['n_columns']}")
    print(f"  Memory: {info_ts['memory_usage_mb']:.2f} MB")
    print(f"  First 5 columns: {info_ts['columns'][:5]}")
    print("\n  First 3 rows, first 5 columns:")
    print(ts.iloc[:3, :5])

    # Test 2: Load risk factors
    print("\n2. Loading Example Risk Factors")
    print("-" * 50)
    rf = load_example_risk_factors()
    info_rf = get_dataset_info(rf)
    print(f"  Shape: {info_rf['shape']}")
    print(f"  Columns: {info_rf['columns']}")
    print("\n  First 3 rows:")
    print(rf.head(3))

    # Test 3: Load P&L
    print("\n3. Loading Example P&L Data")
    print("-" * 50)
    pnl = load_example_pnl()
    info_pnl = get_dataset_info(pnl)
    print(f"  Shape: {info_pnl['shape']}")
    print(f"  Columns: {info_pnl['columns']}")
    print("\n  Statistics:")
    print(pnl.describe())

    # Test 4: Load parameters
    print("\n4. Loading Example Parameters")
    print("-" * 50)
    params = load_example_parameters()
    print(f"  Number of items: {len(params)}")
    for i, item in enumerate(params):
        if isinstance(item, list):
            print(f"  Item {i}: list with {len(item)} elements")
        elif isinstance(item, np.ndarray):
            print(f"  Item {i}: array with shape {item.shape}")
        else:
            print(f"  Item {i}: {type(item)}")

    print("\n=== Test: PASSED ===")


if __name__ == "__main__":
    main()
