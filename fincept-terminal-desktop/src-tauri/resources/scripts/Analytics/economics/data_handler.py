"""
Economic Data Management Module
==============================

Universal data adapter and provider interfaces for economic analysis. Handles data from multiple sources including external APIs and manual input, providing standardized formats and validation for all economic analysis modules with CFA Institute compliance.

===== DATA SOURCES REQUIRED =====
INPUT:
  - External API data sources (World Bank, IMF, central banks)
  - Manual data input and user-provided datasets
  - Real-time economic indicators and market data
  - Historical time series and macroeconomic data
  - Currency exchange rate feeds and financial market data
  - Trade statistics and balance of payments data
  - Custom data sources and proprietary datasets

OUTPUT:
  - Standardized data containers with validation metadata
  - Cleaned and processed economic data series
  - Data quality assessments and validation reports
  - Unified data formats for cross-module compatibility
  - Cached data for performance optimization
  - Error handling and missing data imputation

PARAMETERS:
  - data_source: Data source identifier or API endpoint
  - data_type: Economic data category (GDP, inflation, etc.)
  - frequency: Data frequency (daily, monthly, quarterly, annual)
  - start_date: Data collection start date
  - end_date: Data collection end date
  - countries: List of countries for data collection
  - indicators: Specific economic indicators to retrieve
  - api_key: Authentication key for external APIs
  - cache_duration: Data cache expiration time
"""

import json
import requests
import pandas as pd
import numpy as np
from decimal import Decimal
from typing import Dict, List, Tuple, Optional, Any, Union
from datetime import datetime, timedelta
from abc import ABC, abstractmethod
import logging
from .core import EconomicsBase, ValidationError, CalculationError, DataError, DataContainer

logger = logging.getLogger(__name__)


class DataProvider(ABC):
    """Abstract base class for data providers"""

    def __init__(self, provider_name: str, api_key: Optional[str] = None):
        self.provider_name = provider_name
        self.api_key = api_key
        self.last_request_time = None
        self.rate_limit_delay = 1.0  # seconds between requests

    @abstractmethod
    def get_exchange_rates(self, base_currency: str, target_currencies: List[str],
                           start_date: Optional[datetime] = None,
                           end_date: Optional[datetime] = None) -> pd.DataFrame:
        """Get exchange rate data"""
        pass

    @abstractmethod
    def get_economic_indicators(self, country: str, indicators: List[str],
                                start_date: Optional[datetime] = None,
                                end_date: Optional[datetime] = None) -> pd.DataFrame:
        """Get economic indicator data"""
        pass

    @abstractmethod
    def get_interest_rates(self, country: str, rate_types: List[str],
                           start_date: Optional[datetime] = None,
                           end_date: Optional[datetime] = None) -> pd.DataFrame:
        """Get interest rate data"""
        pass

    def _rate_limit(self):
        """Implement rate limiting"""
        if self.last_request_time:
            time_since_last = datetime.now() - self.last_request_time
            if time_since_last.total_seconds() < self.rate_limit_delay:
                import time
                time.sleep(self.rate_limit_delay - time_since_last.total_seconds())
        self.last_request_time = datetime.now()

    def _handle_api_error(self, response: requests.Response, context: str = ""):
        """Handle API response errors"""
        if not response.ok:
            error_msg = f"{self.provider_name} API error ({response.status_code})"
            if context:
                error_msg += f" in {context}"
            error_msg += f": {response.text}"
            raise DataError(error_msg)


