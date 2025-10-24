"""
Market Data Management Module
============================

Provider-agnostic abstraction layer for market data management supporting derivatives pricing across multiple data sources. Implements unified API interfaces for real-time and historical market data retrieval, caching, and validation.

===== DATA SOURCES REQUIRED =====
INPUT:
  - Real-time market data feeds from external providers
  - Historical price and volatility data series
  - Interest rate curves and yield curve data
  - Dividend payment histories and forecasts
  - Corporate action data and event information
  - Market liquidity and bid-ask spread data
  - Provider-specific API credentials and connections

OUTPUT:
  - Standardized market data snapshots for pricing
  - Validated and cleaned market data series
  - Interpolated yield curves and volatility surfaces
  - Cached market data with expiration management
  - Fallback data from alternative providers
  - Market data quality and validation metrics

PARAMETERS:
  - provider_name: Data provider identifier
  - symbol: Financial instrument ticker or identifier
  - currency: Currency code for market data - default: "USD"
  - maturity: Time to maturity in years
  - curve_type: Yield curve type - default: "government"
  - max_age_seconds: Cache expiration time - default: 300
  - data_frequency: Data update frequency
  - interpolation_method: Curve interpolation method - default: "linear"
"""

from abc import ABC, abstractmethod
from typing import Dict, List, Optional, Union, Any
from datetime import datetime, date
from dataclasses import dataclass, field
import numpy as np
import pandas as pd
import logging
from enum import Enum

from .core import (
    UnderlyingType, DayCountConvention, ValidationError,
    ModelValidator, calculate_time_fraction
)

logger = logging.getLogger(__name__)


class DataProvider(Enum):
    """Supported market data providers"""
    BLOOMBERG = "bloomberg"
    REFINITIV = "refinitiv"
    YAHOO = "yahoo"
    ALPHA_VANTAGE = "alpha_vantage"
    MANUAL = "manual"
    CACHED = "cached"


class DataFrequency(Enum):
    """Data frequency options"""
    REAL_TIME = "real_time"
    MINUTE = "1min"
    HOURLY = "1hour"
    DAILY = "1day"
    WEEKLY = "1week"
    MONTHLY = "1month"


@dataclass
class YieldCurvePoint:
    """Single point on yield curve"""
    maturity: float  # Years
    rate: float  # Interest rate
    instrument_type: str = "government"  # government, corporate, swap

    def __post_init__(self):
        ModelValidator.validate_non_negative(self.maturity, "maturity")
        ModelValidator.validate_rate(self.rate, "interest rate")


@dataclass
class VolatilitySurfacePoint:
    """Single point on volatility surface"""
    strike: float
    time_to_expiry: float
    implied_volatility: float
    delta: Optional[float] = None

    def __post_init__(self):
        ModelValidator.validate_positive(self.strike, "strike")
        ModelValidator.validate_non_negative(self.time_to_expiry, "time_to_expiry")
        ModelValidator.validate_volatility(self.implied_volatility)


@dataclass
class MarketSnapshot:
    """Complete market data snapshot for pricing"""
    timestamp: datetime
    spot_price: float
    risk_free_rate: float
    dividend_yield: float = 0.0
    repo_rate: Optional[float] = None
    borrow_cost: Optional[float] = None
    yield_curve: Optional[List[YieldCurvePoint]] = None
    volatility_surface: Optional[List[VolatilitySurfacePoint]] = None
    bid_ask_spread: Optional[float] = None

    def __post_init__(self):
        ModelValidator.validate_positive(self.spot_price, "spot_price")
        ModelValidator.validate_rate(self.risk_free_rate, "risk_free_rate")
        ModelValidator.validate_non_negative(self.dividend_yield, "dividend_yield")


