"""
Momentum Indicators Calculator
Calculates various momentum indicators for technical analysis
"""

import sys
import json
import yfinance as yf
import pandas as pd
import numpy as np
from datetime import datetime


def calculate_rsi(data, period=14):
    """Calculate Relative Strength Index"""
    delta = data['Close'].diff()
    gain = (delta.where(delta > 0, 0)).rolling(window=period).mean()
    loss = (-delta.where(delta < 0, 0)).rolling(window=period).mean()

    rs = gain / loss
    rsi = 100 - (100 / (1 + rs))

    result = []
    for idx, value in rsi.items():
        result.append({
            'time': idx.isoformat(),
            'value': float(value) if not pd.isna(value) else None
        })

    return {'values': result}


def calculate_macd(data, fast_period=12, slow_period=26, signal_period=9):
    """Calculate MACD (Moving Average Convergence Divergence)"""
    exp1 = data['Close'].ewm(span=fast_period, adjust=False).mean()
    exp2 = data['Close'].ewm(span=slow_period, adjust=False).mean()

    macd_line = exp1 - exp2
    signal_line = macd_line.ewm(span=signal_period, adjust=False).mean()
    histogram = macd_line - signal_line

    macd_result = []
    signal_result = []
    histogram_result = []

    for idx in macd_line.index:
        macd_result.append({
            'time': idx.isoformat(),
            'value': float(macd_line[idx]) if not pd.isna(macd_line[idx]) else None
        })
        signal_result.append({
            'time': idx.isoformat(),
            'value': float(signal_line[idx]) if not pd.isna(signal_line[idx]) else None
        })
        histogram_result.append({
            'time': idx.isoformat(),
            'value': float(histogram[idx]) if not pd.isna(histogram[idx]) else None
        })

    return {
        'macd_line': macd_result,
        'signal_line': signal_result,
        'histogram': histogram_result
    }


def calculate_stochastic(data, k_period=14, d_period=3):
    """Calculate Stochastic Oscillator"""
    low_min = data['Low'].rolling(window=k_period).min()
    high_max = data['High'].rolling(window=k_period).max()

    k_values = 100 * ((data['Close'] - low_min) / (high_max - low_min))
    d_values = k_values.rolling(window=d_period).mean()

    k_result = []
    d_result = []

    for idx in k_values.index:
        k_result.append({
            'time': idx.isoformat(),
            'value': float(k_values[idx]) if not pd.isna(k_values[idx]) else None
        })
        d_result.append({
            'time': idx.isoformat(),
            'value': float(d_values[idx]) if not pd.isna(d_values[idx]) else None
        })

    return {
        'k_values': k_result,
        'd_values': d_result
    }


def calculate_cci(data, period=20):
    """Calculate Commodity Channel Index"""
    tp = (data['High'] + data['Low'] + data['Close']) / 3
    sma = tp.rolling(window=period).mean()
    mad = tp.rolling(window=period).apply(lambda x: np.fabs(x - x.mean()).mean())

    cci = (tp - sma) / (0.015 * mad)

    result = []
    for idx, value in cci.items():
        result.append({
            'time': idx.isoformat(),
            'value': float(value) if not pd.isna(value) else None
        })

    return {'values': result}


def calculate_roc(data, period=12):
    """Calculate Rate of Change"""
    roc = ((data['Close'] - data['Close'].shift(period)) / data['Close'].shift(period)) * 100

    result = []
    for idx, value in roc.items():
        result.append({
            'time': idx.isoformat(),
            'value': float(value) if not pd.isna(value) else None
        })

    return {'values': result}


def calculate_williams_r(data, period=14):
    """Calculate Williams %R"""
    high_max = data['High'].rolling(window=period).max()
    low_min = data['Low'].rolling(window=period).min()

    williams_r = -100 * ((high_max - data['Close']) / (high_max - low_min))

    result = []
    for idx, value in williams_r.items():
        result.append({
            'time': idx.isoformat(),
            'value': float(value) if not pd.isna(value) else None
        })

    return {'values': result}


def calculate_awesome_oscillator(data, fast_period=5, slow_period=34):
    """Calculate Awesome Oscillator"""
    median_price = (data['High'] + data['Low']) / 2

    fast_sma = median_price.rolling(window=fast_period).mean()
    slow_sma = median_price.rolling(window=slow_period).mean()

    ao = fast_sma - slow_sma

    result = []
    for idx, value in ao.items():
        result.append({
            'time': idx.isoformat(),
            'value': float(value) if not pd.isna(value) else None
        })

    return {'values': result}


