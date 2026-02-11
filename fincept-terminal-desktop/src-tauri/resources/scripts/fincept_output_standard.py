"""
Fincept Terminal - Python Output Standardization Library
=========================================================

This library provides a standard output format for all Python scripts in the Fincept Terminal.
It ensures consistent JSON output across 100+ scripts, making frontend parsing simple and reliable.

Standard Output Format:
{
    "success": true/false,
    "data": { ... },
    "metadata": {
        "script": "script_name",
        "timestamp": "ISO-8601",
        "output_type": "table|dict|array|text|number|multi|error",
        "version": "1.0.0",
        "execution_time_ms": 123
    },
    "error": null or {"message": "...", "type": "...", "traceback": "..."}
}

Usage:
    from fincept_output_standard import standardize_output, OutputType

    # Simple usage
    result = standardize_output(data, script_name="my_script")

    # With explicit type
    result = standardize_output(dataframe, output_type=OutputType.TABLE)

    # Print standardized output
    print_standard_output(data, script_name="my_script")
"""

import json
import sys
import traceback
from datetime import datetime
from enum import Enum
from typing import Any, Dict, List, Optional, Union
import time


class OutputType(Enum):
    """Enumeration of supported output types"""
    TABLE = "table"          # Pandas DataFrames, tabular data
    DICT = "dict"            # Dictionary/object data
    ARRAY = "array"          # Lists, arrays
    TEXT = "text"            # String outputs
    NUMBER = "number"        # Numeric outputs
    MULTI = "multi"          # Multiple outputs of different types
    ERROR = "error"          # Error state
    CHART = "chart"          # Chart/visualization data
    TIMESERIES = "timeseries"  # Time series data
    MATRIX = "matrix"        # 2D numeric arrays
    EMPTY = "empty"          # No data/null result


