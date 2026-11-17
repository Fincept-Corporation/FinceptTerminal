import math
import pandas as pd
import numpy as np
from typing import Dict, List, Optional, Union, Any, Tuple
import json
import statsmodels.api as sm
from statsmodels.tsa.api import ARIMA, SARIMAX, VARMAX, AutoReg, ARDL, ExponentialSmoothing, Holt, SimpleExpSmoothing, STL, STLForecast, DynamicFactor, UnobservedComponents, MarkovRegression, MarkovAutoregression, VECM


def _json_safe(obj):
    """Plain-Python copy of obj that json.dumps accepts: numpy/pandas values converted, NaN/inf -> None."""
    if isinstance(obj, dict):
        return {str(k): _json_safe(v) for k, v in obj.items()}
    if isinstance(obj, pd.DataFrame):
        return [_json_safe(row) for row in obj.values.tolist()]
    if isinstance(obj, (pd.Series, pd.Index)):
        return [_json_safe(v) for v in obj.tolist()]
    if isinstance(obj, np.ndarray):
        return _json_safe(obj.tolist())
    if isinstance(obj, (list, tuple)):
        return [_json_safe(v) for v in obj]
    if isinstance(obj, (bool, np.bool_)):
        return bool(obj)
    if isinstance(obj, (int, np.integer)):
        return int(obj)
    if isinstance(obj, (float, np.floating)):
        return float(obj) if math.isfinite(obj) else None
    return obj


def _holtwinters_params(params: Dict[str, Any]) -> Dict[str, Any]:
    """Holt-Winters results.params is a dict {name: value}; keep names AND values.

    Values that do not apply to the fitted model (e.g. damping_trend without a damped trend)
    are NaN in statsmodels and become null.
    """
    return _json_safe(dict(params))


def fit_arima(
    data: Union[List, np.ndarray, pd.Series],
    order: Tuple[int, int, int] = (1, 0, 0),
    seasonal_order: Optional[Tuple[int, int, int, int]] = None
) -> Dict[str, Any]:
    """Fit ARIMA model"""
    data = pd.Series(data) if not isinstance(data, pd.Series) else data

    if seasonal_order:
        model = SARIMAX(data, order=order, seasonal_order=seasonal_order)
    else:
        model = ARIMA(data, order=order)

    results = model.fit()

    return _json_safe({
        'params': results.params,
        'aic': float(results.aic),
        'bic': float(results.bic),
        'hqic': float(results.hqic),
        'resid': results.resid,
        'fittedvalues': results.fittedvalues
    })

def forecast_arima(
    data: Union[List, np.ndarray, pd.Series],
    order: Tuple[int, int, int],
    steps: int = 10
) -> Dict[str, Any]:
    """Fit ARIMA and forecast"""
    data = pd.Series(data) if not isinstance(data, pd.Series) else data

    model = ARIMA(data, order=order)
    results = model.fit()
    forecast = results.forecast(steps=steps)

    return _json_safe({
        'forecast': np.asarray(forecast),
        'aic': float(results.aic),
        'bic': float(results.bic)
    })

def fit_sarimax(
    data: Union[List, np.ndarray, pd.Series],
    order: Tuple[int, int, int] = (1, 0, 0),
    seasonal_order: Tuple[int, int, int, int] = (0, 0, 0, 0),
    exog: Optional[Union[np.ndarray, pd.DataFrame]] = None
) -> Dict[str, Any]:
    """Fit SARIMAX model"""
    data = pd.Series(data) if not isinstance(data, pd.Series) else data

    model = SARIMAX(data, order=order, seasonal_order=seasonal_order, exog=exog)
    results = model.fit()

    return _json_safe({
        'params': results.params,
        'aic': float(results.aic),
        'bic': float(results.bic),
        'hqic': float(results.hqic),
        'resid': results.resid,
        'fittedvalues': results.fittedvalues
    })

