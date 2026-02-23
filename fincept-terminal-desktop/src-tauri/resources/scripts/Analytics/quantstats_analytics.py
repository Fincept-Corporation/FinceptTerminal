"""
QuantStats Analytics Module
============================
Wraps the quantstats library to provide portfolio performance analytics,
risk metrics, drawdown analysis, rolling statistics, and benchmark comparisons.

Usage:
    python quantstats_analytics.py <action> <tickers_json> [benchmark] [period] [risk_free_rate]

Actions:
    stats           - 50+ performance/risk metrics
    returns         - Monthly/yearly returns data + distribution
    drawdown        - Drawdown periods and analysis
    rolling         - Rolling Sharpe, volatility, beta
    full_report     - All of the above combined
    html_report     - Full HTML tearsheet as base64
"""

import sys
import json
import warnings
import base64
import tempfile
import os

warnings.filterwarnings("ignore")

import numpy as np
import pandas as pd
import quantstats as qs
import yfinance as yf


def safe_float(val):
    """Convert numpy/pandas types to JSON-safe Python float."""
    if val is None or (isinstance(val, float) and (np.isnan(val) or np.isinf(val))):
        return None
    try:
        return round(float(val), 6)
    except (TypeError, ValueError):
        return None


def safe_series_to_dict(s):
    """Convert a pandas Series to a JSON-safe dict."""
    if s is None:
        return {}
    result = {}
    for k, v in s.items():
        key = str(k)
        result[key] = safe_float(v)
    return result



_ticker_cache = {}

def _resolve_ticker(ticker: str) -> str:
    """Try to resolve a bare ticker to a yfinance-compatible symbol.

    Some portfolios store bare symbols like 'TCS' or 'RELIANCE' without
    the exchange suffix. yfinance needs 'TCS.NS' or 'RELIANCE.NS' for Indian stocks.
    This function tries the ticker as-is first, then common suffixes.
    """
    if ticker in _ticker_cache:
        return _ticker_cache[ticker]

    # If it already has a suffix/exchange marker, use as-is
    if '.' in ticker or '-' in ticker or '=' in ticker or ticker.startswith('^'):
        _ticker_cache[ticker] = ticker
        return ticker

    def _quick_check(symbol: str) -> bool:
        """Quick check if a yfinance symbol has valid data."""
        try:
            t = yf.Ticker(symbol)
            hist = t.history(period='5d')
            return hist is not None and len(hist) > 1
        except Exception:
            return False

    # Try the bare ticker first
    if _quick_check(ticker):
        _ticker_cache[ticker] = ticker
        return ticker

    # Try common exchange suffixes (Indian exchanges first since this app has Indian broker integrations)
    for suffix in ['.NS', '.BO', '.L', '.TO', '.AX', '.HK', '.T']:
        candidate = ticker + suffix
        if _quick_check(candidate):
            sys.stderr.write(f"[QuantStats] Resolved bare ticker '{ticker}' -> '{candidate}'\n")
            _ticker_cache[ticker] = candidate
            return candidate

    # Fall back to original
    sys.stderr.write(f"[QuantStats] WARNING: Could not resolve ticker '{ticker}' - using as-is\n")
    _ticker_cache[ticker] = ticker
    return ticker