class FREDConnector(DataProvider):
    """Federal Reserve Economic Data (FRED) API connector"""

    def __init__(self, api_key: str):
        super().__init__("FRED", api_key)
        self.base_url = "https://api.stlouisfed.org/fred"
        self.rate_limit_delay = 0.5  # FRED allows 120 requests per minute

    def get_exchange_rates(self, base_currency: str, target_currencies: List[str],
                           start_date: Optional[datetime] = None,
                           end_date: Optional[datetime] = None) -> pd.DataFrame:
        """Get exchange rates from FRED"""

        # FRED exchange rate series mapping
        fx_series_map = {
            'USD': {
                'EUR': 'DEXUSEU',
                'GBP': 'DEXUSUK',
                'JPY': 'DEXJPUS',
                'CAD': 'DEXCAUS',
                'CHF': 'DEXSZUS',
                'AUD': 'DEXUSAL'
            }
        }

        if base_currency not in fx_series_map:
            raise DataError(f"Base currency {base_currency} not supported by FRED")

        fx_data = {}
        for target_currency in target_currencies:
            if target_currency not in fx_series_map[base_currency]:
                logger.warning(f"Exchange rate {base_currency}/{target_currency} not available from FRED")
                continue

            series_id = fx_series_map[base_currency][target_currency]
            data = self._get_fred_series(series_id, start_date, end_date)
            fx_data[f"{base_currency}/{target_currency}"] = data

        if not fx_data:
            raise DataError("No exchange rate data retrieved")

        return pd.DataFrame(fx_data)

    def get_economic_indicators(self, country: str, indicators: List[str],
                                start_date: Optional[datetime] = None,
                                end_date: Optional[datetime] = None) -> pd.DataFrame:
        """Get economic indicators from FRED"""

        # FRED economic indicator series mapping
        us_indicators_map = {
            'gdp': 'GDP',
            'gdp_growth': 'A191RL1Q225SBEA',
            'inflation': 'CPIAUCSL',
            'unemployment': 'UNRATE',
            'interest_rate': 'FEDFUNDS',
            'industrial_production': 'INDPRO',
            'consumer_confidence': 'UMCSENT',
            'trade_balance': 'BOPGSTB',
            'government_debt': 'GFDEBTN'
        }

        if country.upper() != 'US':
            raise DataError(f"FRED primarily supports US data, country {country} not available")

        indicator_data = {}
        for indicator in indicators:
            if indicator.lower() not in us_indicators_map:
                logger.warning(f"Indicator {indicator} not available from FRED")
                continue

            series_id = us_indicators_map[indicator.lower()]
            data = self._get_fred_series(series_id, start_date, end_date)
            indicator_data[indicator] = data

        if not indicator_data:
            raise DataError("No economic indicator data retrieved")

        return pd.DataFrame(indicator_data)

    def get_interest_rates(self, country: str, rate_types: List[str],
                           start_date: Optional[datetime] = None,
                           end_date: Optional[datetime] = None) -> pd.DataFrame:
        """Get interest rates from FRED"""

        us_rates_map = {
            'federal_funds': 'FEDFUNDS',
            '10_year_treasury': 'GS10',
            '2_year_treasury': 'GS2',
            '3_month_treasury': 'GS3M',
            'prime_rate': 'PRIME',
            'corporate_aaa': 'AAA',
            'corporate_baa': 'BAA'
        }

        if country.upper() != 'US':
            raise DataError(f"FRED primarily supports US data, country {country} not available")

        rate_data = {}
        for rate_type in rate_types:
            if rate_type.lower() not in us_rates_map:
                logger.warning(f"Interest rate {rate_type} not available from FRED")
                continue

            series_id = us_rates_map[rate_type.lower()]
            data = self._get_fred_series(series_id, start_date, end_date)
            rate_data[rate_type] = data

        if not rate_data:
            raise DataError("No interest rate data retrieved")

        return pd.DataFrame(rate_data)

    def _get_fred_series(self, series_id: str, start_date: Optional[datetime] = None,
                         end_date: Optional[datetime] = None) -> pd.Series:
        """Get individual FRED series data"""

        self._rate_limit()

        params = {
            'series_id': series_id,
            'api_key': self.api_key,
            'file_type': 'json'
        }

        if start_date:
            params['observation_start'] = start_date.strftime('%Y-%m-%d')
        if end_date:
            params['observation_end'] = end_date.strftime('%Y-%m-%d')

        url = f"{self.base_url}/series/observations"

        try:
            response = requests.get(url, params=params)
            self._handle_api_error(response, f"series {series_id}")

            data = response.json()
            observations = data.get('observations', [])

            if not observations:
                raise DataError(f"No data returned for series {series_id}")

            # Convert to pandas Series
            dates = []
            values = []

            for obs in observations:
                if obs['value'] != '.':  # FRED uses '.' for missing values
                    try:
                        dates.append(pd.to_datetime(obs['date']))
                        values.append(float(obs['value']))
                    except (ValueError, KeyError):
                        continue

            if not dates:
                raise DataError(f"No valid data points for series {series_id}")

            return pd.Series(values, index=dates, name=series_id)

        except requests.exceptions.RequestException as e:
            raise DataError(f"Network error accessing FRED API: {e}")
        except json.JSONDecodeError as e:
            raise DataError(f"Invalid JSON response from FRED API: {e}")