def fit_varmax(
    data: Union[np.ndarray, pd.DataFrame],
    order: Tuple[int, int] = (1, 0),
    trend: str = 'c'
) -> Dict[str, Any]:
    """Fit VARMAX model"""
    data = pd.DataFrame(data) if not isinstance(data, pd.DataFrame) else data

    model = VARMAX(data, order=order, trend=trend)
    results = model.fit()

    return _json_safe({
        'params': results.params,
        'aic': float(results.aic),
        'bic': float(results.bic),
        'hqic': float(results.hqic),
        'resid': np.asarray(results.resid)
    })

def fit_autoreg(
    data: Union[List, np.ndarray, pd.Series],
    lags: int = 1,
    trend: str = 'c'
) -> Dict[str, Any]:
    """Fit AutoReg model"""
    data = pd.Series(data) if not isinstance(data, pd.Series) else data

    model = AutoReg(data, lags=lags, trend=trend)
    results = model.fit()

    return _json_safe({
        'params': results.params,
        'aic': float(results.aic),
        'bic': float(results.bic),
        'hqic': float(results.hqic),
        'resid': results.resid,
        'fittedvalues': results.fittedvalues
    })

def fit_ardl(
    endog: Union[List, np.ndarray, pd.Series],
    exog: Union[np.ndarray, pd.DataFrame],
    lags: int = 1,
    order: Optional[Union[int, List[int]]] = None
) -> Dict[str, Any]:
    """Fit ARDL model"""
    endog = pd.Series(endog) if not isinstance(endog, pd.Series) else endog
    exog = pd.DataFrame(exog) if not isinstance(exog, pd.DataFrame) else exog

    if order is None:
        order = 0

    model = ARDL(endog, lags=lags, exog=exog, order=order)
    results = model.fit()

    return _json_safe({
        'params': results.params,
        'aic': float(results.aic),
        'bic': float(results.bic),
        'resid': results.resid
    })

def fit_exponential_smoothing(
    data: Union[List, np.ndarray, pd.Series],
    trend: Optional[str] = None,
    seasonal: Optional[str] = None,
    seasonal_periods: Optional[int] = None
) -> Dict[str, Any]:
    """Fit Exponential Smoothing model. params is {name: value}."""
    data = pd.Series(data) if not isinstance(data, pd.Series) else data

    model = ExponentialSmoothing(data, trend=trend, seasonal=seasonal, seasonal_periods=seasonal_periods,
                                 initialization_method='estimated')
    results = model.fit()

    return _json_safe({
        'params': _holtwinters_params(results.params),
        'aic': float(results.aic),
        'bic': float(results.bic),
        'fittedvalues': results.fittedvalues,
        'level': results.level
    })

def stl_decompose(
    data: Union[List, np.ndarray, pd.Series],
    period: int = 12,
    seasonal: int = 7
) -> Dict[str, Any]:
    """STL decomposition"""
    data = pd.Series(data) if not isinstance(data, pd.Series) else data

    stl = STL(data, period=period, seasonal=seasonal)
    result = stl.fit()

    return _json_safe({
        'trend': result.trend,
        'seasonal': result.seasonal,
        'resid': result.resid
    })

def fit_dynamic_factor(
    data: Union[np.ndarray, pd.DataFrame],
    k_factors: int = 1,
    factor_order: int = 1
) -> Dict[str, Any]:
    """Fit Dynamic Factor model"""
    data = pd.DataFrame(data) if not isinstance(data, pd.DataFrame) else data

    model = DynamicFactor(data, k_factors=k_factors, factor_order=factor_order)
    results = model.fit()

    return _json_safe({
        'params': results.params,
        'aic': float(results.aic),
        'bic': float(results.bic),
        'hqic': float(results.hqic),
        'factors': np.asarray(results.factors.filtered)
    })

def fit_unobserved_components(
    data: Union[List, np.ndarray, pd.Series],
    level: str = 'local level',
    trend: bool = False,
    seasonal: Optional[int] = None
) -> Dict[str, Any]:
    """Fit Unobserved Components model"""
    data = pd.Series(data) if not isinstance(data, pd.Series) else data

    model = UnobservedComponents(data, level=level, trend=trend, seasonal=seasonal)
    results = model.fit()

    return _json_safe({
        'params': results.params,
        'aic': float(results.aic),
        'bic': float(results.bic),
        'level': np.asarray(results.level.filtered)
    })