@dataclass
class CurveData:
    """Interest rate curve data"""
    curve_date: datetime
    curve_type: str  # government, swap, corporate
    currency: str
    day_count: DayCountConvention
    points: List[YieldCurvePoint] = field(default_factory=list)

    def add_point(self, maturity: float, rate: float, instrument_type: str = "government"):
        """Add point to curve"""
        self.points.append(YieldCurvePoint(maturity, rate, instrument_type))
        self.points.sort(key=lambda x: x.maturity)

    def interpolate_rate(self, maturity: float, method: str = "linear") -> float:
        """Interpolate rate for given maturity"""
        if not self.points:
            raise ValueError("No curve points available")

        # Sort points by maturity
        sorted_points = sorted(self.points, key=lambda x: x.maturity)

        # Handle edge cases
        if maturity <= sorted_points[0].maturity:
            return sorted_points[0].rate
        if maturity >= sorted_points[-1].maturity:
            return sorted_points[-1].rate

        # Find interpolation points
        for i in range(len(sorted_points) - 1):
            if sorted_points[i].maturity <= maturity <= sorted_points[i + 1].maturity:
                if method == "linear":
                    return self._linear_interpolation(
                        sorted_points[i], sorted_points[i + 1], maturity
                    )
                elif method == "cubic":
                    return self._cubic_interpolation(sorted_points, maturity, i)

        raise ValueError(f"Cannot interpolate rate for maturity {maturity}")

    def _linear_interpolation(self, p1: YieldCurvePoint, p2: YieldCurvePoint, maturity: float) -> float:
        """Linear interpolation between two points"""
        if p2.maturity == p1.maturity:
            return p1.rate

        weight = (maturity - p1.maturity) / (p2.maturity - p1.maturity)
        return p1.rate + weight * (p2.rate - p1.rate)

    def _cubic_interpolation(self, points: List[YieldCurvePoint], maturity: float, index: int) -> float:
        """Cubic spline interpolation (simplified)"""
        # For simplicity, use linear interpolation
        # In production, implement proper cubic spline
        return self._linear_interpolation(points[index], points[index + 1], maturity)


class MarketDataProvider(ABC):
    """Abstract base class for market data providers"""

    def __init__(self, provider_name: DataProvider):
        self.provider_name = provider_name
        self.connection_status = False
        self.last_update = None

    @abstractmethod
    def connect(self, **kwargs) -> bool:
        """Establish connection to data provider"""
        pass

    @abstractmethod
    def disconnect(self) -> bool:
        """Disconnect from data provider"""
        pass

    @abstractmethod
    def get_spot_price(self, symbol: str) -> float:
        """Get current spot price"""
        pass

    @abstractmethod
    def get_risk_free_rate(self, currency: str = "USD", maturity: float = 0.25) -> float:
        """Get risk-free rate for given currency and maturity"""
        pass

    @abstractmethod
    def get_dividend_yield(self, symbol: str) -> float:
        """Get dividend yield for equity"""
        pass

    @abstractmethod
    def get_volatility(self, symbol: str, maturity: float, strike: float = None) -> float:
        """Get implied volatility"""
        pass

    @abstractmethod
    def get_yield_curve(self, currency: str, curve_type: str = "government") -> CurveData:
        """Get complete yield curve"""
        pass


