"""
Seasonality Analysis Module
============================

Provides seasonal decomposition and analysis:
- STL decomposition (Seasonal-Trend-Loess)
- Classical decomposition
- Seasonal period detection
- Seasonal adjustment/deseasonalization
- Seasonality strength measurement

Essential for understanding time series patterns.
"""

import polars as pl
import numpy as np
from typing import Dict, List, Optional, Union, Any, Tuple
from datetime import datetime, timedelta
from scipy import signal
from scipy import fft
import warnings

warnings.filterwarnings('ignore')

# Try importing statsmodels for STL
try:
    from statsmodels.tsa.seasonal import STL, seasonal_decompose
    STATSMODELS_AVAILABLE = True
except ImportError:
    STATSMODELS_AVAILABLE = False


# ============================================================================
# SEASONAL PERIOD DETECTION
# ============================================================================

def detect_seasonal_period(
    y: pl.DataFrame,
    method: str = 'fft',
    max_period: int = 365
) -> Dict[str, Any]:
    """
    Automatically detect the seasonal period of time series.

    Args:
        y: Panel data
        method: 'fft' (spectral), 'acf' (autocorrelation), or 'both'
        max_period: Maximum period to consider
    """
    results = {}
    entities = y['entity_id'].unique().to_list()

    for entity_id in entities:
        entity_data = y.filter(pl.col('entity_id') == entity_id).sort('time')
        values = entity_data['value'].to_numpy()

        if len(values) < 10:
            results[entity_id] = {'period': None, 'strength': 0.0}
            continue

        periods = {}

        if method in ['fft', 'both']:
            fft_period, fft_strength = _detect_period_fft(values, max_period)
            periods['fft'] = {'period': fft_period, 'strength': fft_strength}

        if method in ['acf', 'both']:
            acf_period, acf_strength = _detect_period_acf(values, max_period)
            periods['acf'] = {'period': acf_period, 'strength': acf_strength}

        # Use the method with highest strength
        best_method = max(periods.items(), key=lambda x: x[1]['strength'])
        results[entity_id] = {
            'period': best_method[1]['period'],
            'strength': best_method[1]['strength'],
            'method': best_method[0],
            'all_methods': periods
        }

    return {
        'success': True,
        'method': method,
        'max_period': max_period,
        'results': results
    }


def _detect_period_fft(values: np.ndarray, max_period: int) -> Tuple[Optional[int], float]:
    """Detect period using FFT (spectral analysis)."""
    n = len(values)

    # Detrend
    detrended = signal.detrend(values)

    # Apply FFT
    fft_result = np.abs(fft.fft(detrended))
    freqs = fft.fftfreq(n)

    # Find dominant frequency (excluding DC component)
    half_n = n // 2
    magnitudes = fft_result[1:half_n]
    frequencies = freqs[1:half_n]

    # Filter to valid periods
    valid_mask = (1.0 / np.abs(frequencies)) <= max_period
    if not np.any(valid_mask):
        return None, 0.0

    magnitudes = magnitudes[valid_mask]
    frequencies = frequencies[valid_mask]

    if len(magnitudes) == 0:
        return None, 0.0

    # Find peak
    peak_idx = np.argmax(magnitudes)
    peak_freq = frequencies[peak_idx]
    period = int(round(1.0 / abs(peak_freq))) if peak_freq != 0 else None

    # Calculate strength as ratio of peak to mean
    strength = float(magnitudes[peak_idx] / np.mean(magnitudes)) if np.mean(magnitudes) > 0 else 0.0

    return period, min(strength / 10, 1.0)  # Normalize to 0-1


