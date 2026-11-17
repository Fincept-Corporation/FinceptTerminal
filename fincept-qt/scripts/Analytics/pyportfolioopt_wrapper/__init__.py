"""
PyPortfolioOpt Wrapper Module
==============================

Complete wrapper for PyPortfolioOpt library with all features including:
- Efficient Frontier optimization
- Black-Litterman allocation
- Hierarchical Risk Parity (HRP)
- Critical Line Algorithm (CLA)
- CVaR and CDaR optimization
- Custom objectives and constraints
- Discrete allocation
- Portfolio performance analytics
"""

from .core import (
    PyPortfolioOptConfig,
    PyPortfolioOptAnalyticsEngine,
    create_sample_pypfopt_config,
    demo_pypfopt_analytics
)

from .advanced_objectives import (
    add_custom_objective,
    add_sector_constraints,
    add_tracking_error_constraint,
    add_turnover_constraint,
    optimize_with_custom_constraints,
    optimize_with_views
)

from .additional_optimizers import (
    equal_risk_contribution_weights,
    optimize_minimum_tracking_error,
    optimize_risk_parity,
    optimize_equal_weighting,
    optimize_inverse_volatility,
    optimize_market_neutral,
    optimize_maximum_diversification
)

__version__ = "1.0.0"
__all__ = [
    "PyPortfolioOptConfig",
    "PyPortfolioOptAnalyticsEngine",
    "create_sample_pypfopt_config",
    "demo_pypfopt_analytics",
    "add_custom_objective",
    "add_sector_constraints",
    "add_tracking_error_constraint",
    "add_turnover_constraint",
    "optimize_with_custom_constraints",
    "optimize_with_views",
    "equal_risk_contribution_weights",
    "optimize_minimum_tracking_error",
    "optimize_risk_parity",
    "optimize_equal_weighting",
    "optimize_inverse_volatility",
    "optimize_market_neutral",
    "optimize_maximum_diversification"
]