class FinceptOutputStandardizer:
    """Main class for standardizing Python script outputs"""

    VERSION = "1.0.0"

    def __init__(self, script_name: str = "unknown", include_metadata: bool = True):
        """
        Initialize standardizer

        Args:
            script_name: Name of the script producing output
            include_metadata: Whether to include metadata in output
        """
        self.script_name = script_name
        self.include_metadata = include_metadata
        self.start_time = time.time()

    def standardize(self, data: Any, output_type: Optional[OutputType] = None) -> Dict:
        """
        Standardize any Python object into standard format

        Args:
            data: The data to standardize (DataFrame, dict, list, etc.)
            output_type: Optional explicit type specification

        Returns:
            Standardized dictionary ready for JSON serialization
        """
        try:
            # Auto-detect type if not specified
            if output_type is None:
                output_type = self._detect_type(data)

            # Convert data based on type
            converted_data = self._convert_data(data, output_type)

            # Build standard response
            response = {
                "success": True,
                "data": converted_data,
                "error": None
            }

            # Add metadata if requested
            if self.include_metadata:
                response["metadata"] = self._build_metadata(output_type)

            return response

        except Exception as e:
            return self._create_error_response(e)

    def _detect_type(self, data: Any) -> OutputType:
        """Auto-detect the type of data"""

        # Check for None/null
        if data is None:
            return OutputType.EMPTY

        # Check for pandas DataFrame
        try:
            import pandas as pd
            if isinstance(data, pd.DataFrame):
                return OutputType.TABLE
            if isinstance(data, pd.Series):
                return OutputType.ARRAY
        except ImportError:
            pass

        # Check for numpy array
        try:
            import numpy as np
            if isinstance(data, np.ndarray):
                if data.ndim == 1:
                    return OutputType.ARRAY
                elif data.ndim == 2:
                    return OutputType.MATRIX
                else:
                    return OutputType.ARRAY
        except ImportError:
            pass

        # Check Python built-in types
        if isinstance(data, dict):
            # Check if it's a multi-output structure
            if self._is_multi_output(data):
                return OutputType.MULTI
            return OutputType.DICT

        if isinstance(data, (list, tuple)):
            # Check if it's tabular data (list of dicts with same keys)
            if self._is_tabular_list(data):
                return OutputType.TABLE
            return OutputType.ARRAY

        if isinstance(data, str):
            return OutputType.TEXT

        if isinstance(data, (int, float, complex)):
            return OutputType.NUMBER

        # Default to dict (will JSON serialize the object)
        return OutputType.DICT

    def _is_multi_output(self, data: dict) -> bool:
        """Check if dictionary represents multiple outputs"""
        # Look for keys like 'output1', 'output2' or 'result1', 'result2'
        keys = list(data.keys())
        if len(keys) > 1:
            # Check for patterns indicating multiple distinct outputs
            has_varied_types = len(set(type(v).__name__ for v in data.values())) > 1
            if has_varied_types:
                return True
        return False

    def _is_tabular_list(self, data: list) -> bool:
        """Check if list is tabular data (list of dicts with consistent keys)"""
        if not data or len(data) < 2:
            return False

        if not all(isinstance(item, dict) for item in data):
            return False

        # Check if all dicts have the same keys
        first_keys = set(data[0].keys())
        return all(set(item.keys()) == first_keys for item in data[1:])

    def _convert_data(self, data: Any, output_type: OutputType) -> Dict:
        """Convert data to standardized structure based on type"""

        if output_type == OutputType.TABLE:
            return self._convert_table(data)

        elif output_type == OutputType.DICT:
            return self._convert_dict(data)

        elif output_type == OutputType.ARRAY:
            return self._convert_array(data)

        elif output_type == OutputType.TEXT:
            return self._convert_text(data)

        elif output_type == OutputType.NUMBER:
            return self._convert_number(data)

        elif output_type == OutputType.MULTI:
            return self._convert_multi(data)

        elif output_type == OutputType.MATRIX:
            return self._convert_matrix(data)

        elif output_type == OutputType.TIMESERIES:
            return self._convert_timeseries(data)

        elif output_type == OutputType.EMPTY:
            return {"type": "empty", "value": None}

        else:
            # Fallback: try to serialize as-is
            return {"type": "unknown", "value": self._make_serializable(data)}

    def _convert_table(self, data: Any) -> Dict:
        """Convert tabular data to standard format"""
        try:
            import pandas as pd

            # Convert to DataFrame if not already
            if isinstance(data, pd.DataFrame):
                df = data
            elif isinstance(data, list) and self._is_tabular_list(data):
                df = pd.DataFrame(data)
            else:
                df = pd.DataFrame([data])

            # Convert to records format
            result = {
                "type": "table",
                "columns": list(df.columns),
                "rows": df.to_dict('records'),
                "shape": list(df.shape),
                "dtypes": {col: str(dtype) for col, dtype in df.dtypes.items()}
            }

            # Add index if it's not default
            if not isinstance(df.index, pd.RangeIndex):
                result["index"] = df.index.tolist()

            return result

        except Exception as e:
            # Fallback to simple list representation
            return {
                "type": "table",
                "columns": [],
                "rows": data if isinstance(data, list) else [data],
                "error": f"Conversion warning: {str(e)}"
            }

    def _convert_dict(self, data: Any) -> Dict:
        """Convert dictionary to standard format"""
        if isinstance(data, dict):
            serializable_data = self._make_serializable(data)
        else:
            # Try to convert object to dict
            try:
                serializable_data = data.__dict__
            except:
                serializable_data = {"value": str(data)}

        return {
            "type": "dict",
            "value": serializable_data
        }

    def _convert_array(self, data: Any) -> Dict:
        """Convert array/list to standard format"""
        try:
            import numpy as np
            if isinstance(data, np.ndarray):
                array_data = data.tolist()
            elif isinstance(data, (list, tuple)):
                array_data = list(data)
            else:
                array_data = [data]

            return {
                "type": "array",
                "items": self._make_serializable(array_data),
                "length": len(array_data)
            }
        except Exception as e:
            return {
                "type": "array",
                "items": [str(data)],
                "length": 1,
                "error": f"Conversion warning: {str(e)}"
            }

    def _convert_text(self, data: Any) -> Dict:
        """Convert text to standard format"""
        return {
            "type": "text",
            "value": str(data),
            "length": len(str(data))
        }

    def _convert_number(self, data: Any) -> Dict:
        """Convert number to standard format"""
        try:
            import numpy as np
            # Handle numpy scalar types
            if isinstance(data, (np.integer, np.floating)):
                value = data.item()
            else:
                value = float(data) if '.' in str(data) else int(data)
        except:
            value = data

        return {
            "type": "number",
            "value": value
        }

    def _convert_multi(self, data: Dict) -> Dict:
        """Convert multiple outputs to standard format"""
        outputs = []

        for key, value in data.items():
            output_type = self._detect_type(value)
            converted = self._convert_data(value, output_type)
            outputs.append({
                "name": key,
                "data": converted
            })

        return {
            "type": "multi",
            "outputs": outputs,
            "count": len(outputs)
        }

    def _convert_matrix(self, data: Any) -> Dict:
        """Convert 2D matrix to standard format"""
        try:
            import numpy as np

            if isinstance(data, np.ndarray):
                matrix_data = data.tolist()
            else:
                matrix_data = data

            return {
                "type": "matrix",
                "values": matrix_data,
                "shape": [len(matrix_data), len(matrix_data[0]) if matrix_data else 0]
            }
        except Exception as e:
            return {
                "type": "matrix",
                "values": [[str(data)]],
                "error": f"Conversion warning: {str(e)}"
            }

    def _convert_timeseries(self, data: Any) -> Dict:
        """Convert time series data to standard format"""
        try:
            import pandas as pd

            if isinstance(data, pd.Series):
                return {
                    "type": "timeseries",
                    "timestamps": data.index.tolist() if hasattr(data.index, 'tolist') else list(data.index),
                    "values": data.tolist(),
                    "length": len(data)
                }
            else:
                # Assume dict with 'timestamps' and 'values'
                return {
                    "type": "timeseries",
                    "timestamps": data.get("timestamps", []),
                    "values": data.get("values", []),
                    "length": len(data.get("values", []))
                }
        except Exception as e:
            return {
                "type": "timeseries",
                "error": f"Conversion error: {str(e)}"
            }

    def _make_serializable(self, obj: Any) -> Any:
        """Make an object JSON serializable"""
        try:
            import numpy as np
            import pandas as pd

            # Handle pandas types
            if isinstance(obj, pd.DataFrame):
                return obj.to_dict('records')
            if isinstance(obj, pd.Series):
                return obj.tolist()
            if isinstance(obj, (pd.Timestamp, pd.DatetimeTZDtype)):
                return obj.isoformat() if hasattr(obj, 'isoformat') else str(obj)

            # Handle numpy types
            if isinstance(obj, np.ndarray):
                return obj.tolist()
            if isinstance(obj, (np.integer, np.floating)):
                return obj.item()
            if isinstance(obj, np.bool_):
                return bool(obj)

            # Handle datetime
            if hasattr(obj, 'isoformat'):
                return obj.isoformat()

            # Handle dictionaries recursively
            if isinstance(obj, dict):
                return {k: self._make_serializable(v) for k, v in obj.items()}

            # Handle lists/tuples recursively
            if isinstance(obj, (list, tuple)):
                return [self._make_serializable(item) for item in obj]

            # Handle objects with __dict__
            if hasattr(obj, '__dict__'):
                return self._make_serializable(obj.__dict__)

            # Primitive types
            if isinstance(obj, (str, int, float, bool, type(None))):
                return obj

            # Last resort: convert to string
            return str(obj)

        except Exception as e:
            return f"<non-serializable: {type(obj).__name__}>"

    def _build_metadata(self, output_type: OutputType) -> Dict:
        """Build metadata object"""
        execution_time = (time.time() - self.start_time) * 1000  # Convert to ms

        return {
            "script": self.script_name,
            "timestamp": datetime.utcnow().isoformat() + "Z",
            "output_type": output_type.value,
            "version": self.VERSION,
            "execution_time_ms": round(execution_time, 2)
        }

    def _create_error_response(self, error: Exception) -> Dict:
        """Create standardized error response"""
        execution_time = (time.time() - self.start_time) * 1000

        return {
            "success": False,
            "data": None,
            "error": {
                "message": str(error),
                "type": type(error).__name__,
                "traceback": traceback.format_exc()
            },
            "metadata": {
                "script": self.script_name,
                "timestamp": datetime.utcnow().isoformat() + "Z",
                "output_type": OutputType.ERROR.value,
                "version": self.VERSION,
                "execution_time_ms": round(execution_time, 2)
            }
        }


