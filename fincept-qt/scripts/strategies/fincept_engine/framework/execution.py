# ============================================================================
# Fincept Terminal - Execution Models
# Order execution strategies for the framework pipeline
# ============================================================================

from ..algorithm import ExecutionModel


class SpreadExecutionModel(ExecutionModel):
    """Needs bid/ask quotes, which the daily bars do not carry."""
    def __init__(self, *args, **kwargs):
        from ..errors import not_supported
        raise not_supported("spread-based order execution", "It needs bid/ask quotes; the engine has daily OHLC bars.")
