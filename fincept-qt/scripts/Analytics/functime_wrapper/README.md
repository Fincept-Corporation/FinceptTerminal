# Functime service

`functime_service.py` backs AI Quant Lab › Functime. Despite the folder name it
does **not** use the polars-based `functime` library: it is a self-contained
time-series service on pandas, scikit-learn and statsmodels.

```bash
python functime_service.py <operation> '<json params>'   # prints one JSON object
```

| Operation | What it does |
|---|---|
| `check_status` | Which backends are importable |
| `forecast` | Point forecasts (linear / lasso / ridge / elastic-net / KNN regressors on lag features) |
| `anomaly_detection` | Anomalies by z-score, IQR, isolation forest or forecast residual |
| `seasonality` | STL decomposition; period auto-detected (FFT / autocorrelation) when not given |
| `metrics` | Forecast error metrics (MAE, MSE, RMSE, MAPE, sMAPE, bias, R²) |
| `confidence_intervals` | Prediction intervals for a forecast |
| `stationarity` | ADF + KPSS tests with a recommended differencing order |

Every response is `{success, operation, data | error, error_kind, traceback}`.
The parameters of each operation are documented in the module docstring and in
its `op_*` function. The script runs in `venv-numpy1` (its path contains
`functime`; see `PythonRunner.cpp`).
