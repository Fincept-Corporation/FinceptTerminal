"""
Zipline Pipeline API Simulation

Provides stub/simulation classes that mirror Zipline's Pipeline API.
When actual Zipline is installed, these are replaced by the real implementations.
When running in numpy-fallback mode, these provide a functional subset.

Covers:
- Pipeline, CustomFactor, CustomFilter, CustomClassifier
- Factor, Filter, Classifier base classes with methods
- Built-in factors: AverageDollarVolume, BollingerBands, Returns, SMA, VWAP, RSI,
  MACDSignal, MaxDrawdown, Aroon, IchimokuKinkoHyo, AnnualizedVolatility,
  DailyReturns, EWMA, EWMSTD, PercentChange, RateOfChangePercentage,
  TrueRange, FastStochasticOscillator, SimpleBeta, RollingPearsonOfReturns,
  RollingSpearmanOfReturns, RollingLinearRegressionOfReturns, WeightedAverageValue,
  Latest, PeerCount
- Built-in filters: StaticAssets, StaticSids, AllPresent, AtLeastN, SingleAsset
- Classifiers: Quantiles, Everything
- Data: EquityPricing, USEquityPricing, DataSet, Column, BoundColumn
- Domains: US_EQUITIES, GENERIC, and country-specific domains
"""

import numpy as np
from typing import Dict, Any, List, Optional, Callable


# ============================================================================
# Pipeline Domains
# ============================================================================

class _Domain:
    """Represents a market domain for pipeline computations."""
    def __init__(self, name, country_code=None, calendar_name=None):
        self.name = name
        self.country_code = country_code
        self.calendar_name = calendar_name or 'XNYS'

    def __repr__(self):
        return f"Domain('{self.name}')"

GENERIC = _Domain('GENERIC')
US_EQUITIES = _Domain('US_EQUITIES', 'US', 'XNYS')
UK_EQUITIES = _Domain('UK_EQUITIES', 'GB', 'XLON')
CA_EQUITIES = _Domain('CA_EQUITIES', 'CA', 'XTSE')
DE_EQUITIES = _Domain('DE_EQUITIES', 'DE', 'XFRA')
FR_EQUITIES = _Domain('FR_EQUITIES', 'FR', 'XPAR')
JP_EQUITIES = _Domain('JP_EQUITIES', 'JP', 'XTKS')
KR_EQUITIES = _Domain('KR_EQUITIES', 'KR', 'XKRX')
HK_EQUITIES = _Domain('HK_EQUITIES', 'HK', 'XHKG')
CN_EQUITIES = _Domain('CN_EQUITIES', 'CN', 'XSHG')
IN_EQUITIES = _Domain('IN_EQUITIES', 'IN', 'XBOM')
AU_EQUITIES = _Domain('AU_EQUITIES', 'AU', 'XASX')
BR_EQUITIES = _Domain('BR_EQUITIES', 'BR', 'BVMF')
CH_EQUITIES = _Domain('CH_EQUITIES', 'CH', 'XSWX')
ES_EQUITIES = _Domain('ES_EQUITIES', 'ES', 'XMAD')
IT_EQUITIES = _Domain('IT_EQUITIES', 'IT', 'XMIL')
NL_EQUITIES = _Domain('NL_EQUITIES', 'NL', 'XAMS')
NO_EQUITIES = _Domain('NO_EQUITIES', 'NO', 'XOSL')
NZ_EQUITIES = _Domain('NZ_EQUITIES', 'NZ', 'XNZE')
PT_EQUITIES = _Domain('PT_EQUITIES', 'PT', 'XLIS')
SE_EQUITIES = _Domain('SE_EQUITIES', 'SE', 'XSTO')
SG_EQUITIES = _Domain('SG_EQUITIES', 'SG', 'XSES')


# ============================================================================
# Pipeline Data Classes
# ============================================================================

class Column:
    """Abstract column definition."""
    def __init__(self, dtype, missing_value=None, currency_aware=False):
        self.dtype = dtype
        self.missing_value = missing_value
        self.currency_aware = currency_aware


class BoundColumn(Column):
    """A column bound to a specific dataset."""
    def __init__(self, name, dtype, dataset=None, missing_value=None):
        super().__init__(dtype, missing_value)
        self.name = name
        self.dataset = dataset

    def latest(self):
        """Return a Factor/Filter that computes the most recent value."""
        return Latest(inputs=[self], window_length=1)

    def __repr__(self):
        ds = self.dataset.__name__ if self.dataset else '?'
        return f"{ds}.{self.name}"


