
"""Portfolio Data Manager Module
===============================

Portfolio data management and processing

===== DATA SOURCES REQUIRED =====
INPUT:
  - Portfolio holdings and transaction history
  - Asset price data and market returns
  - Benchmark indices and market data
  - Investment policy statements and constraints
  - Risk tolerance and preference parameters

OUTPUT:
  - Portfolio performance metrics and attribution
  - Risk analysis and diversification metrics
  - Rebalancing recommendations and optimization
  - Portfolio analytics reports and visualizations
  - Investment strategy recommendations

PARAMETERS:
  - optimization_method: Portfolio optimization method (default: 'mean_variance')
  - risk_free_rate: Risk-free rate for calculations (default: 0.02)
  - rebalance_frequency: Portfolio rebalancing frequency (default: 'quarterly')
  - max_weight: Maximum single asset weight (default: 0.10)
  - benchmark: Portfolio benchmark index (default: 'market_index')
"""



import pandas as pd
import numpy as np
from typing import Dict, List, Optional, Union, Tuple, Any
from datetime import datetime, timedelta
import warnings
from pathlib import Path
import json
import pickle
from abc import ABC, abstractmethod

from config import (
    DataSchema, AssetClass, MathConstants,
    PRICE_DATA_SCHEMA, RETURN_DATA_SCHEMA, PORTFOLIO_DATA_SCHEMA,
    ERROR_MESSAGES, validate_returns
)


class DataProvider(ABC):
    """Abstract base class for data providers"""

    @abstractmethod
    def get_price_data(self, symbols: List[str], start_date: str, end_date: str) -> pd.DataFrame:
        pass

    @abstractmethod
    def get_economic_data(self, indicators: List[str], start_date: str, end_date: str) -> pd.DataFrame:
        pass


class ManualDataProvider(DataProvider):
    """Manual data input provider"""

    def __init__(self):
        self.price_data = {}
        self.economic_data = {}

    def add_price_data(self, symbol: str, data: pd.DataFrame):
        """Add price data manually"""
        self.price_data[symbol] = data

    def add_economic_data(self, indicator: str, data: pd.DataFrame):
        """Add economic data manually"""
        self.economic_data[indicator] = data

    def get_price_data(self, symbols: List[str], start_date: str, end_date: str) -> pd.DataFrame:
        """Retrieve price data for symbols"""
        result_data = {}
        for symbol in symbols:
            if symbol in self.price_data:
                data = self.price_data[symbol].copy()
                data = self._filter_by_date(data, start_date, end_date)
                result_data[symbol] = data
        return result_data

    def get_economic_data(self, indicators: List[str], start_date: str, end_date: str) -> pd.DataFrame:
        """Retrieve economic data"""
        result_data = {}
        for indicator in indicators:
            if indicator in self.economic_data:
                data = self.economic_data[indicator].copy()
                data = self._filter_by_date(data, start_date, end_date)
                result_data[indicator] = data
        return result_data

    def _filter_by_date(self, data: pd.DataFrame, start_date: str, end_date: str) -> pd.DataFrame:
        """Filter data by date range"""
        if 'date' in data.columns:
            data['date'] = pd.to_datetime(data['date'])
            mask = (data['date'] >= start_date) & (data['date'] <= end_date)
            return data[mask]
        return data


