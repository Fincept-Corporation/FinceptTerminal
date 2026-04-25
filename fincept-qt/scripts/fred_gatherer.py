"""
FRED Macroeconomic Data Gatherer for FinceptTerminal
-----------------------------------------------------
MVP implementation for Fincept-Corporation/FinceptTerminal#224

Integrates FRED (Federal Reserve Economic Data) API to provide
macroeconomic indicators for valuation modeling.

Usage:
    from fred_gatherer import FREDGatherer, FREDConfig
    gatherer = FREDGatherer(api_key="YOUR_KEY")
    gdp = gatherer.get_series("GDP")
    indicators = gatherer.get_indicators(["CPIAUCSL", "FEDFUNDS", "UNRATE"])
"""

import os
import time
import logging
from dataclasses import dataclass, field
from datetime import datetime, timedelta
from typing import Any, Dict, List, Optional, Tuple, Union

import pandas as pd
import numpy as np

try:
    import requests
    HAS_REQUESTS = True
except ImportError:
    HAS_REQUESTS = False

logger = logging.getLogger(__name__)


@dataclass
class FREDConfig:
    """Configuration for FRED data gathering."""
    api_key: str = ""
    base_url: str = "https://api.stlouisfed.org/fred"
    cache_dir: Optional[str] = None
    cache_ttl: int = 3600  # 1 hour default
    timeout: int = 30
    max_retries: int = 3
    retry_delay: float = 1.0

    def __post_init__(self):
        if not self.api_key:
            self.api_key = os.environ.get("FRED_API_KEY", "")
        if not self.api_key:
            logger.warning("No FRED API key provided. Set FRED_API_KEY env var or pass api_key.")


@dataclass
class SeriesMetadata:
    """Metadata for a FRED series."""
    id: str
    title: str
    observation_start: str
    observation_end: str
    frequency: str
    frequency_short: str
    units: str
    units_short: str
    last_updated: str
    popularity: int
    notes: str = ""