class _DataSetMeta(type):
    """Metaclass for DataSet that creates BoundColumn instances."""
    pass


class DataSet:
    """Base class for pipeline datasets."""
    domain = GENERIC


class DataSetFamily:
    """Family of related datasets parameterized by dimensions."""
    def __init__(self, domain=GENERIC):
        self.domain = domain


class _EquityPricingBase(DataSet):
    """Base equity pricing dataset with OHLCV columns."""
    open = BoundColumn('open', float)
    high = BoundColumn('high', float)
    low = BoundColumn('low', float)
    close = BoundColumn('close', float)
    volume = BoundColumn('volume', float)
    currency = BoundColumn('currency', str)

# Set dataset references
for col_name in ['open', 'high', 'low', 'close', 'volume', 'currency']:
    getattr(_EquityPricingBase, col_name).dataset = _EquityPricingBase


class EquityPricing(_EquityPricingBase):
    """Generic equity pricing dataset."""
    domain = GENERIC


class USEquityPricing(_EquityPricingBase):
    """US equity pricing dataset."""
    domain = US_EQUITIES


# ============================================================================
# Base Term / Factor / Filter / Classifier
# ============================================================================

class Term:
    """Abstract base class for all pipeline computations."""
    inputs = []
    outputs = None
    window_length = 0
    mask = None
    domain = GENERIC
    dtype = float
    missing_value = None

    def __init__(self, inputs=None, outputs=None, window_length=None,
                 mask=None, domain=None, **kwargs):
        if inputs is not None:
            self.inputs = inputs
        if outputs is not None:
            self.outputs = outputs
        if window_length is not None:
            self.window_length = window_length
        if mask is not None:
            self.mask = mask
        if domain is not None:
            self.domain = domain
        self._kwargs = kwargs

    def compute(self, today, assets, out, *inputs):
        """Override in subclasses to define computation."""
        raise NotImplementedError


class Factor(Term):
    """
    Base class for numerical pipeline computations.
    Provides ranking, filtering, and statistical methods.
    """
    dtype = float

    # ---- Ranking & Selection ----

    def top(self, N, mask=None, groupby=None):
        """Select top N assets by factor value."""
        return _RankFilter(self, N, ascending=False, mask=mask, groupby=groupby)

    def bottom(self, N, mask=None, groupby=None):
        """Select bottom N assets by factor value."""
        return _RankFilter(self, N, ascending=True, mask=mask, groupby=groupby)

    def rank(self, method='ordinal', ascending=True, mask=None, groupby=None):
        """Compute rank of factor values."""
        return _Rank(self, method=method, ascending=ascending, mask=mask, groupby=groupby)

    def percentile_between(self, min_percentile, max_percentile, mask=None):
        """Filter assets within percentile range."""
        return _PercentileFilter(self, min_percentile, max_percentile, mask=mask)

    def quantiles(self, bins, mask=None):
        """Compute quantile labels."""
        return _Quantiles(self, bins, mask=mask)

    def quartiles(self, mask=None):
        return self.quantiles(4, mask=mask)

    def quintiles(self, mask=None):
        return self.quantiles(5, mask=mask)

    def deciles(self, mask=None):
        return self.quantiles(10, mask=mask)

    # ---- Statistical transforms ----

    def demean(self, mask=None, groupby=None):
        """Remove cross-sectional mean."""
        return _Demean(self, mask=mask, groupby=groupby)

    def zscore(self, mask=None, groupby=None):
        """Compute cross-sectional z-scores."""
        return _ZScore(self, mask=mask, groupby=groupby)

    def winsorize(self, min_percentile, max_percentile, mask=None, groupby=None):
        """Winsorize outliers."""
        return _Winsorize(self, min_percentile, max_percentile, mask=mask, groupby=groupby)

    def clip(self, min_bound, max_bound):
        """Clip values to [min_bound, max_bound]."""
        return _Clip(self, min_bound, max_bound)

    def fillna(self, fill_value):
        """Fill NaN values."""
        return _FillNa(self, fill_value)

    # ---- Aggregation ----

    def mean(self):
        return _Agg(self, 'mean')

    def stddev(self):
        return _Agg(self, 'std')

    def max(self):
        return _Agg(self, 'max')

    def min(self):
        return _Agg(self, 'min')

    def median(self):
        return _Agg(self, 'median')

    def sum(self):
        return _Agg(self, 'sum')

    # ---- Correlation / Regression ----

    def pearsonr(self, target, correlation_length, mask=None):
        return _Correlation(self, target, correlation_length, 'pearson', mask=mask)

    def spearmanr(self, target, correlation_length, mask=None):
        return _Correlation(self, target, correlation_length, 'spearman', mask=mask)

    def linear_regression(self, target, regression_length, mask=None):
        return _LinearRegression(self, target, regression_length, mask=mask)

    # ---- Null checks ----

    def isnan(self):
        return _NullCheck(self, 'isnan')

    def notnan(self):
        return _NullCheck(self, 'notnan')

    def isfinite(self):
        return _NullCheck(self, 'isfinite')

    def eq(self, other):
        return _Comparison(self, other, 'eq')

    # ---- Arithmetic operators ----

    def __add__(self, other):
        return _BinaryOp(self, other, '+')

    def __sub__(self, other):
        return _BinaryOp(self, other, '-')

    def __mul__(self, other):
        return _BinaryOp(self, other, '*')

    def __truediv__(self, other):
        return _BinaryOp(self, other, '/')

    def __mod__(self, other):
        return _BinaryOp(self, other, '%')

    def __pow__(self, other):
        return _BinaryOp(self, other, '**')

    # ---- Comparison operators (return Filters) ----

    def __lt__(self, other):
        return _Comparison(self, other, '<')

    def __le__(self, other):
        return _Comparison(self, other, '<=')

    def __gt__(self, other):
        return _Comparison(self, other, '>')

    def __ge__(self, other):
        return _Comparison(self, other, '>=')

    def __ne__(self, other):
        return _Comparison(self, other, '!=')


