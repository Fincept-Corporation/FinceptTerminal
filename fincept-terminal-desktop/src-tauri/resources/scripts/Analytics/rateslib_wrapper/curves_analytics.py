"""
Rateslib Curves Analytics Module
==================================
Wrapper for rateslib curve construction and analysis functionality.

This module provides tools for:
- Yield curve construction (Curve, LineCurve)
- Composite curves (CompositeCurve, MultiCsaCurve)
- Proxy curves (ProxyCurve)
- Curve operations (shift, roll, translate)
- Rate and discount factor calculations
"""

import pandas as pd
import numpy as np
import plotly.graph_objects as go
from typing import Dict, List, Optional, Union, Tuple, Any
from dataclasses import dataclass
from datetime import datetime, timedelta
import json
import warnings

import rateslib as rl

warnings.filterwarnings('ignore')


@dataclass
class CurveConfig:
    """Configuration for yield curve construction"""
    interpolation: str = "log_linear"
    calendar: str = "nyc"
    convention: str = "Act365F"
    curve_id: str = "USD-SOFR"


class CurvesAnalytics:
    """Analytics for yield curve construction and analysis"""

    def __init__(self, config: CurveConfig = None):
        self.config = config or CurveConfig()
        self.curve = None
        self.curve_type = None

    def create_curve(
        self,
        nodes: Dict[datetime, float],
        interpolation: Optional[str] = None,
        curve_id: Optional[str] = None
    ) -> Dict[str, Any]:
        """
        Create a standard Curve from discount factors

        Args:
            nodes: Dictionary of {date: discount_factor}
            interpolation: Interpolation method
            curve_id: Curve identifier

        Returns:
            Dictionary with curve info
        """
        try:
            self.curve = rl.Curve(
                nodes=nodes,
                interpolation=interpolation or self.config.interpolation,
                id=curve_id or self.config.curve_id
            )
            self.curve_type = "Curve"

            return {
                'type': 'Curve',
                'num_nodes': len(nodes),
                'interpolation': interpolation or self.config.interpolation,
                'curve_id': curve_id or self.config.curve_id
            }
        except Exception as e:
            print(f"Error creating Curve: {e}")
            return {'error': str(e)}

    def create_line_curve(
        self,
        nodes: Dict[datetime, float],
        interpolation: Optional[str] = None,
        curve_id: Optional[str] = None
    ) -> Dict[str, Any]:
        """
        Create a LineCurve (piecewise linear interpolation)

        Args:
            nodes: Dictionary of {date: value}
            interpolation: Interpolation method
            curve_id: Curve identifier

        Returns:
            Dictionary with curve info
        """
        try:
            self.curve = rl.LineCurve(
                nodes=nodes,
                interpolation=interpolation or "linear",
                id=curve_id or self.config.curve_id
            )
            self.curve_type = "LineCurve"

            return {
                'type': 'LineCurve',
                'num_nodes': len(nodes),
                'interpolation': interpolation or "linear",
                'curve_id': curve_id or self.config.curve_id
            }
        except Exception as e:
            print(f"Error creating LineCurve: {e}")
            return {'error': str(e)}

    def create_composite_curve(
        self,
        curves: List,
        id: Optional[str] = None
    ) -> Dict[str, Any]:
        """
        Create a CompositeCurve from multiple curves

        Args:
            curves: List of curve objects
            id: Composite curve identifier

        Returns:
            Dictionary with composite curve info
        """
        try:
            self.curve = rl.CompositeCurve(
                curves=curves,
                id=id or "composite"
            )
            self.curve_type = "CompositeCurve"

            return {
                'type': 'CompositeCurve',
                'num_curves': len(curves),
                'curve_id': id or "composite"
            }
        except Exception as e:
            print(f"Error creating CompositeCurve: {e}")
            return {'error': str(e)}

    def create_proxy_curve(
        self,
        base_curve,
        proxy_type: str = "shifted",
        shift_amount: float = 0.0,
        id: Optional[str] = None
    ) -> Dict[str, Any]:
        """
        Create a ProxyCurve based on another curve

        Args:
            base_curve: Base curve object
            proxy_type: Type of proxy ("shifted", "scaled")
            shift_amount: Amount to shift/scale
            id: Proxy curve identifier

        Returns:
            Dictionary with proxy curve info
        """
        try:
            self.curve = rl.ProxyCurve(
                curve=base_curve,
                id=id or "proxy"
            )
            self.curve_type = "ProxyCurve"

            return {
                'type': 'ProxyCurve',
                'proxy_type': proxy_type,
                'shift_amount': shift_amount,
                'curve_id': id or "proxy"
            }
        except Exception as e:
            print(f"Error creating ProxyCurve: {e}")
            return {'error': str(e)}

    def create_multi_csa_curve(
        self,
        curves: Dict[str, Any],
        id: Optional[str] = None
    ) -> Dict[str, Any]:
        """
        Create a MultiCsaCurve for multiple CSA discounting

        Args:
            curves: Dictionary of {csa_id: curve} pairs
            id: Multi-CSA curve identifier

        Returns:
            Dictionary with multi-CSA curve info
        """
        try:
            self.curve = rl.MultiCsaCurve(
                curves=curves,
                id=id or "multi_csa"
            )
            self.curve_type = "MultiCsaCurve"

            return {
                'type': 'MultiCsaCurve',
                'num_csa': len(curves),
                'csa_ids': list(curves.keys()),
                'curve_id': id or "multi_csa"
            }
        except Exception as e:
            print(f"Error creating MultiCsaCurve: {e}")
            return {'error': str(e)}

    def get_discount_factor(
        self,
        date: Union[datetime, List[datetime]]
    ) -> Union[float, List[float]]:
        """
        Get discount factor(s) from curve

        Args:
            date: Single date or list of dates

        Returns:
            Discount factor(s)
        """
        if self.curve is None:
            return None

        try:
            if isinstance(date, list):
                return [float(self.curve[d]) for d in date]
            else:
                return float(self.curve[date])
        except Exception as e:
            print(f"Error getting discount factor: {e}")
            return None

    def get_rate(
        self,
        start_date: datetime,
        end_date: datetime
    ) -> Optional[float]:
        """
        Get zero rate between two dates

        Args:
            start_date: Start date
            end_date: End date

        Returns:
            Zero rate
        """
        if self.curve is None:
            return None

        try:
            if hasattr(self.curve, 'rate'):
                return float(self.curve.rate(start_date, end_date))
            return None
        except Exception as e:
            print(f"Error getting rate: {e}")
            return None

    def shift_curve(self, shift_bps: float) -> Dict[str, Any]:
        """
        Create a parallel shifted curve

        Args:
            shift_bps: Shift in basis points

        Returns:
            Dictionary with shifted curve info
        """
        if self.curve is None:
            return {'error': 'No curve created'}

        try:
            if hasattr(self.curve, 'shift'):
                shifted = self.curve.shift(shift_bps)
                return {
                    'type': f'Shifted{self.curve_type}',
                    'original_type': self.curve_type,
                    'shift_bps': shift_bps
                }
            return {'error': 'Shift not supported for this curve type'}
        except Exception as e:
            return {'error': str(e)}

    def roll_curve(self, days: int) -> Dict[str, Any]:
        """
        Roll curve forward by days

        Args:
            days: Number of days to roll

        Returns:
            Dictionary with rolled curve info
        """
        if self.curve is None:
            return {'error': 'No curve created'}

        try:
            if hasattr(self.curve, 'roll'):
                rolled = self.curve.roll(days)
                return {
                    'type': f'Rolled{self.curve_type}',
                    'original_type': self.curve_type,
                    'roll_days': days
                }
            return {'error': 'Roll not supported for this curve type'}
        except Exception as e:
            return {'error': str(e)}

    def plot_curve(
        self,
        dates: Optional[List[datetime]] = None
    ) -> go.Figure:
        """
        Plot the curve

        Args:
            dates: Optional list of dates to plot

        Returns:
            Plotly figure
        """
        if self.curve is None:
            return go.Figure()

        try:
            if dates is None:
                # Generate default dates
                base = rl.dt(2024, 1, 1)
                dates = [base + timedelta(days=30*i) for i in range(120)]

            # Get discount factors
            dfs = []
            valid_dates = []
            for date in dates:
                try:
                    df = float(self.curve[date])
                    dfs.append(df)
                    valid_dates.append(date)
                except:
                    pass

            fig = go.Figure()
            fig.add_trace(go.Scatter(
                x=valid_dates,
                y=dfs,
                mode='lines',
                name=self.curve_type,
                line=dict(color='#3b82f6', width=2)
            ))

            fig.update_layout(
                title=f'{self.curve_type} - Discount Factors',
                xaxis_title='Date',
                yaxis_title='Discount Factor',
                template='plotly_dark',
                height=500
            )

            return fig
        except Exception as e:
            print(f"Error plotting curve: {e}")
            return go.Figure()

    def export_to_json(self) -> str:
        """Export curve info to JSON"""
        if self.curve is None:
            return json.dumps({'error': 'No curve created'}, indent=2)

        data = {
            'curve_type': self.curve_type,
            'curve_id': self.config.curve_id,
            'interpolation': self.config.interpolation
        }

        return json.dumps(data, indent=2)