class AlphaVantageConnector(DataProvider):
    """Alpha Vantage API connector for FX and economic data"""

    def __init__(self, api_key: str):
        super().__init__("Alpha Vantage", api_key)
        self.base_url = "https://www.alphavantage.co/query"
        self.rate_limit_delay = 12.0  # Free tier: 5 requests per minute

    def get_exchange_rates(self, base_currency: str, target_currencies: List[str],
                           start_date: Optional[datetime] = None,
                           end_date: Optional[datetime] = None) -> pd.DataFrame:
        """Get exchange rates from Alpha Vantage"""

        fx_data = {}

        for target_currency in target_currencies:
            self._rate_limit()

            params = {
                'function': 'FX_DAILY',
                'from_symbol': base_currency,
                'to_symbol': target_currency,
                'apikey': self.api_key,
                'outputsize': 'full'
            }

            try:
                response = requests.get(self.base_url, params=params)
                self._handle_api_error(response, f"FX data {base_currency}/{target_currency}")

                data = response.json()

                if 'Error Message' in data:
                    logger.warning(
                        f"Alpha Vantage error for {base_currency}/{target_currency}: {data['Error Message']}")
                    continue

                if 'Note' in data:
                    raise DataError(f"Alpha Vantage rate limit exceeded: {data['Note']}")

                time_series = data.get('Time Series FX (Daily)', {})

                if not time_series:
                    logger.warning(f"No FX data returned for {base_currency}/{target_currency}")
                    continue

                # Convert to pandas Series
                dates = []
                values = []

                for date_str, daily_data in time_series.items():
                    date = pd.to_datetime(date_str)

                    # Filter by date range if specified
                    if start_date and date < start_date:
                        continue
                    if end_date and date > end_date:
                        continue

                    dates.append(date)
                    values.append(float(daily_data['4. close']))

                if dates:
                    fx_data[f"{base_currency}/{target_currency}"] = pd.Series(
                        values, index=dates, name=f"{base_currency}/{target_currency}"
                    ).sort_index()

            except requests.exceptions.RequestException as e:
                raise DataError(f"Network error accessing Alpha Vantage API: {e}")
            except json.JSONDecodeError as e:
                raise DataError(f"Invalid JSON response from Alpha Vantage API: {e}")

        if not fx_data:
            raise DataError("No exchange rate data retrieved from Alpha Vantage")

        return pd.DataFrame(fx_data)

    def get_economic_indicators(self, country: str, indicators: List[str],
                                start_date: Optional[datetime] = None,
                                end_date: Optional[datetime] = None) -> pd.DataFrame:
        """Get economic indicators from Alpha Vantage"""

        # Alpha Vantage economic indicators mapping
        indicators_map = {
            'gdp': 'REAL_GDP',
            'gdp_quarterly': 'REAL_GDP_PER_CAPITA',
            'unemployment': 'UNEMPLOYMENT',
            'inflation': 'INFLATION',
            'interest_rate': 'FEDERAL_FUNDS_RATE',
            'consumer_confidence': 'CONSUMER_SENTIMENT'
        }

        if country.upper() != 'US':
            raise DataError(f"Alpha Vantage economic indicators only available for US")

        indicator_data = {}

        for indicator in indicators:
            if indicator.lower() not in indicators_map:
                logger.warning(f"Indicator {indicator} not available from Alpha Vantage")
                continue

            self._rate_limit()

            params = {
                'function': indicators_map[indicator.lower()],
                'apikey': self.api_key
            }

            try:
                response = requests.get(self.base_url, params=params)
                self._handle_api_error(response, f"economic indicator {indicator}")

                data = response.json()

                if 'Error Message' in data:
                    logger.warning(f"Alpha Vantage error for {indicator}: {data['Error Message']}")
                    continue

                # Extract time series data (structure varies by indicator)
                time_series_key = None
                for key in data.keys():
                    if 'data' in key.lower() or 'time series' in key.lower():
                        time_series_key = key
                        break

                if not time_series_key:
                    logger.warning(f"No time series data found for {indicator}")
                    continue

                time_series = data[time_series_key]

                dates = []
                values = []

                for item in time_series:
                    date = pd.to_datetime(item['date'])

                    # Filter by date range if specified
                    if start_date and date < start_date:
                        continue
                    if end_date and date > end_date:
                        continue

                    dates.append(date)
                    values.append(float(item['value']))

                if dates:
                    indicator_data[indicator] = pd.Series(
                        values, index=dates, name=indicator
                    ).sort_index()

            except requests.exceptions.RequestException as e:
                raise DataError(f"Network error accessing Alpha Vantage API: {e}")
            except json.JSONDecodeError as e:
                raise DataError(f"Invalid JSON response from Alpha Vantage API: {e}")

        if not indicator_data:
            raise DataError("No economic indicator data retrieved from Alpha Vantage")

        return pd.DataFrame(indicator_data)

    def get_interest_rates(self, country: str, rate_types: List[str],
                           start_date: Optional[datetime] = None,
                           end_date: Optional[datetime] = None) -> pd.DataFrame:
        """Get interest rates from Alpha Vantage"""

        rate_functions = {
            'federal_funds': 'FEDERAL_FUNDS_RATE',
            '10_year_treasury': 'TREASURY_YIELD',
            '3_month_treasury': 'TREASURY_YIELD'
        }

        if country.upper() != 'US':
            raise DataError(f"Alpha Vantage interest rates only available for US")

        rate_data = {}

        for rate_type in rate_types:
            if rate_type.lower() not in rate_functions:
                logger.warning(f"Interest rate {rate_type} not available from Alpha Vantage")
                continue

            self._rate_limit()

            params = {
                'function': rate_functions[rate_type.lower()],
                'apikey': self.api_key
            }

            # Add maturity parameter for treasury yields
            if 'treasury' in rate_type.lower():
                if '10_year' in rate_type.lower():
                    params['maturity'] = '10year'
                elif '3_month' in rate_type.lower():
                    params['maturity'] = '3month'

            try:
                response = requests.get(self.base_url, params=params)
                self._handle_api_error(response, f"interest rate {rate_type}")

                data = response.json()

                if 'Error Message' in data:
                    logger.warning(f"Alpha Vantage error for {rate_type}: {data['Error Message']}")
                    continue

                # Extract and process data similar to economic indicators
                time_series_key = None
                for key in data.keys():
                    if 'data' in key.lower():
                        time_series_key = key
                        break

                if time_series_key:
                    time_series = data[time_series_key]

                    dates = []
                    values = []

                    for item in time_series:
                        date = pd.to_datetime(item['date'])

                        if start_date and date < start_date:
                            continue
                        if end_date and date > end_date:
                            continue

                        dates.append(date)
                        values.append(float(item['value']))

                    if dates:
                        rate_data[rate_type] = pd.Series(
                            values, index=dates, name=rate_type
                        ).sort_index()

            except requests.exceptions.RequestException as e:
                raise DataError(f"Network error accessing Alpha Vantage API: {e}")
            except json.JSONDecodeError as e:
                raise DataError(f"Invalid JSON response from Alpha Vantage API: {e}")

        if not rate_data:
            raise DataError("No interest rate data retrieved from Alpha Vantage")

        return pd.DataFrame(rate_data)