def _download_single_ticker(ticker: str, period: str) -> pd.Series:
    """Download close prices for a single ticker, returning a clean Series."""
    # Resolve bare tickers to yfinance-compatible symbols
    resolved = _resolve_ticker(ticker)

    try:
        data = yf.download(resolved, period=period, progress=False, auto_adjust=True)
    except Exception:
        data = yf.download(resolved, period=period, progress=False)

    if data is None or data.empty:
        raise ValueError(f"No data for {ticker} (tried as '{resolved}')")

    # Handle MultiIndex columns (yfinance >= 0.2.31)
    if isinstance(data.columns, pd.MultiIndex):
        if "Close" in data.columns.get_level_values(0):
            prices = data["Close"]
        elif "Adj Close" in data.columns.get_level_values(0):
            prices = data["Adj Close"]
        else:
            prices = data.iloc[:, 0]
        # If still a DataFrame, take the first column
        if isinstance(prices, pd.DataFrame):
            prices = prices.iloc[:, 0]
    else:
        if "Close" in data.columns:
            prices = data["Close"]
        elif "Adj Close" in data.columns:
            prices = data["Adj Close"]
        else:
            prices = data.iloc[:, 0]

    prices = prices.dropna()
    prices.name = ticker
    prices.index = pd.to_datetime(prices.index)
    # Flatten MultiIndex date index if present
    if isinstance(prices.index, pd.MultiIndex):
        prices.index = prices.index.get_level_values(0)
    return prices


def download_portfolio_returns(tickers_weights: dict, period: str = "1y") -> pd.Series:
    """Download price data and compute weighted portfolio returns.

    Downloads each ticker individually to avoid numpy concatenation errors
    when tickers have different data lengths, then aligns via inner join.
    """
    tickers = list(tickers_weights.keys())
    weights_raw = np.array(list(tickers_weights.values()))

    # Normalize weights
    weights_raw = weights_raw / weights_raw.sum()

    # Download each ticker individually to avoid shape mismatch
    all_prices = {}
    for ticker in tickers:
        try:
            all_prices[ticker] = _download_single_ticker(ticker, period)
        except Exception as e:
            sys.stderr.write(f"Warning: Could not download {ticker}: {e}\n")

    if not all_prices:
        raise ValueError(f"No price data found for any tickers: {tickers}")

    # Combine into DataFrame with inner join (only dates all tickers share)
    prices = pd.DataFrame(all_prices)
    prices = prices.dropna()

    if prices.empty:
        raise ValueError(f"No overlapping price data for tickers: {list(all_prices.keys())}")

    # Calculate daily returns
    returns = prices.pct_change().dropna()

    # Determine available tickers (in case some failed to download)
    available = [t for t in tickers if t in returns.columns]
    if not available:
        col_map = {c.upper(): c for c in returns.columns}
        available = [col_map[t.upper()] for t in tickers if t.upper() in col_map]

    if not available:
        raise ValueError(f"No valid return data for tickers: {tickers}. Columns found: {list(returns.columns)}")

    # Adjust weights for available tickers only
    avail_indices = [i for i, t in enumerate(tickers) if t in available]
    avail_weights = weights_raw[avail_indices] if len(avail_indices) == len(available) else np.ones(len(available))
    avail_weights = avail_weights / avail_weights.sum()

    # Weighted portfolio returns
    portfolio_returns = (returns[available] * avail_weights).sum(axis=1)
    portfolio_returns.name = "Portfolio"
    portfolio_returns.index = pd.to_datetime(portfolio_returns.index)

    # Ensure clean 1D float Series (prevents numpy shape issues inside quantstats)
    portfolio_returns = portfolio_returns.astype(float)
    if hasattr(portfolio_returns, 'to_numpy'):
        # Flatten to guarantee 1D
        vals = portfolio_returns.to_numpy().flatten()
        portfolio_returns = pd.Series(vals, index=portfolio_returns.index, name="Portfolio")

    return portfolio_returns


def download_benchmark_returns(benchmark: str, period: str = "1y") -> pd.Series:
    """Download benchmark returns."""
    prices = _download_single_ticker(benchmark, period)
    returns = prices.pct_change().dropna().astype(float)
    # Ensure clean 1D Series
    vals = returns.to_numpy().flatten()
    returns = pd.Series(vals, index=returns.index, name=benchmark)
    return returns