class Filter(Term):
    """Base class for boolean pipeline computations."""
    dtype = bool

    def __and__(self, other):
        return _LogicalOp(self, other, 'and')

    def __or__(self, other):
        return _LogicalOp(self, other, 'or')

    def __invert__(self):
        return _LogicalNot(self)

    def if_else(self, if_true, if_false):
        return _IfElse(self, if_true, if_false)


class Classifier(Term):
    """Base class for categorical pipeline computations."""
    dtype = int
    missing_value = -1


class CustomFactor(Factor):
    """
    Base class for user-defined factors.

    Subclass and implement compute():
        class MyFactor(CustomFactor):
            inputs = [EquityPricing.close]
            window_length = 10
            def compute(self, today, assets, out, close):
                out[:] = np.mean(close, axis=0)
    """
    pass


class CustomFilter(Filter):
    """
    Base class for user-defined filters.

    Subclass and implement compute():
        class MyFilter(CustomFilter):
            inputs = [EquityPricing.close]
            window_length = 1
            def compute(self, today, assets, out, close):
                out[:] = close[-1] > 10.0
    """
    pass


class CustomClassifier(Classifier):
    """
    Base class for user-defined classifiers.

    Subclass and implement compute():
        class MyClassifier(CustomClassifier):
            inputs = [EquityPricing.volume]
            window_length = 1
            def compute(self, today, assets, out, volume):
                out[:] = np.where(volume[-1] > 1e6, 0, 1)
    """
    pass


# ============================================================================
# Internal helper terms (used by Factor methods)
# ============================================================================

class _RankFilter(Filter):
    def __init__(self, factor, N, ascending, mask=None, groupby=None):
        super().__init__(mask=mask)
        self.factor = factor
        self.N = N
        self.ascending = ascending

class _Rank(Factor):
    def __init__(self, factor, method, ascending, mask=None, groupby=None):
        super().__init__(mask=mask)
        self.factor = factor
        self.method = method
        self.ascending = ascending

class _PercentileFilter(Filter):
    def __init__(self, factor, min_pct, max_pct, mask=None):
        super().__init__(mask=mask)
        self.factor = factor
        self.min_pct = min_pct
        self.max_pct = max_pct

class _Quantiles(Classifier):
    def __init__(self, factor, bins, mask=None):
        super().__init__(mask=mask)
        self.factor = factor
        self.bins = bins

class _Demean(Factor):
    def __init__(self, factor, mask=None, groupby=None):
        super().__init__(mask=mask)
        self.factor = factor

class _ZScore(Factor):
    def __init__(self, factor, mask=None, groupby=None):
        super().__init__(mask=mask)
        self.factor = factor