def fit_markov_regression(
    data: Union[List, np.ndarray, pd.Series],
    k_regimes: int = 2,
    order: int = 0
) -> Dict[str, Any]:
    """Fit Markov Switching Regression"""
    data = pd.Series(data) if not isinstance(data, pd.Series) else data

    model = MarkovRegression(data, k_regimes=k_regimes, order=order, switching_variance=True)
    results = model.fit()

    return _json_safe({
        'params': results.params,
        'aic': float(results.aic),
        'bic': float(results.bic),
        'smoothed_marginal_probabilities': np.asarray(results.smoothed_marginal_probabilities)
    })

def fit_vecm(
    data: Union[np.ndarray, pd.DataFrame],
    k_ar_diff: int = 1,
    coint_rank: int = 1,
    deterministic: str = 'ci'
) -> Dict[str, Any]:
    """Fit Vector Error Correction Model.

    VECMResults has no params/aic/bic/hqic; it reports the loading matrix alpha (neqs x rank),
    cointegrating vectors beta (neqs x rank), short-run coefficients gamma
    (neqs x neqs*k_ar_diff), deterministic terms outside (det_coef) and inside
    (det_coef_coint) the cointegration relation, and the log-likelihood.
    """
    data = pd.DataFrame(data) if not isinstance(data, pd.DataFrame) else data

    model = VECM(data, k_ar_diff=k_ar_diff, coint_rank=coint_rank, deterministic=deterministic)
    results = model.fit()

    return _json_safe({
        'alpha': np.asarray(results.alpha),
        'beta': np.asarray(results.beta),
        'gamma': np.asarray(results.gamma),
        'det_coef': np.asarray(results.det_coef),
        'det_coef_coint': np.asarray(results.det_coef_coint),
        'llf': float(results.llf),
        'coint_rank': int(results.coint_rank),
        'k_ar_diff': int(k_ar_diff),
        'nobs': int(results.nobs)
    })

def fit_holt(
    data: Union[List, np.ndarray, pd.Series],
    exponential: bool = False,
    damped_trend: bool = False
) -> Dict[str, Any]:
    """Fit Holt exponential smoothing. params is {name: value}."""
    data = pd.Series(data) if not isinstance(data, pd.Series) else data

    # statsmodels >= 0.15 requires an explicit initialization method for Holt
    model = Holt(data, exponential=exponential, damped_trend=damped_trend, initialization_method='estimated')
    results = model.fit()

    return _json_safe({
        'params': _holtwinters_params(results.params),
        'aic': float(results.aic),
        'bic': float(results.bic),
        'fittedvalues': results.fittedvalues
    })

def fit_simple_exp_smoothing(
    data: Union[List, np.ndarray, pd.Series]
) -> Dict[str, Any]:
    """Fit Simple Exponential Smoothing. params is {name: value}."""
    data = pd.Series(data) if not isinstance(data, pd.Series) else data

    # statsmodels >= 0.15 requires an explicit initialization method for SimpleExpSmoothing
    model = SimpleExpSmoothing(data, initialization_method='estimated')
    results = model.fit()

    return _json_safe({
        'params': _holtwinters_params(results.params),
        'aic': float(results.aic),
        'fittedvalues': results.fittedvalues
    })

def fit_markov_autoregression(
    data: Union[List, np.ndarray, pd.Series],
    k_regimes: int = 2,
    order: int = 1
) -> Dict[str, Any]:
    """Fit Markov Switching Autoregression"""
    data = pd.Series(data) if not isinstance(data, pd.Series) else data

    model = MarkovAutoregression(data, k_regimes=k_regimes, order=order, switching_variance=True)
    results = model.fit()

    return _json_safe({
        'params': results.params,
        'aic': float(results.aic),
        'bic': float(results.bic)
    })

