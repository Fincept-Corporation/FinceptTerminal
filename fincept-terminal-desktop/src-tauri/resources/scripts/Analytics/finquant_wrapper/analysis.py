import pandas as pd
import numpy as np
from typing import Dict, List, Optional, Union
import json
from finquant.returns import (
    cumulative_returns,
    daily_returns,
    daily_log_returns,
    historical_mean_return,
    weighted_mean_daily_returns
)
from finquant.moving_average import (
    sma,
    ema,
    sma_std,
    ema_std,
    compute_ma,
    plot_bollinger_band
)

def calculate_cumulative_returns(
    data: Union[pd.DataFrame, Dict],
    dividend: float = 0.0
) -> Dict:
    if isinstance(data, dict):
        data = pd.DataFrame(data)

    cum_returns = cumulative_returns(data, dividend=dividend)

    return {
        'cumulative_returns': cum_returns.to_dict(),
        'columns': cum_returns.columns.tolist(),
        'index': cum_returns.index.astype(str).tolist()
    }

def calculate_daily_returns(
    data: Union[pd.DataFrame, Dict]
) -> Dict:
    if isinstance(data, dict):
        data = pd.DataFrame(data)

    daily_ret = daily_returns(data)

    return {
        'daily_returns': daily_ret.to_dict(),
        'columns': daily_ret.columns.tolist(),
        'index': daily_ret.index.astype(str).tolist()
    }

def calculate_log_returns(
    data: Union[pd.DataFrame, Dict]
) -> Dict:
    if isinstance(data, dict):
        data = pd.DataFrame(data)

    log_ret = daily_log_returns(data)

    return {
        'log_returns': log_ret.to_dict(),
        'columns': log_ret.columns.tolist(),
        'index': log_ret.index.astype(str).tolist()
    }

def calculate_historical_mean_return(
    data: Union[pd.Series, pd.DataFrame, Dict],
    freq: int = 252
) -> Dict:
    if isinstance(data, dict):
        if isinstance(list(data.values())[0], dict):
            data = pd.DataFrame(data)
        else:
            data = pd.Series(data)

    mean_ret = historical_mean_return(data, freq=freq)

    if isinstance(mean_ret, pd.Series):
        return {
            'mean_returns': mean_ret.to_dict(),
            'frequency': freq
        }
    else:
        return {
            'mean_return': float(mean_ret),
            'frequency': freq
        }

def calculate_weighted_returns(
    data: Union[pd.DataFrame, Dict],
    weights: Union[List[float], np.ndarray, pd.Series]
) -> Dict:
    if isinstance(data, dict):
        data = pd.DataFrame(data)

    if isinstance(weights, list):
        weights = np.array(weights)
    elif isinstance(weights, pd.Series):
        weights = weights.values

    weighted_ret = weighted_mean_daily_returns(data, weights)

    return {
        'weighted_returns': weighted_ret.tolist(),
        'weights': weights.tolist()
    }

def calculate_sma(
    data: Union[pd.DataFrame, Dict],
    span: int = 100
) -> Dict:
    if isinstance(data, dict):
        data = pd.DataFrame(data)

    sma_result = sma(data, span=span)

    return {
        'sma': sma_result.to_dict(),
        'span': span,
        'columns': sma_result.columns.tolist(),
        'index': sma_result.index.astype(str).tolist()
    }

def calculate_ema(
    data: Union[pd.DataFrame, Dict],
    span: int = 100
) -> Dict:
    if isinstance(data, dict):
        data = pd.DataFrame(data)

    ema_result = ema(data, span=span)

    return {
        'ema': ema_result.to_dict(),
        'span': span,
        'columns': ema_result.columns.tolist(),
        'index': ema_result.index.astype(str).tolist()
    }

def calculate_bollinger_bands(
    data: Union[pd.DataFrame, Dict],
    span: int = 100,
    use_ema: bool = False
) -> Dict:
    if isinstance(data, dict):
        data = pd.DataFrame(data)

    if use_ema:
        ma_result = ema(data, span=span)
        std_result = ema_std(data, span=span)
    else:
        ma_result = sma(data, span=span)
        std_result = sma_std(data, span=span)

    upper_band = ma_result + 2 * std_result
    lower_band = ma_result - 2 * std_result

    return {
        'moving_average': ma_result.to_dict(),
        'upper_band': upper_band.to_dict(),
        'lower_band': lower_band.to_dict(),
        'span': span,
        'type': 'EMA' if use_ema else 'SMA',
        'columns': ma_result.columns.tolist(),
        'index': ma_result.index.astype(str).tolist()
    }

def calculate_moving_average_custom(
    data: Union[pd.Series, pd.DataFrame, Dict],
    spans: List[int],
    use_ema: bool = False
) -> Dict:
    if isinstance(data, dict):
        if isinstance(list(data.values())[0], dict):
            data = pd.DataFrame(data)
        else:
            data = pd.Series(data)

    ma_func = ema if use_ema else sma
    result = compute_ma(data, fun=ma_func, spans=spans, plot=False)

    return {
        'moving_averages': result.to_dict(),
        'spans': spans,
        'type': 'EMA' if use_ema else 'SMA',
        'columns': result.columns.tolist(),
        'index': result.index.astype(str).tolist()
    }

def main():
    print("Testing FinQuant Analysis Wrapper")

    dates = pd.date_range('2023-01-01', periods=100)
    data = pd.DataFrame({
        'AAPL': np.random.randn(100).cumsum() + 100,
        'GOOG': np.random.randn(100).cumsum() + 100,
        'AMZN': np.random.randn(100).cumsum() + 100
    }, index=dates)

    print("\n1. Testing calculate_daily_returns...")
    try:
        result = calculate_daily_returns(data)
        print("Columns: {}".format(result['columns']))
        print("Number of rows: {}".format(len(result['index'])))
        print("Test 1: PASSED")
    except Exception as e:
        print("Test 1: FAILED - {}".format(str(e)))
        return

    print("\n2. Testing calculate_sma...")
    try:
        result = calculate_sma(data, span=20)
        print("SMA span: {}".format(result['span']))
        print("Columns: {}".format(result['columns']))
        print("Test 2: PASSED")
    except Exception as e:
        print("Test 2: FAILED - {}".format(str(e)))
        return

    print("\n3. Testing calculate_bollinger_bands...")
    try:
        result = calculate_bollinger_bands(data, span=20)
        print("BB span: {}".format(result['span']))
        print("BB type: {}".format(result['type']))
        print("Test 3: PASSED")
    except Exception as e:
        print("Test 3: FAILED - {}".format(str(e)))
        return

    print("\n4. Testing calculate_historical_mean_return...")
    try:
        daily_ret = daily_returns(data)
        result = calculate_historical_mean_return(daily_ret)
        print("Mean returns: {}".format(result['mean_returns']))
        print("Test 4: PASSED")
    except Exception as e:
        print("Test 4: FAILED - {}".format(str(e)))
        return

    print("\nAll tests: PASSED")

if __name__ == "__main__":
    main()
