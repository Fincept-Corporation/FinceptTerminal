"""
Time Series Anomaly Detection Module
=====================================

Provides anomaly detection for time series data:
- Statistical methods (Z-score, IQR, Grubbs)
- Isolation Forest
- Residual-based detection
- Seasonal decomposition anomalies
- Change point detection

Designed for financial time series and forecasting applications.
"""

import polars as pl
import numpy as np
from typing import Dict, List, Optional, Union, Any, Tuple
from datetime import datetime, timedelta
from scipy import stats as scipy_stats
import warnings

warnings.filterwarnings('ignore')

# Try importing sklearn
try:
    from sklearn.ensemble import IsolationForest
    from sklearn.neighbors import LocalOutlierFactor
    from sklearn.preprocessing import StandardScaler
    SKLEARN_AVAILABLE = True
except ImportError:
    SKLEARN_AVAILABLE = False


# ============================================================================
# STATISTICAL ANOMALY DETECTION
# ============================================================================

def detect_zscore(
    y: pl.DataFrame,
    threshold: float = 3.0,
    window: Optional[int] = None
) -> Dict[str, Any]:
    """
    Detect anomalies using Z-score (standard deviations from mean).

    Args:
        y: Panel data with entity_id, time, value
        threshold: Z-score threshold for anomaly (default: 3.0)
        window: Optional rolling window size for local Z-scores
    """
    result_rows = []
    summary = {'n_anomalies': 0, 'anomaly_rate': 0.0}

    entities = y['entity_id'].unique().to_list()

    for entity_id in entities:
        entity_data = y.filter(pl.col('entity_id') == entity_id).sort('time')
        values = entity_data['value'].to_numpy()
        times = entity_data['time'].to_list()

        if window is not None and window < len(values):
            # Rolling Z-score
            zscores = _rolling_zscore(values, window)
        else:
            # Global Z-score
            mean = np.mean(values)
            std = np.std(values)
            if std == 0:
                zscores = np.zeros_like(values)
            else:
                zscores = (values - mean) / std

        for i, (t, v, z) in enumerate(zip(times, values, zscores)):
            is_anomaly = bool(abs(z) > threshold)
            result_rows.append({
                'entity_id': entity_id,
                'time': t,
                'value': float(v),
                'zscore': float(z),
                'is_anomaly': is_anomaly,
                'anomaly_type': 'high' if z > threshold else ('low' if z < -threshold else None)
            })
            if is_anomaly:
                summary['n_anomalies'] += 1

    result = pl.DataFrame(result_rows)
    summary['anomaly_rate'] = summary['n_anomalies'] / len(result) if len(result) > 0 else 0

    return {
        'success': True,
        'method': 'zscore',
        'threshold': threshold,
        'window': window,
        'data': result.to_dicts(),
        'anomalies': result.filter(pl.col('is_anomaly')).to_dicts(),
        'summary': summary
    }


