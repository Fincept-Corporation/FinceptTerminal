"""
skfolio Data Management & Preprocessing
======================================

This module provides comprehensive data management and preprocessing capabilities
for portfolio optimization. It handles data ingestion from multiple sources,
quality validation, missing data imputation, and preprocessing pipelines.

Key Features:
- Multi-source data ingestion (CSV, Excel, databases, APIs)
- Data quality validation and cleaning
- Missing data imputation strategies
- Return calculation and frequency conversion
- Factor data processing
- Outlier detection and handling
- Data alignment and synchronization
- Preprocessing pipelines

Usage:
    from skfolio_data import DataManager

    manager = DataManager()
    data = manager.load_data("prices.csv", start_date="2020-01-01")
    clean_data = manager.clean_data(data)
    returns = manager.calculate_returns(clean_data)
"""

import pandas as pd
import numpy as np
import warnings
from typing import Dict, List, Optional, Union, Tuple, Any, Callable
from dataclasses import dataclass, field
from datetime import datetime, timedelta
from pathlib import Path
import json
import logging

# Statistical imports
from scipy import stats
from sklearn.preprocessing import StandardScaler, MinMaxScaler, RobustScaler
from sklearn.impute import SimpleImputer, KNNImputer

warnings.filterwarnings('ignore')
logger = logging.getLogger(__name__)

@dataclass
class DataSource:
    """Configuration for data sources"""

    source_type: str  # "csv", "excel", "database", "api", "yfinance"
    source_path: str
    asset_column: str = "ticker" or "symbol"
    date_column: str = "date"
    value_columns: List[str] = field(default_factory=lambda: ["close", "open", "high", "low", "volume"])
    date_format: Optional[str] = None
    frequency: str = "daily"  # "daily", "weekly", "monthly"
    currency: str = "USD"

    # Database/API specific
    query: Optional[str] = None
    connection_string: Optional[str] = None
    api_key: Optional[str] = None
    api_params: Optional[Dict] = None

@dataclass
class DataQualityReport:
    """Data quality assessment report"""

    total_observations: int
    total_assets: int
    date_range: Tuple[datetime, datetime]
    missing_data_pct: float
    zero_return_pct: float
    outlier_count: int
    duplicate_dates: int
    frequency_consistency: bool
    data_type_issues: List[str]
    quality_score: float  # 0-100

    def to_dict(self) -> Dict:
        """Convert to dictionary"""
        return {
            "total_observations": self.total_observations,
            "total_assets": self.total_assets,
            "date_range": (self.date_range[0].isoformat(), self.date_range[1].isoformat()),
            "missing_data_pct": self.missing_data_pct,
            "zero_return_pct": self.zero_return_pct,
            "outlier_count": self.outlier_count,
            "duplicate_dates": self.duplicate_dates,
            "frequency_consistency": self.frequency_consistency,
            "data_type_issues": self.data_type_issues,
            "quality_score": self.quality_score
        }

