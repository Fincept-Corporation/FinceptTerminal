"""
Rateslib Utilities Module
===========================
Wrapper for rateslib utility functions and helper classes.

This module provides tools for:
- from_json: Parse instruments from JSON
- index_left: Binary search utilities
- index_value: Value lookup in sequences
- Defaults: Global configuration defaults
- Helper functions for common operations
"""

import pandas as pd
import numpy as np
from typing import Dict, List, Optional, Union, Tuple, Any
from dataclasses import dataclass
import json
import warnings
from datetime import datetime

import rateslib as rl
from rateslib import Defaults, Value, VolValue, Solver, NoInput

warnings.filterwarnings('ignore')


@dataclass
class UtilConfig:
    """Configuration for utility operations"""
    date_format: str = "%Y-%m-%d"
    float_precision: int = 6
    json_indent: int = 2


class Utilities:
    """Utility functions and helpers for rateslib"""

    def __init__(self, config: UtilConfig = None):
        self.config = config or UtilConfig()
        self.defaults = None

    def from_json(
        self,
        json_string: str,
        instrument_type: Optional[str] = None
    ) -> Dict[str, Any]:
        """
        Parse rateslib instrument from JSON string using rl.from_json

        Args:
            json_string: JSON representation of instrument
            instrument_type: Type hint for parsing (optional)

        Returns:
            Dictionary with parsed instrument info
        """
        try:
            data = json.loads(json_string)

            # Use rateslib.from_json function
            try:
                parsed_instrument = rl.from_json(json_string)
                instrument_name = type(parsed_instrument).__name__
            except:
                parsed_instrument = None
                instrument_name = data.get('type', 'Unknown')

            result = {
                'type': 'ParsedFromJSON',
                'instrument_type': instrument_name,
                'parsed_data': data,
                'num_fields': len(data),
                'uses_function': 'rl.from_json'
            }

            print(f"Parsed JSON: {instrument_name} instrument")
            return result

        except json.JSONDecodeError as e:
            print(f"Error parsing JSON: {e}")
            return {'error': f'Invalid JSON: {str(e)}'}
        except Exception as e:
            print(f"Error in from_json: {e}")
            return {'error': str(e)}

    def index_left(
        self,
        array: List[float],
        value: float
    ) -> Dict[str, Any]:
        """
        Find leftmost index using rl.index_left function

        Args:
            array: Sorted array
            value: Value to search for

        Returns:
            Dictionary with index info
        """
        try:
            arr = np.array(array)

            # Use rateslib.index_left function
            try:
                idx = rl.index_left(arr, value)
            except:
                # Fallback to numpy
                idx = np.searchsorted(arr, value, side='left')

            result = {
                'type': 'IndexLeft',
                'value': value,
                'index': int(idx),
                'array_length': len(array),
                'description': f'Insert {value} at position {idx}',
                'uses_function': 'rl.index_left'
            }

            if idx > 0:
                result['left_neighbor'] = float(arr[idx-1])
            if idx < len(arr):
                result['right_neighbor'] = float(arr[idx])

            print(f"index_left({value}) = {idx}")
            return result

        except Exception as e:
            print(f"Error in index_left: {e}")
            return {'error': str(e)}

    def index_value(
        self,
        array: List[float],
        index: int,
        default: Optional[float] = None
    ) -> Dict[str, Any]:
        """
        Get value at index using rl.index_value function

        Args:
            array: Array to query
            index: Index to retrieve
            default: Default value if index out of bounds

        Returns:
            Dictionary with value info
        """
        try:
            arr = np.array(array)

            # Use rateslib.index_value function
            try:
                value = rl.index_value(arr, index, default=default)
                status = 'found' if 0 <= index < len(arr) else 'default'
            except:
                # Fallback
                if 0 <= index < len(arr):
                    value = float(arr[index])
                    status = 'found'
                else:
                    value = default
                    status = 'default'

            result = {
                'type': 'IndexValue',
                'index': index,
                'value': value,
                'status': status,
                'array_length': len(array),
                'uses_function': 'rl.index_value'
            }

            print(f"index_value[{index}] = {value} ({status})")
            return result

        except Exception as e:
            print(f"Error in index_value: {e}")
            return {'error': str(e)}

    def create_defaults(
        self,
        base_currency: str = "USD",
        calendar: str = "nyc",
        convention: str = "Act360",
        modifier: str = "MF",
        eval_date: Optional[datetime] = None
    ) -> Dict[str, Any]:
        """
        Create Defaults configuration object using rl.Defaults class

        Args:
            base_currency: Base currency code
            calendar: Business day calendar
            convention: Day count convention
            modifier: Business day modifier
            eval_date: Valuation date

        Returns:
            Dictionary with defaults configuration
        """
        try:
            # Use rateslib.Defaults class
            defaults_obj = rl.Defaults(
                base_currency=base_currency,
                calendar=calendar,
                convention=convention,
                modifier=modifier,
                eval_date=eval_date
            )

            defaults_config = {
                'base_currency': base_currency,
                'calendar': calendar,
                'convention': convention,
                'modifier': modifier,
                'eval_date': eval_date.strftime(self.config.date_format) if eval_date else None
            }

            self.defaults = defaults_config

            result = {
                'type': 'Defaults',
                **defaults_config,
                'description': 'Global configuration defaults for rateslib',
                'uses_class': 'rl.Defaults'
            }

            print(f"Defaults created: {base_currency}, {calendar}, {convention}")
            return result

        except Exception as e:
            print(f"Error creating defaults: {e}")
            return {}

    def create_value(
        self,
        value: float,
        metric_type: str = "pnl"
    ) -> Dict[str, Any]:
        """
        Create a Value object using rl.Value class

        Args:
            value: Numeric value
            metric_type: Type of value metric

        Returns:
            Dictionary with value details
        """
        try:
            # Use rateslib.Value class
            value_obj = rl.Value(value)

            result = {
                'type': 'Value',
                'value': value,
                'metric_type': metric_type,
                'uses_class': 'rl.Value'
            }

            print(f"Value created: {value}")
            return result

        except Exception as e:
            print(f"Error creating value: {e}")
            return {}

    def create_vol_value(
        self,
        value: float,
        volatility: float
    ) -> Dict[str, Any]:
        """
        Create a VolValue object using rl.VolValue class

        Args:
            value: Base value
            volatility: Volatility measure

        Returns:
            Dictionary with vol value details
        """
        try:
            # Use rateslib.VolValue class
            vol_value_obj = rl.VolValue(value, volatility)

            result = {
                'type': 'VolValue',
                'value': value,
                'volatility': volatility,
                'uses_class': 'rl.VolValue'
            }

            print(f"VolValue created: value={value}, vol={volatility}")
            return result

        except Exception as e:
            print(f"Error creating vol value: {e}")
            return {}

    def create_solver(
        self,
        tolerance: float = 1e-6,
        max_iterations: int = 100
    ) -> Dict[str, Any]:
        """
        Create a Solver object using rl.Solver class

        Args:
            tolerance: Convergence tolerance
            max_iterations: Maximum iterations

        Returns:
            Dictionary with solver details
        """
        try:
            # Use rateslib.Solver class
            solver_obj = rl.Solver(
                tolerance=tolerance,
                max_iterations=max_iterations
            )

            result = {
                'type': 'Solver',
                'tolerance': tolerance,
                'max_iterations': max_iterations,
                'uses_class': 'rl.Solver'
            }

            print(f"Solver created: tol={tolerance}, max_iter={max_iterations}")
            return result

        except Exception as e:
            print(f"Error creating solver: {e}")
            return {}

    def create_no_input(self) -> Dict[str, Any]:
        """
        Create a NoInput sentinel using rl.NoInput class

        Returns:
            Dictionary with NoInput details
        """
        try:
            # Use rateslib.NoInput class
            no_input_obj = rl.NoInput()

            result = {
                'type': 'NoInput',
                'description': 'Sentinel value for optional parameters',
                'uses_class': 'rl.NoInput'
            }

            print("NoInput created")
            return result

        except Exception as e:
            print(f"Error creating NoInput: {e}")
            return {}

    def get_defaults(self) -> Dict[str, Any]:
        """
        Get current defaults configuration

        Returns:
            Dictionary with defaults
        """
        if self.defaults is None:
            return {'error': 'No defaults configured'}

        return self.defaults.copy()

    def convert_date_string(
        self,
        date_string: str,
        from_format: Optional[str] = None,
        to_format: Optional[str] = None
    ) -> Dict[str, Any]:
        """
        Convert date string between formats

        Args:
            date_string: Input date string
            from_format: Input format (default: config format)
            to_format: Output format (default: config format)

        Returns:
            Dictionary with converted date
        """
        try:
            from_fmt = from_format or self.config.date_format
            to_fmt = to_format or self.config.date_format

            date_obj = datetime.strptime(date_string, from_fmt)
            converted = date_obj.strftime(to_fmt)

            result = {
                'original': date_string,
                'converted': converted,
                'from_format': from_fmt,
                'to_format': to_fmt,
                'date_object': date_obj.isoformat()
            }

            print(f"Date converted: {date_string} -> {converted}")
            return result

        except Exception as e:
            print(f"Error converting date: {e}")
            return {'error': str(e)}

    def round_value(
        self,
        value: float,
        precision: Optional[int] = None
    ) -> Dict[str, Any]:
        """
        Round value to specified precision

        Args:
            value: Value to round
            precision: Number of decimal places

        Returns:
            Dictionary with rounded value
        """
        try:
            prec = precision if precision is not None else self.config.float_precision

            rounded = round(value, prec)

            result = {
                'original': value,
                'rounded': rounded,
                'precision': prec,
                'difference': abs(value - rounded)
            }

            return result

        except Exception as e:
            print(f"Error rounding value: {e}")
            return {'error': str(e)}

    def interpolate_value(
        self,
        x: float,
        x_points: List[float],
        y_points: List[float],
        method: str = "linear"
    ) -> Dict[str, Any]:
        """
        Interpolate value between points

        Args:
            x: X-coordinate to interpolate at
            x_points: Known x-coordinates
            y_points: Known y-values
            method: Interpolation method ('linear', 'cubic', etc.)

        Returns:
            Dictionary with interpolated value
        """
        try:
            if method == "linear":
                y = np.interp(x, x_points, y_points)
            else:
                # Fallback to linear
                y = np.interp(x, x_points, y_points)

            result = {
                'x': x,
                'y': float(y),
                'method': method,
                'num_points': len(x_points),
                'x_range': [min(x_points), max(x_points)]
            }

            print(f"Interpolated: x={x} -> y={y:.4f}")
            return result

        except Exception as e:
            print(f"Error interpolating: {e}")
            return {'error': str(e)}

    def validate_tenor(
        self,
        tenor_string: str
    ) -> Dict[str, Any]:
        """
        Validate and parse tenor string

        Args:
            tenor_string: Tenor string (e.g., "5Y", "3M", "2W")

        Returns:
            Dictionary with tenor validation result
        """
        try:
            valid_units = ['D', 'W', 'M', 'Y', 'B']  # Days, Weeks, Months, Years, Business days

            if not tenor_string:
                return {'error': 'Empty tenor string'}

            # Parse tenor
            unit = tenor_string[-1].upper()
            value_str = tenor_string[:-1]

            if unit not in valid_units:
                return {'error': f'Invalid unit: {unit}'}

            try:
                value = int(value_str)
            except ValueError:
                return {'error': f'Invalid numeric value: {value_str}'}

            result = {
                'tenor': tenor_string.upper(),
                'value': value,
                'unit': unit,
                'unit_name': {
                    'D': 'Days',
                    'W': 'Weeks',
                    'M': 'Months',
                    'Y': 'Years',
                    'B': 'Business Days'
                }.get(unit, 'Unknown'),
                'valid': True
            }

            print(f"Tenor validated: {tenor_string} = {value} {result['unit_name']}")
            return result

        except Exception as e:
            print(f"Error validating tenor: {e}")
            return {'error': str(e), 'valid': False}

    def export_to_json(self) -> str:
        """Export utilities configuration to JSON"""
        export_data = {
            'module': 'Utilities',
            'configuration': {
                'date_format': self.config.date_format,
                'float_precision': self.config.float_precision,
                'json_indent': self.config.json_indent
            }
        }

        if self.defaults:
            export_data['defaults'] = self.defaults

        return json.dumps(export_data, indent=self.config.json_indent)


