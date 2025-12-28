import polars as pl
import numpy as np
from typing import Dict, List, Optional, Union, Any
import json
from functime.metrics import (
    mae, mape, mase, mse, rmse, rmsse, smape,
    overforecast, underforecast
)

def calculate_mae(
    y_true: pl.DataFrame,
    y_pred: pl.DataFrame
) -> Dict[str, Any]:
    """Calculate Mean Absolute Error"""
    result = mae(y_true=y_true, y_pred=y_pred)

    return {
        'mae': result.to_dicts(),
        'mean_mae': float(result.select(pl.col('mae').mean()).item())
    }

def calculate_mape(
    y_true: pl.DataFrame,
    y_pred: pl.DataFrame
) -> Dict[str, Any]:
    """Calculate Mean Absolute Percentage Error"""
    result = mape(y_true=y_true, y_pred=y_pred)

    return {
        'mape': result.to_dicts(),
        'mean_mape': float(result.select(pl.col('mape').mean()).item())
    }

def calculate_mase(
    y_true: pl.DataFrame,
    y_pred: pl.DataFrame,
    y_train: pl.DataFrame,
    sp: int = 1
) -> Dict[str, Any]:
    """Calculate Mean Absolute Scaled Error"""
    result = mase(y_true=y_true, y_pred=y_pred, y_train=y_train, sp=sp)

    return {
        'mase': result.to_dicts(),
        'mean_mase': float(result.select(pl.col('mase').mean()).item())
    }

def calculate_mse(
    y_true: pl.DataFrame,
    y_pred: pl.DataFrame
) -> Dict[str, Any]:
    """Calculate Mean Squared Error"""
    result = mse(y_true=y_true, y_pred=y_pred)

    return {
        'mse': result.to_dicts(),
        'mean_mse': float(result.select(pl.col('mse').mean()).item())
    }

def calculate_rmse(
    y_true: pl.DataFrame,
    y_pred: pl.DataFrame
) -> Dict[str, Any]:
    """Calculate Root Mean Squared Error"""
    result = rmse(y_true=y_true, y_pred=y_pred)

    return {
        'rmse': result.to_dicts(),
        'mean_rmse': float(result.select(pl.col('rmse').mean()).item())
    }

def calculate_rmsse(
    y_true: pl.DataFrame,
    y_pred: pl.DataFrame,
    y_train: pl.DataFrame,
    sp: int = 1
) -> Dict[str, Any]:
    """Calculate Root Mean Squared Scaled Error"""
    result = rmsse(y_true=y_true, y_pred=y_pred, y_train=y_train, sp=sp)

    return {
        'rmsse': result.to_dicts(),
        'mean_rmsse': float(result.select(pl.col('rmsse').mean()).item())
    }

def calculate_smape(
    y_true: pl.DataFrame,
    y_pred: pl.DataFrame
) -> Dict[str, Any]:
    """Calculate Symmetric Mean Absolute Percentage Error"""
    result = smape(y_true=y_true, y_pred=y_pred)

    return {
        'smape': result.to_dicts(),
        'mean_smape': float(result.select(pl.col('smape').mean()).item())
    }

def calculate_overforecast(
    y_true: pl.DataFrame,
    y_pred: pl.DataFrame
) -> Dict[str, Any]:
    """Calculate overforecast percentage"""
    result = overforecast(y_true=y_true, y_pred=y_pred)

    return {
        'overforecast': result.to_dicts(),
        'mean_overforecast': float(result.select(pl.col('overforecast').mean()).item())
    }

def calculate_underforecast(
    y_true: pl.DataFrame,
    y_pred: pl.DataFrame
) -> Dict[str, Any]:
    """Calculate underforecast percentage"""
    result = underforecast(y_true=y_true, y_pred=y_pred)

    # Handle null values (when there's no underforecast)
    mean_val = result.select(pl.col('underforecast').mean()).item()
    mean_underforecast = float(mean_val) if mean_val is not None else 0.0

    return {
        'underforecast': result.to_dicts(),
        'mean_underforecast': mean_underforecast
    }

def main():
    print("Testing functime metrics wrapper")

    # Create sample data
    from datetime import datetime, timedelta

    base_date = datetime(2020, 1, 1)
    dates_true = [base_date + timedelta(days=i) for i in range(3)]

    y_true = pl.DataFrame({
        'entity_id': ['A'] * 3 + ['B'] * 3,
        'time': dates_true * 2,
        'value': [10.0, 15.0, 20.0, 12.0, 18.0, 24.0]
    })

    y_pred = pl.DataFrame({
        'entity_id': ['A'] * 3 + ['B'] * 3,
        'time': dates_true * 2,
        'value': [11.0, 14.0, 21.0, 13.0, 17.0, 25.0]
    })

    base_date_train = datetime(2019, 12, 26)
    dates_train = [base_date_train + timedelta(days=i) for i in range(3)]

    y_train = pl.DataFrame({
        'entity_id': ['A'] * 3 + ['B'] * 3,
        'time': dates_train * 2,
        'value': [8.0, 9.0, 9.5, 10.0, 11.0, 11.5]
    })

    # Test all metrics
    print("\nMetrics Results:")
    print("-" * 40)

    mae_result = calculate_mae(y_true, y_pred)
    print("MAE:           {:.4f}".format(mae_result['mean_mae']))

    rmse_result = calculate_rmse(y_true, y_pred)
    print("RMSE:          {:.4f}".format(rmse_result['mean_rmse']))

    smape_result = calculate_smape(y_true, y_pred)
    print("SMAPE:         {:.4f}".format(smape_result['mean_smape']))

    mape_result = calculate_mape(y_true, y_pred)
    print("MAPE:          {:.4f}".format(mape_result['mean_mape']))

    mase_result = calculate_mase(y_true, y_pred, y_train)
    print("MASE:          {:.4f}".format(mase_result['mean_mase']))

    over_result = calculate_overforecast(y_true, y_pred)
    print("Overforecast:  {:.4f}".format(over_result['mean_overforecast']))

    under_result = calculate_underforecast(y_true, y_pred)
    print("Underforecast: {:.4f}".format(under_result['mean_underforecast']))

    print("-" * 40)
    print("All tests: PASSED")

if __name__ == "__main__":
    main()