class DataValidator:
    """Data validation and quality checks"""

    @staticmethod
    def validate_schema(data: pd.DataFrame, schema: DataSchema) -> Tuple[bool, List[str]]:
        """Validate data against schema"""
        errors = []

        # Check required columns
        for col in schema.required_columns:
            if col not in data.columns:
                errors.append(f"Missing required column: {col}")

        # Check minimum observations
        if len(data) < schema.min_observations:
            errors.append(f"Insufficient data: {len(data)} < {schema.min_observations}")

        # Check missing data ratio
        for col in schema.numeric_columns:
            if col in data.columns:
                missing_ratio = data[col].isnull().sum() / len(data)
                if missing_ratio > schema.max_missing_ratio:
                    errors.append(f"Too much missing data in {col}: {missing_ratio:.2%}")

        # Validate date columns
        for col in schema.date_columns:
            if col in data.columns:
                try:
                    pd.to_datetime(data[col])
                except:
                    errors.append(f"Invalid date format in column: {col}")

        return len(errors) == 0, errors

    @staticmethod
    def check_data_quality(data: pd.DataFrame) -> Dict[str, Any]:
        """Comprehensive data quality assessment"""
        quality_report = {
            "total_rows": len(data),
            "total_columns": len(data.columns),
            "missing_data": {},
            "duplicates": 0,
            "outliers": {},
            "data_types": {}
        }

        # Missing data analysis
        for col in data.columns:
            missing_count = data[col].isnull().sum()
            quality_report["missing_data"][col] = {
                "count": missing_count,
                "percentage": missing_count / len(data) * 100
            }

        # Duplicate rows
        quality_report["duplicates"] = data.duplicated().sum()

        # Outlier detection (for numeric columns)
        numeric_cols = data.select_dtypes(include=[np.number]).columns
        for col in numeric_cols:
            if not data[col].isnull().all():
                Q1 = data[col].quantile(0.25)
                Q3 = data[col].quantile(0.75)
                IQR = Q3 - Q1
                lower_bound = Q1 - 1.5 * IQR
                upper_bound = Q3 + 1.5 * IQR
                outliers = ((data[col] < lower_bound) | (data[col] > upper_bound)).sum()
                quality_report["outliers"][col] = outliers

        # Data types
        quality_report["data_types"] = dict(data.dtypes)

        return quality_report


class DataTransformer:
    """Data cleaning and transformation utilities"""

    @staticmethod
    def calculate_returns(prices: pd.Series, method: str = "simple") -> pd.Series:
        """Calculate returns from price series"""
        if method == "simple":
            returns = prices.pct_change().dropna()
        elif method == "log":
            returns = np.log(prices / prices.shift(1)).dropna()
        else:
            raise ValueError("Method must be 'simple' or 'log'")

        return returns

    @staticmethod
    def handle_missing_data(data: pd.DataFrame, method: str = "forward_fill") -> pd.DataFrame:
        """Handle missing data"""
        data_clean = data.copy()

        if method == "forward_fill":
            data_clean = data_clean.fillna(method='ffill')
        elif method == "backward_fill":
            data_clean = data_clean.fillna(method='bfill')
        elif method == "interpolate":
            numeric_cols = data_clean.select_dtypes(include=[np.number]).columns
            data_clean[numeric_cols] = data_clean[numeric_cols].interpolate()
        elif method == "drop":
            data_clean = data_clean.dropna()
        elif method == "mean":
            numeric_cols = data_clean.select_dtypes(include=[np.number]).columns
            data_clean[numeric_cols] = data_clean[numeric_cols].fillna(data_clean[numeric_cols].mean())

        return data_clean

    @staticmethod
    def remove_outliers(data: pd.Series, method: str = "iqr", threshold: float = 1.5) -> pd.Series:
        """Remove outliers from data series"""
        if method == "iqr":
            Q1 = data.quantile(0.25)
            Q3 = data.quantile(0.75)
            IQR = Q3 - Q1
            lower_bound = Q1 - threshold * IQR
            upper_bound = Q3 + threshold * IQR
            return data[(data >= lower_bound) & (data <= upper_bound)]
        elif method == "zscore":
            z_scores = np.abs((data - data.mean()) / data.std())
            return data[z_scores <= threshold]

        return data

    @staticmethod
    def normalize_weights(weights: Union[List, np.ndarray]) -> np.ndarray:
        """Normalize weights to sum to 1.0"""
        weights_array = np.array(weights)
        return weights_array / np.sum(weights_array)

    @staticmethod
    def align_data(data_dict: Dict[str, pd.DataFrame], date_column: str = "date") -> Dict[str, pd.DataFrame]:
        """Align multiple datasets by common dates"""
        if not data_dict:
            return {}

        # Find common date range
        all_dates = None
        for key, df in data_dict.items():
            if date_column in df.columns:
                dates = pd.to_datetime(df[date_column])
                if all_dates is None:
                    all_dates = set(dates)
                else:
                    all_dates = all_dates.intersection(set(dates))

        if all_dates is None:
            return data_dict

        all_dates = sorted(list(all_dates))

        # Align all datasets
        aligned_data = {}
        for key, df in data_dict.items():
            if date_column in df.columns:
                df_copy = df.copy()
                df_copy[date_column] = pd.to_datetime(df_copy[date_column])
                aligned_data[key] = df_copy[df_copy[date_column].isin(all_dates)]
            else:
                aligned_data[key] = df

        return aligned_data


