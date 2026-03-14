# ============================================================================
# Fincept Terminal - Universe Selection Models
# Dynamic security filtering and selection for the framework pipeline
# ============================================================================

from ..algorithm import (
    UniverseSelectionModel, FundamentalUniverseSelectionModel,
    ManualUniverseSelectionModel, ScheduledUniverseSelectionModel,
    CustomUniverseSelectionModel
)


class OptionUniverseSelectionModel(UniverseSelectionModel):
    """Option chain universe selection."""
    def __init__(self, *args, **kwargs):
        pass


class FutureUniverseSelectionModel(UniverseSelectionModel):
    """Futures universe selection."""
    def __init__(self, *args, **kwargs):
        pass


class ETFConstituentsUniverseSelectionModel(UniverseSelectionModel):
    """ETF constituents universe selection."""
    def __init__(self, *args, **kwargs):
        pass


class QC500UniverseSelectionModel(FundamentalUniverseSelectionModel):
    """QC500 universe selection (top 500 by dollar volume)."""
    pass
