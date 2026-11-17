"""
AI Quant Lab - Feature Engineering Module
Tools for creating and manipulating financial features

Features:
- Technical Indicators (MA, EMA, RSI, MACD, Bollinger Bands, etc.)
- Price-based Features (Returns, Momentum, Volatility)
- Volume Features
- Custom Feature Expression Engine
- Feature Selection Tools
"""

import json
import os
import re
import sys
from typing import Dict, List, Any, Optional, Union, Callable
import warnings
warnings.filterwarnings('ignore')

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), '..'))
import fincept_json

try:
    import pandas as pd
    import numpy as np
    PANDAS_AVAILABLE = True
except ImportError:
    PANDAS_AVAILABLE = False
    pd = None
    np = None


class FeatureEngineer:
    """
    Feature engineering toolkit for quantitative finance.
    """

    def __init__(self):
        self.features = {}
        self.last_ic_scores = {}

    # Technical Indicators
    def moving_average(self, data: pd.Series, window: int = 20, method: str = 'sma') -> pd.Series:
        """
        Calculate moving average.

        Args:
            data: Price series
            window: Window size
            method: 'sma' (simple) or 'ema' (exponential)

        Returns:
            Moving average series
        """
        if method == 'sma':
            return data.rolling(window=window).mean()
        elif method == 'ema':
            return data.ewm(span=window, adjust=False).mean()
        else:
            raise ValueError(f"Unknown method: {method}")

    def rsi(self, data: pd.Series, window: int = 14) -> pd.Series:
        """
        Relative Strength Index.

        Args:
            data: Price series
            window: RSI window (typically 14)

        Returns:
            RSI series (0-100)
        """
        delta = data.diff()
        gain = delta.where(delta > 0, 0)
        loss = -delta.where(delta < 0, 0)

        avg_gain = gain.ewm(span=window, adjust=False).mean()
        avg_loss = loss.ewm(span=window, adjust=False).mean()

        rs = avg_gain / (avg_loss + 1e-10)
        rsi = 100 - (100 / (1 + rs))

        return rsi

    def macd(self, data: pd.Series, fast: int = 12, slow: int = 26, signal: int = 9) -> Dict[str, pd.Series]:
        """
        MACD (Moving Average Convergence Divergence).

        Args:
            data: Price series
            fast: Fast EMA period
            slow: Slow EMA period
            signal: Signal line period

        Returns:
            Dict with 'macd', 'signal', and 'histogram'
        """
        ema_fast = data.ewm(span=fast, adjust=False).mean()
        ema_slow = data.ewm(span=slow, adjust=False).mean()

        macd_line = ema_fast - ema_slow
        signal_line = macd_line.ewm(span=signal, adjust=False).mean()
        histogram = macd_line - signal_line

        return {
            'macd': macd_line,
            'signal': signal_line,
            'histogram': histogram
        }

    def bollinger_bands(self, data: pd.Series, window: int = 20, num_std: float = 2.0) -> Dict[str, pd.Series]:
        """
        Bollinger Bands.

        Args:
            data: Price series
            window: Window size
            num_std: Number of standard deviations

        Returns:
            Dict with 'upper', 'middle', and 'lower' bands
        """
        middle = data.rolling(window=window).mean()
        std = data.rolling(window=window).std()

        upper = middle + (std * num_std)
        lower = middle - (std * num_std)

        return {
            'upper': upper,
            'middle': middle,
            'lower': lower
        }

    def atr(self, high: pd.Series, low: pd.Series, close: pd.Series, window: int = 14) -> pd.Series:
        """
        Average True Range (volatility indicator).

        Args:
            high: High prices
            low: Low prices
            close: Close prices
            window: ATR window

        Returns:
            ATR series
        """
        tr1 = high - low
        tr2 = abs(high - close.shift())
        tr3 = abs(low - close.shift())

        true_range = pd.concat([tr1, tr2, tr3], axis=1).max(axis=1)
        atr = true_range.ewm(span=window, adjust=False).mean()

        return atr

    def stochastic_oscillator(self, high: pd.Series, low: pd.Series, close: pd.Series,
                             k_window: int = 14, d_window: int = 3) -> Dict[str, pd.Series]:
        """
        Stochastic Oscillator.

        Args:
            high, low, close: Price series
            k_window: %K window
            d_window: %D window (signal line)

        Returns:
            Dict with '%K' and '%D'
        """
        lowest_low = low.rolling(window=k_window).min()
        highest_high = high.rolling(window=k_window).max()

        k = 100 * (close - lowest_low) / (highest_high - lowest_low + 1e-10)
        d = k.rolling(window=d_window).mean()

        return {
            '%K': k,
            '%D': d
        }

    # Price-based Features
    def returns(self, data: pd.Series, periods: int = 1) -> pd.Series:
        """Calculate returns"""
        return data.pct_change(periods=periods)

    def log_returns(self, data: pd.Series, periods: int = 1) -> pd.Series:
        """Calculate log returns"""
        return np.log(data / data.shift(periods))

    def momentum(self, data: pd.Series, window: int = 20) -> pd.Series:
        """Momentum (rate of change)"""
        return data / data.shift(window) - 1

    def volatility(self, data: pd.Series, window: int = 20, annualize: bool = False) -> pd.Series:
        """Rolling volatility"""
        vol = data.pct_change().rolling(window=window).std()
        if annualize:
            vol *= np.sqrt(252)
        return vol

    # Volume Features
    def obv(self, close: pd.Series, volume: pd.Series) -> pd.Series:
        """
        On-Balance Volume.

        Args:
            close: Close prices
            volume: Volume

        Returns:
            OBV series
        """
        direction = np.sign(close.diff())
        obv = (direction * volume).cumsum()
        return obv

    def vwap(self, high: pd.Series, low: pd.Series, close: pd.Series, volume: pd.Series) -> pd.Series:
        """
        Volume-Weighted Average Price.

        Args:
            high, low, close: Price series
            volume: Volume

        Returns:
            VWAP series
        """
        typical_price = (high + low + close) / 3
        vwap = (typical_price * volume).cumsum() / volume.cumsum()
        return vwap

    def volume_ratio(self, volume: pd.Series, window: int = 20) -> pd.Series:
        """Volume relative to moving average"""
        avg_volume = volume.rolling(window=window).mean()
        return volume / (avg_volume + 1e-10)

    # Advanced Features
    def rolling_correlation(self, series1: pd.Series, series2: pd.Series, window: int = 20) -> pd.Series:
        """Rolling correlation between two series"""
        return series1.rolling(window=window).corr(series2)

    def rolling_beta(self, returns: pd.Series, market_returns: pd.Series, window: int = 60) -> pd.Series:
        """Rolling beta vs market"""
        covariance = returns.rolling(window=window).cov(market_returns)
        market_variance = market_returns.rolling(window=window).var()
        beta = covariance / (market_variance + 1e-10)
        return beta

    def drawdown(self, data: pd.Series) -> pd.Series:
        """Calculate drawdown from peaks"""
        # Prices in, so drawdown is simply price / running peak - 1. Rebuilding a
        # wealth index from pct_change() dropped the first observation (NaN), so a
        # fall from the very first price was never measured.
        return data / data.cummax() - 1.0

    # Feature Selection
    def select_features_by_ic(self,
                              features: pd.DataFrame,
                              returns: pd.Series,
                              top_k: int = 10,
                              method: str = 'pearson') -> List[str]:
        """
        Select top-K features by Information Coefficient.

        Args:
            features: Feature DataFrame
            returns: Target returns
            top_k: Number of features to select
            method: Correlation method

        Returns:
            List of selected feature names
        """
        from scipy.stats import pearsonr, spearmanr

        ic_scores = {}
        self.last_ic_scores = {}   # signed IC per feature, exposed to the CLI for display
        for col in features.columns:
            valid_idx = features[col].notna() & returns.notna()
            if valid_idx.sum() < 10:
                continue

            feature_vals = features.loc[valid_idx, col].values
            return_vals = returns.loc[valid_idx].values

            if method == 'pearson':
                ic, _ = pearsonr(feature_vals, return_vals)
            else:
                ic, _ = spearmanr(feature_vals, return_vals)

            # A constant feature gives NaN correlation; ranking NaN is undefined.
            if not np.isfinite(ic):
                continue

            ic_scores[col] = abs(ic)
            self.last_ic_scores[col] = float(ic)

        # Sort by IC and select top-K
        sorted_features = sorted(ic_scores.items(), key=lambda x: x[1], reverse=True)
        selected = [f[0] for f in sorted_features[:top_k]]

        return selected


