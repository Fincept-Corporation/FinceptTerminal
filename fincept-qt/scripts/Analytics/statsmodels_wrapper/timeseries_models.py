import pandas as pd
import numpy as np
from typing import Dict, List, Optional, Union, Any, Tuple
import json
import statsmodels.api as sm
from statsmodels.tsa.api import ARIMA, SARIMAX, VARMAX, AutoReg, ARDL, ExponentialSmoothing, Holt, SimpleExpSmoothing, STL, STLForecast, DynamicFactor, UnobservedComponents, MarkovRegression, MarkovAutoregression, VECM

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

    return {
        'params': results.params.tolist() if hasattr(results.params, 'tolist') else list(results.params),
        'aic': float(results.aic),
        'bic': float(results.bic),
        'hqic': float(results.hqic),
        'resid': results.resid.tolist() if hasattr(results.resid, 'tolist') else list(results.resid),
        'fittedvalues': results.fittedvalues.tolist() if hasattr(results.fittedvalues, 'tolist') else list(results.fittedvalues)
    }

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

    return {
        'forecast': forecast.tolist() if hasattr(forecast, 'tolist') else list(forecast),
        'aic': float(results.aic),
        'bic': float(results.bic)
    }

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

    return {
        'params': results.params.tolist() if hasattr(results.params, 'tolist') else list(results.params),
        'aic': float(results.aic),
        'bic': float(results.bic),
        'hqic': float(results.hqic),
        'resid': results.resid.tolist() if hasattr(results.resid, 'tolist') else list(results.resid),
        'fittedvalues': results.fittedvalues.tolist() if hasattr(results.fittedvalues, 'tolist') else list(results.fittedvalues)
    }

def fit_varmax(
    data: Union[np.ndarray, pd.DataFrame],
    order: Tuple[int, int] = (1, 0),
    trend: str = 'c'
) -> Dict[str, Any]:
    """Fit VARMAX model"""
    data = pd.DataFrame(data) if not isinstance(data, pd.DataFrame) else data

    model = VARMAX(data, order=order, trend=trend)
    results = model.fit()

    return {
        'params': results.params.tolist() if hasattr(results.params, 'tolist') else list(results.params),
        'aic': float(results.aic),
        'bic': float(results.bic),
        'hqic': float(results.hqic),
        'resid': results.resid.values.tolist() if hasattr(results.resid, 'values') else results.resid.tolist()
    }

def fit_autoreg(
    data: Union[List, np.ndarray, pd.Series],
    lags: int = 1,
    trend: str = 'c'
) -> Dict[str, Any]:
    """Fit AutoReg model"""
    data = pd.Series(data) if not isinstance(data, pd.Series) else data

    model = AutoReg(data, lags=lags, trend=trend)
    results = model.fit()

    return {
        'params': results.params.tolist() if hasattr(results.params, 'tolist') else list(results.params),
        'aic': float(results.aic),
        'bic': float(results.bic),
        'hqic': float(results.hqic),
        'resid': results.resid.tolist() if hasattr(results.resid, 'tolist') else list(results.resid),
        'fittedvalues': results.fittedvalues.tolist() if hasattr(results.fittedvalues, 'tolist') else list(results.fittedvalues)
    }

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

    return {
        'params': results.params.tolist() if hasattr(results.params, 'tolist') else list(results.params),
        'aic': float(results.aic),
        'bic': float(results.bic),
        'resid': results.resid.tolist() if hasattr(results.resid, 'tolist') else list(results.resid)
    }

def fit_exponential_smoothing(
    data: Union[List, np.ndarray, pd.Series],
    trend: Optional[str] = None,
    seasonal: Optional[str] = None,
    seasonal_periods: Optional[int] = None
) -> Dict[str, Any]:
    """Fit Exponential Smoothing model"""
    data = pd.Series(data) if not isinstance(data, pd.Series) else data

    model = ExponentialSmoothing(data, trend=trend, seasonal=seasonal, seasonal_periods=seasonal_periods)
    results = model.fit()

    return {
        'params': results.params.tolist() if hasattr(results.params, 'tolist') else list(results.params),
        'aic': float(results.aic),
        'bic': float(results.bic),
        'fittedvalues': results.fittedvalues.tolist() if hasattr(results.fittedvalues, 'tolist') else list(results.fittedvalues),
        'level': results.level.tolist() if hasattr(results.level, 'tolist') else list(results.level)
    }

def stl_decompose(
    data: Union[List, np.ndarray, pd.Series],
    period: int = 12,
    seasonal: int = 7
) -> Dict[str, Any]:
    """STL decomposition"""
    data = pd.Series(data) if not isinstance(data, pd.Series) else data

    stl = STL(data, period=period, seasonal=seasonal)
    result = stl.fit()

    return {
        'trend': result.trend.tolist() if hasattr(result.trend, 'tolist') else list(result.trend),
        'seasonal': result.seasonal.tolist() if hasattr(result.seasonal, 'tolist') else list(result.seasonal),
        'resid': result.resid.tolist() if hasattr(result.resid, 'tolist') else list(result.resid)
    }

