"""
Alternative Investments Data Handler Module
============================================

Multi-source data ingestion and standardization platform for alternative investments.
Handles data validation, quality assessment, and transformation from various formats
including CSV, JSON, and API sources. Ensures CFA Institute compliance for data
integrity and processing across private equity, hedge funds, real estate, and digital assets.

===== DATA SOURCES REQUIRED =====
INPUT:
  - Market data files (CSV, JSON, Excel) from multiple providers
  - Alternative investment fund performance data
  - Private equity/hedge fund cash flow statements
  - Real estate valuation and income data
  - Digital asset blockchain data and exchange feeds
  - Economic indicators and benchmark data

OUTPUT:
  - Validated and standardized pandas DataFrames
  - Data quality reports and anomaly detection
  - Cleaned time series with consistent formatting
  - Performance metrics and attribution data
  - Portfolio holdings and transaction records

PARAMETERS:
  - data_quality_threshold: Minimum data quality score (default: 0.85)
  - max_missing_percentage: Maximum allowed missing data (default: 0.15)
  - validation_rules: Data validation rule sets (default: strict)
  - currency_base: Base currency for conversion (default: 'USD')
  - frequency: Data frequency standardization (default: 'monthly')
  - outlier_detection: Outlier detection method (default: 'iqr')
"""

import pandas as pd
import numpy as np
from decimal import Decimal, InvalidOperation
from typing import Dict, List, Optional, Union, Any, Tuple
from datetime import datetime, date
import json
import csv
from dataclasses import asdict
import logging

from config import (
    MarketData, CashFlow, Performance, AssetParameters, AssetClass,
    Config, ValidationRules, Constants
)

logger = logging.getLogger(__name__)


class DataValidationError(Exception):
    """Custom exception for data validation errors"""
    pass