class _Winsorize(Factor):
    def __init__(self, factor, min_pct, max_pct, mask=None, groupby=None):
        super().__init__(mask=mask)
        self.factor = factor

class _Clip(Factor):
    def __init__(self, factor, min_bound, max_bound):
        super().__init__()
        self.factor = factor

class _FillNa(Factor):
    def __init__(self, factor, fill_value):
        super().__init__()
        self.factor = factor
        self.fill_value = fill_value

class _Agg(Factor):
    def __init__(self, factor, op):
        super().__init__()
        self.factor = factor
        self.op = op

class _Correlation(Factor):
    def __init__(self, factor, target, length, method, mask=None):
        super().__init__(mask=mask)
        self.factor = factor
        self.target = target
        self.length = length
        self.method = method

class _LinearRegression(Factor):
    def __init__(self, factor, target, length, mask=None):
        super().__init__(mask=mask)
        self.factor = factor
        self.target = target
        self.length = length

class _NullCheck(Filter):
    def __init__(self, factor, check_type):
        super().__init__()
        self.factor = factor
        self.check_type = check_type

class _Comparison(Filter):
    def __init__(self, left, right, op):
        super().__init__()
        self.left = left
        self.right = right
        self.op = op

class _BinaryOp(Factor):
    def __init__(self, left, right, op):
        super().__init__()
        self.left = left
        self.right = right
        self.op = op

class _LogicalOp(Filter):
    def __init__(self, left, right, op):
        super().__init__()
        self.left = left
        self.right = right
        self.op = op

class _LogicalNot(Filter):
    def __init__(self, operand):
        super().__init__()
        self.operand = operand

class _IfElse(Factor):
    def __init__(self, condition, if_true, if_false):
        super().__init__()
        self.condition = condition
        self.if_true = if_true
        self.if_false = if_false


# ============================================================================
# Built-in Factors
# ============================================================================

class Latest(Factor):
    """Most recent value of a given column."""
    window_length = 1
    def compute(self, today, assets, out, data):
        out[:] = data[-1]


class AverageDollarVolume(Factor):
    """Average daily dollar volume over a trailing window."""
    inputs = [EquityPricing.close, EquityPricing.volume]
    def __init__(self, window_length=30, **kwargs):
        super().__init__(window_length=window_length, **kwargs)
    def compute(self, today, assets, out, close, volume):
        out[:] = np.nanmean(close * volume, axis=0)


class BollingerBands(CustomFactor):
    """
    Bollinger Bands - multi-output factor.
    Outputs: upper, middle, lower
    """
    inputs = [EquityPricing.close]
    outputs = ['upper', 'middle', 'lower']
    params = {'k': 2.0}
    def __init__(self, window_length=20, k=2.0, **kwargs):
        super().__init__(window_length=window_length, **kwargs)
        self.params = {'k': k}
    def compute(self, today, assets, out, close):
        mean = np.nanmean(close, axis=0)
        std = np.nanstd(close, axis=0, ddof=1)
        k = self.params.get('k', 2.0)
        out.upper[:] = mean + k * std
        out.middle[:] = mean
        out.lower[:] = mean - k * std


class DailyReturns(Factor):
    """One-day percentage returns."""
    inputs = [EquityPricing.close]
    window_length = 2
    def compute(self, today, assets, out, close):
        out[:] = (close[-1] - close[-2]) / close[-2]


class Returns(Factor):
    """Percentage returns over a trailing window."""
    inputs = [EquityPricing.close]
    def __init__(self, window_length=21, **kwargs):
        super().__init__(window_length=window_length, **kwargs)
    def compute(self, today, assets, out, close):
        out[:] = (close[-1] - close[0]) / close[0]


class SimpleMovingAverage(Factor):
    """Simple moving average."""
    inputs = [EquityPricing.close]
    def __init__(self, inputs=None, window_length=20, **kwargs):
        super().__init__(inputs=inputs, window_length=window_length, **kwargs)
    def compute(self, today, assets, out, data):
        out[:] = np.nanmean(data, axis=0)

# Alias
SMA = SimpleMovingAverage


class ExponentialWeightedMovingAverage(Factor):
    """Exponentially weighted moving average."""
    inputs = [EquityPricing.close]
    params = {'decay_rate': 0.0, 'span': None}
    def __init__(self, inputs=None, window_length=30, decay_rate=None, span=None, **kwargs):
        super().__init__(inputs=inputs, window_length=window_length, **kwargs)
        self.params = {'decay_rate': decay_rate, 'span': span}
    def compute(self, today, assets, out, data):
        span = self.params.get('span') or self.window_length
        alpha = 2.0 / (span + 1)
        result = data[0].copy()
        for i in range(1, len(data)):
            result = alpha * data[i] + (1 - alpha) * result
        out[:] = result

