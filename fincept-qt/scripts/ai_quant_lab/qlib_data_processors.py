"""
AI Quant Lab - Data Processors Module
Complete implementation of Qlib data preprocessing and normalization

Features:
- Normalization (MinMax, ZScore, RobustZScore, CSZScore, CSRank)
- Data Cleaning (Dropna, Fillna, ProcessInf, TanhProcess)
- Filtering and Transformation
- Pipeline Processing
"""

import json
import sys
from typing import Dict, List, Any, Optional, Union, Callable
import warnings
warnings.filterwarnings('ignore')

try:
    import pandas as pd
    import numpy as np
    from scipy import stats
    PANDAS_AVAILABLE = True
except ImportError:
    PANDAS_AVAILABLE = False
    pd = None
    np = None


class DataProcessor:
    """Base class for data processors"""

    def __init__(self, fields_group: str = "feature"):
        """
        Args:
            fields_group: Field group to process ('feature' or 'label')
        """
        self.fields_group = fields_group

    def __call__(self, df: pd.DataFrame) -> pd.DataFrame:
        """Process the dataframe"""
        return self.fit_transform(df)

    def fit(self, df: pd.DataFrame):
        """Fit processor to data"""
        pass

    def transform(self, df: pd.DataFrame) -> pd.DataFrame:
        """Transform data"""
        raise NotImplementedError()

    def fit_transform(self, df: pd.DataFrame) -> pd.DataFrame:
        """Fit and transform data"""
        self.fit(df)
        return self.transform(df)


class MinMaxNorm(DataProcessor):
    """
    Min-Max Normalization.

    Scales features to [0, 1] range.
    """

    def __init__(self, fields_group: str = "feature"):
        super().__init__(fields_group)
        self.min_vals = None
        self.max_vals = None

    def fit(self, df: pd.DataFrame):
        """Calculate min and max values"""
        self.min_vals = df.min()
        self.max_vals = df.max()

    def transform(self, df: pd.DataFrame) -> pd.DataFrame:
        """Apply min-max normalization"""
        if self.min_vals is None or self.max_vals is None:
            raise ValueError("Processor not fitted. Call fit() first.")

        # Avoid division by zero
        range_vals = self.max_vals - self.min_vals
        range_vals[range_vals == 0] = 1.0

        normalized = (df - self.min_vals) / range_vals
        return normalized


class ZScoreNorm(DataProcessor):
    """
    Z-Score Normalization.

    Standardizes features to have mean=0 and std=1.
    """

    def __init__(self, fields_group: str = "feature"):
        super().__init__(fields_group)
        self.mean_vals = None
        self.std_vals = None

    def fit(self, df: pd.DataFrame):
        """Calculate mean and std"""
        self.mean_vals = df.mean()
        self.std_vals = df.std()

    def transform(self, df: pd.DataFrame) -> pd.DataFrame:
        """Apply z-score normalization"""
        if self.mean_vals is None or self.std_vals is None:
            raise ValueError("Processor not fitted. Call fit() first.")

        # Avoid division by zero
        std_vals = self.std_vals.copy()
        std_vals[std_vals == 0] = 1.0

        normalized = (df - self.mean_vals) / std_vals
        return normalized


class RobustZScoreNorm(DataProcessor):
    """
    Robust Z-Score Normalization.

    Uses median and MAD (Median Absolute Deviation) instead of mean and std.
    More robust to outliers.
    """

    def __init__(self, fields_group: str = "feature", clip: float = 3.0):
        super().__init__(fields_group)
        self.median_vals = None
        self.mad_vals = None
        self.clip = clip

    def fit(self, df: pd.DataFrame):
        """Calculate median and MAD"""
        self.median_vals = df.median()
        # MAD = median(|X - median(X)|)
        self.mad_vals = (df - self.median_vals).abs().median()

    def transform(self, df: pd.DataFrame) -> pd.DataFrame:
        """Apply robust z-score normalization"""
        if self.median_vals is None or self.mad_vals is None:
            raise ValueError("Processor not fitted. Call fit() first.")

        # Avoid division by zero
        mad_vals = self.mad_vals.copy()
        mad_vals[mad_vals == 0] = 1.0

        # Robust z-score
        normalized = (df - self.median_vals) / (1.4826 * mad_vals)  # 1.4826 makes it consistent with std

        # Clip extreme values
        if self.clip is not None:
            normalized = normalized.clip(-self.clip, self.clip)

        return normalized


