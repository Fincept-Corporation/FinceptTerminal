import polars as pl
import numpy as np
from typing import Dict, List, Optional, Union, Any
import json
from functime.forecasting import (
    LinearModel, Lasso, Ridge, ElasticNet, KNN, LightGBM,
    AutoLinearModel, AutoLasso, AutoRidge, AutoElasticNet, AutoKNN, AutoLightGBM
)

# Linear Models

def fit_linear_model(
    y_train: pl.DataFrame,
    X_train: Optional[pl.DataFrame] = None,
    freq: str = '1d'
) -> Dict[str, Any]:
    """Fit Linear Regression forecaster"""
    model = LinearModel(freq=freq)
    model.fit(y=y_train, X=X_train)

    return {
        'model_type': 'LinearModel',
        'freq': freq,
        'fitted': True
    }

def forecast_linear_model(
    y_train: pl.DataFrame,
    fh: int,
    X_train: Optional[pl.DataFrame] = None,
    X_future: Optional[pl.DataFrame] = None,
    freq: str = '1d'
) -> Dict[str, Any]:
    """Fit and forecast with Linear Regression"""
    model = LinearModel(freq=freq)
    model.fit(y=y_train, X=X_train)
    forecast = model.predict(fh=fh, X=X_future)

    return {
        'forecast': forecast.to_dicts(),
        'shape': forecast.shape,
        'horizon': fh
    }

def fit_lasso(
    y_train: pl.DataFrame,
    X_train: Optional[pl.DataFrame] = None,
    freq: str = '1d',
    alpha: float = 1.0
) -> Dict[str, Any]:
    """Fit Lasso forecaster"""
    model = Lasso(freq=freq, alpha=alpha)
    model.fit(y=y_train, X=X_train)

    return {
        'model_type': 'Lasso',
        'freq': freq,
        'alpha': alpha,
        'fitted': True
    }

def forecast_lasso(
    y_train: pl.DataFrame,
    fh: int,
    X_train: Optional[pl.DataFrame] = None,
    X_future: Optional[pl.DataFrame] = None,
    freq: str = '1d',
    alpha: float = 1.0
) -> Dict[str, Any]:
    """Fit and forecast with Lasso"""
    model = Lasso(freq=freq, alpha=alpha)
    model.fit(y=y_train, X=X_train)
    forecast = model.predict(fh=fh, X=X_future)

    return {
        'forecast': forecast.to_dicts(),
        'shape': forecast.shape,
        'horizon': fh,
        'alpha': alpha
    }

def fit_ridge(
    y_train: pl.DataFrame,
    X_train: Optional[pl.DataFrame] = None,
    freq: str = '1d',
    alpha: float = 1.0
) -> Dict[str, Any]:
    """Fit Ridge forecaster"""
    model = Ridge(freq=freq, alpha=alpha)
    model.fit(y=y_train, X=X_train)

    return {
        'model_type': 'Ridge',
        'freq': freq,
        'alpha': alpha,
        'fitted': True
    }

def forecast_ridge(
    y_train: pl.DataFrame,
    fh: int,
    X_train: Optional[pl.DataFrame] = None,
    X_future: Optional[pl.DataFrame] = None,
    freq: str = '1d',
    alpha: float = 1.0
) -> Dict[str, Any]:
    """Fit and forecast with Ridge"""
    model = Ridge(freq=freq, alpha=alpha)
    model.fit(y=y_train, X=X_train)
    forecast = model.predict(fh=fh, X=X_future)

    return {
        'forecast': forecast.to_dicts(),
        'shape': forecast.shape,
        'horizon': fh,
        'alpha': alpha
    }

def fit_elasticnet(
    y_train: pl.DataFrame,
    X_train: Optional[pl.DataFrame] = None,
    freq: str = '1d',
    alpha: float = 1.0,
    l1_ratio: float = 0.5
) -> Dict[str, Any]:
    """Fit ElasticNet forecaster"""
    model = ElasticNet(freq=freq, alpha=alpha, l1_ratio=l1_ratio)
    model.fit(y=y_train, X=X_train)

    return {
        'model_type': 'ElasticNet',
        'freq': freq,
        'alpha': alpha,
        'l1_ratio': l1_ratio,
        'fitted': True
    }