def fit_dynamic_factor(
    data: Union[np.ndarray, pd.DataFrame],
    k_factors: int = 1,
    factor_order: int = 1
) -> Dict[str, Any]:
    """Fit Dynamic Factor model"""
    data = pd.DataFrame(data) if not isinstance(data, pd.DataFrame) else data

    model = DynamicFactor(data, k_factors=k_factors, factor_order=factor_order)
    results = model.fit()

    return {
        'params': results.params.tolist() if hasattr(results.params, 'tolist') else list(results.params),
        'aic': float(results.aic),
        'bic': float(results.bic),
        'hqic': float(results.hqic),
        'factors': results.factors.filtered.values.tolist() if hasattr(results.factors.filtered, 'values') else results.factors.filtered.tolist()
    }

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

    return {
        'params': results.params.tolist() if hasattr(results.params, 'tolist') else list(results.params),
        'aic': float(results.aic),
        'bic': float(results.bic),
        'level': results.level.filtered.tolist() if hasattr(results.level.filtered, 'tolist') else list(results.level.filtered)
    }

def fit_markov_regression(
    data: Union[List, np.ndarray, pd.Series],
    k_regimes: int = 2,
    order: int = 0
) -> Dict[str, Any]:
    """Fit Markov Switching Regression"""
    data = pd.Series(data) if not isinstance(data, pd.Series) else data

    model = MarkovRegression(data, k_regimes=k_regimes, order=order, switching_variance=True)
    results = model.fit()

    return {
        'params': results.params.tolist() if hasattr(results.params, 'tolist') else list(results.params),
        'aic': float(results.aic),
        'bic': float(results.bic),
        'smoothed_marginal_probabilities': results.smoothed_marginal_probabilities.values.tolist() if hasattr(results.smoothed_marginal_probabilities, 'values') else results.smoothed_marginal_probabilities.tolist()
    }

def fit_vecm(
    data: Union[np.ndarray, pd.DataFrame],
    k_ar_diff: int = 1,
    coint_rank: int = 1,
    deterministic: str = 'ci'
) -> Dict[str, Any]:
    """Fit Vector Error Correction Model"""
    data = pd.DataFrame(data) if not isinstance(data, pd.DataFrame) else data

    model = VECM(data, k_ar_diff=k_ar_diff, coint_rank=coint_rank, deterministic=deterministic)
    results = model.fit()

    return {
        'params': results.params.tolist() if hasattr(results.params, 'tolist') else [p.tolist() for p in results.params],
        'aic': float(results.aic),
        'bic': float(results.bic),
        'hqic': float(results.hqic),
        'alpha': results.alpha.tolist() if hasattr(results.alpha, 'tolist') else [[float(x) for x in row] for row in results.alpha],
        'beta': results.beta.tolist() if hasattr(results.beta, 'tolist') else [[float(x) for x in row] for row in results.beta]
    }

def fit_holt(
    data: Union[List, np.ndarray, pd.Series],
    exponential: bool = False,
    damped_trend: bool = False
) -> Dict[str, Any]:
    """Fit Holt exponential smoothing"""
    data = pd.Series(data) if not isinstance(data, pd.Series) else data

    model = Holt(data, exponential=exponential, damped_trend=damped_trend)
    results = model.fit()

    return {
        'params': results.params.tolist() if hasattr(results.params, 'tolist') else list(results.params),
        'aic': float(results.aic),
        'bic': float(results.bic),
        'fittedvalues': results.fittedvalues.tolist() if hasattr(results.fittedvalues, 'tolist') else list(results.fittedvalues)
    }

def fit_simple_exp_smoothing(
    data: Union[List, np.ndarray, pd.Series]
) -> Dict[str, Any]:
    """Fit Simple Exponential Smoothing"""
    data = pd.Series(data) if not isinstance(data, pd.Series) else data

    model = SimpleExpSmoothing(data)
    results = model.fit()

    return {
        'params': results.params.tolist() if hasattr(results.params, 'tolist') else list(results.params),
        'aic': float(results.aic),
        'fittedvalues': results.fittedvalues.tolist() if hasattr(results.fittedvalues, 'tolist') else list(results.fittedvalues)
    }

def fit_markov_autoregression(
    data: Union[List, np.ndarray, pd.Series],
    k_regimes: int = 2,
    order: int = 1
) -> Dict[str, Any]:
    """Fit Markov Switching Autoregression"""
    data = pd.Series(data) if not isinstance(data, pd.Series) else data

    model = MarkovAutoregression(data, k_regimes=k_regimes, order=order, switching_variance=True)
    results = model.fit()

    return {
        'params': results.params.tolist() if hasattr(results.params, 'tolist') else list(results.params),
        'aic': float(results.aic),
        'bic': float(results.bic)
    }

def stl_forecast(
    data: Union[List, np.ndarray, pd.Series],
    model,
    period: int = 12,
    seasonal: int = 7
) -> Dict[str, Any]:
    """STL with forecasting model"""
    data = pd.Series(data) if not isinstance(data, pd.Series) else data

    stlf = STLForecast(data, model, period=period, seasonal=seasonal)
    results = stlf.fit()

    return {
        'fittedvalues': results.fittedvalues.tolist() if hasattr(results.fittedvalues, 'tolist') else list(results.fittedvalues)
    }

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