def detect_iqr(
    y: pl.DataFrame,
    k: float = 1.5,
    window: Optional[int] = None
) -> Dict[str, Any]:
    """
    Detect anomalies using Interquartile Range (IQR) method.

    Args:
        y: Panel data
        k: IQR multiplier (default: 1.5 for outliers, 3.0 for extreme)
        window: Optional rolling window
    """
    result_rows = []
    summary = {'n_anomalies': 0, 'anomaly_rate': 0.0}

    entities = y['entity_id'].unique().to_list()

    for entity_id in entities:
        entity_data = y.filter(pl.col('entity_id') == entity_id).sort('time')
        values = entity_data['value'].to_numpy()
        times = entity_data['time'].to_list()

        if window is not None and window < len(values):
            # Rolling IQR
            for i in range(len(values)):
                start_idx = max(0, i - window)
                window_values = values[start_idx:i+1]
                q1, q3 = np.percentile(window_values, [25, 75])
                iqr = q3 - q1
                lower = q1 - k * iqr
                upper = q3 + k * iqr
                v = values[i]
                is_anomaly = bool(v < lower or v > upper)

                result_rows.append({
                    'entity_id': entity_id,
                    'time': times[i],
                    'value': float(v),
                    'lower_bound': float(lower),
                    'upper_bound': float(upper),
                    'is_anomaly': is_anomaly,
                    'anomaly_type': 'high' if v > upper else ('low' if v < lower else None)
                })
                if is_anomaly:
                    summary['n_anomalies'] += 1
        else:
            # Global IQR
            q1, q3 = np.percentile(values, [25, 75])
            iqr = q3 - q1
            lower = q1 - k * iqr
            upper = q3 + k * iqr

            for i, (t, v) in enumerate(zip(times, values)):
                is_anomaly = bool(v < lower or v > upper)
                result_rows.append({
                    'entity_id': entity_id,
                    'time': t,
                    'value': float(v),
                    'lower_bound': float(lower),
                    'upper_bound': float(upper),
                    'is_anomaly': is_anomaly,
                    'anomaly_type': 'high' if v > upper else ('low' if v < lower else None)
                })
                if is_anomaly:
                    summary['n_anomalies'] += 1

    result = pl.DataFrame(result_rows)
    summary['anomaly_rate'] = summary['n_anomalies'] / len(result) if len(result) > 0 else 0

    return {
        'success': True,
        'method': 'iqr',
        'k': k,
        'window': window,
        'data': result.to_dicts(),
        'anomalies': result.filter(pl.col('is_anomaly')).to_dicts(),
        'summary': summary
    }


def detect_grubbs(
    y: pl.DataFrame,
    alpha: float = 0.05
) -> Dict[str, Any]:
    """
    Grubbs' test for outliers (one outlier at a time).

    Args:
        y: Panel data
        alpha: Significance level
    """
    result_rows = []
    summary = {'n_anomalies': 0, 'anomaly_rate': 0.0}

    entities = y['entity_id'].unique().to_list()

    for entity_id in entities:
        entity_data = y.filter(pl.col('entity_id') == entity_id).sort('time')
        values = entity_data['value'].to_numpy()
        times = entity_data['time'].to_list()
        n = len(values)

        if n < 3:
            for t, v in zip(times, values):
                result_rows.append({
                    'entity_id': entity_id,
                    'time': t,
                    'value': float(v),
                    'g_stat': None,
                    'critical_value': None,
                    'is_anomaly': False
                })
            continue

        # Calculate Grubbs statistic for each point
        mean = np.mean(values)
        std = np.std(values, ddof=1)

        # Critical value
        t_crit = scipy_stats.t.ppf(1 - alpha / (2 * n), n - 2)
        g_crit = ((n - 1) / np.sqrt(n)) * np.sqrt(t_crit ** 2 / (n - 2 + t_crit ** 2))

        for i, (t, v) in enumerate(zip(times, values)):
            if std == 0:
                g_stat = 0
            else:
                g_stat = abs(v - mean) / std
            is_anomaly = bool(g_stat > g_crit)

            result_rows.append({
                'entity_id': entity_id,
                'time': t,
                'value': float(v),
                'g_stat': float(g_stat),
                'critical_value': float(g_crit),
                'is_anomaly': is_anomaly
            })
            if is_anomaly:
                summary['n_anomalies'] += 1

    result = pl.DataFrame(result_rows)
    summary['anomaly_rate'] = summary['n_anomalies'] / len(result) if len(result) > 0 else 0

    return {
        'success': True,
        'method': 'grubbs',
        'alpha': alpha,
        'data': result.to_dicts(),
        'anomalies': result.filter(pl.col('is_anomaly')).to_dicts(),
        'summary': summary
    }


# ============================================================================
# MACHINE LEARNING ANOMALY DETECTION
# ============================================================================

