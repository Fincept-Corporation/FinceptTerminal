import pandas as pd
import numpy as np
from typing import Dict, List, Optional, Union, Any
import json
from pmdarima.preprocessing import BoxCoxEndogTransformer, LogEndogTransformer, DateFeaturizer, FourierFeaturizer

def apply_boxcox_transform(
    y: Union[List, np.ndarray, pd.Series],
    lmbda: Optional[float] = None
) -> Dict[str, Any]:
    """Apply Box-Cox transformation to time series"""
    y = pd.Series(y) if not isinstance(y, pd.Series) else y

    transformer = BoxCoxEndogTransformer(lmbda=lmbda)
    y_transformed = transformer.fit_transform(y)

    lmbda_value = transformer.lmbda if transformer.lmbda is not None else 0.0

    return {
        'transformed': y_transformed.tolist() if hasattr(y_transformed, 'tolist') else list(y_transformed),
        'lambda': float(lmbda_value)
    }

def inverse_boxcox_transform(
    y_transformed: Union[List, np.ndarray, pd.Series],
    lmbda: float
) -> Dict[str, Any]:
    """Inverse Box-Cox transformation"""
    y_transformed = pd.Series(y_transformed) if not isinstance(y_transformed, pd.Series) else y_transformed

    from scipy import special
    if lmbda == 0:
        y_original = np.exp(y_transformed)
    else:
        y_original = np.power(lmbda * y_transformed + 1, 1 / lmbda)

    return {
        'original': y_original.tolist() if hasattr(y_original, 'tolist') else list(y_original)
    }

def apply_log_transform(
    y: Union[List, np.ndarray, pd.Series],
    lmbda: float = 0.0
) -> Dict[str, Any]:
    """Apply logarithmic transformation"""
    y = pd.Series(y) if not isinstance(y, pd.Series) else y

    transformer = LogEndogTransformer(lmbda=lmbda)
    y_transformed = transformer.fit_transform(y)

    return {
        'transformed': y_transformed.tolist() if hasattr(y_transformed, 'tolist') else list(y_transformed)
    }

def inverse_log_transform(
    y_transformed: Union[List, np.ndarray, pd.Series],
    lmbda: float = 0.0
) -> Dict[str, Any]:
    """Inverse logarithmic transformation"""
    y_transformed = pd.Series(y_transformed) if not isinstance(y_transformed, pd.Series) else y_transformed

    transformer = LogEndogTransformer(lmbda=lmbda)
    y_original = transformer.inverse_transform(y_transformed)

    return {
        'original': y_original.tolist() if hasattr(y_original, 'tolist') else list(y_original)
    }

def create_date_features(
    dates: Union[pd.DatetimeIndex, pd.Series, List],
    prefix: str = 'date'
) -> Dict[str, Any]:
    """Extract date features from datetime index"""
    if isinstance(dates, list):
        dates = pd.DatetimeIndex(dates)
    elif isinstance(dates, pd.Series):
        dates = pd.DatetimeIndex(dates)

    featurizer = DateFeaturizer(prefix=prefix)
    features = featurizer.fit_transform(None, dates)

    return {
        'features': features.to_dict(orient='list'),
        'feature_names': features.columns.tolist()
    }

def create_fourier_features(
    dates: Union[pd.DatetimeIndex, pd.Series, List],
    m: int = 12,
    k: int = 4
) -> Dict[str, Any]:
    """Create Fourier features for seasonality"""
    if isinstance(dates, list):
        dates = pd.DatetimeIndex(dates)
    elif isinstance(dates, pd.Series):
        dates = pd.DatetimeIndex(dates)

    featurizer = FourierFeaturizer(m=m, k=k)
    features = featurizer.fit_transform(None, dates)

    return {
        'features': features.to_dict(orient='list'),
        'feature_names': features.columns.tolist()
    }

def main():
    print("Testing pmdarima preprocessing wrapper")

    y = np.array([10, 15, 20, 25, 30, 35, 40])

    boxcox_result = apply_boxcox_transform(y, lmbda=0.5)
    print("Box-Cox lambda: {:.4f}, transformed count: {}".format(
        boxcox_result['lambda'],
        len(boxcox_result['transformed'])
    ))

    log_result = apply_log_transform(y)
    print("Log transform count: {}".format(len(log_result['transformed'])))

    print("Test: PASSED")

if __name__ == "__main__":
    main()