# ============================================================================
# Convenience Functions
# ============================================================================

def standardize_output(data: Any,
                      script_name: str = "unknown",
                      output_type: Optional[OutputType] = None,
                      include_metadata: bool = True) -> Dict:
    """
    Standardize output data in one function call

    Args:
        data: The data to standardize
        script_name: Name of the script
        output_type: Optional explicit type
        include_metadata: Whether to include metadata

    Returns:
        Standardized dictionary
    """
    standardizer = FinceptOutputStandardizer(script_name, include_metadata)
    return standardizer.standardize(data, output_type)


def print_standard_output(data: Any,
                         script_name: str = "unknown",
                         output_type: Optional[OutputType] = None,
                         include_metadata: bool = True,
                         indent: Optional[int] = None):
    """
    Standardize and print output directly to stdout

    Args:
        data: The data to standardize and print
        script_name: Name of the script
        output_type: Optional explicit type
        include_metadata: Whether to include metadata
        indent: JSON indentation (None for compact, 2 for readable)
    """
    result = standardize_output(data, script_name, output_type, include_metadata)
    print(json.dumps(result, indent=indent))


def wrap_script_execution(func):
    """
    Decorator to wrap script execution with standard output formatting

    Usage:
        @wrap_script_execution
        def main():
            # Your script logic
            return some_data
    """
    def wrapper(*args, **kwargs):
        script_name = func.__module__ if func.__module__ != "__main__" else func.__name__
        standardizer = FinceptOutputStandardizer(script_name)

        try:
            result = func(*args, **kwargs)
            output = standardizer.standardize(result)
            print(json.dumps(output))
            return 0
        except Exception as e:
            error_output = standardizer._create_error_response(e)
            print(json.dumps(error_output))
            return 1

    return wrapper