class FREDGatherer:
    """
    FRED API client for macroeconomic data gathering.

    Provides methods to fetch economic series, search for indicators,
    and prepare data for valuation modeling.

    Example:
        >>> gatherer = FREDGatherer(api_key="your_key")
        >>>
        >>> # Get a single series
        >>> gdp = gatherer.get_series("GDP")
        >>> print(gdp.tail())
        >>>
        >>> # Get multiple indicators
        >>> indicators = gatherer.get_indicators([
        ...     "CPIAUCSL",    # CPI
        ...     "FEDFUNDS",    # Fed Funds Rate
        ...     "UNRATE",      # Unemployment Rate
        ...     "GDP",         # GDP
        ... ])
        >>> print(indicators.head())
        >>>
        >>> # Search for series
        >>> results = gatherer.search("inflation")
        >>>
        >>> # Get valuation-ready data
        >>> valuation_data = gatherer.get_valuation_indicators()
    """

    # Common macroeconomic series IDs for valuation
    VALUATION_INDICATORS = {
        "gdp": "GDP",
        "real_gdp": "GDPC1",
        "gdp_deflator": "GDPDEF",
        "cpi": "CPIAUCSL",
        "core_cpi": "CPILFESL",
        "pce": "PCEPI",
        "core_pce": "PCEPILFE",
        "fed_funds_rate": "FEDFUNDS",
        "unemployment": "UNRATE",
        "nonfarm_payrolls": "PAYEMS",
        "10y_treasury": "GS10",
        "2y_treasury": "GS2",
        "3m_treasury": "TB3MS",
        "sp500": "SP500",
        "industrial_production": "INDPRO",
        "retail_sales": "RSAFS",
        "housing_starts": "HOUST",
        "consumer_confidence": "UMCSENT",
        "ppi": "PPIACO",
        "imports": "NMII",
        "exports": "NMXEI",
        "trade_balance": "BOPBSTB",
    }

    def __init__(self, config: Optional[FREDConfig] = None, api_key: str = ""):
        self.config = config or FREDConfig(api_key=api_key)
        if not self.config.api_key:
            raise ValueError(
                "FRED API key required. Get one at: https://fred.stlouisfed.org/docs/api/api_key.html"
            )
        self._session = requests.Session() if HAS_REQUESTS else None
        self._cache: Dict[str, Tuple[Any, float]] = {}

    def _api_request(self, endpoint: str, params: Dict[str, Any]) -> Dict:
        """Make an API request with retry logic."""
        if not HAS_REQUESTS:
            raise ImportError("requests package required. Install with: pip install requests")

        url = f"{self.config.base_url}/{endpoint}"
        params["api_key"] = self.config.api_key
        params["file_type"] = "json"

        for attempt in range(self.config.max_retries):
            try:
                resp = self._session.get(
                    url, params=params, timeout=self.config.timeout
                )
                resp.raise_for_status()
                return resp.json()
            except requests.exceptions.RequestException as e:
                if attempt < self.config.max_retries - 1:
                    time.sleep(self.config.retry_delay * (2 ** attempt))
                    continue
                raise RuntimeError(f"FRED API request failed: {e}")

    def get_series(
        self,
        series_id: str,
        observation_start: Optional[str] = None,
        observation_end: Optional[str] = None,
        frequency: Optional[str] = None,
        aggregation_method: str = "avg",
    ) -> pd.Series:
        """
        Fetch a single FRED series.

        Args:
            series_id: FRED series ID (e.g., "GDP", "CPIAUCSL")
            observation_start: Start date (YYYY-MM-DD)
            observation_end: End date (YYYY-MM-DD)
            frequency: Output frequency ("annual", "quarterly", "monthly", "weekly", "daily")
            aggregation_method: How to aggregate ("avg", "sum", "eop")

        Returns:
            pandas Series with datetime index
        """
        params = {"series_id": series_id}
        if observation_start:
            params["observation_start"] = observation_start
        if observation_end:
            params["observation_end"] = observation_end
        if frequency:
            params["frequency"] = frequency
        params["aggregation_method"] = aggregation_method

        data = self._api_request("series/observations", params)

        if not data.get("observations"):
            logger.warning(f"No observations found for {series_id}")
            return pd.Series(dtype=float, name=series_id)

        records = []
        for obs in data["observations"]:
            if obs["value"] != ".":
                records.append({
                    "date": obs["date"],
                    "value": float(obs["value"]),
                })

        df = pd.DataFrame(records)
        if df.empty:
            return pd.Series(dtype=float, name=series_id)

        df["date"] = pd.to_datetime(df["date"])
        series = df.set_index("date")["value"]
        series.name = series_id
        series.index.name = "date"
        return series

    def get_series_metadata(self, series_id: str) -> SeriesMetadata:
        """Get metadata for a FRED series."""
        data = self._api_request("series", {"series_id": series_id})
        series_data = data["series"]
        info = series_data[0] if isinstance(series_data, list) else series_data
        return SeriesMetadata(
            id=info["id"],
            title=info["title"],
            observation_start=info["observation_start"],
            observation_end=info["observation_end"],
            frequency=info["frequency"],
            frequency_short=info["frequency_short"],
            units=info["units"],
            units_short=info["units_short"],
            last_updated=info["last_updated"],
            popularity=info["popularity"],
            notes=info.get("notes", ""),
        )

    def get_indicators(
        self,
        series_ids: List[str],
        observation_start: Optional[str] = None,
        observation_end: Optional[str] = None,
    ) -> pd.DataFrame:
        """
        Fetch multiple series and combine into a single DataFrame.

        Args:
            series_ids: List of FRED series IDs
            observation_start: Start date
            observation_end: End date

        Returns:
            DataFrame with all series as columns, aligned by date
        """
        frames = []
        for sid in series_ids:
            try:
                series = self.get_series(
                    sid,
                    observation_start=observation_start,
                    observation_end=observation_end,
                )
                frames.append(series)
            except Exception as e:
                logger.error(f"Failed to fetch {sid}: {e}")

        if not frames:
            return pd.DataFrame()

        return pd.concat(frames, axis=1)

    def get_valuation_indicators(
        self,
        observation_start: Optional[str] = None,
        observation_end: Optional[str] = None,
    ) -> pd.DataFrame:
        """
        Get a standard set of macroeconomic indicators for valuation modeling.

        Returns:
            DataFrame with common valuation indicators
        """
        series_ids = list(self.VALUATION_INDICATORS.values())
        return self.get_indicators(series_ids, observation_start, observation_end)

    def search(self, query: str, limit: int = 20) -> List[Dict[str, Any]]:
        """
        Search for FRED series by keyword.

        Args:
            query: Search query
            limit: Max results

        Returns:
            List of series info dicts
        """
        data = self._api_request("series/search", {
            "search_text": query,
            "limit": limit,
            "sort_order": "desc",
            "sort_by": "popularity",
        })
        return data.get("seriess", [])

    def get_categories(self) -> List[Dict[str, Any]]:
        """Get FRED category hierarchy."""
        data = self._api_request("category/children", {"category_id": 0})
        return data.get("categories", [])

    def calculate_spread(self, series_a: pd.Series, series_b: pd.Series) -> pd.Series:
        """Calculate spread between two series (e.g., yield curve spread)."""
        aligned = pd.concat([series_a, series_b], axis=1).dropna()
        if len(aligned.columns) < 2:
            return pd.Series(dtype=float)
        return aligned.iloc[:, 0] - aligned.iloc[:, 1]

    def calculate_yoy_growth(self, series: pd.Series, periods: int = 12) -> pd.Series:
        """Calculate year-over-year percentage change."""
        return series.pct_change(periods=periods) * 100

    def calculate_real_value(
        self, nominal_series: pd.Series, cpi_series: pd.Series,
        base_year_cpi: float = 100.0
    ) -> pd.Series:
        """Convert nominal values to real (inflation-adjusted) values."""
        aligned = pd.concat([nominal_series, cpi_series], axis=1).dropna()
        if len(aligned.columns) < 2:
            return pd.Series(dtype=float)
        return (aligned.iloc[:, 0] / aligned.iloc[:, 1]) * base_year_cpi


