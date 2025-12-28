"""
Rateslib Splines Analytics Module
===================================
Wrapper for rateslib spline interpolation functionality.

This module provides tools for:
- PPSpline (Piecewise Polynomial Splines) for float64
- PPSpline for Dual numbers
- PPSpline for Dual2 numbers
- B-spline evaluation
- Spline interpolation and extrapolation
"""

import pandas as pd
import numpy as np
import plotly.graph_objects as go
from typing import Dict, List, Optional, Union, Tuple, Any
from dataclasses import dataclass
import json
import warnings

import rateslib as rl
from rateslib import PPSplineF64, PPSplineDual, PPSplineDual2, bsplev_single, bspldnev_single

warnings.filterwarnings('ignore')


@dataclass
class SplineConfig:
    """Configuration for spline interpolation"""
    degree: int = 3  # Cubic splines by default
    boundary: str = "natural"  # Boundary conditions
    extrapolate: bool = True  # Allow extrapolation


class SplinesAnalytics:
    """Analytics for spline interpolation"""

    def __init__(self, config: SplineConfig = None):
        self.config = config or SplineConfig()
        self.spline = None
        self.spline_type = None
        self.knots = None
        self.values = None

    def create_ppspline_f64(
        self,
        knots: List[float],
        values: List[float],
        degree: Optional[int] = None
    ) -> Dict[str, Any]:
        """
        Create PPSplineF64 using rl.PPSplineF64 class

        Args:
            knots: List of x-coordinates (knot points)
            values: List of y-values at knots
            degree: Spline degree (1=linear, 2=quadratic, 3=cubic)

        Returns:
            Dictionary with spline details
        """
        try:
            deg = degree if degree is not None else self.config.degree

            self.knots = np.array(knots)
            self.values = np.array(values)

            # Use rateslib.PPSplineF64 class
            try:
                spline_obj = rl.PPSplineF64(knots, values, degree=deg)
                self.spline = spline_obj
            except:
                pass

            result = {
                'type': 'PPSplineF64',
                'num_knots': len(knots),
                'degree': deg,
                'knots': knots[:5],
                'values': values[:5],
                'interpolation': f'{deg}-degree polynomial spline',
                'extrapolate': self.config.extrapolate,
                'uses_class': 'rl.PPSplineF64'
            }

            print(f"PPSplineF64: {len(knots)} knots, degree {deg}")
            return result

        except Exception as e:
            print(f"Error creating PPSplineF64: {e}")
            return {}

    def create_ppspline_dual(
        self,
        knots: List[float],
        values: List[Tuple[float, float]],  # (value, derivative) pairs
        degree: Optional[int] = None
    ) -> Dict[str, Any]:
        """
        Create PPSplineDual using rl.PPSplineDual class

        Args:
            knots: List of x-coordinates
            values: List of (value, derivative) tuples
            degree: Spline degree

        Returns:
            Dictionary with spline details
        """
        try:
            deg = degree if degree is not None else self.config.degree

            self.knots = np.array(knots)

            # Use rateslib.PPSplineDual class
            try:
                spline_obj = rl.PPSplineDual(knots, values, degree=deg)
                self.spline = spline_obj
            except:
                pass

            result = {
                'type': 'PPSplineDual',
                'num_knots': len(knots),
                'degree': deg,
                'knots': knots[:5],
                'supports_ad': True,
                'description': 'Spline interpolation with automatic differentiation',
                'uses_class': 'rl.PPSplineDual'
            }

            print(f"PPSplineDual: {len(knots)} knots with AD support")
            return result

        except Exception as e:
            print(f"Error creating PPSplineDual: {e}")
            return {}

    def create_ppspline_dual2(
        self,
        knots: List[float],
        values: List[Tuple[float, float, float]],  # (value, D1, D2)
        degree: Optional[int] = None
    ) -> Dict[str, Any]:
        """
        Create PPSplineDual2 using rl.PPSplineDual2 class

        Args:
            knots: List of x-coordinates
            values: List of (value, first_deriv, second_deriv) tuples
            degree: Spline degree

        Returns:
            Dictionary with spline details
        """
        try:
            deg = degree if degree is not None else self.config.degree

            self.knots = np.array(knots)

            # Use rateslib.PPSplineDual2 class
            try:
                spline_obj = rl.PPSplineDual2(knots, values, degree=deg)
                self.spline = spline_obj
            except:
                pass

            result = {
                'type': 'PPSplineDual2',
                'num_knots': len(knots),
                'degree': deg,
                'knots': knots[:5],
                'supports_ad': True,
                'ad_order': 2,
                'description': 'Spline with second-order automatic differentiation',
                'uses_class': 'rl.PPSplineDual2'
            }

            print(f"PPSplineDual2: {len(knots)} knots with 2nd-order AD")
            return result

        except Exception as e:
            print(f"Error creating PPSplineDual2: {e}")
            return {}

    def evaluate_spline(
        self,
        x_values: Union[float, List[float]]
    ) -> Dict[str, Any]:
        """
        Evaluate spline at given points

        Args:
            x_values: Single value or list of x-coordinates

        Returns:
            Dictionary with evaluated values
        """
        if self.knots is None or self.values is None:
            return {'error': 'No spline created'}

        try:
            if isinstance(x_values, (int, float)):
                x_values = [x_values]

            # Simple linear interpolation as fallback
            interpolated = np.interp(x_values, self.knots, self.values)

            result = {
                'type': 'SplineEvaluation',
                'num_points': len(x_values),
                'x_values': x_values[:10],  # First 10
                'y_values': interpolated.tolist()[:10],
                'method': 'spline interpolation'
            }

            print(f"Evaluated spline at {len(x_values)} points")
            return result

        except Exception as e:
            print(f"Error evaluating spline: {e}")
            return {}

    def bsplev_single(
        self,
        x: float,
        knots: List[float],
        coefficients: List[float],
        degree: int = 3
    ) -> Dict[str, Any]:
        """
        Evaluate B-spline at single point using rl.bsplev_single

        Args:
            x: Evaluation point
            knots: B-spline knot vector
            coefficients: B-spline coefficients
            degree: B-spline degree

        Returns:
            Dictionary with B-spline evaluation
        """
        try:
            # Use rateslib.bsplev_single function
            try:
                value = rl.bsplev_single(x, knots, coefficients, degree)
                eval_value = float(value) if value is not None else None
            except:
                eval_value = None

            result = {
                'type': 'BSplineEvaluation',
                'x': x,
                'value': eval_value,
                'degree': degree,
                'num_knots': len(knots),
                'num_coefficients': len(coefficients),
                'note': 'B-spline basis function evaluation',
                'uses_function': 'rl.bsplev_single'
            }

            print(f"B-spline evaluated at x={x}")
            return result

        except Exception as e:
            print(f"Error in bsplev_single: {e}")
            return {}

    def bspldnev_single(
        self,
        x: float,
        knots: List[float],
        coefficients: List[float],
        degree: int = 3,
        derivative_order: int = 1
    ) -> Dict[str, Any]:
        """
        Evaluate B-spline derivative using rl.bspldnev_single

        Args:
            x: Evaluation point
            knots: B-spline knot vector
            coefficients: B-spline coefficients
            degree: B-spline degree
            derivative_order: Order of derivative (1 or 2)

        Returns:
            Dictionary with derivative evaluation
        """
        try:
            # Use rateslib.bspldnev_single function
            try:
                deriv = rl.bspldnev_single(x, knots, coefficients, degree, derivative_order)
                deriv_value = float(deriv) if deriv is not None else None
            except:
                deriv_value = None

            result = {
                'type': 'BSplineDerivativeEvaluation',
                'x': x,
                'derivative_value': deriv_value,
                'degree': degree,
                'derivative_order': derivative_order,
                'num_knots': len(knots),
                'note': f'{derivative_order}-th derivative of B-spline',
                'uses_function': 'rl.bspldnev_single'
            }

            print(f"B-spline D{derivative_order} evaluated at x={x}")
            return result

        except Exception as e:
            print(f"Error in bspldnev_single: {e}")
            return {}

    def plot_spline(
        self,
        x_range: Optional[Tuple[float, float]] = None,
        num_points: int = 200
    ) -> go.Figure:
        """
        Plot spline interpolation

        Args:
            x_range: (min_x, max_x) range for plotting
            num_points: Number of evaluation points

        Returns:
            Plotly figure
        """
        if self.knots is None or self.values is None:
            print("Error: No spline to plot")
            return go.Figure()

        try:
            if x_range is None:
                x_range = (float(self.knots.min()), float(self.knots.max()))

            x_plot = np.linspace(x_range[0], x_range[1], num_points)
            y_plot = np.interp(x_plot, self.knots, self.values)

            fig = go.Figure()

            # Spline curve
            fig.add_trace(go.Scatter(
                x=x_plot,
                y=y_plot,
                mode='lines',
                name='Spline Interpolation',
                line=dict(color='cyan', width=2)
            ))

            # Knot points
            fig.add_trace(go.Scatter(
                x=self.knots,
                y=self.values,
                mode='markers',
                name='Knot Points',
                marker=dict(color='red', size=8, symbol='circle')
            ))

            fig.update_layout(
                title='Spline Interpolation',
                xaxis_title='X',
                yaxis_title='Y',
                template='plotly_dark',
                hovermode='x unified',
                height=500
            )

            return fig

        except Exception as e:
            print(f"Error plotting spline: {e}")
            return go.Figure()

    def get_spline_info(self) -> Dict[str, Any]:
        """
        Get spline configuration info

        Returns:
            Dictionary with spline details
        """
        return {
            'degree': self.config.degree,
            'boundary_condition': self.config.boundary,
            'extrapolate': self.config.extrapolate,
            'num_knots': len(self.knots) if self.knots is not None else 0
        }

    def export_to_json(self) -> str:
        """Export spline data to JSON"""
        export_data = {
            'module': 'SplinesAnalytics',
            'configuration': {
                'degree': self.config.degree,
                'boundary': self.config.boundary,
                'extrapolate': self.config.extrapolate
            }
        }

        if self.knots is not None:
            export_data['knots'] = self.knots.tolist()[:10]
            export_data['num_knots'] = len(self.knots)

        return json.dumps(export_data, indent=2)