EWMA = ExponentialWeightedMovingAverage


class ExponentialWeightedMovingStdDev(Factor):
    """Exponentially weighted moving standard deviation."""
    inputs = [EquityPricing.close]
    def __init__(self, inputs=None, window_length=30, decay_rate=None, span=None, **kwargs):
        super().__init__(inputs=inputs, window_length=window_length, **kwargs)
        self.params = {'decay_rate': decay_rate, 'span': span}
    def compute(self, today, assets, out, data):
        out[:] = np.nanstd(data, axis=0, ddof=1)

EWMSTD = ExponentialWeightedMovingStdDev


class VWAP(Factor):
    """Volume-Weighted Average Price."""
    inputs = [EquityPricing.close, EquityPricing.volume]
    def __init__(self, window_length=1, **kwargs):
        super().__init__(window_length=window_length, **kwargs)
    def compute(self, today, assets, out, close, volume):
        total_vol = np.nansum(volume, axis=0)
        mask = total_vol > 0
        out[:] = np.where(mask, np.nansum(close * volume, axis=0) / total_vol, close[-1])


class RSI(Factor):
    """Relative Strength Index."""
    inputs = [EquityPricing.close]
    def __init__(self, window_length=15, **kwargs):
        super().__init__(window_length=window_length, **kwargs)
    def compute(self, today, assets, out, close):
        diffs = np.diff(close, axis=0)
        gains = np.where(diffs > 0, diffs, 0)
        losses = np.where(diffs < 0, -diffs, 0)
        avg_gain = np.nanmean(gains, axis=0)
        avg_loss = np.nanmean(losses, axis=0)
        rs = np.where(avg_loss > 1e-10, avg_gain / avg_loss, 100.0)
        out[:] = 100.0 - (100.0 / (1.0 + rs))


class MACDSignal(Factor):
    """MACD Signal line."""
    inputs = [EquityPricing.close]
    def __init__(self, fast_period=12, slow_period=26, signal_period=9, **kwargs):
        wl = slow_period + signal_period + 5
        super().__init__(window_length=wl, **kwargs)
        self._fast = fast_period
        self._slow = slow_period
        self._signal = signal_period
    def compute(self, today, assets, out, close):
        # Simplified: compute column-wise
        for j in range(close.shape[1]):
            col = close[:, j]
            alpha_f = 2.0 / (self._fast + 1)
            alpha_s = 2.0 / (self._slow + 1)
            fast_ema = col[0]
            slow_ema = col[0]
            macd_vals = []
            for v in col:
                fast_ema = alpha_f * v + (1 - alpha_f) * fast_ema
                slow_ema = alpha_s * v + (1 - alpha_s) * slow_ema
                macd_vals.append(fast_ema - slow_ema)
            alpha_sig = 2.0 / (self._signal + 1)
            sig = macd_vals[0]
            for mv in macd_vals:
                sig = alpha_sig * mv + (1 - alpha_sig) * sig
            out[j] = sig


class MaxDrawdown(Factor):
    """Maximum drawdown over a trailing window."""
    inputs = [EquityPricing.close]
    def __init__(self, window_length=252, **kwargs):
        super().__init__(window_length=window_length, **kwargs)
    def compute(self, today, assets, out, close):
        for j in range(close.shape[1]):
            col = close[:, j]
            peak = col[0]
            max_dd = 0.0
            for v in col:
                if v > peak:
                    peak = v
                dd = (v - peak) / peak if peak > 0 else 0
                if dd < max_dd:
                    max_dd = dd
            out[j] = max_dd


class AnnualizedVolatility(Factor):
    """Annualized volatility over a trailing window."""
    inputs = [EquityPricing.close]
    def __init__(self, window_length=252, annualization_factor=252, **kwargs):
        super().__init__(window_length=window_length, **kwargs)
        self._ann = annualization_factor
    def compute(self, today, assets, out, close):
        rets = np.diff(close, axis=0) / close[:-1]
        out[:] = np.nanstd(rets, axis=0, ddof=1) * np.sqrt(self._ann)


