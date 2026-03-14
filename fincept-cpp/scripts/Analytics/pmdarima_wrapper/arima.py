import pandas as pd
import numpy as np
from typing import Dict, List, Optional, Union, Any, Tuple
import json
import pmdarima as pm
from pmdarima import ARIMA, AutoARIMA

def fit_auto_arima(
    y: Union[List, np.ndarray, pd.Series],
    exog: Optional[Union[np.ndarray, pd.DataFrame]] = None,
    start_p: int = 2,
    start_q: int = 2,
    max_p: int = 5,
    max_q: int = 5,
    seasonal: bool = True,
    m: int = 1,
    d: Optional[int] = None,
    D: Optional[int] = None,
    trace: bool = False,
    stepwise: bool = True
) -> Dict[str, Any]:
    """Fit AutoARIMA model with automatic parameter selection"""
    y = pd.Series(y) if not isinstance(y, pd.Series) else y

    model = pm.auto_arima(
        y, exog=exog,
        start_p=start_p, start_q=start_q,
        max_p=max_p, max_q=max_q,
        seasonal=seasonal, m=m,
        d=d, D=D,
        trace=trace, stepwise=stepwise,
        error_action='ignore',
        suppress_warnings=True
    )

    return {
        'order': model.order,
        'seasonal_order': model.seasonal_order,
        'aic': float(model.aic()),
        'bic': float(model.bic()),
        'params': model.params().tolist() if hasattr(model.params(), 'tolist') else list(model.params())
    }

def fit_arima(
    y: Union[List, np.ndarray, pd.Series],
    order: Tuple[int, int, int] = (1, 1, 1),
    seasonal_order: Tuple[int, int, int, int] = (0, 0, 0, 0),
    exog: Optional[Union[np.ndarray, pd.DataFrame]] = None
) -> Dict[str, Any]:
    """Fit ARIMA model with specified parameters"""
    y = pd.Series(y) if not isinstance(y, pd.Series) else y

    model = ARIMA(order=order, seasonal_order=seasonal_order)
    model.fit(y, exogenous=exog)

    return {
        'order': model.order,
        'seasonal_order': model.seasonal_order,
        'aic': float(model.aic()),
        'bic': float(model.bic()),
        'params': model.params().tolist() if hasattr(model.params(), 'tolist') else list(model.params())
    }

def forecast_auto_arima(
    y: Union[List, np.ndarray, pd.Series],
    n_periods: int = 10,
    exog: Optional[Union[np.ndarray, pd.DataFrame]] = None,
    exog_future: Optional[Union[np.ndarray, pd.DataFrame]] = None,
    return_conf_int: bool = True,
    alpha: float = 0.05
) -> Dict[str, Any]:
    """Fit AutoARIMA and generate forecasts"""
    y = pd.Series(y) if not isinstance(y, pd.Series) else y

    model = pm.auto_arima(
        y, exog=exog,
        seasonal=True,
        stepwise=True,
        suppress_warnings=True,
        error_action='ignore'
    )

    forecast, conf_int = model.predict(
        n_periods=n_periods,
        exogenous=exog_future,
        return_conf_int=return_conf_int,
        alpha=alpha
    )

    result = {
        'forecast': forecast.tolist() if hasattr(forecast, 'tolist') else list(forecast),
        'order': model.order,
        'seasonal_order': model.seasonal_order,
        'aic': float(model.aic()),
        'bic': float(model.bic())
    }

    if return_conf_int:
        result['conf_int_lower'] = conf_int[:, 0].tolist()
        result['conf_int_upper'] = conf_int[:, 1].tolist()

    return result

def forecast_arima(
    y: Union[List, np.ndarray, pd.Series],
    order: Tuple[int, int, int],
    n_periods: int = 10,
    exog: Optional[Union[np.ndarray, pd.DataFrame]] = None,
    exog_future: Optional[Union[np.ndarray, pd.DataFrame]] = None,
    return_conf_int: bool = True,
    alpha: float = 0.05
) -> Dict[str, Any]:
    """Fit ARIMA and generate forecasts"""
    y = pd.Series(y) if not isinstance(y, pd.Series) else y

    model = ARIMA(order=order)
    model.fit(y, exogenous=exog)

    forecast, conf_int = model.predict(
        n_periods=n_periods,
        exogenous=exog_future,
        return_conf_int=return_conf_int,
        alpha=alpha
    )

    result = {
        'forecast': forecast.tolist() if hasattr(forecast, 'tolist') else list(forecast),
        'order': model.order,
        'aic': float(model.aic()),
        'bic': float(model.bic())
    }

    if return_conf_int:
        result['conf_int_lower'] = conf_int[:, 0].tolist()
        result['conf_int_upper'] = conf_int[:, 1].tolist()

    return result

def update_arima(
    y: Union[List, np.ndarray, pd.Series],
    order: Tuple[int, int, int],
    new_data: Union[List, np.ndarray, pd.Series]
) -> Dict[str, Any]:
    """Fit ARIMA and update with new data"""
    y = pd.Series(y) if not isinstance(y, pd.Series) else y
    new_data = pd.Series(new_data) if not isinstance(new_data, pd.Series) else new_data

    model = ARIMA(order=order)
    model.fit(y)

    model.update(new_data)

    return {
        'order': model.order,
        'aic': float(model.aic()),
        'n_obs': len(y) + len(new_data)
    }

def main():
    print("Testing pmdarima ARIMA wrapper")

    np.random.seed(42)
    n = 100
    y = np.cumsum(np.random.randn(n)) + 10

    auto_result = fit_auto_arima(y, seasonal=False, stepwise=True)
    print("AutoARIMA order: {}, AIC: {:.4f}".format(auto_result['order'], auto_result['aic']))

    arima_result = fit_arima(y, order=(1, 1, 1))
    print("ARIMA AIC: {:.4f}".format(arima_result['aic']))

    forecast_result = forecast_auto_arima(y, n_periods=10)
    print("Forecast length: {}, first value: {:.4f}".format(
        len(forecast_result['forecast']),
        forecast_result['forecast'][0]
    ))

    arima_forecast = forecast_arima(y, order=(1, 1, 1), n_periods=5)
    print("ARIMA forecast length: {}".format(len(arima_forecast['forecast'])))

    update_result = update_arima(y[:80], order=(1, 1, 1), new_data=y[80:])
    print("Updated model n_obs: {}".format(update_result['n_obs']))

    print("Test: PASSED")

if __name__ == "__main__":
    main()
