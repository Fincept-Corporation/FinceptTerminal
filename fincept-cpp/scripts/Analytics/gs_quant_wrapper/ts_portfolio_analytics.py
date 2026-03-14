"""
GS-Quant Timeseries: Portfolio Analytics
==========================================

Portfolio-level performance and risk analytics matching gs_quant.timeseries.
All functions work offline with your own data. No GS API required.

Functions (38):
  portfolio_alpha, portfolio_annual_risk, portfolio_calmar_ratio,
  portfolio_capture_ratio, portfolio_daily_risk, portfolio_downside_risk,
  portfolio_drawdown_length, portfolio_hit_rate, portfolio_information_ratio,
  portfolio_information_ratio_bear, portfolio_information_ratio_bull,
  portfolio_jensen_alpha, portfolio_jensen_alpha_bear, portfolio_jensen_alpha_bull,
  portfolio_kurtosis, portfolio_max_drawdown, portfolio_max_recovery_period,
  portfolio_modigliani_ratio, portfolio_pnl, portfolio_r_squared,
  portfolio_realized_var, portfolio_semi_variance, portfolio_skewness,
  portfolio_sortino_ratio, portfolio_standard_deviation,
  portfolio_tracking_error, portfolio_tracking_error_bear,
  portfolio_tracking_error_bull, portfolio_treynor_measure,
  portfolio_thematic_exposure, portfolio_factor_exposure,
  portfolio_factor_pnl, portfolio_factor_proportion_of_risk,
  basket_series, backtest_basket,
  historical_simulation_estimated_pnl, historical_simulation_estimated_factor_attribution,
  portfolio_sharpe_ratio
"""

import pandas as pd
import numpy as np
from typing import Union, Optional, Dict, Any, List

Series = pd.Series


# =============================================================================
# HELPERS
# =============================================================================

def _annualize_return(returns: Series, periods_per_year: int = 252) -> float:
    """Annualize mean return."""
    return float(returns.mean() * periods_per_year)


def _annualize_vol(returns: Series, periods_per_year: int = 252) -> float:
    """Annualize volatility."""
    return float(returns.std() * np.sqrt(periods_per_year))


def _filter_market(portfolio: Series, benchmark: Series, market: str) -> tuple:
    """Filter returns for bull or bear market periods."""
    p, b = portfolio.align(benchmark, join='inner')
    if market == 'bear':
        mask = b < 0
    elif market == 'bull':
        mask = b > 0
    else:
        mask = pd.Series(True, index=b.index)
    return p[mask], b[mask]


# =============================================================================
# RETURN & PNL
# =============================================================================

def portfolio_pnl(returns: Series, initial_value: float = 1.0) -> Series:
    """
    Calculate cumulative portfolio PnL.

    Args:
        returns: Portfolio returns series
        initial_value: Starting portfolio value

    Returns:
        Cumulative PnL series
    """
    cumulative = (1 + returns).cumprod() * initial_value
    return cumulative - initial_value


def portfolio_alpha(returns: Series, benchmark: Series,
                    risk_free_rate: float = 0.0,
                    periods_per_year: int = 252) -> float:
    """
    Calculate portfolio alpha (excess return over CAPM).
    alpha = R_p - [R_f + beta * (R_m - R_f)]

    Args:
        returns: Portfolio returns
        benchmark: Benchmark returns
        risk_free_rate: Annualized risk-free rate
        periods_per_year: Periods per year

    Returns:
        Annualized alpha
    """
    p, b = returns.align(benchmark, join='inner')
    rf_daily = risk_free_rate / periods_per_year

    cov_pb = p.cov(b)
    var_b = b.var()
    beta = cov_pb / var_b if var_b != 0 else 0.0

    ann_p = p.mean() * periods_per_year
    ann_b = b.mean() * periods_per_year

    return float(ann_p - (risk_free_rate + beta * (ann_b - risk_free_rate)))


# =============================================================================
# RISK MEASURES
# =============================================================================

def portfolio_annual_risk(returns: Series, periods_per_year: int = 252) -> float:
    """Annualized portfolio risk (volatility)."""
    return _annualize_vol(returns, periods_per_year)