def compute_stats(portfolio_returns: pd.Series, benchmark_returns: pd.Series, rf: float) -> dict:
    """Compute 50+ performance and risk metrics using quantstats."""
    qs.extend_pandas()

    metrics = {}

    def _safe_metric(func, *args, **kwargs):
        """Call a quantstats metric function safely, returning None on error."""
        try:
            return safe_float(func(*args, **kwargs))
        except Exception:
            return None

    # Performance metrics
    metrics["cagr"] = _safe_metric(qs.stats.cagr, portfolio_returns)
    metrics["cumulative_return"] = _safe_metric(qs.stats.comp, portfolio_returns)
    metrics["sharpe"] = _safe_metric(qs.stats.sharpe, portfolio_returns, rf=rf)
    metrics["sortino"] = _safe_metric(qs.stats.sortino, portfolio_returns, rf=rf)
    metrics["calmar"] = _safe_metric(qs.stats.calmar, portfolio_returns)
    metrics["max_drawdown"] = _safe_metric(qs.stats.max_drawdown, portfolio_returns)
    metrics["avg_drawdown"] = _safe_metric(qs.stats.avg_loss, portfolio_returns)
    metrics["volatility"] = _safe_metric(qs.stats.volatility, portfolio_returns)
    metrics["win_rate"] = _safe_metric(qs.stats.win_rate, portfolio_returns)
    metrics["avg_win"] = _safe_metric(qs.stats.avg_win, portfolio_returns)
    metrics["avg_loss"] = _safe_metric(qs.stats.avg_loss, portfolio_returns)
    metrics["best_day"] = _safe_metric(qs.stats.best, portfolio_returns)
    metrics["worst_day"] = _safe_metric(qs.stats.worst, portfolio_returns)
    metrics["best_month"] = _safe_metric(qs.stats.best, portfolio_returns, aggregate="M")
    metrics["worst_month"] = _safe_metric(qs.stats.worst, portfolio_returns, aggregate="M")
    metrics["profit_factor"] = _safe_metric(qs.stats.profit_factor, portfolio_returns)
    metrics["payoff_ratio"] = _safe_metric(qs.stats.payoff_ratio, portfolio_returns)
    metrics["skew"] = _safe_metric(qs.stats.skew, portfolio_returns)
    metrics["kurtosis"] = _safe_metric(qs.stats.kurtosis, portfolio_returns)
    metrics["kelly_criterion"] = _safe_metric(qs.stats.kelly_criterion, portfolio_returns)
    metrics["risk_of_ruin"] = _safe_metric(qs.stats.risk_of_ruin, portfolio_returns)
    metrics["tail_ratio"] = _safe_metric(qs.stats.tail_ratio, portfolio_returns)
    metrics["common_sense_ratio"] = _safe_metric(qs.stats.common_sense_ratio, portfolio_returns)
    metrics["outlier_win_ratio"] = _safe_metric(qs.stats.outlier_win_ratio, portfolio_returns)
    metrics["outlier_loss_ratio"] = _safe_metric(qs.stats.outlier_loss_ratio, portfolio_returns)

    # Extended performance metrics
    try:
        metrics["smart_sharpe"] = safe_float(qs.stats.smart_sharpe(portfolio_returns, rf=rf))
    except Exception:
        metrics["smart_sharpe"] = None
    try:
        metrics["smart_sortino"] = safe_float(qs.stats.smart_sortino(portfolio_returns, rf=rf))
    except Exception:
        metrics["smart_sortino"] = None
    try:
        metrics["adjusted_sortino"] = safe_float(qs.stats.adjusted_sortino(portfolio_returns, rf=rf))
    except Exception:
        metrics["adjusted_sortino"] = None
    try:
        metrics["omega"] = safe_float(qs.stats.omega(portfolio_returns, rf=rf))
    except Exception:
        metrics["omega"] = None
    try:
        metrics["ulcer_index"] = safe_float(qs.stats.ulcer_index(portfolio_returns))
    except Exception:
        metrics["ulcer_index"] = None
    try:
        metrics["upi"] = safe_float(qs.stats.ulcer_performance_index(portfolio_returns))
    except Exception:
        metrics["upi"] = None
    try:
        metrics["serenity_index"] = safe_float(qs.stats.serenity_index(portfolio_returns))
    except Exception:
        metrics["serenity_index"] = None
    try:
        metrics["risk_return_ratio"] = safe_float(qs.stats.risk_return_ratio(portfolio_returns))
    except Exception:
        metrics["risk_return_ratio"] = None
    try:
        metrics["recovery_factor"] = safe_float(qs.stats.recovery_factor(portfolio_returns))
    except Exception:
        metrics["recovery_factor"] = None
    try:
        metrics["cpc_index"] = safe_float(qs.stats.cpc_index(portfolio_returns))
    except Exception:
        metrics["cpc_index"] = None
    try:
        metrics["exposure"] = safe_float(qs.stats.exposure(portfolio_returns))
    except Exception:
        metrics["exposure"] = None

    # Win/Loss streaks
    try:
        metrics["consecutive_wins"] = int(qs.stats.consecutive_wins(portfolio_returns))
    except Exception:
        metrics["consecutive_wins"] = None
    try:
        metrics["consecutive_losses"] = int(qs.stats.consecutive_losses(portfolio_returns))
    except Exception:
        metrics["consecutive_losses"] = None

    # Expected return (geometric mean)
    try:
        metrics["expected_return"] = safe_float(qs.stats.expected_return(portfolio_returns))
    except Exception:
        metrics["expected_return"] = None

    # Probabilistic Sharpe Ratio
    try:
        metrics["probabilistic_sharpe_ratio"] = safe_float(qs.stats.probabilistic_sharpe_ratio(portfolio_returns, rf=rf))
    except Exception:
        metrics["probabilistic_sharpe_ratio"] = None

    # RAR (Risk-Adjusted Return)
    try:
        metrics["rar"] = safe_float(qs.stats.rar(portfolio_returns))
    except Exception:
        metrics["rar"] = None

    # Value at Risk
    metrics["var_95"] = _safe_metric(qs.stats.value_at_risk, portfolio_returns, confidence=0.95)
    metrics["cvar_95"] = _safe_metric(qs.stats.cvar, portfolio_returns, confidence=0.95)
    metrics["var_99"] = _safe_metric(qs.stats.value_at_risk, portfolio_returns, confidence=0.99)

    # Drawdown metrics
    metrics["max_drawdown_duration"] = None
    try:
        dd_details = qs.stats.drawdown_details(qs.stats.to_drawdown_series(portfolio_returns))
        if dd_details is not None and len(dd_details) > 0:
            metrics["max_drawdown_duration"] = int(dd_details["days"].max()) if "days" in dd_details.columns else None
    except Exception as e:
        sys.stderr.write(f"[QuantStats] max_drawdown_duration error: {e}\n")

    # Benchmark comparison metrics
    if benchmark_returns is not None and len(benchmark_returns) > 0:
        # Align dates
        common_idx = portfolio_returns.index.intersection(benchmark_returns.index)
        if len(common_idx) > 10:
            p = portfolio_returns.loc[common_idx]
            b = benchmark_returns.loc[common_idx]

            try:
                greeks = qs.stats.greeks(p, b)
                if isinstance(greeks, dict):
                    metrics["alpha"] = safe_float(greeks.get("alpha", None))
                    metrics["beta"] = safe_float(greeks.get("beta", None))
                else:
                    metrics["alpha"] = None
                    metrics["beta"] = None
            except Exception:
                metrics["alpha"] = None
                metrics["beta"] = None

            try:
                metrics["information_ratio"] = safe_float(qs.stats.information_ratio(p, b))
            except Exception:
                metrics["information_ratio"] = None
            try:
                metrics["treynor_ratio"] = safe_float(qs.stats.treynor_ratio(p, b))
            except Exception:
                metrics["treynor_ratio"] = None

            try:
                metrics["r_squared"] = safe_float(qs.stats.r_squared(p, b))
            except Exception:
                metrics["r_squared"] = None

            try:
                metrics["benchmark_cagr"] = safe_float(qs.stats.cagr(b))
                metrics["benchmark_sharpe"] = safe_float(qs.stats.sharpe(b, rf=rf))
                metrics["benchmark_volatility"] = safe_float(qs.stats.volatility(b))
                metrics["benchmark_max_drawdown"] = safe_float(qs.stats.max_drawdown(b))
            except Exception:
                metrics["benchmark_cagr"] = None
                metrics["benchmark_sharpe"] = None
                metrics["benchmark_volatility"] = None
                metrics["benchmark_max_drawdown"] = None

            try:
                metrics["correlation"] = safe_float(p.corr(b))
            except Exception:
                metrics["correlation"] = None

            try:
                excess = (1 + p).prod() - (1 + b).prod()
                metrics["excess_return"] = safe_float(excess)
            except Exception:
                metrics["excess_return"] = None

    return metrics


