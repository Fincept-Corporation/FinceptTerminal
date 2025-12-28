"""
Fortitudo.tech Data Loading Wrapper
====================================
Example data loading functions

Usage:
    python data.py
"""

import pandas as pd
import numpy as np
from typing import Tuple, List
import fortitudo.tech as ft


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
    return ft.load_time_series()


def load_example_risk_factors() -> pd.DataFrame:
    """
    Load example risk factor data

    Returns:
        DataFrame with risk factor time series
    """
    return ft.load_risk_factors()


def load_example_pnl() -> pd.DataFrame:
    """
    Load example P&L data

    Returns:
        DataFrame with P&L scenarios
    """
    return ft.load_pnl()


def load_example_parameters() -> Tuple[List, np.ndarray, np.ndarray]:
    """
    Load example parameters for volatility surface

    Returns:
        Tuple of (list, array, array) containing parameters
    """
    return ft.load_parameters()


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