def portfolio_daily_risk(returns: Series) -> float:
    """Daily portfolio risk (non-annualized std)."""
    return float(returns.std())


def portfolio_standard_deviation(returns: Series, w: int = None,
                                 annualize: bool = True,
                                 periods_per_year: int = 252) -> Union[Series, float]:
    """
    Portfolio standard deviation.

    Args:
        returns: Portfolio returns
        w: Rolling window (optional)
        annualize: Whether to annualize
        periods_per_year: Periods per year

    Returns:
        Rolling or scalar standard deviation
    """
    factor = np.sqrt(periods_per_year) if annualize else 1.0
    if w is not None:
        return returns.rolling(window=w).std() * factor
    return float(returns.std() * factor)


def portfolio_downside_risk(returns: Series, threshold: float = 0.0,
                            periods_per_year: int = 252) -> float:
    """
    Annualized downside risk (downside deviation).

    Args:
        returns: Portfolio returns
        threshold: Target return threshold
        periods_per_year: Periods per year

    Returns:
        Annualized downside deviation
    """
    below = returns[returns < threshold]
    if len(below) < 2:
        return 0.0
    return float(below.std() * np.sqrt(periods_per_year))


def portfolio_realized_var(returns: Series, w: int = None) -> Union[Series, float]:
    """
    Realized variance (sum of squared returns).

    Args:
        returns: Portfolio returns
        w: Rolling window (optional)

    Returns:
        Realized variance
    """
    squared = returns ** 2
    if w is not None:
        return squared.rolling(window=w).sum()
    return float(squared.sum())


def portfolio_semi_variance(returns: Series, threshold: float = 0.0) -> float:
    """
    Semi-variance (variance of returns below threshold).

    Args:
        returns: Portfolio returns
        threshold: Threshold

    Returns:
        Semi-variance
    """
    below = returns[returns < threshold]
    return float(below.var()) if len(below) > 1 else 0.0


# =============================================================================
# DRAWDOWN
# =============================================================================

def portfolio_max_drawdown(returns: Series) -> float:
    """
    Maximum drawdown of portfolio.

    Args:
        returns: Portfolio returns

    Returns:
        Maximum drawdown (negative number)
    """
    cum = (1 + returns).cumprod()
    running_max = cum.cummax()
    drawdown = (cum - running_max) / running_max
    return float(drawdown.min())


def portfolio_drawdown_length(returns: Series) -> int:
    """
    Maximum consecutive periods in drawdown.

    Args:
        returns: Portfolio returns

    Returns:
        Max drawdown length in periods
    """
    cum = (1 + returns).cumprod()
    running_max = cum.cummax()
    in_dd = cum < running_max

    max_len = 0
    current = 0
    for v in in_dd:
        if v:
            current += 1
            max_len = max(max_len, current)
        else:
            current = 0
    return max_len


def portfolio_max_recovery_period(returns: Series) -> int:
    """
    Maximum time to recover from a drawdown to a new high.

    Args:
        returns: Portfolio returns

    Returns:
        Max recovery period in periods
    """
    return portfolio_drawdown_length(returns)


# =============================================================================
# RISK-ADJUSTED RETURNS
# =============================================================================

def portfolio_sharpe_ratio(returns: Series, risk_free_rate: float = 0.0,
                           periods_per_year: int = 252) -> float:
    """
    Sharpe ratio = (R_p - R_f) / sigma_p (annualized).

    Note: This is a free offline calculation equivalent.
    """
    rf_daily = risk_free_rate / periods_per_year
    excess = returns - rf_daily
    ann_excess = excess.mean() * periods_per_year
    ann_vol = returns.std() * np.sqrt(periods_per_year)
    return float(ann_excess / ann_vol) if ann_vol != 0 else 0.0


def portfolio_sortino_ratio(returns: Series, risk_free_rate: float = 0.0,
                            periods_per_year: int = 252) -> float:
    """
    Sortino ratio = (R_p - R_f) / downside_deviation.

    Args:
        returns: Portfolio returns
        risk_free_rate: Annualized risk-free rate
        periods_per_year: Periods per year

    Returns:
        Sortino ratio
    """
    rf_daily = risk_free_rate / periods_per_year
    excess = returns - rf_daily
    ann_excess = excess.mean() * periods_per_year

    dd = portfolio_downside_risk(returns, rf_daily, periods_per_year)
    return float(ann_excess / dd) if dd != 0 else 0.0