def compute_returns_analysis(portfolio_returns: pd.Series) -> dict:
    """Compute monthly/yearly returns breakdown."""
    result = {}

    # Monthly returns
    monthly = portfolio_returns.resample("ME").apply(lambda x: (1 + x).prod() - 1)
    monthly_data = []
    for date, ret in monthly.items():
        monthly_data.append({
            "year": date.year,
            "month": date.month,
            "return": safe_float(ret)
        })
    result["monthly_returns"] = monthly_data

    # Yearly returns
    yearly = portfolio_returns.resample("YE").apply(lambda x: (1 + x).prod() - 1)
    yearly_data = []
    for date, ret in yearly.items():
        yearly_data.append({
            "year": date.year,
            "return": safe_float(ret)
        })
    result["yearly_returns"] = yearly_data

    # Distribution stats
    result["distribution"] = {
        "mean": safe_float(portfolio_returns.mean()),
        "std": safe_float(portfolio_returns.std()),
        "skew": safe_float(portfolio_returns.skew()),
        "kurtosis": safe_float(portfolio_returns.kurtosis()),
        "min": safe_float(portfolio_returns.min()),
        "max": safe_float(portfolio_returns.max()),
        "median": safe_float(portfolio_returns.median()),
        "positive_days": int((portfolio_returns > 0).sum()),
        "negative_days": int((portfolio_returns < 0).sum()),
        "total_days": len(portfolio_returns),
    }

    # Monthly returns heatmap data (year x month matrix)
    heatmap = {}
    for item in monthly_data:
        year = str(item["year"])
        if year not in heatmap:
            heatmap[year] = {}
        heatmap[year][str(item["month"])] = item["return"]
    result["heatmap"] = heatmap

    return result