def forecast_elasticnet(
    y_train: pl.DataFrame,
    fh: int,
    X_train: Optional[pl.DataFrame] = None,
    X_future: Optional[pl.DataFrame] = None,
    freq: str = '1d',
    alpha: float = 1.0,
    l1_ratio: float = 0.5
) -> Dict[str, Any]:
    """Fit and forecast with ElasticNet"""
    model = ElasticNet(freq=freq, alpha=alpha, l1_ratio=l1_ratio)
    model.fit(y=y_train, X=X_train)
    forecast = model.predict(fh=fh, X=X_future)

    return {
        'forecast': forecast.to_dicts(),
        'shape': forecast.shape,
        'horizon': fh,
        'alpha': alpha,
        'l1_ratio': l1_ratio
    }

# KNN

def fit_knn(
    y_train: pl.DataFrame,
    X_train: Optional[pl.DataFrame] = None,
    freq: str = '1d',
    n_neighbors: int = 5
) -> Dict[str, Any]:
    """Fit KNN forecaster"""
    model = KNN(freq=freq, n_neighbors=n_neighbors)
    model.fit(y=y_train, X=X_train)

    return {
        'model_type': 'KNN',
        'freq': freq,
        'n_neighbors': n_neighbors,
        'fitted': True
    }

def forecast_knn(
    y_train: pl.DataFrame,
    fh: int,
    X_train: Optional[pl.DataFrame] = None,
    X_future: Optional[pl.DataFrame] = None,
    freq: str = '1d',
    n_neighbors: int = 5
) -> Dict[str, Any]:
    """Fit and forecast with KNN"""
    model = KNN(freq=freq, n_neighbors=n_neighbors)
    model.fit(y=y_train, X=X_train)
    forecast = model.predict(fh=fh, X=X_future)

    return {
        'forecast': forecast.to_dicts(),
        'shape': forecast.shape,
        'horizon': fh,
        'n_neighbors': n_neighbors
    }

# LightGBM

def fit_lightgbm(
    y_train: pl.DataFrame,
    X_train: Optional[pl.DataFrame] = None,
    freq: str = '1d',
    **params
) -> Dict[str, Any]:
    """Fit LightGBM forecaster"""
    model = LightGBM(freq=freq, **params)
    model.fit(y=y_train, X=X_train)

    return {
        'model_type': 'LightGBM',
        'freq': freq,
        'params': params,
        'fitted': True
    }

def forecast_lightgbm(
    y_train: pl.DataFrame,
    fh: int,
    X_train: Optional[pl.DataFrame] = None,
    X_future: Optional[pl.DataFrame] = None,
    freq: str = '1d',
    **params
) -> Dict[str, Any]:
    """Fit and forecast with LightGBM"""
    model = LightGBM(freq=freq, **params)
    model.fit(y=y_train, X=X_train)
    forecast = model.predict(fh=fh, X=X_future)

    return {
        'forecast': forecast.to_dicts(),
        'shape': forecast.shape,
        'horizon': fh,
        'params': params
    }

# Auto Models (with hyperparameter tuning)

def auto_linear_model(
    y_train: pl.DataFrame,
    fh: int,
    X_train: Optional[pl.DataFrame] = None,
    X_future: Optional[pl.DataFrame] = None,
    freq: str = '1d',
    **tuning_params
) -> Dict[str, Any]:
    """Auto-tune and forecast with Linear Model"""
    model = AutoLinearModel(freq=freq, **tuning_params)
    forecast = model.fit_predict(y=y_train, fh=fh, X=X_train, X_future=X_future)

    return {
        'forecast': forecast.to_dicts(),
        'shape': forecast.shape,
        'horizon': fh,
        'best_params': model.best_params if hasattr(model, 'best_params') else None
    }

def auto_lasso(
    y_train: pl.DataFrame,
    fh: int,
    X_train: Optional[pl.DataFrame] = None,
    X_future: Optional[pl.DataFrame] = None,
    freq: str = '1d',
    **tuning_params
) -> Dict[str, Any]:
    """Auto-tune and forecast with Lasso"""
    model = AutoLasso(freq=freq, **tuning_params)
    forecast = model.fit_predict(y=y_train, fh=fh, X=X_train, X_future=X_future)

    return {
        'forecast': forecast.to_dicts(),
        'shape': forecast.shape,
        'horizon': fh,
        'best_params': model.best_params if hasattr(model, 'best_params') else None
    }