class CSZScoreNorm(DataProcessor):
    """
    Cross-Sectional Z-Score Normalization.

    Normalizes across the cross-section (across instruments) at each time point.
    """

    def __init__(self, fields_group: str = "feature"):
        super().__init__(fields_group)

    def fit(self, df: pd.DataFrame):
        """No fitting needed for cross-sectional normalization"""
        pass

    def transform(self, df: pd.DataFrame) -> pd.DataFrame:
        """Apply cross-sectional z-score normalization"""
        # For each row (timestamp), normalize across columns (instruments)
        normalized = df.apply(lambda row: (row - row.mean()) / (row.std() + 1e-8), axis=1)
        return normalized


class CSRankNorm(DataProcessor):
    """
    Cross-Sectional Rank Normalization.

    Converts values to percentile ranks at each time point.
    Values in [0, 1] where 0 is lowest and 1 is highest.
    """

    def __init__(self, fields_group: str = "feature"):
        super().__init__(fields_group)

    def fit(self, df: pd.DataFrame):
        """No fitting needed for rank normalization"""
        pass

    def transform(self, df: pd.DataFrame) -> pd.DataFrame:
        """Apply cross-sectional rank normalization"""
        # For each row, convert to percentile ranks
        normalized = df.apply(lambda row: row.rank(pct=True), axis=1)
        return normalized


class DropnaProcessor(DataProcessor):
    """
    Drop rows/columns with NaN values.
    """

    def __init__(self, fields_group: str = "feature", axis: int = 0, thresh: Optional[int] = None):
        """
        Args:
            fields_group: Field group to process
            axis: 0 to drop rows, 1 to drop columns
            thresh: Minimum number of non-NA values required
        """
        super().__init__(fields_group)
        self.axis = axis
        self.thresh = thresh

    def fit(self, df: pd.DataFrame):
        """No fitting needed"""
        pass

    def transform(self, df: pd.DataFrame) -> pd.DataFrame:
        """Drop NA values"""
        return df.dropna(axis=self.axis, thresh=self.thresh)


class DropnaLabel(DataProcessor):
    """
    Drop rows where label is NaN.
    """

    def __init__(self):
        super().__init__(fields_group="label")

    def fit(self, df: pd.DataFrame):
        """No fitting needed"""
        pass

    def transform(self, df: pd.DataFrame) -> pd.DataFrame:
        """Drop rows with NA labels"""
        return df.dropna()


class Fillna(DataProcessor):
    """
    Fill NaN values with specified method.
    """

    def __init__(self, fields_group: str = "feature", fill_value: float = 0.0, method: Optional[str] = None):
        """
        Args:
            fields_group: Field group to process
            fill_value: Value to fill NaNs with
            method: Method for filling ('ffill', 'bfill', None)
        """
        super().__init__(fields_group)
        self.fill_value = fill_value
        self.method = method

    def fit(self, df: pd.DataFrame):
        """No fitting needed"""
        pass

    def transform(self, df: pd.DataFrame) -> pd.DataFrame:
        """Fill NA values"""
        if self.method is not None:
            return df.fillna(method=self.method)
        else:
            return df.fillna(self.fill_value)