class PercentChange(Factor):
    """Percentage change over a trailing window."""
    inputs = [EquityPricing.close]
    def __init__(self, window_length=21, **kwargs):
        super().__init__(window_length=window_length, **kwargs)
    def compute(self, today, assets, out, data):
        out[:] = (data[-1] - data[0]) / data[0]


class RateOfChangePercentage(Factor):
    """Rate of change percentage."""
    inputs = [EquityPricing.close]
    def __init__(self, window_length=12, **kwargs):
        super().__init__(window_length=window_length, **kwargs)
    def compute(self, today, assets, out, close):
        out[:] = (close[-1] - close[0]) / close[0] * 100.0


class TrueRange(Factor):
    """True Range."""
    inputs = [EquityPricing.high, EquityPricing.low, EquityPricing.close]
    window_length = 2
    def compute(self, today, assets, out, high, low, close):
        out[:] = np.maximum(
            high[-1] - low[-1],
            np.maximum(
                np.abs(high[-1] - close[-2]),
                np.abs(low[-1] - close[-2])
            )
        )


class Aroon(CustomFactor):
    """Aroon indicator. Outputs: aroon_up, aroon_down."""
    inputs = [EquityPricing.high, EquityPricing.low]
    outputs = ['up', 'down']
    def __init__(self, window_length=25, **kwargs):
        super().__init__(window_length=window_length, **kwargs)
    def compute(self, today, assets, out, high, low):
        wl = high.shape[0]
        for j in range(high.shape[1]):
            h_idx = np.argmax(high[:, j])
            l_idx = np.argmin(low[:, j])
            out.up[j] = 100.0 * (wl - (wl - 1 - h_idx)) / wl
            out.down[j] = 100.0 * (wl - (wl - 1 - l_idx)) / wl


class FastStochasticOscillator(Factor):
    """Fast Stochastic Oscillator (%K)."""
    inputs = [EquityPricing.high, EquityPricing.low, EquityPricing.close]
    def __init__(self, window_length=14, **kwargs):
        super().__init__(window_length=window_length, **kwargs)
    def compute(self, today, assets, out, high, low, close):
        hh = np.nanmax(high, axis=0)
        ll = np.nanmin(low, axis=0)
        denom = hh - ll
        out[:] = np.where(denom > 1e-10, 100.0 * (close[-1] - ll) / denom, 50.0)


class IchimokuKinkoHyo(CustomFactor):
    """
    Ichimoku Kinko Hyo indicator.
    Outputs: tenkan_sen, kijun_sen, senkou_span_a, senkou_span_b, chikou_span
    """
    inputs = [EquityPricing.high, EquityPricing.low, EquityPricing.close]
    outputs = ['tenkan_sen', 'kijun_sen', 'senkou_span_a', 'senkou_span_b', 'chikou_span']
    def __init__(self, window_length=52, tenkan=9, kijun=26, **kwargs):
        super().__init__(window_length=window_length, **kwargs)
        self._tenkan = tenkan
        self._kijun = kijun
    def compute(self, today, assets, out, high, low, close):
        for j in range(high.shape[1]):
            hh_t = np.max(high[-self._tenkan:, j])
            ll_t = np.min(low[-self._tenkan:, j])
            out.tenkan_sen[j] = (hh_t + ll_t) / 2

            hh_k = np.max(high[-self._kijun:, j])
            ll_k = np.min(low[-self._kijun:, j])
            out.kijun_sen[j] = (hh_k + ll_k) / 2

            out.senkou_span_a[j] = (out.tenkan_sen[j] + out.kijun_sen[j]) / 2

            hh_s = np.max(high[:, j])
            ll_s = np.min(low[:, j])
            out.senkou_span_b[j] = (hh_s + ll_s) / 2

            out.chikou_span[j] = close[-1, j]


class SimpleBeta(Factor):
    """Simple beta coefficient relative to a target asset."""
    def __init__(self, target, regression_length, **kwargs):
        super().__init__(window_length=regression_length, **kwargs)
        self.target = target
        self.regression_length = regression_length
    def compute(self, today, assets, out, *inputs):
        out[:] = 1.0  # Default beta


class RollingPearsonOfReturns(Factor):
    """Rolling Pearson correlation of asset returns vs. a target."""
    def __init__(self, target, returns_length, correlation_length, **kwargs):
        super().__init__(window_length=correlation_length + returns_length, **kwargs)
    def compute(self, today, assets, out, *inputs):
        out[:] = 0.0