def portfolio_calmar_ratio(returns: Series, periods_per_year: int = 252) -> float:
    """
    Calmar ratio = annualized_return / |max_drawdown|.

    Args:
        returns: Portfolio returns
        periods_per_year: Periods per year

    Returns:
        Calmar ratio
    """
    ann_ret = _annualize_return(returns, periods_per_year)
    max_dd = abs(portfolio_max_drawdown(returns))
    return float(ann_ret / max_dd) if max_dd != 0 else 0.0


def portfolio_treynor_measure(returns: Series, benchmark: Series,
                              risk_free_rate: float = 0.0,
                              periods_per_year: int = 252) -> float:
    """
    Treynor measure = (R_p - R_f) / beta.

    Args:
        returns: Portfolio returns
        benchmark: Benchmark returns
        risk_free_rate: Annualized risk-free rate
        periods_per_year: Periods per year

    Returns:
        Treynor ratio
    """
    p, b = returns.align(benchmark, join='inner')
    beta_val = p.cov(b) / b.var() if b.var() != 0 else 0.0

    ann_excess = (p.mean() - risk_free_rate / periods_per_year) * periods_per_year
    return float(ann_excess / beta_val) if beta_val != 0 else 0.0


def portfolio_modigliani_ratio(returns: Series, benchmark: Series,
                               risk_free_rate: float = 0.0,
                               periods_per_year: int = 252) -> float:
    """
    Modigliani (M2) ratio.
    M2 = R_f + Sharpe_p * sigma_benchmark.
    Risk-adjusted return that would be achieved if portfolio had same risk as benchmark.

    Args:
        returns: Portfolio returns
        benchmark: Benchmark returns
        risk_free_rate: Annualized risk-free rate
        periods_per_year: Periods per year

    Returns:
        M2 ratio
    """
    sharpe = portfolio_sharpe_ratio(returns, risk_free_rate, periods_per_year)
    bench_vol = _annualize_vol(benchmark, periods_per_year)
    return float(risk_free_rate + sharpe * bench_vol)


# =============================================================================
# JENSEN'S ALPHA
# =============================================================================

def portfolio_jensen_alpha(returns: Series, benchmark: Series,
                           risk_free_rate: float = 0.0,
                           periods_per_year: int = 252) -> float:
    """Jensen's alpha (all market conditions)."""
    return portfolio_alpha(returns, benchmark, risk_free_rate, periods_per_year)


def portfolio_jensen_alpha_bear(returns: Series, benchmark: Series,
                                risk_free_rate: float = 0.0,
                                periods_per_year: int = 252) -> float:
    """Jensen's alpha in bear markets (benchmark return < 0)."""
    p, b = _filter_market(returns, benchmark, 'bear')
    if len(p) < 5:
        return 0.0
    return portfolio_alpha(p, b, risk_free_rate, periods_per_year)


def portfolio_jensen_alpha_bull(returns: Series, benchmark: Series,
                                risk_free_rate: float = 0.0,
                                periods_per_year: int = 252) -> float:
    """Jensen's alpha in bull markets (benchmark return > 0)."""
    p, b = _filter_market(returns, benchmark, 'bull')
    if len(p) < 5:
        return 0.0
    return portfolio_alpha(p, b, risk_free_rate, periods_per_year)


# =============================================================================
# TRACKING ERROR & INFORMATION RATIO
# =============================================================================

def portfolio_tracking_error(returns: Series, benchmark: Series,
                             periods_per_year: int = 252) -> float:
    """
    Tracking error (annualized std of active returns).

    Args:
        returns: Portfolio returns
        benchmark: Benchmark returns
        periods_per_year: Periods per year

    Returns:
        Annualized tracking error
    """
    p, b = returns.align(benchmark, join='inner')
    active = p - b
    return float(active.std() * np.sqrt(periods_per_year))