def compute_drawdowns(portfolio_returns: pd.Series) -> dict:
    """Compute drawdown analysis."""
    result = {}

    # Drawdown series
    dd_series = qs.stats.to_drawdown_series(portfolio_returns)
    dd_data = []
    for date, val in dd_series.items():
        dd_data.append({
            "date": date.strftime("%Y-%m-%d"),
            "drawdown": safe_float(val)
        })
    result["drawdown_series"] = dd_data

    # Drawdown details (top drawdown periods)
    try:
        dd_details = qs.stats.drawdown_details(dd_series)
        if dd_details is not None and len(dd_details) > 0:
            # Flatten MultiIndex columns (e.g. ('AAPL', 'start') -> 'start')
            if isinstance(dd_details.columns, pd.MultiIndex):
                dd_details.columns = dd_details.columns.get_level_values(-1)
            periods = []
            for _, row in dd_details.head(10).iterrows():
                period = {}
                for col in dd_details.columns:
                    val = row[col]
                    if val is pd.NaT or (isinstance(val, float) and np.isnan(val)):
                        period[col] = None
                    elif isinstance(val, pd.Timestamp):
                        period[col] = val.strftime("%Y-%m-%d")
                    elif isinstance(val, str):
                        period[col] = val
                    elif isinstance(val, (int, np.integer)):
                        period[col] = int(val)
                    else:
                        period[col] = safe_float(val)
                periods.append(period)
            result["drawdown_periods"] = periods
        else:
            result["drawdown_periods"] = []
    except Exception as e:
        sys.stderr.write(f"[QuantStats] drawdown_periods error: {e}\n")
        result["drawdown_periods"] = []

    return result