class DataManager:
    """
    Comprehensive data management for portfolio optimization

    Handles data loading, cleaning, validation, and preprocessing
    for portfolio optimization workflows.
    """

    def __init__(self):
        """Initialize data manager"""
        self.data_sources = {}
        self.loaded_data = {}
        self.quality_reports = {}
        self.preprocessing_history = []

        # Default preprocessing parameters
        self.default_params = {
            "missing_data_threshold": 0.05,  # 5% threshold
            "outlier_threshold": 3.0,  # 3 standard deviations
            "min_history_length": 252,  # 1 year of daily data
            "max_missing_consecutive": 5,  # days
            "return_frequency": "daily",
            "log_returns": False
        }

        logger.info("DataManager initialized")

    def register_data_source(self, name: str, source: DataSource) -> Dict:
        """
        Register a data source configuration

        Parameters:
        -----------
        name : str
            Name for the data source
        source : DataSource
            Data source configuration

        Returns:
        --------
        Dict with registration status
        """

        try:
            self.data_sources[name] = source

            return {
                "status": "success",
                "source_name": name,
                "source_type": source.source_type,
                "message": f"Data source '{name}' registered successfully"
            }

        except Exception as e:
            logger.error(f"Failed to register data source '{name}': {e}")
            return {
                "status": "error",
                "source_name": name,
                "message": str(e)
            }

    def load_data(self,
                  data: Union[Dict, pd.DataFrame, str] = None,
                  asset_names: Optional[List[str]] = None,
                  source_name: Optional[str] = None,
                  start_date: Optional[str] = None,
                  end_date: Optional[str] = None,
                  assets: Optional[List[str]] = None,
                  columns: Optional[List[str]] = None,
                  progress_callback: Optional[Callable] = None) -> pd.DataFrame:
        """
        Load data from various sources

        Parameters:
        -----------
        data : Union[Dict, pd.DataFrame, str], optional
            Direct data input (dict, DataFrame, or file path)
        asset_names : List[str], optional
            Asset names for dict data
        source_name : str, optional
            Name of registered data source
        start_date : str, optional
            Start date for data
        end_date : str, optional
            End date for data
        assets : List[str], optional
            Specific assets to load
        columns : List[str], optional
            Specific columns to load
        progress_callback : Callable, optional
            Progress callback

        Returns:
        --------
        DataFrame with loaded data
        """

        # Handle direct data input
        if data is not None:
            if isinstance(data, dict):
                # Convert dict to DataFrame
                df = pd.DataFrame(data).T
                if asset_names:
                    df.columns = asset_names
                df.index = pd.to_datetime(df.index)
                return df
            elif isinstance(data, pd.DataFrame):
                return data
            elif isinstance(data, str):
                # Load from file path
                if data.endswith('.csv'):
                    df = pd.read_csv(data, index_col=0, parse_dates=True)
                    return df
                elif data.endswith('.xlsx') or data.endswith('.xls'):
                    df = pd.read_excel(data, index_col=0)
                    df.index = pd.to_datetime(df.index)
                    return df

        # Handle registered data source
        if source_name is not None:
            if source_name not in self.data_sources:
                raise ValueError(f"Data source '{source_name}' not registered")

            source = self.data_sources[source_name]

            try:
                if progress_callback:
                    progress_callback(f"Loading data from {source.source_type} source...")

                # Load data based on source type
                if source.source_type == "csv":
                    data_df = self._load_csv(source)
                elif source.source_type == "excel":
                    data_df = self._load_excel(source)
                elif source.source_type == "database":
                    data_df = self._load_database(source)
                elif source.source_type == "yfinance":
                    data_df = self._load_yfinance(source)
                else:
                    raise ValueError(f"Unsupported source type: {source.source_type}")

                if progress_callback:
                    progress_callback("Data loaded successfully")

                # Process loaded data
                data_df = self._process_raw_data(data_df, source)

                # Apply filters
                if start_date or end_date:
                    data_df = self._filter_by_date(data_df, start_date, end_date)

                if assets:
                    data_df = self._filter_by_assets(data_df, assets)

                if columns:
                    data_df = self._filter_by_columns(data_df, columns)

                # Store loaded data
                self.loaded_data[source_name] = data_df

                # Generate quality report
                quality_report = self.assess_data_quality(data_df)
                self.quality_reports[source_name] = quality_report

                if progress_callback:
                    progress_callback(f"Data processing completed. Quality score: {quality_report.quality_score:.1f}")

                return data_df

            except Exception as e:
                logger.error(f"Failed to load data from '{source_name}': {e}")
                raise

        raise ValueError("Either data, source_name must be provided")

    def preprocess_data(self,
                       data: pd.DataFrame,
                       method: str = "fill_forward") -> pd.DataFrame:
        """
        Preprocess data for optimization

        Parameters:
        -----------
        data : pd.DataFrame
            Data to preprocess
        method : str
            Preprocessing method ("fill_forward", "interpolate", "drop")

        Returns:
        --------
        Preprocessed DataFrame
        """

        if method == "fill_forward":
            # Forward fill missing values
            processed_data = data.fillna(method='ffill').fillna(method='bfill')
        elif method == "interpolate":
            # Linear interpolation
            processed_data = data.interpolate(method='linear')
        elif method == "drop":
            # Drop rows with missing values
            processed_data = data.dropna()
        else:
            raise ValueError(f"Unknown preprocessing method: {method}")

        return processed_data

    def _load_csv(self, source: DataSource) -> pd.DataFrame:
        """Load data from CSV file"""

        data = pd.read_csv(source.source_path)

        # Parse dates
        if source.date_column in data.columns:
            if source.date_format:
                data[source.date_column] = pd.to_datetime(data[source.date_column],
                                                          format=source.date_format)
            else:
                data[source.date_column] = pd.to_datetime(data[source.date_column])

        return data

    def _load_excel(self, source: DataSource) -> pd.DataFrame:
        """Load data from Excel file"""

        data = pd.read_excel(source.source_path)

        # Parse dates
        if source.date_column in data.columns:
            data[source.date_column] = pd.to_datetime(data[source.date_column])

        return data

    def _load_database(self, source: DataSource) -> pd.DataFrame:
        """Load data from database"""

        try:
            import sqlalchemy
            engine = sqlalchemy.create_engine(source.connection_string)
            data = pd.read_sql(source.query, engine)

            if source.date_column in data.columns:
                data[source.date_column] = pd.to_datetime(data[source.date_column])

            return data

        except ImportError:
            raise ImportError("sqlalchemy is required for database connections")

    def _load_yfinance(self, source: DataSource) -> pd.DataFrame:
        """Load data from Yahoo Finance"""

        try:
            import yfinance as yf

            # Extract tickers from source path or parameters
            tickers = source.api_params.get('tickers', []) if source.api_params else []

            if not tickers:
                raise ValueError("Tickers must be specified in api_params for yfinance source")

            # Download data
            data = yf.download(tickers, start=source.api_params.get('start'),
                             end=source.api_params.get('end'))

            # Flatten multi-level columns if needed
            if isinstance(data.columns, pd.MultiIndex):
                data.columns = [f"{col[1]}_{col[0]}" if col[1] != "" else col[0]
                              for col in data.columns.values]

            # Reset index to get dates as column
            data = data.reset_index()

            return data

        except ImportError:
            raise ImportError("yfinance is required for Yahoo Finance data")

    def _process_raw_data(self, data: pd.DataFrame, source: DataSource) -> pd.DataFrame:
        """Process raw data into standard format"""

        # Set date column as index
        if source.date_column in data.columns:
            data = data.set_index(source.date_column)
            data.index.name = "date"

        # Identify price columns
        price_columns = []
        for col in source.value_columns:
            if col in data.columns:
                price_columns.append(col)

        if not price_columns:
            raise ValueError("No valid price columns found in data")

        # Handle multi-asset data (wide format)
        if source.asset_column in data.columns:
            # Convert from long to wide format
            price_data = data.pivot(
                index=data.index.name if data.index.name else 'date',
                columns=source.asset_column,
                values=price_columns[0]  # Use first price column
            )
        else:
            # Assume wide format
            price_data = data[price_columns]

        # Sort by date
        price_data = price_data.sort_index()

        return price_data

    def _filter_by_date(self, data: pd.DataFrame, start_date: str, end_date: str) -> pd.DataFrame:
        """Filter data by date range"""

        if start_date:
            data = data[data.index >= pd.to_datetime(start_date)]
        if end_date:
            data = data[data.index <= pd.to_datetime(end_date)]

        return data

    def _filter_by_assets(self, data: pd.DataFrame, assets: List[str]) -> pd.DataFrame:
        """Filter data by specific assets"""

        available_assets = [asset for asset in assets if asset in data.columns]
        missing_assets = set(assets) - set(available_assets)

        if missing_assets:
            logger.warning(f"Assets not found in data: {missing_assets}")

        return data[available_assets]

    def _filter_by_columns(self, data: pd.DataFrame, columns: List[str]) -> pd.DataFrame:
        """Filter data by specific columns"""

        available_columns = [col for col in columns if col in data.columns]
        return data[available_columns]

    def assess_data_quality(self, data: pd.DataFrame) -> DataQualityReport:
        """
        Assess data quality and generate report

        Parameters:
        -----------
        data : pd.DataFrame
            Data to assess

        Returns:
        --------
        DataQualityReport
        """

        # Basic statistics
        total_observations = len(data)
        total_assets = len(data.columns)
        date_range = (data.index[0], data.index[-1])

        # Missing data analysis
        missing_data = data.isnull().sum().sum()
        missing_data_pct = (missing_data / (total_observations * total_assets)) * 100

        # Zero return analysis (for return data)
        if data.values.max() < 10:  # Likely return data
            zero_return_pct = (data == 0).sum().sum() / (total_observations * total_assets) * 100
        else:
            zero_return_pct = 0

        # Outlier detection
        outlier_count = self._detect_outliers(data).sum().sum()

        # Duplicate dates
        duplicate_dates = data.index.duplicated().sum()

        # Frequency consistency
        frequency_consistency = self._check_frequency_consistency(data)

        # Data type issues
        data_type_issues = []
        if data.index.dtype != 'datetime64[ns]':
            data_type_issues.append("Index is not datetime")

        if not all(data[col].dtype in [np.float64, np.float32, np.int64, np.int32]
                  for col in data.columns):
            data_type_issues.append("Non-numeric data columns")

        # Calculate quality score
        quality_score = self._calculate_quality_score(
            missing_data_pct, zero_return_pct, outlier_count,
            duplicate_dates, frequency_consistency, data_type_issues
        )

        return DataQualityReport(
            total_observations=total_observations,
            total_assets=total_assets,
            date_range=date_range,
            missing_data_pct=missing_data_pct,
            zero_return_pct=zero_return_pct,
            outlier_count=outlier_count,
            duplicate_dates=duplicate_dates,
            frequency_consistency=frequency_consistency,
            data_type_issues=data_type_issues,
            quality_score=quality_score
        )

    def _detect_outliers(self, data: pd.DataFrame, threshold: float = 3.0) -> pd.DataFrame:
        """Detect outliers using z-score method"""

        z_scores = np.abs(stats.zscore(data, nan_policy='omit'))
        outliers = z_scores > threshold

        return pd.DataFrame(outliers, index=data.index, columns=data.columns)

    def _check_frequency_consistency(self, data: pd.DataFrame) -> bool:
        """Check if data frequency is consistent"""

        if len(data) < 2:
            return True

        # Calculate time differences
        time_diffs = data.index.to_series().diff().dropna()

        # Check if most differences are the same (allowing for some missing dates)
        mode_diff = time_diffs.mode().iloc[0]
        consistent_count = (time_diffs == mode_diff).sum()

        return consistent_count / len(time_diffs) > 0.9

    def _calculate_quality_score(self,
                                missing_pct: float,
                                zero_return_pct: float,
                                outlier_count: int,
                                duplicate_dates: int,
                                frequency_consistent: bool,
                                data_type_issues: List[str]) -> float:
        """Calculate overall data quality score (0-100)"""

        score = 100.0

        # Penalize missing data
        score -= min(50, missing_pct * 2)

        # Penalize zero returns
        score -= min(20, zero_return_pct)

        # Penalize outliers
        score -= min(15, outlier_count / 100)

        # Penalize duplicate dates
        score -= min(10, duplicate_dates)

        # Penalize frequency inconsistency
        if not frequency_consistent:
            score -= 10

        # Penalize data type issues
        score -= len(data_type_issues) * 5

        return max(0, score)

    def clean_data(self,
                   data: pd.DataFrame,
                   missing_data_strategy: str = "interpolate",
                   outlier_strategy: str = "clip",
                   outlier_threshold: float = 3.0,
                   min_history_ratio: float = 0.5,
                   progress_callback: Optional[Callable] = None) -> Tuple[pd.DataFrame, Dict]:
        """
        Clean and preprocess data

        Parameters:
        -----------
        data : pd.DataFrame
            Data to clean
        missing_data_strategy : str
            Strategy for missing data ("drop", "interpolate", "forward_fill", "knn")
        outlier_strategy : str
            Strategy for outliers ("clip", "remove", "transform")
        outlier_threshold : float
            Threshold for outlier detection (z-score)
        min_history_ratio : float
            Minimum ratio of required history
        progress_callback : Callable, optional
            Progress callback

        Returns:
        --------
        Tuple of (cleaned_data, cleaning_report)
        """

        original_shape = data.shape
        cleaning_report = {
            "original_shape": original_shape,
            "steps_performed": [],
            "assets_removed": [],
            "observations_removed": 0,
            "missing_data_filled": 0,
            "outliers_handled": 0
        }

        try:
            if progress_callback:
                progress_callback("Starting data cleaning...")

            # 1. Remove assets with insufficient history
            if progress_callback:
                progress_callback("Checking asset history requirements...")

            min_required = int(len(data) * min_history_ratio)
            valid_assets = data.columns[data.notna().sum() >= min_required]

            if len(valid_assets) < len(data.columns):
                removed_assets = set(data.columns) - set(valid_assets)
                cleaning_report["assets_removed"] = list(removed_assets)
                data = data[valid_assets]
                cleaning_report["steps_performed"].append("removed_insufficient_history")

            # 2. Remove duplicate dates
            if progress_callback:
                progress_callback("Removing duplicate dates...")

            if data.index.duplicated().any():
                data = data[~data.index.duplicated(keep='first')]
                cleaning_report["steps_performed"].append("removed_duplicate_dates")

            # 3. Handle missing data
            if progress_callback:
                progress_callback(f"Handling missing data using {missing_data_strategy} strategy...")

            data, missing_filled = self._handle_missing_data(data, missing_data_strategy)
            cleaning_report["missing_data_filled"] = missing_filled
            cleaning_report["steps_performed"].append(f"missing_data_{missing_data_strategy}")

            # 4. Handle outliers
            if progress_callback:
                progress_callback(f"Handling outliers using {outlier_strategy} strategy...")

            data, outliers_handled = self._handle_outliers(
                data, outlier_strategy, outlier_threshold
            )
            cleaning_report["outliers_handled"] = outliers_handled
            cleaning_report["steps_performed"].append(f"outliers_{outlier_strategy}")

            # 5. Forward fill any remaining missing values
            if progress_callback:
                progress_callback("Final missing value handling...")

            data = data.fillna(method='ffill').fillna(method='bfill')

            # 6. Remove any remaining rows with NaN values
            if data.isnull().any().any():
                original_len = len(data)
                data = data.dropna()
                cleaning_report["observations_removed"] = original_len - len(data)
                cleaning_report["steps_performed"].append("removed_nan_rows")

            # Final report
            cleaning_report["final_shape"] = data.shape
            cleaning_report["data_retention_ratio"] = (data.shape[0] * data.shape[1]) / (original_shape[0] * original_shape[1])

            if progress_callback:
                progress_callback(f"Data cleaning completed. Final shape: {data.shape}")

            return data, cleaning_report

        except Exception as e:
            logger.error(f"Data cleaning failed: {e}")
            raise

    def _handle_missing_data(self, data: pd.DataFrame, strategy: str) -> Tuple[pd.DataFrame, int]:
        """Handle missing data based on strategy"""

        if strategy == "drop":
            # Drop rows with any missing data
            original_len = len(data)
            data = data.dropna()
            filled_count = (original_len - len(data)) * len(data.columns)

        elif strategy == "interpolate":
            # Linear interpolation
            filled_count = data.isnull().sum().sum()
            data = data.interpolate(method='linear')

        elif strategy == "forward_fill":
            # Forward fill
            filled_count = data.isnull().sum().sum()
            data = data.fillna(method='ffill')

        elif strategy == "knn":
            # KNN imputation
            try:
                imputer = KNNImputer(n_neighbors=5)
                filled_count = data.isnull().sum().sum()
                data_values = imputer.fit_transform(data)
                data = pd.DataFrame(data_values, index=data.index, columns=data.columns)
            except ImportError:
                logger.warning("KNN imputation requires scikit-learn, using interpolation instead")
                filled_count = data.isnull().sum().sum()
                data = data.interpolate(method='linear')

        else:
            raise ValueError(f"Unknown missing data strategy: {strategy}")

        return data, int(filled_count)

    def _handle_outliers(self, data: pd.DataFrame, strategy: str, threshold: float) -> Tuple[pd.DataFrame, int]:
        """Handle outliers based on strategy"""

        outliers = self._detect_outliers(data, threshold)
        outliers_count = outliers.sum().sum()

        if strategy == "clip":
            # Clip outliers to threshold
            data_clipped = data.copy()
            for col in data.columns:
                col_mean = data[col].mean()
                col_std = data[col].std()
                upper_bound = col_mean + threshold * col_std
                lower_bound = col_mean - threshold * col_std

                data_clipped[col] = data[col].clip(lower_bound, upper_bound)

            data = data_clipped

        elif strategy == "remove":
            # Remove outliers (set to NaN)
            data = data.where(~outliers)

        elif strategy == "transform":
            # Apply robust transformation
            scaler = RobustScaler()
            data_values = scaler.fit_transform(data)
            data = pd.DataFrame(data_values, index=data.index, columns=data.columns)

        else:
            raise ValueError(f"Unknown outlier strategy: {strategy}")

        return data, int(outliers_count)

    def calculate_returns(self,
                         prices: pd.DataFrame,
                         method: str = "simple",
                         frequency: Optional[str] = None,
                         log_offset: float = 1e-8) -> pd.DataFrame:
        """
        Calculate returns from price data

        Parameters:
        -----------
        prices : pd.DataFrame
            Price data
        method : str
            Return calculation method ("simple", "log")
        frequency : str, optional
            Target frequency ("daily", "weekly", "monthly")
        log_offset : float
            Small offset to avoid log(0)

        Returns:
        --------
        DataFrame with returns
        """

        # Convert frequency if needed
        if frequency:
            prices = self._convert_frequency(prices, frequency)

        # Calculate returns
        if method == "simple":
            returns = prices.pct_change()
        elif method == "log":
            returns = np.log(prices / prices.shift(1)).replace([np.inf, -np.inf], np.nan)
        else:
            raise ValueError(f"Unknown return method: {method}")

        # Drop first row (NaN)
        returns = returns.dropna()

        return returns

    def _convert_frequency(self, data: pd.DataFrame, target_freq: str) -> pd.DataFrame:
        """Convert data frequency"""

        if target_freq == "weekly":
            return data.resample('W').last()
        elif target_freq == "monthly":
            return data.resample('M').last()
        elif target_freq == "daily":
            return data
        else:
            raise ValueError(f"Unknown frequency: {target_freq}")

    def align_datasets(self,
                       datasets: Dict[str, pd.DataFrame],
                       method: str = "inner",
                       fill_method: str = "ffill") -> Dict[str, pd.DataFrame]:
        """
        Align multiple datasets by date

        Parameters:
        -----------
        datasets : Dict[str, pd.DataFrame]
            Dictionary of datasets to align
        method : str
            Alignment method ("inner", "outer", "left", "right")
        fill_method : str
            Method to fill missing values after alignment

        Returns:
        --------
        Dictionary of aligned datasets
        """

        if not datasets:
            return {}

        # Get all date ranges
        all_dates = []
        for name, data in datasets.items():
            all_dates.extend(data.index.tolist())

        # Create common date range based on method
        all_dates = sorted(set(all_dates))

        if method == "inner":
            # Keep only dates common to all datasets
            common_dates = set(datasets[list(datasets.keys())[0]].index)
            for data in datasets.values():
                common_dates = common_dates.intersection(set(data.index))
            common_dates = sorted(common_dates)
        elif method == "outer":
            # Keep all dates from any dataset
            common_dates = all_dates
        else:
            # For left/right, use first dataset as base
            common_dates = datasets[list(datasets.keys())[0]].index.tolist()

        # Align each dataset
        aligned_datasets = {}
        for name, data in datasets.items():
            aligned_data = data.reindex(common_dates)

            # Fill missing values
            if fill_method == "ffill":
                aligned_data = aligned_data.fillna(method='ffill')
            elif fill_method == "interpolate":
                aligned_data = aligned_data.interpolate(method='linear')

            aligned_datasets[name] = aligned_data

        return aligned_datasets

    def create_factor_data(self,
                          price_data: pd.DataFrame,
                          factor_definitions: Dict[str, Dict]) -> pd.DataFrame:
        """
        Create factor data from price data

        Parameters:
        -----------
        price_data : pd.DataFrame
            Price data
        factor_definitions : Dict[str, Dict]
            Factor definitions

        Returns:
        --------
        DataFrame with factor data
        """

        factor_data = pd.DataFrame(index=price_data.index)

        for factor_name, factor_def in factor_definitions.items():
            factor_type = factor_def.get("type")

            if factor_type == "momentum":
                # Momentum factor (past returns)
                lookback = factor_def.get("lookback", 21)
                factor_data[factor_name] = price_data.pct_change(lookback)

            elif factor_type == "reversal":
                # Reversal factor (negative momentum)
                lookback = factor_def.get("lookback", 5)
                factor_data[factor_name] = -price_data.pct_change(lookback)

            elif factor_type == "volatility":
                # Volatility factor
                lookback = factor_def.get("lookback", 21)
                factor_data[factor_name] = price_data.pct_change().rolling(lookback).std()

            elif factor_type == "value":
                # Value factor (inverse of price level)
                factor_data[factor_name] = 1 / price_data

            elif factor_type == "size":
                # Size factor (market cap approximation using price levels)
                factor_data[factor_name] = np.log(price_data)

            else:
                logger.warning(f"Unknown factor type: {factor_type}")

        return factor_data

    def export_data(self,
                    data: pd.DataFrame,
                    file_path: str,
                    format: str = "csv",
                    include_quality_report: bool = True) -> Dict:
        """
        Export data to file

        Parameters:
        -----------
        data : pd.DataFrame
            Data to export
        file_path : str
            Output file path
        format : str
            Export format ("csv", "excel", "parquet")
        include_quality_report : bool
            Include quality report

        Returns:
        --------
        Dict with export status
        """

        try:
            # Create directory if it doesn't exist
            Path(file_path).parent.mkdir(parents=True, exist_ok=True)

            # Export data
            if format == "csv":
                data.to_csv(file_path)
            elif format == "excel":
                data.to_excel(file_path)
            elif format == "parquet":
                data.to_parquet(file_path)
            else:
                raise ValueError(f"Unknown export format: {format}")

            # Export quality report if requested
            if include_quality_report:
                quality_report = self.assess_data_quality(data)
                report_path = file_path.replace(f".{format}", "_quality_report.json")

                with open(report_path, 'w') as f:
                    json.dump(quality_report.to_dict(), f, indent=2, default=str)

            return {
                "status": "success",
                "file_path": file_path,
                "format": format,
                "shape": data.shape,
                "date_range": (data.index[0].isoformat(), data.index[-1].isoformat())
            }

        except Exception as e:
            logger.error(f"Failed to export data: {e}")
            return {
                "status": "error",
                "message": str(e)
            }

    def get_preprocessing_summary(self) -> Dict:
        """Get summary of all preprocessing operations"""

        return {
            "data_sources": list(self.data_sources.keys()),
            "loaded_datasets": list(self.loaded_data.keys()),
            "quality_reports": {name: report.to_dict()
                              for name, report in self.quality_reports.items()},
            "preprocessing_history": self.preprocessing_history,
            "default_parameters": self.default_params
        }