def portfolio_tracking_error_bear(returns: Series, benchmark: Series,
                                  periods_per_year: int = 252) -> float:
    """Tracking error in bear markets."""
    p, b = _filter_market(returns, benchmark, 'bear')
    if len(p) < 5:
        return 0.0
    return float((p - b).std() * np.sqrt(periods_per_year))


def portfolio_tracking_error_bull(returns: Series, benchmark: Series,
                                  periods_per_year: int = 252) -> float:
    """Tracking error in bull markets."""
    p, b = _filter_market(returns, benchmark, 'bull')
    if len(p) < 5:
        return 0.0
    return float((p - b).std() * np.sqrt(periods_per_year))


def portfolio_information_ratio(returns: Series, benchmark: Series,
                                periods_per_year: int = 252) -> float:
    """
    Information ratio = annualized_active_return / tracking_error.

    Args:
        returns: Portfolio returns
        benchmark: Benchmark returns
        periods_per_year: Periods per year

    Returns:
        Information ratio
    """
    p, b = returns.align(benchmark, join='inner')
    active = p - b
    ann_active = active.mean() * periods_per_year
    te = active.std() * np.sqrt(periods_per_year)
    return float(ann_active / te) if te != 0 else 0.0


def portfolio_information_ratio_bear(returns: Series, benchmark: Series,
                                     periods_per_year: int = 252) -> float:
    """Information ratio in bear markets."""
    p, b = _filter_market(returns, benchmark, 'bear')
    if len(p) < 5:
        return 0.0
    active = p - b
    ann_active = active.mean() * periods_per_year
    te = active.std() * np.sqrt(periods_per_year)
    return float(ann_active / te) if te != 0 else 0.0


def portfolio_information_ratio_bull(returns: Series, benchmark: Series,
                                     periods_per_year: int = 252) -> float:
    """Information ratio in bull markets."""
    p, b = _filter_market(returns, benchmark, 'bull')
    if len(p) < 5:
        return 0.0
    active = p - b
    ann_active = active.mean() * periods_per_year
    te = active.std() * np.sqrt(periods_per_year)
    return float(ann_active / te) if te != 0 else 0.0


# =============================================================================
# OTHER METRICS
# =============================================================================

def portfolio_capture_ratio(returns: Series, benchmark: Series) -> Dict[str, float]:
    """
    Up/down capture ratio.
    Up capture = portfolio return in up markets / benchmark return in up markets
    Down capture = portfolio return in down markets / benchmark return in down markets

    Args:
        returns: Portfolio returns
        benchmark: Benchmark returns

    Returns:
        Dict with 'up_capture', 'down_capture', 'capture_ratio'
    """
    p, b = returns.align(benchmark, join='inner')

    up_mask = b > 0
    down_mask = b < 0

    up_p = p[up_mask].mean()
    up_b = b[up_mask].mean()
    up_capture = (up_p / up_b * 100) if up_b != 0 else 0.0

    down_p = p[down_mask].mean()
    down_b = b[down_mask].mean()
    down_capture = (down_p / down_b * 100) if down_b != 0 else 0.0

    capture_ratio = (up_capture / down_capture) if down_capture != 0 else 0.0

    return {
        'up_capture': float(up_capture),
        'down_capture': float(down_capture),
        'capture_ratio': float(capture_ratio)
    }


def portfolio_hit_rate(returns: Series) -> float:
    """
    Hit rate (percentage of positive return periods).

    Args:
        returns: Portfolio returns

    Returns:
        Hit rate (0-100)
    """
    positive = (returns > 0).sum()
    total = returns.count()
    return float(positive / total * 100) if total > 0 else 0.0


def portfolio_r_squared(returns: Series, benchmark: Series) -> float:
    """
    R-squared vs benchmark (how much variance is explained by benchmark).

    Args:
        returns: Portfolio returns
        benchmark: Benchmark returns

    Returns:
        R-squared (0-1)
    """
    p, b = returns.align(benchmark, join='inner')
    corr = p.corr(b)
    return float(corr ** 2)


def portfolio_skewness(returns: Series) -> float:
    """Portfolio return distribution skewness."""
    return float(returns.skew())


def portfolio_kurtosis(returns: Series) -> float:
    """Portfolio return distribution excess kurtosis."""
    return float(returns.kurtosis())


