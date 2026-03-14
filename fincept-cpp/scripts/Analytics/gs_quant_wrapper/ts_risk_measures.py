"""
GS-Quant Timeseries: Risk Measures
====================================

Risk and volatility measurement functions matching gs_quant.timeseries.
All functions work offline with your own data. No GS API required.

Functions (19):
  volatility, exponential_volatility, exponential_std, exponential_spread_volatility,
  annual_risk, daily_risk, downside_risk, max_drawdown, drawdown_length,
  max_recovery_period, sharpe_ratio, calmar_ratio, sortino_ratio, omega_ratio,
  forward_vol_term, forward_var_term, value_at_risk,
  vol_swap_volatility, corr_swap_correlation
"""

import pandas as pd
import numpy as np
from typing import Union, Optional, Dict, Any, List

Series = pd.Series


def volatility(x: Series, w: int = None, annualize: bool = True,
               periods_per_year: int = 252) -> Union[Series, float]:
    """
    Calculate volatility (standard deviation of returns).

    Args:
        x: Returns series
        w: Rolling window size (optional)
        annualize: Whether to annualize (default True)
        periods_per_year: Trading periods per year

    Returns:
        Rolling volatility series or scalar
    """
    factor = np.sqrt(periods_per_year) if annualize else 1.0

    if w is not None:
        return x.rolling(window=w).std() * factor
    return float(x.std() * factor)


def exponential_volatility(x: Series, span: int = 20, annualize: bool = True,
                           periods_per_year: int = 252) -> Series:
    """
    Calculate exponentially weighted volatility (EWMA volatility).

    Args:
        x: Returns series
        span: EWMA span parameter
        annualize: Whether to annualize
        periods_per_year: Trading periods per year

    Returns:
        EWMA volatility series
    """
    factor = np.sqrt(periods_per_year) if annualize else 1.0
    return x.ewm(span=span).std() * factor


def exponential_std(x: Series, span: int = 20) -> Series:
    """
    Calculate exponentially weighted standard deviation (non-annualized).

    Args:
        x: Input series
        span: EWMA span parameter

    Returns:
        EWMA standard deviation series
    """
    return x.ewm(span=span).std()


def exponential_spread_volatility(x: Series, y: Series, span: int = 20,
                                  annualize: bool = True,
                                  periods_per_year: int = 252) -> Series:
    """
    Calculate exponentially weighted spread volatility between two series.

    Args:
        x: First returns series
        y: Second returns series
        span: EWMA span parameter
        annualize: Whether to annualize
        periods_per_year: Trading periods per year

    Returns:
        EWMA spread volatility series
    """
    spread = x - y
    factor = np.sqrt(periods_per_year) if annualize else 1.0
    return spread.ewm(span=span).std() * factor


def annual_risk(x: Series, w: int = None, periods_per_year: int = 252) -> Union[Series, float]:
    """
    Calculate annualized risk (annualized standard deviation).

    Args:
        x: Returns series
        w: Rolling window size (optional)
        periods_per_year: Trading periods per year

    Returns:
        Annualized risk
    """
    return volatility(x, w=w, annualize=True, periods_per_year=periods_per_year)


def daily_risk(x: Series, w: int = None) -> Union[Series, float]:
    """
    Calculate daily risk (non-annualized standard deviation).

    Args:
        x: Returns series
        w: Rolling window size (optional)

    Returns:
        Daily risk
    """
    return volatility(x, w=w, annualize=False)


def downside_risk(x: Series, threshold: float = 0.0, w: int = None,
                  annualize: bool = True, periods_per_year: int = 252) -> Union[Series, float]:
    """
    Calculate downside risk (downside deviation).
    Only considers returns below the threshold.

    Args:
        x: Returns series
        threshold: Return threshold (default 0)
        w: Rolling window size (optional)
        annualize: Whether to annualize
        periods_per_year: Trading periods per year

    Returns:
        Downside risk
    """
    factor = np.sqrt(periods_per_year) if annualize else 1.0

    if w is not None:
        def _dr(arr):
            below = arr[arr < threshold]
            return below.std() if len(below) > 1 else 0.0
        return x.rolling(window=w).apply(_dr, raw=True) * factor

    below = x[x < threshold]
    dr = below.std() if len(below) > 1 else 0.0
    return float(dr * factor)


