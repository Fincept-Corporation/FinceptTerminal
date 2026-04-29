"""
FFN (Financial Functions for Python) Wrapper Module
===================================================

Advanced financial performance analytics and portfolio statistics using the ffn library.
Provides comprehensive performance analysis, risk metrics, portfolio optimization,
and visualization capabilities.

===== DATA SOURCES REQUIRED =====
INPUT:
  - Pandas DataFrame or Series with price/return data (datetime index)
  - Multiple asset price series for portfolio analysis
  - Benchmark data for relative performance analysis
  - Risk-free rate for Sharpe/Sortino calculations

OUTPUT:
  - Performance statistics (returns, volatility, Sharpe ratio, etc.)
  - Drawdown analysis and maximum drawdown
  - Rolling performance metrics
  - Portfolio weights (ERC, minimum variance, inverse volatility)
  - Correlation matrices and heatmaps
  - Performance visualizations
  - Group statistics for multi-asset comparison

PARAMETERS:
  - risk_free_rate: Risk-free rate for Sharpe/Sortino calculations (default: 0.0)
  - annualization_factor: Days per year for annualization (default: 252)
  - rebase_value: Starting value for price rebasing (default: 100)
  - weight_bounds: Min/max weight constraints (default: (0.0, 1.0))
  - covar_method: Covariance estimation method (default: 'ledoit-wolf')
"""


__all__ = [
    'FFNAnalyticsEngine',
    'FFNConfig',
    'FFNPerformanceAnalyzer',
    'FFNPortfolioOptimizer',
]


# ── Lazy attribute resolution (PEP 562) ─────────────────────────────────────
# Submodules below have an `if __name__ == "__main__":` block and may be
# invoked via `python -m`. Eagerly importing them here would put each in
# sys.modules before Python re-executes them as __main__, triggering a
# RuntimeWarning ("found in sys.modules ... prior to execution"). The lazy
# loader keeps the public API intact while deferring import to first access.
_LAZY_ATTRS: dict[str, tuple[str, str]] = {
    "FFNAnalyticsEngine": ("ffn_analytics", "FFNAnalyticsEngine"),
    "FFNConfig": ("ffn_analytics", "FFNConfig"),
    "FFNPerformanceAnalyzer": ("ffn_performance", "FFNPerformanceAnalyzer"),
    "FFNPortfolioOptimizer": ("ffn_portfolio", "FFNPortfolioOptimizer"),
}


def __getattr__(name: str):  # PEP 562
    target = _LAZY_ATTRS.get(name)
    if target is None:
        raise AttributeError(f"module {__name__!r} has no attribute {name!r}")
    submodule, original_name = target
    import importlib
    mod = importlib.import_module(f".{submodule}", __name__)
    value = getattr(mod, original_name)
    globals()[name] = value  # cache for subsequent access
    return value


def __dir__() -> list[str]:
    return sorted(set(globals()) | set(_LAZY_ATTRS))