class ManualDataProvider(MarketDataProvider):
    """Manual data input provider for custom scenarios"""

    def __init__(self):
        super().__init__(DataProvider.MANUAL)
        self.data_cache = {}
        self.connection_status = True

    def connect(self, **kwargs) -> bool:
        self.connection_status = True
        return True

    def disconnect(self) -> bool:
        self.connection_status = False
        return True

    def set_spot_price(self, symbol: str, price: float):
        """Manually set spot price"""
        ModelValidator.validate_positive(price, f"spot price for {symbol}")
        self.data_cache[f"spot_{symbol}"] = price
        self.last_update = datetime.now()

    def set_risk_free_rate(self, rate: float, currency: str = "USD", maturity: float = 0.25):
        """Manually set risk-free rate"""
        ModelValidator.validate_rate(rate, f"risk-free rate for {currency}")
        self.data_cache[f"rf_{currency}_{maturity}"] = rate
        self.last_update = datetime.now()

    def set_dividend_yield(self, symbol: str, yield_rate: float):
        """Manually set dividend yield"""
        ModelValidator.validate_non_negative(yield_rate, f"dividend yield for {symbol}")
        self.data_cache[f"div_{symbol}"] = yield_rate
        self.last_update = datetime.now()

    def set_volatility(self, symbol: str, volatility: float, maturity: float = None, strike: float = None):
        """Manually set volatility"""
        ModelValidator.validate_volatility(volatility)
        key = f"vol_{symbol}"
        if maturity is not None:
            key += f"_{maturity}"
        if strike is not None:
            key += f"_{strike}"
        self.data_cache[key] = volatility
        self.last_update = datetime.now()

    def get_spot_price(self, symbol: str) -> float:
        key = f"spot_{symbol}"
        if key not in self.data_cache:
            raise ValueError(f"No spot price data for symbol: {symbol}")
        return self.data_cache[key]

    def get_risk_free_rate(self, currency: str = "USD", maturity: float = 0.25) -> float:
        key = f"rf_{currency}_{maturity}"
        if key not in self.data_cache:
            # Try default rate
            default_key = f"rf_{currency}_0.25"
            if default_key in self.data_cache:
                return self.data_cache[default_key]
            # Use global default
            return 0.02  # 2% default
        return self.data_cache[key]

    def get_dividend_yield(self, symbol: str) -> float:
        key = f"div_{symbol}"
        return self.data_cache.get(key, 0.0)  # Default to 0%

    def get_volatility(self, symbol: str, maturity: float, strike: float = None) -> float:
        # Try exact match first
        key = f"vol_{symbol}_{maturity}"
        if strike is not None:
            key += f"_{strike}"

        if key in self.data_cache:
            return self.data_cache[key]

        # Try without strike
        key_no_strike = f"vol_{symbol}_{maturity}"
        if key_no_strike in self.data_cache:
            return self.data_cache[key_no_strike]

        # Try generic volatility
        generic_key = f"vol_{symbol}"
        if generic_key in self.data_cache:
            return self.data_cache[generic_key]

        # Default volatility
        return 0.20  # 20% default

    def get_yield_curve(self, currency: str, curve_type: str = "government") -> CurveData:
        """Get yield curve (simplified for manual input)"""
        curve = CurveData(
            curve_date=datetime.now(),
            curve_type=curve_type,
            currency=currency,
            day_count=DayCountConvention.ACT_365
        )

        # Add default curve points if none exist
        default_rates = [
            (0.25, 0.02), (0.5, 0.022), (1.0, 0.025), (2.0, 0.028),
            (5.0, 0.032), (10.0, 0.035), (30.0, 0.038)
        ]

        for maturity, rate in default_rates:
            rf_rate = self.get_risk_free_rate(currency, maturity)
            curve.add_point(maturity, rf_rate)

        return curve


class YahooFinanceProvider(MarketDataProvider):
    """Yahoo Finance data provider (simplified implementation)"""

    def __init__(self):
        super().__init__(DataProvider.YAHOO)
        self.api_key = None

    def connect(self, **kwargs) -> bool:
        """Connect to Yahoo Finance"""
        try:
            # In real implementation, establish connection
            self.connection_status = True
            logger.info("Connected to Yahoo Finance")
            return True
        except Exception as e:
            logger.error(f"Failed to connect to Yahoo Finance: {e}")
            return False

    def disconnect(self) -> bool:
        self.connection_status = False
        return True

    def get_spot_price(self, symbol: str) -> float:
        """Get spot price from Yahoo Finance"""
        if not self.connection_status:
            raise ConnectionError("Not connected to Yahoo Finance")

        # Placeholder implementation
        # In real implementation, use yfinance library
        logger.info(f"Fetching spot price for {symbol} from Yahoo Finance")
        return 100.0  # Placeholder

    def get_risk_free_rate(self, currency: str = "USD", maturity: float = 0.25) -> float:
        """Get treasury rate as risk-free rate"""
        if not self.connection_status:
            raise ConnectionError("Not connected to Yahoo Finance")

        # Placeholder implementation
        return 0.02  # 2% placeholder

    def get_dividend_yield(self, symbol: str) -> float:
        """Get dividend yield from Yahoo Finance"""
        if not self.connection_status:
            raise ConnectionError("Not connected to Yahoo Finance")

        # Placeholder implementation
        return 0.015  # 1.5% placeholder

    def get_volatility(self, symbol: str, maturity: float, strike: float = None) -> float:
        """Calculate historical volatility"""
        if not self.connection_status:
            raise ConnectionError("Not connected to Yahoo Finance")

        # Placeholder implementation
        return 0.25  # 25% placeholder

    def get_yield_curve(self, currency: str, curve_type: str = "government") -> CurveData:
        """Get treasury yield curve"""
        if not self.connection_status:
            raise ConnectionError("Not connected to Yahoo Finance")

        # Placeholder implementation
        curve = CurveData(
            curve_date=datetime.now(),
            curve_type=curve_type,
            currency=currency,
            day_count=DayCountConvention.ACT_365
        )

        # Add sample treasury rates
        treasury_rates = [
            (0.25, 0.020), (0.5, 0.022), (1.0, 0.025), (2.0, 0.028),
            (5.0, 0.032), (10.0, 0.035), (30.0, 0.038)
        ]

        for maturity, rate in treasury_rates:
            curve.add_point(maturity, rate)

        return curve