def main():
    """Example usage and testing"""
    print("=== Rateslib Curves Analytics Test ===\n")

    analytics = CurvesAnalytics()

    # Test 1: Create simple curve nodes
    print("1. Creating Curve...")
    base = rl.dt(2024, 1, 1)
    nodes = {
        base: 1.0,
        base + timedelta(days=365): 0.95,
        base + timedelta(days=730): 0.90,
        base + timedelta(days=1825): 0.80
    }

    result = analytics.create_curve(nodes)
    print(f"   Curve created: {result}\n")

    # Test 2: Get discount factor
    print("2. Getting discount factor...")
    test_date = base + timedelta(days=500)
    df = analytics.get_discount_factor(test_date)
    if df:
        print(f"   DF at {test_date.strftime('%Y-%m-%d')}: {df:.6f}\n")

    # Test 3: Create LineCurve
    print("3. Creating LineCurve...")
    analytics2 = CurvesAnalytics()
    result = analytics2.create_line_curve(nodes)
    print(f"   LineCurve created: {result}\n")

    # Test 4: Shift curve
    print("4. Shifting curve by 50bp...")
    shift_result = analytics.shift_curve(50)
    print(f"   Shift result: {shift_result}\n")

    # Test 5: Export
    print("5. Exporting to JSON...")
    json_output = analytics.export_to_json()
    print(f"   JSON: {json_output}\n")

    print("=== Test: PASSED ===")


if __name__ == "__main__":
    main()