def calculate_tsi(data, long_period=25, short_period=13, signal_period=13):
    """Calculate True Strength Index"""
    price_change = data['Close'].diff()

    # Double smoothed momentum
    first_smooth = price_change.ewm(span=long_period, adjust=False).mean()
    double_smooth = first_smooth.ewm(span=short_period, adjust=False).mean()

    # Double smoothed absolute momentum
    abs_first_smooth = price_change.abs().ewm(span=long_period, adjust=False).mean()
    abs_double_smooth = abs_first_smooth.ewm(span=short_period, adjust=False).mean()

    tsi = 100 * (double_smooth / abs_double_smooth)
    signal_line = tsi.ewm(span=signal_period, adjust=False).mean()

    tsi_result = []
    signal_result = []

    for idx in tsi.index:
        tsi_result.append({
            'time': idx.isoformat(),
            'value': float(tsi[idx]) if not pd.isna(tsi[idx]) else None
        })
        signal_result.append({
            'time': idx.isoformat(),
            'value': float(signal_line[idx]) if not pd.isna(signal_line[idx]) else None
        })

    return {
        'values': tsi_result,
        'signal_line': signal_result
    }


def calculate_ultimate_oscillator(data, period1=7, period2=14, period3=28, weight1=4, weight2=2, weight3=1):
    """Calculate Ultimate Oscillator"""
    bp = data['Close'] - data[['Low', 'Close']].shift(1).min(axis=1)
    tr = data[['High', 'Close']].shift(1).max(axis=1) - data[['Low', 'Close']].shift(1).min(axis=1)

    avg1 = bp.rolling(window=period1).sum() / tr.rolling(window=period1).sum()
    avg2 = bp.rolling(window=period2).sum() / tr.rolling(window=period2).sum()
    avg3 = bp.rolling(window=period3).sum() / tr.rolling(window=period3).sum()

    uo = 100 * ((weight1 * avg1) + (weight2 * avg2) + (weight3 * avg3)) / (weight1 + weight2 + weight3)

    result = []
    for idx, value in uo.items():
        result.append({
            'time': idx.isoformat(),
            'value': float(value) if not pd.isna(value) else None
        })

    return {'values': result}


def main():
    try:
        if len(sys.argv) < 3:
            raise ValueError("Usage: python momentum_indicators.py <symbol> <indicator_type> <params_json>")

        symbol = sys.argv[1]
        indicator_type = sys.argv[2]
        params = json.loads(sys.argv[3]) if len(sys.argv) > 3 else {}

        # Get timeframe and interval
        timeframe = params.get('timeframe', '1y')
        interval = params.get('interval', '1d')

        # Fetch data
        ticker = yf.Ticker(symbol)
        data = ticker.history(period=timeframe, interval=interval)

        if data.empty:
            raise ValueError(f"No data found for symbol {symbol}")

        # Calculate indicator based on type
        if indicator_type == 'rsi':
            result = calculate_rsi(data, params.get('period', 14))
        elif indicator_type == 'macd':
            result = calculate_macd(
                data,
                params.get('fast_period', 12),
                params.get('slow_period', 26),
                params.get('signal_period', 9)
            )
        elif indicator_type == 'stochastic':
            result = calculate_stochastic(
                data,
                params.get('k_period', 14),
                params.get('d_period', 3)
            )
        elif indicator_type == 'cci':
            result = calculate_cci(data, params.get('period', 20))
        elif indicator_type == 'roc':
            result = calculate_roc(data, params.get('period', 12))
        elif indicator_type == 'williams_r':
            result = calculate_williams_r(data, params.get('period', 14))
        elif indicator_type == 'awesome_oscillator':
            result = calculate_awesome_oscillator(
                data,
                params.get('fast_period', 5),
                params.get('slow_period', 34)
            )
        elif indicator_type == 'tsi':
            result = calculate_tsi(
                data,
                params.get('long_period', 25),
                params.get('short_period', 13),
                params.get('signal_period', 13)
            )
        elif indicator_type == 'ultimate_oscillator':
            result = calculate_ultimate_oscillator(
                data,
                params.get('period1', 7),
                params.get('period2', 14),
                params.get('period3', 28),
                params.get('weight1', 4),
                params.get('weight2', 2),
                params.get('weight3', 1)
            )
        else:
            raise ValueError(f"Unknown indicator type: {indicator_type}")

        print(json.dumps(result))

    except Exception as e:
        error_msg = {'error': str(e)}
        print(json.dumps(error_msg))
        sys.exit(1)


if __name__ == '__main__':
    main()