# ── Supervised dataset from market data (Meta Learning / Advanced Models) ─────

# Scale-free feature set built on daily OHLCV; every column is comparable
# across tickers and price levels (returns, ratios to price, 0..1 oscillators).
SUPERVISED_FEATURES = [
    'ret_1', 'ret_5', 'ret_10', 'ret_20', 'ret_60', 'vol_20', 'vol_60', 'rsi_14',
    'macd_rel', 'macd_signal_rel', 'macd_hist_rel', 'bb_position', 'atr_rel',
    'volume_ratio_20', 'drawdown_60', 'range_rel',
]


def load_daily_bars(ticker: str, start_date: str, end_date: str, timeout: int = 30) -> 'pd.DataFrame':
    """Daily OHLCV (split/dividend adjusted) from Yahoo Finance through fincept_yahoo's shared bar
    cache (its own network timeout applies); raises ValueError when empty."""
    import fincept_yahoo as fy  # imported here: this module is also used without network access

    raw = fy.download(ticker, start=start_date, end=end_date, auto_adjust=True)
    if raw is None or raw.empty:
        raise ValueError(f"No daily price data for '{ticker}' between {start_date} and {end_date} "
                         "(unknown ticker, or no network access to Yahoo Finance)")
    if isinstance(raw.columns, pd.MultiIndex):
        raw.columns = raw.columns.get_level_values(0)
    df = raw.rename(columns=str.lower)
    missing = [c for c in ('open', 'high', 'low', 'close', 'volume') if c not in df.columns]
    if missing:
        raise ValueError(f"Price data for '{ticker}' lacks column(s): {', '.join(missing)}")
    df = df[['open', 'high', 'low', 'close', 'volume']].astype(float).dropna()
    return df[df['close'] > 0]