# Convenience functions
def quick_load_csv(file_path: str,
                   date_column: str = "date",
                   price_column: str = "close",
                   start_date: Optional[str] = None,
                   end_date: Optional[str] = None) -> pd.DataFrame:
    """
    Quick CSV loading function

    Parameters:
    -----------
    file_path : str
        Path to CSV file
    date_column : str
        Name of date column
    price_column : str
        Name of price column
    start_date : str, optional
        Start date filter
    end_date : str, optional
        End date filter

    Returns:
    --------
    DataFrame with price data
    """

    manager = DataManager()

    source = DataSource(
        source_type="csv",
        source_path=file_path,
        date_column=date_column,
        value_columns=[price_column]
    )

    manager.register_data_source("temp_source", source)
    return manager.load_data("temp_source", start_date=start_date, end_date=end_date)

def calculate_portfolio_returns(weights: np.ndarray,
                               returns: pd.DataFrame,
                               rebalance_freq: Optional[str] = None) -> pd.Series:
    """
    Calculate portfolio returns from weights and asset returns

    Parameters:
    -----------
    weights : np.ndarray
        Portfolio weights
    returns : pd.DataFrame
        Asset returns
    rebalance_freq : str, optional
        Rebalancing frequency

    Returns:
    --------
    Portfolio returns series
    """

    if rebalance_freq:
        # Implement rebalancing logic
        # For now, just multiply weights by returns
        portfolio_returns = (returns * weights).sum(axis=1)
    else:
        portfolio_returns = (returns * weights).sum(axis=1)

    return portfolio_returns