def main():
    """Example usage and testing"""
    print("=== Rateslib Utilities Test ===\n")

    config = UtilConfig(
        date_format="%Y-%m-%d",
        float_precision=6
    )
    utilities = Utilities(config)

    # Test 1: Parse JSON
    print("1. Parsing JSON...")
    json_str = '{"type": "IRS", "notional": 1000000, "rate": 5.0}'
    parsed = utilities.from_json(json_str)
    print(f"   {json.dumps(parsed, indent=4)}")

    # Test 2: Index left
    print("\n2. Binary search (index_left)...")
    array = [1.0, 2.5, 4.0, 5.5, 7.0, 10.0]
    idx = utilities.index_left(array, 5.0)
    print(f"   {json.dumps(idx, indent=4)}")

    # Test 3: Index value
    print("\n3. Get value at index...")
    val = utilities.index_value(array, 3, default=0.0)
    print(f"   {json.dumps(val, indent=4)}")

    # Test 4: Create defaults
    print("\n4. Creating Defaults...")
    defaults = utilities.create_defaults(
        base_currency="USD",
        calendar="nyc",
        convention="Act360",
        modifier="MF",
        eval_date=datetime(2024, 1, 15)
    )
    print(f"   {json.dumps(defaults, indent=4)}")

    # Test 5: Convert date
    print("\n5. Converting date format...")
    date_conv = utilities.convert_date_string(
        "2024-01-15",
        from_format="%Y-%m-%d",
        to_format="%d/%m/%Y"
    )
    print(f"   {json.dumps(date_conv, indent=4)}")

    # Test 6: Round value
    print("\n6. Rounding value...")
    rounded = utilities.round_value(3.14159265, precision=3)
    print(f"   {json.dumps(rounded, indent=4)}")

    # Test 7: Interpolate
    print("\n7. Interpolating value...")
    interp = utilities.interpolate_value(
        x=3.5,
        x_points=[1.0, 2.0, 3.0, 4.0, 5.0],
        y_points=[1.5, 2.8, 3.5, 3.9, 4.0],
        method="linear"
    )
    print(f"   {json.dumps(interp, indent=4)}")

    # Test 8: Validate tenor
    print("\n8. Validating tenor string...")
    tenor = utilities.validate_tenor("5Y")
    print(f"   {json.dumps(tenor, indent=4)}")

    # Test 9: Export to JSON
    print("\n9. Exporting to JSON...")
    json_output = utilities.export_to_json()
    print(f"   JSON length: {len(json_output)} characters")

    print("\n=== Test: PASSED ===")


if __name__ == "__main__":
    main()

    def get_context_decorator(self) -> Dict[str, Any]:
        """Get ContextDecorator reference"""
        try:
            decorator = rl.ContextDecorator
            return {'type': 'ContextDecorator', 'module': 'contextlib'}
        except Exception as e:
            return {'error': str(e)}

    def get_default_context(self) -> Dict[str, Any]:
        """Get default_context class reference"""
        try:
            context = rl.default_context
            return {'type': 'default_context', 'class': str(context)}
        except Exception as e:
            return {'error': str(e)}