# ============================================================================
# Legacy Output Handler (for gradual migration)
# ============================================================================

class LegacyOutputHandler:
    """
    Handler for wrapping legacy script outputs without modifying the scripts
    This can be used at the Rust level to intercept stdout
    """

    @staticmethod
    def try_parse_and_standardize(raw_output: str, script_name: str = "unknown") -> Dict:
        """
        Attempt to parse legacy output and standardize it

        Args:
            raw_output: Raw stdout from script
            script_name: Name of the script

        Returns:
            Standardized output
        """
        try:
            # Try to parse as JSON first
            parsed = json.loads(raw_output)
            return standardize_output(parsed, script_name=script_name)
        except json.JSONDecodeError:
            # Not JSON, treat as text
            return standardize_output(raw_output, script_name=script_name, output_type=OutputType.TEXT)
        except Exception as e:
            # Complete failure, wrap in error
            return {
                "success": False,
                "data": None,
                "error": {
                    "message": f"Failed to parse output: {str(e)}",
                    "type": "OutputParsingError",
                    "raw_output": raw_output[:1000]  # First 1000 chars
                },
                "metadata": {
                    "script": script_name,
                    "timestamp": datetime.utcnow().isoformat() + "Z",
                    "output_type": OutputType.ERROR.value,
                    "version": FinceptOutputStandardizer.VERSION
                }
            }


# ============================================================================
# Example Usage
# ============================================================================

if __name__ == "__main__":
    # Example 1: DataFrame output
    try:
        import pandas as pd
        df = pd.DataFrame({
            'A': [1, 2, 3],
            'B': [4, 5, 6]
        })
        print("Example 1 - DataFrame:")
        print_standard_output(df, script_name="example_dataframe", indent=2)
    except ImportError:
        print("Pandas not available, skipping DataFrame example")

    print("\n" + "="*50 + "\n")

    # Example 2: Dictionary output
    data = {
        "symbol": "AAPL",
        "price": 150.25,
        "change": 2.5,
        "metrics": {"pe_ratio": 25.3, "volume": 1000000}
    }
    print("Example 2 - Dictionary:")
    print_standard_output(data, script_name="example_dict", indent=2)

    print("\n" + "="*50 + "\n")

    # Example 3: Array output
    array_data = [100, 200, 300, 400]
    print("Example 3 - Array:")
    print_standard_output(array_data, script_name="example_array", indent=2)

    print("\n" + "="*50 + "\n")

    # Example 4: Using decorator
    @wrap_script_execution
    def example_script():
        return {"result": "success", "value": 42}

    print("Example 4 - Decorator:")
    example_script()
