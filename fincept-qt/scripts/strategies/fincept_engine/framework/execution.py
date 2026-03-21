# ============================================================================
# Fincept Terminal - Execution Models
# Order execution strategies for the framework pipeline
# ============================================================================

from ..algorithm import ExecutionModel


class SpreadExecutionModel(ExecutionModel):
    """Spread-based execution model (stub)."""
    def __init__(self, accepting_spread_percent=0.005):
        self._spread = accepting_spread_percent

    def execute(self, algorithm, targets):
        for target in targets:
            existing = algorithm.portfolio[str(target.symbol)].quantity
            diff = target.quantity - existing
            if abs(diff) > 0:
                algorithm.market_order(target.symbol, diff)