def compute_rolling(portfolio_returns: pd.Series, benchmark_returns: pd.Series, rf: float) -> dict:
    """Compute rolling statistics."""
    result = {}

    windows = [21, 63, 126, 252]  # 1M, 3M, 6M, 1Y

    for window in windows:
        if len(portfolio_returns) < window:
            continue

        label = f"{window}d"

        # Rolling Sharpe
        try:
            rolling_sharpe = qs.stats.rolling_sharpe(portfolio_returns, rf=rf, rolling_period=window)
            if rolling_sharpe is not None:
                sharpe_data = []
                for date, val in rolling_sharpe.dropna().items():
                    sharpe_data.append({
                        "date": date.strftime("%Y-%m-%d"),
                        "value": safe_float(val)
                    })
                result[f"rolling_sharpe_{label}"] = sharpe_data
        except Exception as e:
            sys.stderr.write(f"[QuantStats] rolling_sharpe_{label} error: {e}\n")

        # Rolling Volatility
        try:
            rolling_vol = qs.stats.rolling_volatility(portfolio_returns, rolling_period=window)
            if rolling_vol is not None:
                vol_data = []
                for date, val in rolling_vol.dropna().items():
                    vol_data.append({
                        "date": date.strftime("%Y-%m-%d"),
                        "value": safe_float(val)
                    })
                result[f"rolling_volatility_{label}"] = vol_data
        except Exception as e:
            sys.stderr.write(f"[QuantStats] rolling_volatility_{label} error: {e}\n")

        # Rolling Sortino
        try:
            rolling_sortino = qs.stats.rolling_sortino(portfolio_returns, rf=rf, rolling_period=window)
            if rolling_sortino is not None:
                sortino_data = []
                for date, val in rolling_sortino.dropna().items():
                    sortino_data.append({
                        "date": date.strftime("%Y-%m-%d"),
                        "value": safe_float(val)
                    })
                result[f"rolling_sortino_{label}"] = sortino_data
        except Exception as e:
            sys.stderr.write(f"[QuantStats] rolling_sortino_{label} error: {e}\n")

    # Cumulative returns series
    cum_returns = (1 + portfolio_returns).cumprod() - 1
    cum_data = []
    for date, val in cum_returns.items():
        cum_data.append({
            "date": date.strftime("%Y-%m-%d"),
            "value": safe_float(val)
        })
    result["cumulative_returns"] = cum_data

    # Benchmark cumulative returns
    if benchmark_returns is not None:
        common_idx = portfolio_returns.index.intersection(benchmark_returns.index)
        if len(common_idx) > 0:
            b = benchmark_returns.loc[common_idx]
            bench_cum = (1 + b).cumprod() - 1
            bench_data = []
            for date, val in bench_cum.items():
                bench_data.append({
                    "date": date.strftime("%Y-%m-%d"),
                    "value": safe_float(val)
                })
            result["benchmark_cumulative_returns"] = bench_data

    return result