def build_supervised_dataset(ticker: str, start_date: Optional[str] = None, end_date: Optional[str] = None,
                             horizon: int = 5, task_type: str = 'regression', test_fraction: float = 0.2,
                             min_train_rows: int = 120, min_test_rows: int = 30) -> Dict[str, Any]:
    """Features and a forward-return target on a ticker's daily bars, split chronologically.

    target = close[t + horizon] / close[t] - 1 (classification: 1 when > 0, else 0).
    The last ``horizon`` training rows are purged: their labels look into the test
    window, which would leak hold-out prices into training.

    Returns X_train / y_train / X_test / y_test (numpy), the feature frames (index =
    dates) and ``info`` describing the data window. Raises ValueError with a plain
    message when the ticker, window or horizon cannot give a usable dataset.
    """
    from datetime import date, timedelta

    ticker = str(ticker or '').strip().upper()
    if not ticker:
        raise ValueError("ticker is required (e.g. \"AAPL\")")
    try:
        horizon = int(horizon)
    except (TypeError, ValueError):
        raise ValueError("horizon must be a whole number of trading days")
    if not 1 <= horizon <= 60:
        raise ValueError("horizon must be between 1 and 60 trading days")
    if task_type not in ('regression', 'classification'):
        raise ValueError("task_type must be 'regression' or 'classification'")
    end_date = str(end_date or date.today().isoformat())
    start_date = str(start_date or (date.fromisoformat(end_date) - timedelta(days=5 * 365)).isoformat())
    if start_date >= end_date:
        raise ValueError(f"start_date ({start_date}) must be before end_date ({end_date})")

    bars = load_daily_bars(ticker, start_date, end_date)
    close, high, low, volume = bars['close'], bars['high'], bars['low'], bars['volume']
    fe = FeatureEngineer()
    feats = pd.DataFrame(index=bars.index)
    for n in (1, 5, 10, 20, 60):
        feats[f'ret_{n}'] = fe.returns(close, n)
    daily = close.pct_change()
    feats['vol_20'] = daily.rolling(20).std()
    feats['vol_60'] = daily.rolling(60).std()
    feats['rsi_14'] = fe.rsi(close, 14) / 100.0
    macd = fe.macd(close)
    feats['macd_rel'] = macd['macd'] / close
    feats['macd_signal_rel'] = macd['signal'] / close
    feats['macd_hist_rel'] = macd['histogram'] / close
    bb = fe.bollinger_bands(close, 20, 2.0)
    band = (bb['upper'] - bb['lower']).replace(0.0, np.nan)
    feats['bb_position'] = (close - bb['middle']) / band
    feats['atr_rel'] = fe.atr(high, low, close, 14) / close
    feats['volume_ratio_20'] = fe.volume_ratio(volume, 20)
    feats['drawdown_60'] = close / close.rolling(60).max() - 1.0
    feats['range_rel'] = (high - low) / close
    feats = feats[SUPERVISED_FEATURES].replace([np.inf, -np.inf], np.nan)

    fwd = close.shift(-horizon) / close - 1.0
    frame = feats.assign(_target=fwd).dropna()
    n = len(frame)
    split = int(round(n * (1.0 - test_fraction)))
    train = frame.iloc[:max(0, split - horizon)]
    test = frame.iloc[split:]
    if len(train) < min_train_rows or len(test) < min_test_rows:
        raise ValueError(f"Only {n} usable rows for {ticker} between {start_date} and {end_date} "
                         f"({len(train)} train / {len(test)} test after the {horizon}-day purge); need at "
                         f"least {min_train_rows} / {min_test_rows} -- widen the date range")

    def target(s):
        return (s > 0).astype(int).to_numpy() if task_type == 'classification' else s.to_numpy(dtype=float)

    info = {
        'ticker': ticker,
        'start_date': start_date,
        'end_date': end_date,
        'horizon': horizon,
        'task_type': task_type,
        'target': (f'{horizon}-day forward return > 0' if task_type == 'classification'
                   else f'{horizon}-day forward return'),
        'bars': int(len(bars)),
        'rows': int(n),
        'train_rows': int(len(train)),
        'test_rows': int(len(test)),
        'purged_rows': int(split - len(train)),
        'train_period': [str(train.index[0].date()), str(train.index[-1].date())],
        'test_period': [str(test.index[0].date()), str(test.index[-1].date())],
        'features': list(SUPERVISED_FEATURES),
        'source': 'Yahoo Finance daily bars (adjusted)',
    }
    return {
        'X_train': train[SUPERVISED_FEATURES].to_numpy(dtype=float),
        'y_train': target(train['_target']),
        'X_test': test[SUPERVISED_FEATURES].to_numpy(dtype=float),
        'y_test': target(test['_target']),
        'train_frame': train,
        'test_frame': test,
        # Whole usable frame (features + '_target') and the row where the test window
        # starts: sequence models read the bars before a test row as INPUT history.
        'frame': frame,
        'split_index': int(split),
        'feature_names': list(SUPERVISED_FEATURES),
        'info': info,
    }


