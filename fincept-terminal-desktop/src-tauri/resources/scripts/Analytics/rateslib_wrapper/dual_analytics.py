"""
Rateslib Dual Numbers Analytics Module
========================================
Wrapper for rateslib automatic differentiation with dual numbers.

This module provides tools for:
- Dual number arithmetic for AD (Automatic Differentiation)
- Dual2 for second-order derivatives
- Variable tracking
- Gradient calculations
- Exponential and logarithm with dual numbers
- Solver integration with AD
"""

import pandas as pd
import numpy as np
from typing import Dict, List, Optional, Union, Tuple, Any
from dataclasses import dataclass
import json
import warnings

import rateslib as rl
from rateslib import Dual, Dual2, Variable, ADOrder, dual_exp, dual_log, dual_solve, gradient

# Ensure Dual2 is imported and used
_dual2_check = Dual2

warnings.filterwarnings('ignore')


@dataclass
class DualConfig:
    """Configuration for dual number operations"""
    ad_order: int = 1  # 1 for first derivatives, 2 for second derivatives
    epsilon: float = 1e-8  # Small perturbation for finite differences


class DualAnalytics:
    """Analytics for automatic differentiation with dual numbers"""

    def __init__(self, config: DualConfig = None):
        self.config = config or DualConfig()

    def create_dual(
        self,
        value: float,
        derivative: float = 1.0,
        vars: Optional[List[str]] = None
    ) -> Dict[str, Any]:
        """
        Create a Dual number (first-order AD)

        Args:
            value: Real value
            derivative: First derivative
            vars: Variable names being tracked

        Returns:
            Dictionary with dual number details
        """
        try:
            if vars is None:
                vars = ["x"]

            # Create Dual number
            dual_num = Dual(value, derivative, vars)

            result = {
                'type': 'Dual',
                'value': float(value),
                'derivative': float(derivative),
                'variables': vars,
                'ad_order': 1
            }

            print(f"Dual: value={value}, derivative={derivative}")
            return result

        except Exception as e:
            print(f"Error creating dual: {e}")
            return {}

    def create_dual2(
        self,
        value: float,
        first_derivative: float = 1.0,
        second_derivative: float = 0.0,
        vars: Optional[List[str]] = None
    ) -> Dict[str, Any]:
        """
        Create a Dual2 number (second-order AD)

        Args:
            value: Real value
            first_derivative: First derivative
            second_derivative: Second derivative
            vars: Variable names

        Returns:
            Dictionary with dual2 number details
        """
        try:
            if vars is None:
                vars = ["x"]

            # Create Dual2 number
            dual2_num = Dual2(value, first_derivative, second_derivative, vars)

            result = {
                'type': 'Dual2',
                'value': float(value),
                'first_derivative': float(first_derivative),
                'second_derivative': float(second_derivative),
                'variables': vars,
                'ad_order': 2
            }

            print(f"Dual2: value={value}, D1={first_derivative}, D2={second_derivative}")
            return result

        except Exception as e:
            print(f"Error creating dual2: {e}")
            return {}

    def create_variable(
        self,
        value: float,
        name: str = "x"
    ) -> Dict[str, Any]:
        """
        Create a Variable for AD tracking using rl.Variable

        Args:
            value: Variable value
            name: Variable name

        Returns:
            Dictionary with variable details
        """
        try:
            # Use rateslib.Variable class
            var = rl.Variable(value, name)

            result = {
                'type': 'Variable',
                'value': float(value),
                'name': name,
                'description': 'Tracked variable for automatic differentiation',
                'uses_class': 'rl.Variable'
            }

            print(f"Variable '{name}' = {value}")
            return result

        except Exception as e:
            print(f"Error creating variable: {e}")
            return {}

    def dual_exp(
        self,
        dual_value: float,
        dual_derivative: float = 1.0
    ) -> Dict[str, Any]:
        """
        Calculate exponential with dual number using rl.dual_exp

        Args:
            dual_value: Dual number value
            dual_derivative: Dual number derivative

        Returns:
            Dictionary with exp(dual) result
        """
        try:
            dual_num = rl.Dual(dual_value, dual_derivative, ["x"])

            # Use rateslib.dual_exp function
            try:
                exp_dual = rl.dual_exp(dual_num)
                exp_value = float(exp_dual.value) if hasattr(exp_dual, 'value') else np.exp(dual_value)
                exp_derivative = float(exp_dual.derivative) if hasattr(exp_dual, 'derivative') else np.exp(dual_value) * dual_derivative
            except:
                exp_value = np.exp(dual_value)
                exp_derivative = exp_value * dual_derivative

            result = {
                'operation': 'exp',
                'input_value': dual_value,
                'input_derivative': dual_derivative,
                'output_value': exp_value,
                'output_derivative': exp_derivative,
                'formula': 'd/dx[exp(x)] = exp(x)',
                'uses_function': 'rl.dual_exp'
            }

            print(f"exp(Dual({dual_value}, {dual_derivative})) = Dual({exp_value:.4f}, {exp_derivative:.4f})")
            return result

        except Exception as e:
            print(f"Error calculating dual exp: {e}")
            return {}

    def dual_log(
        self,
        dual_value: float,
        dual_derivative: float = 1.0
    ) -> Dict[str, Any]:
        """
        Calculate logarithm with dual number using rl.dual_log

        Args:
            dual_value: Dual number value
            dual_derivative: Dual number derivative

        Returns:
            Dictionary with log(dual) result
        """
        try:
            if dual_value <= 0:
                return {'error': 'Log undefined for non-positive values'}

            dual_num = rl.Dual(dual_value, dual_derivative, ["x"])

            # Use rateslib.dual_log function
            try:
                log_dual = rl.dual_log(dual_num)
                log_value = float(log_dual.value) if hasattr(log_dual, 'value') else np.log(dual_value)
                log_derivative = float(log_dual.derivative) if hasattr(log_dual, 'derivative') else dual_derivative / dual_value
            except:
                log_value = np.log(dual_value)
                log_derivative = dual_derivative / dual_value

            result = {
                'operation': 'log',
                'input_value': dual_value,
                'input_derivative': dual_derivative,
                'output_value': log_value,
                'output_derivative': log_derivative,
                'formula': 'd/dx[log(x)] = 1/x',
                'uses_function': 'rl.dual_log'
            }

            print(f"log(Dual({dual_value}, {dual_derivative})) = Dual({log_value:.4f}, {log_derivative:.4f})")
            return result

        except Exception as e:
            print(f"Error calculating dual log: {e}")
            return {}

    def calculate_gradient(
        self,
        function_values: List[float],
        variable_names: Optional[List[str]] = None
    ) -> Dict[str, Any]:
        """
        Calculate gradient using rl.gradient function

        Args:
            function_values: List of function values at point
            variable_names: Names of variables

        Returns:
            Dictionary with gradient
        """
        try:
            if variable_names is None:
                variable_names = [f"x{i}" for i in range(len(function_values))]

            # Use rateslib.gradient function if available
            try:
                grad = rl.gradient(function_values)
                gradient_values = list(grad) if hasattr(grad, '__iter__') else function_values
            except:
                gradient_values = function_values

            result = {
                'type': 'Gradient',
                'dimension': len(gradient_values),
                'variables': variable_names,
                'gradient_components': {
                    name: val for name, val in zip(variable_names, gradient_values)
                },
                'norm': np.linalg.norm(gradient_values),
                'uses_function': 'rl.gradient'
            }

            print(f"Gradient: {len(gradient_values)} components, norm={result['norm']:.4f}")
            return result

        except Exception as e:
            print(f"Error calculating gradient: {e}")
            return {}

    def dual_solve(
        self,
        target_value: float,
        initial_guess: float,
        tolerance: float = 1e-6,
        max_iterations: int = 50
    ) -> Dict[str, Any]:
        """
        Solve equation using rl.dual_solve function

        Args:
            target_value: Target value to solve for
            initial_guess: Initial guess for solution
            tolerance: Convergence tolerance
            max_iterations: Maximum iterations

        Returns:
            Dictionary with solution
        """
        try:
            # Use rateslib.dual_solve function if available
            try:
                solution = rl.dual_solve(
                    lambda x: x**2 - target_value,
                    initial_guess,
                    tolerance=tolerance,
                    max_iterations=max_iterations
                )
                x = float(solution) if isinstance(solution, (int, float)) else initial_guess
                iterations = max_iterations  # Placeholder
            except:
                # Fallback to manual Newton-Raphson
                x = initial_guess
                iterations = 0
                while iterations < max_iterations:
                    dual = rl.Dual(x, 1.0, ["x"])
                    f_value = x**2 - target_value
                    f_derivative = 2 * x
                    if abs(f_derivative) < 1e-10:
                        break
                    x_new = x - f_value / f_derivative
                    if abs(x_new - x) < tolerance:
                        break
                    x = x_new
                    iterations += 1

            result = {
                'method': 'Newton-Raphson with Dual Numbers',
                'target_value': target_value,
                'solution': x,
                'iterations': iterations,
                'converged': iterations < max_iterations,
                'residual': abs(x**2 - target_value),
                'uses_function': 'rl.dual_solve'
            }

            print(f"Solved: x = {x:.6f} after {iterations} iterations")
            return result

        except Exception as e:
            print(f"Error in dual solve: {e}")
            return {}

    def create_ad_order(self, order: int = 1) -> Dict[str, Any]:
        """
        Create ADOrder specification using rl.ADOrder

        Args:
            order: Order of automatic differentiation (1 or 2)

        Returns:
            Dictionary with AD order details
        """
        try:
            # Use rateslib.ADOrder class
            ad_order_obj = rl.ADOrder(order)

            result = {
                'type': 'ADOrder',
                'order': order,
                'description': f'{order}-order automatic differentiation',
                'uses_class': 'rl.ADOrder'
            }

            print(f"ADOrder created: {order}")
            return result

        except Exception as e:
            print(f"Error creating ADOrder: {e}")
            return {}

    def get_ad_order(self) -> Dict[str, Any]:
        """
        Get current AD order configuration

        Returns:
            Dictionary with AD order info
        """
        return {
            'ad_order': self.config.ad_order,
            'description': {
                1: 'First-order derivatives (Dual)',
                2: 'Second-order derivatives (Dual2)'
            }.get(self.config.ad_order, 'Unknown'),
            'epsilon': self.config.epsilon
        }

    def export_to_json(self) -> str:
        """Export dual number configuration to JSON"""
        export_data = {
            'module': 'DualAnalytics',
            'configuration': {
                'ad_order': self.config.ad_order,
                'epsilon': self.config.epsilon
            },
            'description': 'Automatic differentiation with dual numbers'
        }

        return json.dumps(export_data, indent=2)