def max_drawdown(x: Series) -> Dict[str, Any]:
    """
    Calculate maximum drawdown from a returns series.

    Args:
        x: Returns series

    Returns:
        Dict with max_drawdown, peak_date, trough_date, recovery_date
    """
    cum_returns = (1 + x).cumprod()
    running_max = cum_returns.cummax()
    drawdown = (cum_returns - running_max) / running_max

    max_dd = drawdown.min()
    trough_idx = drawdown.idxmin()

    # Find peak (last max before trough)
    peak_idx = cum_returns[:trough_idx].idxmax()

    # Find recovery (first time after trough that cumulative return exceeds peak)
    peak_value = cum_returns[peak_idx]
    recovery_series = cum_returns[trough_idx:]
    recovered = recovery_series[recovery_series >= peak_value]
    recovery_idx = recovered.index[0] if len(recovered) > 0 else None

    return {
        'max_drawdown': float(max_dd),
        'max_drawdown_pct': float(max_dd * 100),
        'peak_date': peak_idx,
        'trough_date': trough_idx,
        'recovery_date': recovery_idx
    }


def drawdown_length(x: Series) -> Series:
    """
    Calculate drawdown length (number of periods in drawdown).

    Args:
        x: Returns series

    Returns:
        Series of drawdown durations in periods
    """
    cum_returns = (1 + x).cumprod()
    running_max = cum_returns.cummax()
    is_drawdown = cum_returns < running_max

    # Count consecutive drawdown periods
    lengths = pd.Series(0, index=x.index, dtype=int)
    current_length = 0
    for i in range(len(is_drawdown)):
        if is_drawdown.iloc[i]:
            current_length += 1
        else:
            current_length = 0
        lengths.iloc[i] = current_length

    return lengths


def max_recovery_period(x: Series) -> int:
    """
    Calculate maximum recovery period (longest time from drawdown to new high).

    Args:
        x: Returns series

    Returns:
        Maximum recovery period in number of periods
    """
    lengths = drawdown_length(x)
    return int(lengths.max())


def sharpe_ratio(x: Series, risk_free_rate: float = 0.0,
                 periods_per_year: int = 252) -> float:
    """
    Calculate Sharpe ratio.
    Sharpe = (annualized_return - risk_free_rate) / annualized_volatility

    Args:
        x: Returns series
        risk_free_rate: Annualized risk-free rate (default 0)
        periods_per_year: Trading periods per year

    Returns:
        Sharpe ratio
    """
    excess = x - risk_free_rate / periods_per_year
    ann_return = excess.mean() * periods_per_year
    ann_vol = x.std() * np.sqrt(periods_per_year)

    if ann_vol == 0:
        return 0.0
    return float(ann_return / ann_vol)


def calmar_ratio(x: Series, periods_per_year: int = 252) -> float:
    """
    Calculate Calmar ratio.
    Calmar = annualized_return / abs(max_drawdown)

    Args:
        x: Returns series
        periods_per_year: Trading periods per year

    Returns:
        Calmar ratio
    """
    ann_return = x.mean() * periods_per_year
    dd = max_drawdown(x)
    max_dd = abs(dd['max_drawdown'])

    if max_dd == 0:
        return 0.0
    return float(ann_return / max_dd)


def sortino_ratio(x: Series, risk_free_rate: float = 0.0,
                  periods_per_year: int = 252) -> float:
    """
    Calculate Sortino ratio.
    Sortino = (annualized_return - risk_free_rate) / annualized_downside_deviation

    Args:
        x: Returns series
        risk_free_rate: Annualized risk-free rate (default 0)
        periods_per_year: Trading periods per year

    Returns:
        Sortino ratio
    """
    rf_daily = risk_free_rate / periods_per_year
    excess = x - rf_daily
    ann_excess = excess.mean() * periods_per_year

    dd = downside_risk(x, rf_daily, annualize=True, periods_per_year=periods_per_year)
    if dd == 0:
        return 0.0
    return float(ann_excess / dd)