class FeatureExpressionEngine:
    """
    Expression-based feature creation.

    Supports expressions like: "Ref($close, 5) / $close - 1"
    """

    def __init__(self, data: pd.DataFrame):
        """
        Args:
            data: DataFrame with columns like 'open', 'high', 'low', 'close', 'volume'
        """
        self.data = data
        self.functions = {
            'Ref': self._ref,
            'Mean': self._mean,
            'Std': self._std,
            'Sum': self._sum,
            'Max': self._max,
            'Min': self._min,
            'Rank': self._rank,
            'Log': self._log,
            'Abs': self._abs,
            'Sign': self._sign
        }

    def _ref(self, series: pd.Series, periods: int) -> pd.Series:
        """Reference past value"""
        return series.shift(periods)

    def _mean(self, series: pd.Series, window: int) -> pd.Series:
        """Moving average"""
        return series.rolling(window=window).mean()

    def _std(self, series: pd.Series, window: int) -> pd.Series:
        """Moving standard deviation"""
        return series.rolling(window=window).std()

    def _sum(self, series: pd.Series, window: int) -> pd.Series:
        """Moving sum"""
        return series.rolling(window=window).sum()

    def _max(self, series: pd.Series, window: int) -> pd.Series:
        """Moving max"""
        return series.rolling(window=window).max()

    def _min(self, series: pd.Series, window: int) -> pd.Series:
        """Moving min"""
        return series.rolling(window=window).min()

    def _rank(self, series: pd.Series, window: int) -> pd.Series:
        """Rolling rank (percentile)"""
        return series.rolling(window=window).apply(lambda x: pd.Series(x).rank(pct=True).iloc[-1])

    def _log(self, series: pd.Series) -> pd.Series:
        """Natural logarithm"""
        return np.log(series)

    def _abs(self, series: pd.Series) -> pd.Series:
        """Absolute value"""
        return series.abs()

    def _sign(self, series: pd.Series) -> pd.Series:
        """Sign (-1, 0, 1)"""
        return np.sign(series)

    def evaluate(self, expression: str) -> pd.Series:
        """
        Evaluate a feature expression.

        Examples:
        - "Ref($close, 5) / $close - 1"  # 5-day momentum
        - "Mean($close, 20)"  # 20-day MA
        - "($close - Mean($close, 20)) / Std($close, 20)"  # Z-score

        Args:
            expression: Feature expression string

        Returns:
            Calculated feature series
        """
        # This used to return data['close'] for every expression, so the panel
        # reported the raw price column as if it were the computed feature. Parse the
        # expression with ``ast`` and walk a strict whitelist instead -- no eval(), so
        # nothing but arithmetic, field names and the functions registered above runs.
        import ast
        import operator
        import re

        text = re.sub(r'\$([A-Za-z_][A-Za-z0-9_]*)', r'\1', str(expression or '').strip())
        if not text:
            raise ValueError("Expression is empty")
        try:
            tree = ast.parse(text, mode='eval')
        except SyntaxError as e:
            raise ValueError(f"Invalid expression syntax: {e.msg}")

        columns = {str(c).lower(): self.data[c].astype(float) for c in self.data.columns}
        if not columns:
            raise ValueError("Data has no columns")
        windowed = {'ref', 'mean', 'std', 'sum', 'max', 'min', 'rank'}

        def bind(name, fn):
            if name in windowed:
                return lambda s, n: fn(s, self._window(n))
            return fn

        functions = {name.lower(): bind(name.lower(), fn) for name, fn in self.functions.items()}
        functions['greater'] = lambda a, b: np.maximum(a, b)
        functions['less'] = lambda a, b: np.minimum(a, b)
        functions['delta'] = lambda s, n: s - s.shift(self._window(n))
        binops = {ast.Add: operator.add, ast.Sub: operator.sub, ast.Mult: operator.mul,
                  ast.Div: operator.truediv, ast.Pow: operator.pow}

        def walk(node):
            if isinstance(node, ast.Expression):
                return walk(node.body)
            if isinstance(node, ast.Constant):
                if isinstance(node.value, (int, float)) and not isinstance(node.value, bool):
                    return node.value
                raise ValueError("Only numeric literals are allowed")
            if isinstance(node, ast.Name):
                key = node.id.lower()
                if key not in columns:
                    raise ValueError(f"Unknown field '{node.id}'. Available fields: {', '.join(columns)}")
                return columns[key]
            if isinstance(node, ast.UnaryOp) and isinstance(node.op, (ast.USub, ast.UAdd)):
                value = walk(node.operand)
                return -value if isinstance(node.op, ast.USub) else value
            if isinstance(node, ast.BinOp) and type(node.op) in binops:
                return binops[type(node.op)](walk(node.left), walk(node.right))
            if isinstance(node, ast.Call) and isinstance(node.func, ast.Name) and not node.keywords:
                fn = functions.get(node.func.id.lower())
                if fn is None:
                    raise ValueError(f"Unknown function '{node.func.id}'. Available: "
                                     f"{', '.join(sorted(self.functions))}, Greater, Less, Delta")
                return fn(*[walk(a) for a in node.args])
            raise ValueError("Unsupported expression element; use fields, numbers, + - * / **, "
                             "and the supported functions")

        result = walk(tree)
        if not isinstance(result, pd.Series):
            raise ValueError("Expression must reference at least one data field")
        return result.replace([np.inf, -np.inf], np.nan)

    @staticmethod
    def _window(n) -> int:
        """Window / lag arguments must be positive whole numbers, not series."""
        if isinstance(n, pd.Series) or float(n) != int(n) or int(n) < 1:
            raise ValueError("Window / period arguments must be positive whole numbers")
        return int(n)


