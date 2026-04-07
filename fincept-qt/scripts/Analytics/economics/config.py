"""Economics module configuration."""

from dataclasses import dataclass, field
from typing import Dict, List


@dataclass
class DataSources:
    fred_api_key: str = ""
    world_bank_base: str = "https://api.worldbank.org/v2"
    imf_base: str = "https://www.imf.org/external/datamapper/api/v1"
    oecd_base: str = "https://stats.oecd.org/SDMX-JSON/data"


@dataclass
class CalculationPrecision:
    default: int = 8
    display: int = 4
    currency: int = 2


@dataclass
class EconomicsConfig:
    precision: CalculationPrecision = field(default_factory=CalculationPrecision)
    data_sources: DataSources = field(default_factory=DataSources)
    base_currency: str = "USD"
    forecast_horizon: int = 12
    confidence_level: float = 0.90
    lookback_years: int = 10
