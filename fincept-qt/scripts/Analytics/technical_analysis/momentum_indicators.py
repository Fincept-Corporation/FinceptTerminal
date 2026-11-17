"""
Momentum Indicators Calculator
Calculates various momentum indicators for technical analysis

CLI: python momentum_indicators.py <indicator> '{"symbol": "AAPL", ...params}'
  indicator: rsi, macd, stochastic, cci, roc, williams_r, awesome_oscillator, tsi, ultimate_oscillator
  native: python momentum_indicators.py <symbol> <indicator> [params_json]
"""

import os
import sys
import json
import pandas as pd
import numpy as np

try:
    import fincept_json
except ImportError:  # run by hand without PYTHONPATH=scripts
    sys.path.append(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))
    import fincept_json


def _mask_warmup(series, n):
    """Blank the first n values: they come from a recursion seeded on the first bar, not from data."""
    series = series.copy()
    series.iloc[:max(0, int(n))] = np.nan
    return series


def wilder_rsi(close, period=14):
    """Wilder's RSI, as in the terminal's own IndicatorEngine::compute_rsi.

    The first average gain / loss is the simple mean of the first `period` changes; after that
    avg = (prev_avg * (period - 1) + current) / period. RSI is 100 when the average loss is ~0.
    The first `period` bars have no value.
    """
    close = pd.Series(close, dtype=float)
    n = len(close)
    out = np.full(n, np.nan)
    if period < 1 or n < period + 1:
        return pd.Series(out, index=close.index)
    diff = close.diff().to_numpy()
    gains = np.where(diff > 0, diff, 0.0)
    losses = np.where(diff < 0, -diff, 0.0)
    avg_gain = gains[1:period + 1].mean()
    avg_loss = losses[1:period + 1].mean()

    def _rsi(ag, al):
        return 100.0 if al < 1e-10 else 100.0 - 100.0 / (1.0 + ag / al)

    out[period] = _rsi(avg_gain, avg_loss)
    for i in range(period + 1, n):
        avg_gain = (avg_gain * (period - 1) + gains[i]) / period
        avg_loss = (avg_loss * (period - 1) + losses[i]) / period
        out[i] = _rsi(avg_gain, avg_loss)
    return pd.Series(out, index=close.index)


def calculate_rsi(data, period=14):
    """Calculate Relative Strength Index (Wilder smoothing, matching the Technicals tab)"""
    rsi = wilder_rsi(data['Close'], int(period))

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
    # Warm-up: the EMAs start on the first bar, so the line reads 0.0 there; values are only
    # meaningful once the slow EMA (and then the signal EMA) has seen a full window.
    macd_line = _mask_warmup(macd_line, slow_period - 1)
    signal_line = _mask_warmup(signal_line, slow_period + signal_period - 2)
    histogram = _mask_warmup(histogram, slow_period + signal_period - 2)

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
    # Warm-up (first change is on bar 1): the second bar read exactly +/-100.
    tsi = _mask_warmup(tsi, long_period + short_period - 1)
    signal_line = _mask_warmup(signal_line, long_period + short_period + signal_period - 2)

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
    """Calculate Ultimate Oscillator (Williams).

    Buying pressure BP = Close - min(Low, prior Close); true range TR = max(High, prior Close) -
    min(Low, prior Close). Only the CLOSE is the prior bar's - the old code shifted High and Low too.
    """
    prev_close = data['Close'].shift(1)
    true_low = pd.concat([data['Low'], prev_close], axis=1).min(axis=1)
    true_high = pd.concat([data['High'], prev_close], axis=1).max(axis=1)
    bp = data['Close'] - true_low
    tr = true_high - true_low
    # First bar has no prior close: leave it out of the sums instead of using its own range
    bp.iloc[0] = np.nan
    tr.iloc[0] = np.nan

    def _avg(window):
        tr_sum = tr.rolling(window=window).sum()
        return bp.rolling(window=window).sum() / tr_sum.where(tr_sum != 0)  # flat window: no value

    avg1, avg2, avg3 = _avg(period1), _avg(period2), _avg(period3)

    uo = 100 * ((weight1 * avg1) + (weight2 * avg2) + (weight3 * avg3)) / (weight1 + weight2 + weight3)

    result = []
    for idx, value in uo.items():
        result.append({
            'time': idx.isoformat(),
            'value': float(value) if not pd.isna(value) else None
        })

    return {'values': result}


INDICATORS = ('rsi', 'macd', 'stochastic', 'cci', 'roc', 'williams_r', 'awesome_oscillator', 'tsi',
              'ultimate_oscillator')


def main():
    try:
        # Second argv form (used by agents through the script index; there is no C++ caller):
        # (indicator_type, {"symbol":..., ...}) is expanded into the native
        # (symbol, indicator_type, params_json) argv. A plain indicator_type in argv[2] is not
        # JSON, so the native form falls through unchanged.
        if len(sys.argv) == 3:
            try:
                _qp = json.loads(sys.argv[2])
                if isinstance(_qp, dict) and "symbol" in _qp:
                    _qsym = _qp.pop("symbol")
                    sys.argv = [sys.argv[0], str(_qsym), str(sys.argv[1]), json.dumps(_qp)]
            except Exception:
                pass

        if len(sys.argv) < 3:
            raise ValueError("Usage: python momentum_indicators.py <symbol> <indicator_type> <params_json>")

        symbol = sys.argv[1]
        indicator_type = sys.argv[2]
        params = json.loads(sys.argv[3]) if len(sys.argv) > 3 else {}
        if not isinstance(params, dict):
            raise ValueError("params_json must be a JSON object")
        if indicator_type not in INDICATORS:  # before the network fetch
            raise ValueError(f"Unknown indicator type: {indicator_type} (available: {', '.join(INDICATORS)})")

        # Get timeframe and interval
        timeframe = params.get('timeframe', '1y')
        interval = params.get('interval', '1d')

        # Fetch data
        import contextlib
        import fincept_yahoo as fy  # the shared bar cache (one long download per symbol, sliced)
        with contextlib.redirect_stdout(sys.stderr):  # keep library chatter out of the JSON stdout
            data = fy.ticker_history(symbol, period=timeframe, interval=interval)

        if data is None or data.empty:
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

        fincept_json.emit(result)  # ROC / %R / stochastic divide by prices or ranges that can be 0

    except Exception as e:
        fincept_json.emit({'error': str(e) or type(e).__name__})
        sys.exit(1)


if __name__ == '__main__':
    main()