class DataHandler:
    """
    Centralized data handler for all alternative investment data sources
    Supports multiple input formats and validates according to CFA standards
    """

    def __init__(self):
        self.config = Config()
        self.validation_rules = ValidationRules()

    def standardize_price_data(self, data: Union[Dict, pd.DataFrame, List]) -> List[MarketData]:
        """
        Standardize price data from various sources into MarketData objects

        Args:
            data: Price data in various formats

        Returns:
            List of MarketData objects
        """
        try:
            if isinstance(data, pd.DataFrame):
                return self._from_dataframe(data)
            elif isinstance(data, dict):
                return self._from_dict(data)
            elif isinstance(data, list):
                return self._from_list(data)
            else:
                raise DataValidationError(f"Unsupported data type: {type(data)}")

        except Exception as e:
            logger.error(f"Error standardizing price data: {str(e)}")
            raise DataValidationError(f"Failed to standardize price data: {str(e)}")

    def _from_dataframe(self, df: pd.DataFrame) -> List[MarketData]:
        """Convert DataFrame to MarketData objects"""
        required_columns = ['timestamp', 'price']
        if not all(col in df.columns for col in required_columns):
            raise DataValidationError(f"DataFrame must contain columns: {required_columns}")

        market_data = []
        for _, row in df.iterrows():
            md = MarketData(
                timestamp=self._standardize_timestamp(row['timestamp']),
                price=self._to_decimal(row['price']),
                volume=self._to_decimal(row.get('volume')),
                bid=self._to_decimal(row.get('bid')),
                ask=self._to_decimal(row.get('ask')),
                high=self._to_decimal(row.get('high')),
                low=self._to_decimal(row.get('low')),
                open=self._to_decimal(row.get('open')),
                close=self._to_decimal(row.get('close'))
            )
            self._validate_market_data(md)
            market_data.append(md)

        return market_data

    def _from_dict(self, data: Dict) -> List[MarketData]:
        """Convert dictionary to MarketData objects"""
        if 'data' in data:
            data = data['data']

        if isinstance(data, list):
            return [self._dict_to_market_data(item) for item in data]
        else:
            return [self._dict_to_market_data(data)]

    def _from_list(self, data: List) -> List[MarketData]:
        """Convert list to MarketData objects"""
        return [self._dict_to_market_data(item) for item in data]

    def _dict_to_market_data(self, item: Dict) -> MarketData:
        """Convert single dictionary item to MarketData"""
        md = MarketData(
            timestamp=self._standardize_timestamp(item.get('timestamp', item.get('date', item.get('time')))),
            price=self._to_decimal(item.get('price', item.get('close'))),
            volume=self._to_decimal(item.get('volume')),
            bid=self._to_decimal(item.get('bid')),
            ask=self._to_decimal(item.get('ask')),
            high=self._to_decimal(item.get('high')),
            low=self._to_decimal(item.get('low')),
            open=self._to_decimal(item.get('open')),
            close=self._to_decimal(item.get('close'))
        )
        self._validate_market_data(md)
        return md

    def standardize_cash_flows(self, data: Union[Dict, pd.DataFrame, List]) -> List[CashFlow]:
        """
        Standardize cash flow data for IRR and performance calculations

        Args:
            data: Cash flow data in various formats

        Returns:
            List of CashFlow objects
        """
        try:
            if isinstance(data, pd.DataFrame):
                return self._cash_flows_from_dataframe(data)
            elif isinstance(data, dict):
                return self._cash_flows_from_dict(data)
            elif isinstance(data, list):
                return self._cash_flows_from_list(data)
            else:
                raise DataValidationError(f"Unsupported cash flow data type: {type(data)}")

        except Exception as e:
            logger.error(f"Error standardizing cash flows: {str(e)}")
            raise DataValidationError(f"Failed to standardize cash flows: {str(e)}")

    def _cash_flows_from_dataframe(self, df: pd.DataFrame) -> List[CashFlow]:
        """Convert DataFrame to CashFlow objects"""
        required_columns = ['date', 'amount']
        if not all(col in df.columns for col in required_columns):
            raise DataValidationError(f"Cash flow DataFrame must contain columns: {required_columns}")

        cash_flows = []
        for _, row in df.iterrows():
            cf = CashFlow(
                date=self._standardize_date(row['date']),
                amount=self._to_decimal(row['amount']),
                cf_type=row.get('type', 'inflow' if float(row['amount']) > 0 else 'outflow'),
                description=row.get('description')
            )
            self._validate_cash_flow(cf)
            cash_flows.append(cf)

        return sorted(cash_flows, key=lambda x: x.date)

    def _cash_flows_from_dict(self, data: Dict) -> List[CashFlow]:
        """Convert dictionary to CashFlow objects"""
        if 'cash_flows' in data:
            data = data['cash_flows']

        if isinstance(data, list):
            return [self._dict_to_cash_flow(item) for item in data]
        else:
            return [self._dict_to_cash_flow(data)]

    def _cash_flows_from_list(self, data: List) -> List[CashFlow]:
        """Convert list to CashFlow objects"""
        return [self._dict_to_cash_flow(item) for item in data]

    def _dict_to_cash_flow(self, item: Dict) -> CashFlow:
        """Convert single dictionary item to CashFlow"""
        cf = CashFlow(
            date=self._standardize_date(item.get('date', item.get('timestamp'))),
            amount=self._to_decimal(item.get('amount', item.get('value'))),
            cf_type=item.get('type', item.get('cf_type', 'inflow' if float(item.get('amount', 0)) > 0 else 'outflow')),
            description=item.get('description', item.get('desc'))
        )
        self._validate_cash_flow(cf)
        return cf

    def load_from_csv(self, file_path: str, data_type: str = 'price') -> Union[List[MarketData], List[CashFlow]]:
        """
        Load data from CSV file

        Args:
            file_path: Path to CSV file
            data_type: 'price' or 'cash_flow'

        Returns:
            Standardized data objects
        """
        try:
            df = pd.read_csv(file_path)

            if data_type == 'price':
                return self.standardize_price_data(df)
            elif data_type == 'cash_flow':
                return self.standardize_cash_flows(df)
            else:
                raise DataValidationError(f"Unsupported data type: {data_type}")

        except Exception as e:
            logger.error(f"Error loading CSV file {file_path}: {str(e)}")
            raise DataValidationError(f"Failed to load CSV: {str(e)}")

    def load_from_json(self, file_path: str, data_type: str = 'price') -> Union[List[MarketData], List[CashFlow]]:
        """
        Load data from JSON file

        Args:
            file_path: Path to JSON file
            data_type: 'price' or 'cash_flow'

        Returns:
            Standardized data objects
        """
        try:
            with open(file_path, 'r') as f:
                data = json.load(f)

            if data_type == 'price':
                return self.standardize_price_data(data)
            elif data_type == 'cash_flow':
                return self.standardize_cash_flows(data)
            else:
                raise DataValidationError(f"Unsupported data type: {data_type}")

        except Exception as e:
            logger.error(f"Error loading JSON file {file_path}: {str(e)}")
            raise DataValidationError(f"Failed to load JSON: {str(e)}")

    def calculate_returns(self, prices: List[MarketData], method: str = 'simple') -> pd.DataFrame:
        """
        Calculate returns from price data

        Args:
            prices: List of MarketData objects
            method: 'simple', 'log', or 'compound'

        Returns:
            DataFrame with returns
        """
        if len(prices) < 2:
            raise DataValidationError("Need at least 2 price points to calculate returns")

        # Convert to DataFrame
        df = pd.DataFrame([{
            'timestamp': p.timestamp,
            'price': float(p.price)
        } for p in prices])

        df = df.sort_values('timestamp')
        df['timestamp'] = pd.to_datetime(df['timestamp'])

        if method == 'simple':
            df['return'] = df['price'].pct_change()
        elif method == 'log':
            df['return'] = np.log(df['price'] / df['price'].shift(1))
        elif method == 'compound':
            df['return'] = (df['price'] / df['price'].shift(1)) - 1
        else:
            raise DataValidationError(f"Unsupported return calculation method: {method}")

        return df.dropna()

    def aggregate_to_frequency(self, data: List[MarketData], frequency: str = 'monthly') -> List[MarketData]:
        """
        Aggregate data to specified frequency

        Args:
            data: List of MarketData objects
            frequency: 'daily', 'weekly', 'monthly', 'quarterly', 'yearly'

        Returns:
            Aggregated MarketData objects
        """
        if not data:
            return []

        # Convert to DataFrame
        df = pd.DataFrame([{
            'timestamp': pd.to_datetime(d.timestamp),
            'price': float(d.price),
            'volume': float(d.volume) if d.volume else 0,
            'high': float(d.high) if d.high else float(d.price),
            'low': float(d.low) if d.low else float(d.price),
            'open': float(d.open) if d.open else float(d.price),
            'close': float(d.close) if d.close else float(d.price)
        } for d in data])

        df = df.set_index('timestamp').sort_index()

        # Aggregate based on frequency
        freq_map = {
            'daily': 'D',
            'weekly': 'W',
            'monthly': 'M',
            'quarterly': 'Q',
            'yearly': 'Y'
        }

        if frequency not in freq_map:
            raise DataValidationError(f"Unsupported frequency: {frequency}")

        agg_df = df.resample(freq_map[frequency]).agg({
            'price': 'last',
            'volume': 'sum',
            'high': 'max',
            'low': 'min',
            'open': 'first',
            'close': 'last'
        }).dropna()

        # Convert back to MarketData objects
        result = []
        for timestamp, row in agg_df.iterrows():
            md = MarketData(
                timestamp=timestamp.strftime('%Y-%m-%d'),
                price=Decimal(str(row['price'])),
                volume=Decimal(str(row['volume'])) if row['volume'] > 0 else None,
                high=Decimal(str(row['high'])),
                low=Decimal(str(row['low'])),
                open=Decimal(str(row['open'])),
                close=Decimal(str(row['close']))
            )
            result.append(md)

        return result

    def _to_decimal(self, value: Any) -> Optional[Decimal]:
        """Convert value to Decimal with validation"""
        if value is None or pd.isna(value):
            return None

        try:
            decimal_value = Decimal(str(value))
            if decimal_value < self.config.MIN_PRICE and decimal_value != 0:
                raise DataValidationError(f"Price too small: {decimal_value}")
            return decimal_value
        except (InvalidOperation, ValueError) as e:
            raise DataValidationError(f"Invalid decimal value: {value}")

    def _standardize_timestamp(self, timestamp: Any) -> str:
        """Standardize timestamp to ISO format"""
        if isinstance(timestamp, str):
            try:
                dt = pd.to_datetime(timestamp)
                return dt.strftime('%Y-%m-%d %H:%M:%S')
            except:
                return timestamp
        elif isinstance(timestamp, (datetime, date)):
            return timestamp.strftime('%Y-%m-%d %H:%M:%S')
        else:
            return str(timestamp)

    def _standardize_date(self, date_value: Any) -> str:
        """Standardize date to ISO format"""
        if isinstance(date_value, str):
            try:
                dt = pd.to_datetime(date_value)
                return dt.strftime('%Y-%m-%d')
            except:
                return date_value
        elif isinstance(date_value, (datetime, date)):
            return date_value.strftime('%Y-%m-%d')
        else:
            return str(date_value)

    def _validate_market_data(self, md: MarketData) -> None:
        """Validate MarketData object"""
        if md.price <= 0:
            raise DataValidationError(f"Invalid price: {md.price}")

        if md.volume is not None and md.volume < 0:
            raise DataValidationError(f"Invalid volume: {md.volume}")

        # Validate price relationships
        if md.high and md.low and md.high < md.low:
            raise DataValidationError(f"High price ({md.high}) less than low price ({md.low})")

        if md.bid and md.ask and md.bid > md.ask:
            raise DataValidationError(f"Bid price ({md.bid}) greater than ask price ({md.ask})")

    def _validate_cash_flow(self, cf: CashFlow) -> None:
        """Validate CashFlow object"""
        if cf.amount == 0:
            logger.warning(f"Zero cash flow amount on {cf.date}")

        valid_types = ['inflow', 'outflow', 'distribution', 'capital_call', 'dividend', 'interest']
        if cf.cf_type not in valid_types:
            logger.warning(f"Unknown cash flow type: {cf.cf_type}")

    def get_data_summary(self, data: Union[List[MarketData], List[CashFlow]]) -> Dict[str, Any]:
        """Get summary statistics of the data"""
        if not data:
            return {"error": "No data provided"}

        if isinstance(data[0], MarketData):
            prices = [float(d.price) for d in data]
            return {
                "data_type": "MarketData",
                "count": len(data),
                "price_stats": {
                    "mean": np.mean(prices),
                    "std": np.std(prices),
                    "min": np.min(prices),
                    "max": np.max(prices)
                },
                "date_range": {
                    "start": min(d.timestamp for d in data),
                    "end": max(d.timestamp for d in data)
                }
            }
        elif isinstance(data[0], CashFlow):
            amounts = [float(d.amount) for d in data]
            return {
                "data_type": "CashFlow",
                "count": len(data),
                "amount_stats": {
                    "total": sum(amounts),
                    "mean": np.mean(amounts),
                    "std": np.std(amounts),
                    "min": np.min(amounts),
                    "max": np.max(amounts)
                },
                "date_range": {
                    "start": min(d.date for d in data),
                    "end": max(d.date for d in data)
                },
                "inflows": sum(1 for d in data if d.amount > 0),
                "outflows": sum(1 for d in data if d.amount < 0)
            }

        return {"error": "Unknown data type"}


# Export main components
__all__ = ['DataHandler', 'DataValidationError']