class RollingSpearmanOfReturns(Factor):
    """Rolling Spearman correlation of asset returns vs. a target."""
    def __init__(self, target, returns_length, correlation_length, **kwargs):
        super().__init__(window_length=correlation_length + returns_length, **kwargs)
    def compute(self, today, assets, out, *inputs):
        out[:] = 0.0

class RollingLinearRegressionOfReturns(CustomFactor):
    """Rolling linear regression. Outputs: alpha, beta, r_value, p_value, stderr."""
    outputs = ['alpha', 'beta', 'r_value', 'p_value', 'stderr']
    def __init__(self, target, returns_length, regression_length, **kwargs):
        super().__init__(window_length=regression_length + returns_length, **kwargs)
    def compute(self, today, assets, out, *inputs):
        n = out.alpha.shape[0] if hasattr(out, 'alpha') else 0
        for attr in ['alpha', 'beta', 'r_value', 'p_value', 'stderr']:
            if hasattr(out, attr):
                getattr(out, attr)[:] = 0.0

RollingPearson = RollingPearsonOfReturns
RollingSpearman = RollingSpearmanOfReturns


class WeightedAverageValue(Factor):
    """Weighted average of a column using another column as weights."""
    def __init__(self, inputs=None, window_length=1, **kwargs):
        super().__init__(inputs=inputs, window_length=window_length, **kwargs)
    def compute(self, today, assets, out, values, weights):
        total_w = np.nansum(weights, axis=0)
        mask = total_w > 0
        out[:] = np.where(mask, np.nansum(values * weights, axis=0) / total_w, 0)


class PeerCount(Factor):
    """Count of non-NaN peer assets in each row."""
    window_length = 1
    def compute(self, today, assets, out, data):
        out[:] = np.sum(~np.isnan(data[-1]))


class RecarrayField(Factor):
    """Output type for multi-output CustomFactors."""
    pass


# ============================================================================
# Built-in Filters
# ============================================================================

class StaticAssets(Filter):
    """Filter that returns True for a fixed set of assets."""
    def __init__(self, assets):
        super().__init__()
        self.assets = set(assets)

class StaticSids(Filter):
    """Filter that returns True for a fixed set of SIDs."""
    def __init__(self, sids):
        super().__init__()
        self.sids = set(sids)

class AllPresent(Filter):
    """Filter that returns True if all inputs are present (non-NaN)."""
    def __init__(self, inputs=None, window_length=1, **kwargs):
        super().__init__(inputs=inputs, window_length=window_length, **kwargs)

class AtLeastN(Filter):
    """Filter that returns True if at least N inputs are present."""
    def __init__(self, inputs=None, window_length=1, N=1, **kwargs):
        super().__init__(inputs=inputs, window_length=window_length, **kwargs)
        self.N = N

class SingleAsset(Filter):
    """Filter for a single specific asset."""
    def __init__(self, asset):
        super().__init__()
        self.asset = asset


# ============================================================================
# Built-in Classifiers
# ============================================================================

class Quantiles(Classifier):
    """Classifier based on quantile rank."""
    def __init__(self, inputs=None, bins=5, mask=None, **kwargs):
        super().__init__(inputs=inputs, mask=mask, **kwargs)
        self.bins = bins

class Everything(Classifier):
    """Classifier that classifies all assets into the same group."""
    def __init__(self):
        super().__init__()


# ============================================================================
# Pipeline Class
# ============================================================================

class Pipeline:
    """
    Main pipeline class. Container for factors, filters, and classifiers.

    Usage:
        pipe = Pipeline(columns={'momentum': Returns(window_length=21)},
                        screen=AverageDollarVolume(window_length=30).top(500))
        attach_pipeline(pipe, 'my_pipe')
        # In before_trading_start:
        output = pipeline_output('my_pipe')
    """
    def __init__(self, columns=None, screen=None, domain=GENERIC):
        self.columns = columns or {}
        self.screen = screen
        self.domain = domain

    def add(self, term, name):
        """Add a named column to the pipeline."""
        self.columns[name] = term
        return self

    def remove(self, name):
        """Remove a named column."""
        self.columns.pop(name, None)
        return self

    def set_screen(self, screen):
        """Set the pipeline screen filter."""
        self.screen = screen
        return self

    def __repr__(self):
        cols = list(self.columns.keys())
        return f"Pipeline(columns={cols}, screen={self.screen is not None})"
