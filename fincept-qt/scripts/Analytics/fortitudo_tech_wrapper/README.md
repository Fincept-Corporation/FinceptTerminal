# Fortitudo service

`fortitudo_service.py` backs AI Quant Lab › Fortitudo: risk-aware portfolio
analytics and optimisation. It uses the `fortitudo.tech` library when it is
installed and falls back to numpy implementations otherwise.

```bash
python fortitudo_service.py <operation> '<json params>'   # prints one JSON object
```

| Operation | What it does |
|---|---|
| `check_status` | Whether fortitudo.tech is importable, and the available operations |
| `portfolio_metrics` | Volatility, VaR, CVaR and simulation moments of a weighted portfolio |
| `covariance_matrix` | Covariance and correlation matrices plus per-asset moments |
| `mean_variance_optimize` | Mean-variance portfolio: minimum variance, maximum Sharpe or target return; long-only and weight bounds |
| `mean_cvar_optimize` | Mean-CVaR optimal portfolio |
| `efficient_frontier` | Mean-variance efficient frontier (5–50 points) |
| `exp_decay_probabilities` | Exponentially decaying scenario probabilities (half-life) |

Every response is `{success, operation, data | error, error_kind, traceback}`;
the parameters of each operation are documented in its `op_*` function.

| Module | Role |
|---|---|
| `functions.py` | Portfolio statistics (volatility, VaR, CVaR, covariance, correlation, simulation moments, exponential-decay probabilities, normal calibration) with numpy fallbacks |
| `optimization.py` | Mean-variance and mean-CVaR optimisers and frontiers |
| `fortitudo_service.py` | The CLI the terminal (and AI agents, via the MCP `python` tool) runs |