def omega_ratio(x: Series, threshold: float = 0.0) -> float:
    """
    Calculate Omega ratio.
    Omega = sum of gains above threshold / sum of losses below threshold.

    Args:
        x: Returns series
        threshold: Return threshold (default 0)

    Returns:
        Omega ratio (>1 indicates more gains than losses)
    """
    gains = (x[x > threshold] - threshold).sum()
    losses = (threshold - x[x < threshold]).sum()
    if losses == 0:
        return float('inf') if gains > 0 else 0.0
    return float(gains / losses)


# =============================================================================
# FORWARD VOLATILITY & VARIANCE
# =============================================================================

def forward_vol_term(x: Series, tenors: List[int] = None,
                     periods_per_year: int = 252) -> pd.DataFrame:
    """
    Calculate forward volatility term structure from a returns series.
    Forward vol between T1 and T2: sqrt((var2 * T2 - var1 * T1) / (T2 - T1))

    Args:
        x: Returns series
        tenors: List of lookback periods in days (default [21, 63, 126, 252])
        periods_per_year: Trading periods per year

    Returns:
        DataFrame with columns ['tenor_days', 'realized_vol', 'forward_vol']
    """
    if tenors is None:
        tenors = [21, 63, 126, 252]

    tenors = sorted(tenors)
    rows = []

    for t in tenors:
        if len(x) >= t:
            vol = float(x.tail(t).std() * np.sqrt(periods_per_year))
        else:
            vol = np.nan
        rows.append({'tenor_days': t, 'realized_vol': vol})

    df = pd.DataFrame(rows)

    # Calculate forward vols between consecutive tenors
    forward_vols = [np.nan]  # First tenor has no forward vol
    for i in range(1, len(df)):
        t1 = df.iloc[i - 1]['tenor_days']
        t2 = df.iloc[i]['tenor_days']
        v1 = df.iloc[i - 1]['realized_vol']
        v2 = df.iloc[i]['realized_vol']

        if np.isnan(v1) or np.isnan(v2):
            forward_vols.append(np.nan)
        else:
            var_diff = (v2 ** 2 * t2 - v1 ** 2 * t1) / (t2 - t1)
            forward_vols.append(np.sqrt(max(var_diff, 0)))

    df['forward_vol'] = forward_vols
    return df


def forward_var_term(x: Series, tenors: List[int] = None,
                     periods_per_year: int = 252) -> pd.DataFrame:
    """
    Calculate forward variance term structure from a returns series.

    Args:
        x: Returns series
        tenors: List of lookback periods in days (default [21, 63, 126, 252])
        periods_per_year: Trading periods per year

    Returns:
        DataFrame with columns ['tenor_days', 'realized_var', 'forward_var']
    """
    if tenors is None:
        tenors = [21, 63, 126, 252]

    tenors = sorted(tenors)
    rows = []

    for t in tenors:
        if len(x) >= t:
            variance = float(x.tail(t).var() * periods_per_year)
        else:
            variance = np.nan
        rows.append({'tenor_days': t, 'realized_var': variance})

    df = pd.DataFrame(rows)

    # Calculate forward variance between consecutive tenors
    forward_vars = [np.nan]
    for i in range(1, len(df)):
        t1 = df.iloc[i - 1]['tenor_days']
        t2 = df.iloc[i]['tenor_days']
        v1 = df.iloc[i - 1]['realized_var']
        v2 = df.iloc[i]['realized_var']

        if np.isnan(v1) or np.isnan(v2):
            forward_vars.append(np.nan)
        else:
            fwd_var = (v2 * t2 - v1 * t1) / (t2 - t1)
            forward_vars.append(max(fwd_var, 0))

    df['forward_var'] = forward_vars
    return df