# ============================================================
# Example usage
# ============================================================

def demo():
    """Demonstrate FRED gatherer with mock data."""
    print("FRED Macroeconomic Data Gatherer - MVP Demo")
    print("=" * 50)

    # Show available indicators
    gatherer = FREDGatherer.__new__(FREDGatherer)
    gatherer.VALUATION_INDICATORS = FREDGatherer.VALUATION_INDICATORS

    print("\nAvailable Valuation Indicators:")
    for name, sid in gatherer.VALUATION_INDICATORS.items():
        print(f"  {name:25s} → {sid}")

    print(f"\nTotal indicators: {len(gatherer.VALUATION_INDICATORS)}")

    # Show example code
    print("\n" + "=" * 50)
    print("Example Usage:")
    print("""
    from fred_gatherer import FREDGatherer

    gatherer = FREDGatherer(api_key="your_key")

    # Get GDP data
    gdp = gatherer.get_series("GDP", observation_start="2000-01-01")
    print(gdp.tail())

    # Get multiple indicators
    indicators = gatherer.get_indicators([
        "CPIAUCSL",    # CPI
        "FEDFUNDS",    # Fed Funds Rate
        "UNRATE",      # Unemployment
    ])

    # Get all valuation indicators
    valuation = gatherer.get_valuation_indicators()

    # Calculate yield curve spread (10y - 2y)
    t10 = gatherer.get_series("GS10")
    t2 = gatherer.get_series("GS2")
    spread = gatherer.calculate_spread(t10, t2)
    print(f"Current spread: {spread.dropna().iloc[-1]:.2f}%")

    # Search for series
    results = gatherer.search("GDP growth")
    for r in results[:5]:
        print(f"  {r['id']}: {r['title']}")
    """)


if __name__ == "__main__":
    demo()