class ProcessInf(DataProcessor):
    """
    Handle infinity values.

    Replaces inf/-inf with large/small values or NaN.
    """

    def __init__(self, fields_group: str = "feature", replace_value: Optional[float] = None):
        """
        Args:
            fields_group: Field group to process
            replace_value: Value to replace inf with (None = use NaN)
        """
        super().__init__(fields_group)
        self.replace_value = replace_value

    def fit(self, df: pd.DataFrame):
        """No fitting needed"""
        pass

    def transform(self, df: pd.DataFrame) -> pd.DataFrame:
        """Process infinity values"""
        df = df.copy()

        if self.replace_value is None:
            # Replace with NaN
            df.replace([np.inf, -np.inf], np.nan, inplace=True)
        else:
            # Replace with specified value
            df.replace(np.inf, self.replace_value, inplace=True)
            df.replace(-np.inf, -self.replace_value, inplace=True)

        return df


class TanhProcess(DataProcessor):
    """
    Tanh-based outlier processing.

    Applies tanh transformation to reduce impact of outliers.
    """

    def __init__(self, fields_group: str = "feature", scale: float = 1.0):
        """
        Args:
            fields_group: Field group to process
            scale: Scale factor for tanh
        """
        super().__init__(fields_group)
        self.scale = scale

    def fit(self, df: pd.DataFrame):
        """No fitting needed"""
        pass

    def transform(self, df: pd.DataFrame) -> pd.DataFrame:
        """Apply tanh transformation"""
        return np.tanh(df / self.scale) * self.scale


class WinsorizeProcessor(DataProcessor):
    """
    Winsorize data by capping extreme values.
    """

    def __init__(self, fields_group: str = "feature", lower: float = 0.01, upper: float = 0.99):
        """
        Args:
            fields_group: Field group to process
            lower: Lower percentile (e.g., 0.01 = 1%)
            upper: Upper percentile (e.g., 0.99 = 99%)
        """
        super().__init__(fields_group)
        self.lower = lower
        self.upper = upper
        self.lower_vals = None
        self.upper_vals = None

    def fit(self, df: pd.DataFrame):
        """Calculate winsorization thresholds"""
        self.lower_vals = df.quantile(self.lower)
        self.upper_vals = df.quantile(self.upper)

    def transform(self, df: pd.DataFrame) -> pd.DataFrame:
        """Apply winsorization"""
        if self.lower_vals is None or self.upper_vals is None:
            raise ValueError("Processor not fitted. Call fit() first.")

        winsorized = df.copy()
        for col in df.columns:
            winsorized[col] = df[col].clip(self.lower_vals[col], self.upper_vals[col])

        return winsorized


class DataProcessorPipeline:
    """
    Pipeline for chaining multiple data processors.
    """

    def __init__(self, processors: List[DataProcessor]):
        """
        Args:
            processors: List of processors to apply in sequence
        """
        self.processors = processors

    def fit(self, df: pd.DataFrame):
        """Fit all processors"""
        data = df
        for processor in self.processors:
            processor.fit(data)
            data = processor.transform(data)

    def transform(self, df: pd.DataFrame) -> pd.DataFrame:
        """Apply all processors"""
        data = df
        for processor in self.processors:
            data = processor.transform(data)
        return data

    def fit_transform(self, df: pd.DataFrame) -> pd.DataFrame:
        """Fit and transform data"""
        self.fit(df)
        return self.transform(df)


