"""
Risk Management Module

Renaissance Technologies' risk management framework.
Sophisticated mathematical risk models for position sizing and portfolio protection.

Key concepts:
- Volatility modeling (GARCH-like)
- Factor risk decomposition
- Value at Risk (VaR)
- Maximum drawdown analysis
- Correlation risk
- Liquidity risk
"""

import math
from typing import List, Dict, Any, Optional, Tuple
from dataclasses import dataclass
from enum import Enum


class VolatilityRegime(str, Enum):
    LOW = "low"
    NORMAL = "normal"
    HIGH = "high"
    EXTREME = "extreme"


@dataclass
class RiskMetrics:
    """Comprehensive risk metrics output"""
    volatility_annualized: float
    volatility_regime: VolatilityRegime
    var_95: float
    var_99: float
    max_drawdown: float
    current_drawdown: float
    sharpe_ratio: Optional[float]
    sortino_ratio: Optional[float]
    beta: float
    correlation_to_market: float
    idiosyncratic_ratio: float
    liquidity_score: float
    risk_score: float  # 0-10, lower is better (less risky)
    details: List[str]


def calculate_volatility(returns: List[float]) -> float:
    """Calculate annualized volatility from returns."""
    if len(returns) < 2:
        return 0.0

    mean_return = sum(returns) / len(returns)
    variance = sum((r - mean_return) ** 2 for r in returns) / (len(returns) - 1)
    daily_vol = math.sqrt(variance)

    # Annualize (252 trading days)
    return daily_vol * math.sqrt(252)


def classify_volatility_regime(annualized_vol: float) -> VolatilityRegime:
    """Classify current volatility regime."""
    if annualized_vol < 0.12:
        return VolatilityRegime.LOW
    elif annualized_vol < 0.20:
        return VolatilityRegime.NORMAL
    elif annualized_vol < 0.35:
        return VolatilityRegime.HIGH
    else:
        return VolatilityRegime.EXTREME


def calculate_ewma_volatility(
    returns: List[float],
    lambda_param: float = 0.94
) -> float:
    """
    Calculate EWMA (Exponentially Weighted Moving Average) volatility.
    Similar to RiskMetrics methodology.
    """
    if len(returns) < 2:
        return 0.0

    # Initialize with simple variance
    variance = returns[0] ** 2

    # EWMA update
    for r in returns[1:]:
        variance = lambda_param * variance + (1 - lambda_param) * (r ** 2)

    daily_vol = math.sqrt(variance)
    return daily_vol * math.sqrt(252)  # Annualize


def calculate_var(
    returns: List[float],
    confidence: float = 0.95
) -> float:
    """
    Calculate historical Value at Risk.
    Returns the VaR as a positive percentage (loss).
    """
    if len(returns) < 10:
        return 0.0

    sorted_returns = sorted(returns)
    index = int((1 - confidence) * len(sorted_returns))
    var = -sorted_returns[index]  # Convert to positive loss

    # Annualize (approximately)
    return var * math.sqrt(252)


def calculate_max_drawdown(prices: List[float]) -> Tuple[float, float]:
    """
    Calculate maximum drawdown and current drawdown.

    Returns:
        Tuple of (max_drawdown, current_drawdown) as positive percentages
    """
    if len(prices) < 2:
        return (0.0, 0.0)

    # Prices are most recent first, reverse for calculation
    prices_chronological = list(reversed(prices))

    peak = prices_chronological[0]
    max_dd = 0.0
    current_dd = 0.0

    for price in prices_chronological:
        if price > peak:
            peak = price
        drawdown = (peak - price) / peak
        max_dd = max(max_dd, drawdown)

    # Current drawdown (from most recent peak)
    recent_peak = max(prices[:min(20, len(prices))])
    current_dd = (recent_peak - prices[0]) / recent_peak if recent_peak > 0 else 0

    return (max_dd, max(0, current_dd))


def calculate_sharpe_ratio(
    returns: List[float],
    risk_free_rate: float = 0.05
) -> Optional[float]:
    """Calculate annualized Sharpe ratio."""
    if len(returns) < 20:
        return None

    mean_return = sum(returns) / len(returns)
    volatility = calculate_volatility(returns) / math.sqrt(252)  # Daily vol

    if volatility == 0:
        return None

    # Annualize
    annual_return = mean_return * 252
    annual_vol = volatility * math.sqrt(252)

    sharpe = (annual_return - risk_free_rate) / annual_vol
    return sharpe