def _json_safe(obj):
    """Replace NaN / +-Infinity with None before json.dumps.

    Python emits them as bare ``NaN`` / ``Infinity`` tokens, which are not JSON: the
    terminal's parser rejects the WHOLE payload ("malformed JSON") over one empty cell.
    """
    if isinstance(obj, float):
        return obj if obj == obj and obj not in (float('inf'), float('-inf')) else None
    if isinstance(obj, dict):
        return {k: _json_safe(v) for k, v in obj.items()}
    if isinstance(obj, (list, tuple)):
        return [_json_safe(v) for v in obj]
    return obj


def _aligned(close_values, params: dict, keys: tuple) -> tuple:
    """(series for each of `keys`..., close) as float Series of one common length.

    ATR / stochastic / VWAP / OBV need high, low and/or volume alongside the close;
    the C++ panel sends them as extra arrays. Mismatched lengths used to be silently
    index-aligned by pandas, producing NaN-padded or shifted results.
    """
    close = pd.Series(close_values, dtype=float)
    if close.empty:
        raise ValueError("Price data (close) is empty")
    out = []
    for key in keys:
        values = params.get(key)
        if values is None or (hasattr(values, "__len__") and len(values) == 0):
            raise ValueError(f"'{key}' series is required for this indicator")
        s = pd.Series(values, dtype=float)
        if len(s) != len(close):
            raise ValueError(f"'{key}' has {len(s)} values but the close series has {len(close)}; "
                             "they must be the same length")
        out.append(s)
    if "high" in keys and "low" in keys:
        hi, lo = out[keys.index("high")], out[keys.index("low")]
        if bool((hi < lo).any()):
            raise ValueError("high is below low on at least one bar")
    if "volume" in keys and bool((out[keys.index("volume")] < 0).any()):
        raise ValueError("volume cannot be negative")
    return (*out, close)