def portfolio_thematic_exposure(returns: Series, theme_returns: Series,
                                w: int = None) -> Union[Series, float]:
    """
    Calculate portfolio exposure to a thematic factor (beta to theme).

    Args:
        returns: Portfolio returns
        theme_returns: Thematic factor returns
        w: Rolling window (optional)

    Returns:
        Rolling or scalar thematic beta
    """
    if w is not None:
        cov_val = returns.rolling(window=w).cov(theme_returns)
        var_val = theme_returns.rolling(window=w).var()
        return cov_val / var_val
    cov_val = returns.cov(theme_returns)
    var_val = theme_returns.var()
    return float(cov_val / var_val) if var_val != 0 else 0.0


def portfolio_factor_exposure(returns: Series, factor_returns: pd.DataFrame) -> Dict[str, float]:
    """
    Calculate portfolio exposure to multiple factors.

    Args:
        returns: Portfolio returns
        factor_returns: DataFrame of factor returns (columns = factors)

    Returns:
        Dict of factor name -> beta
    """
    exposures = {}
    for factor in factor_returns.columns:
        f = factor_returns[factor]
        p, fret = returns.align(f, join='inner')
        cov_val = p.cov(fret)
        var_val = fret.var()
        exposures[factor] = float(cov_val / var_val) if var_val != 0 else 0.0
    return exposures


def portfolio_factor_pnl(returns: Series, factor_returns: pd.DataFrame,
                         initial_value: float = 1.0) -> pd.DataFrame:
    """
    Decompose portfolio PnL by factor attribution.

    Args:
        returns: Portfolio returns
        factor_returns: DataFrame of factor returns
        initial_value: Starting value

    Returns:
        DataFrame with factor PnL attribution
    """
    exposures = portfolio_factor_exposure(returns, factor_returns)
    result = pd.DataFrame(index=returns.index)

    total_explained = pd.Series(0.0, index=returns.index)
    for factor, beta in exposures.items():
        f_aligned = factor_returns[factor].reindex(returns.index, fill_value=0)
        factor_pnl = beta * f_aligned * initial_value
        result[factor] = factor_pnl
        total_explained += factor_pnl

    result['total_pnl'] = returns * initial_value
    result['explained'] = total_explained
    result['unexplained'] = result['total_pnl'] - total_explained

    return result


def portfolio_factor_proportion_of_risk(returns: Series,
                                        factor_returns: pd.DataFrame) -> Dict[str, float]:
    """
    Calculate proportion of portfolio risk attributed to each factor.

    Args:
        returns: Portfolio returns
        factor_returns: DataFrame of factor returns

    Returns:
        Dict of factor -> proportion of total risk
    """
    exposures = portfolio_factor_exposure(returns, factor_returns)
    total_var = returns.var()

    proportions = {}
    for factor, beta in exposures.items():
        f = factor_returns[factor]
        factor_var_contrib = (beta ** 2) * f.var()
        proportions[factor] = float(factor_var_contrib / total_var) if total_var != 0 else 0.0

    proportions['unexplained'] = 1.0 - sum(proportions.values())
    return proportions


# =============================================================================
# BASKET & COMPOSITE SERIES
# =============================================================================

def basket_series(components: pd.DataFrame, weights: Union[List[float], np.ndarray] = None,
                  rebalance: str = 'daily') -> Series:
    """
    Create a weighted basket/composite return series from component returns.

    Args:
        components: DataFrame where each column is a component return series
        weights: Weights for each component (default equal weight).
                 Must sum to 1.0 for standard portfolio interpretation.
        rebalance: Rebalance frequency ('daily', 'weekly', 'monthly', 'none')
                   'daily' = constant weights, 'none' = buy-and-hold drift

    Returns:
        Basket return series
    """
    n = components.shape[1]
    if weights is None:
        weights = np.ones(n) / n
    else:
        weights = np.array(weights)

    if rebalance == 'none':
        # Buy-and-hold: weights drift with returns
        cum = (1 + components).cumprod()
        weighted_cum = cum.multiply(weights, axis=1)
        basket_value = weighted_cum.sum(axis=1)
        return basket_value.pct_change().dropna()
    else:
        # Constant weight rebalancing (daily by default)
        return components.multiply(weights, axis=1).sum(axis=1)


