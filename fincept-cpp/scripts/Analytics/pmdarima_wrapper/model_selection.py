import pandas as pd
import numpy as np
from typing import Dict, List, Optional, Union, Any
import json
from pmdarima import model_selection, ARIMA

def split_train_test(
    y: Union[List, np.ndarray, pd.Series],
    test_size: Union[int, float] = 0.2
) -> Dict[str, Any]:
    """Split time series into train and test sets"""
    y = pd.Series(y) if not isinstance(y, pd.Series) else y

    train, test = model_selection.train_test_split(y, test_size=test_size)

    return {
        'train': train.tolist() if hasattr(train, 'tolist') else list(train),
        'test': test.tolist() if hasattr(test, 'tolist') else list(test),
        'train_size': len(train),
        'test_size': len(test)
    }

def cross_validate_arima(
    y: Union[List, np.ndarray, pd.Series],
    order: tuple = (1, 1, 1),
    cv_splits: int = 3,
    step: int = 1
) -> Dict[str, Any]:
    """Cross-validate ARIMA model"""
    y = pd.Series(y) if not isinstance(y, pd.Series) else y

    model = ARIMA(order=order)
    cv = model_selection.SlidingWindowForecastCV(h=step, step=step, window_size=len(y) // cv_splits)

    scores = model_selection.cross_val_score(model, y, cv=cv, scoring='mean_squared_error')

    return {
        'scores': scores.tolist() if hasattr(scores, 'tolist') else list(scores),
        'mean_score': float(np.mean(scores)),
        'std_score': float(np.std(scores))
    }

def rolling_forecast_cv(
    y: Union[List, np.ndarray, pd.Series],
    order: tuple = (1, 1, 1),
    h: int = 1,
    step: int = 1,
    initial_window: Optional[int] = None
) -> Dict[str, Any]:
    """Rolling forecast cross-validation"""
    y = pd.Series(y) if not isinstance(y, pd.Series) else y

    if initial_window is None:
        initial_window = len(y) // 2

    model = ARIMA(order=order)
    cv = model_selection.RollingForecastCV(h=h, step=step, initial=initial_window)

    predictions = model_selection.cross_val_predict(model, y, cv=cv)

    return {
        'predictions': predictions.tolist() if hasattr(predictions, 'tolist') else list(predictions),
        'n_predictions': len(predictions)
    }

def sliding_window_cv(
    y: Union[List, np.ndarray, pd.Series],
    order: tuple = (1, 1, 1),
    h: int = 1,
    window_size: int = 50,
    step: int = 1
) -> Dict[str, Any]:
    """Sliding window cross-validation"""
    y = pd.Series(y) if not isinstance(y, pd.Series) else y

    model = ARIMA(order=order)
    cv = model_selection.SlidingWindowForecastCV(h=h, step=step, window_size=window_size)

    predictions = model_selection.cross_val_predict(model, y, cv=cv)

    return {
        'predictions': predictions.tolist() if hasattr(predictions, 'tolist') else list(predictions),
        'n_predictions': len(predictions)
    }

def main():
    print("Testing pmdarima model_selection wrapper")

    np.random.seed(42)
    y = np.cumsum(np.random.randn(100)) + 50

    split_result = split_train_test(y, test_size=0.2)
    print("Train size: {}, Test size: {}".format(split_result['train_size'], split_result['test_size']))

    cv_result = cross_validate_arima(y, order=(1, 1, 1), cv_splits=3)
    print("CV mean score: {:.4f}, std: {:.4f}".format(cv_result['mean_score'], cv_result['std_score']))

    rolling_result = rolling_forecast_cv(y, order=(1, 0, 0), h=1, initial_window=60)
    print("Rolling predictions: {}".format(rolling_result['n_predictions']))

    sliding_result = sliding_window_cv(y, order=(1, 0, 0), window_size=50)
    print("Sliding window predictions: {}".format(sliding_result['n_predictions']))

    print("Test: PASSED")

if __name__ == "__main__":
    main()