class ManualDataInput(DataProvider):
    """Manual data input interface for user-provided data"""

    def __init__(self):
        super().__init__("Manual Input")
        self.data_store = {}

    def add_exchange_rate_data(self, currency_pair: str, dates: List[datetime],
                               rates: List[float]):
        """Add manual exchange rate data"""
        if len(dates) != len(rates):
            raise ValidationError("Dates and rates lists must have same length")

        df = pd.DataFrame({
            'date': dates,
            'rate': rates
        }).set_index('date')

        if 'exchange_rates' not in self.data_store:
            self.data_store['exchange_rates'] = {}

        self.data_store['exchange_rates'][currency_pair] = df['rate']

    def add_economic_indicator_data(self, country: str, indicator: str,
                                    dates: List[datetime], values: List[float]):
        """Add manual economic indicator data"""
        if len(dates) != len(values):
            raise ValidationError("Dates and values lists must have same length")

        df = pd.DataFrame({
            'date': dates,
            'value': values
        }).set_index('date')

        if 'economic_indicators' not in self.data_store:
            self.data_store['economic_indicators'] = {}
        if country not in self.data_store['economic_indicators']:
            self.data_store['economic_indicators'][country] = {}

        self.data_store['economic_indicators'][country][indicator] = df['value']

    def add_interest_rate_data(self, country: str, rate_type: str,
                               dates: List[datetime], rates: List[float]):
        """Add manual interest rate data"""
        if len(dates) != len(rates):
            raise ValidationError("Dates and rates lists must have same length")

        df = pd.DataFrame({
            'date': dates,
            'rate': rates
        }).set_index('date')

        if 'interest_rates' not in self.data_store:
            self.data_store['interest_rates'] = {}
        if country not in self.data_store['interest_rates']:
            self.data_store['interest_rates'][country] = {}

        self.data_store['interest_rates'][country][rate_type] = df['rate']

    def load_from_csv(self, file_path: str, data_type: str, **kwargs):
        """Load data from CSV file"""
        try:
            df = pd.read_csv(file_path)

            if data_type == 'exchange_rates':
                self._load_fx_from_csv(df, **kwargs)
            elif data_type == 'economic_indicators':
                self._load_indicators_from_csv(df, **kwargs)
            elif data_type == 'interest_rates':
                self._load_rates_from_csv(df, **kwargs)
            else:
                raise ValidationError(f"Unknown data type: {data_type}")

        except Exception as e:
            raise DataError(f"Error loading CSV file {file_path}: {e}")

    def _load_fx_from_csv(self, df: pd.DataFrame, date_column: str = 'date', **kwargs):
        """Load FX data from CSV"""
        if date_column not in df.columns:
            raise ValidationError(f"Date column '{date_column}' not found in CSV")

        df[date_column] = pd.to_datetime(df[date_column])
        df.set_index(date_column, inplace=True)

        if 'exchange_rates' not in self.data_store:
            self.data_store['exchange_rates'] = {}

        for column in df.columns:
            if column != date_column:
                self.data_store['exchange_rates'][column] = df[column]

    def _load_indicators_from_csv(self, df: pd.DataFrame, country: str,
                                  date_column: str = 'date', **kwargs):
        """Load economic indicators from CSV"""
        if date_column not in df.columns:
            raise ValidationError(f"Date column '{date_column}' not found in CSV")

        df[date_column] = pd.to_datetime(df[date_column])
        df.set_index(date_column, inplace=True)

        if 'economic_indicators' not in self.data_store:
            self.data_store['economic_indicators'] = {}
        if country not in self.data_store['economic_indicators']:
            self.data_store['economic_indicators'][country] = {}

        for column in df.columns:
            if column != date_column:
                self.data_store['economic_indicators'][country][column] = df[column]

    def _load_rates_from_csv(self, df: pd.DataFrame, country: str,
                             date_column: str = 'date', **kwargs):
        """Load interest rates from CSV"""
        if date_column not in df.columns:
            raise ValidationError(f"Date column '{date_column}' not found in CSV")

        df[date_column] = pd.to_datetime(df[date_column])
        df.set_index(date_column, inplace=True)

        if 'interest_rates' not in self.data_store:
            self.data_store['interest_rates'] = {}
        if country not in self.data_store['interest_rates']:
            self.data_store['interest_rates'][country] = {}

        for column in df.columns:
            if column != date_column:
                self.data_store['interest_rates'][country][column] = df[column]

    def get_exchange_rates(self, base_currency: str, target_currencies: List[str],
                           start_date: Optional[datetime] = None,
                           end_date: Optional[datetime] = None) -> pd.DataFrame:
        """Get exchange rates from manual data store"""

        if 'exchange_rates' not in self.data_store:
            raise DataError("No exchange rate data available")

        fx_data = {}

        for target_currency in target_currencies:
            pair = f"{base_currency}/{target_currency}"

            if pair in self.data_store['exchange_rates']:
                series = self.data_store['exchange_rates'][pair]

                # Filter by date range if specified
                if start_date or end_date:
                    mask = pd.Series(True, index=series.index)
                    if start_date:
                        mask &= series.index >= start_date
                    if end_date:
                        mask &= series.index <= end_date
                    series = series[mask]

                fx_data[pair] = series
            else:
                logger.warning(f"Exchange rate {pair} not found in manual data")

        if not fx_data:
            raise DataError("No matching exchange rate data found")

        return pd.DataFrame(fx_data)

    def get_economic_indicators(self, country: str, indicators: List[str],
                                start_date: Optional[datetime] = None,
                                end_date: Optional[datetime] = None) -> pd.DataFrame:
        """Get economic indicators from manual data store"""

        if 'economic_indicators' not in self.data_store:
            raise DataError("No economic indicator data available")

        if country not in self.data_store['economic_indicators']:
            raise DataError(f"No data available for country {country}")

        indicator_data = {}
        country_data = self.data_store['economic_indicators'][country]

        for indicator in indicators:
            if indicator in country_data:
                series = country_data[indicator]

                # Filter by date range if specified
                if start_date or end_date:
                    mask = pd.Series(True, index=series.index)
                    if start_date:
                        mask &= series.index >= start_date
                    if end_date:
                        mask &= series.index <= end_date
                    series = series[mask]

                indicator_data[indicator] = series
            else:
                logger.warning(f"Indicator {indicator} not found for {country}")

        if not indicator_data:
            raise DataError("No matching economic indicator data found")

        return pd.DataFrame(indicator_data)

    def get_interest_rates(self, country: str, rate_types: List[str],
                           start_date: Optional[datetime] = None,
                           end_date: Optional[datetime] = None) -> pd.DataFrame:
        """Get interest rates from manual data store"""

        if 'interest_rates' not in self.data_store:
            raise DataError("No interest rate data available")

        if country not in self.data_store['interest_rates']:
            raise DataError(f"No interest rate data available for country {country}")

        rate_data = {}
        country_data = self.data_store['interest_rates'][country]

        for rate_type in rate_types:
            if rate_type in country_data:
                series = country_data[rate_type]

                # Filter by date range if specified
                if start_date or end_date:
                    mask = pd.Series(True, index=series.index)
                    if start_date:
                        mask &= series.index >= start_date
                    if end_date:
                        mask &= series.index <= end_date
                    series = series[mask]

                rate_data[rate_type] = series
            else:
                logger.warning(f"Interest rate {rate_type} not found for {country}")

        if not rate_data:
            raise DataError("No matching interest rate data found")

        return pd.DataFrame(rate_data)


