# GluonTS service

`gluonts_service.py` backs AI Quant Lab › GluonTS. It covers the same ground as
GluonTS — probabilistic point and interval forecasts, distribution fits,
evaluation metrics and baseline predictors — with scipy, scikit-learn and numpy
only; it does **not** import the torch-based `gluonts` package.

```bash
python gluonts_service.py <operation> '<json params>'   # prints one JSON object
```

| Operation | What it does |
|---|---|
| `check_status` | Which backends are importable |
| `probabilistic_forecast` | Point forecast with predictive distribution |
| `quantile_forecast` | Forecast quantiles (e.g. P10 / P50 / P90) |
| `distribution_fit` | Fit candidate distributions to a series and rank them |
| `evaluate_forecast` | Accuracy and calibration metrics for a forecast |
| `seasonal_naive` | Seasonal-naive baseline forecast |

Every response is `{success, operation, data | error, error_kind, traceback}`.
The parameters of each operation are documented in the module docstring and in
its `op_*` function. The script runs in `venv-numpy1` (its path contains
`gluonts`; see `PythonRunner.cpp`).