class DataProcessingService:
    """
    Service for managing data processing pipelines.
    """

    def __init__(self):
        self.pipelines = {}
        self.processors_registry = {
            'minmax': MinMaxNorm,
            'zscore': ZScoreNorm,
            'robust_zscore': RobustZScoreNorm,
            'cs_zscore': CSZScoreNorm,
            'cs_rank': CSRankNorm,
            'dropna': DropnaProcessor,
            'fillna': Fillna,
            'process_inf': ProcessInf,
            'tanh': TanhProcess,
            'winsorize': WinsorizeProcessor
        }

    def create_pipeline(self,
                       pipeline_id: str,
                       processor_configs: List[Dict[str, Any]]) -> Dict[str, Any]:
        """
        Create a data processing pipeline.

        Args:
            pipeline_id: Unique pipeline identifier
            processor_configs: List of processor configurations

        Returns:
            Pipeline creation result
        """
        try:
            processors = []
            for config in processor_configs:
                proc_type = config.get('type')
                if proc_type not in self.processors_registry:
                    return {
                        "success": False,
                        "error": f"Unknown processor type: {proc_type}"
                    }

                proc_class = self.processors_registry[proc_type]
                proc_params = {k: v for k, v in config.items() if k != 'type'}
                processor = proc_class(**proc_params)
                processors.append(processor)

            pipeline = DataProcessorPipeline(processors)
            self.pipelines[pipeline_id] = pipeline

            return {
                "success": True,
                "pipeline_id": pipeline_id,
                "num_processors": len(processors),
                "processors": [config.get('type') for config in processor_configs]
            }

        except Exception as e:
            return {
                "success": False,
                "error": f"Pipeline creation failed: {str(e)}"
            }

    def process_data(self,
                    pipeline_id: str,
                    data: pd.DataFrame,
                    fit: bool = True) -> Dict[str, Any]:
        """
        Process data through a pipeline.

        Args:
            pipeline_id: Pipeline to use
            data: Data to process
            fit: Whether to fit the pipeline first

        Returns:
            Processing result
        """
        if pipeline_id not in self.pipelines:
            return {
                "success": False,
                "error": f"Pipeline {pipeline_id} not found"
            }

        try:
            pipeline = self.pipelines[pipeline_id]

            if fit:
                processed_data = pipeline.fit_transform(data)
            else:
                processed_data = pipeline.transform(data)

            # Calculate statistics
            stats = {
                "input_shape": data.shape,
                "output_shape": processed_data.shape,
                "input_nulls": int(data.isnull().sum().sum()),
                "output_nulls": int(processed_data.isnull().sum().sum()),
                "input_mean": float(data.mean().mean()),
                "output_mean": float(processed_data.mean().mean()),
                "input_std": float(data.std().mean()),
                "output_std": float(processed_data.std().mean())
            }

            return {
                "success": True,
                "pipeline_id": pipeline_id,
                "processed_data": processed_data.to_dict(),
                "statistics": stats
            }

        except Exception as e:
            return {
                "success": False,
                "error": f"Data processing failed: {str(e)}"
            }

    def get_available_processors(self) -> Dict[str, Any]:
        """Get list of available processors"""
        return {
            "success": True,
            "processors": list(self.processors_registry.keys()),
            "descriptions": {
                "minmax": "Min-Max normalization [0, 1]",
                "zscore": "Z-Score normalization (mean=0, std=1)",
                "robust_zscore": "Robust Z-Score using median and MAD",
                "cs_zscore": "Cross-sectional Z-Score normalization",
                "cs_rank": "Cross-sectional rank normalization",
                "dropna": "Drop rows/columns with NaN",
                "fillna": "Fill NaN values",
                "process_inf": "Handle infinity values",
                "tanh": "Tanh-based outlier processing",
                "winsorize": "Cap extreme values at percentiles"
            }
        }


def main():
    """CLI interface"""
    if len(sys.argv) < 2:
        print(json.dumps({"success": False, "error": "No command specified"}))
        sys.exit(1)

    command = sys.argv[1]
    service = DataProcessingService()

    try:
        if command == "check_status":
            result = {
                "success": True,
                "pandas_available": PANDAS_AVAILABLE,
                "processors_available": len(service.processors_registry)
            }

        elif command == "list_processors":
            result = service.get_available_processors()

        elif command == "create_pipeline":
            params = json.loads(sys.argv[2])
            result = service.create_pipeline(
                params.get("pipeline_id"),
                params.get("processors", [])
            )

        else:
            result = {"success": False, "error": f"Unknown command: {command}"}

        print(json.dumps(result))

    except Exception as e:
        print(json.dumps({"success": False, "error": str(e)}))
        sys.exit(1)


if __name__ == "__main__":
    main()