class DataStandardizer(EconomicsBase):
    """Data standardization and normalization utilities"""

    def __init__(self, precision: int = 8):
        super().__init__(precision)

    def standardize_exchange_rates(self, fx_data: pd.DataFrame,
                                   target_frequency: str = 'D') -> pd.DataFrame:
        """Standardize exchange rate data format and frequency"""

        if fx_data.empty:
            raise ValidationError("Empty exchange rate data provided")

        # Ensure datetime index
        if not isinstance(fx_data.index, pd.DatetimeIndex):
            raise ValidationError("Exchange rate data must have datetime index")

        # Remove any non-numeric data
        fx_data = fx_data.select_dtypes(include=[np.number])

        # Resample to target frequency
        if target_frequency != 'D':
            fx_data = fx_data.resample(target_frequency).last()

        # Forward fill missing values (carry forward last observation)
        fx_data = fx_data.fillna(method='ffill')

        # Drop any remaining NaN values
        fx_data = fx_data.dropna()

        # Convert to high precision decimals
        for column in fx_data.columns:
            fx_data[column] = fx_data[column].apply(lambda x: self.to_decimal(x) if pd.notna(x) else None)

        return fx_data

    def standardize_economic_indicators(self, indicator_data: pd.DataFrame,
                                        target_frequency: str = 'M') -> pd.DataFrame:
        """Standardize economic indicator data format and frequency"""

        if indicator_data.empty:
            raise ValidationError("Empty economic indicator data provided")

        # Ensure datetime index
        if not isinstance(indicator_data.index, pd.DatetimeIndex):
            raise ValidationError("Economic indicator data must have datetime index")

        # Remove any non-numeric data
        indicator_data = indicator_data.select_dtypes(include=[np.number])

        # Resample to target frequency (typically monthly for economic data)
        if target_frequency != 'M':
            # Use appropriate aggregation method based on data type
            indicator_data = indicator_data.resample(target_frequency).mean()

        # Handle missing values more carefully for economic data
        indicator_data = indicator_data.fillna(method='ffill', limit=3)  # Max 3 forward fills

        # Convert to high precision decimals
        for column in indicator_data.columns:
            indicator_data[column] = indicator_data[column].apply(
                lambda x: self.to_decimal(x) if pd.notna(x) else None
            )

        return indicator_data

    def calculate_returns(self, price_data: pd.DataFrame,
                          return_type: str = 'simple') -> pd.DataFrame:
        """Calculate returns from price data"""

        if price_data.empty:
            raise ValidationError("Empty price data provided")

        returns_data = pd.DataFrame(index=price_data.index)

        for column in price_data.columns:
            if return_type.lower() == 'simple':
                # Simple returns: (P_t - P_t-1) / P_t-1
                returns_data[f"{column}_return"] = price_data[column].pct_change()
            elif return_type.lower() == 'log':
                # Log returns: ln(P_t / P_t-1)
                returns_data[f"{column}_return"] = np.log(price_data[column] / price_data[column].shift(1))
            else:
                raise ValidationError(f"Unknown return type: {return_type}")

        # Remove first row (NaN from pct_change)
        returns_data = returns_data.dropna()

        return returns_data

    def align_time_series(self, *dataframes: pd.DataFrame,
                          join_method: str = 'inner') -> List[pd.DataFrame]:
        """Align multiple time series to common dates"""

        if not dataframes:
            raise ValidationError("No dataframes provided for alignment")

        # Find common date range
        if join_method == 'inner':
            # Use intersection of all date ranges
            common_dates = dataframes[0].index
            for df in dataframes[1:]:
                common_dates = common_dates.intersection(df.index)
        elif join_method == 'outer':
            # Use union of all date ranges
            common_dates = dataframes[0].index
            for df in dataframes[1:]:
                common_dates = common_dates.union(df.index)
        else:
            raise ValidationError(f"Unknown join method: {join_method}")

        if common_dates.empty:
            raise DataError("No common dates found across time series")

        # Reindex all dataframes to common dates
        aligned_dfs = []
        for df in dataframes:
            aligned_df = df.reindex(common_dates)
            if join_method == 'outer':
                # Forward fill missing values for outer join
                aligned_df = aligned_df.fillna(method='ffill')
            aligned_dfs.append(aligned_df)

        return aligned_dfs

    def detect_outliers(self, data: pd.Series, method: str = 'iqr',
                        threshold: float = 1.5) -> pd.Series:
        """Detect outliers in time series data"""

        if data.empty:
            raise ValidationError("Empty data series provided")

        outliers = pd.Series(False, index=data.index)

        if method.lower() == 'iqr':
            # Interquartile range method
            Q1 = data.quantile(0.25)
            Q3 = data.quantile(0.75)
            IQR = Q3 - Q1
            lower_bound = Q1 - threshold * IQR
            upper_bound = Q3 + threshold * IQR
            outliers = (data < lower_bound) | (data > upper_bound)

        elif method.lower() == 'zscore':
            # Z-score method
            z_scores = np.abs((data - data.mean()) / data.std())
            outliers = z_scores > threshold

        elif method.lower() == 'modified_zscore':
            # Modified Z-score using median
            median = data.median()
            mad = np.median(np.abs(data - median))
            modified_z_scores = 0.6745 * (data - median) / mad
            outliers = np.abs(modified_z_scores) > threshold

        else:
            raise ValidationError(f"Unknown outlier detection method: {method}")

        return outliers

    def clean_data(self, data: pd.DataFrame,
                   remove_outliers: bool = True,
                   outlier_method: str = 'iqr',
                   fill_missing: bool = True,
                   fill_method: str = 'ffill') -> pd.DataFrame:
        """Comprehensive data cleaning"""

        cleaned_data = data.copy()

        # Remove outliers if requested
        if remove_outliers:
            for column in cleaned_data.select_dtypes(include=[np.number]).columns:
                outliers = self.detect_outliers(cleaned_data[column], method=outlier_method)
                cleaned_data.loc[outliers, column] = np.nan

        # Fill missing values if requested
        if fill_missing:
            if fill_method == 'ffill':
                cleaned_data = cleaned_data.fillna(method='ffill')
            elif fill_method == 'bfill':
                cleaned_data = cleaned_data.fillna(method='bfill')
            elif fill_method == 'interpolate':
                cleaned_data = cleaned_data.interpolate()
            elif fill_method == 'mean':
                cleaned_data = cleaned_data.fillna(cleaned_data.mean())
            else:
                raise ValidationError(f"Unknown fill method: {fill_method}")

        return cleaned_data


