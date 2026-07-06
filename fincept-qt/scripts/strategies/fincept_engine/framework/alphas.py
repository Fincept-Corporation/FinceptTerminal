# ============================================================================
# Fincept Terminal - Alpha Models
# Pre-built alpha models for the framework pipeline
# ============================================================================

from ..algorithm import AlphaModel, ConstantAlphaModel, CompositeAlphaModel


class RsiAlphaModel(AlphaModel):
    """RSI-based alpha model."""
    def __init__(self, period=14, resolution=None):
        self._period = period


class HistoricalReturnsAlphaModel(AlphaModel):
    """Historical returns alpha model."""
    def __init__(self, lookback=1, resolution=None):
        self._lookback = lookback


class EmaCrossAlphaModel(AlphaModel):
    """EMA crossover alpha model."""
    def __init__(self, fast_period=12, slow_period=26, resolution=None):
        self._fast = fast_period
        self._slow = slow_period


class MacdAlphaModel(AlphaModel):
    """MACD-based alpha model."""
    def __init__(self, fast_period=12, slow_period=26, signal_period=9, *args, **kwargs):
        pass


class PairsTradingAlphaModel(AlphaModel):
    """Pairs trading alpha model."""
    def __init__(self, *args, **kwargs):
        pass


class BasePairsTradingAlphaModel(AlphaModel):
    """Base class for pairs trading alpha models."""
    def __init__(self, *args, **kwargs):
        pass


class PearsonCorrelationPairsTradingAlphaModel(AlphaModel):
    """Pearson correlation pairs trading alpha model."""
    def __init__(self, *args, **kwargs):
        pass