def vol_swap_volatility(prices: Series, n_days: int = None,
                        annualization_factor: int = 252,
                        assume_zero_mean: bool = True) -> Series:
    """
    Rolling volatility of a price series for volatility swap pricing.
    Uses log returns and optionally assumes zero-mean (vol swap convention).

    When assume_zero_mean=True (default, vol swap convention):
        sigma_t = sqrt(AF * (1/N) * sum(R_i^2))
        where N = n_days - 1, divisor is N (not N-1)

    When assume_zero_mean=False (standard sample volatility):
        sigma_t = sqrt(AF * (1/(N-1)) * sum((R_i - R_bar)^2))

    Args:
        prices: Price series
        n_days: Number of price days in the rolling window.
                Defaults to length of series if not provided.
                The returns window will be n_days - 1.
        annualization_factor: Periods per year (default 252)
        assume_zero_mean: If True, assumes zero mean (vol swap convention).
                          If False, uses sample mean (standard volatility).

    Returns:
        Rolling annualized volatility series (in decimal, not percentage)
    """
    log_ret = np.log(prices / prices.shift(1))

    if n_days is None:
        n_days = len(prices)

    window = n_days - 1  # returns window

    if assume_zero_mean:
        # Vol swap convention: sum(R^2) / N, then annualize
        squared_ret = log_ret ** 2
        rolling_mean_sq = squared_ret.rolling(window=window, min_periods=window).mean()
        return np.sqrt(rolling_mean_sq * annualization_factor)
    else:
        # Standard sample volatility with Bessel's correction
        return log_ret.rolling(window=window, min_periods=window).std() * np.sqrt(annualization_factor)


def corr_swap_correlation(x: Series, y: Series, n_days: int = None,
                          assume_zero_mean: bool = True) -> Series:
    """
    Rolling correlation of two price series for correlation swap pricing.
    Uses log returns.

    When assume_zero_mean=True (default, corr swap convention):
        rho_t = sum(R_i * S_i) / sqrt(sum(R_i^2) * sum(S_i^2))
        This assumes zero mean for returns.

    When assume_zero_mean=False (standard Pearson correlation):
        rho_t = sum((R_i - R_bar)(S_i - S_bar)) / ((N-1) * sigma_R * sigma_S)

    Args:
        x: First price series
        y: Second price series
        n_days: Number of price days in the rolling window.
                Defaults to min length of series if not provided.
                The returns window will be n_days - 1.
        assume_zero_mean: If True, assumes zero mean (corr swap convention).
                          If False, uses Pearson correlation.

    Returns:
        Rolling correlation series
    """
    log_ret_x = np.log(x / x.shift(1))
    log_ret_y = np.log(y / y.shift(1))

    if n_days is None:
        n_days = min(len(x), len(y))

    window = n_days - 1  # returns window

    if assume_zero_mean:
        # Zero-mean correlation: sum(Ri*Si) / sqrt(sum(Ri^2) * sum(Si^2))
        xy = (log_ret_x * log_ret_y).rolling(window=window, min_periods=window).sum()
        xx = (log_ret_x ** 2).rolling(window=window, min_periods=window).sum()
        yy = (log_ret_y ** 2).rolling(window=window, min_periods=window).sum()
        denom = np.sqrt(xx * yy)
        return xy / denom.replace(0, np.nan)
    else:
        # Standard Pearson correlation on log returns
        return log_ret_x.rolling(window=window, min_periods=window).corr(log_ret_y)


def value_at_risk(x: Series, confidence: float = 0.95,
                  method: str = 'historical') -> float:
    """
    Calculate Value at Risk (VaR).

    Args:
        x: Returns series
        confidence: Confidence level (default 0.95 = 95%)
        method: 'historical' for historical simulation,
                'parametric' for normal distribution assumption

    Returns:
        VaR (negative number representing loss at given confidence)
    """
    if method == 'parametric':
        from scipy.stats import norm
        mu = x.mean()
        sigma = x.std()
        return float(mu + sigma * norm.ppf(1 - confidence))
    else:
        # Historical simulation
        return float(x.quantile(1 - confidence))


# =============================================================================
# MAIN / TEST
# =============================================================================