def calculate_sortino_ratio(
    returns: List[float],
    risk_free_rate: float = 0.05
) -> Optional[float]:
    """Calculate Sortino ratio (uses downside deviation only)."""
    if len(returns) < 20:
        return None

    mean_return = sum(returns) / len(returns)

    # Downside returns only
    downside_returns = [r for r in returns if r < 0]
    if not downside_returns:
        return None

    downside_variance = sum(r ** 2 for r in downside_returns) / len(downside_returns)
    downside_vol = math.sqrt(downside_variance) * math.sqrt(252)

    if downside_vol == 0:
        return None

    annual_return = mean_return * 252
    sortino = (annual_return - risk_free_rate) / downside_vol
    return sortino


def calculate_beta(
    asset_returns: List[float],
    market_returns: List[float]
) -> float:
    """Calculate beta relative to market."""
    if len(asset_returns) != len(market_returns) or len(asset_returns) < 20:
        return 1.0

    mean_asset = sum(asset_returns) / len(asset_returns)
    mean_market = sum(market_returns) / len(market_returns)

    covariance = sum(
        (asset_returns[i] - mean_asset) * (market_returns[i] - mean_market)
        for i in range(len(asset_returns))
    ) / len(asset_returns)

    market_variance = sum(
        (r - mean_market) ** 2 for r in market_returns
    ) / len(market_returns)

    if market_variance == 0:
        return 1.0

    return covariance / market_variance


def calculate_correlation(
    series_a: List[float],
    series_b: List[float]
) -> float:
    """Calculate Pearson correlation coefficient."""
    if len(series_a) != len(series_b) or len(series_a) < 10:
        return 0.0

    mean_a = sum(series_a) / len(series_a)
    mean_b = sum(series_b) / len(series_b)

    numerator = sum(
        (series_a[i] - mean_a) * (series_b[i] - mean_b)
        for i in range(len(series_a))
    )

    std_a = math.sqrt(sum((x - mean_a) ** 2 for x in series_a))
    std_b = math.sqrt(sum((x - mean_b) ** 2 for x in series_b))

    if std_a == 0 or std_b == 0:
        return 0.0

    return numerator / (std_a * std_b)


def estimate_liquidity_score(market_cap: float, avg_volume: Optional[float] = None) -> float:
    """
    Estimate liquidity score (0-10, higher is more liquid).
    """
    # Market cap based score
    if market_cap >= 200_000_000_000:  # $200B+
        cap_score = 10
    elif market_cap >= 50_000_000_000:  # $50B+
        cap_score = 9
    elif market_cap >= 10_000_000_000:  # $10B+
        cap_score = 8
    elif market_cap >= 2_000_000_000:  # $2B+
        cap_score = 6
    elif market_cap >= 500_000_000:  # $500M+
        cap_score = 4
    else:
        cap_score = 2

    # Adjust for volume if available
    if avg_volume:
        if avg_volume >= 10_000_000:
            vol_adjustment = 1.0
        elif avg_volume >= 1_000_000:
            vol_adjustment = 0.8
        else:
            vol_adjustment = 0.6
        return cap_score * vol_adjustment

    return cap_score


def decompose_factor_risk(
    asset_returns: List[float],
    market_returns: List[float]
) -> Dict[str, float]:
    """
    Decompose risk into factor and idiosyncratic components.
    Simplified factor model.
    """
    if len(asset_returns) != len(market_returns) or len(asset_returns) < 20:
        return {
            "market_risk": 0.5,
            "idiosyncratic_risk": 0.5,
            "r_squared": 0.5
        }

    beta = calculate_beta(asset_returns, market_returns)
    correlation = calculate_correlation(asset_returns, market_returns)

    # R-squared represents systematic risk proportion
    r_squared = correlation ** 2

    # Idiosyncratic ratio
    idiosyncratic_ratio = 1 - r_squared

    return {
        "market_risk": r_squared,
        "idiosyncratic_risk": idiosyncratic_ratio,
        "r_squared": r_squared,
        "beta": beta
    }


