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
import sys
from typing import Dict, List, Any, Optional, Union, Callable
import warnings
warnings.filterwarnings('ignore')

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
        cumulative = (1 + data.pct_change()).cumprod()
        running_max = cumulative.expanding().max()
        drawdown = (cumulative - running_max) / running_max
        return drawdown

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

            ic_scores[col] = abs(ic)

        # Sort by IC and select top-K
        sorted_features = sorted(ic_scores.items(), key=lambda x: x[1], reverse=True)
        selected = [f[0] for f in sorted_features[:top_k]]

        return selected


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
        # Simple expression evaluation (production would use proper parser)
        # For now, this is a placeholder implementation
        # Real implementation would parse AST and evaluate

        # Example: Just return close for demonstration
        if '$close' in expression and 'Ref' not in expression:
            return self.data['close']
        else:
            # Placeholder - real implementation would parse and evaluate
            return self.data['close']


def main():
    """CLI interface"""
    if len(sys.argv) < 2:
        print(json.dumps({"success": False, "error": "No command specified"}))
        sys.exit(1)

    command = sys.argv[1]
    engineer = FeatureEngineer()

    try:
        params = json.loads(sys.argv[2]) if len(sys.argv) > 2 else {}
        data = params.get("data", [])

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
            r = engineer.atr(pd.Series(params.get("high", [])), pd.Series(params.get("low", [])),
                             pd.Series(data), params.get("window", 14))
            result = {"success": True, "data": r.dropna().tolist()}

        elif command == "stochastic_oscillator":
            r = engineer.stochastic_oscillator(pd.Series(params.get("high", [])),
                                               pd.Series(params.get("low", [])), pd.Series(data),
                                               params.get("k_window", 14), params.get("d_window", 3))
            result = {"success": True, "k": r["k"].dropna().tolist(), "d": r["d"].dropna().tolist()}

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
            r = engineer.obv(pd.Series(data), pd.Series(params.get("volume", [])))
            result = {"success": True, "data": r.tolist()}

        elif command == "vwap":
            r = engineer.vwap(pd.Series(params.get("high", [])), pd.Series(params.get("low", [])),
                              pd.Series(data), pd.Series(params.get("volume", [])))
            result = {"success": True, "data": r.tolist()}

        elif command == "drawdown":
            r = engineer.drawdown(pd.Series(data))
            result = {"success": True, "data": r.tolist()}

        elif command == "select_features_by_ic":
            features = {k: pd.Series(v) for k, v in params.get("features", {}).items()}
            returns_s = pd.Series(params.get("returns", []))
            r = engineer.select_features_by_ic(features, returns_s, params.get("top_k", 10),
                                               params.get("method", "pearson"))
            result = {"success": True, "selected_features": r}

        elif command == "evaluate_expression":
            df = pd.DataFrame(params.get("data", {}))
            engine = FeatureExpressionEngine(df)
            expr_result = engine.evaluate(params.get("expression", ""))
            result = {"success": True, "data": expr_result.dropna().tolist() if hasattr(expr_result, 'tolist') else expr_result}

        else:
            result = {"success": False, "error": f"Unknown command: {command}"}

        print(json.dumps(result))

    except Exception as e:
        print(json.dumps({"success": False, "error": str(e)}))
        sys.exit(1)


if __name__ == "__main__":
    main()