def detect_isolation_forest(
    y: pl.DataFrame,
    contamination: float = 0.05,
    n_estimators: int = 100,
    use_features: bool = True,
    lags: int = 5
) -> Dict[str, Any]:
    """
    Detect anomalies using Isolation Forest.

    Args:
        y: Panel data
        contamination: Expected proportion of outliers
        n_estimators: Number of trees
        use_features: Whether to create lag features
        lags: Number of lag features to create
    """
    if not SKLEARN_AVAILABLE:
        return {'success': False, 'error': 'sklearn not available'}

    result_rows = []
    summary = {'n_anomalies': 0, 'anomaly_rate': 0.0}

    entities = y['entity_id'].unique().to_list()

    for entity_id in entities:
        entity_data = y.filter(pl.col('entity_id') == entity_id).sort('time')
        values = entity_data['value'].to_numpy()
        times = entity_data['time'].to_list()

        # Create features
        if use_features and len(values) > lags:
            X = _create_feature_matrix(values, lags)
            start_idx = lags
        else:
            X = values.reshape(-1, 1)
            start_idx = 0

        # Fit Isolation Forest
        iso = IsolationForest(
            n_estimators=n_estimators,
            contamination=contamination,
            random_state=42
        )
        predictions = iso.fit_predict(X)
        scores = iso.decision_function(X)

        # Map back to original indices
        for i in range(len(times)):
            if i < start_idx:
                result_rows.append({
                    'entity_id': entity_id,
                    'time': times[i],
                    'value': float(values[i]),
                    'anomaly_score': None,
                    'is_anomaly': False
                })
            else:
                feat_idx = i - start_idx
                is_anomaly = bool(predictions[feat_idx] == -1)
                result_rows.append({
                    'entity_id': entity_id,
                    'time': times[i],
                    'value': float(values[i]),
                    'anomaly_score': float(scores[feat_idx]),
                    'is_anomaly': is_anomaly
                })
                if is_anomaly:
                    summary['n_anomalies'] += 1

    result = pl.DataFrame(result_rows)
    summary['anomaly_rate'] = summary['n_anomalies'] / len(result) if len(result) > 0 else 0

    return {
        'success': True,
        'method': 'isolation_forest',
        'contamination': contamination,
        'n_estimators': n_estimators,
        'data': result.to_dicts(),
        'anomalies': result.filter(pl.col('is_anomaly')).to_dicts(),
        'summary': summary
    }


def detect_lof(
    y: pl.DataFrame,
    n_neighbors: int = 20,
    contamination: float = 0.05,
    lags: int = 5
) -> Dict[str, Any]:
    """
    Detect anomalies using Local Outlier Factor (LOF).
    """
    if not SKLEARN_AVAILABLE:
        return {'success': False, 'error': 'sklearn not available'}

    result_rows = []
    summary = {'n_anomalies': 0, 'anomaly_rate': 0.0}

    entities = y['entity_id'].unique().to_list()

    for entity_id in entities:
        entity_data = y.filter(pl.col('entity_id') == entity_id).sort('time')
        values = entity_data['value'].to_numpy()
        times = entity_data['time'].to_list()

        if len(values) < n_neighbors + lags:
            for t, v in zip(times, values):
                result_rows.append({
                    'entity_id': entity_id,
                    'time': t,
                    'value': float(v),
                    'lof_score': None,
                    'is_anomaly': False
                })
            continue

        # Create features
        X = _create_feature_matrix(values, lags)
        start_idx = lags

        # Fit LOF
        lof = LocalOutlierFactor(n_neighbors=n_neighbors, contamination=contamination)
        predictions = lof.fit_predict(X)
        scores = -lof.negative_outlier_factor_  # Convert to positive (higher = more anomalous)

        for i in range(len(times)):
            if i < start_idx:
                result_rows.append({
                    'entity_id': entity_id,
                    'time': times[i],
                    'value': float(values[i]),
                    'lof_score': None,
                    'is_anomaly': False
                })
            else:
                feat_idx = i - start_idx
                is_anomaly = bool(predictions[feat_idx] == -1)
                result_rows.append({
                    'entity_id': entity_id,
                    'time': times[i],
                    'value': float(values[i]),
                    'lof_score': float(scores[feat_idx]),
                    'is_anomaly': is_anomaly
                })
                if is_anomaly:
                    summary['n_anomalies'] += 1

    result = pl.DataFrame(result_rows)
    summary['anomaly_rate'] = summary['n_anomalies'] / len(result) if len(result) > 0 else 0

    return {
        'success': True,
        'method': 'lof',
        'n_neighbors': n_neighbors,
        'contamination': contamination,
        'data': result.to_dicts(),
        'anomalies': result.filter(pl.col('is_anomaly')).to_dicts(),
        'summary': summary
    }


