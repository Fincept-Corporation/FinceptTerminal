# ============================================================================
# Fincept Terminal - Portfolio Construction Models
# Portfolio optimization and construction for the framework pipeline
# ============================================================================


class UnconstrainedMeanVariancePortfolioOptimizer:
    """Mean-variance optimizer (simplified stub)."""
    def __init__(self, *args, **kwargs):
        pass

    def optimize(self, returns, *args, **kwargs):
        n = len(returns) if returns is not None else 1
        return [1.0 / n] * n