class DataCache:
    """Simple caching mechanism for market data"""

    def __init__(self, max_age_seconds: int = 300):  # 5 minutes default
        self.cache = {}
        self.max_age = max_age_seconds

    def get(self, key: str) -> Optional[Any]:
        """Get cached data if not expired"""
        if key in self.cache:
            data, timestamp = self.cache[key]
            age = (datetime.now() - timestamp).total_seconds()
            if age <= self.max_age:
                return data
            else:
                del self.cache[key]  # Remove expired data
        return None

    def set(self, key: str, data: Any):
        """Cache data with timestamp"""
        self.cache[key] = (data, datetime.now())

    def clear(self):
        """Clear all cached data"""
        self.cache.clear()


class MarketDataManager:
    """Central manager for market data from multiple providers"""

    def __init__(self, primary_provider: MarketDataProvider = None):
        self.providers = {}
        self.primary_provider = primary_provider or ManualDataProvider()
        self.cache = DataCache()

        # Register default providers
        self.register_provider("manual", ManualDataProvider())
        self.register_provider("yahoo", YahooFinanceProvider())

    def register_provider(self, name: str, provider: MarketDataProvider):
        """Register a market data provider"""
        self.providers[name] = provider

    def set_primary_provider(self, provider_name: str):
        """Set primary data provider"""
        if provider_name not in self.providers:
            raise ValueError(f"Provider {provider_name} not registered")
        self.primary_provider = self.providers[provider_name]

    def get_market_snapshot(self, symbol: str, currency: str = "USD") -> MarketSnapshot:
        """Get complete market snapshot for symbol"""
        cache_key = f"snapshot_{symbol}_{currency}"
        cached_data = self.cache.get(cache_key)

        if cached_data:
            return cached_data

        try:
            snapshot = MarketSnapshot(
                timestamp=datetime.now(),
                spot_price=self.primary_provider.get_spot_price(symbol),
                risk_free_rate=self.primary_provider.get_risk_free_rate(currency),
                dividend_yield=self.primary_provider.get_dividend_yield(symbol)
            )

            self.cache.set(cache_key, snapshot)
            return snapshot

        except Exception as e:
            logger.error(f"Failed to get market snapshot: {e}")
            raise

    def get_spot_price(self, symbol: str, provider: str = None) -> float:
        """Get spot price with fallback providers"""
        provider_obj = self.providers.get(provider, self.primary_provider)

        try:
            return provider_obj.get_spot_price(symbol)
        except Exception as e:
            logger.warning(f"Primary provider failed for {symbol}: {e}")
            # Try fallback providers
            for name, fallback_provider in self.providers.items():
                if fallback_provider != provider_obj:
                    try:
                        return fallback_provider.get_spot_price(symbol)
                    except:
                        continue
            raise ValueError(f"No provider could fetch spot price for {symbol}")


# Create global market data manager instance
market_data_manager = MarketDataManager()

# Export main classes and functions
__all__ = [
    'DataProvider', 'DataFrequency', 'YieldCurvePoint', 'VolatilitySurfacePoint',
    'MarketSnapshot', 'CurveData', 'MarketDataProvider', 'ManualDataProvider',
    'YahooFinanceProvider', 'DataCache', 'MarketDataManager', 'market_data_manager'
]