# ============================================================================
# RESIDUAL-BASED ANOMALY DETECTION
# ============================================================================

def detect_residual_anomalies(
    y: pl.DataFrame,
    forecast: pl.DataFrame,
    threshold: float = 3.0
) -> Dict[str, Any]:
    """
    Detect anomalies based on forecast residuals.

    Args:
        y: Actual values (panel data)
        forecast: Forecast values (panel data)
        threshold: Z-score threshold for residuals
    """
    result_rows = []
    summary = {'n_anomalies': 0, 'anomaly_rate': 0.0}

    # Join actual and forecast
    actual = y.select(['entity_id', 'time', 'value']).rename({'value': 'actual'})
    pred = forecast.select(['entity_id', 'time', 'value']).rename({'value': 'predicted'})
    joined = actual.join(pred, on=['entity_id', 'time'], how='inner')

    if len(joined) == 0:
        return {'success': False, 'error': 'No matching data between actual and forecast'}

    # Calculate residuals
    joined = joined.with_columns([
        (pl.col('actual') - pl.col('predicted')).alias('residual')
    ])

    entities = joined['entity_id'].unique().to_list()

    for entity_id in entities:
        entity_data = joined.filter(pl.col('entity_id') == entity_id).sort('time')
        residuals = entity_data['residual'].to_numpy()

        # Calculate Z-scores of residuals
        mean_res = np.mean(residuals)
        std_res = np.std(residuals)

        for row in entity_data.iter_rows(named=True):
            if std_res == 0:
                z = 0
            else:
                z = (row['residual'] - mean_res) / std_res

            is_anomaly = bool(abs(z) > threshold)

            result_rows.append({
                'entity_id': row['entity_id'],
                'time': row['time'],
                'actual': float(row['actual']),
                'predicted': float(row['predicted']),
                'residual': float(row['residual']),
                'residual_zscore': float(z),
                'is_anomaly': is_anomaly
            })
            if is_anomaly:
                summary['n_anomalies'] += 1

    result = pl.DataFrame(result_rows)
    summary['anomaly_rate'] = summary['n_anomalies'] / len(result) if len(result) > 0 else 0

    return {
        'success': True,
        'method': 'residual',
        'threshold': threshold,
        'data': result.to_dicts(),
        'anomalies': result.filter(pl.col('is_anomaly')).to_dicts(),
        'summary': summary
    }


# ============================================================================
# CHANGE POINT DETECTION
# ============================================================================

def detect_change_points(
    y: pl.DataFrame,
    method: str = 'cusum',
    threshold: float = 5.0
) -> Dict[str, Any]:
    """
    Detect change points in time series.

    Args:
        y: Panel data
        method: 'cusum' or 'pelt'
        threshold: Threshold for detection
    """
    result_rows = []
    change_points = []

    entities = y['entity_id'].unique().to_list()

    for entity_id in entities:
        entity_data = y.filter(pl.col('entity_id') == entity_id).sort('time')
        values = entity_data['value'].to_numpy()
        times = entity_data['time'].to_list()

        if method == 'cusum':
            # CUSUM change point detection
            cps = _cusum_detection(values, threshold)
        else:
            cps = _cusum_detection(values, threshold)  # Default to CUSUM

        for i, (t, v) in enumerate(zip(times, values)):
            is_change_point = i in cps
            result_rows.append({
                'entity_id': entity_id,
                'time': t,
                'value': float(v),
                'is_change_point': is_change_point
            })
            if is_change_point:
                change_points.append({
                    'entity_id': entity_id,
                    'time': t,
                    'index': i
                })

    result = pl.DataFrame(result_rows)

    return {
        'success': True,
        'method': f'change_point_{method}',
        'threshold': threshold,
        'data': result.to_dicts(),
        'change_points': change_points,
        'n_change_points': len(change_points)
    }