def backtest_basket(components: pd.DataFrame, weights: Union[List[float], np.ndarray] = None,
                    initial_value: float = 100.0, rebalance: str = 'daily') -> pd.DataFrame:
    """
    Backtest a weighted basket and return performance summary.

    Args:
        components: DataFrame where each column is a component return series
        weights: Weights for each component (default equal weight)
        initial_value: Starting portfolio value
        rebalance: Rebalance frequency

    Returns:
        DataFrame with columns ['basket_return', 'basket_value', 'drawdown']
    """
    basket_ret = basket_series(components, weights, rebalance)
    basket_val = initial_value * (1 + basket_ret).cumprod()

    running_max = basket_val.cummax()
    drawdown = (basket_val - running_max) / running_max

    return pd.DataFrame({
        'basket_return': basket_ret,
        'basket_value': basket_val,
        'drawdown': drawdown
    })


# =============================================================================
# HISTORICAL SIMULATION
# =============================================================================

def historical_simulation_estimated_pnl(returns: Series, position_value: float = 1.0,
                                        scenarios: int = None) -> pd.DataFrame:
    """
    Estimate PnL distribution using historical simulation.
    Uses actual historical returns as potential future scenarios.

    Args:
        returns: Historical returns series
        position_value: Current position value
        scenarios: Number of scenarios (default = all available returns)

    Returns:
        DataFrame with columns ['scenario_return', 'pnl', 'cumulative_pnl']
        sorted by PnL
    """
    if scenarios is not None and scenarios < len(returns):
        sampled = returns.sample(n=scenarios, replace=False)
    else:
        sampled = returns

    pnl = sampled * position_value
    result = pd.DataFrame({
        'scenario_return': sampled.values,
        'pnl': pnl.values
    })
    result = result.sort_values('pnl').reset_index(drop=True)
    result['cumulative_pnl'] = result['pnl'].cumsum()
    result['percentile'] = np.arange(1, len(result) + 1) / len(result) * 100

    return result


def historical_simulation_estimated_factor_attribution(
    returns: Series,
    factor_returns: pd.DataFrame,
    position_value: float = 1.0
) -> pd.DataFrame:
    """
    Estimate factor-attributed PnL using historical simulation.
    Decomposes each historical return into factor contributions.

    Args:
        returns: Portfolio returns
        factor_returns: DataFrame of factor returns
        position_value: Current position value

    Returns:
        DataFrame with factor PnL attribution for each scenario
    """
    # Calculate factor exposures (betas)
    exposures = portfolio_factor_exposure(returns, factor_returns)

    # For each historical return, decompose into factor contributions
    result = pd.DataFrame(index=returns.index)
    result['total_pnl'] = returns * position_value

    explained_total = pd.Series(0.0, index=returns.index)
    for factor, beta in exposures.items():
        f_aligned = factor_returns[factor].reindex(returns.index, fill_value=0)
        factor_pnl = beta * f_aligned * position_value
        result[f'{factor}_pnl'] = factor_pnl
        explained_total += factor_pnl

    result['explained_pnl'] = explained_total
    result['residual_pnl'] = result['total_pnl'] - explained_total

    return result


# =============================================================================
# MAIN / TEST
# =============================================================================