def auto_ridge(
    y_train: pl.DataFrame,
    fh: int,
    X_train: Optional[pl.DataFrame] = None,
    X_future: Optional[pl.DataFrame] = None,
    freq: str = '1d',
    **tuning_params
) -> Dict[str, Any]:
    """Auto-tune and forecast with Ridge"""
    model = AutoRidge(freq=freq, **tuning_params)
    forecast = model.fit_predict(y=y_train, fh=fh, X=X_train, X_future=X_future)

    return {
        'forecast': forecast.to_dicts(),
        'shape': forecast.shape,
        'horizon': fh,
        'best_params': model.best_params if hasattr(model, 'best_params') else None
    }

def auto_elasticnet(
    y_train: pl.DataFrame,
    fh: int,
    X_train: Optional[pl.DataFrame] = None,
    X_future: Optional[pl.DataFrame] = None,
    freq: str = '1d',
    **tuning_params
) -> Dict[str, Any]:
    """Auto-tune and forecast with ElasticNet"""
    model = AutoElasticNet(freq=freq, **tuning_params)
    forecast = model.fit_predict(y=y_train, fh=fh, X=X_train, X_future=X_future)

    return {
        'forecast': forecast.to_dicts(),
        'shape': forecast.shape,
        'horizon': fh,
        'best_params': model.best_params if hasattr(model, 'best_params') else None
    }

def auto_knn(
    y_train: pl.DataFrame,
    fh: int,
    X_train: Optional[pl.DataFrame] = None,
    X_future: Optional[pl.DataFrame] = None,
    freq: str = '1d',
    **tuning_params
) -> Dict[str, Any]:
    """Auto-tune and forecast with KNN"""
    model = AutoKNN(freq=freq, **tuning_params)
    forecast = model.fit_predict(y=y_train, fh=fh, X=X_train, X_future=X_future)

    return {
        'forecast': forecast.to_dicts(),
        'shape': forecast.shape,
        'horizon': fh,
        'best_params': model.best_params if hasattr(model, 'best_params') else None
    }

def auto_lightgbm(
    y_train: pl.DataFrame,
    fh: int,
    X_train: Optional[pl.DataFrame] = None,
    X_future: Optional[pl.DataFrame] = None,
    freq: str = '1d',
    **tuning_params
) -> Dict[str, Any]:
    """Auto-tune and forecast with LightGBM"""
    model = AutoLightGBM(freq=freq, **tuning_params)
    forecast = model.fit_predict(y=y_train, fh=fh, X=X_train, X_future=X_future)

    return {
        'forecast': forecast.to_dicts(),
        'shape': forecast.shape,
        'horizon': fh,
        'best_params': model.best_params if hasattr(model, 'best_params') else None
    }

def main():
    print("Testing functime forecasting wrapper")

    # Create sample panel data
    df = pl.DataFrame({
        'entity_id': ['A'] * 10,
        'time': pl.datetime_range(
            start=pl.datetime(2020, 1, 1),
            end=pl.datetime(2020, 1, 10),
            interval='1d',
            eager=True
        ).to_list(),
        'value': [10.0, 12.0, 15.0, 14.0, 18.0, 20.0, 22.0, 21.0, 25.0, 28.0]
    })

    # Test Linear Model
    linear_result = forecast_linear_model(df, fh=3, freq='1d')
    print("Linear forecast shape: {}".format(linear_result['shape']))

    # Test Lasso
    lasso_result = forecast_lasso(df, fh=3, freq='1d', alpha=0.1)
    print("Lasso forecast shape: {}".format(lasso_result['shape']))

    # Test Ridge
    ridge_result = forecast_ridge(df, fh=3, freq='1d', alpha=0.1)
    print("Ridge forecast shape: {}".format(ridge_result['shape']))

    print("Test: PASSED")

if __name__ == "__main__":
    main()
