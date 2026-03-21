"""Renaissance Technologies quantitative strategies."""

from .mean_reversion import (
    MeanReversionResult,
    analyze_mean_reversion,
    analyze_pairs_cointegration,
    calculate_z_score,
    estimate_half_life,
    calculate_hurst_exponent,
)

from .momentum import (
    MomentumResult,
    analyze_momentum,
    calculate_momentum_score,
    calculate_trend_strength,
    calculate_acceleration,
    find_optimal_lookback,
    calculate_relative_strength,
    calculate_cross_sectional_momentum_rank,
)

from .statistical_arbitrage import (
    MarketRegime,
    StatArbResult,
    PairsTradingResult,
    analyze_statistical_arbitrage,
    analyze_pairs_trading,
    detect_market_regime,
    detect_statistical_mispricing,
    estimate_edge,
)

from .risk_management import (
    VolatilityRegime,
    RiskMetrics,
    analyze_risk,
    calculate_volatility,
    calculate_var,
    calculate_max_drawdown,
    calculate_sharpe_ratio,
    calculate_sortino_ratio,
    calculate_beta,
    calculate_kelly_criterion,
    calculate_position_size,
)

from .execution import (
    ExecutionUrgency,
    ExecutionAnalysis,
    analyze_execution,
    estimate_market_impact,
    estimate_spread_cost,
    estimate_slippage,
    calculate_optimal_trade_size,
)

# Analysis functions (pure Python, no LLM - extracted from legacy agents)
from .analysis import (
    run_signal_analysis,
    run_risk_analysis,
    run_execution_analysis,
    run_stat_arb_analysis,
    synthesize_decision,
)

__all__ = [
    # Mean Reversion
    "MeanReversionResult",
    "analyze_mean_reversion",
    "analyze_pairs_cointegration",
    "calculate_z_score",
    "estimate_half_life",
    "calculate_hurst_exponent",
    # Momentum
    "MomentumResult",
    "analyze_momentum",
    "calculate_momentum_score",
    "calculate_trend_strength",
    "calculate_acceleration",
    "find_optimal_lookback",
    "calculate_relative_strength",
    "calculate_cross_sectional_momentum_rank",
    # Statistical Arbitrage
    "MarketRegime",
    "StatArbResult",
    "PairsTradingResult",
    "analyze_statistical_arbitrage",
    "analyze_pairs_trading",
    "detect_market_regime",
    "detect_statistical_mispricing",
    "estimate_edge",
    # Risk Management
    "VolatilityRegime",
    "RiskMetrics",
    "analyze_risk",
    "calculate_volatility",
    "calculate_var",
    "calculate_max_drawdown",
    "calculate_sharpe_ratio",
    "calculate_sortino_ratio",
    "calculate_beta",
    "calculate_kelly_criterion",
    "calculate_position_size",
    # Execution
    "ExecutionUrgency",
    "ExecutionAnalysis",
    "analyze_execution",
    "estimate_market_impact",
    "estimate_spread_cost",
    "estimate_slippage",
    "calculate_optimal_trade_size",
    # Analysis Functions (from legacy agents)
    "run_signal_analysis",
    "run_risk_analysis",
    "run_execution_analysis",
    "run_stat_arb_analysis",
    "synthesize_decision",
]