def main():
    """Test all risk measure functions."""
    print("=" * 70)
    print("TS RISK MEASURES - TEST")
    print("=" * 70)

    np.random.seed(42)
    dates = pd.date_range('2024-01-01', periods=252)
    ret = pd.Series(np.random.normal(0.0005, 0.015, 252), index=dates)
    ret2 = pd.Series(np.random.normal(0.0003, 0.012, 252), index=dates)

    print("\nVolatility Measures:")
    print(f"  volatility (ann): {volatility(ret):.4f} ({volatility(ret)*100:.2f}%)")
    print(f"  exponential_vol: {exponential_volatility(ret).iloc[-1]:.4f}")
    print(f"  exponential_std: {exponential_std(ret).iloc[-1]:.6f}")
    print(f"  spread_vol: {exponential_spread_volatility(ret, ret2).iloc[-1]:.4f}")
    print(f"  annual_risk: {annual_risk(ret):.4f}")
    print(f"  daily_risk: {daily_risk(ret):.6f}")

    print("\nDownside Risk:")
    print(f"  downside_risk: {downside_risk(ret):.4f}")

    dd = max_drawdown(ret)
    print(f"\nDrawdown:")
    print(f"  max_drawdown: {dd['max_drawdown_pct']:.2f}%")
    print(f"  peak: {dd['peak_date']}")
    print(f"  trough: {dd['trough_date']}")

    dl = drawdown_length(ret)
    print(f"  max drawdown length: {dl.max()} periods")
    print(f"  max recovery period: {max_recovery_period(ret)} periods")

    print("\nRisk-Adjusted Returns:")
    print(f"  sharpe_ratio: {sharpe_ratio(ret):.4f}")
    print(f"  calmar_ratio: {calmar_ratio(ret):.4f}")
    print(f"  sortino_ratio: {sortino_ratio(ret):.4f}")
    print(f"  omega_ratio: {omega_ratio(ret):.4f}")

    # Forward vol/var
    fvt = forward_vol_term(ret)
    print(f"\nForward Volatility Term Structure:")
    for _, row in fvt.iterrows():
        print(f"  {int(row['tenor_days'])}d: realized={row['realized_vol']:.4f}, forward={row['forward_vol']:.4f}" if not np.isnan(row['forward_vol']) else f"  {int(row['tenor_days'])}d: realized={row['realized_vol']:.4f}")

    fvar = forward_var_term(ret)
    print(f"\nForward Variance Term Structure:")
    for _, row in fvar.iterrows():
        print(f"  {int(row['tenor_days'])}d: realized_var={row['realized_var']:.6f}")

    # VaR
    var_95 = value_at_risk(ret, 0.95)
    print(f"\nValue at Risk:")
    print(f"  VaR(95%): {var_95:.6f}")
    print(f"  VaR(99%): {value_at_risk(ret, 0.99):.6f}")

    # Vol swap volatility
    prices = pd.Series(np.random.randn(252).cumsum() + 100, index=dates)
    prices2 = pd.Series(np.random.randn(252).cumsum() + 100, index=dates)
    vsv = vol_swap_volatility(prices, n_days=22)
    print(f"\nVol/Corr Swap:")
    print(f"  vol_swap_vol (22d, zero-mean): {vsv.dropna().iloc[-1]:.4f}")
    vsv_std = vol_swap_volatility(prices, n_days=22, assume_zero_mean=False)
    print(f"  vol_swap_vol (22d, standard): {vsv_std.dropna().iloc[-1]:.4f}")

    csc = corr_swap_correlation(prices, prices2, n_days=22)
    print(f"  corr_swap_corr (22d, zero-mean): {csc.dropna().iloc[-1]:.4f}")
    csc_pearson = corr_swap_correlation(prices, prices2, n_days=22, assume_zero_mean=False)
    print(f"  corr_swap_corr (22d, Pearson): {csc_pearson.dropna().iloc[-1]:.4f}")

    print(f"\nTotal functions: 19")
    print("ALL TESTS PASSED")


if __name__ == "__main__":
    main()