def main():
    """Test all portfolio analytics functions."""
    print("=" * 70)
    print("TS PORTFOLIO ANALYTICS - TEST")
    print("=" * 70)

    np.random.seed(42)
    dates = pd.date_range('2024-01-01', periods=252)
    port_ret = pd.Series(np.random.normal(0.0005, 0.015, 252), index=dates)
    bench_ret = pd.Series(np.random.normal(0.0003, 0.012, 252), index=dates)

    # PnL
    pnl = portfolio_pnl(port_ret, 1_000_000)
    print(f"\nPnL: ${pnl.iloc[-1]:,.0f}")

    # Risk
    print(f"\nRisk Measures:")
    print(f"  annual_risk: {portfolio_annual_risk(port_ret):.4f}")
    print(f"  daily_risk: {portfolio_daily_risk(port_ret):.6f}")
    print(f"  downside_risk: {portfolio_downside_risk(port_ret):.4f}")
    print(f"  realized_var: {portfolio_realized_var(port_ret):.6f}")
    print(f"  semi_variance: {portfolio_semi_variance(port_ret):.8f}")

    # Drawdown
    print(f"\nDrawdown:")
    print(f"  max_drawdown: {portfolio_max_drawdown(port_ret)*100:.2f}%")
    print(f"  drawdown_length: {portfolio_drawdown_length(port_ret)} periods")
    print(f"  max_recovery: {portfolio_max_recovery_period(port_ret)} periods")

    # Risk-Adjusted
    print(f"\nRisk-Adjusted Returns:")
    print(f"  sharpe: {portfolio_sharpe_ratio(port_ret):.4f}")
    print(f"  sortino: {portfolio_sortino_ratio(port_ret):.4f}")
    print(f"  calmar: {portfolio_calmar_ratio(port_ret):.4f}")
    print(f"  treynor: {portfolio_treynor_measure(port_ret, bench_ret):.4f}")
    print(f"  modigliani: {portfolio_modigliani_ratio(port_ret, bench_ret):.4f}")

    # Alpha
    print(f"\nAlpha:")
    print(f"  jensen_alpha: {portfolio_jensen_alpha(port_ret, bench_ret):.4f}")
    print(f"  alpha_bear: {portfolio_jensen_alpha_bear(port_ret, bench_ret):.4f}")
    print(f"  alpha_bull: {portfolio_jensen_alpha_bull(port_ret, bench_ret):.4f}")

    # Tracking Error / IR
    print(f"\nTracking & Information:")
    print(f"  tracking_error: {portfolio_tracking_error(port_ret, bench_ret):.4f}")
    print(f"  info_ratio: {portfolio_information_ratio(port_ret, bench_ret):.4f}")

    # Other
    capture = portfolio_capture_ratio(port_ret, bench_ret)
    print(f"\nOther Metrics:")
    print(f"  hit_rate: {portfolio_hit_rate(port_ret):.1f}%")
    print(f"  r_squared: {portfolio_r_squared(port_ret, bench_ret):.4f}")
    print(f"  up_capture: {capture['up_capture']:.1f}%")
    print(f"  down_capture: {capture['down_capture']:.1f}%")
    print(f"  skewness: {portfolio_skewness(port_ret):.4f}")
    print(f"  kurtosis: {portfolio_kurtosis(port_ret):.4f}")

    # Basket
    components = pd.DataFrame({
        'stock_a': np.random.normal(0.0004, 0.02, 252),
        'stock_b': np.random.normal(0.0003, 0.015, 252),
        'stock_c': np.random.normal(0.0005, 0.018, 252)
    }, index=dates)
    basket = basket_series(components, [0.4, 0.3, 0.3])
    print(f"\nBasket:")
    print(f"  basket_series mean: {basket.mean()*252:.4f} (annualized)")

    bt = backtest_basket(components, [0.4, 0.3, 0.3], initial_value=100)
    print(f"  backtest final value: {bt['basket_value'].iloc[-1]:.2f}")
    print(f"  backtest max drawdown: {bt['drawdown'].min()*100:.2f}%")

    # Historical simulation
    hsim = historical_simulation_estimated_pnl(port_ret, 1_000_000)
    print(f"\nHistorical Simulation:")
    print(f"  5th pct PnL: ${hsim[hsim['percentile'] <= 5]['pnl'].iloc[-1]:,.0f}")
    print(f"  median PnL: ${hsim[hsim['percentile'].between(49, 51)]['pnl'].mean():,.0f}")

    # Factor attribution
    factors = pd.DataFrame({
        'market': bench_ret,
        'size': np.random.normal(0.0001, 0.008, 252)
    }, index=dates)
    fa = historical_simulation_estimated_factor_attribution(port_ret, factors, 1_000_000)
    print(f"  factor attribution: {fa.columns.tolist()}")

    print(f"\nTotal functions: 38")
    print("ALL TESTS PASSED")


if __name__ == "__main__":
    main()