def compute_montecarlo(portfolio_returns: pd.Series, sims: int = 1000) -> dict:
    """Run Monte Carlo simulations via bootstrap resampling of historical returns.

    qs.stats.montecarlo* functions do not exist in quantstats 0.0.64.
    We implement a bootstrap simulation: each path draws T daily returns
    (with replacement) from the historical return distribution, then
    compounds them to produce a cumulative return path.
    """
    result = {}

    returns_arr = portfolio_returns.dropna().values.astype(float)
    n = len(returns_arr)
    if n < 20:
        null = {"wealth_distribution": None, "cagr_distribution": None,
                "sharpe_distribution": None, "max_drawdown_distribution": None,
                "simulation_paths": None}
        result.update(null)
        return result

    rng = np.random.default_rng()
    # Each simulated path has the same length as the observed history
    T = n

    def _full_distribution(values: np.ndarray) -> dict:
        return {
            "mean": safe_float(np.mean(values)),
            "std":  safe_float(np.std(values)),
            "min":  safe_float(np.min(values)),
            "max":  safe_float(np.max(values)),
            "p5":   safe_float(np.percentile(values, 5)),
            "p10":  safe_float(np.percentile(values, 10)),
            "p25":  safe_float(np.percentile(values, 25)),
            "p50":  safe_float(np.percentile(values, 50)),
            "p75":  safe_float(np.percentile(values, 75)),
            "p90":  safe_float(np.percentile(values, 90)),
            "p95":  safe_float(np.percentile(values, 95)),
            "count": int(sims),
        }

    # Generate all paths at once: shape (T, sims)
    sampled = rng.choice(returns_arr, size=(T, sims), replace=True)

    # Cumulative return paths: (T, sims)  — values are cumulative returns (e.g. 0.15 = +15%)
    cum_paths = np.cumprod(1 + sampled, axis=0) - 1

    # Terminal wealth (final cumulative return per path)
    final_values = cum_paths[-1, :]
    result["wealth_distribution"] = _full_distribution(final_values)

    # CAGR distribution
    years = T / 252.0
    cagr_values = np.where(
        final_values > -1,
        (1 + final_values) ** (1.0 / max(years, 1e-6)) - 1,
        -1.0
    )
    result["cagr_distribution"] = _full_distribution(cagr_values)

    # Sharpe distribution (annualised, per path)
    path_means = sampled.mean(axis=0)
    path_stds  = sampled.std(axis=0)
    with np.errstate(divide='ignore', invalid='ignore'):
        sharpe_values = np.where(path_stds > 0, (path_means / path_stds) * np.sqrt(252), 0.0)
    result["sharpe_distribution"] = _full_distribution(sharpe_values)

    # Max drawdown distribution (per path)
    wealth = 1 + cum_paths                          # absolute wealth index
    running_max = np.maximum.accumulate(wealth, axis=0)
    drawdowns = (wealth - running_max) / running_max
    max_dd_values = drawdowns.min(axis=0)
    result["max_drawdown_distribution"] = _full_distribution(max_dd_values)

    # --- Simulation paths for chart (sample 100 evenly-spaced paths) ---
    # Downsample to ~100 time points
    max_points = min(100, T)
    row_step = max(1, T // max_points)
    row_indices = list(range(0, T, row_step))
    if row_indices[-1] != T - 1:
        row_indices.append(T - 1)

    # Pick 100 paths sorted by final value (fan shape)
    sorted_cols = np.argsort(final_values)
    step = max(1, sims // 100)
    selected = sorted_cols[::step][:100]

    paths = []
    for col_idx in selected:
        path_vals = [safe_float(cum_paths[r, col_idx]) for r in row_indices]
        paths.append(path_vals)

    # Percentile bands at each sampled time step
    cum_at_rows = cum_paths[row_indices, :]
    p5_band  = [safe_float(np.percentile(cum_at_rows[i], 5))  for i in range(len(row_indices))]
    p50_band = [safe_float(np.percentile(cum_at_rows[i], 50)) for i in range(len(row_indices))]
    p95_band = [safe_float(np.percentile(cum_at_rows[i], 95)) for i in range(len(row_indices))]

    # Actual historical cumulative return path (same time-step sampling)
    actual_cum = (np.cumprod(1 + returns_arr) - 1)
    original_path = [safe_float(actual_cum[r]) for r in row_indices]

    result["simulation_paths"] = {
        "paths": paths,
        "time_steps": len(row_indices),
        "num_paths": len(paths),
        "p5_band": p5_band,
        "p50_band": p50_band,
        "p95_band": p95_band,
        "original_path": original_path,
    }

    return result


def generate_html_report(portfolio_returns: pd.Series, benchmark_returns: pd.Series) -> str:
    """Generate full HTML tearsheet and return as base64."""
    with tempfile.NamedTemporaryFile(suffix=".html", delete=False, mode="w") as f:
        tmp_path = f.name

    try:
        qs.reports.html(
            portfolio_returns,
            benchmark=benchmark_returns,
            output=tmp_path,
            title="Portfolio QuantStats Report"
        )

        with open(tmp_path, "r", encoding="utf-8") as f:
            html_content = f.read()

        return base64.b64encode(html_content.encode("utf-8")).decode("utf-8")
    finally:
        try:
            os.unlink(tmp_path)
        except Exception:
            pass


def main():
    if len(sys.argv) < 3:
        print(json.dumps({"error": "Usage: quantstats_analytics.py <action> <tickers_json> [benchmark] [period] [risk_free_rate]"}))
        sys.exit(1)

    action = sys.argv[1]
    tickers_json = sys.argv[2]
    benchmark = sys.argv[3] if len(sys.argv) > 3 else "SPY"
    period = sys.argv[4] if len(sys.argv) > 4 else "1y"
    risk_free_rate = float(sys.argv[5]) if len(sys.argv) > 5 else 0.02
    num_sims = int(sys.argv[6]) if len(sys.argv) > 6 else 1000

    try:
        tickers_weights = json.loads(tickers_json)
    except json.JSONDecodeError as e:
        print(json.dumps({"error": f"Invalid tickers JSON: {str(e)}"}))
        sys.exit(1)

    if not tickers_weights:
        print(json.dumps({"error": "No tickers provided"}))
        sys.exit(1)

    try:
        # Download data
        sys.stderr.write(f"[QuantStats] Script version: 2026-02-04-v3\n")
        sys.stderr.write(f"[QuantStats] Action={action}, Tickers={list(tickers_weights.keys())}, Benchmark={benchmark}, Period={period}\n")
        portfolio_returns = download_portfolio_returns(tickers_weights, period)
        sys.stderr.write(f"[QuantStats] Portfolio returns: shape={portfolio_returns.shape}, dtype={portfolio_returns.dtype}, len={len(portfolio_returns)}\n")

        benchmark_returns = None
        try:
            benchmark_returns = download_benchmark_returns(benchmark, period)
        except Exception as e:
            sys.stderr.write(f"Warning: Could not download benchmark {benchmark}: {e}\n")

        result = {"success": True, "action": action}

        if action == "stats":
            result["data"] = compute_stats(portfolio_returns, benchmark_returns, risk_free_rate)

        elif action == "returns":
            result["data"] = compute_returns_analysis(portfolio_returns)

        elif action == "drawdown":
            result["data"] = compute_drawdowns(portfolio_returns)

        elif action == "rolling":
            result["data"] = compute_rolling(portfolio_returns, benchmark_returns, risk_free_rate)

        elif action == "montecarlo":
            result["data"] = compute_montecarlo(portfolio_returns, sims=num_sims)

        elif action == "full_report":
            result["data"] = {
                "stats": compute_stats(portfolio_returns, benchmark_returns, risk_free_rate),
                "returns": compute_returns_analysis(portfolio_returns),
                "drawdown": compute_drawdowns(portfolio_returns),
                "rolling": compute_rolling(portfolio_returns, benchmark_returns, risk_free_rate),
                "montecarlo": compute_montecarlo(portfolio_returns, sims=num_sims),
            }

        elif action == "html_report":
            html_b64 = generate_html_report(portfolio_returns, benchmark_returns)
            result["data"] = {"html_base64": html_b64}

        else:
            result = {"error": f"Unknown action: {action}"}

        print(json.dumps(result))

    except Exception as e:
        import traceback
        sys.stderr.write(traceback.format_exc())
        print(json.dumps({"error": str(e), "success": False}))
        # Exit 0 so Rust extracts our JSON error instead of wrapping it in "Script failed"
        sys.exit(0)


if __name__ == "__main__":
    main()