def main():
    """CLI interface"""
    if len(sys.argv) < 2:
        fincept_json.emit({"success": False, "error": "No command specified"})
        sys.exit(1)

    command = sys.argv[1]
    engineer = FeatureEngineer()

    def num_list(value):
        """Accept a JSON array or the comma/space-separated string typed into the UI."""
        if isinstance(value, str):
            return [float(tok) for tok in re.split(r'[,;\s]+', value.strip()) if tok]
        return value

    try:
        params = json.loads(sys.argv[2]) if len(sys.argv) > 2 else {}
        data = params.get("data", [])
        if command != "evaluate_expression":
            data = num_list(data)
            for key in ("high", "low", "volume"):
                if key in params:
                    params[key] = num_list(params[key])

        if command == "check_status":
            result = {
                "success": True,
                "pandas_available": PANDAS_AVAILABLE,
                "indicators_available": [
                    "moving_average", "rsi", "macd", "bollinger_bands",
                    "atr", "stochastic_oscillator", "returns", "log_returns",
                    "momentum", "volatility", "obv", "vwap", "volume_ratio",
                    "rolling_correlation", "rolling_beta", "drawdown", "select_features_by_ic"
                ]
            }

        elif command == "moving_average":
            s = pd.Series(data)
            result_series = engineer.moving_average(s, params.get("window", 20), params.get("method", "sma"))
            result = {"success": True, "data": result_series.dropna().tolist()}

        elif command == "rsi":
            s = pd.Series(data)
            result_series = engineer.rsi(s, params.get("window", 14))
            result = {"success": True, "data": result_series.dropna().tolist()}

        elif command == "macd":
            s = pd.Series(data)
            r = engineer.macd(s, params.get("fast", 12), params.get("slow", 26), params.get("signal", 9))
            result = {"success": True, "macd": r["macd"].dropna().tolist(),
                      "signal": r["signal"].dropna().tolist(), "histogram": r["histogram"].dropna().tolist()}

        elif command == "bollinger_bands":
            s = pd.Series(data)
            r = engineer.bollinger_bands(s, params.get("window", 20), params.get("num_std", 2.0))
            result = {"success": True, "upper": r["upper"].dropna().tolist(),
                      "middle": r["middle"].dropna().tolist(), "lower": r["lower"].dropna().tolist()}

        elif command == "atr":
            high, low, close = _aligned(data, params, ("high", "low"))
            r = engineer.atr(high, low, close, int(params.get("window", 14)))
            # The first bar has no previous close, so its true range is only high - low.
            result = {"success": True, "data": r.iloc[1:].dropna().tolist()}

        elif command == "stochastic_oscillator":
            high, low, close = _aligned(data, params, ("high", "low"))
            k_window = int(params.get("k_window", params.get("window", 14)))
            d_window = int(params.get("d_window", 3))
            if len(close) < k_window + d_window - 1:
                raise ValueError(f"%K over {k_window} bars and %D over {d_window} need at least "
                                 f"{k_window + d_window - 1} bars; got {len(close)}")
            r = engineer.stochastic_oscillator(high, low, close, k_window, d_window)
            # The engine names the lines '%K' / '%D' (reading r["k"] raised KeyError: 'k').
            k = r["%K"]
            d = r["%D"]
            valid = d.notna()
            result = {"success": True, "k": k[valid].tolist(), "d": d[valid].tolist()}

        elif command == "returns":
            r = engineer.returns(pd.Series(data), params.get("periods", 1))
            result = {"success": True, "data": r.dropna().tolist()}

        elif command == "log_returns":
            r = engineer.log_returns(pd.Series(data), params.get("periods", 1))
            result = {"success": True, "data": r.dropna().tolist()}

        elif command == "momentum":
            r = engineer.momentum(pd.Series(data), params.get("window", 20))
            result = {"success": True, "data": r.dropna().tolist()}

        elif command == "volatility":
            r = engineer.volatility(pd.Series(data), params.get("window", 20), params.get("annualize", False))
            result = {"success": True, "data": r.dropna().tolist()}

        elif command == "obv":
            volume, close = _aligned(data, params, ("volume",))
            # OBV starts at 0 on the first bar (no prior close to compare with).
            r = engineer.obv(close, volume).fillna(0.0)
            result = {"success": True, "data": r.tolist()}

        elif command == "vwap":
            high, low, volume, close = _aligned(data, params, ("high", "low", "volume"))
            if float(volume.sum()) <= 0:
                raise ValueError("VWAP needs a positive total volume")
            # Cumulative (anchored at the first bar) VWAP; bars before any volume have none.
            r = engineer.vwap(high, low, close, volume)
            result = {"success": True, "data": r.tolist()}

        elif command == "drawdown":
            r = engineer.drawdown(pd.Series(data))
            result = {"success": True, "data": r.tolist()}

        elif command == "select_features_by_ic":
            # The selector works on a DataFrame (feature columns x observations); it used
            # to be handed a dict and failed with "'dict' object has no attribute 'columns'".
            features = pd.DataFrame({k: pd.Series(num_list(v), dtype=float)
                                     for k, v in params.get("features", {}).items()})
            returns_s = pd.Series(num_list(params.get("returns", [])), dtype=float)
            r = engineer.select_features_by_ic(features, returns_s, int(params.get("top_k", 10)),
                                               params.get("method", "pearson"))
            if not engineer.last_ic_scores:
                result = {"success": False,
                          "error": "No feature had at least 10 paired observations with the returns "
                                   "(and a non-constant value) -- supply longer series."}
            else:
                result = {"success": True, "selected_features": r,
                          "ic_scores": {k: engineer.last_ic_scores[k] for k in r}}

        elif command == "evaluate_expression":
            df = pd.DataFrame({k: pd.Series(num_list(v), dtype=float)
                               for k, v in params.get("data", {}).items()})
            engine = FeatureExpressionEngine(df)
            expr_result = engine.evaluate(params.get("expression", ""))
            result = {"success": True, "expression": params.get("expression", ""),
                      "data": expr_result.dropna().tolist()}

        else:
            result = {"success": False, "error": f"Unknown command: {command}"}

        fincept_json.emit(_json_safe(result))

    except Exception as e:
        fincept_json.emit({"success": False, "error": str(e)})
        sys.exit(1)


if __name__ == "__main__":
    main()