class DataCache:
    """Simple file-based caching system"""

    def __init__(self, cache_dir: str = "cache"):
        self.cache_dir = Path(cache_dir)
        self.cache_dir.mkdir(exist_ok=True)

    def _get_cache_path(self, key: str) -> Path:
        """Generate cache file path"""
        return self.cache_dir / f"{key}.pkl"

    def get(self, key: str) -> Optional[Any]:
        """Retrieve data from cache"""
        cache_path = self._get_cache_path(key)
        if cache_path.exists():
            try:
                with open(cache_path, 'rb') as f:
                    cached_data = pickle.load(f)

                # Check if cache is still valid (24 hours)
                cache_time = datetime.fromtimestamp(cache_path.stat().st_mtime)
                if datetime.now() - cache_time < timedelta(hours=24):
                    return cached_data
            except:
                pass
        return None

    def set(self, key: str, data: Any) -> None:
        """Store data in cache"""
        cache_path = self._get_cache_path(key)
        try:
            with open(cache_path, 'wb') as f:
                pickle.dump(data, f)
        except Exception as e:
            warnings.warn(f"Failed to cache data: {e}")

    def clear(self) -> None:
        """Clear all cached data"""
        for cache_file in self.cache_dir.glob("*.pkl"):
            cache_file.unlink()


class DataManager:
    """Main data management interface"""

    def __init__(self, provider: Optional[DataProvider] = None, use_cache: bool = True):
        self.provider = provider or ManualDataProvider()
        self.validator = DataValidator()
        self.transformer = DataTransformer()
        self.cache = DataCache() if use_cache else None

    def get_validated_data(self, data_type: str, **kwargs) -> Tuple[pd.DataFrame, Dict]:
        """Get and validate data"""
        cache_key = f"{data_type}_{hash(str(kwargs))}"

        # Try cache first
        if self.cache:
            cached_result = self.cache.get(cache_key)
            if cached_result is not None:
                return cached_result

        # Get data from provider
        if data_type == "price":
            raw_data = self.provider.get_price_data(**kwargs)
            schema = PRICE_DATA_SCHEMA
        elif data_type == "economic":
            raw_data = self.provider.get_economic_data(**kwargs)
            schema = None  # Economic data schema varies
        else:
            raise ValueError(f"Unknown data type: {data_type}")

        # Process and validate data
        processed_data = {}
        validation_results = {}

        for key, df in raw_data.items():
            # Data quality check
            quality_report = self.validator.check_data_quality(df)

            # Schema validation if applicable
            if schema:
                is_valid, errors = self.validator.validate_schema(df, schema)
                validation_results[key] = {
                    "valid": is_valid,
                    "errors": errors,
                    "quality": quality_report
                }

            # Clean data
            cleaned_df = self.transformer.handle_missing_data(df)
            processed_data[key] = cleaned_df

        result = (processed_data, validation_results)

        # Cache result
        if self.cache:
            self.cache.set(cache_key, result)

        return result

    def calculate_return_matrix(self, price_data: Dict[str, pd.DataFrame],
                                return_type: str = "simple") -> pd.DataFrame:
        """Calculate return matrix from price data"""
        returns_dict = {}

        for symbol, df in price_data.items():
            if 'close' in df.columns and 'date' in df.columns:
                df_sorted = df.sort_values('date')
                prices = df_sorted['close']
                returns = self.transformer.calculate_returns(prices, return_type)
                returns_dict[symbol] = returns

        if not returns_dict:
            raise ValueError("No valid price data found")

        # Align returns by index
        return_matrix = pd.DataFrame(returns_dict)
        return return_matrix.dropna()

    def get_covariance_matrix(self, returns: pd.DataFrame,
                              annualize: bool = True) -> np.ndarray:
        """Calculate covariance matrix from returns"""
        cov_matrix = returns.cov().values

        if annualize:
            cov_matrix *= MathConstants.TRADING_DAYS_YEAR

        return cov_matrix

    def get_correlation_matrix(self, returns: pd.DataFrame) -> np.ndarray:
        """Calculate correlation matrix from returns"""
        return returns.corr().values