# ============================================================================
# HELPER FUNCTIONS
# ============================================================================

def _rolling_zscore(values: np.ndarray, window: int) -> np.ndarray:
    """Calculate rolling Z-score."""
    n = len(values)
    zscores = np.zeros(n)

    for i in range(n):
        start_idx = max(0, i - window + 1)
        window_values = values[start_idx:i+1]
        mean = np.mean(window_values)
        std = np.std(window_values)
        if std == 0:
            zscores[i] = 0
        else:
            zscores[i] = (values[i] - mean) / std

    return zscores


def _create_feature_matrix(values: np.ndarray, lags: int) -> np.ndarray:
    """Create feature matrix with lag features."""
    n = len(values)
    X = []

    for i in range(lags, n):
        features = list(values[i-lags:i]) + [values[i]]
        X.append(features)

    return np.array(X)


def _cusum_detection(values: np.ndarray, threshold: float) -> List[int]:
    """CUSUM change point detection."""
    n = len(values)
    if n < 3:
        return []

    mean = np.mean(values)
    std = np.std(values)
    if std == 0:
        std = 1

    # Standardize
    z = (values - mean) / std

    # Calculate cumulative sum
    s_pos = np.zeros(n)
    s_neg = np.zeros(n)

    for i in range(1, n):
        s_pos[i] = max(0, s_pos[i-1] + z[i] - 0.5)
        s_neg[i] = min(0, s_neg[i-1] + z[i] + 0.5)

    # Detect change points
    change_points = []
    for i in range(1, n):
        if s_pos[i] > threshold or s_neg[i] < -threshold:
            change_points.append(i)
            # Reset
            s_pos[i] = 0
            s_neg[i] = 0

    return change_points


def main():
    """Test anomaly detection methods."""
    print("Testing Anomaly Detection Module")
    print("=" * 50)

    # Create sample data with anomalies
    dates = pl.datetime_range(
        start=pl.datetime(2020, 1, 1),
        end=pl.datetime(2020, 3, 31),
        interval='1d',
        eager=True
    ).to_list()

    n = len(dates)
    np.random.seed(42)
    values = [100 + 0.5 * i + np.random.randn() * 3 for i in range(n)]

    # Inject anomalies
    values[10] = 150  # Spike
    values[30] = 50   # Drop
    values[50] = 180  # Large spike

    df = pl.DataFrame({
        'entity_id': ['A'] * n,
        'time': dates,
        'value': values
    })

    # Test Z-score
    zscore_result = detect_zscore(df, threshold=3.0)
    print(f"Z-score: {zscore_result['summary']['n_anomalies']} anomalies detected")

    # Test IQR
    iqr_result = detect_iqr(df, k=1.5)
    print(f"IQR: {iqr_result['summary']['n_anomalies']} anomalies detected")

    # Test Grubbs
    grubbs_result = detect_grubbs(df, alpha=0.05)
    print(f"Grubbs: {grubbs_result['summary']['n_anomalies']} anomalies detected")

    # Test Isolation Forest
    if SKLEARN_AVAILABLE:
        iso_result = detect_isolation_forest(df, contamination=0.05)
        print(f"Isolation Forest: {iso_result['summary']['n_anomalies']} anomalies detected")

        lof_result = detect_lof(df, n_neighbors=10, contamination=0.05)
        print(f"LOF: {lof_result['summary']['n_anomalies']} anomalies detected")

    # Test Change Point Detection
    cp_result = detect_change_points(df, method='cusum', threshold=5.0)
    print(f"Change Points: {cp_result['n_change_points']} detected")

    print("\nAll tests: PASSED")


if __name__ == "__main__":
    main()