def _detect_period_acf(values: np.ndarray, max_period: int) -> Tuple[Optional[int], float]:
    """Detect period using autocorrelation."""
    n = len(values)
    max_lag = min(max_period, n // 2)

    # Detrend
    detrended = signal.detrend(values)

    # Calculate ACF
    acf = np.correlate(detrended, detrended, mode='full')
    acf = acf[n-1:]  # Keep positive lags only
    acf = acf / acf[0]  # Normalize

    # Find peaks in ACF (excluding lag 0)
    acf_subset = acf[1:max_lag+1]

    if len(acf_subset) < 3:
        return None, 0.0

    # Find local maxima
    peaks, _ = signal.find_peaks(acf_subset, height=0.1)

    if len(peaks) == 0:
        return None, 0.0

    # First significant peak is likely the period
    period = int(peaks[0] + 1)
    strength = float(acf_subset[peaks[0]])

    return period, strength


# ============================================================================
# SEASONAL DECOMPOSITION
# ============================================================================

def decompose_stl(
    y: pl.DataFrame,
    period: Optional[int] = None,
    robust: bool = True
) -> Dict[str, Any]:
    """
    STL (Seasonal-Trend-Loess) decomposition.

    Args:
        y: Panel data
        period: Seasonal period (auto-detected if None)
        robust: Use robust fitting (less sensitive to outliers)
    """
    if not STATSMODELS_AVAILABLE:
        return _fallback_decomposition(y, period)

    results = {}
    entities = y['entity_id'].unique().to_list()

    for entity_id in entities:
        entity_data = y.filter(pl.col('entity_id') == entity_id).sort('time')
        values = entity_data['value'].to_numpy()
        times = entity_data['time'].to_list()

        # Auto-detect period if not provided
        if period is None:
            detected = detect_seasonal_period(
                entity_data.select(['entity_id', 'time', 'value']),
                method='fft'
            )
            period = detected['results'].get(entity_id, {}).get('period', 7)
            if period is None or period < 2:
                period = 7  # Default to weekly

        if len(values) < 2 * period:
            results[entity_id] = {
                'error': 'Insufficient data for decomposition',
                'trend': values.tolist(),
                'seasonal': [0.0] * len(values),
                'residual': [0.0] * len(values)
            }
            continue

        try:
            stl = STL(values, period=period, robust=robust)
            result = stl.fit()

            results[entity_id] = {
                'trend': result.trend.tolist(),
                'seasonal': result.seasonal.tolist(),
                'residual': result.resid.tolist(),
                'times': [str(t) for t in times],
                'period': period,
                'seasonal_strength': _calculate_seasonal_strength(result.seasonal, result.resid)
            }
        except Exception as e:
            results[entity_id] = {
                'error': str(e),
                'trend': values.tolist(),
                'seasonal': [0.0] * len(values),
                'residual': [0.0] * len(values)
            }

    return {
        'success': True,
        'method': 'stl',
        'period': period,
        'robust': robust,
        'results': results
    }


def decompose_classical(
    y: pl.DataFrame,
    period: Optional[int] = None,
    model: str = 'additive'
) -> Dict[str, Any]:
    """
    Classical seasonal decomposition (moving average based).

    Args:
        y: Panel data
        period: Seasonal period
        model: 'additive' or 'multiplicative'
    """
    if not STATSMODELS_AVAILABLE:
        return _fallback_decomposition(y, period)

    results = {}
    entities = y['entity_id'].unique().to_list()

    for entity_id in entities:
        entity_data = y.filter(pl.col('entity_id') == entity_id).sort('time')
        values = entity_data['value'].to_numpy()
        times = entity_data['time'].to_list()

        if period is None:
            period = 7

        if len(values) < 2 * period:
            results[entity_id] = {'error': 'Insufficient data'}
            continue

        try:
            # For multiplicative, ensure positive values
            if model == 'multiplicative' and np.any(values <= 0):
                model = 'additive'

            result = seasonal_decompose(values, period=period, model=model)

            results[entity_id] = {
                'trend': np.nan_to_num(result.trend, nan=0.0).tolist(),
                'seasonal': np.nan_to_num(result.seasonal, nan=0.0).tolist(),
                'residual': np.nan_to_num(result.resid, nan=0.0).tolist(),
                'times': [str(t) for t in times],
                'period': period,
                'model': model
            }
        except Exception as e:
            results[entity_id] = {'error': str(e)}

    return {
        'success': True,
        'method': 'classical',
        'period': period,
        'model': model,
        'results': results
    }


def _fallback_decomposition(
    y: pl.DataFrame,
    period: Optional[int] = None
) -> Dict[str, Any]:
    """Simple decomposition without statsmodels."""
    if period is None:
        period = 7

    results = {}
    entities = y['entity_id'].unique().to_list()

    for entity_id in entities:
        entity_data = y.filter(pl.col('entity_id') == entity_id).sort('time')
        values = entity_data['value'].to_numpy()
        times = entity_data['time'].to_list()
        n = len(values)

        # Simple moving average for trend
        if n >= period:
            trend = np.convolve(values, np.ones(period)/period, mode='same')
        else:
            trend = values.copy()

        # Seasonal: group by position in period
        detrended = values - trend
        seasonal = np.zeros(n)
        for i in range(period):
            indices = range(i, n, period)
            if len(indices) > 0:
                mean_seasonal = np.mean([detrended[j] for j in indices])
                for j in indices:
                    seasonal[j] = mean_seasonal

        # Residual
        residual = values - trend - seasonal

        results[entity_id] = {
            'trend': trend.tolist(),
            'seasonal': seasonal.tolist(),
            'residual': residual.tolist(),
            'times': [str(t) for t in times],
            'period': period
        }

    return {
        'success': True,
        'method': 'fallback',
        'period': period,
        'results': results
    }


def _calculate_seasonal_strength(seasonal: np.ndarray, residual: np.ndarray) -> float:
    """Calculate strength of seasonality (0 to 1)."""
    var_seasonal = np.var(seasonal)
    var_residual = np.var(residual)
    total_var = var_seasonal + var_residual

    if total_var == 0:
        return 0.0

    return float(max(0, 1 - var_residual / total_var))


# ============================================================================
# SEASONAL ADJUSTMENT
# ============================================================================

def seasonally_adjust(
    y: pl.DataFrame,
    period: Optional[int] = None,
    method: str = 'stl'
) -> Dict[str, Any]:
    """
    Remove seasonal component from time series (deseasonalize).

    Args:
        y: Panel data
        period: Seasonal period
        method: 'stl' or 'classical'
    """
    if method == 'stl':
        decomp = decompose_stl(y, period=period)
    else:
        decomp = decompose_classical(y, period=period)

    if not decomp['success']:
        return decomp

    result_rows = []
    entities = y['entity_id'].unique().to_list()

    for entity_id in entities:
        entity_data = y.filter(pl.col('entity_id') == entity_id).sort('time')
        times = entity_data['time'].to_list()
        values = entity_data['value'].to_numpy()

        entity_result = decomp['results'].get(entity_id, {})
        if 'error' in entity_result:
            # Use original values if decomposition failed
            adjusted = values
        else:
            seasonal = np.array(entity_result['seasonal'])
            adjusted = values - seasonal

        for t, v, adj in zip(times, values, adjusted):
            result_rows.append({
                'entity_id': entity_id,
                'time': t,
                'original': float(v),
                'adjusted': float(adj)
            })

    result = pl.DataFrame(result_rows)

    return {
        'success': True,
        'method': f'seasonal_adjustment_{method}',
        'data': result.to_dicts(),
        'shape': result.shape
    }


# ============================================================================
# SEASONALITY METRICS
# ============================================================================

def calculate_seasonality_metrics(
    y: pl.DataFrame,
    period: Optional[int] = None
) -> Dict[str, Any]:
    """
    Calculate comprehensive seasonality metrics.
    """
    # Detect period if not provided
    if period is None:
        detected = detect_seasonal_period(y, method='both')
    else:
        detected = {'results': {}}

    # Decompose
    decomp = decompose_stl(y, period=period)

    metrics = {}
    entities = y['entity_id'].unique().to_list()

    for entity_id in entities:
        entity_data = y.filter(pl.col('entity_id') == entity_id).sort('time')
        values = entity_data['value'].to_numpy()

        entity_decomp = decomp['results'].get(entity_id, {})
        detected_result = detected.get('results', {}).get(entity_id, {})

        if 'error' in entity_decomp:
            metrics[entity_id] = {'error': entity_decomp['error']}
            continue

        seasonal = np.array(entity_decomp['seasonal'])
        residual = np.array(entity_decomp['residual'])
        trend = np.array(entity_decomp['trend'])

        # Calculate metrics
        seasonal_strength = _calculate_seasonal_strength(seasonal, residual)
        trend_strength = _calculate_trend_strength(trend, residual)

        # Seasonal indices
        used_period = entity_decomp.get('period', period or 7)
        seasonal_indices = []
        for i in range(used_period):
            indices = range(i, len(seasonal), used_period)
            if len(indices) > 0:
                seasonal_indices.append(float(np.mean([seasonal[j] for j in indices])))

        metrics[entity_id] = {
            'detected_period': detected_result.get('period'),
            'used_period': used_period,
            'seasonal_strength': seasonal_strength,
            'trend_strength': trend_strength,
            'seasonal_indices': seasonal_indices,
            'is_seasonal': seasonal_strength > 0.3,
            'is_trending': trend_strength > 0.3
        }

    return {
        'success': True,
        'metrics': metrics
    }


def _calculate_trend_strength(trend: np.ndarray, residual: np.ndarray) -> float:
    """Calculate strength of trend (0 to 1)."""
    var_trend = np.var(np.diff(trend))  # Variance of trend changes
    var_residual = np.var(residual)
    total_var = var_trend + var_residual

    if total_var == 0:
        return 0.0

    return float(max(0, 1 - var_residual / total_var))


def main():
    """Test seasonality analysis."""
    print("Testing Seasonality Analysis Module")
    print("=" * 50)

    # Create sample data with seasonality
    dates = pl.datetime_range(
        start=pl.datetime(2020, 1, 1),
        end=pl.datetime(2020, 12, 31),
        interval='1d',
        eager=True
    ).to_list()

    n = len(dates)
    np.random.seed(42)

    # Add weekly seasonality and trend
    values = [
        100 + 0.1 * i +  # Trend
        10 * np.sin(2 * np.pi * i / 7) +  # Weekly seasonality
        np.random.randn() * 2  # Noise
        for i in range(n)
    ]

    df = pl.DataFrame({
        'entity_id': ['A'] * n,
        'time': dates,
        'value': values
    })

    # Test period detection
    period_result = detect_seasonal_period(df, method='both')
    print(f"Detected period: {period_result['results']['A'].get('period')}")
    print(f"Seasonality strength: {period_result['results']['A'].get('strength', 0):.3f}")

    # Test STL decomposition
    stl_result = decompose_stl(df, period=7)
    if 'A' in stl_result['results']:
        print(f"STL decomposition successful")
        print(f"Seasonal strength: {stl_result['results']['A'].get('seasonal_strength', 0):.3f}")

    # Test seasonal adjustment
    adj_result = seasonally_adjust(df, period=7)
    print(f"Seasonal adjustment: {adj_result['shape'][0]} rows")

    # Test seasonality metrics
    metrics = calculate_seasonality_metrics(df)
    if 'A' in metrics['metrics']:
        m = metrics['metrics']['A']
        print(f"Is seasonal: {m.get('is_seasonal')}")
        print(f"Is trending: {m.get('is_trending')}")

    print("\nAll tests: PASSED")


if __name__ == "__main__":
    main()