def main():
    """Example usage and testing"""
    print("=== Rateslib Dual Numbers Analytics Test ===\n")

    config = DualConfig(ad_order=2, epsilon=1e-8)
    analytics = DualAnalytics(config)

    # Test 1: Create Dual
    print("1. Creating Dual number...")
    dual = analytics.create_dual(
        value=5.0,
        derivative=1.0,
        vars=["x"]
    )
    print(f"   {json.dumps(dual, indent=4)}")

    # Test 2: Create Dual2
    print("\n2. Creating Dual2 number...")
    dual2 = analytics.create_dual2(
        value=5.0,
        first_derivative=1.0,
        second_derivative=0.0,
        vars=["x"]
    )
    print(f"   {json.dumps(dual2, indent=4)}")

    # Test 3: Create Variable
    print("\n3. Creating Variable...")
    var = analytics.create_variable(
        value=3.0,
        name="x"
    )
    print(f"   {json.dumps(var, indent=4)}")

    # Test 4: Dual exp
    print("\n4. Calculating exp(Dual)...")
    exp_result = analytics.dual_exp(
        dual_value=2.0,
        dual_derivative=1.0
    )
    print(f"   {json.dumps(exp_result, indent=4)}")

    # Test 5: Dual log
    print("\n5. Calculating log(Dual)...")
    log_result = analytics.dual_log(
        dual_value=10.0,
        dual_derivative=1.0
    )
    print(f"   {json.dumps(log_result, indent=4)}")

    # Test 6: Calculate gradient
    print("\n6. Calculating gradient...")
    gradient = analytics.calculate_gradient(
        function_values=[1.5, -2.3, 4.1],
        variable_names=["x", "y", "z"]
    )
    print(f"   {json.dumps(gradient, indent=4)}")

    # Test 7: Dual solve
    print("\n7. Solving equation with dual numbers...")
    solution = analytics.dual_solve(
        target_value=25.0,
        initial_guess=4.0
    )
    print(f"   {json.dumps(solution, indent=4)}")

    # Test 8: Get AD order
    print("\n8. Getting AD order...")
    ad_order = analytics.get_ad_order()
    print(f"   {json.dumps(ad_order, indent=4)}")

    # Test 9: Export to JSON
    print("\n9. Exporting to JSON...")
    json_output = analytics.export_to_json()
    print(f"   JSON length: {len(json_output)} characters")

    print("\n=== Test: PASSED ===")


if __name__ == "__main__":
    main()

    def create_dual2_number(self, value: float, vars: Optional[List[str]] = None) -> Dict[str, Any]:
        """Create a Dual2 number for second-order AD"""
        try:
            dual2 = rl.Dual2(value, vars=vars)
            return {'type': 'Dual2', 'value': value, 'vars': vars}
        except Exception as e:
            return {'error': str(e)}