class DataHandler(EconomicsBase):
    """Main data handler coordinating multiple data sources"""

    def __init__(self, precision: int = 8, base_currency: str = 'USD'):
        super().__init__(precision, base_currency)
        self.providers = {}
        self.standardizer = DataStandardizer(precision)
        self.cache = {}
        self.cache_ttl = 3600  # 1 hour cache TTL

    def add_provider(self, provider_name: str, provider: DataProvider):
        """Add a data provider"""
        self.providers[provider_name] = provider
        logger.info(f"Added data provider: {provider_name}")

    def remove_provider(self, provider_name: str):
        """Remove a data provider"""
        if provider_name in self.providers:
            del self.providers[provider_name]
            logger.info(f"Removed data provider: {provider_name}")

    def list_providers(self) -> List[str]:
        """List available data providers"""
        return list(self.providers.keys())

    def get_exchange_rates(self, base_currency: str, target_currencies: List[str],
                           start_date: Optional[datetime] = None,
                           end_date: Optional[datetime] = None,
                           providers: Optional[List[str]] = None,
                           standardize: bool = True) -> pd.DataFrame:
        """Get exchange rates from specified providers"""

        # Use all providers if none specified
        if providers is None:
            providers = list(self.providers.keys())

        # Check cache first
        cache_key = f"fx_{base_currency}_{'_'.join(target_currencies)}_{start_date}_{end_date}"
        if cache_key in self.cache:
            cache_entry = self.cache[cache_key]
            if datetime.now() - cache_entry['timestamp'] < timedelta(seconds=self.cache_ttl):
                logger.info(f"Returning cached exchange rate data")
                return cache_entry['data']

        all_data = []

        for provider_name in providers:
            if provider_name not in self.providers:
                logger.warning(f"Provider {provider_name} not available")
                continue

            try:
                provider = self.providers[provider_name]
                data = provider.get_exchange_rates(
                    base_currency, target_currencies, start_date, end_date
                )

                if not data.empty:
                    # Add provider suffix to column names
                    data.columns = [f"{col}_{provider_name}" for col in data.columns]
                    all_data.append(data)
                    logger.info(f"Retrieved FX data from {provider_name}: {len(data)} rows")

            except Exception as e:
                logger.error(f"Error retrieving FX data from {provider_name}: {e}")
                continue

        if not all_data:
            raise DataError("No exchange rate data retrieved from any provider")

        # Combine data from all providers
        combined_data = pd.concat(all_data, axis=1, sort=True)

        # Standardize if requested
        if standardize:
            combined_data = self.standardizer.standardize_exchange_rates(combined_data)

        # Cache the result
        self.cache[cache_key] = {
            'data': combined_data,
            'timestamp': datetime.now()
        }

        return combined_data

    def get_economic_indicators(self, country: str, indicators: List[str],
                                start_date: Optional[datetime] = None,
                                end_date: Optional[datetime] = None,
                                providers: Optional[List[str]] = None,
                                standardize: bool = True) -> pd.DataFrame:
        """Get economic indicators from specified providers"""

        # Use all providers if none specified
        if providers is None:
            providers = list(self.providers.keys())

        # Check cache first
        cache_key = f"indicators_{country}_{'_'.join(indicators)}_{start_date}_{end_date}"
        if cache_key in self.cache:
            cache_entry = self.cache[cache_key]
            if datetime.now() - cache_entry['timestamp'] < timedelta(seconds=self.cache_ttl):
                logger.info(f"Returning cached economic indicator data")
                return cache_entry['data']

        all_data = []

        for provider_name in providers:
            if provider_name not in self.providers:
                logger.warning(f"Provider {provider_name} not available")
                continue

            try:
                provider = self.providers[provider_name]
                data = provider.get_economic_indicators(
                    country, indicators, start_date, end_date
                )

                if not data.empty:
                    # Add provider suffix to column names
                    data.columns = [f"{col}_{provider_name}" for col in data.columns]
                    all_data.append(data)
                    logger.info(f"Retrieved indicator data from {provider_name}: {len(data)} rows")

            except Exception as e:
                logger.error(f"Error retrieving indicator data from {provider_name}: {e}")
                continue

        if not all_data:
            raise DataError("No economic indicator data retrieved from any provider")

        # Combine data from all providers
        combined_data = pd.concat(all_data, axis=1, sort=True)

        # Standardize if requested
        if standardize:
            combined_data = self.standardizer.standardize_economic_indicators(combined_data)

        # Cache the result
        self.cache[cache_key] = {
            'data': combined_data,
            'timestamp': datetime.now()
        }

        return combined_data

    def get_interest_rates(self, country: str, rate_types: List[str],
                           start_date: Optional[datetime] = None,
                           end_date: Optional[datetime] = None,
                           providers: Optional[List[str]] = None,
                           standardize: bool = True) -> pd.DataFrame:
        """Get interest rates from specified providers"""

        # Use all providers if none specified
        if providers is None:
            providers = list(self.providers.keys())

        # Check cache first
        cache_key = f"rates_{country}_{'_'.join(rate_types)}_{start_date}_{end_date}"
        if cache_key in self.cache:
            cache_entry = self.cache[cache_key]
            if datetime.now() - cache_entry['timestamp'] < timedelta(seconds=self.cache_ttl):
                logger.info(f"Returning cached interest rate data")
                return cache_entry['data']

        all_data = []

        for provider_name in providers:
            if provider_name not in self.providers:
                logger.warning(f"Provider {provider_name} not available")
                continue

            try:
                provider = self.providers[provider_name]
                data = provider.get_interest_rates(
                    country, rate_types, start_date, end_date
                )

                if not data.empty:
                    # Add provider suffix to column names
                    data.columns = [f"{col}_{provider_name}" for col in data.columns]
                    all_data.append(data)
                    logger.info(f"Retrieved rate data from {provider_name}: {len(data)} rows")

            except Exception as e:
                logger.error(f"Error retrieving rate data from {provider_name}: {e}")
                continue

        if not all_data:
            raise DataError("No interest rate data retrieved from any provider")

        # Combine data from all providers
        combined_data = pd.concat(all_data, axis=1, sort=True)

        # Standardize if requested
        if standardize:
            combined_data = self.standardizer.standardize_economic_indicators(combined_data)

        # Cache the result
        self.cache[cache_key] = {
            'data': combined_data,
            'timestamp': datetime.now()
        }

        return combined_data

    def clear_cache(self):
        """Clear the data cache"""
        self.cache.clear()
        logger.info("Data cache cleared")

    def get_cache_stats(self) -> Dict[str, Any]:
        """Get cache statistics"""
        now = datetime.now()
        active_entries = 0
        expired_entries = 0

        for cache_entry in self.cache.values():
            if now - cache_entry['timestamp'] < timedelta(seconds=self.cache_ttl):
                active_entries += 1
            else:
                expired_entries += 1

        return {
            'total_entries': len(self.cache),
            'active_entries': active_entries,
            'expired_entries': expired_entries,
            'cache_ttl_seconds': self.cache_ttl
        }

    def validate_data_quality(self, data: pd.DataFrame,
                              data_type: str = 'unknown') -> Dict[str, Any]:
        """Validate data quality and provide quality metrics"""

        if data.empty:
            return {
                'quality_score': 0,
                'issues': ['Empty dataset'],
                'recommendations': ['Obtain data from reliable sources']
            }

        quality_metrics = {
            'total_observations': len(data),
            'total_variables': len(data.columns),
            'missing_values': data.isnull().sum().sum(),
            'missing_percentage': (data.isnull().sum().sum() / (len(data) * len(data.columns))) * 100,
            'duplicate_rows': data.duplicated().sum(),
            'date_range': f"{data.index.min()} to {data.index.max()}" if isinstance(data.index,
                                                                                    pd.DatetimeIndex) else "Not time series"
        }

        # Detect outliers for numeric columns
        outlier_info = {}
        for column in data.select_dtypes(include=[np.number]).columns:
            outliers = self.standardizer.detect_outliers(data[column])
            outlier_info[column] = outliers.sum()

        quality_metrics['outliers_by_column'] = outlier_info
        quality_metrics['total_outliers'] = sum(outlier_info.values())

        # Calculate quality score (0-100)
        quality_score = 100

        # Deduct for missing values
        missing_penalty = min(quality_metrics['missing_percentage'] * 2, 50)
        quality_score -= missing_penalty

        # Deduct for outliers
        outlier_percentage = (quality_metrics['total_outliers'] / quality_metrics['total_observations']) * 100
        outlier_penalty = min(outlier_percentage * 1.5, 30)
        quality_score -= outlier_penalty

        # Deduct for duplicates
        duplicate_percentage = (quality_metrics['duplicate_rows'] / quality_metrics['total_observations']) * 100
        duplicate_penalty = min(duplicate_percentage * 3, 20)
        quality_score -= duplicate_penalty

        quality_score = max(0, quality_score)  # Ensure non-negative

        # Generate issues and recommendations
        issues = []
        recommendations = []

        if quality_metrics['missing_percentage'] > 10:
            issues.append(f"High missing data rate: {quality_metrics['missing_percentage']:.1f}%")
            recommendations.append("Consider data imputation or alternative data sources")

        if outlier_percentage > 5:
            issues.append(f"High outlier rate: {outlier_percentage:.1f}%")
            recommendations.append("Review outliers for data errors or consider robust analysis methods")

        if quality_metrics['duplicate_rows'] > 0:
            issues.append(f"Found {quality_metrics['duplicate_rows']} duplicate rows")
            recommendations.append("Remove duplicate observations")

        if isinstance(data.index, pd.DatetimeIndex):
            # Check for gaps in time series
            expected_freq = pd.infer_freq(data.index)
            if expected_freq:
                expected_range = pd.date_range(start=data.index.min(),
                                               end=data.index.max(),
                                               freq=expected_freq)
                missing_dates = len(expected_range) - len(data.index)
                if missing_dates > 0:
                    issues.append(f"Missing {missing_dates} time periods in series")
                    recommendations.append("Fill missing time periods or adjust analysis for irregular data")

        return {
            'quality_score': quality_score,
            'quality_metrics': quality_metrics,
            'issues': issues,
            'recommendations': recommendations,
            'data_type': data_type
        }

    def generate_data_summary(self, data: pd.DataFrame) -> Dict[str, Any]:
        """Generate comprehensive data summary"""

        summary = {
            'basic_info': {
                'shape': data.shape,
                'data_types': data.dtypes.to_dict(),
                'memory_usage': data.memory_usage(deep=True).sum(),
                'index_type': type(data.index).__name__
            },
            'statistical_summary': {},
            'missing_data': {
                'total_missing': data.isnull().sum().sum(),
                'missing_by_column': data.isnull().sum().to_dict(),
                'missing_percentage_by_column': (data.isnull().sum() / len(data) * 100).to_dict()
            }
        }

        # Statistical summary for numeric columns
        numeric_data = data.select_dtypes(include=[np.number])
        if not numeric_data.empty:
            summary['statistical_summary'] = {
                'count': numeric_data.count().to_dict(),
                'mean': numeric_data.mean().to_dict(),
                'std': numeric_data.std().to_dict(),
                'min': numeric_data.min().to_dict(),
                'max': numeric_data.max().to_dict(),
                'median': numeric_data.median().to_dict(),
                'skewness': numeric_data.skew().to_dict(),
                'kurtosis': numeric_data.kurtosis().to_dict()
            }

        # Time series specific information
        if isinstance(data.index, pd.DatetimeIndex):
            summary['time_series_info'] = {
                'start_date': data.index.min(),
                'end_date': data.index.max(),
                'frequency': pd.infer_freq(data.index),
                'total_periods': len(data.index),
                'business_days_only': data.index.freq == 'B' if hasattr(data.index, 'freq') else False
            }

        return summary

    def calculate(self, operation: str, **kwargs) -> Any:
        """Main data handler operation dispatcher"""

        operations = {
            'validate_quality': lambda: self.validate_data_quality(
                kwargs['data'], kwargs.get('data_type', 'unknown')
            ),
            'generate_summary': lambda: self.generate_data_summary(kwargs['data']),
            'standardize_fx': lambda: self.standardizer.standardize_exchange_rates(kwargs['data']),
            'standardize_indicators': lambda: self.standardizer.standardize_economic_indicators(kwargs['data']),
            'calculate_returns': lambda: self.standardizer.calculate_returns(
                kwargs['data'], kwargs.get('return_type', 'simple')
            ),
            'align_series': lambda: self.standardizer.align_time_series(
                *kwargs['dataframes'], join_method=kwargs.get('join_method', 'inner')
            ),
            'detect_outliers': lambda: self.standardizer.detect_outliers(
                kwargs['data'], kwargs.get('method', 'iqr'), kwargs.get('threshold', 1.5)
            ),
            'clean_data': lambda: self.standardizer.clean_data(
                kwargs['data'],
                kwargs.get('remove_outliers', True),
                kwargs.get('outlier_method', 'iqr'),
                kwargs.get('fill_missing', True),
                kwargs.get('fill_method', 'ffill')
            )
        }

        if operation not in operations:
            raise ValidationError(f"Unknown data operation: {operation}")

        result = operations[operation]()

        # Add metadata for certain operations
        if operation in ['validate_quality', 'generate_summary']:
            if isinstance(result, dict):
                result['metadata'] = self.get_metadata()
                result['operation'] = operation

        return result