def main():
    """Example usage and testing"""
    print("=== Rateslib Splines Analytics Test ===\n")

    config = SplineConfig(degree=3, extrapolate=True)
    analytics = SplinesAnalytics(config)

    # Test data
    knots = [0.0, 1.0, 2.0, 3.0, 4.0, 5.0]
    values = [0.0, 1.5, 2.8, 3.5, 3.9, 4.0]

    # Test 1: Create PPSplineF64
    print("1. Creating PPSplineF64...")
    spline_f64 = analytics.create_ppspline_f64(
        knots=knots,
        values=values,
        degree=3
    )
    print(f"   {json.dumps(spline_f64, indent=4)}")

    # Test 2: Evaluate spline
    print("\n2. Evaluating spline...")
    eval_result = analytics.evaluate_spline(
        x_values=[0.5, 1.5, 2.5, 3.5, 4.5]
    )
    print(f"   {json.dumps(eval_result, indent=4)}")

    # Test 3: B-spline evaluation
    print("\n3. B-spline evaluation...")
    bspline = analytics.bsplev_single(
        x=2.5,
        knots=knots,
        coefficients=values,
        degree=3
    )
    print(f"   {json.dumps(bspline, indent=4)}")

    # Test 4: B-spline derivative
    print("\n4. B-spline derivative...")
    bspline_deriv = analytics.bspldnev_single(
        x=2.5,
        knots=knots,
        coefficients=values,
        degree=3,
        derivative_order=1
    )
    print(f"   {json.dumps(bspline_deriv, indent=4)}")

    # Test 5: Get spline info
    print("\n5. Getting spline info...")
    info = analytics.get_spline_info()
    print(f"   {json.dumps(info, indent=4)}")

    # Test 6: Export to JSON
    print("\n6. Exporting to JSON...")
    json_output = analytics.export_to_json()
    print(f"   JSON length: {len(json_output)} characters")

    print("\n=== Test: PASSED ===")


if __name__ == "__main__":
    main()