# Command line interface
def main():
    """Command line interface"""
    import sys
    import json

    if len(sys.argv) < 2:
        print(json.dumps({
            "error": "Usage: python skfolio_data.py <command> <args>",
            "commands": ["load", "assess_quality", "clean", "calculate_returns"]
        }))
        return

    command = sys.argv[1]

    if command == "load":
        if len(sys.argv) < 3:
            print(json.dumps({"error": "Usage: load <file_path>"}))
            return

        try:
            data = quick_load_csv(sys.argv[2])
            print(json.dumps({
                "status": "success",
                "shape": data.shape,
                "date_range": (data.index[0].isoformat(), data.index[-1].isoformat()),
                "columns": data.columns.tolist()
            }))

        except Exception as e:
            print(json.dumps({"error": str(e)}))

    elif command == "assess_quality":
        if len(sys.argv) < 3:
            print(json.dumps({"error": "Usage: assess_quality <file_path>"}))
            return

        try:
            data = quick_load_csv(sys.argv[2])
            manager = DataManager()
            report = manager.assess_data_quality(data)

            print(json.dumps(report.to_dict(), indent=2, default=str))

        except Exception as e:
            print(json.dumps({"error": str(e)}))

    else:
        print(json.dumps({"error": f"Unknown command: {command}"}))

if __name__ == "__main__":
    main()