def analyze_risk(
    prices: List[float],
    market_prices: Optional[List[float]] = None,
    market_cap: float = 10_000_000_000
) -> RiskMetrics:
    """
    Comprehensive risk analysis.

    Args:
        prices: Asset price series (most recent first)
        market_prices: Market benchmark prices for beta/correlation
        market_cap: Market capitalization for liquidity scoring
    """
    details = []

    if len(prices) < 20:
        return RiskMetrics(
            volatility_annualized=0.0,
            volatility_regime=VolatilityRegime.NORMAL,
            var_95=0.0,
            var_99=0.0,
            max_drawdown=0.0,
            current_drawdown=0.0,
            sharpe_ratio=None,
            sortino_ratio=None,
            beta=1.0,
            correlation_to_market=0.5,
            idiosyncratic_ratio=0.5,
            liquidity_score=5.0,
            risk_score=5.0,
            details=["Insufficient data for risk analysis"]
        )

    # Calculate returns
    returns = [(prices[i] / prices[i + 1]) - 1 for i in range(len(prices) - 1)]

    # Volatility
    vol = calculate_volatility(returns)
    ewma_vol = calculate_ewma_volatility(returns)
    vol_regime = classify_volatility_regime(vol)
    details.append(f"Annualized volatility: {vol * 100:.1f}%")
    details.append(f"Volatility regime: {vol_regime.value}")

    # VaR
    var_95 = calculate_var(returns, 0.95)
    var_99 = calculate_var(returns, 0.99)
    details.append(f"95% VaR: {var_95 * 100:.1f}%")

    # Drawdown
    max_dd, current_dd = calculate_max_drawdown(prices)
    details.append(f"Max drawdown: {max_dd * 100:.1f}%")
    if current_dd > 0.05:
        details.append(f"Current drawdown: {current_dd * 100:.1f}%")

    # Risk-adjusted returns
    sharpe = calculate_sharpe_ratio(returns)
    sortino = calculate_sortino_ratio(returns)
    if sharpe:
        details.append(f"Sharpe ratio: {sharpe:.2f}")

    # Market correlation and beta
    beta = 1.0
    correlation = 0.5
    idiosyncratic_ratio = 0.5

    if market_prices and len(market_prices) >= len(prices):
        market_returns = [
            (market_prices[i] / market_prices[i + 1]) - 1
            for i in range(len(prices) - 1)
        ]
        beta = calculate_beta(returns, market_returns)
        correlation = calculate_correlation(returns, market_returns)
        factor_decomp = decompose_factor_risk(returns, market_returns)
        idiosyncratic_ratio = factor_decomp["idiosyncratic_risk"]

        details.append(f"Beta: {beta:.2f}")
        details.append(f"Market correlation: {correlation:.2f}")

    # Liquidity
    liquidity_score = estimate_liquidity_score(market_cap)
    details.append(f"Liquidity score: {liquidity_score:.1f}/10")

    # Composite risk score (lower is better)
    risk_components = [
        vol / 0.3 * 3,  # Volatility contribution (normalized to 30%)
        var_95 / 0.2 * 2,  # VaR contribution
        max_dd / 0.5 * 2,  # Drawdown contribution
        (1 - idiosyncratic_ratio) * 2,  # Systematic risk contribution
        (10 - liquidity_score) / 10 * 1  # Illiquidity contribution
    ]

    risk_score = min(10, max(0, sum(risk_components)))

    return RiskMetrics(
        volatility_annualized=vol,
        volatility_regime=vol_regime,
        var_95=var_95,
        var_99=var_99,
        max_drawdown=max_dd,
        current_drawdown=current_dd,
        sharpe_ratio=sharpe,
        sortino_ratio=sortino,
        beta=beta,
        correlation_to_market=correlation,
        idiosyncratic_ratio=idiosyncratic_ratio,
        liquidity_score=liquidity_score,
        risk_score=risk_score,
        details=details
    )


def calculate_kelly_criterion(
    win_probability: float,
    win_loss_ratio: float
) -> float:
    """
    Calculate Kelly Criterion for position sizing.
    f* = (p * b - q) / b
    where p = win prob, q = loss prob, b = win/loss ratio
    """
    if win_probability <= 0 or win_probability >= 1:
        return 0.0
    if win_loss_ratio <= 0:
        return 0.0

    q = 1 - win_probability
    kelly = (win_probability * win_loss_ratio - q) / win_loss_ratio

    # Cap at 25% for safety (Renaissance uses fractional Kelly)
    return max(0, min(0.25, kelly))


def calculate_position_size(
    portfolio_value: float,
    risk_score: float,
    signal_confidence: float,
    max_position_pct: float = 5.0
) -> float:
    """
    Calculate recommended position size as percentage of portfolio.
    Renaissance style: many small positions with high conviction.
    """
    # Base position from confidence
    base_position = signal_confidence / 100 * max_position_pct

    # Risk adjustment (higher risk = smaller position)
    risk_multiplier = 1 - (risk_score / 10) * 0.5

    # Final position
    position_pct = base_position * risk_multiplier

    return max(0, min(max_position_pct, position_pct))