def _stl_forecast_model(model):
    """(model class, default model_kwargs) for stl_forecast's model argument (a class or a name)."""
    if not isinstance(model, str):
        return model, {}
    from statsmodels.tsa.exponential_smoothing.ets import ETSModel
    named = {
        'arima': (ARIMA, {'order': (1, 1, 0)}),
        'autoreg': (AutoReg, {'lags': 1}),
        'ar': (AutoReg, {'lags': 1}),
        'ets': (ETSModel, {'error': 'add', 'trend': 'add'}),
    }
    key = model.strip().lower()
    if key not in named:
        raise ValueError(f"Unknown stl_forecast model {model!r}; use one of {sorted(named)} or a model class")
    return named[key]

def stl_forecast(
    data: Union[List, np.ndarray, pd.Series],
    model: Union[str, type] = 'arima',
    period: int = 12,
    seasonal: int = 7,
    steps: int = 10,
    model_kwargs: Optional[Dict[str, Any]] = None
) -> Dict[str, Any]:
    """STL decomposition + a forecasting model on the seasonally adjusted series.

    model: a statsmodels model class (e.g. ARIMA) or a name: 'arima' (default order (1, 1, 0)),
    'autoreg'/'ar' (lags=1) or 'ets' (additive error and trend). model_kwargs override the defaults.
    fittedvalues are in-sample predictions on the original scale (model fit + STL seasonal),
    one per observation (null where the model has no fitted value, e.g. AutoReg's first lags).
    """
    data = pd.Series(data) if not isinstance(data, pd.Series) else data

    model_cls, kwargs = _stl_forecast_model(model)
    kwargs = {**kwargs, **(model_kwargs or {})}

    stlf = STLForecast(data, model_cls, model_kwargs=kwargs, period=period, seasonal=seasonal)
    results = stlf.fit()

    seasonal_comp = pd.Series(np.asarray(results.result.seasonal), index=data.index)
    fitted = results.model_result.fittedvalues
    if not isinstance(fitted, pd.Series):
        fitted = np.asarray(fitted)
        fitted = pd.Series(fitted, index=data.index[len(data) - len(fitted):])
    fitted = fitted.reindex(data.index) + seasonal_comp

    return _json_safe({
        'fittedvalues': fitted,
        'forecast': np.asarray(results.forecast(steps)),
        'trend': np.asarray(results.result.trend),
        'seasonal': seasonal_comp
    })

def main():
    print("Testing statsmodels timeseries models wrapper")

    np.random.seed(42)
    n = 100

    ts_data = np.cumsum(np.random.randn(n)) + 10

    arima_result = fit_arima(ts_data, order=(1, 1, 1))
    print("ARIMA AIC: {:.4f}".format(arima_result['aic']))

    forecast_result = forecast_arima(ts_data, order=(1, 1, 1), steps=10)
    print("Forecast length: {}".format(len(forecast_result['forecast'])))

    autoreg_result = fit_autoreg(ts_data, lags=2)
    print("AutoReg AIC: {:.4f}".format(autoreg_result['aic']))

    seasonal_data = np.sin(np.arange(n) * 2 * np.pi / 12) + np.random.randn(n) * 0.1
    es_result = fit_exponential_smoothing(seasonal_data, trend='add', seasonal='add', seasonal_periods=12)
    print("Exponential Smoothing AIC: {:.4f}".format(es_result['aic']))

    stl_result = stl_decompose(seasonal_data, period=12)
    print("STL trend length: {}".format(len(stl_result['trend'])))

    mv_data = np.random.randn(n, 2).cumsum(axis=0)
    varmax_result = fit_varmax(mv_data, order=(1, 0))
    print("VARMAX AIC: {:.4f}".format(varmax_result['aic']))

    holt_result = fit_holt(ts_data)
    print("Holt AIC: {:.4f}".format(holt_result['aic']))

    ses_result = fit_simple_exp_smoothing(ts_data)
    print("Simple ES AIC: {:.4f}".format(ses_result['aic']))

    print("Test: PASSED")

if __name__ == "__main__":
    main()
